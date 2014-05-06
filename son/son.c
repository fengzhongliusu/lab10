//文件名: son/son.c
//
//描述: 这个文件实现SON进程 
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程. 
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中. 
//
//创建日期: 2013年

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "son.h"
#include "../topology/topology.h"
#include "neighbortable.h"

//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 60
/**************************************************************/
//声明全局变量
/**************************************************************/

//将邻居表声明为一个全局变量 
nbr_entry_t* nt; 
//将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn; 

/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止. 
void* waitNbrs(void* arg){
	printf("等待节点ID比自己大的所有邻居的进入连接\n");
	struct sockaddr_in cliaddr, dest; //客户端和服务器端的socket
	int listenfd, connfd;
	int i;
	int count = topology_getNbrNum();
	int num = 0;
	int myID = topology_getMyNodeID();

	for(i = 0 ;i < count;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}
	for (i = 0; i < count; ++i)
	{
		if (myID < nt[i].nodeID)
		{
			num++;
		}
	}

	i = num;
	socklen_t clilen;//用户
	memset(&dest, 0, sizeof(dest));
	dest.sin_family=AF_INET;  /*PF_INET为IPV4，internet协议，在<netinet/in.h>中，地址族*/ 
	dest.sin_addr.s_addr = htonl(INADDR_ANY);//等待节点ID比自己大的所有邻居的进入连接
	dest.sin_port = htons(CONNECTION_PORT);   /*端口号,htons()返回一个以主机字节顺序表达的数。*/  

	if((listenfd = socket(AF_INET,SOCK_STREAM,0))<0){ /*AF_INEI套接字协议族，SOCK_STREAM套接字类型，调用socket函数来创建一个能够进行网络通信的套接字。这里判断是否创建成功*/  
        perror("raw socket created error");  
        exit(1);  
       } 
       
    const int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bind(listenfd,(struct  sockaddr *)&dest,sizeof(dest));
    listen(listenfd,num);
    printf("waiting for connections.\n");
    num = i ;
    printf("num is %d\n", num);
    for (i = 0; i < num; ++i)
    {
    	clilen = sizeof(cliaddr);
    	printf("accept ID : %d myID : %d\n", nt[i].nodeID , myID);
    	connfd = accept(listenfd,(struct sockaddr *)&cliaddr, &clilen);
    	printf("connfd is : %d\n", connfd);
    	printf("client IP is :%s\n", inet_ntoa(cliaddr.sin_addr));
    	for (i = 0; i < count; ++i)
    	{
    		if (nt[i].nodeIP == inet_addr(inet_ntoa(cliaddr.sin_addr)))
    		{
    			printf("find client in nt at %d\n", i);
    			break;
    		}
    			
    	}

    	if (i == count)
    	{
    		exit(1);
    	}
    	if (nt_addconn(&nt[i],nt[i].nodeID,connfd)==-1)
    	{
    		printf("创建失败 at %d\n" , nt[i].nodeID);
    	}

    }

    printf("waitNbrs is Over\n");
    close(listenfd);
    pthread_exit(NULL);
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {
	printf("连接到节点ID比自己小的所有邻居\n");	
	int myID = topology_getMyNodeID();
	int num = topology_getNbrNum();
	int i;
	for(i=0;i<num;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}

	for (i = 0; i < num; ++i)
	{
		if (myID > nt[i].nodeID)
		{
			int sockfd;	struct sockaddr_in dest;
			printf( "找到一个比自己小的邻居 %d 进行连接 myID : %d ,neighborID %d \n", i , myID , nt[i].nodeID);
			memset(&dest, 0, sizeof(dest));
			dest.sin_family=AF_INET;  /*PF_INET为IPV4，internet协议，在<netinet/in.h>中，地址族*/ 
			dest.sin_port = htons(CONNECTION_PORT);   /*端口号,htons()返回一个以主机字节顺序表达的数。*/
			printf("%s\n", inet_ntoa((*(struct in_addr *)&(nt[i].nodeIP))));
			dest.sin_addr.s_addr = inet_addr(inet_ntoa((*(struct in_addr *)&(nt[i].nodeIP))));
			// dest.sin_addr.s_addr = nt[i].nodeIP;
			if(( sockfd = socket (AF_INET, SOCK_STREAM, 0 )) < 0 )
			{
				perror("raw socket created error");  
	       		exit(1); 
			}
			if (connect( sockfd, (struct sockaddr *) &dest, sizeof(dest)) < 0)
			{
				perror("connect 失败\n");
				return -1;
			}
			if (nt_addconn(&nt[i],nt[i].nodeID,sockfd)==-1)
	    	{
	    		printf("创建失败 at %d\n" , nt[i].nodeID);
	    	}
		}
	}
	return 1;

}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的. 
void* listen_to_neighbor(void* arg)
{
	printf("持续接收来自一个邻居的报文\n");
	int index = *((int *)arg);
	assert(nt[index].nodeID != -1);
	sip_pkt_t *revbuf = (sip_pkt_t *)malloc(sizeof( sip_pkt_t));
	while(recvpkt(revbuf,nt[index].conn) == 1)
	forwardpktToSIP(revbuf , sip_conn);
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP(){
	printf("等待来自本地SIP进程的进入连接\n");
	struct sockaddr_in  dest , cliaddr; //客户端和服务器端的socket
	int listenfd;
	int i;
	int num = topology_getNbrNum();
	socklen_t clilen;//用户
	memset(&dest, 0, sizeof(dest));
	dest.sin_family=AF_INET; 
	dest.sin_addr.s_addr = htonl(INADDR_ANY);
	dest.sin_port = htons(SON_PORT);   /*端口号,htons()返回一个以主机字节顺序表达的数。*/  

	if((listenfd = socket(AF_INET,SOCK_STREAM,0))<0){ /*AF_INEI套接字协议族，SOCK_STREAM套接字类型，调用socket函数来创建一个能够进行网络通信的套接字。这里判断是否创建成功*/  
        perror("raw socket created error");  
        exit(1);  
       }

    const int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bind(listenfd,(struct  sockaddr *)&dest,sizeof(dest));
    listen(listenfd,LISTEN_NUM);
    printf("waiting for connections.\n");

    clilen = sizeof(cliaddr);
   	sip_conn = accept(listenfd,(struct sockaddr *)&cliaddr , &clilen);

   	if (sip_conn < 0 )
   	{
   		printf("accept sip is wrong!\n");
   		return;
   	}
   	printf("sip_conn is %d\n", sip_conn);
   	sip_pkt_t* pkt = (sip_pkt_t *)malloc(sizeof(sip_pkt_t));
   	int* nextNode = (int *)malloc(4);


   	while(getpktToSend(pkt,nextNode,sip_conn) > 0)
   	{
   		
   		if ((*nextNode) == BROADCAST_NODEID)
   		{
   			printf("收到BROADCAST_NODEID进行群发\n");
   			for (i = 0; i < num; i++)
   			{
				sendpkt(pkt, nt[i].conn);
			}
   		}
   		else
   		{
   			for (i = 0; i < num; i++)
   			{
				if (*nextNode == nt[i].nodeID)
					break;
			}
			sendpkt(pkt, nt[i].conn);
   		}
   	}
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() {
	int num = topology_getNbrNum();
	int i;
	for (i = 0; i < num; ++i)
	{
		close(nt[i].conn);
	}
	free(nt);
	close(sip_conn);
	exit(1);
}

int main() {
	//启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n",topology_getMyNodeID());	

	//创建一个邻居表
	nt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接
	sip_conn = -1;
	
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);

	//打印所有邻居
	int nbrNum = topology_getNbrNum();
	int i;
	for(i=0;i<nbrNum;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}

	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread,NULL,waitNbrs,(void*)0);

	//等待其他节点启动
	sleep(SON_START_DELAY);
	
	//连接到节点ID比自己小的所有邻居
	connectNbrs();

	//等待waitNbrs线程返回
	pthread_join(waitNbrs_thread,NULL);	

	//此时, 所有与邻居之间的连接都建立好了
	
	//创建线程监听所有邻居
	for(i=0;i<nbrNum;i++) {
		int* idx = (int*)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread,NULL,listen_to_neighbor,(void*)idx);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");

	//等待来自SIP进程的连接
	waitSIP();
}
