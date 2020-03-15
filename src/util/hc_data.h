#pragma once
#include "hc_dict.h"
typedef struct HC_DataQueue{
    int cur_len;
    int in_index;
    int out_index;
    int max;
    uint64_t  *datas;
    void *ptr;
}HC_DataQueue;

typedef struct HC_RecycleDataQueue{
    int re_cur_len;
    int re_in_index;
    int re_out_index;
    int cur_len;
    int in_index;
    int out_index;
    int max;
    uint64_t  *datas;
    void *ptr;
}HC_RecycleDataQueue;

typedef struct HC_Queue{
    int cur_len;
    int in_index;
    int out_index;
    int max;
    uint64_t  *datas;
    void *ptr;
}HC_Queue;

typedef struct HC_RecycleQueue{
    int re_cur_len;
    int re_in_index;
    int re_out_index;
    int cur_len;
    int in_index;
    int out_index;
    int max;
    uint64_t  *datas;
    int  *indexs;
    void *ptr;
}HC_RecycleQueue;


typedef struct HC_SeqQueue{
    int cur_len;
    int in_index;
    int out_index;
    int max;
    void *ptr;
}HC_SeqQueue;

typedef struct HC_ConnPool{
    int step_num;
    int keep_num;
    int type;
    int max;
    HC_Dict *param;
    HC_Queue *queue;
    HC_Queue *recycle_queue;
    HC_Queue *wait_queue;
    struct HC_ConnItem *items;
    int (*fun_get)(struct HC_ConnItem*);
    int (*fun_recycle)(struct HC_ConnItem*);
    void *ptr;
}HC_ConnPool;

typedef struct HC_ConnItem{
    char type; // 1:pg,3:orcl,4:odbc
    char custom_flg;
    char status;
    uint8_t count;
    int fd;
    int index;
    union{
        void *ptr;
        long l64;
        unsigned long u64;
    };
    union{
        void *ptr2;
        void *dbdata;
    };
    HC_ConnPool *pool;
}HC_ConnItem;



int hc_queue_in(HC_Queue *queue, void* data);
void* hc_queue_out(HC_Queue *queue);
HC_Queue* hc_queue_create(int max);
void hc_queue_free(HC_Queue *q);
int hc_queue_init_datas(HC_Queue *q, void* start_addr,int data_size);
HC_ConnPool* hc_connpool_create(int type, int keep_num, int max, int wait_num);
int hc_connpool_free(HC_ConnPool *pool);
int hc_connpool_add(HC_ConnPool *pool, int len);
HC_ConnItem* hc_connpool_get(HC_ConnPool *pool);
int hc_connpool_recycle(HC_ConnItem* item);
void hc_requeue_free(HC_RecycleQueue *q);
HC_RecycleQueue* hc_requeue_create(int max, void *start, int data_size);
void* hc_requeue_out(HC_RecycleQueue *queue);
int hc_requeue_in(HC_RecycleQueue *queue, void* data);
void* hc_requeue_get_data(HC_RecycleQueue *queue);
int hc_requeue_cancel_data(HC_RecycleQueue *queue, void* data);
int hc_queue_cancel_in(HC_Queue *queue, void*data);
HC_ConnItem* hc_connpool_wait(HC_ConnPool *pool);
void* hc_queue_drop_by_index(HC_Queue *queue, int index);
HC_ConnItem* hc_connpool_get_by_index(HC_ConnPool *pool,int index);
int hc_queue_set_zero(HC_Queue *queue);
