#pragma once
#ifndef uint32_t
    #include<stdint.h>
#endif
#include "hc_dict.h"

#define TA_RENEW 0b00000001
#define TA_CHECK 0b00000010
#define TA_IGNORETIME 0b00000100

typedef struct HC_TimeArrayItem{
    char status:4;
    char user_flg:4;
    char type; //0:raw 1:num
    int index;
    int new_index;
    int data_len;
    int64_t timeout;
    union {
        uint64_t u64;
        int64_t i64;
        void* data;
    };
    union{
        struct{
            uint64_t check1:32;
            uint64_t check2:32;
        };
        uint64_t check;
    };
}HC_TimeArrayItem;

typedef struct HC_TimeArray{
    int item_max;
    int data_max;
    int cur_index;
    int timeout;
    uint32_t per_data_size;
    HC_TimeArrayItem *items;
    void *buf;
    union{
        void *arg_ptr;
        uint64_t arg_u64;
    };
    void *check_fun;
}HC_TimeArray;

typedef struct HC_TimeDict{
    HC_TimeArray *time_array;
    HC_Dict *dict;
}HC_TimeDict;

int hc_time_array_init_item(HC_TimeArray *ta);
int hc_time_array_init_data(HC_TimeArray *ta);
HC_TimeArray *hc_time_array_create(int max,int timeout);
int hc_ta_set_item_num(HC_TimeArray *time_array,int index,uint64_t num,int flag,unsigned long check);
int hc_ta_set_item_mem(HC_TimeArray *time_array,int index,void* data,uint32_t data_len,int flag,unsigned long check);
int hc_ta_append_item_mem(HC_TimeArray *time_array,int index,void* data,uint32_t data_len,int flag,unsigned long check);
void* hc_ta_get_itemdata(HC_TimeArray *time_array,int index,HC_TimeArrayItem **item_ptr,int flag,unsigned long check);
HC_TimeArrayItem* hc_ta_get_item(HC_TimeArray *time_array,int index,int flag,unsigned long check);
void hc_time_array_free(HC_TimeArray *ta);
void hc_timedict_free(HC_TimeDict *td);
HC_TimeDict *hc_timedict_create(int max,int timeout);
int hc_timedict_set_num(HC_TimeDict *td, void *key, int key_len, uint64_t num, int flag);
uint64_t hc_timedict_get_num(HC_TimeDict *td,void *key,int key_len,int *pIndex,int flag);
