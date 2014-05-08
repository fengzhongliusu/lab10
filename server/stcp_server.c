//�ļ���: server/stcp_server.c
//
//����: ����ļ�����STCP�������ӿ�ʵ��. 
//
//��������: 2013��1��

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

//����tcbtableΪȫ�ֱ���
server_tcb_t* tcbtable[MAX_TRANSPORT_CONNECTIONS];
//������SIP���̵�����Ϊȫ�ֱ���
int sip_conn;

/*********************************************************************/
//
//STCP APIʵ��
//
/*********************************************************************/

// ���������ʼ��TCB��, ��������Ŀ���ΪNULL. �������TCP�׽���������conn��ʼ��һ��STCP���ȫ�ֱ���, 
// �ñ�����Ϊsip_sendseg��sip_recvseg���������. ���, �����������seghandler�߳�����������STCP��.
// ������ֻ��һ��seghandler.
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

// ����������ҷ�����TCB�����ҵ���һ��NULL��Ŀ, Ȼ��ʹ��malloc()Ϊ����Ŀ����һ���µ�TCB��Ŀ.
// ��TCB�е������ֶζ�����ʼ��, ����, TCB state������ΪCLOSED, �������˿ڱ�����Ϊ�������ò���server_port. 
// TCB������Ŀ������Ӧ��Ϊ�����������׽���ID�������������, �����ڱ�ʶ�������˵�����. 
// ���TCB����û����Ŀ����, �����������-1.
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

// �������ʹ��sockfd���TCBָ��, �������ӵ�stateת��ΪLISTENING. ��Ȼ��������ʱ������æ�ȴ�ֱ��TCB״̬ת��ΪCONNECTED 
// (���յ�SYNʱ, seghandler�����״̬��ת��). �ú�����һ������ѭ���еȴ�TCB��stateת��ΪCONNECTED,  
// ��������ת��ʱ, �ú�������1. �����ʹ�ò�ͬ�ķ�����ʵ�����������ȴ�.
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

// ��������STCP�ͻ��˵�����. �������ÿ��RECVBUF_POLLING_INTERVALʱ��
// �Ͳ�ѯ���ջ�����, ֱ���ȴ������ݵ���, ��Ȼ��洢���ݲ�����1. ����������ʧ��, �򷵻�-1.
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

// �����������free()�ͷ�TCB��Ŀ. ��������Ŀ���ΪNULL, �ɹ�ʱ(��λ����ȷ��״̬)����1,
// ʧ��ʱ(��λ�ڴ����״̬)����-1.
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

// ������stcp_server_init()�������߳�. �������������Կͻ��˵Ľ�������. seghandler�����Ϊһ������sip_recvseg()������ѭ��, 
// ���sip_recvseg()ʧ��, ��˵����SIP���̵������ѹر�, �߳̽���ֹ. ����STCP�ε���ʱ����������״̬, ���Բ�ȡ��ͬ�Ķ���.
// ��鿴�����FSM���˽����ϸ��.
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
