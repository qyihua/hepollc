#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <time.h>
#include <sys/timerfd.h>
#include <ctype.h>
#include <fcntl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "util/hc_util.h"

#include "http.h"
#include "http_client.h"


extern char g_char_to_hex[55];

SSL_CTX *g_httpc_tls_context;
int sslWrite(SSL* sslHandle,char *buf,int len);
int sslRead(SSL* sslHandle);


static char g_httpc_host_char[78]={ //-.:0-9 a-z, A-Z -45
    1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1,\
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,\
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};


HC_Dict *g_dns_dict=NULL;
HC_Dict *g_httpc_c_headers=NULL;
extern HC_Dict *g_confs;
uint32_t g_dns_srv_ip4=0;

static int hc_httpc_check_redirect(HC_HttpClient *hc, int *method);
int hc_httpc_init_handle(int a)
{

    g_httpc_c_headers = hc_dict_create(TYPE_RAW,32);
    hc_dict_set_item(g_httpc_c_headers, "Connection", -1, "keep-alive", -1,TYPE_RAW);
    hc_dict_set_item(g_httpc_c_headers, "Pragma", -1, "no-cache", -1,TYPE_RAW);
    hc_dict_set_item(g_httpc_c_headers, "User-Agent", -1, "Mozilla/5.0 (X11; Linux x86_64)", -1,TYPE_RAW);
    SSL_load_error_strings ();
    g_httpc_tls_context = SSL_CTX_new(TLS_client_method());//SSLv23_method());

    g_dns_dict = hc_dict_create(TYPE_NUM,100);

    int ret;
    char *ptmp = hc_dict_get_ptr(g_confs,"dns_server",-1,&ret);
    if(ptmp){
        g_dns_srv_ip4 = inet_addr(ptmp);
        hc_printf_once_warn("----  dns server:%s\n",ptmp);
        if(INADDR_NONE==g_dns_srv_ip4){
            g_dns_srv_ip4 = inet_addr("8.8.8.8");
        }
    }else{
        hc_printf_once_warn("----  dns server: 8.8.8.8\n");
        g_dns_srv_ip4 = inet_addr("8.8.8.8");
    }
    HC_Array *hosts = hc_dict_get_array(g_confs,"dns_hosts[]",-1);
    if(!hosts) return 0;
    for(int i=0;i<hosts->item_num;i++){
        char *str = hc_array_get_item_retptr(hosts,i,&ret);
        HC_Array *host_arr = hc_split_trim(NULL, str, ret, ',',0,' ');
        if(host_arr->item_num==2){
            int key_len;
            char *key = hc_array_get_item_retptr(host_arr,0,&key_len);
            char *val = hc_array_get_item_retptr(host_arr,1,&ret);
            uint32_t ip4 = inet_addr(val);
            hc_printf_once_warn("      dns host:%s,val:%s\n",key,val);
            if(INADDR_NONE!=ip4){
                hc_dict_set_num(g_dns_dict, key,key_len, 
                        ip4, 8);
            }
        }
        hc_array_free(host_arr);
    }
    hc_array_free(hosts);
    hosts=NULL;
    hc_dict_set_item(g_confs, "dns_hosts[]", -1, hosts, 8, TYPE_LIST);

    return 0;
}

uint32_t hc_dns_get_ip4(char*name, char len)
{
    static uint16_t trans_id = 0;
    int cfd=0;
    int ret=0,top=0,tmp=0;
    int i=0;
    int ttl=0;
    uint32_t ip=0;
    char c;
    unsigned long int data;
    if(1>len) len=strlen(name);
    if(1>len) return 0;
    if('.'==name[0]) return 0;

    if(!g_dns_dict) g_dns_dict = hc_dict_create(TYPE_NUM,100);
    if(0<(data=hc_dict_get_num(g_dns_dict,name,len,&ret))){
        ttl = data>>32;
        if(0==ttl) return data;
        ret = time((time_t*)NULL);
        if(ret-ttl<3600){
            return data;
        }
    }
    data = 0;
    char labels[4];
    uint8_t lnum=0;
    tmp = 0;
    int tmp2=0;
    for(i=0;i<len&&lnum<4;i++){
        c = name[i];
        if('.'==c){
            tmp = i - tmp;
            if(0==tmp) return 0;
            labels[lnum++] = tmp;
            tmp = i+1;
            if(tmp2<256){
                ip<<=8;
                ip|=(uint8_t)tmp2;
                tmp2=0;
            }
            continue;
        }
        if('0'<=c&&'9'>=c&&tmp2<256){
            tmp2 = tmp2*10 + c - '0';
        }else if(tmp2!=256){
            tmp2 = 256;
        }
        c-=45;
        if(0>c||77<c) return 0;
        if(0==g_httpc_host_char[c]) return 0;
    }

    if(i==len){
        if(tmp!=i){
            tmp = i - tmp;
            labels[lnum++] = tmp;
            if(lnum==4&&tmp2<256){
                ip<<=8;
                ip|=tmp2;
                tmp2=0;
                return htonl(ip);
            }
        }
    }
    int src_top = i;

    HC_MemBuf mem;
    hc_membuf_init(&mem, MAX_DNS_BUF);
    char *buf = mem.buf;
    HC_DnsHeader *dh = (HC_DnsHeader*)buf;
    memset(dh,0,sizeof(HC_DnsHeader));
    dh->trans_id = trans_id++;
    dh->rd = 1;
    dh->questions = 1;
    top = 12;
    tmp2 = 0;
    for(i=0;i<lnum;i++){
        tmp = labels[i];
        buf[top++] = tmp;
        memcpy(buf+top, name+tmp2, tmp);
        tmp2 = tmp2 + tmp + 1;
        top+=tmp;
    }
    tmp2 = top;
    for(i=src_top;i<len;i++){
        c = name[i];
        if('.'==c){
            tmp = top - tmp2 -1;
            if(0==tmp) goto error_code;
            buf[tmp2] = tmp;
            tmp2 = top;
            continue;
        }
        buf[top++]=c;
        c-=45;
        if(0>c||77<c) goto error_code;
        if(0==g_httpc_host_char[c]) goto error_code;
    }
    if(tmp2!=top){
        tmp = top - tmp2 - 1;
        buf[tmp2] = tmp;
    }
    memcpy(buf+top,"\x0\x0\x1\x0\x1",5);
    top+=5;
    

    if((cfd=socket(PF_INET,SOCK_DGRAM,0))<0){
        perror("socket");
        goto error_code;
    }
    setnonblocking(cfd);
    ret = hc_co_sendto_fd(cfd, buf, top, g_dns_srv_ip4, htons(53));
    if(-1==ret) goto error_code;
    top=0;
    mem.len = 0;
    ret = hc_co_read_fd_timeout_until(cfd, &mem, 20, 12);
    if(0>ret) goto error_code;
    if(0!=dh->rcode) goto error_code;
    top = 18 + len; // 12 + 6
    ret = hc_co_read_fd_timeout_until(cfd, &mem, 20, top);
    
    if(0>ret) goto error_code;
    uint16_t u16 = ntohs(*(short*)(buf+top));
    for(int i=0;i<dh->answer_rrs;i++){
        ret = hc_co_read_fd_timeout_until(cfd, &mem,20, top+16);
        if(0>ret) goto error_code;
        u16 = ntohs(*(short*)(buf+top));
        if((0xc000&u16) != 0xc000) goto error_code;
        top+=2;
        u16 = ntohs(*(short*)(buf+top)); //type
        if(1!=u16){
            top += 8;
            u16 = ntohs(*(short*)(buf+top));
            if(256<u16) goto error_code;
            top = top + 2 + u16;
            continue;
        }
        if(1!=u16) goto error_code;
        top+=4;
        ttl = ntohl(*(int*)(buf+top));
        top+=4;
        u16 = ntohs(*(short*)(buf+top));
        if(4!=u16) goto error_code;
        top+=2;
        ip = *(int*)(buf+top);
        free(mem.buf);
        close(cfd);
        data = time((time_t*)NULL);
        data<<=32;
        data|=ip;
        hc_dict_set_num(g_dns_dict, name, len, data, 8);
        return ip;
    }
    goto error_code;

error_code:
    close(cfd);
    free(mem.buf);
    return 0;
}


int hc_http_client_parse(HC_HttpClient *hc, char *raw_url,int url_len)
{
    char c_cur,status, no_change=0;
    int top,i_start,i_len,tmp;
    
    if(1>url_len) url_len=strlen(raw_url);

    if(!hc->raw_url){ hc->raw_url=malloc(url_len); memcpy(hc->raw_url,raw_url,url_len);}
    else{
        if(!(!hc_bin_casecmp("http:",5,raw_url,5)||!hc_bin_casecmp("https:",6,raw_url,6))){
            tmp = url_len + hc->host_index + hc->host_len;
            if('/'!=raw_url[0]) tmp++;
            if(hc->raw_url_len< tmp){
                hc->raw_url = realloc(hc->raw_url,tmp);
            }
            tmp-=url_len;
            if('/'!=raw_url[0]){
                hc->raw_url[tmp++]='/';
            }
            memcpy(hc->raw_url+tmp,raw_url,url_len);
            url_len+=tmp;
            no_change = 1;
        }else{
            if(hc->raw_url_len< url_len){
                hc->raw_url = realloc(hc->raw_url,url_len);
            }
            memcpy(hc->raw_url,raw_url,url_len);
        }
    }
    hc->raw_url_len = url_len;
    if(no_change){
        hc->req_url_len = url_len-hc->req_url_index;
        return 0;
    }
    no_change = 0;

    raw_url = hc->raw_url;

    if(0!=hc_bin_casecmp("http",4,raw_url,4)) return -1;
    if('s'==raw_url[4]||'S'==raw_url[4]){
        top=8;
        hc->ssl_flg=1;
    }
    else{
        top=7;
        hc->ssl_flg=0;
    }
    hc->host_index = top;
    tmp = 0;
    status = 0;
    for(i_start=top;top<url_len;top++){
        c_cur = raw_url[top];
        switch(status){
            case 0:

                if('/'==c_cur){
code_end_noport:
                    tmp = top-i_start;
                    if(3>tmp||125<tmp) return -1;
                    hc->domain_len = tmp;
                    hc->host_len = tmp; 
                    if(hc->ssl_flg){
                        tmp = htons(443);
                    }
                    else{
                        tmp = htons(80);
                    }
                    if(tmp==hc->port){
                        no_change = 1;
                    }else
                        hc->port = tmp;
                    goto code_for_break;
                }
                if(':'==c_cur){
                    tmp = top-i_start;
                    if(3>tmp||125<tmp) return -1;
                    hc->domain_len = tmp;
                    tmp = 0;
                    status = 1;
                    i_start = top+1;
                    break;
                }
                tmp = c_cur-45;
                if(0>tmp||77<tmp) return -1;
                if(0==g_httpc_host_char[tmp]) return -1;
                break;
            case 1:
                if('/'==c_cur){
code_end_port:
                    hc->host_len = top - i_start + hc->domain_len + 1;
                    if(1>tmp||65535<tmp)
                        return -1;
                    tmp = htons(tmp);
                    if(hc->port==tmp){
                        no_change = 1;
                    }else
                        hc->port = tmp;
                    goto code_for_break;
                }
                if('0'>c_cur||'9'<c_cur) return -1;
                tmp = tmp*10 + c_cur-'0';
                continue;
        }
    }
    if(0==status){
        goto code_end_noport;
    }
    goto code_end_noport;

code_for_break:
    if(top == url_len){
        hc->req_url_index = top-1;
        hc->req_url_len = 0;
    }
    else{
        hc->req_url_index = top;
        hc->req_url_len = url_len-top;
    }
    tmp = hc_dns_get_ip4(raw_url+hc->host_index, hc->domain_len);
    if(0==tmp){
        if(-1<hc->fd){
            close(hc->fd);
            hc->fd=-1;
            if(hc->ssl_handle){
                SSL_free(hc->ssl_handle);
                hc->ssl_handle=NULL;
            }
        }
        hc->ip=0;
        return -1;
    }
    if(-1<hc->fd){
        if(!no_change || hc->ip!=tmp){
            close(hc->fd);
            hc->fd=-1;
            if(hc->ssl_handle){
                SSL_free(hc->ssl_handle);
                hc->ssl_handle=NULL;
            }
        }
    }
    hc->ip = tmp;
    HC_DictItem *ditem;
    HC_Dict *dict;
    if(hc->enable_cookie){
        ditem = hc_dict_get_item(hc->domain_cookies, hc->raw_url+hc->host_index, hc->domain_len);
        if(!ditem){
            dict = hc_dict_create(TYPE_RAW, 36);
            ditem = hc_dict_set_item(hc->domain_cookies, hc->raw_url+hc->host_index, hc->domain_len, dict, 8, TYPE_NUM);
        }else{
            dict = (HC_Dict*)ditem->data;
        }
        hc->send_cookies = dict;
    }
    return 0;
}

int hc_http_client_set_zero(HC_HttpClient *hc)
{
    if(hc->ssl_handle){
        SSL_free(hc->ssl_handle);
        hc->ssl_handle=NULL;
    }
    hc->fd = -1;
    return 0;
}

int hc_http_client_fetch(HC_HttpClient *hc, int send_method)
{
    int count, redirect_count=0;
    int fd;
code_start:
    count = 0;
    hc_http_client_gen_data(hc,send_method);
code_fd:
    fd = hc->fd;
    if(0>fd){
       fd = hc_co_tcp4_connect(hc->ip, hc->port, 3);
       if(0>fd){
           return -3;
       }
       hc->fd = fd;
    }
    int ret=0;
    if(hc->ssl_flg){
        if(!hc->ssl_handle){
            ret = hc_httpc_ssl_create(hc);
            if(0!=ret){
                close(fd);
                hc->fd = -1;
                if(count>1)
                    return -1;
                count++;
                goto code_fd;
            }
        }
        ret = hc_ssl_write(hc->ssl_handle, hc->send_buf.buf, hc->send_buf.len);
        if(0!=ret){
            close(fd);
            SSL_free(hc->ssl_handle);
            hc->ssl_handle=NULL;
            hc->fd = -1;
            if(count>1){
                return -2;
            }
            count++;
            goto code_fd;
        }
        ret=hc_httpc_read(hc);
        hc->recv_buf.buf[hc->recv_buf.len]=0;
        if(0!=ret){
            SSL_free(hc->ssl_handle);
            hc->ssl_handle=NULL;
            close(fd);
            hc->fd = -1;
            if(count>1){
                return -3;
            }
            count++;
            goto code_fd;
        }
    }
    else{
        ret =hc_co_write_fd(fd, hc->send_buf.buf, hc->send_buf.len);
        if(0!=ret){
            if(count>1){
                close(fd);
                hc->fd = -1;
                return -2;
            }
            count++;
            close(fd);
            hc->fd = -1;
            goto code_fd;
        }
        ret=hc_httpc_read(hc);
        if(0!=ret){
            if(count>1){
                close(fd);
                hc->fd = -1;
                return -3;
            }
            count++;
            close(fd);
            hc->fd = -1;
            goto code_fd;
        }
    }
    if(0==hc->keep_alive){
        close(fd);
        if(hc->ssl_handle){
            SSL_free(hc->ssl_handle);
            hc->ssl_handle=NULL;
        }
        hc->fd=-1;
    }
    ret = hc_httpc_check_redirect(hc, &send_method);
    if(1==ret){
        redirect_count++;
        if(10>redirect_count)
            goto code_start;
    }
    else if(0!=ret){
        return -1;
    }
    hc->recv_data = hc->recv_buf.buf+hc->rsp_header_len;
    return 0;
}

static inline int hc_httpc_check_redirect(HC_HttpClient *hc, int *method)
{
    if(302!=hc->rsp_code&&301!=hc->rsp_code){
        return 0;
    }
    int ret=0, i_start;
    uint64_t dict_num=hc_dict_get_num(hc->rsp_header_indexs,"location",8,&ret);
    if(0<ret){
        i_start = (int)dict_num;
        ret = (int)(dict_num>>32);
        if(1>ret) return 0;
        ret = hc_http_client_parse(hc,hc->recv_buf.buf+i_start,ret);
        if(0!=ret){
            if(hc->keep_alive){
                close(hc->fd);
                if(hc->ssl_handle){
                    SSL_free(hc->ssl_handle);
                    hc->ssl_handle=NULL;
                }
                hc->fd = -1;
            }
            return -1;
        }
        *method = HTTP_GET;
        return 1;
    }
    return 0;
}

int hc_httpc_dict_encodeuri(HC_Dict *dict, HC_MemBuf *mem)
{
    HC_DictItem *di;
    int i=0;
    HC_MemBuf *keymem=&dict->key_mem;
    HC_MemBuf *datamem=&dict->data_mem;
    int len = mem->len;
    char *ptmp;
    for(;(di=hc_dict_get_item_by_kindex(dict,&i))!=NULL;){
        if(di->user_flag) continue;
        char *key = keymem->buf+di->key_index;
        hc_http_encodeuri_realloc(key, ((HC_DictKey*)key-1)->len, mem);
        ptmp = hc_membuf_check_size(mem, 1);
        ptmp[(mem->len)++]='=';
        if(di->data_index>0){
            ptmp = datamem->buf+di->data_index;
            hc_http_encodeuri_realloc(ptmp, ((HC_DictData*)ptmp-1)->len, mem);
        }
        ptmp = hc_membuf_check_size(mem, 1);
        ptmp[(mem->len)++]='&';
    }
    return mem->len - len;
}

int hc_http_client_gen_data(HC_HttpClient *hc, int send_method)
{
    HC_Dict *send_headers = hc->send_header;
    HC_Dict *send_params = hc->send_param;
    int send_buf_max;
    send_buf_max = hc->raw_url_len + hc->host_len + send_headers->key_mem.len + 
        send_headers->data_mem.len + 40;
    if(send_params){
        send_buf_max = send_buf_max + send_params->key_mem.len + send_params->data_mem.len;
    }
    if(hc->enable_cookie && hc->send_cookies){
        send_buf_max = send_buf_max + hc->send_cookies->key_mem.len + 
            hc->send_cookies->data_mem.len;
    }
    HC_MemBuf *membuf = &hc->send_buf;
    char *buf = membuf->buf;
    int top = 0;
    membuf->len=0;
    if(!buf){
        buf = malloc(send_buf_max);
        membuf->buf = buf;
        membuf->max = send_buf_max;
    }else{
        buf = hc_membuf_check_size(membuf, send_buf_max);
        send_buf_max = membuf->max;
    }

    switch(send_method){
        case(HTTP_GET):
            memcpy(buf,"GET ",4);
            top = 4;
            break;
        case(HTTP_POST):
            memcpy(buf,"POST ",5);
            top = 5;
            break;
        case(HTTP_PUT):
            memcpy(buf,"PUT ",4);
            top = 4;
            break;
        case(HTTP_DELETE):
            memcpy(buf,"DELETE ",7);
            top = 7;
            break;
        case(HTTP_HEAD):
            memcpy(buf,"HEAD ",5);
            top = 5;
            break;
        case(HTTP_OPTIONS):
            memcpy(buf,"OPTIONS ",8);
            top = 8;
            break;
        default:
            send_method = HTTP_GET;
            memcpy(buf,"GET ",4);
            top = 4;
            break;
    }
    if(0<hc->req_url_len){
        memcpy(buf+top, hc->raw_url+hc->req_url_index, hc->req_url_len);
        top+=hc->req_url_len;
    }
    else{
        buf[top++]='/';
    }
    int tmp=0,i=0;
    if(HTTP_GET==send_method){
        if(1<hc->send_data_len || send_params){
            for(i=hc->req_url_index;i<hc->raw_url_len;i++){
                if('?'==hc->raw_url[i]){
                    tmp=1;
                    break;
                }
            }
            if(0==tmp){
                buf[top++]='?';
            }else{
                buf[top++]='&';
            }
            if(1<hc->send_data_len){
                memcpy(buf+top, hc->send_data, hc->send_data_len);
                top+=hc->send_data_len;
            }
            if(send_params){
                if('&'!=buf[top-1] && '?'!=buf[top-1])
                    buf[top++]='&';
                membuf->len = top;
                hc_httpc_dict_encodeuri(send_params, membuf);
                buf = membuf->buf;
                top = membuf->len;
            }
        }
    }
    memcpy(buf+top," HTTP/1.1\r\nHost:",16);
    top+=16;
    memcpy(buf+top, hc->raw_url+hc->host_index, hc->host_len);
    top+=hc->host_len;
    memcpy(buf+top,"\r\n",2);
    top+=2;
    HC_DictItem *dict_item;   
    if(HTTP_POST==send_method || HTTP_PUT==send_method){
        if(HTTPC_SEND_JSON==hc->send_content_type)
            hc_dict_set_item(send_headers,"Content-Type",12,"application/json;charset=utf-8",30,TYPE_RAW);
        else if(HTTPC_SEND_URLENCODE==hc->send_content_type||hc->send_param)
            hc_dict_set_item(send_headers,"Content-Type",12,"application/x-www-form-urlencoded",33,TYPE_RAW);
    }else{
        dict_item = hc_dict_get_item(send_headers,"Content-Type",12);
        if(dict_item){
            ((HC_DictKey*)(send_headers->key_mem.buf+dict_item->key_index)-1)->status = 1;
        }
    }
    tmp = hc_dict_format(send_headers, buf+top, send_buf_max-top,"%0:%1\r\n",0,NULL);
    top+=tmp;
    int content_len=0, ret=0;
    if(hc->enable_cookie && hc->send_cookies&&hc->send_cookies->item_num){
        memcpy(buf+top,"Cookie:",7);
        top+=7;
        ret = hc_dict_format(hc->send_cookies,buf+top,send_buf_max-top,
                "%0=%1;",6,NULL);
        top+=ret;
        memcpy(buf+top,"\r\n",2);
        top+=2;
    }
    if(HTTP_POST==send_method || HTTP_PUT==send_method){
        memcpy(buf+top,"Content-Length:          0\r\n\r\n",30);
        ret = top+25;
        top+=30;
        if(HTTPC_SEND_JSON==hc->send_content_type){
            if(send_params){
                content_len = top;
                buf[top++]='{';
                ret = hc_dict_format(send_params,buf+top,send_buf_max-top,
                        "\"%0\":\"%1\",",10,NULL);
                top+=ret;
                buf[top-1]='}';
                content_len = top-content_len;
            }
        }
        else{
            content_len = top;
            if(1<hc->send_data_len){
                memcpy(buf+top,hc->send_data,hc->send_data_len);
                top+=hc->send_data_len;
            }
            if(send_params){
                if('&'!=buf[top-1] && '\n'!=buf[top-1])
                    buf[top++]='&';
                membuf->len = top;
                hc_httpc_dict_encodeuri(send_params, membuf);
                buf = membuf->buf;
                top = membuf->len;
            }
            content_len = top - content_len;
        }
        if(0<content_len){
            hc_num_to_str(content_len,10,NULL,buf+ret);
        }

    }
    else{
        memcpy(buf+top,"\r\n",2);
        top+=2;
    }
    membuf->len = top;
    return 0;
}

int hc_httpc_read(HC_HttpClient *hc)
{
    int ret =0, fd = hc->fd;
    HC_MemBuf *membuf = &hc->recv_buf;
    if(!membuf->buf){
        hc_membuf_init(membuf, 20);
    }
    hc->tmp = 0;
    hc->status = 0;
    hc_dict_reset_to_zero(hc->rsp_header_indexs);
    membuf->len = 0;
    for(ret=1;;){
        if(hc->ssl_flg){
            ret = hc_ssl_read_realloc_timeout(hc->ssl_handle,membuf, 
                    HTTPC_MAX_SIZE,30);
        }else{
            ret = hc_co_read_fd_realloc_timeout(fd,membuf,
                    HTTPC_MAX_SIZE,30);
        }
        if(1>ret){
            return -1;
        }
        if(20>hc->status){
            ret = hc_httpc_quick_check(hc);
            if(0>ret){
                return -1;
            }
            if(1==ret) continue;
            hc->tmp_var4 = 0;
        }
        if(1==hc->rsp_chunked){
            ret = hc_httpc_check_body_chunk(hc);
            if(0<ret) continue;
            if(0>ret){
                return -1;
            }
        }else{
            ret = hc_httpc_check_body_nochunk(hc);
            if(0<ret){
                if(membuf->event&EPOLLRDHUP){
                    hc->rsp_content_len = membuf->len - hc->rsp_header_len;
                }else
                    continue;
            }
        }
        break;
    }

    if(membuf->event&EPOLLRDHUP){
        hc->keep_alive = 0;
    }
    return 0;
}

int hc_httpc_quick_check(HC_HttpClient *hc)
{
    HC_MemBuf *membuf = &hc->recv_buf;
    char *src=membuf->buf;
    int src_max=membuf->len,str_len,i_end,ret=0;
    int i=hc->tmp, status = hc->status, i_start;
    char c_cur, end_char,str_break = 0;
    HC_Dict *header_indexs = hc->rsp_header_indexs;
    uint64_t dict_num;
code_for:
    for(;i<src_max;){
        for(i_start=i; i<src_max; i++){
            c_cur = src[i];
            switch(status){
                case 0:
                    if(' '==c_cur ){goto code_str_end;}
                    if(!isprint(c_cur)) return -1;
                    break;
                case 1:
                    if(' '==c_cur){ goto code_str_end;}
                    if('0'>c_cur||'9'<c_cur) return -1;
                    break;
                case 2: 
                    if('\r'==c_cur){goto code_str_end;}
                    if(!isprint(c_cur)) return -1;
                    continue;
                case 8:
                    if('\r'==c_cur || '\n'==c_cur){goto code_str_end;}
                    continue;
                case 10:
                    if(':'==c_cur || '\r'==c_cur){goto code_str_end;}
                    if(!isprint(c_cur)) return -1;
                    if('A'<=c_cur&&'Z'>=c_cur){src[i]|=32;}
                    continue;
                case 12:
                    if('\r'==c_cur){goto code_str_end;}
                    //if(!isprint(c_cur)) return -1;
                    continue;
                default:
                    goto code_str_end;
            }
        }
code_str_end:
        i_end = i;
        str_len = i_end - i_start;
        if(i==src_max)
            end_char = 0;
        else
            end_char = src[i];
        goto code_after_get;

code_after_get:
        switch(status){
            case 0: // version
                if(0==str_len) return -1;
                hc->rsp_version_len = str_len;
                ret = _http_escape_(src,&i,src_max,' ');
                if(ret!=0){
                    goto code_save;
                }
                status = 1;
                hc->tmp_var1 = i;
                hc->tmp_var2 = 0;
                goto code_for;

            case 1: // code 
                hc->tmp_var2 += str_len;
                if(3!=hc->tmp_var2) return -1;
                hc->rsp_code = hc_str_to_num(10, src+hc->tmp_var1,
                        3, &ret);
                if(-1==ret || !hc->rsp_code){
                    return -1;
                }
                ret = _http_escape_(src,&i,src_max,' ');
                if(ret!=0){
                    goto code_save;
                }
                if(127<i) return -1;
                hc->rsp_reason_index = i;
                status = 2;
                goto code_for;
            case 2: // reason
                hc->rsp_reason_len += str_len;
                if(127<hc->rsp_reason_len) return -1;
                if(!end_char){
                    goto code_for;
                }
                if('\r'==end_char){
                    ret = hc->rsp_reason_len;
                    _http_escape_right_(src,hc->rsp_reason_index,
                            &ret,' ');
                    hc->rsp_reason_len = ret;
                    status = 8;
                    hc->tmp_var1=0;
                }
                break;
            case 8:
                ret = _http_is_end_(src,&i,src_max,&hc->tmp_var1);
                if(ret!=0){
                    goto code_save;
                }
                i++;
                status = 9;
            case 9: //header
                ret = _http_escape_(src,&i,src_max,' ');
                if(ret!=0){
                    goto code_save;
                }
                status = 10;
                hc->tmp_var1 = i;
                hc->tmp_var2 = 0;
                break;
            case 10:
                hc->tmp_var2 += str_len;
                if(':'==end_char){
                    _http_escape_right_(src,hc->tmp_var1,&hc->tmp_var2,' ');
                    i++;
                    status = 11;
                    goto code_after_get;
                }else if('\r'==end_char){
                    _http_escape_right_(src,hc->tmp_var1,&hc->tmp_var2,' ');
                    if(hc->tmp_var2!=0) return -1;
                    hc->tmp_var1 = 0;
                    status = 14;
                }
                break;
            case 11: //header
                ret = _http_escape_(src,&i,src_max,' ');
                if(ret!=0){
                    goto code_save;
                }

                status = 12;
                hc->tmp_var3 = i;
                hc->tmp_var4 = 0;
                break;
            case 12: //header value
                hc->tmp_var4 += str_len;
                if('\r'==end_char){
                    if(0==hc_bin_cmp("set-cookie",10,src+hc->tmp_var1,
                                hc->tmp_var2) 
                            && hc->enable_cookie && hc->send_cookies)
                        hc_httpc_get_cookie(hc->send_cookies,src+hc->tmp_var3,hc->tmp_var4);
                    dict_num = hc->tmp_var4;
                    dict_num<<=32;
                    dict_num|=hc->tmp_var3;
                    hc_dict_set_num(header_indexs,src+hc->tmp_var1,hc->tmp_var2, dict_num, 8);
                    status = 13;
                    hc->tmp_var1=0;
                }
                break;
            case 13:
                ret = _http_is_end_(src,&i,src_max,&hc->tmp_var1);
                if(ret!=0){
                    goto code_save;
                }
                i++;
                status = 9;
                break;
            case 14:
                ret = _http_is_end_(src,&i,src_max,&hc->tmp_var1);
                if(ret!=0){
                    goto code_save;
                }
                goto code_end;
                break;
        }
    }

code_end:
    if(14!=status){
        ret = 1;
        goto code_save;
    }
    hc->tmp = ++i;
    hc->rsp_header_len = i;
    hc->status = 20;
    dict_num=hc_dict_get_num(header_indexs,"transfer-encoding",17,&ret);
    if(0<ret){
        i_start = (int)dict_num;
        ret = (int)(dict_num>>32);
        hc_alpha_to(src+i_start, ret, 0);
        if(0==hc_bin_cmp(src+i_start, ret, "chunked",7)){
            hc->rsp_chunked = 1;
            hc->rsp_content_len = 0;
        }else{
            return -1;
        }
    }else{
        dict_num=hc_dict_get_num(header_indexs,"content-length",14,&ret);
        if(-1==ret){
            hc->rsp_chunked = -1;
        }else{
            hc->rsp_chunked = 0;
            str_len = hc_str_to_num(10, src+(int)dict_num, 
                    (int)(dict_num>>32), &ret);
            if(-1==ret || 0>str_len){
                return -1;
            }
            hc->rsp_content_len = str_len;
        }
    }
    dict_num=hc_dict_get_num(header_indexs,"connection",10,&ret);
    if(0<ret){
        i_start = (int)dict_num;
        ret = (int)(dict_num>>32);
        hc_alpha_to(src+i_start, ret, 0);
        if(0==hc_bin_cmp(src+i_start, ret, "keep-alive",10)){
            hc->keep_alive= 1;
        }
        else if(0==hc_bin_cmp(src+i_start, ret, "upgrade",7)){
            hc->keep_alive= 1;
            //hc->rsp_content_len = 0;
            //hc->rsp_chunked = 0;
        }else{
            hc->keep_alive = 0;
        }
    }
    return 0;
code_save:
    hc->tmp = i;
    hc->status = status;
    return ret;
}

int hc_httpc_get_cookie(HC_Dict *cookies,char *src,int len)
{
    char c;
    int status=0, k_start,k_len=0,v_start,v_len=0;
    int  i = 0;
    _http_escape_(src,&i,len,' ');
    k_start = i;
    for(;i<len;i++){
        c = src[i];
        switch(status){
            case 0:
                if(';'==c||!isprint(c)) return -1;
                if('='==c){
                    k_len = i - k_start;
                    _http_escape_right_(src,k_start,&k_len,' ');
                    status = 1;
                    i++;
                    _http_escape_(src,&i,len,' ');
                    v_start = i;
                    i--;
                }
                break;
            case 1:
                if(';'==c||'\r'==c){
code_end:
                    v_len = i - v_start;
                    hc_dict_set_item(cookies,src+k_start,k_len,
                            src+v_start,v_len,TYPE_RAW);
                    return 0;
                }
                if(!isprint(c)) return -1;
                break;
        }
    }
    if(1==status)
        goto code_end;
    return -1;
}

int hc_httpc_check_body_chunk(HC_HttpClient *hc)
{
    HC_Dict *header_indexs = hc->rsp_header_indexs;
    int status = hc->status;
    int i_stare = 0, len=0, ret = 0;
    int i = hc->tmp, max = hc->recv_buf.len;
    char c, *src = hc->recv_buf.buf;
        switch(status){
code_start:
            case 20:
                hc->tmp_var1 = i;
                ret = _http_escape_(src,&i,max,' ');
                if(0!=ret){
                    goto code_save;
                }
                status = 21;
                hc->tmp_var2 = 0;
                hc->tmp_var3 = 0;
            case 21:
                for(;i<max;i++){
                    c = src[i];
                    if('\r'==c){
                        if(0!=hc->tmp_var3) return -1;
                        hc->tmp_var3 = 1;
                        continue;
                    }else if('\n'==c){
                        if(1!=hc->tmp_var3) return -1;
                        hc->tmp_var3 = i+1;
                        hc->tmp_var1 = hc->tmp_var3-hc->tmp_var1;
                        status = 22;
                        goto code_get_chunked;
                    }
                    c-='0';
                    if(0>c||54<c) return -1;
                    c = g_char_to_hex[c];
                    if(-1==c) return -1;
                    hc->tmp_var2 = hc->tmp_var2*16+c;
                }
                goto code_save;
                break;
            case 22:
code_get_chunked:
                len = hc->tmp_var2;
                hc->tmp_var4 -= hc->tmp_var1;
                if(0==len){
                    hc->tmp_var4 -= 2;
                    hc->recv_buf.len += hc->tmp_var4;;
                    return 0;
                }
                i = hc->tmp_var3;
                hc->rsp_content_len += len;
                ret = i + len + 2 - max;
                if(0<ret){
                    goto code_save;
                }
                hc_mem_move(src+i, len, hc->tmp_var4);
                hc->tmp_var4 -= 2;
                i = i + len + 2;
                status=20;
                goto code_start;
        }

code_save:
    hc->status = status;
    hc->tmp = i;
    return ret;
}

int hc_httpc_check_body_nochunk(HC_HttpClient *hc)
{
    if(0==hc->rsp_chunked){
        int len = hc->rsp_content_len;
        if(hc->rsp_header_len+len>hc->recv_buf.len){
            return 1;
        }
    }else{
        return 1;
    }
    return 0;
}


inline int hc_ssl_read_realloc_timeout(SSL *ssl_handle, HC_MemBuf *mem,int read_max, int timeout)
{
    int ret=0;
    mem->event = 0;
    int fd = SSL_get_fd(ssl_handle);
    uint32_t ev;
    for(int i=0;i<2;i++){
        ret = hc_ssl_read_realloc(ssl_handle,mem,read_max);
        if(CO_ERROR == mem->status) return -1;
        if(CO_EAGAIN== mem->status){
            ev = hc_co_wait_fd_timeout(fd, EPOLLIN|EPOLLRDHUP, timeout);
        }
        else if(CO_SSL_WANT_WRITE == mem->status)
            ev = hc_co_wait_fd_timeout(fd, EPOLLOUT|EPOLLRDHUP, timeout);
        else
            return ret;

        if(!ev) return -1;
        mem->event = ev;
    }
    if(0==ret) return -1;
    return ret;
}

inline int hc_ssl_read_realloc(SSL *ssl_handle, HC_MemBuf *mem, int read_max)
{
    int  tmp, max=mem->max, top=mem->len;
        
    char *buf = mem->buf;
    int read_count_cur = mem->len;
    for(;(tmp = SSL_read(ssl_handle, buf + top, max-top)) > 0;){
        top += tmp;
        if(top>=max){
            tmp = read_max - max;
            if(1>tmp) break;
            max += max;
            if(tmp>max) tmp = max;
            else max = read_max;
            mem->len = top;
            buf = hc_membuf_check_size(mem, tmp);
        }
    }
    read_count_cur = top - read_count_cur;
    if(1>read_count_cur){
        tmp = SSL_get_error(ssl_handle, tmp);
        if(tmp == SSL_ERROR_WANT_READ){
            mem->status = CO_EAGAIN;
            return 0;
        }else if(tmp==SSL_ERROR_ZERO_RETURN){
            mem->status = CO_ERROR;
            return -1;
        }
        else if(tmp == SSL_ERROR_WANT_WRITE){
            mem->status = CO_SSL_WANT_WRITE;
            return 0;
        }
        perror(" realloc co_buf ssl read");
        mem->status = CO_ERROR;
        return -1;
    }
    if(max==read_max&&0==errno){
        mem->status = CO_NEEDSPACE;
    }else
        mem->status = CO_OK;
    mem->len = top;
    return read_count_cur;
}


inline int hc_ssl_write(SSL* ssl_handle,char *buf,int len)
{
    int tmp;
    uint32_t ev;
    int fd = SSL_get_fd(ssl_handle);
        
    if(-1==len){
        len=strlen(buf);
    }
    for(int i=0;i<len;){
        tmp = len-i;
        tmp=SSL_write(ssl_handle,buf+i,tmp);
        if(tmp<1){
            tmp = SSL_get_error(ssl_handle, tmp);
            if(tmp == SSL_ERROR_WANT_READ){
                ev = hc_co_wait_fd_timeout(fd,EPOLLIN|EPOLLRDHUP, 20);
            }
            else if(tmp == SSL_ERROR_WANT_WRITE){
                ev = hc_co_wait_fd_timeout(fd,EPOLLOUT|EPOLLRDHUP, 20);
            }
            else if(tmp==SSL_ERROR_ZERO_RETURN){
                return -1;
            }else 
                return -1;
            if(!ev) return -1;
            if(ev & EPOLLRDHUP) return -1;
            continue;
        }
        i+=tmp;
    }
    return 0;
}

int hc_httpc_ssl_create(HC_HttpClient *hc)
{
    SSL *ssl_handle;
    SSL_CTX *ssl_context;
    ssl_handle = SSL_new (g_httpc_tls_context);
    if(NULL==ssl_handle){
        ERR_print_errors_fp (stderr);
        return -1;
    }
    if(NULL==ssl_handle){
        ERR_print_errors_fp (stderr);
        return -1;
    }
    int tmp = hc->raw_url[hc->host_index];
    hc->raw_url[hc->host_index+hc->host_len]='\0';
    SSL_set_tlsext_host_name(ssl_handle, hc->raw_url+hc->host_index);
    hc->raw_url[hc->host_index]=tmp;
    int fd = hc->fd;
    if(!SSL_set_fd (ssl_handle, fd)){
        ERR_print_errors_fp (stderr);
        return -1;
    }
    SSL_set_connect_state(ssl_handle);

    uint32_t ev;
    int ret = SSL_do_handshake(ssl_handle);
    for(;1!=ret;){
        ret = SSL_get_error(ssl_handle, ret);

        if(ret==SSL_ERROR_WANT_READ){
            ev = hc_co_wait_fd_timeout(fd,EPOLLIN|EPOLLET|EPOLLRDHUP, 20);
        }else if(ret == SSL_ERROR_WANT_WRITE){
            ev = hc_co_wait_fd_timeout(fd,EPOLLOUT|EPOLLET|EPOLLRDHUP, 20);
        }else{
            ERR_print_errors_fp (stderr);
            printf("SSL ERRROR Handle---=-=-\n");
            goto code_error;
        }
        if(!ev) goto code_error;
        if(ev & EPOLLRDHUP) goto code_error;
        ret = SSL_do_handshake(ssl_handle);
    }
    hc->ssl_handle = ssl_handle;
    return 0;
code_error:
    SSL_free(ssl_handle);
    hc->ssl_handle = NULL;
    return -1;
}

HC_HttpClient * hc_http_client_calloc(int enable_cookie)
{
    HC_HttpClient *hc = calloc(sizeof(HC_HttpClient),1);
    hc->send_header = hc_dict_copy(g_httpc_c_headers);
    hc->rsp_header_indexs = hc_dict_create(TYPE_NUM,32);
    if(enable_cookie){
        hc->enable_cookie = 1;
        hc->domain_cookies = hc_dict_create(TYPE_NUM,32);
    }
    hc->fd = -1;
    return hc;
}

int hc_http_client_free(HC_HttpClient *hc)
{
    hc_dict_free(hc->send_header);
    hc_dict_free(hc->rsp_header_indexs);
    if(-1<hc->fd) close(hc->fd);
    if(hc->domain_cookies){
        int i = 0;
        HC_Dict *dict = hc->domain_cookies;
        HC_DictItem *ditem;
        for(;NULL!=(ditem = hc_dict_get_item_by_kindex(dict,&i));){
            if(ditem->data) hc_dict_free((HC_Dict*)ditem->data);
        }
        hc_dict_free(dict);
    }
    if(hc->raw_url) free(hc->raw_url);
    if(hc->send_buf.buf) free(hc->send_buf.buf);
    if(hc->recv_buf.buf) free(hc->recv_buf.buf);
    if(hc->ssl_handle){
        SSL_free(hc->ssl_handle);
        hc->ssl_handle=NULL;
    }
    free(hc);
    return 0;
}

char * hc_httpc_get_header(HC_HttpClient *hc, char *key,int key_len,int *ret)
{

    uint64_t u64 = hc_dict_get_num(hc->rsp_header_indexs, key, key_len, ret);
    if(0>ret||!u64){
        *ret = 0;
        return NULL;
    }
    int index = (int)u64;
    *ret = (int)(u64>>32);
    return hc->recv_buf.buf+index;
}

int hc_http_client_write(HC_HttpClient *hc, void *buf, int buf_len)
{
    if(hc->ssl_flg){
        return hc_ssl_write(hc->ssl_handle, buf, buf_len);
    }
    return hc_co_write_fd(hc->fd, buf, buf_len);
}

inline int hc_ssl_write_keep_read(HC_CoFd *cofd,SSL* ssl_handle,void *buf,int len)
{
    int tmp;
    char change=0;
    uint32_t ev;
    int fd = SSL_get_fd(ssl_handle);
        
    if(-1==len){
        len=strlen(buf);
    }
    for(int i=0;i<len;){
        tmp = len-i;
        tmp=SSL_write(ssl_handle,buf+i,tmp);
        if(tmp<1){
            ERR_print_errors_fp (stderr);
            tmp = SSL_get_error(ssl_handle, tmp);
            if(tmp == SSL_ERROR_WANT_READ){
            hc_printf_error(" ssl write want read.. re handshake...\n");
                cofd->w_flg = 1;
                ev = hc_co_wait_fd(fd,EPOLLIN|EPOLLRDHUP);
                cofd->w_flg = 0;
            }
            else if(tmp == SSL_ERROR_WANT_WRITE){
            hc_printf_error(" ssl write want write.. %d\n",fd);
                cofd->w_flg = 1;
                ev = hc_co_wait_fd(fd,EPOLLOUT|EPOLLRDHUP);
                cofd->w_flg = 0;
            }
            else if(tmp==SSL_ERROR_ZERO_RETURN){
            hc_printf_error(" ssl write zero.. re handshake...\n");
                goto code_error;
            }else{
                hc_printf_error(" ssl other err:%d,fd:%d\n",tmp,fd);
                goto code_error;
            }
            change = 1;
            if(!ev){
                hc_printf_green("timeout write:%d\n",fd);
                goto code_error;
            }
            if(ev & EPOLLRDHUP){
                goto code_error;
            }
            hc_printf_error(" ssl write want write.ok. %d\n",fd);
            continue;
        }
        i+=tmp;
    }
    if(change && cofd->r_flg){
        hc_cofd_add_to_epoll(cofd, cofd->raw_events);
    }
    return 0;
code_error:
    hc_printf_error( " ssl wr err:%d, rflg:%d\n",cofd->fd,cofd->r_flg);
    if(change && cofd->r_flg){
        hc_cofd_add_to_epoll(cofd, cofd->raw_events);
    }
    return -1;
}

int hc_http_client_write_keep_read(HC_CoFd *cofd,HC_HttpClient *hc, void *buf, int buf_len)
{
    if(hc->ssl_flg){
        return hc_ssl_write_keep_read(cofd,hc->ssl_handle, buf, buf_len);
    }
    return hc_co_write_cofd_keep_read(cofd, buf, buf_len);
}

int hc_http_client_read(SSL *ssl_handle, HC_MemBuf *mem)
{
    int  tmp, max=mem->max, top=mem->len;
        
    if(top>=max){
        mem->status = CO_ERROR;
        return -2;
    }
    char *buf = mem->buf;
    int read_count_cur = mem->len;
    for(;(tmp = SSL_read(ssl_handle, buf + top, max-top)) > 0;){
        top += tmp;
        if(top>=max){
            break;
        }
    }
    read_count_cur = top - mem->len;
    if(1>read_count_cur){
    //    ERR_print_errors_fp (stderr);
        tmp = SSL_get_error(ssl_handle, tmp);
        if(tmp == SSL_ERROR_WANT_READ){
            mem->status = CO_EAGAIN;
            return 0;
        }else if(tmp==SSL_ERROR_WANT_WRITE){
            hc_printf_error(" ssl read want write.. re handshake...\n");
            mem->status = CO_SSL_WANT_WRITE;
            return 0;
        }
        else if(tmp==SSL_ERROR_ZERO_RETURN){
            mem->status = CO_ERROR;
            return -1;
        }
        mem->status = CO_ERROR;
        return -1;
    }
    if(top>=max){
        mem->status = CO_NEEDSPACE;
    }else
        mem->status = CO_OK;
    mem->len = top;
    return read_count_cur;
}

int hc_http_client_read_until(HC_HttpClient *hc, HC_MemBuf *mem,
        int until_top)
{
    int ret=0;
    mem->event = 0;
    uint32_t ev;
    for(int i=mem->len;i<until_top;i+=ret){
        if(hc->ssl_flg){
            if(hc->ssl_handle) return -1;
            ret = hc_http_client_read(hc->ssl_handle, mem);
            if(CO_ERROR == mem->status) return -1;
            if(CO_EAGAIN== mem->status){
                ev = hc_co_wait_fd(hc->fd, EPOLLIN|EPOLLRDHUP);
            }else if(CO_SSL_WANT_WRITE==mem->status){
                ev = hc_co_wait_fd(hc->fd, EPOLLOUT|EPOLLRDHUP);
            }else
                continue;
        }
        else{
            if(-1==hc->fd) return -1;
            ret = hc_co_read_fd_to_buf(hc->fd,mem);
            if(CO_ERROR == mem->status) return -1;
            if(CO_EAGAIN!= mem->status){
                continue;
            }
            ev = hc_co_wait_fd(hc->fd, EPOLLIN|EPOLLRDHUP);
        }
        if(!ev) return -1;
        if(ev&EPOLLRDHUP && i<until_top){
            return -1;
        }
        mem->event = ev;
    }
    return ret;
}

int hc_http_client_read_until_keep_cofd(HC_CoFd *cofd, HC_HttpClient *hc, HC_MemBuf *mem,
        int until_top)
{
    int ret=0;
    mem->event = 0;
    uint32_t ev=0;
    for(int i=mem->len;i<until_top;i+=ret){
        if(hc->ssl_flg){
            if(!hc->ssl_handle){ return -1;}
            ret = hc_http_client_read(hc->ssl_handle,mem);
            if(CO_ERROR == mem->status) return -1;
            if(CO_EAGAIN== mem->status){
                if(ev&EPOLLRDHUP){ return -1;};
                cofd->r_flg = 1;
                ev = hc_co_wait_cofd(cofd, EPOLLIN|EPOLLRDHUP);
                cofd->r_flg = 0;
            }else if(CO_SSL_WANT_WRITE==mem->status){
                hc_printf_error("ssl read want write:%d\n",cofd->fd);
                if(ev&EPOLLRDHUP){ return -1;};
                cofd->r_flg = 1;
                ev = hc_co_wait_cofd(cofd, EPOLLOUT|EPOLLRDHUP);
                cofd->r_flg = 0;
            }else
                continue;
        }
        else{
            if(-1==hc->fd) return -1;
            ret = hc_co_read_fd_to_buf(hc->fd,mem);
            if(CO_ERROR == mem->status) return -1;
            if(CO_EAGAIN!= mem->status){
                continue;
            }
            if(ev&EPOLLRDHUP){ return -1;};
            cofd->r_flg = 1;
            ev = hc_co_wait_cofd(cofd, EPOLLIN|EPOLLRDHUP);
            cofd->r_flg = 0;
        }
        if(!ev) return -1;
        mem->event = ev;
    }
    return ret;
}
