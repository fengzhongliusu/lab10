// �ļ���: common/pkt.c
// ��������: 2013��

#include "pkt.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
// son_sendpkt()��SIP���̵���, ��������Ҫ��SON���̽����ķ��͵��ص�������. SON���̺�SIP����ͨ��һ������TCP���ӻ���.
// ��son_sendpkt()��, ���ļ�����һ���Ľڵ�ID����װ�����ݽṹsendpkt_arg_t, ��ͨ��TCP���ӷ��͸�SON����. 
// ����son_conn��SIP���̺�SON����֮���TCP�����׽���������.
// ��ͨ��SIP���̺�SON����֮���TCP���ӷ������ݽṹsendpkt_arg_tʱ, ʹ��'!&'��'!#'��Ϊ�ָ���, ����'!& sendpkt_arg_t�ṹ !#'��˳����.
// ������ͳɹ�, ����1, ���򷵻�-1.
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
		printf("23->son_sendpkt() ����!&fail\n");
		return -1;
	}
	if (send(son_conn , &sendbuf , pkt->header.length + 16 , 0 ) < 0)
	{
		printf("28->son_sendpkt ����!&pkt fail\n");
		return -1;
	}
	if (send(son_conn , stop , 2, 0 ) < 0 )
	{
		printf("33->son_sendpkt ����!# fail\n");
		return -1;
	}
  return 1;
}

// son_recvpkt()������SIP���̵���, �������ǽ�������SON���̵ı���. 
// ����son_conn��SIP���̺�SON����֮��TCP���ӵ��׽���������. ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
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

  	if (recv(son_conn,&recvchar,1,0) <= 0)//�ر�����
  	{
  		return -1;//���ӹرգ����ϲ㺯�����ó����������ʱ��Ӧ��ֹ�߳�
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
            		printf("������Χ��ֱ�ӵ���ʧ����!!!\n");
  			 		return -1;//������Χ��ֱ�ӵ���ʧ����
  			 	}
  			 }
  			 break;
  		case SEGSTOP1:
  			if (recvchar == '#')
  			 {
  			 	memset((void *)pkt , 0 , sizeof(sip_pkt_t));
  			 	memcpy(pkt, recvbuf, index);
        
  			 		return 1 ;//���ճɹ�
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
  		 		return -1;//û�г��ָ�״̬������ʧ����
  	}
  }
}

// ���������SON���̵���, �������ǽ������ݽṹsendpkt_arg_t.
// ���ĺ���һ���Ľڵ�ID����װ��sendpkt_arg_t�ṹ.
// ����sip_conn����SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// sendpkt_arg_t�ṹͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ���
// ����ɹ�����sendpkt_arg_t�ṹ, ����1, ���򷵻�-1.

int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn)
{
   // printf("����getpktToSend����\n");

  int  state ;
  char recvbuf[sizeof(sip_pkt_t)];
  int index = 0;
  char recvchar;
  state = SEGSTART1;
  for (;;)
  {

  	if (recv(sip_conn,&recvchar,1,0) <= 0)//�ر�����
  	{
  		return -1;//���ӹرգ����ϲ㺯�����ó����������ʱ��Ӧ��ֹ�߳�
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
            		printf("������Χ��ֱ�ӵ���ʧ����!!!\n");
  			 		return -1;//������Χ��ֱ�ӵ���ʧ����
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
  			 	return 1 ;//���ճɹ�
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
  		 		return -1;//û�г��ָ�״̬������ʧ����
  	}
  }
}

// forwardpktToSIP()��������SON���̽��յ������ص����������ھӵı��ĺ󱻵��õ�. 
// SON���̵����������������ת����SIP����. 
// ����sip_conn��SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
  // printf("����forwardpktToSIP����\n");
	char start[2] = "!&";
	char stop[2] = "!#";
	sip_pkt_t sendbuf;
	memcpy(&(sendbuf),pkt,sizeof(sip_pkt_t));
	if (send(sip_conn , start , 2, 0 ) < 0 )
	{
		printf("235->forwardpktToSIP ���� !& fail\n");
		return -1;
	}
	if (send(sip_conn , &sendbuf , sizeof(sip_pkt_t), 0 ) < 0)
	{
		printf("240->forwardpktToSIP ���� pkt fail\n");
		return -1;
	}
	if (send(sip_conn , stop , 2, 0 ) < 0 )
	{
		printf("245->forwardpktToSIP���� !# fail\n");
		return -1;
	}
  return 1;
}

// sendpkt()������SON���̵���, �������ǽ�������SIP���̵ı��ķ��͸���һ��.
// ����conn�ǵ���һ���ڵ��TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھӽڵ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
  	//printf("����sendpkt����\n");
	char start[2] = "!&";
	char stop[2] = "!#";
	sip_pkt_t sendbuf;
	memcpy(&(sendbuf),pkt,pkt->header.length + 12);
	if (send(conn , start , 2, 0 ) < 0 )
	{
		printf("264->sendpkt ����!& fail socket %d\n",conn);
		return -1;
	}
	if (send(conn , &sendbuf , pkt->header.length + 12 , 0 ) < 0)
	{
		printf("269->sendpkt ����pkt fail\n");
		return -1;
	}
	if (send(conn , stop , 2, 0 ) < 0 )
	{
		printf("274->sendpkt ����!# fail\n");
		return -1;
	}
  return 1;
}

// recvpkt()������SON���̵���, �������ǽ��������ص����������ھӵı���.
// ����conn�ǵ����ھӵ�TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
int recvpkt(sip_pkt_t* pkt, int conn)
{
  //printf("����recvpkt����\n");
  int  state ;
  char recvbuf[sizeof(sip_pkt_t)];
  int index = 0;
  char recvchar;
  state = SEGSTART1;
  for (;;)
  {

  	if (recv(conn,&recvchar,1,0) <= 0)//�ر�����
  	{
  		return -1;//���ӹرգ����ϲ㺯�����ó����������ʱ��Ӧ��ֹ�߳�
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
            		printf("������Χ��ֱ�ӵ���ʧ����!!!\n");
  			 		return -1;//������Χ��ֱ�ӵ���ʧ����
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
  			 	return 1 ;//���ճɹ�
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
  		 		return -1;//û�г��ָ�״̬������ʧ����
  	}
  }
}
