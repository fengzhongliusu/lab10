//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程  
//
//创建日期: 2013年1月

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "sip.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"

//SIP层等待这段时间让SIP路由协议建立路由路径. 
#define SIP_WAITTIME 60

/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 			//到重叠网络的连接
int stcp_conn;			//到STCP的连接
nbr_cost_entry_t* nct;			//邻居代价表
dv_t* dv;				//距离矢量表
pthread_mutex_t* dv_mutex;		//距离矢量表互斥量
routingtable_t* routingtable;		//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT.
//成功时返回连接描述符, 否则返回-1.
int connectToSON() { 
	int sockfd; 
	struct sockaddr_in dest;
	memset(&dest, 0, sizeof(dest));
	dest.sin_family=AF_INET;					 /*PF_INET为IPV4，internet协议，在<netinet/in.h>中，地址族*/ 
	dest.sin_addr.s_addr = inet_addr("127.0.0.1");
	dest.sin_port = htons(SON_PORT);			/*端口号,htons()返回一个以主机字节顺序表达的数。*/  

	if((sockfd = socket(AF_INET,SOCK_STREAM,0))<0){ /*AF_INEI套接字协议族，SOCK_STREAM套接字类型，调用socket函数来创建一个能够进行网络通信的套接字。这里判断是否创建成功*/  
        perror("raw socket created error");  
        return -1;  
       }  

    if (connect(sockfd, (const struct sockaddr *) &dest, sizeof(dest)) != 0) {
		perror("Problem in connnecting to the server");
		return -1;
	}

	printf("Client connect successful!\n");
	return sockfd;
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.路由更新报文包含这个节点
//的距离矢量.广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
void* routeupdate_daemon(void* arg) {
	int i;
	sip_pkt_t sip_pkt;	
	sip_hdr_t sip_hdr;
	pkt_routeupdate_t update_msg;

	while(1){
		sip_hdr.src_nodeID = topology_getMyNodeID();
		sip_hdr.dest_nodeID = BROADCAST_NODEID;
		sip_hdr.length =  sizeof(pkt_routeupdate_t);   //data len
		sip_hdr.type = ROUTE_UPDATE;

		/*update msg*/
		update_msg.entryNum = NODE_NUM;
		for(i=0; i<NODE_NUM; i++)
		{
			pthread_mutex_lock(dv_mutex);
			update_msg.entry[i].nodeID = dv[0].dvEntry[i].nodeID;
			update_msg.entry[i].cost = dv[0].dvEntry[i].cost;
			pthread_mutex_unlock(dv_mutex);
		}

		memcpy(&(sip_pkt.header),&(sip_hdr),sizeof(sip_hdr_t));
		memcpy(&(sip_pkt.data),&(update_msg),sizeof(pkt_routeupdate_t));


		if(son_sendpkt(BROADCAST_NODEID,&sip_pkt, son_conn)==-1) //TODO check for MEANING
		{
			perror("update routing msg send error!!\n");
		}
		sleep(ROUTEUPDATE_INTERVAL);
	}
}



//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表.
void* pkthandler(void* arg) {
	pkt_routeupdate_t update_pkt;
	sip_pkt_t sip_recv_pkt;
	seg_t seg_tcp;
	int next_node;
	while(1)
	{
		memset(&sip_recv_pkt,0,sizeof(sip_pkt_t));
		memset(&seg_tcp,0,sizeof(seg_t));
		memset(&update_pkt,0,sizeof(pkt_routeupdate_t));
	
		if(son_recvpkt(&sip_recv_pkt,son_conn)==-1){
			perror("pkthandler--->sip recv error!!\n");
		}	

		if(sip_recv_pkt.header.type == SIP)    //SIP报文
		{

			if(sip_recv_pkt.header.dest_nodeID == topology_getMyNodeID())   //目的节点
			{
				seg_tcp.header.src_port = 0;
				seg_tcp.header.dest_port = 0;
				seg_tcp.header.seq_num = 0;
				seg_tcp.header.ack_num = 0;
				seg_tcp.header.type = 0;
				seg_tcp.header.rcv_win = 0;
				seg_tcp.header.checksum = 0;
				seg_tcp.header.length = sip_recv_pkt.header.length;
				memcpy(&(seg_tcp.data),&(sip_recv_pkt.data),seg_tcp.header.length);
				seg_tcp.header.checksum = checksum(&seg_tcp);

				forwardsegToSTCP(stcp_conn,sip_recv_pkt.header.src_nodeID,&seg_tcp);  //发送给本地STCP
			}

			else						//转发
			{	//get next node id;
				pthread_mutex_lock(routingtable_mutex);
				next_node = routingtable_getnextnode(routingtable,sip_recv_pkt.header.dest_nodeID);	
				pthread_mutex_unlock(routingtable_mutex);

				son_sendpkt(next_node, &sip_recv_pkt,son_conn);
			}

		}

		else if(sip_recv_pkt.header.type == ROUTE_UPDATE)			//更新路由表
		{
			printf("pkthandler--->update msg recved!!\n"); fflush(stdout);
			memcpy(&update_pkt, &sip_recv_pkt.data, sizeof(pkt_routeupdate_t));
			update_table(&(update_pkt),sip_recv_pkt.header.src_nodeID);	  
		}

		else
			perror("pkthandler--->error pkt from son");
	}

}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop() 
{
	printf("-------close stcp connect--------\n");	
	close(stcp_conn);
	printf("\n--------- close son connect --------\n");
	close(son_conn);
	
	pthread_mutex_destroy(dv_mutex);		
	pthread_mutex_destroy(routingtable_mutex);		
	
	printf("\n-------- free table ----------\n");
	nbrcosttable_destroy(nct);
	dvtable_destroy(dv);
	routingtable_destroy(routingtable);
	exit(1);
}


//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t. 
//接收的段被封装进数据报(一个段在一个数据报中), 然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.
void waitSTCP()
{	
	int n;
	int next_id;
	int listenfd;
	socklen_t clilen;
	struct sockaddr_in servaddr;
	struct sockaddr_in cliaddr;
	char buffer[MAX_SEG_LEN];
	sendseg_arg_t send_seg;
	sip_pkt_t sip_pkt;
	stcp_conn = -1;

	if((listenfd=socket(AF_INET,SOCK_STREAM,0))<0){
		printf("failed to create listen socket!!\n");
		exit(1);
	}

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SON_PORT);

	const int on = 1;
	setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
	bind(listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr));

	listen(listenfd,LISTENQ);	
	printf("wait to be connected!!\n");

	clilen = sizeof(cliaddr);

WAIT:	
	stcp_conn = accept(listenfd,(struct sockaddr*)&cliaddr,&clilen);
	printf("client connected!!\n");

	memset(&buffer,0,MAX_SEG_LEN);
	memset(&send_seg,0,sizeof(sendseg_arg_t));
	memset(&sip_pkt,0,sizeof(sip_pkt_t));
	while((n=recv(stcp_conn,buffer,MAX_SEG_LEN,0))>0)
	{
		memcpy(&send_seg,&buffer,sizeof(sendseg_arg_t));

		pthread_mutex_lock(routingtable_mutex);
		next_id = routingtable_getnextnode(routingtable,send_seg.nodeID);
		pthread_mutex_unlock(routingtable_mutex);

		sip_pkt.header.src_nodeID = topology_getMyNodeID();
		sip_pkt.header.dest_nodeID = send_seg.nodeID;
		sip_pkt.header.length = sizeof(sendseg_arg_t);
		sip_pkt.header.type = SIP;
		memcpy(&(sip_pkt.data),&send_seg,sizeof(sendseg_arg_t));

		if(son_sendpkt(next_id,&sip_pkt,son_conn) == -1)		//send to son
			perror("error sending to son from stcp!!\n");

		memset(&buffer,0,MAX_SEG_LEN);
		memset(&send_seg,0,sizeof(sendseg_arg_t));
		memset(&sip_pkt,0,sizeof(sip_pkt_t));
	}	

	printf("local stcp disconnect,waiting for another....\n");
	goto WAIT;
}



//update the dv_table and routing table
void update_table(pkt_routeupdate_t* update_pkt,int pass_id)
{
	int i,j;
	int dest_id = 0;
	int update_cost = 0;
	int temp_cost = 0;
	int origin_cost = 0;
	int dv_size = topology_getNbrNum()+1;   //size of dv_table
	
	printf("*************pass_id: %d****************\n",pass_id);
	for(i=0; i<update_pkt->entryNum; i++)
	{
		dest_id = update_pkt->entry[i].nodeID;
		update_cost = update_pkt->entry[i].cost;
		printf("dest id: %d cost: %d \n",dest_id,update_cost);

		for(j=0; j<dv_size; j++)
		{
			pthread_mutex_lock(dv_mutex);
			temp_cost = dvtable_getcost(dv,dv[j].nodeID,pass_id) + update_cost;
			origin_cost = dvtable_getcost(dv,dv[j].nodeID,dest_id);
			printf("-->srcID %d to destID %d origin cost %d ,if update,cost:%d\n",dv[j].nodeID,dest_id,origin_cost,temp_cost);
			pthread_mutex_unlock(dv_mutex);

			if(temp_cost < origin_cost)
			{
				printf("-----------------update distance vector table---------------\n");
				pthread_mutex_lock(dv_mutex);
				dvtable_setcost(dv,dv[j].nodeID,dest_id,temp_cost);    //update dvtabe
				dvtable_print(dv);
				pthread_mutex_unlock(dv_mutex);

				if(j == 0)			// the local node
				{
					printf("-----------------update routing table---------------\n");
					pthread_mutex_lock(routingtable_mutex);
					routingtable_setnextnode(routingtable,dest_id,pass_id);	 //update routingtable
					routingtable_print(routingtable);
					pthread_mutex_unlock(routingtable_mutex);
				}

			}

		}

	}

}



int main(int argc, char *argv[]) {
	printf("SIP layer is starting, pls wait...\n");

	//初始化全局变量
	nct = nbrcosttable_create();
	dv = dvtable_create();
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
	son_conn = -1;
	stcp_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到本地SON进程 
	son_conn = connectToSON();
	if(son_conn<0) {
		printf("can't connect to SON process\n");
		exit(1);		
	}
	
	//启动线程处理来自SON进程的进入报文 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("SIP layer is started...\n");
	printf("waiting for routes to be established\n");
	sleep(SIP_WAITTIME);
	routingtable_print(routingtable);

	//等待来自STCP进程的连接
	printf("waiting for connection from STCP process\n");
	waitSTCP(); 

}


