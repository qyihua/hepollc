#pragma once

#include "../util/hc_data.h"
#include "co_memblock.h"


#define COFD_FREE 0
#define COFD_USING 1
#define CO_ERROR 2
#define CO_OK 0
#define CO_NEEDSPACE 3
#define CO_EAGAIN 1
#define CO_SSL_WANT_WRITE 4
#define MAX_EVENT 1000
#define COFD_MAX 100000
#define COTYPE_FUN 1
#define COTYPE_STACK 2
#define COTYPE_NOPOOLSTACK 3
#define CO_FDTYPE_FD 1
#define CO_FDTYPE_TIME 2

#define CO_TYPE_FUNFD 0x11
#define CO_TYPE_FUNTIME 0x21
#define CO_TYPE_STACKFD 0x12
#define CO_TYPE_STACKTIME 0x22
#define CO_TYPE_NOPOOLSTACKFD 0x13
#define CO_TYPE_NOPOOLSTACKTIME 0x23

#define CO_FLAG_1RW 0b00000111
#define CO_FLAG_0RW 0b00000110
#define CO_FLAG_ZERO 0b00000000
#define CO_FLAG_1ZERO 0b00000001

typedef struct HC_CoCallBack{
    void* fun;
    void* arg1;
    void* arg2;
}HC_CoCallBack;

typedef struct HC_CoCallBackQueue{
    HC_CoCallBack* callbacks;
    HC_RecycleQueue *queue;
}HC_CoCallBackQueue;

typedef struct HC_CoLoop{
    int fd;
    HC_CoCallBackQueue *callback;
    HC_CoMemPool *mem_pool;
}HC_CoLoop; 

typedef struct HC_CoFd{
    union{
        uint8_t flag:8;
        struct{
            uint8_t no_del:1;
            uint8_t read_and_write:1; 
            uint8_t r_flg:1;
            uint8_t w_flg:1;
            uint8_t _:4;
        };
    };
    union{
        struct{
            uint8_t co_type:4;
            uint8_t fd_type:4;
        };
        uint8_t _type;
    };
    uint8_t status; 
    uint8_t my_status; 
    int fd;
    uint32_t events;
    uint32_t raw_events;
    union{
        uint64_t u64;
        void *fun;
    };
    void *ptr;
    void *ptr2;
    char buf[0];
}HC_CoFd;

int hc_co_init_handle();
HC_CoCallBackQueue *hc_co_callbackqueue_create(int size);
HC_CoLoop *hc_co_ioloop_create(int max_callback,uint64_t max_stack);
int hc_co_cancel_callback(void* data);
int hc_co_callback_in(void *data, void *arg);
HC_CoCallBack *hc_co_prepare_callback(void *fun, void *arg1, void *arg2);
HC_CoCallBack* hc_co_add_callback(void *fun, void *arg1, void *arg2);
//int hc_co_ioloop_start()__attribute__((optimize(0)));
int hc_co_ioloop_start();
void  hc_co_call_cofd(HC_CoFd *cofd, uint32_t ev);
HC_CoFd * hc_co_get_cofd(int fd,uint8_t type, uint8_t flag);
int hc_co_recycle_cofd(HC_CoFd *cofd);
int hc_mod_fd_to_epoll(int fd,void *u64,uint32_t events);
int hc_add_fd_to_epoll_no_mod(int fd,void *u64,uint32_t events);
int hc_add_fd_to_epoll(int fd,void *u64,uint32_t events);
int hc_del_fd_from_epoll(int fd);
int hc_get_timerfd(int sec,int nsec);
int hc_set_timerfd(int timefd,int sec,int nsec);
HC_CoFd* hc_cofd_mod_to_epoll_timeout(HC_CoFd *cofd, uint32_t events,int timeout);
HC_CoFd* hc_cofd_add_to_epoll_timeout(HC_CoFd *cofd, uint32_t events,int timeout);
int hc_cofd_add_to_epoll(HC_CoFd *cofd, uint32_t events);
int hc_cofd_mod_to_epoll(HC_CoFd *cofd, uint32_t events);
int hc_co_close_cofd(HC_CoFd *cofd);
int hc_fork(int process_num);
