#pragma once
#ifndef pthread_t
    #include <pthread.h>
#endif
#include "co_ioloop.h"

int hc_socket_listen(char *addr,int port,int backlog);
int hc_socket_listen_reuseport(char *addr,int port,int backlog);
int setnonblocking(int sockfd);
int hc_accept_fd(int server_fd);
int hc_co_read_fd_realloc(int fd,HC_MemBuf *mem,int read_max);
int hc_co_read_fd_realloc_timeout(int fd, HC_MemBuf *mem,int read_max,int timeout);
int hc_co_read_fd_until(int fd, HC_MemBuf *mem,int until_top);
int hc_co_read_cofd_until(HC_CoFd *cofd, HC_MemBuf *mem,int until_top);
int hc_co_read_fd_timeout_until(int fd, HC_MemBuf *mem,int timeout,int until_top);
int hc_co_read_fd_to_buf_timeout(int fd, HC_MemBuf *mem, int timeout);
int hc_co_read_fd_to_buf(int fd,HC_MemBuf *mem);

uint64_t hc_co_create_wait();
void* hc_co_wait(uint64_t top_block);

HC_CoFd* hc_co_wait_fd_cofd(int fd,uint32_t events);
uint32_t hc_co_wait_fd(int fd,uint32_t events);
HC_CoFd* hc_co_wait_fd_timeout_cofd(int fd, uint32_t events,int timeout);
uint32_t  hc_co_wait_fd_timeout(int fd, uint32_t events,int timeout);
int hc_co_write_fd(int fd, void *buf, int len);
int hc_co_tcp4_connect(int ip,short int port, int timeout);
int hc_co_sendto_fd(int fd, void *buf, int len,uint32_t ip, uint16_t port);
pthread_t hc_create_thread(void *fun, void*arg);
int hc_co_timeout_thread(int timeout, void *fun, void *arg1);
int hc_co_write_cofd_keep_read(HC_CoFd *cofd, void *buf, int len);
uint32_t hc_co_wait_cofd(HC_CoFd *cofd,uint32_t events);
int hc_co_sleep(int sec, int nsec);
HC_CoFd* hc_co_timeout_fun(int sec, int nsec, void *fun, void *arg1, void*arg2);
void* hc_co_wait_callback(void *cbptr);
