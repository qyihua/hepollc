#pragma once
#include "../util/hc_base_type.h"
#include "../util/hc_array.h"
#include "../util/hc_dict.h"
#include "../coroutine/coroutine.h"

#ifndef SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#define MAX_DNS_BUF 2048
#define HTTPC_MAX_SIZE 50000000
#define HTTPC_SEND_JSON 2
#define HTTPC_SEND_URLENCODE 1

typedef struct __attribute__((packed, scalar_storage_order("big-endian"))) HC_DnsHeader 
{
    uint16_t trans_id;
    union __attribute__((packed, scalar_storage_order("big-endian"))){
        struct __attribute__((packed, scalar_storage_order("big-endian"))){
            uint16_t qr:1;
            uint16_t opcode:4;
            uint16_t aa:1;
            uint16_t tc:1;
            uint16_t rd:1;
            uint16_t ra:1;
            uint16_t z:3;
            uint16_t rcode:4;
        };
        uint16_t flags;
    };
    uint16_t questions;
    uint16_t answer_rrs;
    uint16_t auth_rrs;
    uint16_t addi_rrs;
}HC_DnsHeader;

typedef struct HC_HttpClient{
    char ssl_read;
    char ssl_write;
    char method;
    char enable_cookie;
    char keep_alive;
    char rsp_version_len;
    char rsp_reason_index;
    char rsp_reason_len;
    char rsp_chunked;
    
    char ssl_flg;
    char keep_fd;
    char send_content_type;

    int fd;
    int rsp_header_len;
    int rsp_content_len;
    int tmp;
    char *raw_url;
    short int rsp_code;
    short int raw_url_len;
    short int host_index;
    short int host_len;
    short int domain_len;
    short int req_url_index;
    short int req_url_len;
    char *send_data;
    HC_MemBuf send_buf;
    HC_MemBuf recv_buf;
    char *recv_data;
    int send_data_len;
    HC_Dict *rsp_header_indexs;
    HC_Dict *send_param;
    HC_Dict *send_header;
    HC_Dict *send_cookies;
    HC_Dict *domain_cookies;
    int ip;
    short int port;
    short int code;
    int tmp_var1;
    int tmp_var2;
    int tmp_var3;
    int tmp_var4;
    int status;
    SSL *ssl_handle;
    void *ssl_writecallback;
    void *ssl_readcallback;

}HC_HttpClient;

int hc_http_client_parse(HC_HttpClient *p_HC, char *raw_url,int url_len);
uint32_t hc_dns_get_ip4(char*name,char len);
int hc_http_client_fetch(HC_HttpClient *hc, int send_method);
HC_HttpClient * hc_http_client_calloc(int enable_cookie);
int hc_http_client_gen_data(HC_HttpClient *hc, int send_method);
int hc_httpc_quick_check(HC_HttpClient *hc);
int hc_httpc_read(HC_HttpClient *hc);
int hc_httpc_get_cookie(HC_Dict *cookies,char *src,int len);
int hc_httpc_check_body_chunk(HC_HttpClient *hc);
int hc_httpc_check_body_nochunk(HC_HttpClient *hc);
int hc_ssl_read_realloc(SSL *ssl_handle, HC_MemBuf *mem, int read_max);
int hc_ssl_read_realloc_timeout(SSL *ssl_handle, HC_MemBuf *mem, int read_max, int timeout);
int hc_httpc_ssl_create(HC_HttpClient *hc);
int hc_ssl_write(SSL* ssl_handle,char *buf,int len);
int hc_http_client_free(HC_HttpClient *hc);
char * hc_httpc_get_header(HC_HttpClient *hc, char *key,int key_len,int *ret);
int hc_http_client_write(HC_HttpClient *hc, void *buf, int buf_len);
int hc_http_client_write_keep_read(HC_CoFd *cofd,HC_HttpClient *hc, void *buf, int buf_len);
int hc_ssl_write_keep_read(HC_CoFd *cofd,SSL* ssl_handle,void *buf,int len);
int hc_http_client_read(SSL *ssl_handle, HC_MemBuf *mem);
int hc_http_client_read_until(HC_HttpClient *hc, HC_MemBuf *mem,
        int until_top);
int hc_http_client_read_until_keep_cofd(HC_CoFd *cofd, HC_HttpClient *hc,
        HC_MemBuf *mem, int until_top);
int hc_http_client_set_zero(HC_HttpClient *hc);
int hc_httpc_dict_encodeuri(HC_Dict *dict, HC_MemBuf *mem);
int hc_httpc_init_handle(int a);
