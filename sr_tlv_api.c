/*
 * =====================================================================================
 *
 *       Filename:  sr_tlv_api.c
 *
 *    Description:  implements the API to manipulate SR related TLVs
 *
 *        Version:  1.0
 *        Created:  Monday 26 February 2018 01:49:08  IST
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

#include "sr_tlv_api.h"
#include "instance.h"
#include "prefix.h"
#include "spfutil.h"
#include "bitsop.h"
#include "glthread.h"
#include "LinuxMemoryManager/uapi_mm.h"

void
diplay_prefix_sid(prefix_t *prefix){

    char subnet[PREFIX_LEN_WITH_MASK + 1];

    memset(subnet, 0, PREFIX_LEN_WITH_MASK + 1);
    apply_mask2(prefix->prefix, prefix->mask, subnet);
    prefix_sid_subtlv_t *prefix_sid = get_prefix_sid(prefix);

    if(prefix_sid){
        printf("\t%-18s : %-8u %s\n", subnet, prefix_sid->sid.sid, 
            IS_PREFIX_SR_ACTIVE(prefix) ? "ACTIVE" : "INACTIVE");
        printf("\t\tFLAGS : R:%c N:%c P:%c E:%c V:%c L:%c\n", 
        IS_BIT_SET(prefix_sid->flags, RE_ADVERTISEMENT_R_FLAG) ? '1' : '0',
        IS_BIT_SET(prefix_sid->flags, NODE_SID_N_FLAG) ? '1' : '0',
        IS_BIT_SET(prefix_sid->flags, NO_PHP_P_FLAG) ? '1' : '0',
        IS_BIT_SET(prefix_sid->flags, EXPLICIT_NULL_E_FLAG) ? '1' : '0',
        IS_BIT_SET(prefix_sid->flags, VALUE_V_FLAG) ? '1' : '0',
        IS_BIT_SET(prefix_sid->flags, LOCAL_SIGNIFICANCE_L_FLAG) ? '1' : '0');
    }
    else{
        printf("\t%-18s : Not assigned\n", subnet);
    }
}

boolean
set_node_sid(node_t *node, unsigned int node_sid_value){

    LEVEL level_it;
    boolean trigger_conflict_res = FALSE,
            is_prefix_sid_updated = FALSE;
    prefix_t *router_id = NULL;

    if(!node->spring_enabled){
        printf("Error : SPRING not enabled\n");
        return FALSE;
    }

    if(is_srgb_index_in_use(node->srgb, node_sid_value)){
        printf("Warning : Conflict Detected\n");
    }

    for(level_it = LEVEL1 ; level_it < MAX_LEVEL; level_it++){
        router_id = node_local_prefix_search(node, level_it, node->router_id, 32);
        assert(router_id);
        is_prefix_sid_updated = update_prefix_sid(node, router_id, node_sid_value, level_it); 
        if(is_prefix_sid_updated)
            trigger_conflict_res = TRUE;
    }
    return trigger_conflict_res;
}

boolean
unset_node_sid(node_t *node){

    LEVEL level_it;
    boolean trigger_conflict_res = FALSE;
    prefix_t *router_id = NULL;
    unsigned int index = 0 ;

    if(!node->spring_enabled){
        printf("Error : SPRING not enabled\n");
        return FALSE;
    }
    for(level_it = LEVEL1 ; level_it < MAX_LEVEL; level_it++){
        router_id = node_local_prefix_search(node, level_it, node->router_id, 32);
        assert(router_id);
        if(!router_id->psid_thread_ptr){
            continue;
        }
        index = PREFIX_SID_INDEX(router_id);
        mark_srgb_index_not_in_use(node->srgb, index);
        free_prefix_sid(router_id);
        trigger_conflict_res = TRUE;
        MARK_PREFIX_SR_INACTIVE(router_id);
    }
    return trigger_conflict_res;
}

boolean
set_interface_address_prefix_sid(node_t *node, char *intf_name, 
                    unsigned int prefix_sid_value){

    LEVEL level_it;
    boolean trigger_conflict_res = FALSE,
            is_prefix_sid_updated = FALSE;
    prefix_t *prefix = NULL,
             *intf_prefix = NULL;

    if(!node->spring_enabled){
        printf("Error : SPRING not enabled\n");
        return FALSE;
    }

    edge_end_t *interface = get_interface_from_intf_name(node, intf_name);\

    if(!interface){
        printf("Error : Node %s Interface %s do not exist\n", node->node_name, intf_name);
        return FALSE;
    }

    if(is_srgb_index_in_use(node->srgb, prefix_sid_value)){
        printf("Warning : Conflict Detected\n");
    }

    for(level_it = LEVEL1 ; level_it < MAX_LEVEL; level_it++){
        intf_prefix = interface->prefix[level_it];
        if(!intf_prefix) continue;
        prefix = node_local_prefix_search(node, level_it, intf_prefix->prefix, intf_prefix->mask);
        assert(prefix);
        is_prefix_sid_updated = update_prefix_sid(node, prefix, prefix_sid_value, level_it);
        if(is_prefix_sid_updated)
            trigger_conflict_res = TRUE;
    }
    return trigger_conflict_res;
}

boolean
unset_interface_address_prefix_sid(node_t *node, char *intf_name){

    LEVEL level_it;
    boolean trigger_conflict_res = FALSE;
    prefix_t *prefix = NULL, *intf_prefix = NULL;
    edge_end_t *interface = NULL;
    mpls_label_t label = 0;

    if(!node->spring_enabled){
        printf("Error : SPRING not enabled\n");
        return FALSE;
    }

    interface = get_interface_from_intf_name(node, intf_name);

    if(!interface){
        printf("Error : Node %s Interface %s do not exist\n", node->node_name, intf_name);
        return FALSE;
    }

    for(level_it = LEVEL1 ; level_it < MAX_LEVEL; level_it++){
        intf_prefix = interface->prefix[level_it];
        if(!intf_prefix) continue;
        prefix = node_local_prefix_search(node, level_it, intf_prefix->prefix, intf_prefix->mask);
        assert(prefix);
        if(!prefix->psid_thread_ptr)
            continue;
        label = PREFIX_SID_INDEX(prefix);
        mark_srgb_index_not_in_use(node->srgb, label);
        free_prefix_sid(prefix);
        MARK_PREFIX_SR_INACTIVE(prefix);
        trigger_conflict_res = TRUE;
    }
    
    return trigger_conflict_res;
}

boolean
set_interface_adj_sid(node_t *node, char *interface, unsigned int adj_sid_value){
    return TRUE;
}

boolean
unset_interface_adj_sid(node_t *node, char *interface){
    return TRUE;
}

boolean
update_prefix_sid(node_t *node, prefix_t *prefix, 
        unsigned int prefix_sid_value, LEVEL level){

    boolean trigger_conflict_res = FALSE;
    prefix_sid_subtlv_t *prefix_sid = NULL;
                
    if(!prefix->psid_thread_ptr){
        prefix_sid = XCALLOC(1, prefix_sid_subtlv_t);
        prefix_sid->type = PREFIX_SID_SUBTLV_TYPE;
        prefix_sid->length = 0; /*never used*/
        prefix_sid->flags = 0;
        prefix_sid->algorithm = SHORTEST_PATH_FIRST;
        prefix_sid->sid.type = SID_SUBTLV_TYPE;
        prefix_sid->sid.sid = prefix_sid_value;
        /*Bi-Directional association*/
        prefix->psid_thread_ptr = &prefix_sid->glthread;
        prefix_sid->prefix = prefix;
        MARK_PREFIX_SR_ACTIVE(prefix);
        trigger_conflict_res = TRUE;
        mark_srgb_index_in_use(node->srgb, prefix_sid_value);
        glthread_add_next(&node->prefix_sids_thread_lst[level], &prefix_sid->glthread);
        return trigger_conflict_res;
    }

    if(prefix_sid_value != PREFIX_SID_INDEX(prefix)){
        trigger_conflict_res = TRUE;
        mark_srgb_index_in_use(node->srgb, prefix_sid_value);
        mark_srgb_index_not_in_use(node->srgb, PREFIX_SID_INDEX(prefix));
    }

    prefix_sid->sid.sid = prefix_sid_value;

    if(trigger_conflict_res)
        MARK_PREFIX_SR_ACTIVE(prefix);

    return trigger_conflict_res;
}

unsigned int
PREFIX_SID_INDEX(prefix_t *prefix){
    assert(prefix->psid_thread_ptr);
    return (glthread_to_prefix_sid(prefix->psid_thread_ptr))->sid.sid;
}

prefix_sid_subtlv_t *
get_prefix_sid(prefix_t *prefix){
   
   if(!prefix->psid_thread_ptr) 
       return NULL;

    return glthread_to_prefix_sid(prefix->psid_thread_ptr);
}

void
free_prefix_sid(prefix_t *prefix){

    if(!prefix->psid_thread_ptr)
        return;
    /*de-associate the prefix SID and free it*/
    glthread_t *glthread = prefix->psid_thread_ptr;
    prefix->psid_thread_ptr = NULL;
    prefix_sid_subtlv_t *prefix_sid = glthread_to_prefix_sid(glthread);
    prefix_sid->prefix = NULL;
    remove_glthread(glthread);
    XFREE(prefix_sid);
}
