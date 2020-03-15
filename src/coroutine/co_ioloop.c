#include <stdio.h>
#include <stdlib.h>
#include <errno.h> 
#include <unistd.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <string.h>
#include <sys/timerfd.h>
#include <sys/prctl.h>

#include "../util/hc_util.h"
#include "coroutine.h"

int g_process_id=1;

HC_CoLoop *g_co_loop;
HC_CoMemPool *g_hc_co_mem_pool;
extern HC_Dict* g_confs;

HC_Queue *g_cofds=NULL;

uint64_t g_coroutine_base;

void hc_block_SIGPIPE()
{
    signal(SIGPIPE, SIG_IGN);
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    sigprocmask(SIG_BLOCK, &set, NULL);
    return ;
}

int hc_co_init_handle()
{
    uint64_t u64;
    int ret;
    u64 = hc_dict_get_num(g_confs,"max_cofd",-1,&ret);
    if(1>u64) u64 = COFD_MAX;
    hc_printf_once_green("----  max cofd:%ld\n",u64);
    g_cofds = hc_queue_create(u64);
    g_cofds->ptr = malloc(sizeof(HC_CoFd)*u64);
    hc_queue_init_datas(g_cofds,g_cofds->ptr,sizeof(HC_CoFd));
    return 0;
}

void stop()
{
    close(g_co_loop->fd);
    exit(0);
}

HC_CoCallBackQueue *hc_co_callbackqueue_create(int size)
{
    HC_CoCallBackQueue *q = calloc(1,sizeof(HC_CoCallBackQueue));
    q->callbacks = calloc(sizeof(HC_CoCallBack), size);
    q->queue = hc_requeue_create(size,q->callbacks,sizeof(HC_CoCallBack));
    return q;
}

HC_CoLoop *hc_co_ioloop_create(int max_callback,uint64_t max_stack)
{

    HC_CoLoop *co_loop = calloc(1,sizeof(HC_CoLoop));
    
    int efd=epoll_create(1);
    void *ptr;
    if(-1==efd){
        perror("create epoll error:");
        return NULL;
    }
    co_loop->fd=efd;
    co_loop->callback= hc_co_callbackqueue_create(max_callback);

    ptr = hc_co_memblock_create(max_stack);
    if(!ptr){
        printf("create ioloop error\n");
        return NULL;
    }
    co_loop->mem_pool = ptr;
    g_hc_co_mem_pool = co_loop->mem_pool;
    g_co_loop = co_loop;
    return co_loop;
}

int hc_co_cancel_callback(void* data)
{
    return hc_requeue_cancel_data(g_co_loop->callback->queue, data);
}

int hc_co_callback_in(void *data, void *arg)
{
    ((HC_CoCallBack*)data)->arg2 = arg;
    return hc_requeue_in(g_co_loop->callback->queue, data);
}

HC_CoCallBack *hc_co_prepare_callback(void *fun, void *arg1, void *arg2)
{
    HC_RecycleQueue *queue = g_co_loop->callback->queue;
    HC_CoCallBack *cb = hc_requeue_get_data(queue);
    if(!cb) return NULL;
    cb->fun=fun;
    cb->arg1=arg1;
    cb->arg2=arg2;
    return cb;
}

HC_CoCallBack* hc_co_add_callback(void *fun, void *arg1, void *arg2)
{
    HC_RecycleQueue *queue = g_co_loop->callback->queue;
    HC_CoCallBack *cb = hc_requeue_get_data(queue);
    if(!cb) return NULL;
    cb->fun=fun;
    cb->arg1=arg1;
    cb->arg2=arg2;
    hc_requeue_in(queue, cb);
    return cb;
}

int hc_co_ioloop_start()
{
    signal(SIGINT, stop);
    hc_block_SIGPIPE();
    volatile int ev_num;
    volatile int i;
    volatile int cb_num=0;
    volatile int efd=g_co_loop->fd;
    volatile uint64_t cb_queue = (uint64_t)g_co_loop->callback->queue;
    volatile uint64_t  events;
    struct epoll_event  *ev;
    events = (uint64_t)malloc(MAX_EVENT*sizeof(struct epoll_event));
    HC_CoCallBack *cb;
    HC_CoFd *cofd=NULL;
    __asm__(
        "movq %%rsp, %0;"
        :"=r"(g_coroutine_base)
    );
    g_coroutine_base -= 16;
    for(;;){
        cb_num = ((HC_RecycleQueue*)cb_queue)->cur_len;
        ev_num = epoll_wait(efd, (struct epoll_event*)events, MAX_EVENT, 0<cb_num?0:-1);

        if(-1 == ev_num){
            perror("epoll_wait");
            if(errno!=EINTR){
                close(efd);
                free((void*)events);
                exit(EXIT_FAILURE);
            }
            continue;
        }
        for(i = 0; i<ev_num; i++){
            ev = (struct epoll_event*)events+i;
            cofd = (HC_CoFd*)ev->data.u64;
            if(cofd->read_and_write && cofd->w_flg){
                continue;
            }
            if(!cofd->no_del){
                epoll_ctl(efd, EPOLL_CTL_DEL, cofd->fd, ev);
            }
            hc_co_call_cofd(cofd, ev->events);
        }
        for(i=0;i<cb_num;i++){
            cb = hc_requeue_out((HC_RecycleQueue*)cb_queue);
            ((int (* )(void*,void*))cb->fun) (cb->arg1,cb->arg2);
        }
    }
    return 0;
}

void  hc_co_call_cofd(HC_CoFd *cofd, uint32_t ev)
{
    int (*co_fun)(HC_CoFd*,uint32_t);
    if(COFD_FREE == cofd->status){
        hc_printf_error("  cofd has free..:%d,ev:%x\n",cofd->fd,ev);
        return;
    }
    if(COTYPE_FUN == cofd->co_type){
        cofd->events = ev;
        co_fun = cofd->fun;
        co_fun(cofd, ev);
    }else if(COTYPE_STACK == cofd->co_type || COTYPE_NOPOOLSTACK ==cofd->co_type){
        cofd->events = ev;
        hc_co_context_resume_cofd(cofd);
    }
}


HC_CoFd * hc_co_get_cofd(int fd,uint8_t type, uint8_t flag)
{
    HC_CoFd *cofd = (HC_CoFd*)hc_queue_out(g_cofds);
    if(!cofd) return NULL;
    cofd->status = COFD_USING;
    cofd->fd = fd;
    cofd->_type=type;
    cofd->flag = flag;
    return cofd;
}

int hc_co_recycle_cofd(HC_CoFd *cofd)
{
    cofd->status = COFD_FREE;
    int index = hc_queue_in(g_cofds, cofd);
    return index;
}

int hc_mod_fd_to_epoll(int fd,void *u64,uint32_t events)
{
    int efd = g_co_loop->fd;
    struct epoll_event ev;
    if(0==events) events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.events = events;
    ev.data.u64=(uint64_t)u64;
    if(-1 == epoll_ctl(efd, EPOLL_CTL_MOD, fd, &ev)){
        if(-1 == epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev)){
            perror("epoll_ctl");
            return -1;
        }
    }
    return 0;
}

int hc_add_fd_to_epoll_no_mod(int fd,void *u64,uint32_t events)
{
    int efd = g_co_loop->fd;
    struct epoll_event ev;
    if(0==events) events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.events = events;
    ev.data.u64=(uint64_t)u64;
    if(-1 == epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev)){
        if(EEXIST == errno){
            return 1;
        }
        else{
            perror("epoll_ctl");
            return -1;
        }
    }
    return 0;
}

int hc_add_fd_to_epoll(int fd,void *u64,uint32_t events)
{
    int efd = g_co_loop->fd;
    struct epoll_event ev;
    if(0==events) events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.events = events;
    ev.data.u64=(uint64_t)u64;
    if(-1 == epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev)){
        if(EEXIST == errno){
            if(-1 == epoll_ctl(efd, EPOLL_CTL_MOD, fd, &ev)){
                perror("epoll_ctl");
                return -1;
            }
        }
        else{
            perror("epoll_ctl");
            return -1;
        }
    }
    return 0;
}

int hc_del_fd_from_epoll(int fd)
{
    int efd = g_co_loop->fd;
    struct epoll_event ev;
    epoll_ctl(efd, EPOLL_CTL_DEL, fd, &ev);
    return 0;
}

int hc_get_timerfd(int sec,int nsec)
{
    int timefd;
    struct itimerspec new_value;
    timefd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if(-1==timefd){
        perror("timefd create:\n");
        return -1;
    }
    new_value.it_value.tv_sec = sec;
    new_value.it_value.tv_nsec = nsec;
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;
    if(-1==timerfd_settime(timefd,0,&new_value,NULL)){
        perror("timefd set:\n");
        close(timefd);
        return -1;
    }
    return timefd;
}

int hc_set_timerfd(int timefd,int sec,int nsec)
{
    struct itimerspec new_value;
    new_value.it_value.tv_sec = sec;
    new_value.it_value.tv_nsec = nsec;
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;
    if(-1==timerfd_settime(timefd,0,&new_value,NULL)){
        perror("timefd set:\n");
        close(timefd);
        return -1;
    }
    return 0;
}

HC_CoFd* hc_cofd_mod_to_epoll_timeout(HC_CoFd *cofd, uint32_t events,int timeout)
{
    cofd->flag=0;
    cofd->raw_events = events;
    if(-1==hc_mod_fd_to_epoll(cofd->fd, cofd, events)){
        return NULL;
    }
    int timefd=hc_get_timerfd(timeout,0);
    if(0>timefd){
        hc_del_fd_from_epoll(cofd->fd);
        return NULL;
    }
    HC_CoFd *cofd_time = hc_co_get_cofd(timefd,CO_TYPE_STACKTIME, CO_FLAG_ZERO);
    if(!cofd_time){
        hc_del_fd_from_epoll(cofd->fd);
        close(timefd);
        return NULL;
    }

    cofd_time->co_type = cofd->co_type;
    cofd_time->fun = cofd->fun;
    cofd_time->ptr = cofd->ptr;
    cofd_time->ptr2 = cofd;
    cofd->ptr2 = cofd_time;
    
    if(-1==hc_add_fd_to_epoll(timefd, cofd_time, EPOLLIN)){
        hc_co_recycle_cofd(cofd_time);
        hc_del_fd_from_epoll(cofd->fd);
        close(timefd);
        return NULL;
    }
    return cofd_time;
}

HC_CoFd* hc_cofd_add_to_epoll_timeout(HC_CoFd *cofd, uint32_t events,int timeout)
{
    cofd->flag=0;
    cofd->raw_events = events;
    if(-1==hc_add_fd_to_epoll(cofd->fd, cofd, events)){
        return NULL;
    }
    int timefd=hc_get_timerfd(timeout,0);
    if(0>timefd){
        hc_del_fd_from_epoll(cofd->fd);
        return NULL;
    }
    HC_CoFd *cofd_time = hc_co_get_cofd(timefd,CO_TYPE_STACKTIME, CO_FLAG_ZERO);
    if(!cofd_time){
        hc_del_fd_from_epoll(cofd->fd);
        close(timefd);
        return NULL;
    }

    cofd_time->co_type = cofd->co_type;
    cofd_time->fun = cofd->fun;
    cofd_time->ptr = cofd->ptr;
    cofd_time->ptr2 = cofd;
    cofd->ptr2 = cofd_time;
    
    if(-1==hc_add_fd_to_epoll(timefd, cofd_time, EPOLLIN)){
        hc_co_recycle_cofd(cofd_time);
        hc_del_fd_from_epoll(cofd->fd);
        close(timefd);
        return NULL;
    }
    return cofd_time;
}

int hc_cofd_add_to_epoll(HC_CoFd *cofd, uint32_t events)
{
    cofd->raw_events = events;
    cofd->r_flg = 1;
    if(!cofd->w_flg && -1==hc_add_fd_to_epoll(cofd->fd, cofd, events)){
        return -1;
    }
    return 0;
}

int hc_cofd_mod_to_epoll(HC_CoFd *cofd, uint32_t events)
{
    cofd->raw_events = events;
    if(-1==hc_mod_fd_to_epoll(cofd->fd, cofd, events)){
        return 1;
    }
    return 0;
}

int hc_co_close_cofd(HC_CoFd *cofd)
{
    close(cofd->fd);
    cofd->status = COFD_FREE;
    return hc_queue_in(g_cofds, cofd);
}

int hc_fork(int process_num)
{

    int i;
    process_num--;
    for(i=0;i<process_num;i++){
        g_process_id = fork();
        if(0>g_process_id){
            hc_printf_error("create process:%d fail, exit\n",i);
            return -1;
        }
        if(g_process_id==0){
            prctl(PR_SET_PDEATHSIG, SIGHUP);
            break;
        }
    }
    return 0;
}

