/*
 * Copyright (c) 2016 Dell Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 * LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 * FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 */

/*
 * filename: nas_mac_unittest.cpp
 */

#include "cps_api_events.h"
#include "cps_api_key.h"
#include "cps_api_operation.h"
#include "cps_api_object.h"
#include "cps_api_errors.h"
#include "gtest/gtest.h"
#include "dell-base-l2-mac.h"
#include "dell-interface.h"
#include "dell-base-if.h"
#include "dell-base-if-vlan.h"
#include "iana-if-type.h"
#include "cps_class_map.h"
#include "cps_api_object_key.h"

#include <iostream>
#include <stdlib.h>
#include <net/if.h>
#include <map>

using namespace std;

enum del_filter {
    DEL_MAC = 0x1,
    DEL_VLAN = 0x2,
    DEL_IFINDEX = 0x4,
    DEL_ALL = 0x8
};

using cps_oper = cps_api_return_code_t (*)(cps_api_transaction_params_t * trans,
        cps_api_object_t object);

static std::map<std::string,cps_oper> trans = {
    {"delete",cps_api_delete },
    {"create",cps_api_create},
    {"set",cps_api_set},
    {"rpc",cps_api_action},
};


bool nas_mac_exec_transaction(std::string op,cps_api_transaction_params_t *tran, cps_api_object_t obj){

    if(trans[op](tran,obj) != cps_api_ret_code_OK ){
         std::cout<<"cps api " + op +"failed"<<std::endl;
         return false;
    }

    if(cps_api_commit(tran) != cps_api_ret_code_OK ){
        std::cout<<"cps api commit failed"<<std::endl;
        return false;
    }

    if(cps_api_transaction_close(tran) != cps_api_ret_code_OK ){
        std::cout<<"cps api transaction close failed"<<std::endl;
        return false;
    }

    return true;
}

typedef struct mac_struct_ {
    hal_mac_addr_t mac_addr;
    hal_vlan_id_t  vlan;
    const char     *if_name;
} mac_struct_t;


bool nas_mac_vlan_create(){

    for(unsigned int ix = 2 ; ix < 100 ; ++ix){
        cps_api_object_t obj = cps_api_object_create();

        cps_api_key_from_attr_with_qual(cps_api_object_key(obj),
                                    DELL_BASE_IF_CMN_IF_INTERFACES_INTERFACE_OBJ,
                                    cps_api_qualifier_TARGET);

        cps_api_object_attr_add(obj,IF_INTERFACES_INTERFACE_TYPE,
           (const char *)IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN,
           sizeof(IF_INTERFACE_TYPE_IANAIFT_IANA_INTERFACE_TYPE_IANAIFT_L2VLAN));

        cps_api_object_attr_add_u32(obj,BASE_IF_VLAN_IF_INTERFACES_INTERFACE_ID,ix);

        const char * mac =  "00:11:11:11:11:11";

        cps_api_object_attr_add(obj, DELL_IF_IF_INTERFACES_INTERFACE_PHYS_ADDRESS, mac, strlen(mac) + 1);

        cps_api_transaction_params_t tr;
        if ( cps_api_transaction_init(&tr) != cps_api_ret_code_OK ) return false;

        if(!nas_mac_exec_transaction(std::string("create"),&tr,obj)) return false;
    }
    return true;
}


bool nas_mac_create_test(){

    mac_struct_t mac_list = { {0x0, 0x0, 0x0, 0x0, 0x0, 0x1}, 1 ,"e101-001-0"};
    size_t fw_index = 5;
    size_t bw_index = 4;
    int index = if_nametoindex(mac_list.if_name);
    int start_index = index;

    cout<<"Entered nas_mac_create_test"<<endl;
    cout<<"========================"<<endl;
    for (unsigned int i = 0; i < 3000 ; i ++) {
        cps_api_transaction_params_t tran;
        if ( cps_api_transaction_init(&tran) != cps_api_ret_code_OK ) return false;

        cps_api_key_t key;
        cps_api_key_from_attr_with_qual(&key, BASE_MAC_TABLE_OBJ, cps_api_qualifier_TARGET);

        cps_api_object_t obj = cps_api_object_create();

        if(obj == NULL )
            return false;

        cps_api_object_set_key(obj,&key);
        cps_api_object_attr_add(obj,BASE_MAC_TABLE_MAC_ADDRESS, mac_list.mac_addr, sizeof(hal_mac_addr_t));
        cps_api_object_attr_add_u16(obj,BASE_MAC_TABLE_VLAN,mac_list.vlan++);
        cps_api_object_attr_add_u32(obj,BASE_MAC_TABLE_IFINDEX, index++);

        if((index-start_index)>= 10){
            index = start_index;
        }

        if(mac_list.vlan == 100){
            mac_list.vlan = 1;
        }
        ++mac_list.mac_addr[fw_index];
        if(mac_list.mac_addr[fw_index] == 255){
            mac_list.mac_addr[fw_index] = 0;
            mac_list.mac_addr[bw_index] += 1;
        }

        if(!nas_mac_exec_transaction(std::string("create"),&tran,obj)) return false;
    }
    return true;
}


bool nas_mac_delete(int del_filter,hal_vlan_id_t vlan_id, const char * ifname){

    mac_struct_t mac_list = { {0x0, 0x0, 0x0, 0x0, 0x0, 0x1}, 1 ,"e101-001-0"};

    cps_api_transaction_params_t tran;

    cout<<"Entered nas_mac_delete"<<endl;
    cout<<"========================"<<endl;
    if ( cps_api_transaction_init(&tran) != cps_api_ret_code_OK ) return false;

    cps_api_key_t key;
    cps_api_key_from_attr_with_qual(&key, BASE_MAC_TABLE_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_t obj = cps_api_object_create();

    if(obj == NULL )
        return false;

    cps_api_object_set_key(obj,&key);
    if (del_filter & DEL_MAC) {
        cps_api_object_attr_add(obj,BASE_MAC_QUERY_MAC_ADDRESS,
                                mac_list.mac_addr, sizeof(hal_mac_addr_t));
    }
    if (del_filter & DEL_VLAN || del_filter & DEL_MAC) {
        cps_api_object_attr_add_u32(obj,BASE_MAC_QUERY_VLAN, vlan_id);
    }
    if (del_filter & DEL_IFINDEX || del_filter & DEL_MAC) {
        int index = if_nametoindex(ifname);
        cps_api_object_attr_add_u32(obj,BASE_MAC_QUERY_IFINDEX, index);
    }

    if(!nas_mac_exec_transaction(std::string("delete"),&tran,obj)) return false;

    return true;
}


bool nas_mac_flush_test(bool vlan,bool ifindex){

    cps_api_transaction_params_t tran;
    if ( cps_api_transaction_init(&tran) != cps_api_ret_code_OK ) return false;

    cps_api_key_t key;
    cps_api_key_from_attr_with_qual(&key, BASE_MAC_FLUSH_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_t obj = cps_api_object_create();

    if(obj == NULL)
        return false;

    cps_api_object_set_key(obj,&key);
    cps_api_attr_id_t ids[3] = {BASE_MAC_FLUSH_INPUT_FILTER,0, BASE_MAC_FLUSH_INPUT_FILTER_VLAN };
    const int ids_len = sizeof(ids)/sizeof(ids[0]);
    if (vlan) {
        uint16_t vlan_list[3]={1,2,3};
        for(unsigned int ix=0;
            ix<sizeof(vlan_list)/sizeof(vlan_list[0]);
            ++ix){
            ids[1]=ix;
            cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U16,&(vlan_list[ix]),sizeof(vlan_list[ix]));
        }
    }

    if (ifindex) {
        int ifidx;
        ids[2]=BASE_MAC_FLUSH_INPUT_FILTER_IFINDEX;
        for(unsigned int ix=0; ix<3; ++ix){
            ids[1]=ix;
            std::cout<<"Enter the ifindex"<<std::endl;
            std::cin>>ifidx;
            cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U16,&(ifidx),sizeof(ifidx));
        }
    }

    if(!nas_mac_exec_transaction(std::string("rpc"),&tran,obj)) return false;

    return true;
}

bool nas_mac_bulk_flush_test(){

    cps_api_transaction_params_t tran;
    if ( cps_api_transaction_init(&tran) != cps_api_ret_code_OK ) return false;

    cps_api_key_t key;
    cps_api_key_from_attr_with_qual(&key, BASE_MAC_FLUSH_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_t obj = cps_api_object_create();

    if(obj == NULL)
        return false;

    cps_api_object_set_key(obj,&key);
    cps_api_attr_id_t ids[3] = {BASE_MAC_FLUSH_INPUT_FILTER,0, BASE_MAC_FLUSH_INPUT_FILTER_VLAN };
    const int ids_len = sizeof(ids)/sizeof(ids[0]);
    for(uint16_t ix=1; ix<1024; ++ix){
        ids[1]=ix;
        cps_api_object_e_add(obj,ids,ids_len,cps_api_object_ATTR_T_U16,&ix,sizeof(ix));
    }

    if(!nas_mac_exec_transaction(std::string("rpc"),&tran,obj)) return false;

    return true;
}


bool nas_mac_auto_flush_management(bool enable){

    cps_api_transaction_params_t tran;
    if ( cps_api_transaction_init(&tran) != cps_api_ret_code_OK ) return false;

    cps_api_key_t key;
    cps_api_key_from_attr_with_qual(&key, BASE_MAC_FLUSH_MANAGEMENT_OBJ, cps_api_qualifier_TARGET);

    cps_api_object_t obj = cps_api_object_create();

    if(obj == NULL )
        return false;

    cps_api_object_set_key(obj,&key);
    cps_api_object_attr_add_u32(obj,BASE_MAC_FLUSH_MANAGEMENT_ENABLE,enable);

    if(!nas_mac_exec_transaction(std::string("create"),&tran,obj)) return false;

    cps_api_get_params_t gp;
    cps_api_get_request_init(&gp);
    cps_api_key_t key1;
    cps_api_key_from_attr_with_qual(&key1,BASE_MAC_FLUSH_MANAGEMENT_OBJ,cps_api_qualifier_TARGET);
    gp.key_count = 1;
    gp.keys = &key1;

    bool rc = false;
    if (cps_api_get(&gp)==cps_api_ret_code_OK) {
        size_t mx = cps_api_object_list_size(gp.list);
        for (size_t ix = 0 ; ix < mx ; ++ix ) {
            cps_api_object_t obj = cps_api_object_list_get(gp.list,ix);
            cps_api_object_attr_t auto_mgmt = cps_api_object_attr_get(obj,BASE_MAC_FLUSH_MANAGEMENT_ENABLE);
            if(auto_mgmt){
                cout<<"Auto MAC Management Value : "<<cps_api_object_attr_data_u32(auto_mgmt)<<endl;
            }
        }
        rc = true;
    }

    cps_api_get_request_close(&gp);
    return rc;
}


TEST(cps_api_events,mac_test) {

    cout<<"VLAN Create"<<endl;
    ASSERT_TRUE(nas_mac_vlan_create());

    cout<<"MAC create"<<endl;
    ASSERT_TRUE(nas_mac_create_test());

    cout<<"DEL_MAC"<<endl;
    ASSERT_TRUE(nas_mac_delete(DEL_MAC,1,"e101-001-0"));

    cout<<"DEL_IFINDEX "<<endl;
    ASSERT_TRUE(nas_mac_delete(DEL_IFINDEX,0,"e101-001-0"));

    cout<<"DEL_VLAN"<<endl;
    ASSERT_TRUE(nas_mac_delete(DEL_VLAN,1,NULL));

    cout<<"DEL_IFINDEX | VLAN"<<endl;
    ASSERT_TRUE(nas_mac_delete(DEL_IFINDEX | DEL_VLAN,5,"e101-002-0"));

    cout<<"FLUSH ALL"<<endl;
    ASSERT_TRUE(nas_mac_delete(DEL_ALL,0,NULL));

    cout<<"FLUSH LIST"<<endl;
    ASSERT_TRUE(nas_mac_create_test());
    ASSERT_TRUE(nas_mac_flush_test(true,false));
    ASSERT_TRUE(nas_mac_flush_test(false,true));
    ASSERT_TRUE(nas_mac_flush_test(true,true));

    cout<<"AUTO FLUSH"<<endl;
    ASSERT_TRUE(nas_mac_auto_flush_management(false));
    ASSERT_TRUE(nas_mac_auto_flush_management(true));

    cout<<"Bulk Flush Test"<<endl;
    ASSERT_TRUE(nas_mac_bulk_flush_test());

}

static void mac_dump_flush_obj(cps_api_object_t obj, const cps_api_object_it_t & it){
     cps_api_object_it_t it_lvl_1 = it;

     for (cps_api_object_it_inside (&it_lvl_1); cps_api_object_it_valid (&it_lvl_1);
             cps_api_object_it_next (&it_lvl_1)) {

         cps_api_object_it_t it_lvl_2 = it_lvl_1;

         for (cps_api_object_it_inside (&it_lvl_2); cps_api_object_it_valid (&it_lvl_2);
                 cps_api_object_it_next (&it_lvl_2)) {

             switch(cps_api_object_attr_id(it_lvl_2.attr)){
             case BASE_MAC_FLUSH_EVENT_FILTER_MEMBER_IFINDEX:
                 std::cout<<"Member port index "<<cps_api_object_attr_data_u32(it_lvl_2.attr)<<std::endl;
                 break;

             case BASE_MAC_FLUSH_EVENT_FILTER_IFINDEX:
                 std::cout<<"Port index "<<cps_api_object_attr_data_u32(it_lvl_2.attr)<<std::endl;
                 break;

             case BASE_MAC_FLUSH_EVENT_FILTER_VLAN:
                 std::cout<<"Vlan id "<<cps_api_object_attr_data_u16(it_lvl_2.attr)<<std::endl;
                 break;

             default:
                 break;
             }
         }
     }
}

static bool mac_event_cb(cps_api_object_t obj, void *param)
{

     cps_api_object_it_t it;
     cps_api_object_it_begin(obj,&it);

        for ( ; cps_api_object_it_valid(&it) ; cps_api_object_it_next(&it) ) {
            int id = (int) cps_api_object_attr_id(it.attr);
            switch(id) {
                case BASE_MAC_FLUSH_EVENT_FILTER:
                    mac_dump_flush_obj(obj,it);
                    break;
                default:
                    break;
            }
        }
    return true;
}

TEST(std_nas_mac_events, mac_flush_events)
{

    cps_api_event_reg_t reg;
    memset(&reg,0,sizeof(reg));

    if (cps_api_event_service_init() != cps_api_ret_code_OK) {
        printf("***ERROR*** cps_api_event_service_init() failed\n");
        return ;
    }

    if (cps_api_event_thread_init() != cps_api_ret_code_OK) {
        printf("***ERROR*** cps_api_event_thread_init() failed\n");
        return;
    }

    cps_api_key_t key;

    cps_api_key_from_attr_with_qual(&key,BASE_MAC_FLUSH_EVENT_OBJ ,
            cps_api_qualifier_OBSERVED);

    reg.number_of_objects = 1;
    reg.objects = &key;

    if (cps_api_event_thread_reg(&reg,mac_event_cb,NULL)!=cps_api_ret_code_OK) {
        std::cout << " registration failure"<<std::endl;
        return;
    }

    while(1);
    // infinite loop
}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
