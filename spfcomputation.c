/*
 * =====================================================================================
 *
 *       Filename:  spfcomputation.c
 *
 *    Description:  This file implements the functionality to run SPF computation on the Network Graph
 *
 *        Version:  1.0
 *        Created:  Thursday 24 August 2017 04:31:44  IST
 *       Revision:  1.0
 *       Compiler:  gcc
 *
 *         Author:  Er. Abhishek Sagar, Networking Developer (AS), sachinites@gmail.com
 *        Company:  Brocade Communications(Jul 2012- Mar 2016), Current : Juniper Networks(Apr 2017 - Present)
 *        
 *        This file is part of the SPFComputation distribution (https://github.com/sachinites).
 *        Copyright (c) 2017 Abhishek Sagar.
 *        This program is free software: you can redistribute it and/or modify
 *        it under the terms of the GNU General Public License as published by  
 *        the Free Software Foundation, version 3.
 *
 *        This program is distributed in the hope that it will be useful, but 
 *        WITHOUT ANY WARRANTY; without even the implied warranty of 
 *        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 *        General Public License for more details.
 *
 *        You should have received a copy of the GNU General Public License 
 *        along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * =====================================================================================
 */

#include "graph.h"
#include "heap_interface.h"
#include "spfutil.h"
#include "spfcomputation.h"

extern graph_t *graph;
spf_stats_t spf_stats;

static void
run_dijkastra(LEVEL level){

    spf_stats.spf_runs_count[level]++;
}


/*-----------------------------------------------------------------------------
 *  Fn to initialize the Necessary Data structure prior to run SPF computation.
 *  level can be LEVEL1 Or LEVEL2 Or BOTH
 *-----------------------------------------------------------------------------*/
static void
spf_init(candidate_tree_t *ctree, LEVEL level){

    /*step 1 : Purge NH list of all nodes in the topo*/

    node_t *node = NULL;
    unsigned int i = 0;
    edge_t *edge = NULL;
    singly_ll_node_t *list_node = NULL;

    ITERATE_LIST(graph->graph_node_list, list_node){    
        node = (node_t *)list_node->data;
        for(i = 0; i < MAX_NXT_HOPS; i++){
            node->next_hop[i] = 0;
            node->direct_next_hop[i] = 0;   
        }
    }

    /*step 2 : Metric intialization*/
    ITERATE_LIST(graph->graph_node_list, list_node){
        node = (node_t *)list_node->data;
        node->spf_metric = INFINITE_METRIC;
    }
    graph->graph_root->spf_metric = 0;

    /*step 3 : Initialize direct nexthops.
     * Iterate over real physical nbrs of root (that is skip PNs)
     * and initialize their direct next hop list*/

    ITERATE_NODE_NBRS_BEGIN(graph->graph_root, node, edge, level){
        if(node->node_type == PSEUDONODE)/*Do not initialize direct nxt hop of PNs*/
            continue;
        node->direct_next_hop[0] = node;
    }
    ITERATE_NODE_NBRS_END;

    /*Step 4 : Initialize candidate tree with root*/
   
   INSERT_NODE_INTO_CANDIDATE_TREE(ctree, graph->graph_root);
   
   /*Step 5 : Link Directly Conneccted PN to the graph root
    * I dont know why it is done, but lets do */

    ITERATE_NODE_NBRS_BEGIN(graph->graph_root, node, edge, level){

        if(node->node_type == PSEUDONODE)
            node->pn_intf = &edge->from;/*There is exactly one PN per LAN per level*/            
    }
    ITERATE_NODE_NBRS_END;
}

void
spf_computation(LEVEL level){

    candidate_tree_t ctree;
    CANDIDATE_TREE_INIT(&ctree);

    if(IS_LEVEL_SET(level, LEVEL1)){
        spf_init(&ctree, LEVEL1);
        run_dijkastra(LEVEL1);
    }

    if(IS_LEVEL_SET(level, LEVEL2)){
        spf_init(&ctree, LEVEL2);
        run_dijkastra(LEVEL2);
    }

    /*Comment out below line to avoid assertion*/
    //FREE_CANDIDATE_TREE_INTERNALS(&ctree);
}


