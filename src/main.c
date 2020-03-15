#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <string.h>
#include <sys/sysinfo.h>

#include "include/hepollc.h"

// 存放配置参数
extern HC_Dict *g_confs;

int main(int argc, char *argv[])
{
    int num=0, ret;
    hc_conf_init_handle(argc, argv);
    int process_num = hc_dict_get_num(g_confs,"process_num",-1,&ret);
    if(0==process_num){
        //如果process_num为0,读取可用处理器数作为进程数
        process_num = get_nprocs();
    }
    hc_printf_green("----  process_num:%d\n",process_num);
    char *ptmp;
    if(1<process_num){
        hc_fork(process_num);
    }
    
    uint64_t u64;
    //主ioloop内最大支持的回调数和协程栈可用的内存池大小
    num = hc_dict_get_num(g_confs,"max_callback",-1,&ret);
    u64 = hc_dict_get_num(g_confs,"max_mempool",-1,&ret);
    hc_printf_once_green("----  max_callback num:%d, max_mempool:%fMB\n",num,((float)u64)/1024/1024);
    HC_CoLoop *co_loop = hc_co_ioloop_create(num,u64);
    if(!co_loop){
        hc_printf_error("failed to create ioloop, exit!\n");
        return 1;
    }

    int port = hc_dict_get_num(g_confs,"port",4,&ret);
    ptmp = hc_dict_get_ptr(g_confs,"address",-1,&ret);
    int fd=-1;
    num = hc_dict_get_num(g_confs,"reuseport",-1,&ret);
    for(;;){
        if(num==1){
            //通过reuserport参数启用SO_REUSEPORT
            fd = hc_socket_listen_reuseport(ptmp, port, 10000);
        }else
            fd = hc_socket_listen(ptmp, port, 10000);
        if(-2==fd){
            //如果端口已经被其他应用绑定，port+1继续尝试
            port++;
            hc_printf_warn("port+1\n");
            continue;
        }
        if(-1<fd) break;
        else return -1;
    }
    //将初始化函数加入ioloop，ioloop运行的时候即可调用，函数可接收2个参数
    hc_co_add_callback(hc_init_all,0,0);
    
    //创建http服务器
    HC_HttpServer *srv = hc_http_srv_create(fd);
    hc_printf_warn("----  http bind %s:%d\n",ptmp,port);
    ptmp = hc_dict_get_ptr(g_confs,"app_file",-1,&ret);
    //根据app配置文件加载app到http服务器
    hc_co_add_callback(hc_load_app,srv,ptmp);

    //创建cofd加入到epoll中
    HC_CoFd *cofd=calloc(sizeof(HC_CoFd),1);
    cofd->fd = fd;
    cofd->status = COFD_USING;
    //设置cofd回调类型为函数，fd类型为socket
    cofd->_type = CO_TYPE_FUNFD;
    //将http服务器的listen处理放到cofd中，有请求进来就会调用listen函数
    cofd->fun = srv->fun;
    //标记该cofd为永不自动从epoll中删除
    cofd->flag = CO_FLAG_1ZERO;
    //cofd有个void* ptr可以存放任何指针
    cofd->ptr = srv;
    //将cofd加入epoll
    hc_add_fd_to_epoll(fd,cofd,EPOLLIN);

    ptmp = hc_dict_get_ptr(g_confs,"server_mode",-1,&ret);
    hc_printf_once_green("----  server_mode:%s\n",ptmp);

    //开始ioloop
    hc_co_ioloop_start();
    return 0;
}
