//文件名: server/app_stress_server.c

//描述: 这是压力测试版本的服务器程序代码. 服务器首先连接到本地SIP进程. 然后它调用stcp_server_init()初始化STCP服务器.
//它通过调用stcp_server_sock()和stcp_server_accept()创建套接字并等待来自客户端的连接. 它然后接收文件长度. 
//在这之后, 它创建一个缓冲区, 接收文件数据并将它保存到receivedtext.txt文件中.
//最后, 服务器通过调用stcp_server_close()关闭套接字, 并断开与本地SIP进程的连接.

//创建日期: 2013年1月

//输入: 无

//输出: STCP服务器状态

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include "../common/constants.h"
#include "stcp_server.h"

//创建一个连接, 使用客户端端口号87和服务器端口号88. 
#define CLIENTPORT1 87
#define SERVERPORT1 88
//在接收的文件数据被保存后, 服务器等待15秒, 然后关闭连接.
#define WAITTIME 130

//这个函数连接到本地SIP进程的端口SIP_PORT. 如果TCP连接失败, 返回-1. 连接成功, 返回TCP套接字描述符, STCP将使用该描述符发送段.
int connectToSIP() {

		int sockfd; 
		struct sockaddr_in dest;
		memset(&dest, 0, sizeof(dest));
		dest.sin_family=AF_INET;  /*PF_INET为IPV4，internet协议，在<netinet/in.h>中，地址族*/ 
		dest.sin_addr.s_addr = inet_addr("127.0.0.1");
		dest.sin_port = htons(SIP_PORT);   /*端口号,htons()返回一个以主机字节顺序表达的数。*/  

	/*AF_INEI套接字协议族，SOCK_STREAM套接字类型，调用socket函数来创建一个能够进行网络通信的套接字。判断是否创建成功*/
		if((sockfd = socket(AF_INET,SOCK_STREAM,0))<0){   
			perror("raw socket created error");  
			return -1;  
		}  
		if (connect(sockfd, (const struct sockaddr *) &dest, sizeof(dest)) != 0) {
			perror("Problem in connnecting to the server");
			return -1;
		}

		system("clear");
		printf("Client connect successful!\n");

		return sockfd;
}

//这个函数断开到本地SIP进程的TCP连接. 
void disconnectToSIP(int sip_conn) {

	close(sip_conn);
	printf("server--->close socket!\n");
}

int main() {
	//用于丢包率的随机数种子
	srand(time(NULL));

	//连接到SIP进程并获得TCP套接字描述符
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("can not connect to the local SIP process\n");
	}

	//初始化STCP服务器
	stcp_server_init(sip_conn);

	//在端口SERVERPORT1上创建STCP服务器套接字 
	int sockfd= stcp_server_sock(SERVERPORT1);
	if(sockfd<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//监听并接受来自STCP客户端的连接 
	stcp_server_accept(sockfd);

	//首先接收文件长度, 然后接收文件数据
	int fileLen;
	stcp_server_recv(sockfd,&fileLen,sizeof(int));
	char* buf = (char*) malloc(fileLen);
	stcp_server_recv(sockfd,buf,fileLen);

	//将接收到的文件数据保存到文件receivedtext.txt中
	FILE* f;
	f = fopen("receivedtext.txt","a");
	fwrite(buf,fileLen,1,f);
	fclose(f);
	free(buf);

	//等待一会儿
	sleep(WAITTIME);

	//关闭STCP服务器 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				

	//断开与SIP进程之间的连接
	disconnectToSIP(sip_conn);
}
