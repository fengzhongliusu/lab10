// 文件名: common/pkt.c
// 创建日期: 2013年

#include "pkt.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
// son_sendpkt()由SIP进程调用, 其作用是要求SON进程将报文发送到重叠网络中. SON进程和SIP进程通过一个本地TCP连接互连.
// 在son_sendpkt()中, 报文及其下一跳的节点ID被封装进数据结构sendpkt_arg_t, 并通过TCP连接发送给SON进程. 
// 参数son_conn是SIP进程和SON进程之间的TCP连接套接字描述符.
// 当通过SIP进程和SON进程之间的TCP连接发送数据结构sendpkt_arg_t时, 使用'!&'和'!#'作为分隔符, 按照'!& sendpkt_arg_t结构 !#'的顺序发送.
// 如果发送成功, 返回1, 否则返回-1.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn)
{
	//printf("func-->son_sendpkt()\n");
	char start[2] = "!&";
	char stop[2] = "!#";
	sendpkt_arg_t sendbuf;
	sendbuf.nextNodeID = nextNodeID;
	memcpy(&(sendbuf.pkt),pkt,pkt->header.length + 12);
	if (send(son_conn , start , 2, 0 ) < 0 )
	{
		printf("23->son_sendpkt() 发送!&fail\n");
		return -1;
	}
	if (send(son_conn , &sendbuf , pkt->header.length + 16 , 0 ) < 0)
	{
		printf("28->son_sendpkt 发送!&pkt fail\n");
		return -1;
	}
	if (send(son_conn , stop , 2, 0 ) < 0 )
	{
		printf("33->son_sendpkt 发送!# fail\n");
		return -1;
	}
  return 1;
}

// son_recvpkt()函数由SIP进程调用, 其作用是接收来自SON进程的报文. 
// 参数son_conn是SIP进程和SON进程之间TCP连接的套接字描述符. 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
  //printf("func-->son_recvpkt()\n");

  int  state ;
  char recvbuf[sizeof(sip_pkt_t)];
  int index = 0;
  char recvchar;
  state = SEGSTART1;
  for (;;)
  {

  	if (recv(son_conn,&recvchar,1,0) <= 0)//关闭连接
  	{
  		return -1;//连接关闭，在上层函数调用出现这种情况时，应终止线程
  	}

  	switch(state)
  	{
  		case SEGSTART1:
  			 if (recvchar == '!')
  			 {
  			 	/* code */
  			 	state = SEGSTART2;
  			 }
  			 break;
  		case SEGSTART2:
  			if (recvchar == '&')
  			 {
  			 	/* code */
  			 	state = SEGRECV;
  			 }
  			 break;
  		case SEGRECV:
  			if (recvchar == '!')
  			 {
  			 	/* code */
  			 	state = SEGSTOP1;
  			 }
  			 else
  			 {
  			 	recvbuf[index] = recvchar;
  			 	index++;
  			 	if (index > sizeof(sip_pkt_t))
  			 	{
            		printf("超出范围，直接当丢失处理!!!\n");
  			 		return -1;//超出范围，直接当丢失处理
  			 	}
  			 }
  			 break;
  		case SEGSTOP1:
  			if (recvchar == '#')
  			 {
  			 	memset((void *)pkt , 0 , sizeof(sip_pkt_t));
  			 	memcpy(pkt, recvbuf, index);
        
  			 		return 1 ;//接收成功
  			 }
  			 else
  			 {
  			 	recvbuf[index] = '!';
	            index++;
	            if (recvchar == '!')
	            {
	              state = SEGSTOP1;
	            }
	            else
	            {
	              recvbuf[index] = recvchar;
	              index++;
	              state = SEGRECV;
	            }
  			 }
  			 break;
  		default: 
  		 		printf( "No such state\n ");
  		 		return -1;//没有出现该状态，当丢失处理
  	}
  }
}

// 这个函数由SON进程调用, 其作用是接收数据结构sendpkt_arg_t.
// 报文和下一跳的节点ID被封装进sendpkt_arg_t结构.
// 参数sip_conn是在SIP进程和SON进程之间的TCP连接的套接字描述符. 
// sendpkt_arg_t结构通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 如果成功接收sendpkt_arg_t结构, 返回1, 否则返回-1.

int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn)
{
   // printf("调用getpktToSend函数\n");

  int  state ;
  char recvbuf[sizeof(sip_pkt_t)];
  int index = 0;
  char recvchar;
  state = SEGSTART1;
  for (;;)
  {

  	if (recv(sip_conn,&recvchar,1,0) <= 0)//关闭连接
  	{
  		return -1;//连接关闭，在上层函数调用出现这种情况时，应终止线程
  	}

  	switch(state)
  	{
  		case SEGSTART1:
  			 if (recvchar == '!')
  			 {
  			 	state = SEGSTART2;
  			 }
  			 break;
  		case SEGSTART2:
  			if (recvchar == '&')
  			 {
  			 	/* code */
  			 	state = SEGRECV;
  			 }
  			 break;
  		case SEGRECV:
  			if (recvchar == '!')
  			 {
  			 	/* code */
  			 	state = SEGSTOP1;
  			 }
  			 else
  			 {
  			 	recvbuf[index] = recvchar;
  			 	index++;
  			 	if (index > sizeof(sip_pkt_t))
  			 	{
            		printf("超出范围，直接当丢失处理!!!\n");
  			 		return -1;//超出范围，直接当丢失处理
  			 	}
  			 }
  			 break;
  		case SEGSTOP1:
  			if (recvchar == '#')
  			 {          
  			 	memset((void *)pkt , 0 , sizeof(sip_pkt_t));
  			 	memcpy(pkt, recvbuf + 4 , index-4);
        		memcpy(nextNode , recvbuf , 4);
            if(pkt->header.type != 1)   //sip pkt
              printf("pkt.c 191 getpktToSend() get a pkt from sip\n");
  			 	return 1 ;//接收成功
  			 }
  			 else
  			 {
  			 	recvbuf[index] = '!';
	            index++;
	            if (recvchar == '!')
	            {
	              state = SEGSTOP1;
	            }
	            else
	            {
	              recvbuf[index] = recvchar;
	              index++;
	              state = SEGRECV;
	            }
  			 }
  			 break;
  		default: 
  		 		printf( "No such state\n ");
  		 		return -1;//没有出现该状态，当丢失处理
  	}
  }
}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的. 
// SON进程调用这个函数将报文转发给SIP进程. 
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符. 
// 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
  // printf("调用forwardpktToSIP函数\n");
	char start[2] = "!&";
	char stop[2] = "!#";
	sip_pkt_t sendbuf;
	memcpy(&(sendbuf),pkt,sizeof(sip_pkt_t));
	if (send(sip_conn , start , 2, 0 ) < 0 )
	{
		printf("235->forwardpktToSIP 发送 !& fail\n");
		return -1;
	}
	if (send(sip_conn , &sendbuf , sizeof(sip_pkt_t), 0 ) < 0)
	{
		printf("240->forwardpktToSIP 发送 pkt fail\n");
		return -1;
	}
	if (send(sip_conn , stop , 2, 0 ) < 0 )
	{
		printf("245->forwardpktToSIP发送 !# fail\n");
		return -1;
	}
  return 1;
}

// sendpkt()函数由SON进程调用, 其作用是将接收自SIP进程的报文发送给下一跳.
// 参数conn是到下一跳节点的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居节点之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
  	//printf("调用sendpkt函数\n");
	char start[2] = "!&";
	char stop[2] = "!#";
	sip_pkt_t sendbuf;
	memcpy(&(sendbuf),pkt,pkt->header.length + 12);
	if (send(conn , start , 2, 0 ) < 0 )
	{
		printf("264->sendpkt 发送!& fail socket %d\n",conn);
		return -1;
	}
	if (send(conn , &sendbuf , pkt->header.length + 12 , 0 ) < 0)
	{
		printf("269->sendpkt 发送pkt fail\n");
		return -1;
	}
	if (send(conn , stop , 2, 0 ) < 0 )
	{
		printf("274->sendpkt 发送!# fail\n");
		return -1;
	}
  return 1;
}

// recvpkt()函数由SON进程调用, 其作用是接收来自重叠网络中其邻居的报文.
// 参数conn是到其邻居的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int recvpkt(sip_pkt_t* pkt, int conn)
{
  //printf("调用recvpkt函数\n");
  int  state ;
  char recvbuf[sizeof(sip_pkt_t)];
  int index = 0;
  char recvchar;
  state = SEGSTART1;
  for (;;)
  {

  	if (recv(conn,&recvchar,1,0) <= 0)//关闭连接
  	{
  		return -1;//连接关闭，在上层函数调用出现这种情况时，应终止线程
  	}

  	switch(state)
  	{
  		case SEGSTART1:
  			 if (recvchar == '!')
  			 {
  			 	state = SEGSTART2;
  			 }
  			 break;
  		case SEGSTART2:
  			if (recvchar == '&')
  			 {
  			 	/* code */
  			 	state = SEGRECV;
  			 }
  			 break;
  		case SEGRECV:
  			if (recvchar == '!')
  			 {
  			 	/* code */
  			 	state = SEGSTOP1;
  			 }
  			 else
  			 {
  			 	recvbuf[index] = recvchar;
  			 	index++;
  			 	if (index > sizeof(sip_pkt_t))
  			 	{
            		printf("超出范围，直接当丢失处理!!!\n");
  			 		return -1;//超出范围，直接当丢失处理
  			 	}
  			 }
  			 break;
  		case SEGSTOP1:
  			if (recvchar == '#')
  			 {          
  			 	memset((void *)pkt , 0 , sizeof(sip_pkt_t));
  			 	memcpy(pkt, recvbuf, index);
          // printf("srcID IS : %d\n" , pkt->header.src_nodeID);
          // printf("TYPE IS  : %d\n", pkt->header.type);
  			 	return 1 ;//接收成功
  			 }
  			 else
  			 {
  			 	recvbuf[index] = '!';
	            index++;
	            if (recvchar == '!')
	            {
	              state = SEGSTOP1;
	            }
	            else
	            {
	              recvbuf[index] = recvchar;
	              index++;
	              state = SEGRECV;
	            }
  			 }
  			 break;
  		default: 
  		 		printf( "No such state\n ");
  		 		return -1;//没有出现该状态，当丢失处理
  	}
  }
}
