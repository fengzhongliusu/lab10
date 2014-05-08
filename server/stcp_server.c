//文件名: server/stcp_server.c
//
//描述: 这个文件包含STCP服务器接口实现. 
//
//创建日期: 2013年1月

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/select.h>
#include <strings.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include "stcp_server.h"
#include "../topology/topology.h"
#include "../common/constants.h"

//声明tcbtable为全局变量
server_tcb_t* tcbtable[MAX_TRANSPORT_CONNECTIONS];
//声明到SIP进程的连接为全局变量
int sip_conn;

/*********************************************************************/
//
//STCP API实现
//
/*********************************************************************/

// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对TCP套接字描述符conn初始化一个STCP层的全局变量, 
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
void stcp_server_init(int conn) 
{
	int i;
	int rc;
	pthread_t seghdl_pid;

	for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++)
	{
		tcbtable[i] = NULL;
	}

	sip_conn = conn;

	rc = pthread_create(&seghdl_pid,NULL,seghandler,NULL);
	if(rc){
		perror("error create seghandler thread\n");
	}
	else{
		printf("seghandler thread created successful!!\n");
	}
	return;
}

// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
int stcp_server_sock(unsigned int server_port) 
{
	int i;
	for(i=0;i<MAX_TRANSPORT_CONNECTIONS;i++){
		if(tcbtable[i]==NULL){
			printf("tcb available!!!\n");
			tcbtable[i] = (server_tcb_t*)malloc(sizeof(server_tcb_t));			
			tcbtable[i]->server_nodeID = topology_getMyNodeID();    //TODO  check
			tcbtable[i]->server_portNum = server_port;
			tcbtable[i]->client_nodeID = 0;   //TODO check
			tcbtable[i]->client_portNum = 0;
			tcbtable[i]->state = CLOSED;				
			tcbtable[i]->expect_seqNum = 0;
			tcbtable[i]->recvBuf = (char*)malloc(RECEIVE_BUF_SIZE);     // add
			tcbtable[i]->usedBufLen = 0;				
			tcbtable[i]->bufMutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));   //add
			pthread_mutex_init(tcbtable[i]->bufMutex,NULL);  //add
			return i;
		}
	}
	printf("tcb inavailable!!\n");	
	return -1;
}

// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后启动定时器进入忙等待直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
int stcp_server_accept(int sockfd) 
{
  struct timespec time_inter;
	time_inter.tv_sec = 0;
	time_inter.tv_nsec = ACCEPT_POLLING_INTERVAL;

	tcbtable[sockfd]->state = LISTENING;

	while(1){		
		if(tcbtable[sockfd]->state==CONNECTED){			
			break;
		}
		nanosleep(&time_inter,&time_inter);	
	}
	return 1;
}

// 接收来自STCP客户端的数据. 这个函数每隔RECVBUF_POLLING_INTERVAL时间
// 就查询接收缓冲区, 直到等待的数据到达, 它然后存储数据并返回1. 如果这个函数失败, 则返回-1.
int stcp_server_recv(int sockfd, void* buf, unsigned int length) 
{
  	if(tcbtable[sockfd] ==NULL && tcbtable[sockfd]->state != CONNECTED){
  		return -1;
  	}  	  	  	  	
  	assert(buf != NULL);  	
  	while(1){
  		printf("----------------in recv cycle %d and %d\n",tcbtable[sockfd]->usedBufLen,length);fflush(stdout);
  		if(tcbtable[sockfd]->usedBufLen >= length){         //data enough
  			pthread_mutex_lock(tcbtable[sockfd]->bufMutex);  			
	  		memcpy(buf,tcbtable[sockfd]->recvBuf,length);		//get data
	  		memcpy(tcbtable[sockfd]->recvBuf,tcbtable[sockfd]->recvBuf+length,tcbtable[sockfd]->usedBufLen-length);		//move data
	  		pthread_mutex_unlock(tcbtable[sockfd]->bufMutex);	  			  		
	  		tcbtable[sockfd]->usedBufLen -= length;      //buffer len minus the length fetched
	  		return 1;
	  	}
	  	usleep(RECVBUF_POLLING_INTERVAL*1000000);
  	}
}

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
int stcp_server_close(int sockfd) 
{
  if(tcbtable[sockfd]->state==CLOSED){
		printf("tcb freed!!!\n");
		free(tcbtable[sockfd]->recvBuf);
		pthread_mutex_destroy(tcbtable[sockfd]->bufMutex);		
		free(tcbtable[sockfd]);
		tcbtable[sockfd] = NULL;
		return 1;
	}
	else{
		printf("state error!!!\n");
		return -1;
	}
}

// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明到SIP进程的连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
void* seghandler(void* arg) 
{
	printf("**********in pthreadhandler******\n");fflush(stdout);
	seg_t* segBuf = (seg_t*)malloc(sizeof(seg_t));	
	server_tcb_t* cnn_tcb;
	int type;
	int tcb_no;	
	pthread_t time_pid;
	int rc;	
	int re_value;
	int src_node_id;
	memset(segBuf,0,sizeof(seg_t));

	while((re_value=sip_recvseg(sip_conn,&src_node_id,segBuf)) != -1){			
		if(re_value == 1){    // segment lost or checksum is wrong
			continue;
		}		

		tcb_no = find_tcb(segBuf->header);     //find the server tcb
		cnn_tcb = tcbtable[tcb_no];

		if(tcb_no==-1){
			printf("In seghandler___tcb error!!\n");
			//pthread_exit(NULL);
			continue;
		}

		type = segBuf->header.type;

		switch(cnn_tcb->state)
		{
			case CLOSED:
				printf("fsm error!!!\n");
				break;

			case LISTENING:
				if(type==SYN){
					printf("#### in LISTENING: recv a SYN!!!\n");          			      	
					cnn_tcb->client_portNum = segBuf->header.src_port;			
					send_segack(cnn_tcb,src_node_id,SYNACK);        			
					cnn_tcb->state = CONNECTED;        			
				}
				else{
					printf("LISTENING: bad segment!!!\n");
				}
				break;

			case CONNECTED:
				if(type==SYN){				//connect handshake
					printf("#### in CONNECTED: recv a SYN!!!\n");
					send_segack(cnn_tcb,src_node_id,SYNACK);
				}

				else if(type == FIN){		//connect handshake
					printf("#### in FSM: recv a FIN!!!\n");
					send_segack(cnn_tcb,src_node_id,FINACK);
					cnn_tcb->state = CLOSEWAIT;

					rc = pthread_create(&time_pid,NULL,countTime,(void*)tcb_no);
					if(rc){
						printf("error;return code from pthread_create() is %d:\n",rc);		
					}
					else{
						printf("count time pthread create successfully!!\n");
					}        			
				}

				else if(type == DATA){        			
					printf("&&&&&& socket%d recv seq_num :%d-----serv expect_seqNum : %d\n",tcb_no, segBuf->header.seq_num, cnn_tcb->expect_seqNum);
					// seqNum is correct
					if(segBuf->header.seq_num == cnn_tcb->expect_seqNum){          
						//copy to the tcb buffer        				
						pthread_mutex_lock(cnn_tcb->bufMutex);        				
						memcpy(cnn_tcb->recvBuf+cnn_tcb->usedBufLen,segBuf->data,segBuf->header.length);        				
						pthread_mutex_unlock(cnn_tcb->bufMutex);        				

						//        				printf("now server buf len is :%d\n", strlen(cnn_tcb->recvBuf));
						cnn_tcb->usedBufLen += segBuf->header.length;
						//       				printf("~~~~~~~~~~~~~~~~~before expeced seq_num is :%d \n", cnn_tcb->expect_seqNum);        				
						cnn_tcb->expect_seqNum += segBuf->header.length;        			
						//     				printf("~~~~~~~~~~~~~~~~~after expeced seq_num is :%d \n", cnn_tcb->expect_seqNum);	
						send_segack(cnn_tcb,src_node_id,DATAACK);                 //ack the data        				
					}
					// seqNum is wrong
					else{			
						send_segack(cnn_tcb,src_node_id,DATAACK);   		//send DATAACK with original seqnum
					}
				}

				else{
					printf("CONNECTED: bad segment\n");
				}
				break;

			case CLOSEWAIT:        		        		
				if(type==FIN){
					printf("#### in CLOSEWAIT: recv a FIN!!!\n");
					send_segack(cnn_tcb,src_node_id,FINACK);        		
				}
				else{
					printf("CLOSEWAIT: bad segment!!!\n");
				}	        		        		
				break;        		        		        	        		    			        		

			default:
				printf("impossible situation!!\n");
				break;
		}        
		memset(segBuf,0,sizeof(seg_t));
		type = -1;               
	}
	printf("seghandler ended!!\n");fflush(stdout);
	pthread_exit(NULL);
}

/**
*send ack
*seq_num_sign 1:seq_num is correct;
*/
void send_segack(server_tcb_t* server_tcb,int dst_nodeID,int ack_type)
{
	seg_t seg_ack;	
	memset(&seg_ack,0,sizeof(seg_t));
	assert(server_tcb->client_portNum != 0);
	seg_ack.header.src_port = server_tcb->server_portNum;
	seg_ack.header.dest_port = server_tcb->client_portNum;
	seg_ack.header.type = ack_type;
	if(ack_type == DATAACK){   //ack the data received
		seg_ack.header.ack_num = server_tcb->expect_seqNum;
	}
	sip_sendseg(sip_conn,dst_nodeID,&seg_ack);
	return;
}


/**
*count closewait time
*/
void *countTime(void* arg)
{	
	if(tcbtable[(int)arg]->state != CLOSEWAIT){
		printf("error state!!!\n");
	}
	usleep(CLOSEWAIT_TIMEOUT*1000000);
	printf("CLOSEWAIT_TIMEOUT state change!!!\n");
	tcbtable[(int)arg]->state = CLOSED;
	tcbtable[(int)arg]->usedBufLen = 0;
	pthread_exit(NULL);
}



/**
*find the tcb according to the segBuf received
*/
int find_tcb(stcp_hdr_t hdr)
{
	int i = 0;
	// printf("in find_tcb port is:%d\n", hdr.dest_port);
	while(i<MAX_TRANSPORT_CONNECTIONS){		
		if(tcbtable[i]!=NULL){			
			if(tcbtable[i]->server_portNum == hdr.dest_port){
				printf("find the tcb!!!\n");fflush(stdout);
				return i;
			}
		}	
		i++;
	}
	printf("do not find the tcb!!!\n");	
	return -1;
}
