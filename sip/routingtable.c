
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"

//makehash()是由路由表使用的哈希函数.
//它将输入的目的节点ID作为哈希键,并返回针对这个目的节点ID的槽号作为哈希值.
int makehash(int node)
{
	return node%MAX_ROUTINGTABLE_SLOTS;
}

//这个函数动态创建路由表.表中的所有条目都被初始化为NULL指针.
//然后对有直接链路的邻居,使用邻居本身作为下一跳节点创建路由条目,并插入到路由表中.
//该函数返回动态创建的路由表结构.
routingtable_t* routingtable_create()
{
	int i;
	int nbr_num;
	int* nbrid_array;
	int hash_num;
	routingtable_t* route_table;

	nbr_num = topology_getNbrNum();
	nbrid_array = topology_getNbrArray();

	route_table = (routingtable_t*)malloc(sizeof(routingtable_t));
	if(route_table == NULL){
		printf("malloc fail\n");
		exit(1);
	}

	/*初始为NULL*/
	for(i=0; i<MAX_ROUTINGTABLE_SLOTS; i++)
	{
		route_table->hash[i] = NULL;
	}

	/*将邻居节点插入路由表*/
	for(i=0; i<nbr_num; i++)
	{
		hash_num = makehash(nbrid_array[i]);
		if(route_table->hash[hash_num] == NULL)    //not allocate
		{
			route_table->hash[hash_num] = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
			assert(route_table->hash[hash_num]!=NULL);
			route_table->hash[hash_num]->destNodeID = nbrid_array[i];
			route_table->hash[hash_num]->nextNodeID = nbrid_array[i];				
			route_table->hash[hash_num]->next = NULL;
		}
		else
		{	
			//新建表项插入首部
			routingtable_entry_t* node = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
			node->destNodeID = nbrid_array[i];
			node->nextNodeID = nbrid_array[i];
			node->next = route_table->hash[hash_num];
			route_table->hash[hash_num] = node;
		}
	}

	return route_table;
}

//这个函数删除路由表.
//所有为路由表动态分配的数据结构将被释放.
void routingtable_destroy(routingtable_t* routingtable)
{
	int i;
	routingtable_entry_t* temp;
	routingtable_entry_t* node_dele;

	for(i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++)
	{
		if(routingtable->hash[i] != NULL)		//槽中有表项
		{
			temp = routingtable->hash[i];
			while(temp != NULL){
				node_dele = temp;
				temp = temp->next;
				free(node_dele);
			}
			routingtable->hash[i] = NULL;
		}
	}
	routingtable = NULL;
	return;	
}

//这个函数使用给定的目的节点ID和下一跳节点ID更新路由表.
//如果给定目的节点的路由条目已经存在, 就更新已存在的路由条目.如果不存在, 就添加一条.
//路由表中的每个槽包含一个路由条目链表, 这是因为可能有冲突的哈希值存在(不同的哈希键, 即目的节点ID不同, 可能有相同的哈希值, 即槽号相同).
//为在哈希表中添加一个路由条目:
//首先使用哈希函数makehash()获得这个路由条目应被保存的槽号.
//然后将路由条目附加到该槽的链表中.
void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID)
{
	int hash_num;
	int find_sign = 0;
	
	hash_num =makehash(destNodeID);
	routingtable_entry_t* node_ite;

	if(routingtable->hash[hash_num]==NULL){   //no such routing list
		routingtable->hash[hash_num] = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
		assert(routingtable->hash[hash_num]!=NULL);
		routingtable->hash[hash_num]->destNodeID = destNodeID;
		routingtable->hash[hash_num]->nextNodeID = nextNodeID;
		routingtable->hash[hash_num]->next = NULL;
	}
	else
	{
		node_ite = routingtable->hash[hash_num];
		while(node_ite != NULL)
		{
			if(node_ite->destNodeID == destNodeID)
			{
				node_ite->nextNodeID = nextNodeID;
				find_sign = 1;
				break;
			}
			node_ite = node_ite->next;
		}

		if(find_sign == 0)    //can't find
		{
			routingtable_entry_t* node = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
			node->destNodeID = destNodeID;
			node->nextNodeID = nextNodeID;
			node->next = routingtable->hash[hash_num];
			routingtable->hash[hash_num] = node;
		}

	}
}


//这个函数在路由表中查找指定的目标节点ID.
//为找到一个目的节点的路由条目, 你应该首先使用哈希函数makehash()获得槽号,
//然后遍历该槽中的链表以搜索路由条目.如果发现destNodeID, 就返回针对这个目的节点的下一跳节点ID, 否则返回-1.
int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID)
{
	int hash_num;
	routingtable_entry_t* node_ite;

	hash_num = makehash(destNodeID);
	node_ite = routingtable->hash[hash_num];

	while(node_ite != NULL)
	{
		if(node_ite->destNodeID == destNodeID)
			return node_ite->nextNodeID;
		node_ite = node_ite->next;
	}
	return -1;
}


//这个函数打印路由表的内容
void routingtable_print(routingtable_t* routingtable)
{
	int i;
	routingtable_entry_t* node_ite;
	printf("\n--------------------Routing Table---------------------\n");
	for(i=0; i<MAX_ROUTINGTABLE_SLOTS; i++)
	{
		if(routingtable->hash[i] != NULL)
		{
			node_ite = routingtable->hash[i];
			while(node_ite != NULL)
			{
				printf("dest ID-->%d   next ID-->%d\n",node_ite->destNodeID,node_ite->nextNodeID);
				node_ite = node_ite->next;			
			}
		}
	}
	printf("--------------------Routing Table---------------------\n\n");

}
