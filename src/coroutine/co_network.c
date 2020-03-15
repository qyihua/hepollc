#include <stdio.h>
#include <stdlib.h>
#include <errno.h> 
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/timerfd.h>

#include "../util/hc_util.h"
#include "coroutine.h"

int setnonblocking(int sockfd) 
{
    int opts;

    opts = fcntl(sockfd, F_GETFL);
    if(opts < 0) {
        perror("fcntl(F_GETFL)\n");
        return -1;
    }
    opts = (opts | O_NONBLOCK);
    if(fcntl(sockfd, F_SETFL, opts) < 0) {
        perror("fcntl(F_SETFL)\n");
        return -1;
    }
    return 0;
}

int hc_accept_fd(int server_fd)
{
    struct sockaddr_in sa;
    int addrlen = sizeof(sa);
    int client_fd = accept(server_fd, (struct sockaddr *)&sa, &addrlen);
    if(-1 == client_fd){
        perror("accept");
        return -1;
    }
    setnonblocking(client_fd); 
    return client_fd;
}

int hc_socket_listen_reuseport(char *addr,int port,int backlog)
{
    int opt=1;
    struct sockaddr_in sa_s;

    memset(&sa_s,0,sizeof(sa_s));
    sa_s.sin_family=AF_INET;
    if(!addr)
        sa_s.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    else if('*'==addr[0])
        sa_s.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        sa_s.sin_addr.s_addr = inet_addr(addr);
    sa_s.sin_port = htons(port);

    int fd = socket(AF_INET, SOCK_STREAM,0);
    setnonblocking(fd);
    if(0>setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))){
        perror("setsockopt failed");
        return -1;
    }
    opt=1;
    if(0>setsockopt(fd, SOL_SOCKET, SO_REUSEPORT,&opt,sizeof(opt))){
        perror("setsockopt reuseport failed");
        return -1;
    }
    if(-1==bind(fd, (struct sockaddr *)&sa_s, sizeof(sa_s))){
        return -2;
    }
    if(-1==listen(fd,backlog)){
        perror("listen");
        return -1;
    }
    return fd;
}

int hc_socket_listen(char *addr,int port,int backlog)
{
    int opt=1;
    struct sockaddr_in sa_s;

    memset(&sa_s,0,sizeof(sa_s));
    sa_s.sin_family=AF_INET;
    if(!addr)
        sa_s.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    else if('*'==addr[0])
        sa_s.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        sa_s.sin_addr.s_addr = inet_addr(addr);
    sa_s.sin_port = htons(port);

    int fd = socket(AF_INET, SOCK_STREAM,0);
    setnonblocking(fd);
    if(0>setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))){
        close(fd);
        perror("setsockopt failed");
        return -1;
    }
    if(-1==bind(fd, (struct sockaddr *)&sa_s, sizeof(sa_s))){
        close(fd);
        return -2;
    }
    if(-1==listen(fd,backlog)){
        close(fd);
        perror("listen");
        return -2;
    }
    return fd;
}

inline int hc_co_read_fd_realloc(int fd,HC_MemBuf *mem,int read_max)
{
    int  tmp, max=mem->max, top=mem->len;
        
    int read_count_cur = mem->len;
    char *buf = mem->buf;
    for(;(tmp = read(fd, buf + top, max-top)) > 0;){
        top += tmp;
        if(top>=max){
            tmp = read_max - max;
            if(1>tmp) break;
            max += max;
            if(tmp>max) tmp=max;
            else max = read_max;
            mem->len = top;
            buf = hc_membuf_check_size(mem, tmp);
        }
    }
    read_count_cur = top - read_count_cur;
    if(1>read_count_cur){
        if(EAGAIN!=errno){
            perror("co_buf read");
            mem->status = CO_ERROR;
            return -1;
        }
        mem->status = CO_EAGAIN;
        return 0;
    }
    mem->status = CO_OK;
    mem->len = top;
    return read_count_cur;
}


inline int hc_co_read_fd_realloc_timeout(int fd, HC_MemBuf *mem,int read_max,int timeout)
{
    int ret=0;
    mem->event = 0;
    uint32_t ev;
    for(int i=0;i<2;i++){
        ret = hc_co_read_fd_realloc(fd,mem,read_max);
        if(CO_ERROR == mem->status) return -1;
        if(CO_EAGAIN!=mem->status){
            return ret;
        }
        ev = hc_co_wait_fd_timeout(fd,0, timeout);
        if(!ev) return -1;
        mem->event = ev;
    }
    if(0==ret) return -1;
    return ret;
}

int hc_co_read_fd_until(int fd, HC_MemBuf *mem,int until_top)
{
    int ret=0;
    mem->event = 0;
    uint32_t ev=0;
    for(int i=mem->len;i<until_top;i+=ret){
        ret = hc_co_read_fd_to_buf(fd,mem);
        //printf("  fd:%d read untile +:%d,i:%d,top:%d\n",fd,ret,i,until_top);
        if(CO_ERROR == mem->status) return -1;
        if(CO_EAGAIN!=mem->status){
            continue;
        }
        if(ev&EPOLLRDHUP) return -1;
        ev = hc_co_wait_fd(fd,0);
        if(!ev) return -1;
        mem->event = ev;
    }
    return ret;
}

int hc_co_read_cofd_until(HC_CoFd *cofd, HC_MemBuf *mem,int until_top)
{
    int ret=0,fd=cofd->fd;
    mem->event = 0;
    uint32_t ev=0;
    for(int i=mem->len;i<until_top;i+=ret){
        ret = hc_co_read_fd_to_buf(fd,mem);
        if(CO_ERROR == mem->status) return -1;
        if(CO_EAGAIN!= mem->status){
            continue;
        }
        if(ev&EPOLLRDHUP) return -1;
        cofd->r_flg = 1;
        ev = hc_co_wait_cofd(cofd,0);
        cofd->r_flg = 0;
        if(!ev) return -1;
        mem->event = ev;
    }
    return ret;
}

int hc_co_read_fd_timeout_until(int fd, HC_MemBuf *mem,int timeout,int until_top)
{
    int ret=0;
    mem->event = 0;
    uint32_t ev=0;
    for(int i=mem->len;i<until_top;i+=ret){
        ret = hc_co_read_fd_to_buf(fd, mem);
        if(CO_ERROR == mem->status) return -1;
        if(CO_EAGAIN!= mem->status){
            continue;
        }
        if(ev&EPOLLRDHUP) return -1;
        ev = hc_co_wait_fd_timeout(fd,0, timeout);
        if(!ev) return -1;
        mem->event = ev;
    }
    return ret;
}

int hc_co_read_fd_to_buf_timeout(int fd, HC_MemBuf *mem, int timeout)
{
    int ret=0;
    mem->event = 0;
    uint32_t ev;
    for(int i=0;i<2;i++){
        ret = hc_co_read_fd_to_buf(fd,mem);
        if(CO_ERROR == mem->status) return -1;
        if(CO_EAGAIN!= mem->status){
            return ret;
        }
        ev = hc_co_wait_fd_timeout(fd,0, timeout);
        if(!ev) return -1;
        mem->event = ev;
    }
    return ret;
}

inline int hc_co_read_fd_to_buf(int fd,HC_MemBuf *mem)
{
    int  tmp, max=mem->max, top=mem->len;
        
    if(top>=max){
        mem->status = CO_ERROR;
        return -2;
    }
    char *buf = mem->buf;
    int read_count_cur = mem->len;
    for(;(tmp = read(fd, buf + top, max-top)) > 0;){
        top += tmp;
        if(top>max){
            break;
        }
    }
    read_count_cur = top-read_count_cur;
    if(1>read_count_cur){
        if(EAGAIN!=errno){
            //perror("co_buf read");
            mem->status = CO_ERROR;
            return -1;
        }
        mem->status = CO_EAGAIN;
        return 0;
    }
    if(top>=max){
        mem->status = CO_NEEDSPACE;
    }else
        mem->status = CO_OK;
    mem->len = top;
    return read_count_cur;
}

uint64_t hc_co_create_wait()
{
    uint64_t rsp=(uint64_t)__builtin_frame_address(0);
    return hc_co_context_calloc(rsp);
}

void* hc_co_wait(uint64_t top_block)
{

    uint64_t regs[5];
    __asm__(
           "movq %%r12,%0;" 
           "movq %%r13,%1;" 
           "movq %%r14,%2;" 
           "movq %%r15,%3;" 
           "movq %%rbx,%4;" 
           :"=m"(regs[0]),"=m"(regs[1]),"=m"(regs[2]),"=m"(regs[3]),"=m"(regs[4])
           :
           :"r12","r13","r14","r15","rbx"
            );
    return hc_co_context_save_after_calloc2(top_block, regs);
}

HC_CoFd* hc_co_wait_fd_cofd(int fd,uint32_t events)
{
    HC_CoFd *cofd = hc_co_get_cofd(fd,CO_TYPE_STACKFD, CO_FLAG_ZERO);
    if(!cofd) return NULL;
    uint64_t top_block=(uint64_t)__builtin_frame_address(0);
    top_block = hc_co_context_calloc(0);

    if(1>top_block){
        hc_co_recycle_cofd(cofd);
        return NULL;
    }
    cofd->fd = fd;
    cofd->u64 = top_block;
    cofd->raw_events = events;

    if(-1==hc_add_fd_to_epoll(fd, cofd, events)){
        hc_co_memblock_free(NULL,top_block);
    }
    hc_co_context_save_after_calloc(top_block);

    hc_co_recycle_cofd(cofd);
    return cofd;
}

uint32_t hc_co_wait_fd(int fd,uint32_t events)
{
    HC_CoFd *cofd = hc_co_get_cofd(fd,CO_TYPE_STACKFD, CO_FLAG_ZERO);
    if(!cofd) return 0;
    uint64_t top_block = (long)__builtin_frame_address(0);
    top_block = hc_co_context_calloc(0);

    if(1>top_block){
        hc_co_recycle_cofd(cofd);
        return 0;
    }
    cofd->fd = fd;
    cofd->u64 = top_block;

    if(-1==hc_add_fd_to_epoll(fd, cofd, events)){
        hc_co_memblock_free(NULL,top_block);
    }
    hc_co_context_save_after_calloc(top_block);

    hc_co_recycle_cofd(cofd);
    return cofd->events;
}

HC_CoFd* hc_co_wait_fd_timeout_cofd(int fd, uint32_t events,int timeout)
{
    HC_CoFd *cofd = hc_co_get_cofd(fd,CO_TYPE_STACKFD, CO_FLAG_ZERO);
    HC_CoFd *cofd_time=NULL, *cofd_wait=NULL;
    if(!cofd) return NULL;
    int timefd=0;
    uint64_t top_block = (long)__builtin_frame_address(0);
    top_block = hc_co_context_calloc(0);

    if(1>top_block){
        hc_co_recycle_cofd(cofd);
        return NULL;
    }
    cofd->u64 = top_block;
    cofd->raw_events = events;

    if(-1==hc_add_fd_to_epoll(fd, cofd, events)){
        hc_co_memblock_free(NULL,top_block);
    }
    if(0<timeout){
        timefd=hc_get_timerfd(timeout,0);
        if(0>timefd){
            hc_co_recycle_cofd(cofd);
            hc_co_memblock_free(NULL,top_block);
            hc_del_fd_from_epoll(fd);
            return NULL;
        }
        cofd_time = hc_co_get_cofd(timefd,CO_TYPE_STACKTIME, CO_FLAG_ZERO);
        if(!cofd_time){
            hc_co_recycle_cofd(cofd);
            hc_co_memblock_free(NULL,top_block);
            hc_del_fd_from_epoll(fd);
            close(timefd);
            return NULL;
        }

        if(-1==hc_add_fd_to_epoll(timefd, cofd_time, EPOLLIN)){
            hc_co_recycle_cofd(cofd);
            hc_co_recycle_cofd(cofd_time);
            hc_co_memblock_free(NULL,top_block);
            hc_del_fd_from_epoll(fd);
            close(timefd);
            return NULL;
        }
        cofd_time->u64 = top_block;
    }
    cofd_wait = hc_co_context_save_after_calloc(top_block);

    hc_co_recycle_cofd(cofd);
    if(!cofd_time) return cofd;
    hc_co_close_cofd(cofd_time);
    return cofd_wait;
}

uint32_t  hc_co_wait_fd_timeout(int fd, uint32_t events,int timeout)
{
    HC_CoFd *cofd = hc_co_get_cofd(fd,CO_TYPE_STACKFD, CO_FLAG_ZERO);
    HC_CoFd *cofd_time=NULL, *cofd_wait=NULL;
    if(!cofd) return 0;
    int timefd=0;
    uint64_t top_block = (long)__builtin_frame_address(0);
    top_block = hc_co_context_calloc(0);

    if(1>top_block){
        hc_co_recycle_cofd(cofd);
        return 0;
    }
    cofd->u64 = top_block;

    if(-1==hc_add_fd_to_epoll(fd, cofd, events)){
        hc_co_recycle_cofd(cofd);
        hc_co_memblock_free(NULL,top_block);
        return 0;
    }
    if(0<timeout){
        timefd=hc_get_timerfd(timeout,0);
        if(0>timefd){
            hc_co_recycle_cofd(cofd);
            hc_co_memblock_free(NULL,top_block);
            hc_del_fd_from_epoll(fd);
            return 0;
        }
        cofd_time = hc_co_get_cofd(timefd,CO_TYPE_STACKTIME, CO_FLAG_ZERO);
        if(!cofd_time){
            hc_co_recycle_cofd(cofd);
            hc_co_memblock_free(NULL,top_block);
            hc_del_fd_from_epoll(fd);
            close(timefd);
            return 0;
        }

        if(-1==hc_add_fd_to_epoll(timefd, cofd_time, EPOLLIN)){
            hc_co_recycle_cofd(cofd);
            hc_co_recycle_cofd(cofd_time);
            hc_co_memblock_free(NULL,top_block);
            hc_del_fd_from_epoll(fd);
            close(timefd);
            return 0;
        }
        cofd_time->u64 = top_block;
    }
    cofd_wait = hc_co_context_save_after_calloc(top_block);

    hc_co_recycle_cofd(cofd);
    if(!cofd_time) return cofd->events;
    hc_co_close_cofd(cofd_time);
    if(cofd_wait==cofd_time) return 0;
    return cofd_wait->events;
}

inline int hc_co_write_fd(int fd, void *buf, int len)
{
    int tmp;
    uint32_t ev;
        
    if(-1==len){
        len=strlen((char*)buf);
    }
    for(int i=0;i<len;){
        tmp = len-i;
        tmp = write(fd,buf+i,tmp);
        if(tmp<1){
            if(errno==EAGAIN){
                ev = hc_co_wait_fd_timeout(fd,EPOLLOUT|EPOLLRDHUP, 80);
                if(!ev) return -1;
                if(ev & EPOLLOUT) continue;
                return -1;
            }else{
                return -1;
            }
        }
        i+=tmp;
    }
    return 0;
}

int hc_co_tcp4_connect(int ip,short int port, int timeout)
{
    int cfd=0;
    struct sockaddr_in sa_remote;
    memset(&sa_remote,0,sizeof(sa_remote));
    sa_remote.sin_family=AF_INET;
    sa_remote.sin_addr.s_addr=ip;
    sa_remote.sin_port=port;

    if((cfd=socket(PF_INET,SOCK_STREAM,0))<0){
        perror("socket");
        return -1;
    }
    setnonblocking(cfd);
    int ret = connect(cfd, (struct sockaddr*) &sa_remote, sizeof (struct sockaddr));
    if(ret<0 && errno!=EINPROGRESS){
        return -4;
    }else if(0==ret){
        return cfd;
    }
    uint32_t ev= hc_co_wait_fd_timeout(cfd, EPOLLOUT|EPOLLET|EPOLLRDHUP, timeout);
    if(!ev){ 
        close(cfd); return -2;
    }
    if(ev&EPOLLRDHUP){ 
        close(cfd); return -3;
    }
    return cfd;
}

inline int hc_co_sendto_fd(int fd, void *buf, int len,uint32_t ip, uint16_t port)
{
    int tmp;
    struct sockaddr_in sa_remote;
    memset(&sa_remote,0,sizeof(sa_remote));
    sa_remote.sin_family=AF_INET;
    sa_remote.sin_addr.s_addr=ip;
    sa_remote.sin_port=port;
    uint32_t ev;
        
    if(-1==len){
        len=strlen((char*)buf);
    }
    for(int i=0;i<len;){
        tmp = len-i;
        tmp = sendto(fd,buf+i,tmp,0,(struct sockaddr*)&sa_remote,
                sizeof(sa_remote));
        if(tmp<1){
            if(errno==EAGAIN){
                ev = hc_co_wait_fd_timeout(fd,EPOLLOUT|EPOLLRDHUP, 60);
                if(!ev) return -1;
                if(ev & EPOLLOUT) continue;
                return -1;
            }else{
                return -1;
            }
        }
        i+=tmp;
    }
    return 0;
}

pthread_t hc_create_thread(void *fun, void*arg)
{
    pthread_t pth;
    pthread_attr_t attr = {0};
    pthread_attr_init (&attr);
    pthread_attr_setscope (&attr, PTHREAD_SCOPE_PROCESS);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
    if(0!=pthread_create(&pth, &attr, fun, arg)){
        return 0;
    }
    return pth;
}

int hc_co_timeout_thread(int timeout, void *fun, void *arg1)
{
    HC_CoFd *cofd_time;
    int timefd;
    uint64_t top_block = (long)__builtin_frame_address(0);
    top_block = hc_co_context_calloc(0);

    if(1>top_block){
        return 1;
    }
    timefd=hc_get_timerfd(timeout,0);
    if(0>timefd){
        hc_co_memblock_free(NULL,top_block);
        return 1;
    }
    cofd_time = hc_co_get_cofd(timefd,CO_TYPE_STACKTIME, CO_FLAG_ZERO);
    if(!cofd_time){
        hc_co_memblock_free(NULL,top_block);
        close(timefd);
        return 1;
    }
    cofd_time->ptr2 = arg1;
    cofd_time->u64 = top_block;

    if(-1==hc_add_fd_to_epoll(timefd, cofd_time, EPOLLIN)){
        hc_co_close_cofd(cofd_time);
        return 1;
    }
    pthread_t pth;
    pth=hc_create_thread(fun, cofd_time);
    if(!pth){
        hc_co_close_cofd(cofd_time);
        return 1;
    }
    cofd_time->my_status = 1;
    hc_co_context_save_after_calloc(top_block);

    hc_co_close_cofd(cofd_time);
    top_block = cofd_time->my_status;
    if(1==top_block){
        pthread_cancel(pth);
        return 1;
    }
    return top_block;
}

inline int hc_co_write_cofd_keep_read(HC_CoFd *cofd, void *buf, int len)
{
    int tmp, fd=cofd->fd;
    uint32_t ev;
        
    if(-1==len){
        len=strlen((char*)buf);
    }
    char change=0;
    for(int i=0;i<len;){
        tmp = len-i;
        tmp = write(fd,buf+i,tmp);
        if(tmp<1){
            if(errno==EAGAIN){
                cofd->w_flg = 1;
                ev = hc_co_wait_fd(fd,EPOLLOUT|EPOLLRDHUP);
                cofd->w_flg = 0;
                if(!ev) goto code_error;
                change = 1;
                if(ev & EPOLLOUT){
                    continue;
                }
                goto code_error;
            }else{
                goto code_error;
            }
        }
        i+=tmp;
    }
    if(change && cofd->r_flg){
        hc_cofd_add_to_epoll(cofd, cofd->raw_events);
    }
    return 0;
code_error:
    if(change && cofd->r_flg){
        hc_cofd_add_to_epoll(cofd, cofd->raw_events);
    }
    return -1;
}

uint32_t hc_co_wait_cofd(HC_CoFd *cofd,uint32_t events)
{
    cofd->raw_events = events;
    cofd->co_type = COTYPE_STACK; 
    uint64_t top_block = (long)__builtin_frame_address(0);
    top_block = hc_co_context_calloc(0);

    if(1>top_block){
        return 0;
    }
    cofd->u64 = top_block;

    
    if(!cofd->w_flg && -1==hc_add_fd_to_epoll(cofd->fd, cofd, events)){
        hc_co_memblock_free(NULL,top_block);
    }
    hc_co_context_save_after_calloc(top_block);

    return cofd->events;
}

int hc_co_sleep(int sec, int nsec)
{
    HC_CoFd *cofd_time=NULL;
    int timefd=0;
    uint64_t top_block = (long)__builtin_frame_address(0);

    top_block = hc_co_context_calloc(0);

    if(1>top_block){
        return 1;
    }

    timefd=hc_get_timerfd(sec,nsec);
    if(0>timefd){
        hc_co_memblock_free(NULL,top_block);
        return 2;
    }
    cofd_time = hc_co_get_cofd(timefd,CO_TYPE_STACKTIME, CO_FLAG_ZERO);
    if(!cofd_time){
        hc_co_memblock_free(NULL,top_block);
        close(timefd);
        return 2;
    }

    if(-1==hc_add_fd_to_epoll(timefd, cofd_time, EPOLLIN)){
        hc_co_recycle_cofd(cofd_time);
        hc_co_memblock_free(NULL,top_block);
        close(timefd);
        return 3;
    }
    cofd_time->u64 = top_block;
    hc_co_context_save_after_calloc(top_block);

    hc_co_close_cofd(cofd_time);
    return 0;
}

HC_CoFd* hc_co_timeout_fun(int sec, int nsec, void *fun, void *arg1, void*arg2)
{
    HC_CoFd *cofd_time=NULL;
    int timefd=0;

    timefd=hc_get_timerfd(sec,nsec);
    if(0>timefd){
        return NULL;
    }
    cofd_time = hc_co_get_cofd(timefd,CO_TYPE_FUNTIME, CO_FLAG_ZERO);
    if(!cofd_time){
        close(timefd);
        return NULL;
    }

    if(-1==hc_add_fd_to_epoll(timefd, cofd_time, EPOLLIN)){
        hc_co_close_cofd(cofd_time);
        return NULL;
    }
    cofd_time->fun = fun;
    cofd_time->ptr = arg1;
    cofd_time->ptr2 = arg2;

    return cofd_time;
}

void* hc_co_wait_callback(void *cbptr)
{
    HC_CoCallBack *cb = hc_co_prepare_callback(hc_co_context_resume,NULL,NULL);
    if(!cb){
        *(HC_CoCallBack**)cbptr = NULL;
        return NULL;
    }
    *(uint64_t*)cbptr = (uint64_t)cb;
    void *data = hc_co_context_save(0, (uint64_t*)&(cb->arg1));
    if(NULL==cb->arg1){
        hc_co_cancel_callback(cb);
        *(HC_CoCallBack**)cbptr = NULL;
        return NULL;
    }
    return data;
}
