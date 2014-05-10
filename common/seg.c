#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

#include "seg.h"

//STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
//参数sip_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t发送成功,就返回1,否则返回-1.
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segPtr)
{
	sendseg_arg_t  send_seg;
	segPtr->header.checksum = 0;
	segPtr->header.checksum = checksum(segPtr);

	send_seg.nodeID = dest_nodeID;
	memcpy(&(send_seg.seg),segPtr,sizeof(seg_t));	
	if(send(sip_conn,&send_seg,sizeof(sendseg_arg_t),0)<0){
		perror("send to sip error!!\n");
		return -1;
	}
	return 1;
}

//STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
//参数sip_conn是STCP进程和SIP进程之间连接的TCP描述符.
//当接收到段时, 使用seglost()来判断该段是否应被丢弃并检查校验和.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int sip_recvseg(int sip_conn, int* src_nodeID, seg_t* segPtr)
{
	int n;
	char buffer[sizeof(sendseg_arg_t)+1];
	sendseg_arg_t recv_seg;

	if((n=recv(sip_conn,buffer,sizeof(sendseg_arg_t)+1,0)) <=0 ){
		perror("stcp recv seg from sip error!!!\n");
		return -1;
	}
	
	printf("sip_recv--->recv size:%d and %d\n",n,sizeof(sendseg_arg_t));
	memcpy(&recv_seg,buffer,sizeof(sendseg_arg_t));

	if(seglost(&recv_seg.seg)==1)
	{
		perror("seg from sip is lost!!!\n");
		return 1;
	}
	// else
	// {
	// 	// if(checkchecksum(&recv_seg.seg) == -1)
	// 	// {
	// 	// 	perror("seg recv from sip checksum is wrong!!!\n");
	// 	// 	return 1;
	// 	// }		
	// }

	printf("sip_recv----->>get seg form sip!!\n");
	memcpy(segPtr,&recv_seg.seg,sizeof(seg_t));
	*src_nodeID = recv_seg.nodeID;
//	memcpy(src_nodeID,&recv_seg.nodeID,sizeof(int));

	return 0;
}

//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr)
{
	printf("seg.c 74 getsegTosend()-->get a seg from stcp\n");
	int n;
	char buffer[MAX_PKT_LEN];
	sendseg_arg_t recv_seg;

	if((n=recv(stcp_conn,buffer,MAX_PKT_LEN,0))<=0)
	{
		perror("getsegTosend--->get seg fail!!\n");
		return -1;
	}
	// printf("getsegTosend()---->n %d and %d\n",n,sizeof(sendseg_arg_t));
	
	memcpy(&recv_seg,buffer,sizeof(sendseg_arg_t));
//	memcpy(dest_nodeID,&recv_seg.nodeID,sizeof(int));
	memcpy(segPtr,&recv_seg.seg,sizeof(seg_t));
	*dest_nodeID = recv_seg.nodeID;

	return 1;
}

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr)
{
	sendseg_arg_t send_seg;

	send_seg.nodeID = src_nodeID;
	memcpy(&send_seg.seg,segPtr,sizeof(seg_t));
	
	if(send(stcp_conn,&send_seg,sizeof(sendseg_arg_t),0) < 0)
	{
		printf("forwardsegToSTCP()--->send fail!!\n");
		return -1;
	}
	printf("forwardsegToSTCP()--->send successful!!\n");
	return 1;
}

// 一个段有PKT_LOST_RATE/2的可能性丢失, 或PKT_LOST_RATE/2的可能性有着错误的校验和.
// 如果数据包丢失了, 就返回1, 否则返回0. 
// 即使段没有丢失, 它也有PKT_LOST_RATE/2的可能性有着错误的校验和.
// 我们在段中反转一个随机比特来创建错误的校验和.
int seglost(seg_t* segPtr)
{
  int random = rand()%100;
  if(random<PKT_LOSS_RATE*100) {
    //50%可能性丢失段
    if(rand()%2==0) {
      printf("seg lost!!!\n");
                        return 1;
    }
    //50%可能性是错误的校验和
    else {
      //获取数据长度
      int len = sizeof(stcp_hdr_t)+segPtr->header.length;
      //获取要反转的随机位
      int errorbit = rand()%(len*8);
      //反转该比特
      char* temp = (char*)segPtr;
      temp = temp + errorbit/8;
      *temp = *temp^(1<<(errorbit%8));
      return 0;
    }
  }
  return 0;
}

//这个函数计算指定段的校验和.
//校验和计算覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment)
{
  //首先将校验和字段清零
  int length = 24 + segment->header.length;
  return checksum_of_kernel((unsigned char *)segment,length);
}

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1.
int checkchecksum(seg_t* segment)
{
    //首先将校验和字段清零
   if (checksum(segment) == 0)
     return 1;   
   else
    return -1;  
}


unsigned short checksum_of_kernel(unsigned char *buf,int len)  //checksum
{  
    unsigned int sum=0;  
    unsigned short *cbuf;  
  
    cbuf=(unsigned short *)buf;  
  
    while(len>1){  
    sum+=*cbuf++;  
    len-=2;  
    }  
  
    if(len)  
        sum+=*(unsigned char*)cbuf;  

    while(sum>>16)
        sum=(sum>>16)+(sum & 0xffff);  
  
        return ~sum;  

}


long getCurrentTime() {
  struct timeval tv;    
  gettimeofday(&tv,NULL);
  unsigned int time_curr = tv.tv_sec * 1000 + tv.tv_usec / 1000 ;
  return time_curr;
}
