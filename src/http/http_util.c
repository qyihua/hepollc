#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../util/hc_util.h"
#include "http.h"

extern char *g_hex_to_char;

int hc_http_finish_push(HC_HttpRequest *hr, char *send_data,int data_len);
int hc_http_finish_start(HC_HttpRequest *hr, int data_len)
{
    if(hr->finish||0!=hr->srv_buf_len){
        return 0;
    }
    int fd =hr->fd;
    hc_httpsrv_gen_header(hr, data_len);
    hc_co_write_fd(fd, hr->srv_buf, hr->srv_buf_len);
    hr->finish_len = data_len;
    return 0;
}

int hc_http_finish_push(HC_HttpRequest *hr, char *send_data,int data_len)
{
    if(hr->finish) return 0;
    int fd =hr->fd;
    if(-1==data_len) data_len = strlen(send_data);
    if(data_len>=hr->finish_len){
        data_len=hr->finish_len;
        hr->finish=1;
    }else{
        hr->finish_len -= data_len;
    }
    hc_co_write_fd(fd, send_data, data_len);
    return 0;
}

int hc_http_finish(HC_HttpRequest *hr, char *send_data, int data_len)
{
    if(hr->finish) return 0;
    hr->finish = 1;
    int fd =hr->fd, ret=0;
    if(-1==data_len) data_len = strlen(send_data);
    if(0==hr->srv_buf_len){
        hc_httpsrv_gen_header(hr, data_len);
        hc_co_write_fd(fd, hr->srv_buf, hr->srv_buf_len);
    }
    if(HTTP_HEAD == hr->remote.method) return 0;
    if(0<data_len){
        if(hr->srv_tran_enc){
            char tmp_buf[20];
            char *ptmp = hc_num_to_str(data_len,16,&ret,tmp_buf+17);
            memcpy(ptmp+ret,"\r\n",2);
            hc_co_write_fd(fd, ptmp, 2+ret);
            hc_co_write_fd(fd, send_data, data_len);
            hc_co_write_fd(fd, "\r\n0\r\n\r\n", 7);
        }else
            hc_co_write_fd(fd, send_data, data_len);
    }else if(hr->srv_tran_enc){
        hc_co_write_fd(fd, "0\r\n\r\n", 5);
    }
    return 0;
}

int hc_http_write(HC_HttpRequest *hr, char *send_data, int data_len)
{
    if(hr->finish) return 0;
    int fd =hr->fd, ret=0;
    hr->srv_tran_enc = 1;
    if(-1==data_len) data_len = strlen(send_data);
    if(0==hr->srv_buf_len){
        hc_httpsrv_gen_header(hr, data_len);
        hc_co_write_fd(fd, hr->srv_buf, hr->srv_buf_len);
    }
    if(HTTP_HEAD == hr->remote.method) return 0;
    if(0<data_len){
        char tmp_buf[20];
        char *ptmp = hc_num_to_str(data_len,16,&ret,tmp_buf+17);
        memcpy(ptmp+ret,"\r\n",2);
        hc_co_write_fd(fd, ptmp, 2+ret);
        hc_co_write_fd(fd, send_data, data_len);
        hc_co_write_fd(fd, ptmp+ret, 2);
    }else{
        hc_co_write_fd(fd, "0\r\n\r\n", 5);
        hr->finish = 1;
    }
    return 0;
}

int hc_http_set_cookie(HC_HttpRequest *hr, char *key,int key_len,
        char *val,int val_len)
{
    if(!hr->srv_cookies){
        hr->srv_cookies = hc_array_create(12);
    }
    if(0>key_len) key_len = strlen(key);
    if(0>val_len) val_len = strlen(val);
    HC_Array *arr = hr->srv_cookies;
    HC_ArrayData *adata;
    int index = hc_array_append_item(arr,NULL, key_len+val_len+1,TYPE_RAW);
    int top = arr->data_indexs[index];
    adata = (HC_ArrayData*)(arr->data_buf+top);
    memcpy(adata->data, key, key_len);
    top=key_len;
    adata->data[top++] = '=';
    memcpy(adata->data+top, val, val_len);
    top+=val_len;
    adata->len = top;
    return index;
}

int hc_http_clear_cookie(HC_HttpRequest *hr,const char* key,int key_len,char *cookie_info, int info_len)
{
    if(!hr->srv_cookies){
        hr->srv_cookies = hc_array_create(12);
    }
    if(0>key_len) key_len = strlen(key);
    if(info_len==-1) info_len = strlen(cookie_info);
    HC_Array *arr = hr->srv_cookies;
    HC_ArrayData *adata;
    int index = hc_array_append_item(arr,NULL, key_len+46+info_len,TYPE_RAW);
    int top = arr->data_indexs[index];
    adata = (HC_ArrayData*)(arr->data_buf+top);
    memcpy(adata->data, key, key_len);
    top=key_len;
    memcpy(adata->data+top,"=;expires=Thu, 01 Jan 1970 00:00:00 GMT;",40);
    top+=40;
    if(0<info_len){
        memcpy(adata->data+top, cookie_info, info_len);
        top+=info_len;
    }
    adata->len = top;
    return index;
}

int hc_http_redirect(HC_HttpRequest *hr, int code,char *url,
        int url_len)
{
    if(301>code) code=302;
    if(0 != hr->srv_buf_len) return 0;
    hr->srv_code = code;
    hc_http_set_header(hr, "Location", 8, url, url_len);
    hc_http_finish(hr, NULL, 0);
    return 0;
}


char g_http_urichar[]="!*'();:@&=+$,/?#[]";

char *hc_http_encodeuri(char *raw, int raw_len,int *ret)
{
    int data_max=raw_len, data_top=0;
    char *data=malloc(data_max);
    char c;
    char need_encode=0;
    char *hex_str=g_hex_to_char;
    for(int i=0;i<raw_len;i++){
        c = raw[i];
        if(isprint(c)){
            switch(c){
                case '!':
                case '*':
                case '\'':
                case '(':
                case ')':
                case ';':
                case ':':
                case '@':
                case '&':
                case '=':
                case '+':
                case '$':
                case ',':
                case '?':
                case '#':
                case '[':
                case ']':
                    need_encode=3;
                    break;
                default:
                    need_encode=0;
            }
        }else
            need_encode = 3;
        if(!need_encode){
            data[data_top++] = c;
        }
        else{
            hc_check_buf_space(&data,data_top,&data_max, need_encode);
            data[data_top++]='%';
            data[data_top++]=hex_str[c>>4];
            data[data_top++]=hex_str[0b1111&c];
        }
    }
    *ret=data_top;
    return data;
}

int hc_http_encodeuri_realloc(char *raw, int raw_len, HC_MemBuf *mem)
{
    char *data;
    uint8_t c;
    char char_len=1;
    int len = mem->len;
    char *hex_str=g_hex_to_char;
    for(int i=0;i<raw_len;i++){
        c = raw[i];
        if(isprint(c)){
            switch(c){
                case '!':
                case '*':
                case '\'':
                case '(':
                case ')':
                case ';':
                case ':':
                case '@':
                case '&':
                case '=':
                case '+':
                case '$':
                case ',':
                case '?':
                case '#':
                case '[':
                case ']':
                    char_len=3;
                    break;
                default:
                    char_len=1;
            }
        }else
            char_len = 3;
        data = hc_membuf_check_size(mem, char_len);
        if(1==char_len){
            data[(mem->len)++] = c;
        }
        else{
            data[(mem->len)++]='%';
            data[(mem->len)++]=hex_str[c>>4];
            data[(mem->len)++]=hex_str[0b1111&c];
        }
    }
    len=mem->len-len;
    return len;
}
