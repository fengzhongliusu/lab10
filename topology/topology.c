//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 
//
//创建日期: 2013年

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include "../common/constants.h"
#include "topology.h"

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) 
{
	//printf("调用topology_getNodeIDfromname函数 \n");
	printf("hostname is %s\n", hostname);
	struct hostent *host = gethostbyname(hostname);
	if (host == NULL)
	{
		printf("name is null\n");
	}
	unsigned int ID ;
	ID = (unsigned int)(host->h_addr_list[0][3]& 0x000000FF);
	printf("ID is %d\n", ID);
	return (int)ID;
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr)
{
//	printf("调用topology_getNodeIDfromip函数 \n");
  	int address = ntohl( addr->s_addr );
	return address%256;
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
{
 // printf("调用topology_getMyNodeID函数 \n");
  char hostname[32];
  gethostname(hostname , 32);
  return topology_getNodeIDfromname(hostname);
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{
//	printf("调用topology_getNbrNum函数 \n");
	int neighbor = 0;
	char src[MAX_NAME_LENG];
	char des[MAX_NAME_LENG];
	int cost = 0;
	FILE *file = fopen( "../topology/topology.dat","r" );
	if(file == NULL)
	{
		perror("fail to open file\n");
		exit(1);
	}

	char localname[MAX_NAME_LENG];
	gethostname(localname,MAX_NAME_LENG);
	//printf("hostname = %s\n", localname);
	if( localname == NULL){
		return 0;
	}

	while(1)
	{
		memset(src, 0, MAX_NAME_LENG);
		memset(des, 0, MAX_NAME_LENG);
		fscanf( file, "%s %s %d",src,des, &cost);
		if(feof(file))
			break;
		if (strcmp(src , localname) == 0 || strcmp(des , localname) == 0)
		{
			neighbor++;
		}
	}
	fclose(file);
	//printf("num of neighbor is %d \n", neighbor);
	return neighbor;

}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{ 
//	printf("调用topology_getNodeNum函数 \n");
    int Nodenum = 0;
	int srcExist = 0;
	int dstExist = 0;

	char src[MAX_NAME_LENG];
	char des[MAX_NAME_LENG];
	int cost = 0;
	FILE *file = fopen( "../topology/topology.dat","r" );
	if(file == NULL)
	{
		perror("fail to open file\n");
		exit(1);
	}

	char Node[MAX_NUM_OF_NODE][MAX_NAME_LENG];

	while(1)
	{
		memset( src, 0, MAX_NAME_LENG);
		memset( des, 0, MAX_NAME_LENG);
		fscanf(file, "%s %s %d", src, des, &cost);
		if(feof(file))
		  break;
		int i = 0;
		srcExist = 0;
		dstExist = 0;
		for ( i = 0; i < Nodenum ; i++)
		{
			if( strcmp( Node[i], des) == 0)
				dstExist = 1;
			if( strcmp( Node[i], src) == 0)
				srcExist = 1;
		}
		if( dstExist == 0 ){
			memcpy( Node[Nodenum], des, MAX_NAME_LENG);
			Nodenum ++;
		}
		if( srcExist == 0 ){
			memcpy( Node[Nodenum], src, MAX_NAME_LENG);
			Nodenum ++;
		}

	}

	fclose( file); 
	return Nodenum;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID. 
int* topology_getNodeArray()
{
//	printf("调用topology_getNodeArray函数 \n");
	int Nodenum = 0;
	int srcExist = 0;
	int dstExist = 0;
	int i  = 0;
	char src[MAX_NAME_LENG];
	char des[MAX_NAME_LENG];
	int cost = 0;
	FILE *file = fopen( "../topology/topology.dat","r" );
	if(file == NULL)
	{
		perror("fail to open file\n");
		exit(1);
	}

	char Node[MAX_NUM_OF_NODE][MAX_NAME_LENG];
	int* NodeArray = (int *)malloc(4 * MAX_NUM_OF_NODE);

	for (; i < MAX_NUM_OF_NODE; ++i)
	{
		NodeArray[i] = 0;
	}
	while(1)
	{
		memset( src, 0, MAX_NAME_LENG);
		memset( des, 0, MAX_NAME_LENG);
		fscanf(file, "%s %s %d", src, des, &cost);
		if(feof(file))
		  break;

		srcExist = 0;
		dstExist = 0;
		for ( i = 0; i < Nodenum ; i++)
		{
			if( strcmp( Node[i], des) == 0)
				dstExist = 1;
			if( strcmp( Node[i], src) == 0)
				srcExist = 1;
		}
		if( dstExist == 0 )
		{
			memcpy( Node[Nodenum], des, MAX_NAME_LENG);
			Nodenum ++;
		}
		if( srcExist == 0 )
		{
			memcpy( Node[Nodenum], src, MAX_NAME_LENG);
			Nodenum ++;
		}

	}

	fclose( file );
	
	for (i = 0; i < Nodenum; ++i)
	{
		NodeArray[i] = topology_getNodeIDfromname(Node[i]);
	}

	return (int *)NodeArray;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray()
{
//	printf("调用topology_getNbrArray函数 \n");
	int cost;
	char src[MAX_NAME_LENG];
	char des[MAX_NAME_LENG];
	char localname[MAX_NAME_LENG];
	gethostname(localname,MAX_NAME_LENG);
	int* NbrArray = (int *)malloc(MAX_NUM_OF_NODE * 4);
	int i = 0;
	for (; i < MAX_NUM_OF_NODE; ++i)
	{
		NbrArray[i] = 0;
	}

	FILE *file = fopen( "../topology/topology.dat","r" );
	if(file == NULL)
	{
		perror("fail to open file\n");
		exit(1);
	}
	while(1)
	{
		memset(src, 0, MAX_NAME_LENG);
		memset(des, 0, MAX_NAME_LENG);
		fscanf( file, "%s %s %d",src,des ,&cost);
		if(feof(file))
			break;

		if (strcmp(src , localname) == 0)
		{
			for (i = 0; NbrArray[i] != 0 ; ++i)
			{
				if (NbrArray[i] == topology_getNodeIDfromname(des))
				{
					break;
				}
			}
			if (NbrArray[i] == 0)
			{
				NbrArray[i] = topology_getNodeIDfromname(des);
			}
		}
		else if (strcmp(des , localname) == 0)
		{
			for (i = 0; NbrArray[i] != 0 ; ++i)
			{
				if (NbrArray[i] == topology_getNodeIDfromname(src))
				{
					break;
				}
			}
			if (NbrArray[i] == 0)
			{
				NbrArray[i] = topology_getNodeIDfromname(src);
			}
		}
	}

	fclose( file );
	return NbrArray;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
//	printf("调用topology_getCost函数 \n");
  	FILE *file;
  	file = fopen("../topology/topology.dat", "r");
	if(file == NULL)
	{
		perror("fail to open file\n");
		exit(1);
	}
	assert(file != NULL);

	char hostname1[MAX_NAME_LENG], hostname2[MAX_NAME_LENG];
	int NodeID1, NodeID2;
	int cost;
	while(1)
	{
		memset( hostname1, 0, MAX_NAME_LENG);
		memset( hostname2, 0, MAX_NAME_LENG);
		fscanf(file, "%s %s %d", hostname1, hostname2, &cost);
		if(feof(file))
		   break;

		NodeID1 = topology_getNodeIDfromname(hostname1);
		NodeID2 = topology_getNodeIDfromname(hostname2);
		if ((NodeID1 == fromNodeID && NodeID2 == toNodeID) || (NodeID1 == toNodeID && NodeID2 == fromNodeID))
		{
			fclose(file);
			return cost;
		}
	}
	fclose(file);
	return INFINITE_COST;
}


