
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"

//这个函数动态创建距离矢量表.
//距离矢量表包含n+1个条目, 其中n是这个节点的邻居数,剩下1个是这个节点本身.
//距离矢量表中的每个条目是一个dv_t结构,它包含一个源节点ID和一个有N个dv_entry_t结构的数组, 其中N是重叠网络中节点总数.
//每个dv_entry_t包含一个目的节点地址和从该源节点到该目的节点的链路代价.
//距离矢量表也在这个函数中初始化.从这个节点到其邻居的链路代价使用提取自topology.dat文件中的直接链路代价初始化.
//其他链路代价被初始化为INFINITE_COST.
//该函数返回动态创建的距离矢量表.
dv_t* dvtable_create()
{
	dv_t* dvt;
	int i,j;
	int nbr_num;
	int* nodeid_array;
	int* nbrid_array;
	
	nbr_num = topology_getNbrNum();    //get the num of nbr	
	nbrid_array = topology_getNbrArray();   //get the nbr id arraya
	nodeid_array = topology_getNodeArray();  //get all node if array
	dvt = (dv_t*)malloc((nbr_num+1)*sizeof(dv_t));
	if(dvt == NULL){
		printf("malloc fail\n");
		exit(1);
	}
	
	dvt[0].nodeID = topology_getMyNodeID();
	dvt[0].dvEntry = (dv_entry_t*)malloc(sizeof(dv_entry_t)*NODE_NUM);
	if(dvt[0].dvEntry == NULL){
		printf("malloc fail");
		exit(1);
	}

	//给每个距离适量分配空间
	for(i = 0; i <nbr_num; i++ )
	{
		dvt[i+1].nodeID =	nbrid_array[i];
		dvt[i+1].dvEntry =(dv_entry_t*)malloc(sizeof(dv_entry_t)*NODE_NUM);
		if(dvt[i+1].dvEntry == NULL){
			printf("malloc fail");
			exit(1);
		}
	}
	
	//初始化每个距离矢量的目标节点名称
	for(i=0; i<=nbr_num; i++)
		for(j=0; j<NODE_NUM; j++)
		{			
			dvt[i].dvEntry[j].nodeID = nodeid_array[j];
			if(i != 0)			//the nbr node
				dvt[i].dvEntry[j].cost = INFINITE_COST;
		}

	//初始化节点自身的距离矢量
	for(i=0; i<NODE_NUM; i++)
	{
		if(dvt[0].dvEntry[i].nodeID == dvt[0].nodeID)
		    dvt[0].dvEntry[i].cost = 0;
		else	
		    dvt[0].dvEntry[i].cost = topology_getCost(dvt[0].nodeID,dvt[0].dvEntry[i].nodeID);
	}
	
	return dvt;
}

//这个函数删除距离矢量表.
//它释放所有为距离矢量表动态分配的内存.
void dvtable_destroy(dv_t* dvtable)
{
	int i;
	int nbr_num;

	nbr_num = topology_getNbrNum();

	for(i=0; i<=nbr_num; i++)
	{
		free(dvtable[i].dvEntry);
		dvtable[i].dvEntry = NULL;
	}
	free(dvtable);
	dvtable = NULL;
}

//这个函数设置距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,并且链路代价也被成功设置了,就返回1,否则返回-1.
int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost)
{
	int i;
	int j;
	int nbr_num;
	int change_sign = 0;

	nbr_num = topology_getNbrNum();
	for(i=0; i<nbr_num+1; i++)	
	{
		if(dvtable[i].nodeID == fromNodeID)
		{
			for(j=0; j<NODE_NUM; j++)
				if(dvtable[i].dvEntry[j].nodeID == toNodeID)
					dvtable[i].dvEntry[j].cost = cost;
			change_sign = 1;
		}

		if(dvtable[i].nodeID == toNodeID)
		{
			for(j=0; j<NODE_NUM; j++)
				if(dvtable[i].dvEntry[j].nodeID == fromNodeID)
					dvtable[i].dvEntry[j].cost = cost;
			change_sign = 1;
		}
	}

	if(change_sign == 1)
		return 1;
	else
		return -1;
}

//这个函数返回距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,就返回链路代价,否则返回INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID)
{
	int i;
	int j;
	int nbr_num;	
	nbr_num = topology_getNbrNum();

	for(i = 0; i<nbr_num+1; i++)
	{
		if(dvtable[i].nodeID == fromNodeID)
		{
			for(j=0; j<NODE_NUM; j++)
				if(dvtable[i].dvEntry[j].nodeID == toNodeID)
					return dvtable[i].dvEntry[j].cost;
		}
		
		if(dvtable[i].nodeID == toNodeID)
		{
			for(j=0; j<NODE_NUM; j++)
				if(dvtable[i].dvEntry[j].nodeID == fromNodeID)
					return dvtable[i].dvEntry[j].cost;
		}
	}
	return -1;
}

//这个函数打印距离矢量表的内容.
void dvtable_print(dv_t* dvtable)
{
	int i,j;
	int nbr_num;
	
	nbr_num = topology_getNbrNum();

	printf("\n----------------Distance vector table-------------------\n");
	for(i=0; i< nbr_num+1; i++)	
	{
		printf("src_node:%d-->> ",dvtable[i].nodeID);
		for(j=0; j<NODE_NUM; j++)
		{
			printf("dst_node:%d,cost:%d  ",dvtable[i].dvEntry[j].nodeID,dvtable[i].dvEntry[j].cost);
		}
		printf("\n");
	}
	printf("----------------Distance vector table-------------------\n\n");
}
