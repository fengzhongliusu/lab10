//�ļ���: server/app_stress_server.c

//����: ����ѹ�����԰汾�ķ������������. �������������ӵ�����SIP����. Ȼ��������stcp_server_init()��ʼ��STCP������.
//��ͨ������stcp_server_sock()��stcp_server_accept()�����׽��ֲ��ȴ����Կͻ��˵�����. ��Ȼ������ļ�����. 
//����֮��, ������һ��������, �����ļ����ݲ��������浽receivedtext.txt�ļ���.
//���, ������ͨ������stcp_server_close()�ر��׽���, ���Ͽ��뱾��SIP���̵�����.

//��������: 2013��1��

//����: ��

//���: STCP������״̬

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

//����һ������, ʹ�ÿͻ��˶˿ں�87�ͷ������˿ں�88. 
#define CLIENTPORT1 87
#define SERVERPORT1 88
//�ڽ��յ��ļ����ݱ������, �������ȴ�15��, Ȼ��ر�����.
#define WAITTIME 130

//����������ӵ�����SIP���̵Ķ˿�SIP_PORT. ���TCP����ʧ��, ����-1. ���ӳɹ�, ����TCP�׽���������, STCP��ʹ�ø����������Ͷ�.
int connectToSIP() {

		int sockfd; 
		struct sockaddr_in dest;
		memset(&dest, 0, sizeof(dest));
		dest.sin_family=AF_INET;  /*PF_INETΪIPV4��internetЭ�飬��<netinet/in.h>�У���ַ��*/ 
		dest.sin_addr.s_addr = inet_addr("127.0.0.1");
		dest.sin_port = htons(SIP_PORT);   /*�˿ں�,htons()����һ���������ֽ�˳���������*/  

	/*AF_INEI�׽���Э���壬SOCK_STREAM�׽������ͣ�����socket����������һ���ܹ���������ͨ�ŵ��׽��֡��ж��Ƿ񴴽��ɹ�*/
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

//��������Ͽ�������SIP���̵�TCP����. 
void disconnectToSIP(int sip_conn) {

	close(sip_conn);
	printf("server--->close socket!\n");
}

int main() {
	//���ڶ����ʵ����������
	srand(time(NULL));

	//���ӵ�SIP���̲����TCP�׽���������
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("can not connect to the local SIP process\n");
	}

	//��ʼ��STCP������
	stcp_server_init(sip_conn);

	//�ڶ˿�SERVERPORT1�ϴ���STCP�������׽��� 
	int sockfd= stcp_server_sock(SERVERPORT1);
	if(sockfd<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//��������������STCP�ͻ��˵����� 
	stcp_server_accept(sockfd);

	//���Ƚ����ļ�����, Ȼ������ļ�����
	int fileLen;
	stcp_server_recv(sockfd,&fileLen,sizeof(int));
	char* buf = (char*) malloc(fileLen);
	stcp_server_recv(sockfd,buf,fileLen);

	//�����յ����ļ����ݱ��浽�ļ�receivedtext.txt��
	FILE* f;
	f = fopen("receivedtext.txt","a");
	fwrite(buf,fileLen,1,f);
	fclose(f);
	free(buf);

	//�ȴ�һ���
	sleep(WAITTIME);

	//�ر�STCP������ 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				

	//�Ͽ���SIP����֮�������
	disconnectToSIP(sip_conn);
}
