#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "../util/hc_dict.h"
#include "../util/hc_array.h"
#include "db.h"
#include "util/hc_base_type.h"
#include "../util/hc_util.h"

extern HC_Dict *gpConf;

static long g_mt_vals[7];

HC_DBData *hc_dbdata_alloc()
{
    HC_DBData *p=calloc(sizeof(HC_DBData),1);
    p->data = hc_array_create(16);
    return p;
}

int hc_dbdata_reset(HC_DBData *p)
{
    p->row_num=0;
    p->col_num=0;
    hc_array_reset_to_zero(p->data);
    return 0;
}

void hc_dbdata_free(HC_DBData *p)
{
    if(p->data)
        hc_array_free(p->data);
    free(p);
}

char *hc_db_get_col(HC_DBData *pdb,int row,int col,int *ret)
{
    char *pTmp;
    if(row >= pdb->row_num || col>=pdb->col_num){
//        printf("pdbrow:%d,pwcol:%d\n",pdb->row_num,pdb->col_num);
        *ret = -1;
        return NULL;
    }
    int index = (row+1)*pdb->col_num+col;
    HC_Array *pA = pdb->data;
    pTmp = hc_array_get_item_retptr(pA,index,ret);
    return pTmp;
}

HC_Array *hc_db_get_row(HC_DBData *pdb,int row)
{
    char *pTmp;
    if(row >= pdb->row_num){
        return NULL;
    }
    HC_Array *pA = pdb->data;
    pA->start_index = (row+1)*pdb->col_num;
    return pA;
}

HC_MemBuf* hc_dbdata_to_jsbuf(HC_DBData *dbdata, HC_MemBuf *mem, int start,
        int slice_len, uint8_t *indexs)
{
    HC_Array *array = dbdata->data;
    int row = dbdata->row_num;
    int col = dbdata->col_num;
    long n=0;
    char *pbuf = NULL, *data_buf=array->data_buf;

    if(!mem){
        mem = hc_membuf_create(6);
    }else{
        hc_membuf_check_size(mem, 6);
    }
    char *ptmp;
    HC_ArrayData *arrdata;
    int cur_len = mem->len;
    HC_JsBuf *jsbuf;
    hc_jsbuf_set_header(mem, 4, 0, JS_TYPE_DICT);
    int i=0,len,num;
    for(i=0;i<2;i++){
        if(i==0){
            ptmp="row"; len=3;
            num = dbdata->row_num;
        }else{
            ptmp="count"; len=5;
            num = dbdata->count;
        }
        hc_jsbuf_set_data(mem, ptmp, len, TYPE_RAW);
        hc_jsbuf_set_data(mem, &num, 4, TYPE_NUM);
    }

    if(!indexs && slice_len==0){
        slice_len = col;
    }
    hc_jsbuf_set_data(mem, "keys", 4, TYPE_RAW);
    array->start_index = 0;
    hc_array_to_jsbuf(array, mem, 0, slice_len, indexs);

    hc_jsbuf_set_data(mem, "datas", 5, TYPE_RAW);
    int datas_top = mem->len;
    hc_jsbuf_set_header(mem, 4, 0, JS_TYPE_ARRAY);

    for(i=0;i<row;i++){
        array->start_index += col;
        hc_array_to_jsbuf(array, mem, start, slice_len, indexs);
    }
    jsbuf = (HC_JsBuf*)(mem->buf+ datas_top);
    jsbuf->len4 = mem->len - datas_top - 5;

    jsbuf = (HC_JsBuf*)(mem->buf+ cur_len);
    mem->cur_len = mem->len - cur_len;
    jsbuf->len4 = mem->cur_len - 5;
    return mem;

}
