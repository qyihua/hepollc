#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <time.h>

#include "hc_util.h"
#include "hc_time_array.h"

HC_TimeArray *hc_time_array_create(int max,int timeout)
{
    if(1>max) max=32;
    HC_TimeArray *time_array = calloc(1,sizeof(HC_TimeArray));
    if(!time_array){
        perror("time_array create");
        return NULL;
    }
    time_array->item_max=max;
    time_array->timeout=timeout;
    time_array->items=calloc(max,sizeof(HC_TimeArrayItem));
    if(!time_array->items){
        perror("time_array items create");
        free(time_array);
        return NULL;
    }
    return time_array;
}

void hc_time_array_free(HC_TimeArray *ta)
{
    free(ta->items);
    free(ta);
    return;
}

int hc_time_array_init_item(HC_TimeArray *ta)
{
    int max = ta->item_max;
    HC_TimeArrayItem *ta_item=ta->items;
    for(int i=0;i<max;i++){
        ta_item->index = i;
        ta_item++;
    }
    return 0;
}

int hc_time_array_init_data(HC_TimeArray *ta)
{
    int max = ta->item_max;
    ta->buf = malloc(max*ta->per_data_size);
    uint64_t start_addr = (uint64_t)ta->buf;
    HC_TimeArrayItem *ta_item=ta->items;
    uint32_t per_data_size = ta->per_data_size;
    for(int i=0;i<max;i++){
        ta_item->u64 = start_addr;
        ta_item->index = i;
        start_addr += per_data_size;
        ta_item++;
    }
    return 0;
}

static inline int ta_get_new_index(HC_TimeArray *time_array,time_t now,int flag, unsigned long check)
{
    int index=time_array->cur_index;
    HC_TimeArrayItem *item=time_array->items+index;
    if(now<item->timeout&&0==(flag&TA_IGNORETIME)){
        return -1;
    }
    item->timeout = time_array->timeout+now;
    item->new_index = -1;
    if(TA_CHECK&flag){
        item->check = check;
    }else{
        item->check1++;
    }
    if(++time_array->cur_index < time_array->item_max)
        return index;
    time_array->cur_index=0;
    return index;
}


//flag: 0011 check,renew
static inline int ta_get_index_for_set(HC_TimeArray *time_array,int index,int flag,unsigned long check)
{
    time_t now_sec;
    time(&now_sec);
    if(-1==index){
        index=ta_get_new_index(time_array,now_sec,flag,check);
        return index;
    }else if(index>=time_array->item_max)
        return -1;

    time_t renew_sec, item_sec;
    int new_index;
    HC_TimeArrayItem *item=NULL,*itemtmp=NULL;
    item = time_array->items+index;
    item_sec = item->timeout;
    if(item_sec<now_sec) return -1;

    if(TA_CHECK&flag){
        if(time_array->check_fun){
            if(0>((int (*)(HC_TimeArray*,HC_TimeArrayItem*,void*))time_array->check_fun)(time_array,item,(void*)check)) return -1;
        }else{
            if(check!=item->check) return -1;
        }
    }
    if(TA_RENEW&flag){
        renew_sec = item_sec-time_array->timeout;
        if( renew_sec<=now_sec && item_sec>now_sec){
            new_index = item->new_index;
            if(-1==new_index){
                new_index=ta_get_new_index(time_array,now_sec,2,item->check);
                if(0>new_index){
                    item->timeout = time_array->timeout+now_sec;
                    time_array->cur_index=index;
                    return index;
                }
                item->new_index = new_index;
                itemtmp = time_array->items+new_index;
                check = itemtmp->u64;
                itemtmp->data = item->data;
                item->u64 = check;
                itemtmp->user_flg = item->user_flg;
            }
            return new_index;
        }
    }
    return index;
}

int hc_ta_set_item_num(HC_TimeArray *time_array,int index,uint64_t num,int flag,unsigned long check)
{
    index = ta_get_index_for_set(time_array,index,flag,check);
    if(-1==index) return -1;
    HC_TimeArrayItem *item = time_array->items+index;
    item->u64 = num;
    return index;
}

int hc_ta_set_item_mem(HC_TimeArray *time_array,int index,void* data,uint32_t data_len,int flag,unsigned long check)
{
    index = ta_get_index_for_set(time_array,index,flag,check);
    if(-1==index) return -1;
    HC_TimeArrayItem *item = time_array->items+index;
    void* item_data = item->data;
    if(data_len>time_array->per_data_size)
        data_len = time_array->per_data_size;
    if(!item_data) return -1;

    if(data){
        memcpy(item_data,(char*)data,data_len);
        item->data_len = data_len;
    }else
        item->data_len = 0;
    return index;
}

int hc_ta_append_item_mem(HC_TimeArray *time_array,int index,void* data,uint32_t data_len,int flag,unsigned long check)
{
    int has = index>-1?1:0;
    index = ta_get_index_for_set(time_array,index,flag,check);
    if(-1==index) return -1;
    HC_TimeArrayItem *item = time_array->items+index;
    void *item_data = item->data;
    if(!item_data) return -1;

    if(!has){
        item->data_len = 0;
    }
    if(data){
        memcpy(item_data+item->data_len,(char*)data,data_len);
        item->data_len += data_len;
    }
    return index;
}

static inline int ta_get_index_for_get(HC_TimeArray *time_array,int index,int flag,unsigned long check){
    if(-1==index||index>=time_array->item_max){
        return -1;
    }

    time_t now_sec;
    time(&now_sec);
    time_t renew_sec, item_sec;
    int new_index;
    uint64_t tmp;
    HC_TimeArrayItem *item=NULL,*itemtmp=NULL;

    item = time_array->items+index;
    if(TA_CHECK&flag){
        if(time_array->check_fun){
            if(0>((int (*)(HC_TimeArray*,HC_TimeArrayItem*,void*))time_array->check_fun)(time_array,item,(void*)check)) return -1;
        }else{
            if(check!=item->check) return -1;
        }
    }
    item_sec = item->timeout;
    renew_sec = item_sec-(time_array->timeout>>1);
    if(renew_sec<=now_sec && item_sec>now_sec){
        new_index = item->new_index;
        if(-1==new_index){
            if(TA_RENEW&flag){
                new_index=ta_get_new_index(time_array,now_sec,2,item->check);
                if(0>new_index){
                    item->timeout = time_array->timeout+now_sec;
                    time_array->cur_index=index;
                    return index;
                }
                item->new_index = new_index;
                itemtmp = time_array->items+new_index;
                tmp = itemtmp->u64;
                itemtmp->u64 = item->u64;
                item->u64 = tmp;
                itemtmp->user_flg = item->user_flg;
                return new_index;
            }
            return index;
        }else
            return new_index;
    }else if(item_sec<=now_sec){
        return -1;
    }
    return index;
}

void* hc_ta_get_itemdata(HC_TimeArray *time_array,int index,HC_TimeArrayItem **item_ptr,int flag,unsigned long check)
{
    index = ta_get_index_for_get(time_array,index,flag,check);
    if(-1==index){
        if(item_ptr) *item_ptr = NULL;
        return NULL;
    }
    HC_TimeArrayItem *item = time_array->items+index;
    if(item_ptr) *item_ptr = item;
    return item->data;
}

HC_TimeArrayItem* hc_ta_get_item(HC_TimeArray *time_array,int index,int flag,unsigned long check)
{
    index = ta_get_index_for_get(time_array,index,flag,check);
    if(-1==index){
        return NULL;
    }
    return time_array->items+index;
}

HC_TimeDict *hc_timedict_create(int max,int timeout)
{
    HC_TimeArray *ta = hc_time_array_create(max, timeout);
    if(!ta){
        printf("timedict ta create error\n");
        return NULL;
    }
    hc_time_array_init_item(ta);
    HC_Dict *dict = hc_dict_create(TYPE_NUM, max);
    if(!dict){
        printf("timedict dict create error\n");
        hc_time_array_free(ta);
        return NULL;
    }
    HC_TimeDict *td = malloc(sizeof(HC_TimeDict));
    if(!td){
        printf("timedict create error\n");
        hc_time_array_free(ta);
        hc_dict_free(dict);
        return NULL;
    }
    td->dict = dict;
    td->time_array = ta;
    return td;
}

void hc_timedict_free(HC_TimeDict *td)
{
    hc_time_array_free(td->time_array);
    hc_dict_free(td->dict);
    free(td);
    return;
}

int hc_timedict_set_num(HC_TimeDict *td, void *key, int key_len, uint64_t num, int flag)
{
    HC_TimeArray *time_array = td->time_array;
    HC_Dict *key_to_index = td->dict;
    int ret;
    int index = hc_dict_get_num(key_to_index,key,key_len,&ret);
    int index_new;
    HC_TimeArrayItem *ta_item;
    ta_item = hc_ta_get_item(time_array,index, flag, 0);
    if(ta_item){
        ta_item->u64 = num;
        index_new = ta_item->index;
    }else{
        index_new = hc_ta_set_item_num(time_array, -1, num,  flag, 0);
        if(0>index_new){
            return -1;
        }
    }
    if(index_new!=index){
        index = index_new;
        hc_dict_set_num(key_to_index,key,key_len,index,4);
    }
    return index;
}

uint64_t hc_timedict_get_num(HC_TimeDict *td,void *key,int key_len,int *pIndex,int flag)
{
    HC_TimeArray *time_array = td->time_array;
    HC_Dict *key_to_index = td->dict;
    int index_new, ret;
    int index = hc_dict_get_num(key_to_index,key,key_len,&ret);
    
    HC_TimeArrayItem *ta_item=hc_ta_get_item(time_array,index,flag,0);
    if(!ta_item){
        *pIndex = -1;
        return 0;
    }
    index_new = ta_item->index;
    *pIndex = index_new;
    if(index_new!=index){
        hc_dict_set_num(key_to_index, key, key_len,index_new,4);
    }
    return ta_item->u64;
}

