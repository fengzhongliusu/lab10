//�ļ���: client/stcp_client.c
//
//����: ����ļ�����STCP�ͻ��˽ӿ�ʵ�� 
//
//��������: 2013��1��

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

//����tcbtableΪȫ�ֱ���
client_tcb_t* tcbtable[MAX_TRANSPORT_CONNECTIONS];
//������SIP���̵�TCP����Ϊȫ�ֱ���
int sip_conn;

/*********************************************************************/
//
//STCP APIʵ��
//
/*********************************************************************/

// ���������ʼ��TCB��, ��������Ŀ���ΪNULL.  
// �������TCP�׽���������conn��ʼ��һ��STCP���ȫ�ֱ���, �ñ�����Ϊsip_sendseg��sip_recvseg���������.
// ���, �����������seghandler�߳�����������STCP��. �ͻ���ֻ��һ��seghandler.
void stcp_client_init(int conn) 
{
	int i ;
	int rc ;//��¼�����߳��Ƿ�ɹ�
	pthread_t tid;

	sip_conn = conn ;

	for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; ++i)
	{
		tcbtable[i] = NULL;
	}

	rc = pthread_create(&tid,NULL,seghandler,NULL);

	if (rc)//����ʧ��
	{		
		printf("ERROR ; return code from pthread_create() %d\n", rc);
		exit(-1);
	}
	else//�����ɹ�
		printf("seghandler thread Create sucess ! ! !\n");

}

// ����������ҿͻ���TCB�����ҵ���һ��NULL��Ŀ, Ȼ��ʹ��malloc()Ϊ����Ŀ����һ���µ�TCB��Ŀ.
// ��TCB�е������ֶζ�����ʼ��. ����, TCB state������ΪCLOSED���ͻ��˶˿ڱ�����Ϊ�������ò���client_port. 
// TCB������Ŀ��������Ӧ��Ϊ�ͻ��˵����׽���ID�������������, �����ڱ�ʶ�ͻ��˵�����. 
// ���TCB����û����Ŀ����, �����������-1.
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

// ��������������ӷ�����. �����׽���ID, �������ڵ�ID�ͷ������Ķ˿ں���Ϊ�������. �׽���ID�����ҵ�TCB��Ŀ.  
// �����������TCB�ķ������ڵ�ID�ͷ������˿ں�,  Ȼ��ʹ��sip_sendseg()����һ��SYN�θ�������.  
// �ڷ�����SYN��֮��, һ����ʱ��������. �����SYNSEG_TIMEOUTʱ��֮��û���յ�SYNACK, SYN �ν����ش�. 
// ����յ���, �ͷ���1. ����, ����ش�SYN�Ĵ�������SYN_MAX_RETRY, �ͽ�stateת����CLOSED, ������-1.
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
	segment.header.length = 0;//dataΪ0
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

// �������ݸ�STCP������. �������ʹ���׽���ID�ҵ�TCB���е���Ŀ.
// Ȼ����ʹ���ṩ�����ݴ���segBuf, �������ӵ����ͻ�����������.
// ������ͻ������ڲ�������֮ǰΪ��, һ����Ϊsendbuf_timer���߳̾ͻ�����.
// ÿ��SENDBUF_ROLLING_INTERVALʱ���ѯ���ͻ������Լ���Ƿ��г�ʱ�¼�����. 
// ��������ڳɹ�ʱ����1�����򷵻�-1. 
// stcp_client_send��һ����������������.
// ��Ϊ�û����ݱ���ƬΪ�̶���С��STCP��, ����һ��stcp_client_send���ÿ��ܻ�������segBuf
// ����ӵ����ͻ�����������. ������óɹ�, ���ݾͱ�����TCB���ͻ�����������, ���ݻ������ڵ����,
// ���ݿ��ܱ����䵽������, ���ڶ����еȴ�����.
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

// ����������ڶϿ���������������. �����׽���ID��Ϊ�������. �׽���ID�����ҵ�TCB���е���Ŀ.  
// �����������FIN�θ�������. �ڷ���FIN֮��, state��ת����FINWAIT, ������һ����ʱ��.
// ��������ճ�ʱ֮ǰstateת����CLOSED, �����FINACK�ѱ��ɹ�����. ����, ����ھ���FIN_MAX_RETRY�γ���֮��,
// state��ȻΪFINWAIT, state��ת����CLOSED, ������-1.
int stcp_client_disconnect(int sockfd) 
{
	printf("FUNC->stcp_client_disconnect\n");

	seg_t *segment = (seg_t *)malloc(sizeof(seg_t));

	segment->header.src_port = tcbtable[sockfd]->client_portNum;
	segment->header.dest_port = tcbtable[sockfd]->server_portNum;
	segment->header.seq_num = 0;
	// tcbtable[sockfd]->next_seqNum++;
	segment->header.ack_num = 0 ;
	segment->header.length = 0;//dataΪ0
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

// �����������free()�ͷ�TCB��Ŀ. ��������Ŀ���ΪNULL, �ɹ�ʱ(��λ����ȷ��״̬)����1,
// ʧ��ʱ(��λ�ڴ����״̬)����-1.
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

	pthread_mutex_destroy(tcbtable[sockfd]->bufMutex);//�ͷŻ���ṹ
	free(tcbtable[sockfd]);

	tcbtable[sockfd] = NULL;
	printf("SOCKET AT %d Closed successful !\n" , sockfd);
	return 1;
}

// ������stcp_client_init()�������߳�. �������������Է������Ľ����. 
// seghandler�����Ϊһ������sip_recvseg()������ѭ��. ���sip_recvseg()ʧ��, ��˵����SIP���̵������ѹر�,
// �߳̽���ֹ. ����STCP�ε���ʱ����������״̬, ���Բ�ȡ��ͬ�Ķ���. ��鿴�ͻ���FSM���˽����ϸ��.
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
					printf("ȫ����������ȫ��ȷ��\n");
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
					printf("notȫ������\n");
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
					// 	printf("ȫ��ȷ��\n");
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


//����̳߳�����ѯ���ͻ������Դ�����ʱ�¼�. ������ͻ������ǿ�, ��Ӧһֱ����.
//���(��ǰʱ�� - ��һ���ѷ��͵�δ��ȷ�϶εķ���ʱ��) > DATA_TIMEOUT, �ͷ���һ�γ�ʱ�¼�.
//����ʱ�¼�����ʱ, ���·��������ѷ��͵�δ��ȷ�϶�. �����ͻ�����Ϊ��ʱ, ����߳̽���ֹ.
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
		if((currenttime - tcbtable_tem->sendBufHead->sentTime)> DATA_TIMEOUT/1000000 )//��ʱ�¼�����
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

