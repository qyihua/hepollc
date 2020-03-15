#pragma once
#include "hc_base_type.h"
#include "hc_array.h"
typedef struct HC_DictKey{
    uint8_t type; //0:raw 1:num
    uint8_t status:7;
    uint8_t no_format:1;
    uint8_t flg_nxt;
    uint8_t len;
    uint32_t item_index;
    char data[0];
}HC_DictKey;

typedef struct HC_DictData{
    uint8_t type:4; //0:raw 1:num
    uint8_t type2:4; //custom type
    uint8_t status;
    uint32_t len;
    uint32_t max_len;
    char data[0];
}HC_DictData;

typedef struct HC_DictItem{
    uint8_t type;
    uint8_t user_flag:4;
    uint8_t status:4;
    int key_index;
    int nxt_item_index;
    union{
        uint64_t data;
        uint64_t data_index;
    };
}HC_DictItem;

typedef struct HC_Dict{
    uint8_t type;
    uint8_t bit;
    uint8_t flg_fix;
    uint32_t item_num;
    uint32_t nxt_link_top;
    uint32_t item_max_size;
    HC_DictItem *items;
    HC_DictItem *nxt_items;
    HC_MemBuf key_mem;
    HC_MemBuf data_mem;
    void *key_to_index;
}HC_Dict;


HC_Dict *hc_dict_create(uint8_t type,uint32_t size_a);
HC_DictItem* hc_dict_has(HC_Dict *dict,const char *str,int len);
HC_DictItem* hc_dict_get_item(HC_Dict *dict,const char *str,int len);
long hc_dict_get_num(HC_Dict *dict,const char *str,int len,int *ret);
char *hc_dict_get_ptr(HC_Dict *dict,const char *str,int len,int *ret);
int hc_dict_get_data_len(HC_Dict *dict,const char *str,int len);
HC_DictItem * hc_dict_set_item(HC_Dict *dict,const char *str,int len,void* data,int data_len,char data_type);
HC_DictItem * hc_dict_set_num(HC_Dict *dict,const char *str,int len,long data,int data_len);
int hc_dict_push_data(HC_Dict *dict,HC_DictItem *item,void * data,int data_len,char data_type);
int hc_dict_remap(HC_Dict *dict,int size,char bit);
int hc_dict_free(HC_Dict *dict);
int hc_dict_format(HC_Dict *dict,char *buf,int bufLen,const char *pFormat,int a_len,char *pNull);
HC_Dict* hc_dict_copy(HC_Dict *src);

HC_DictItem * hc_dict_set_item_by_num(HC_Dict *dict,uint64_t key,int len,void* data,int data_len,char data_type);
int hc_dict_copy_to_dst(HC_Dict *src,HC_Dict *dst);
int hc_dict_reset_to_zero(HC_Dict *dst);
HC_DictItem *hc_dict_get_item_by_kindex(HC_Dict *dict,int *kindex);
HC_Array* hc_dict_get_array(HC_Dict *dict, char *str, int len);
int hc_json_to_dict(HC_Dict *p_dict,char *pStr,int a_len);
HC_MemBuf* hc_dict_to_json(HC_Dict *dict, HC_MemBuf *mem);
HC_MemBuf* hc_dict_to_jsbuf(HC_Dict *dict, HC_MemBuf *mem);
