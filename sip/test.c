/*
 * =====================================================================================
 *
 *       Filename:  test.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  05/04/2014 10:39:28 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Chengshuo (cshuo), chengshuo357951@gmail.com
 *        Company:  Nanjing Unversity
 *
 * =====================================================================================
 */


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "../common/constants.h"

#include "nbrcosttable.h"
#include "routingtable.h"
#include "dvtable.h"


int main()
{
	dv_t* dv_table;
	routingtable_t* route_table;
	nbr_cost_entry_t* nbr_ct;

	nbr_ct = nbrcosttable_create();
	nbrcosttable_print(nbr_ct);

	printf("--------------- create dv table--------------------\n");
	dv_table = dvtable_create();
	printf("--------------- create dv table--------------------\n");
	dvtable_print(dv_table);

	printf("--------------- create route table--------------------\n");
	route_table = routingtable_create();
	printf("--------------- create route table--------------------\n");
	routingtable_print(route_table);

	nbrcosttable_destroy(nbr_ct);
	dvtable_destroy(dv_table);
	routingtable_destroy(route_table);

	return 0;
}
