#pragma once
#include "../util/hc_base_type.h"
#include "../util/hc_array.h"
#include "../util/hc_dict.h"
#include "../coroutine/coroutine.h"

#ifndef timeval
#include<sys/time.h>
#endif

#define HTTP_NOMETHOD 0
#define HTTP_GET 1
#define HTTP_POST 2
#define HTTP_OPTIONS 3
#define HTTP_HEAD 4
#define HTTP_PUT 5
#define HTTP_DELETE 6

#define HTTP_INDEX_METHOD 0
#define HTTP_INDEX_PATH 4
#define HTTP_INDEX_REALIP  1

#define HTTP_RAWDATA_URL  1
#define HTTP_RAWDATA_PATH  2
#define HTTP_RAWDATA_QUERY  3

#define HTTP_MAX_SIZE 20480000
#define MAX_HTTPCLIENT_RES 50000000

typedef struct HC_HttpRequestRemote{
    char status;
    char method;
    char method_len;
    short int decode_path_len;
    short int app_path_index;
    short int app_path_len;
    short int path_index;
    short int path_len;
    
    int16_t path_param_index;
    int16_t path_param_len;

    int16_t url_query_index;
    int16_t url_query_len;

    int16_t url_version_index;
    int16_t url_version_len;

    int body_len;
    int header_len;


    int tmp_var1;
    int tmp_var2;
    int tmp_var3;
    int tmp_var4;

    char *body_ptr;
    HC_Dict *header_indexs;
    HC_Dict *cookie_indexs;
    HC_MemBuf raw_buf;
    uint32_t *path_indexs;
    HC_Array *decode_datas;
    HC_Array *path_arr;
    HC_Array *param_arr;
    HC_Dict *param_dict;
}HC_HttpRequestRemote;

typedef struct HC_HttpHeaderS{
    char status;
    short int code;
    int content_len;
    int buf_len;
    int buf_max;
    char *send_buf;
} HC_HttpHeaderS;

typedef struct HC_HttpServer{
    int fd;
    HC_Dict *app;
    HC_Dict *url;
    HC_Dict *app_lib;
    void *fun;
    void *data;
}HC_HttpServer;

typedef struct HC_HttpRequest{
    char finish;
    char keep_alive;
    char head_method;
    char srv_tran_enc;
    char keep_fd;
    char srv_status;
    short int srv_code;
    int fd;
    uint32_t finish_len;
    unsigned int remote_ipv4;
    unsigned int real_ipv4;
    int tmp;
    int srv_content_len;
    int srv_buf_len;
    int srv_buf_max;
    char *srv_buf;
    HC_MemBuf tmp_buf;
    HC_HttpRequestRemote remote;
    struct timeval start_time;
    void *my_data;

    HC_Dict *srv_headers;
    HC_Dict *srv_codes;
    HC_Array *srv_cookies;
    HC_HttpServer *srv;
}HC_HttpRequest;

typedef struct Url{
    char *url;
    void *fun;
    char noCheck;
    char t2;
}Url;


int hc_http_quick_check(HC_HttpRequest *hr);
int hc_http_check_body(HC_HttpRequest *hr);
int hc_http_check_params(HC_HttpRequest *hr);

int hc_http_check_header(HC_HttpRequestRemote *hhc); 

int _http_check_url(HC_HttpRequestRemote *hhc, char *src,int *src_top,int src_max,int*);
int _http_check_size(HC_HttpRequestRemote *hhc,int len);
int hc_http_free(HC_HttpRequest *p_HR);

int hc_http_decode(const char *src,int src_len,char *dst);
int hc_http_decode_realloc(const char *src,int src_len,char **dst, int *dst_top,int *dst_max);
int hc_alpha_to(char *src,int len,int type);
int hc_http_decode_raw(HC_HttpRequest *hr);
int hc_http_remote_get_data_path(HC_HttpRequestRemote *hhc);
int hc_http_remote_get_data(HC_HttpRequest *hr);
int hc_http_remote_get_data_urldecoded(HC_HttpRequestRemote *hhc,char *src, int i,int src_len);
int hc_http_check_cookie(HC_HttpRequestRemote *hhc, int i, int cookies_len);
char * hc_http_get_param_by_index(HC_HttpRequestRemote *hhc,int index,int *ret);
char * hc_http_get_param_by_key(HC_HttpRequestRemote *hhc, char *key,int key_len,int *ret);
char * hc_http_get_cookie(HC_HttpRequestRemote *hhc, char *key,int key_len,int *ret);
 char * hc_http_get_header(HC_HttpRequestRemote *hhc, char *key,int key_len,int *ret);
int hc_httpsrv_gen_header(HC_HttpRequest *hr, int content_len);
int hc_http_finish(HC_HttpRequest *hr, char *send_data, int data_len);
int hc_http_write(HC_HttpRequest *hr, char *send_data, int data_len);
int hc_http_set_cookie(HC_HttpRequest *hr, char *key,int key_len,
        char *val,int val_len);
int hc_http_clear_cookie(HC_HttpRequest *hr,const char* key,int key_len,char *cookie_info, int info_len);
int hc_http_set_header(HC_HttpRequest *hr, char *key,int key_len,char *val,int val_len);
int hc_http_redirect(HC_HttpRequest *hr, int code,char *url,int len);
int hc_http_end(HC_HttpRequest *hr);
int hc_http_free(HC_HttpRequest *hr);
int _http_escape_(char *src,int *i,int max,char escape_chr);
int _http_is_end_(char *src,int *i,int max,int *status_ptr);
int _http_until_(char *src,int *i,int max,char until_chr);
int _http_escape_right_(char *src,int start,int *len,char escape_chr);

HC_HttpServer* hc_http_srv_create(int fd);
int hc_http_srv_set_url(HC_HttpServer *http_srv,char *url, int url_len,void *fun);
int hc_http_srv_set_app(HC_HttpServer *http_srv,char *name, int name_len,void *fun);
int hc_http_url_handle(HC_HttpRequest *hr);
void* hc_http_url_get_handle(HC_HttpRequest *hr);
int hc_load_app(HC_HttpServer *srv, char *app_conf_file);
char * hc_http_get_pathparam(HC_HttpRequestRemote *hhc,int index,int *ret);
char * hc_http_get_httpdata(HC_HttpRequestRemote *hhc,int index,int *ret);
int hc_http_set_code(HC_HttpRequest *hr, short int code, char *msg, int msg_len);
char * hc_http_get_pathdata(HC_HttpRequestRemote *hhc,int index,int *ret);
HC_MemBuf* hc_http_get_tmpbuf(HC_HttpRequest *hr, uint32_t len);
void hc_http_srv_free(HC_HttpServer *srv);
char * hc_http_get_rawdata(HC_HttpRequestRemote *hhc,int index,int *ret);
int hc_check_buf_space(void *src,int top,int *max,int len);
char *hc_http_encodeuri(char *raw, int raw_len,int *ret);
int hc_http_encodeuri_realloc(char *raw, int raw_len,HC_MemBuf *mem);
int hc_http_finish_push(HC_HttpRequest *hr, char *send_data,int data_len);
int hc_http_finish_start(HC_HttpRequest *hr, int data_len);
int hc_http_init_handle();
int hc_http_listen_handle(HC_CoFd *cofd, uint32_t events);
