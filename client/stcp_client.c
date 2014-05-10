//文件名: client/stcp_client.c
//
//描述: 这个文件包含STCP客户端接口实现 
//
//创建日期: 2013年1月

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <assert.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "../topology/topology.h"
#include "stcp_client.h"
#include "../common/seg.h"

//声明tcbtable为全局变量
client_tcb_t* tcbtable[MAX_TRANSPORT_CONNECTIONS];
//声明到SIP进程的TCP连接为全局变量
int sip_conn;

/*********************************************************************/
//
//STCP API实现
//
/*********************************************************************/

// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
void stcp_client_init(int conn) 
{
	int i ;
	int rc ;//记录创建线程是否成功
	pthread_t tid;

	sip_conn = conn ;

	for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; ++i)
	{
		tcbtable[i] = NULL;
	}

	rc = pthread_create(&tid,NULL,seghandler,NULL);

	if (rc)//创建失败
	{		
		printf("ERROR ; return code from pthread_create() %d\n", rc);
		exit(-1);
	}
	else//创建成功
		printf("seghandler thread Create sucess ! ! !\n");

}

// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
int stcp_client_sock(unsigned int client_port) 
{
	printf("FUNC->stcp_client_sock\n");
	int index ;

	for (index = 0; index < MAX_TRANSPORT_CONNECTIONS; ++index)
	{
		/* code */
		if (tcbtable[index] == NULL)
		{
			/* code */printf("Free TCB at %d \n", index);
			tcbtable[index] = (client_tcb_t *)malloc(sizeof(client_tcb_t));
			tcbtable[index]->server_nodeID = 0;
			tcbtable[index]->client_nodeID = topology_getMyNodeID();
			tcbtable[index]->server_portNum =  0;
			tcbtable[index]->client_portNum = client_port;
			tcbtable[index]->state = CLOSED;
			tcbtable[index]->next_seqNum = 0;
			tcbtable[index]->bufMutex = malloc(sizeof(pthread_mutex_t));
			tcbtable[index]->sendBufHead = NULL;
			tcbtable[index]->sendBufunSent = NULL;
			tcbtable[index]->sendBufTail = NULL;
			tcbtable[index]->unAck_segNum = 0 ;
			pthread_mutex_init(tcbtable[index]->bufMutex, PTHREAD_MUTEX_TIMED_NP);// init lock

			return index;
		}	
	}
	printf("There is no free tcb\n");
	return -1;  
}

// 这个函数用于连接服务器. 它以套接字ID, 服务器节点ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器节点ID和服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
int stcp_client_connect(int sockfd, int nodeID, unsigned int server_port) 
{
	printf("FUNC->stcp_client_connect\n");

	tcbtable[sockfd]->server_nodeID = nodeID;
	tcbtable[sockfd]->server_portNum = server_port;
	tcbtable[sockfd]->state = SYNSENT;

	seg_t segment;

	segment.header.src_port = tcbtable[sockfd]->client_portNum;
	segment.header.dest_port = tcbtable[sockfd]->server_portNum;
	segment.header.seq_num = 0;
	segment.header.ack_num = 0 ;
	segment.header.length = 0;//data为0
	segment.header.type = SYN;
	segment.header.rcv_win = 0;
	segment.header.checksum = 0;

	sip_sendseg(sip_conn,nodeID, &segment);
	usleep(SYN_TIMEOUT/1000);

	int num_resend= 0;

	while(tcbtable[sockfd]->state != CONNECTED)
	{
		printf("Time out\n");

		if (num_resend >= SYN_MAX_RETRY)
		{
			/* code */
			printf("Num of retry > SYN_MAX_RETRY, the state return CLOSED!\n");
			tcbtable[sockfd]->state = CLOSED;
			return -1;
		}
		else
		{
			num_resend++;
			printf("139:stcp_client_connect()-->send a SYN!!\n");
			sip_sendseg(sip_conn,nodeID, &segment);
			usleep(SYN_TIMEOUT/1000);			
		}

	}
	printf("Connect on %dth socket succesful! The state of Client is: %d\n", sockfd, tcbtable[sockfd]->state);
	return 1;
}

// 发送数据给STCP服务器. 这个函数使用套接字ID找到TCB表中的条目.
// 然后它使用提供的数据创建segBuf, 将它附加到发送缓冲区链表中.
// 如果发送缓冲区在插入数据之前为空, 一个名为sendbuf_timer的线程就会启动.
// 每隔SENDBUF_ROLLING_INTERVAL时间查询发送缓冲区以检查是否有超时事件发生. 
// 这个函数在成功时返回1，否则返回-1. 
// stcp_client_send是一个非阻塞函数调用.
// 因为用户数据被分片为固定大小的STCP段, 所以一次stcp_client_send调用可能会产生多个segBuf
// 被添加到发送缓冲区链表中. 如果调用成功, 数据就被放入TCB发送缓冲区链表中, 根据滑动窗口的情况,
// 数据可能被传输到网络中, 或在队列中等待传输.
int stcp_client_send(int sockfd, void* data, unsigned int length) 
{
	printf("FUNC->stcp_client_send\n");
	pthread_mutex_lock(tcbtable[sockfd]->bufMutex);
	while(length > 0)	
	{
		printf("length > 0,next sending..\n");
		segBuf_t *sendbuf = (segBuf_t *)malloc(sizeof(segBuf_t));
		sendbuf->seg.header.src_port = tcbtable[sockfd]->client_portNum;
		sendbuf->seg.header.dest_port = tcbtable[sockfd]->server_portNum;
		//printf("stcp_client 167 -> dest_port is %d\n",tcbtable[sockfd]->server_portNum);
		sendbuf->seg.header.seq_num = tcbtable[sockfd]->next_seqNum;
		// tcbtable[sockfd]->next_seqNum++;
		sendbuf->seg.header.ack_num = 0;
		sendbuf->seg.header.type = DATA;
		sendbuf->seg.header.rcv_win = 0;
		sendbuf->next = NULL;
		if (length > MAX_SEG_LEN)
		{
			sendbuf->seg.header.length = MAX_SEG_LEN;
			memcpy(sendbuf->seg.data, data, MAX_SEG_LEN);
			data = (char *)data  + MAX_SEG_LEN ;
			length = length - MAX_SEG_LEN;
			tcbtable[sockfd]->next_seqNum += MAX_SEG_LEN;
		}
		else
		{
			tcbtable[sockfd]->next_seqNum += length;
			sendbuf->seg.header.length = length;
			memcpy(sendbuf->seg.data, data, length);
			length = 0;
		}

		sendbuf->seg.header.checksum = 0;


		/*-----------------------init--------------------------------*/
		if (tcbtable[sockfd]->sendBufHead == NULL)
		{
			printf("sendbufhead is NULL, sendbuf_timer start......\n");
			int rc;
			pthread_t start_timer;    	

			tcbtable[sockfd]->sendBufHead = sendbuf;
			// tcbtable[sockfd]->sendBufunSent = sendbuf;
			tcbtable[sockfd]->sendBufTail = sendbuf;

			rc = pthread_create(&start_timer , NULL , sendBuf_timer ,(void*)tcbtable[sockfd]);
			if (rc)
			{
				printf("sendBuf_timer Create ERROR \n");
				return -1;
			}        

			/***************send directly***********************/
			tcbtable[sockfd]->unAck_segNum++;
			printf("SendBufHead is  NULL;Send directly! \n");
			sendbuf->sentTime = getCurrentTime();    	
			sip_sendseg(sip_conn,tcbtable[sockfd]->server_nodeID,&(sendbuf->seg));
		}
		else
		{
			tcbtable[sockfd]->sendBufTail->next = sendbuf;
			tcbtable[sockfd]->sendBufTail = sendbuf;
			if (tcbtable[sockfd]->sendBufunSent == NULL)
				tcbtable[sockfd]->sendBufunSent = tcbtable[sockfd]->sendBufTail ;

			while(tcbtable[sockfd]->unAck_segNum < GBN_WINDOW && tcbtable[sockfd]->sendBufunSent != NULL)
			{
				/* code */
				sendbuf->sentTime = getCurrentTime();
				sip_sendseg(sip_conn,tcbtable[sockfd]->server_nodeID,&(sendbuf->seg));
				printf("Send a data frame. seq_num is %d\n", sendbuf->seg.header.seq_num);
				tcbtable[sockfd]->sendBufunSent = tcbtable[sockfd]->sendBufunSent->next;
				tcbtable[sockfd]->unAck_segNum++;
			}
		}

		/*-----------------------add_segBuf--------------------------------*/
	}

	pthread_mutex_unlock(tcbtable[sockfd]->bufMutex);

	return 1;
}

// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN段给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
int stcp_client_disconnect(int sockfd) 
{
	printf("FUNC->stcp_client_disconnect\n");

	seg_t *segment = (seg_t *)malloc(sizeof(seg_t));

	segment->header.src_port = tcbtable[sockfd]->client_portNum;
	segment->header.dest_port = tcbtable[sockfd]->server_portNum;
	segment->header.seq_num = 0;
	// tcbtable[sockfd]->next_seqNum++;
	segment->header.ack_num = 0 ;
	segment->header.length = 0;//data为0
	segment->header.type = FIN;
	segment->header.rcv_win = 0;
	segment->header.checksum = 0;

	sip_sendseg(sip_conn, tcbtable[sockfd]->server_nodeID,segment);
	tcbtable[sockfd]->state = FINWAIT;
	usleep(FIN_TIMEOUT/1000);

	int num_resend;

	while(tcbtable[sockfd]->state != CLOSED)
	{
		printf("Time out\n");

		if (num_resend >= FIN_MAX_RETRY)
		{
			/* code */
			printf("Num of retry > FIN_MAX_RETRY, the state return CLOSED!\n");
			tcbtable[sockfd]->state = CLOSED;
			return -1;
		}

		else
		{
			num_resend++;
			sip_sendseg(sip_conn,tcbtable[sockfd]->server_nodeID, segment);
			usleep(FIN_TIMEOUT/1000);
		}
	}

	printf("DISCONNECT on %dth socket succesful! The state of Client is: %d\n", sockfd, tcbtable[sockfd]->state);
	return 1;
}

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
int stcp_client_close(int sockfd) 
{
	printf("FUNC->stcp_client_close\n");

	segBuf_t *head  = tcbtable[sockfd]->sendBufHead;

	if (tcbtable[sockfd]->state != CLOSED)
	{
		printf("stcp_client_close()----->error state!!\n");
		return -1;
	}

	for (; head != NULL ;)
	{
		/* code */
		segBuf_t *head1 = head;
		head = head->next;
		free(head1);  
	}

	tcbtable[sockfd]->sendBufHead = NULL;
	tcbtable[sockfd]->sendBufunSent = NULL;
	tcbtable[sockfd]->sendBufTail = NULL;

	pthread_mutex_destroy(tcbtable[sockfd]->bufMutex);//释放互斥结构
	free(tcbtable[sockfd]);

	tcbtable[sockfd] = NULL;
	printf("SOCKET AT %d Closed successful !\n" , sockfd);
	return 1;
}

// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明到SIP进程的连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
void* seghandler(void* arg) 
{
	printf("FUNC->seghandler\n");
	int is_Wrong ;
	int src_node_id;
	seg_t *segment = (seg_t *)malloc(sizeof(seg_t));

	while(1)
	{
		memset( (void *)segment, 0, sizeof(seg_t) );
		is_Wrong = sip_recvseg(sip_conn,&src_node_id,segment);

		if (is_Wrong == -1)
		{
			/* code */
			printf("Connect is wrong !\n");   //thread exit
			break;
		}
		if (is_Wrong == 1)
		{
			/* code */
			printf("Segment is lost !\n");
			continue ;
		}

		int index ;
		for (index = 0; index < MAX_TRANSPORT_CONNECTIONS; ++index)
		{
			if (tcbtable[index] == NULL)
			{
				/* code */continue;
			}
			if ( tcbtable[index]->client_portNum == segment->header.dest_port && tcbtable[index]->server_portNum == segment->header.src_port)
			{
				break;
			}
		}

		if (index == MAX_TRANSPORT_CONNECTIONS)
		{
			/* code */
			printf("Can not find client_tcb\n");
			continue;
		}


		switch(segment->header.type)
		{
			case SYN:
				printf("Type  :  SYN !\n");
				printf( "Server  error\n" );
				break;
			case SYNACK:
				printf("Type  :  SYNACK !\n");
				if (tcbtable[index]->state == SYNSENT)
				{
					printf( "client CONNECTED\n" );
					tcbtable[index]->state = CONNECTED;
					//tcbtable[index]->next_seqNum ++;
				}
				break;
			case FIN:		
				printf( "Type  :  FIN !\n" );
				printf( "Server   error\n" );
				break;
			case FINACK:	
				printf( "Type  :  FINACK !\n" );			
				if( tcbtable[index]->state == FINWAIT )
				{
					printf( "client CLOSED\n" );
					tcbtable[index]->state = CLOSED;
					//tcbtable[index]->next_seqNum++;
				}
				break;
			case DATA:
				printf( "Type  :  DATA!\n" );
				printf( "Server   error\n" );
				break;
			case DATAACK:
				printf( "Type  :  DATAACK!\n" );
				pthread_mutex_lock( tcbtable[index]->bufMutex );
				segBuf_t *head_of_dataack = tcbtable[index]->sendBufHead;
				segBuf_t *find = tcbtable[index]->sendBufHead;
				// segBuf_t* newtemp = tcbtable[index]->sendBufHead;
				// for(;newtemp!=tcbtable[index]->sendBufTail;newtemp = newtemp->next){
				// 	printf("seq_num is: %d\n", newtemp->seg.header.seq_num);
				// }

				if (tcbtable[index]->sendBufTail->seg.header.seq_num < segment->header.ack_num)
				{					
					printf("全部发出并且全部确认\n");
					head_of_dataack = tcbtable[index]->sendBufTail;
					for (; find != NULL ; )
					{
						tcbtable[index]->sendBufHead = tcbtable[index]->sendBufHead->next;
						printf("+++++++++++++free seq_num is %d \n", find->seg.header.seq_num);
						free(find);						
						find = tcbtable[index]->sendBufHead;
						tcbtable[index]->unAck_segNum--;
					}

					// free(find);
					tcbtable[index]->sendBufHead = NULL;
					tcbtable[index]->sendBufTail = NULL;
					tcbtable[index]->sendBufunSent = NULL;
					// tcbtable[index]->unAck_segNum--;

				}
				else{
					printf("not全部发出\n");
					for (; head_of_dataack->seg.header.seq_num < segment->header.ack_num ; head_of_dataack = head_of_dataack->next);

					for (; find != head_of_dataack;)
					{
						tcbtable[index]->sendBufHead = tcbtable[index]->sendBufHead->next;
						printf("+++++++++++++free seq_num is %d \n", find->seg.header.seq_num);
						free(find);
						find = tcbtable[index]->sendBufHead;
						tcbtable[index]->unAck_segNum--;
					}

					// if (find == tcbtable[index]->sendBufunSent)
					// {					
					// 	printf("全部确认\n");
					// 	tcbtable[index]->sendBufHead = tcbtable[index]->sendBufunSent;
					// }

				}

				while((tcbtable[index]->unAck_segNum < GBN_WINDOW) && (tcbtable[index]->sendBufunSent != NULL)) {
					tcbtable[index]->sendBufunSent->sentTime = getCurrentTime();
					sip_sendseg(sip_conn,tcbtable[index]->server_nodeID, &(tcbtable[index]->sendBufunSent->seg));
					tcbtable[index]->sendBufunSent = tcbtable[index]->sendBufunSent->next;
					tcbtable[index]->unAck_segNum++;
				}

				pthread_mutex_unlock( tcbtable[index]->bufMutex );
				break;

			default:	
				printf( "Server error\n" );	
		}

	}

	pthread_exit(NULL);
}


//这个线程持续轮询发送缓冲区以触发超时事件. 如果发送缓冲区非空, 它应一直运行.
//如果(当前时间 - 第一个已发送但未被确认段的发送时间) > DATA_TIMEOUT, 就发生一次超时事件.
//当超时事件发生时, 重新发送所有已发送但未被确认段. 当发送缓冲区为空时, 这个线程将终止.
void* sendBuf_timer(void* clienttcb) 
{
	client_tcb_t *tcbtable_tem = (client_tcb_t *)clienttcb;
	long currenttime;
	while(1)
	{
		if (tcbtable_tem->sendBufHead == NULL)
		{
			printf("sendbufhead is null,timer exit...\n");
			break;
		}

		pthread_mutex_lock(tcbtable_tem->bufMutex);
		currenttime = getCurrentTime();
		if((currenttime - tcbtable_tem->sendBufHead->sentTime)> DATA_TIMEOUT/1000000 )//超时事件触发
		{
			/* code */
			printf("socket %d!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!time out ! \n",tcbtable_tem->client_portNum);
			segBuf_t *tem = tcbtable_tem->sendBufHead;
			while (tem != tcbtable_tem->sendBufunSent){
				tem->sentTime = getCurrentTime();
				sip_sendseg(sip_conn, tcbtable_tem->server_nodeID,&(tem->seg));
				tem = tem->next;
			}
		}

		pthread_mutex_unlock(tcbtable_tem->bufMutex);
		usleep(SENDBUF_POLLING_INTERVAL/1000);//sleep 
	}

	printf("socket%d-------------------The sendBuf_timer thread  over!\n",tcbtable_tem->client_portNum);
	pthread_exit(NULL);
}

