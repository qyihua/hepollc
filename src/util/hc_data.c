#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hc_data.h"
#include "hc_util.h"
#include "../coroutine/coroutine.h"


int hc_requeue_cancel_data(HC_RecycleQueue *queue, void* data)
{
    int top = queue->re_out_index-1;
    queue->datas[top] = (uint64_t)data;
    queue->re_cur_len++;
    if(0>top) top=queue->max-1;
    queue->re_out_index = top;
    return 0;
}

 void* hc_requeue_get_data(HC_RecycleQueue *queue)
{
    if(0==queue->re_cur_len) return NULL;
    int top = queue->re_out_index;
    uint64_t data = queue->datas[top++];
    queue->re_cur_len--;
    if(top==queue->max) top=0;
    queue->re_out_index = top;
    return (void*)data;
}

int hc_requeue_in(HC_RecycleQueue *queue, void* data)
{
    int top = queue->in_index;
    int ret = top;
    queue->cur_len++;
    queue->datas[top++] = (uint64_t)data;
    if(top>=queue->max) top=0;
    queue->in_index = top;
    return ret;
}

void* hc_requeue_out(HC_RecycleQueue *queue)
{
    if(0==queue->cur_len) return NULL;
    int top = queue->out_index;
    uint64_t data = queue->datas[top++];
    queue->cur_len--;
    queue->re_cur_len++;
    if(top==queue->max) top=0;
    queue->out_index = top;
    return (void*)data;
}

HC_RecycleQueue* hc_requeue_create(int max, void* start_addr, int data_size)
{
    HC_RecycleQueue *q=calloc(sizeof(HC_RecycleQueue),1);
    q->max = max;
    q->datas = (uint64_t*)malloc(sizeof(uint64_t)*max);
    uint64_t *datas = q->datas;
    q->re_cur_len=max;
    uint64_t start = (uint64_t)start_addr;
    for(int i=0;i<max;i++, start+=data_size){
        datas[i] = start;
    }
    return q;
}

void hc_requeue_free(HC_RecycleQueue *q)
{
    free(q->datas);
    free(q);
}


int hc_queue_cancel_in(HC_Queue *queue, void *data)
{
    int top = queue->in_index;
    queue->cur_len--;
    queue->datas[--top] = (uint64_t)data;
    if(0>top) top=queue->max-1;
    queue->in_index = top;
    return 0;
}

int hc_queue_in(HC_Queue *queue, void* data)
{
    if(queue->cur_len==queue->max) return -1;
    int top = queue->in_index;
    int ret = top;
    queue->cur_len++;
    queue->datas[top++] = (uint64_t)data;
    if(top>=queue->max) top=0;
    queue->in_index = top;
    return ret;
}

void* hc_queue_out(HC_Queue *queue)
{
    if(0==queue->cur_len) return NULL;
    int top = queue->out_index;
    uint64_t data = queue->datas[top++];
    queue->cur_len--;
    if(top==queue->max) top=0;
    queue->out_index = top;
    return (void*)data;
}

int hc_queue_set_zero(HC_Queue *queue)
{
    queue->cur_len = 0;
    queue->in_index=0;
    queue->out_index =0;
    return 0;
}

void* hc_queue_drop_by_index(HC_Queue *queue, int index)
{
    if(0==queue->cur_len) return NULL;
    int top = queue->out_index;
    uint64_t data;
    if(index!=top){
        data = queue->datas[top];
        queue->datas[index] = data;
    }else
        data = 0;
    top++;
    queue->cur_len--;
    if(top==queue->max) top=0;
    queue->out_index = top;
    return (void*)data;
}

HC_Queue* hc_queue_create(int max)
{
    HC_Queue *q=calloc(sizeof(HC_Queue),1);
    q->max = max;
    q->datas = (uint64_t*)malloc(sizeof(uint64_t)*max);
    return q;
}

void hc_queue_free(HC_Queue *q)
{
    free(q->datas);
    free(q);
}

int hc_queue_init_datas(HC_Queue *q, void* start_addr,int data_size)
{
    uint64_t *datas = q->datas;
    int size = q->max;
    q->cur_len=size;
    uint64_t start = (uint64_t)start_addr;
    for(int i=0;i<size;i++,start+=data_size){
        datas[i] = start;
    }
    return 0;
}


HC_SeqQueue *hc_seqqueue_create(int max){
    HC_SeqQueue *pHQ=calloc(sizeof(HC_SeqQueue),1);
    pHQ->max = max;
    return pHQ;
}

void HC_seqqueue_free(HC_SeqQueue *pHQ){
    free(pHQ);
}

int hc_seqqueue_in(HC_SeqQueue *pHQ){
    int top=pHQ->in_index,  max=pHQ->max;
    if( pHQ->max==pHQ->cur_len ){
        return -1;
    }
    int ret=top;
    if(++top>=max)
        top=0;
    pHQ->in_index=top;
    pHQ->cur_len++;
    return ret;
}

int hc_seqqueue_out(HC_SeqQueue *pHQ){
    int top=pHQ->out_index, max=pHQ->max;
    if( 0==pHQ->cur_len ){
        return -1;
    }
    int ret = top;
    if(++top>=max)
        top=0;
    pHQ->out_index = top;
    pHQ->cur_len--;
    return ret;
}

int hc_seqqueue_out1(HC_SeqQueue *pHQ){
    if( 0==pHQ->cur_len ){
        return -1;
    }
    return pHQ->out_index;
}

void hc_seqqueue_out2(HC_SeqQueue *pHQ){
    int top=pHQ->out_index;
    if(++top>=pHQ->max)
        top=0;
    pHQ->out_index = top;
    pHQ->cur_len--;
    return;
}


HC_ConnPool* hc_connpool_create(int type, int keep_num,int max, int wait_num)
{
    HC_ConnPool *pool=NULL;
    int ret=0;
    pool=calloc(sizeof(HC_ConnPool),1);
    HC_ConnItem *items = calloc(sizeof(HC_ConnItem),max);
    if(max<keep_num)
        max = keep_num;
    pool->type=type;
    pool->keep_num=keep_num;
    pool->max=max;
    ret = keep_num>>1;
    if(1>ret) ret = 1;
    pool->step_num = ret;
    pool->param = hc_dict_create(TYPE_RAW,32);
    pool->queue = hc_queue_create(max);
    pool->recycle_queue = hc_queue_create(max);
    hc_queue_init_datas(pool->recycle_queue,items,sizeof(HC_ConnItem));

    if(0<wait_num){
        pool->wait_queue = hc_queue_create(wait_num);
        pool->wait_queue->ptr = calloc(sizeof(uint64_t), wait_num);
    }

    pool->items=items;
    for(int i=0; i<max; i++){
            items->pool = pool;
            items->index = i;
            items->type=type;
            items++;
    }
    return pool;
}

int hc_connpool_free(HC_ConnPool *pool)
{
    hc_queue_free(pool->queue);
    hc_queue_free(pool->recycle_queue);
    if(pool->wait_queue){
        free(pool->wait_queue->ptr);
        hc_queue_free(pool->wait_queue);
    }
    hc_dict_free(pool->param);
    free(pool->items);
    free(pool);
    return 0;
}

HC_ConnItem* hc_connpool_wait(HC_ConnPool *pool)
{
    HC_Queue *q=pool->wait_queue;
    int wait_index = hc_queue_in(q,0);
    if(0>wait_index){
        return NULL;
    }
    HC_ConnItem *ci = hc_co_wait_callback(q->datas+wait_index);
    if(!q->datas[wait_index]){
        hc_queue_cancel_in(q,0);
        return NULL;
    }
    return ci;
}

HC_ConnItem* hc_connpool_get(HC_ConnPool *pool)
{
    HC_Queue *q=pool->queue;
    int ret;
    HC_ConnItem *ci = hc_queue_out(q);
    if(ci) return ci;
    ret = hc_connpool_add(pool, pool->step_num);
    if(0<ret){
        return hc_queue_out(q);
    }
    if(!pool->wait_queue) return NULL;

    return hc_connpool_wait(pool);
}

int hc_connpool_add(HC_ConnPool *pool, int len)
{
    int i;
    void *data;
    HC_Queue *qr = pool->recycle_queue,
        *q = pool->queue;;
    for(i=0;i<len;i++){
      data = hc_queue_out(qr);
      if(!data) break;
      hc_queue_in(q,data);
    }
    return i;
}

int hc_connpool_recycle(HC_ConnItem* item)
{
    HC_ConnPool *pool = item->pool;
    HC_Queue *q=pool->wait_queue;
    if(q){
        HC_CoCallBack *cb = hc_queue_out(q);
        if(cb){
            hc_co_callback_in(cb,item);
            return 0;
        }
    }
    q=pool->queue;
    if(pool->keep_num <= q->cur_len){
        hc_queue_in(pool->recycle_queue, item);
        return 1;
    }
    hc_queue_in(q, item);
    return 0;
}

HC_ConnItem* hc_connpool_get_by_index(HC_ConnPool *pool,int index)
{
    if(0>index || pool->max<=index){
        return NULL;
    }
    return pool->items+index;
}

