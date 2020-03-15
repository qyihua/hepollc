#pragma once
#include "hc_base_type.h"
typedef struct HC_Array{
    char type;
    int max_size;
    int start_index;
    int item_num;
    int data_max;
    int data_top;
    int *data_indexs;
    char *data_buf;
}HC_Array;

typedef struct HC_ArrayData{
    char type; //0:raw 1:num
    char type2; // custom
    char status;
    int len;
    int max_len;
    char data[0];
}HC_ArrayData;

HC_Array *hc_array_create(int size);
int hc_array_free(HC_Array *array);
int hc_array_get_item_retlen(HC_Array *array,int index,void *data);
char* hc_array_get_item_retptr(HC_Array *array,int index,int *len_ptr);
int hc_array_set_item(HC_Array *array,int index,void* data,int data_len,short type);
int hc_array_append_item(HC_Array *array,void *data,int data_len,short type);
HC_ArrayData* hc_array_append_data(HC_Array *array,int index,void *data,int data_len,short type);
int hc_array_set_item_as_num(HC_Array *array,int index,long data,int data_len);
int hc_array_append_item_as_num(HC_Array *array,long data,int data_len);
HC_MemBuf* hc_array_to_json(HC_Array *array,HC_MemBuf*,int a_start,int a_end);
char* hc_array_to_str(HC_Array *array,int *a_len,char **a_pStr,int *a_maxSize,int a_start,int a_end);
int hc_array_reset_to_zero(HC_Array *arr);
HC_ArrayData* hc_array_get_item(HC_Array *array,int index);
HC_MemBuf* hc_array_to_jsbuf(HC_Array *array, HC_MemBuf *mem, int start, int slice_len, uint8_t *indexs);
