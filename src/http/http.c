#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "../com/g_data.h"
#include "util/hc_util.h"

#include "http.h"

extern char g_tmp_buf[MAX_BUF_SIZE];

HC_Dict *g_httpsrv_c_cookie=NULL;
HC_Dict *g_httpsrv_s_header=NULL;
HC_Dict *g_httpsrv_codes=NULL;
HC_Dict *g_http_methods=NULL;
extern HC_Dict *g_confs;

extern char g_char_to_hex[55];
extern char *g_hex_to_char;

int hc_http_free(HC_HttpRequest *hr)
{
    HC_HttpRequestRemote *hhc=&hr->remote;
    if(hr->srv_headers){
        free(hhc->raw_buf.buf);
        free(hr->srv_buf);
        hc_dict_free(hr->srv_headers);
        hc_dict_free(hr->srv_codes);
        hc_dict_free(hhc->header_indexs);
        hc_dict_free(hhc->cookie_indexs);
        hc_dict_free(hhc->param_dict);
        hc_array_free(hhc->decode_datas);
        hc_array_free(hhc->path_arr);
        hc_array_free(hhc->param_arr);
        if(hr->srv_cookies){
            hc_array_free(hr->srv_cookies);
        }
    }
    if(hr->tmp_buf.buf)
        free(hr->tmp_buf.buf);
    free(hr);
    return 0;
}

HC_HttpRequest* hc_http_malloc(int fd)
{
    struct sockaddr_in peer_addr;
    uint32_t tmp=0;
    tmp = sizeof(peer_addr);
    if(0!=getpeername(fd, (struct sockaddr *)&peer_addr, &tmp)){
        return NULL;
    }
    HC_HttpRequest *hr = calloc(1,sizeof(HC_HttpRequest));
    hr->fd = fd;
    hr->remote_ipv4 = peer_addr.sin_addr.s_addr;
    return hr;
}

int hc_http_reset_srv(HC_HttpRequest *hr)
{
    HC_HttpRequestRemote *hhc=&hr->remote;
    memset(hhc, 0, 32);
    hhc->raw_buf.len = 0;
    hhc->header_len = 0;
    hhc->body_len = -1;
    hhc->path_param_index = -1;
    hr->srv_buf_len = 0;
    hr->srv_code = 200;
    hr->finish = 0;
    hr->srv_tran_enc = 0;
    gettimeofday(&hr->start_time,NULL);
    if(hr->srv_cookies){
        hc_array_free(hr->srv_cookies);
        hr->srv_cookies = NULL;
    }
    if(hr->tmp_buf.buf){
        free(hr->tmp_buf.buf);
        hr->tmp_buf.buf = NULL;
    }
    if(!hr->srv_headers){
        hc_membuf_init(&hhc->raw_buf, 30);
        hr->srv_buf = malloc(52);
        hr->srv_buf_max = 52;
        hr->srv_headers = hc_dict_copy(g_httpsrv_s_header);
        hr->srv_codes = hc_dict_copy(g_httpsrv_codes);
        hhc->header_indexs = hc_dict_create(TYPE_NUM,16);
        hhc->cookie_indexs = hc_dict_create(TYPE_NUM,32);
        hhc->param_dict= hc_dict_create(TYPE_RAW,32);
        hhc->decode_datas = hc_array_create(16);
        hhc->path_arr = hc_array_create(50);
        hhc->param_arr = hc_array_create(10);
    }else{
        hc_dict_copy_to_dst(g_httpsrv_s_header, hr->srv_headers);
        hc_dict_copy_to_dst(g_httpsrv_codes, hr->srv_codes);
        hc_dict_reset_to_zero(hhc->header_indexs);
        hc_dict_reset_to_zero(hhc->cookie_indexs);
        hc_dict_reset_to_zero(hhc->param_dict);
        hc_array_reset_to_zero(hhc->path_arr);
        hc_array_reset_to_zero(hhc->param_arr);
        hc_array_reset_to_zero(hhc->decode_datas);
    }
        
    return 0;
}

int hc_http_handle(HC_CoFd *cofd, uint32_t events){
    HC_HttpRequest *hr = cofd->ptr;
    hc_co_close_cofd(cofd->ptr2);
    if( EPOLLRDHUP&events ){
        //printf("client close...\n");
        hc_co_close_cofd(cofd);
        hc_http_free(hr);
        return 0;
    }
    if( CO_FDTYPE_TIME==cofd->fd_type ){
        //printf("client timeout...cofd:%x \n",cofd);
        hc_co_close_cofd(cofd);
        hc_http_free(hr);
        return 0;
    }
    int ret = 0, fd =cofd->fd;
    hc_http_reset_srv(hr);
    HC_HttpRequestRemote *hhc=&hr->remote;
    HC_MemBuf *mem = &hhc->raw_buf;
    for(ret=1;;){
        ret = hc_co_read_fd_realloc_timeout(fd,mem, HTTP_MAX_SIZE,60);
        if(1>ret){
            hr->srv_code = 408;
            break;
        }
        if(mem->event&EPOLLRDHUP){
            hc_co_close_cofd(cofd);
            hc_http_free(hr);
            return 0;
        }
        if(20>hhc->status){
            ret = hc_http_quick_check(hr);
            if(0>ret){
                if(-1==ret) hr->srv_code = 400;
                break;
            }
            if(1==ret) continue;
        }
        ret = hc_http_check_body(hr);
        if(0>ret){hr->srv_code = 400;break;}
        if(1==ret) continue;
        ret = hc_http_remote_get_data(hr);
        break;
    }
    if(0!=ret){
        goto code_error;
    }
    ret = hc_http_url_handle(hr);
    if(0>ret){
code_error:
        hc_http_set_header(hr, "Connection", 10, "close", 5);
        hc_http_end(hr);
        cofd->ptr=NULL;
        hc_co_close_cofd(cofd);
        hc_http_free(hr);
        return 0;
    }
    hc_http_end(hr);
    if(hr->keep_alive){
        hc_cofd_add_to_epoll_timeout(cofd, EPOLLIN| EPOLLET | EPOLLRDHUP, 10);
    }else{
        cofd->ptr=NULL;
        if(hr->keep_fd){
            hc_co_recycle_cofd(cofd);
        }else
            hc_co_close_cofd(cofd);
        hc_http_free(hr);
    }
    return 0;
}


int hc_http_init_handle()
{

    g_httpsrv_s_header = hc_dict_create(TYPE_RAW,32);
    hc_dict_set_item(g_httpsrv_s_header,"Content-Type",-1,"text/html; charset=utf-8",-1,TYPE_RAW);
    hc_dict_set_item(g_httpsrv_s_header,"Connection",-1,"keep-alive",-1,TYPE_RAW);
    hc_dict_set_item(g_httpsrv_s_header,"Server",-1,"HepollC Server",-1,TYPE_RAW);
    
    g_httpsrv_codes = hc_dict_create(TYPE_RAW,32);
    hc_dict_set_item_by_num(g_httpsrv_codes,200,2,"OK",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,304,2,"Not Modif",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,301,2,"Moved Permanently",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,302,2,"Found",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,307,2,"Temporary Redirect",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,400,2,"Bad Request",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,403,2,"Forbidden",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,404,2,"Not Found",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,408,2,"Request Time-out",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,411,2,"Length Require",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,413,2,"Request Entity Too Large",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,431,2,"Request Header Fields Too Large",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,501,2,"Not Implemented",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,500,2,"Internal Error",-1,TYPE_RAW);
    hc_dict_set_item_by_num(g_httpsrv_codes,101,2,"Switching Protocols",-1,TYPE_RAW);

    g_http_methods = hc_dict_create(TYPE_NUM,32);
    hc_dict_set_num(g_http_methods,"get",-1, HTTP_GET,0);
    hc_dict_set_num(g_http_methods,"post",-1, HTTP_POST,0);
    hc_dict_set_num(g_http_methods,"put",-1, HTTP_PUT,0);
    hc_dict_set_num(g_http_methods,"delete",-1, HTTP_DELETE,0);
    hc_dict_set_num(g_http_methods,"head",-1, HTTP_HEAD,0);
    hc_dict_set_num(g_http_methods,"options",-1, HTTP_OPTIONS,0);
    return 0;
}


int hc_http_listen_handle(HC_CoFd *cofd, uint32_t events)
{
    
    int cfd = hc_accept_fd(cofd->fd);
    if(-1==cfd) return -1;


    HC_CoFd *cofd_client = hc_co_get_cofd(cfd,CO_TYPE_FUNFD,CO_FLAG_ZERO);
    if(!cofd_client){
        close(cfd);
        return -1;
    }
    HC_HttpRequest *hr = hc_http_malloc(cfd);
    cofd_client->ptr = hr;
    cofd_client->fun = hc_http_handle;

    HC_HttpServer *srv = cofd->ptr;
    hr->srv = srv;
    cofd_client = hc_cofd_add_to_epoll_timeout(cofd_client, EPOLLIN| EPOLLET | EPOLLRDHUP, 60);
    return 0;
}

int hc_http_end(HC_HttpRequest *hr)
{
    char *ptmp=NULL;
    char *tmp_buf = g_tmp_buf;
    HC_HttpRequestRemote *hhc=&(hr->remote);
    int ret=0, top=0;
    struct  timeval end_time;
    gettimeofday(&end_time,NULL);
    if(!hr->finish){
        hc_http_finish(hr,NULL, 0);
    }
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    printf("%04d-%02d-%02d_%02d:%02d:%02d ",timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    HC_Array *arr = hhc->decode_datas;
    ptmp = hc_array_get_item_retptr(arr, HTTP_INDEX_REALIP, &ret);
    memcpy(tmp_buf, ptmp, ret);
    top=ret;
    tmp_buf[top++]=' ';
    ptmp = hc_num_to_str(hr->srv_code, 10, &ret,NULL);
    memcpy(tmp_buf+top, ptmp, ret);
    top+=ret;
    tmp_buf[top++]=' ';
    memcpy(tmp_buf+top, hhc->raw_buf.buf, hhc->method_len+1);
    top = top + hhc->method_len + 1;

    ret = hhc->path_len + 1;
    if(hhc->url_query_len)
        ret = ret + hhc->url_query_len + 1;
    if(1000<ret) ret=1000;
    memcpy(tmp_buf+top, hhc->raw_buf.buf+hhc->path_index, ret);
    top+=ret;
    tmp_buf[top]='\0';
    int sec = end_time.tv_sec - hr->start_time.tv_sec;
    int usec = end_time.tv_usec - hr->start_time.tv_usec;
    if(0>usec){sec-=1;usec = 0-usec;}
    printf("%s %d.%06ds\n",tmp_buf, sec, usec);
    return 0;
}

HC_HttpServer* hc_http_srv_create(int fd)
{
    HC_HttpServer *http_srv = calloc(sizeof(HC_HttpServer),1);
    http_srv->fd = fd;
    http_srv->app = hc_dict_create(TYPE_NUM,32);
    http_srv->app_lib = hc_dict_create(TYPE_NUM,32);
    http_srv->url = hc_dict_create(TYPE_NUM,32);
    http_srv->fun = hc_http_listen_handle;
    return http_srv;
}

void hc_http_srv_free(HC_HttpServer *srv)
{
    hc_dict_free(srv->app);
    hc_dict_free(srv->app_lib);
    hc_dict_free(srv->url);
    free(srv);
}

int hc_http_srv_set_app(HC_HttpServer *http_srv,char *name, int name_len,void *fun)
{
    hc_dict_set_num(http_srv->app, name, name_len, (uint64_t)fun, 8);
    return 0;
}

int hc_http_srv_set_url(HC_HttpServer *http_srv,char *url, int url_len,void *fun)
{
    hc_dict_set_num(http_srv->url, url, url_len, (uint64_t)fun, 8);
    return 0;
}

void* hc_http_url_get_handle(HC_HttpRequest *hr)
{
    int len,url_len;
    HC_HttpRequestRemote *hrr=&hr->remote;
    HC_HttpServer *srv = hr->srv;
    char *app = hc_array_get_item_retptr(hrr->path_arr,0,&len);
    char *url = hc_array_get_item_retptr(hrr->decode_datas,HTTP_INDEX_PATH,&url_len);

    uint64_t fun;
    int ret;
    fun = hc_dict_get_num(srv->url, url, url_len, &ret);
    if(8==ret) return (void*)fun;
    if(len){
        fun = hc_dict_get_num(srv->app, app, len, &ret);
        if(8==ret){
            return ((void* (*)(char*,int) )fun)(url+hrr->app_path_index, 
                    hrr->app_path_len);
        }
    }
    return NULL;
}

int hc_http_url_handle(HC_HttpRequest *hr)
{
    void *fun = hc_http_url_get_handle(hr);
    if(!fun){
        hr->srv_code = 404;
        return 0;
    }
    return ((int (*)(HC_HttpRequest *))fun)(hr);
}

HC_MemBuf* hc_http_get_tmpbuf(HC_HttpRequest *hr, uint32_t len)
{
    HC_MemBuf *mem = &hr->tmp_buf;
    void *ptr;
    if(!mem->buf){
        if(0==len) len = 1024;
        ptr = malloc(len);
        if(!ptr) return NULL;
        mem->buf = ptr;
        mem->max = len;
    }else if(len > mem->max){
        ptr = realloc(mem->buf, len);
        if(!ptr) return NULL;
        mem->max = len;
        mem->buf = ptr;
    }
    mem->len = 0;
    return mem;
}
