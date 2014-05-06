//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API
//
//创建日期: 2013年

#include "neighbortable.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include "../common/constants.h"
#include "../topology/topology.h"

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t* nt_create()
{
	printf("调用n_create函数 \n");
	int number_of_neighbor = topology_getNbrNum();
	printf("邻居有 %d\n",number_of_neighbor);
	nbr_entry_t* neighbortable = (nbr_entry_t *)malloc(sizeof(nbr_entry_t) * number_of_neighbor);
	int idofmy = topology_getMyNodeID();
	int i  = 0;
	int j  = 0;
	for (; i < number_of_neighbor; ++i)
	{
		neighbortable[i].nodeID = -1;
  		neighbortable[i].nodeIP = -1;     //邻居的IP地址
		neighbortable[i].conn = -1;
	}
	
	char src[MAX_NAME_LENG];
	char des[MAX_NAME_LENG];
	int  cost;
	FILE *file = fopen("../topology/topology.dat","r" );

	while(!feof(file))
	{
		memset(src , 0 , MAX_NAME_LENG);
		memset(des , 0 , MAX_NAME_LENG);
		fscanf(file , "%s" , src);
		fscanf(file , "%s" , des);
		fscanf(file , "%d" , &cost);
		if (topology_getNodeIDfromname(src) == idofmy)
			{
				for (j = 0; j < number_of_neighbor; ++j)
				{
					if (neighbortable[j].nodeID != -1 && neighbortable[j].nodeID == topology_getNodeIDfromname(des))
					{
						break;
					}
				}
				if (j == number_of_neighbor)
				{
					for (i = 0; i < number_of_neighbor && neighbortable[i].nodeID != -1; ++i);
						assert(i < number_of_neighbor);
					neighbortable[i].nodeID = topology_getNodeIDfromname(des);
					neighbortable[i].nodeIP = topology_getIP(des);
					neighbortable[i].conn = -1;
				}
			}
		else if (topology_getNodeIDfromname(des) == idofmy)
			{
				for (j = 0; j < number_of_neighbor; ++j)
				{
					if (neighbortable[j].nodeID != -1 && neighbortable[j].nodeID == topology_getNodeIDfromname(src))
					{
						break;
					}
				}
				if (j == number_of_neighbor)
				{
					for (i = 0; i < number_of_neighbor && neighbortable[i].nodeID != -1; ++i);
						assert(i < number_of_neighbor);
					neighbortable[i].nodeID = topology_getNodeIDfromname(src);
					neighbortable[i].nodeIP = topology_getIP(src);
					neighbortable[i].conn = -1;
				}
			}
	}

  return (nbr_entry_t*)neighbortable;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt)   //TODO check
{
  printf("调用nt_destroy(函数\n");
  while(nt == NULL )
  {
	close(nt->conn);
	nt++;
  }

  free(nt);

}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{
  printf("调用nt_addconn函数\n");
  if (nt == NULL || nt->nodeID != nodeID)
  {
  	return -1;
  }
  else
  	nt->conn = conn;
  return 1;
}

in_addr_t topology_getIP(char* hostname)
{
	printf("调用topology_getIP函数\n");
	struct hostent *host = gethostbyname(hostname);
	char *IP = inet_ntoa(*(struct in_addr *)*(host->h_addr_list));
	printf("%s\n", IP);
	struct in_addr addr_n;/*IP地址的二进制表示形式*/
	if(inet_pton(AF_INET,IP,&addr_n)<0)/*地址由字符串转换为二级制数*/
	{
		perror("fail to convert");
		exit(1);
	}
	printf("address:%x\n",addr_n.s_addr);/*打印地址的16进制形式*/
	return  addr_n.s_addr;
}
