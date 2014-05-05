
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat. 
nbr_cost_entry_t* nbrcosttable_create()
{
	int num_nbr = 0;
	int i;
	int my_id;
	int *nbrid_array;
	unsigned int temp_cost;
	nbr_cost_entry_t* nbr_ct;

	my_id = topology_getMyNodeID();				//my node id
	num_nbr = topology_getNbrNum();				//get the num of nbr
	nbr_ct = (nbr_cost_entry_t*)malloc(num_nbr*sizeof(nbr_cost_entry_t));//动态创建邻居表
	if(nbr_ct == NULL)
	{
		printf("malloc fail\n");
		exit(1);
	}

	nbrid_array = topology_getNbrArray();			//get the nbr id
	assert(nbrid_array != NULL);

	for(i=0; i<num_nbr; i++)
	{
		temp_cost = topology_getCost(my_id,nbrid_array[i]);
		nbr_ct[i].cost = temp_cost;
		nbr_ct[i].nodeID = nbrid_array[i];
	}
	return nbr_ct;
}



//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
	free(nct);
	nct = NULL;
	return;
}



//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
	int i;
	int nbr_num;

	nbr_num = topology_getNbrNum();    //get the num of nbr
	
	for(i=0; i<nbr_num; i++)
	{
		if(nodeID == nct[i].nodeID)
			return nct[i].cost;
	}
	return INFINITE_COST;
}



//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t* nct)
{
	int i;
	int nbr_num ;

	nbr_num = topology_getNbrNum();		

	printf("\n---------------- print nbr_cost-----------------\n");
	for(i=0; i<nbr_num; i++)	
	{
		printf("nbr_ID: %d ----- nbr_cost: %d\n",nct[i].nodeID,nct[i].cost);
	}
	printf("---------------- print nbr_cost-----------------\n\n");
}
