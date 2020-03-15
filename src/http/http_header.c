#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <string.h>

#include "../util/hc_util.h"
#include "http.h"

extern uint64_t g_coroutine_base;

extern HC_Dict *g_http_methods;
extern HC_Dict *g_httpsrv_s_header;

static char gPathChar[94]={1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1};

static char gParaChar[94]={1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1};

extern char *g_hex_to_char;
extern char g_char_to_hex[55];

int hc_check_buf_space(void *src,int top,int *max,int len);
inline int _http_is_end_(char *src,int *i,int max,int *status_ptr)
{
        for(; ;(*i)++ ){
            switch(*status_ptr){
                case 0:
                    if(src[*i]!='\r')
                        return -1;
                    (*status_ptr)++;
                    break;
                case 1:
                    if(src[*i]!='\n')
                        return -1;
                    return 0;
                default:
                    return -1;
            }
        }
}

inline int _http_until_(char *src,int *i,int max,char until_chr)
{
    for(; ;(*i)++ ){
        if(*i>=max){
            return 1;
        }
        if(src[*i]!=until_chr)
            continue;
        break;
    }
    return 0;
}

inline int _http_escape_(char *src,int *i,int max,char escape_chr)
{
    for(; ; (*i)++){
        if(*i>=max){
            return 1;
        }
        if(src[*i]==escape_chr)
            continue;
        break;
    }
    return 0;
}

inline int _http_escape_right_(char *src,int start,int *len,char escape_chr)
{
    src+=start;
    start = *len-1;
    for(;start>=0;start--){
        if(src[start]==escape_chr){
            continue;
        }
        *len = start+1;
        return 0;
    }
    *len = start+1;
    return 0;
}

int hc_http_quick_check(HC_HttpRequest *hr)
{
    HC_HttpRequestRemote *hhc = &(hr->remote);
    char *src=hhc->raw_buf.buf;
    int src_max=hhc->raw_buf.len,str_len,i_end,ret=0;
    int i=hr->tmp, status = hhc->status, i_start;
    char c_cur, end_char,str_break = 0;
    HC_Dict *header_indexs = hhc->header_indexs;
    uint64_t dict_num;
code_for:
    for(;i<src_max;){
        str_break = 0;
        for(i_start=i; i<src_max; i++){
            c_cur = src[i];
            switch(status){
                case 0:
                    if('A'<=c_cur&&'Z'>=c_cur){c_cur|=32;src[i]|=32;}
                    if('a'<=c_cur && 'z'>=c_cur) continue;
                    str_break = 1; break;
                case 1:
                    if('/'==c_cur){ i_start = i; continue;}
                    status = 2;
                case 2: case 3:
                    if('?'==c_cur || ' '==c_cur){str_break = 1;break;}
                    if('\r'==c_cur || '\n'==c_cur) return -1;
                    if(!isprint(c_cur)) return -1;
                    continue;
                case 6: 
                    if(' '==c_cur){str_break = 1;break;}
                    if('\r'==c_cur || '\n'==c_cur) return -1;
                    if(!isprint(c_cur)) return -1;
                    continue;
                case 7:
                    if(' '==c_cur||'\r'==c_cur ){str_break = 1;break;}
                    if(!isprint(c_cur)){return -1;}
                    continue;
                case 8:
                    if('\r'==c_cur || '\n'==c_cur){str_break = 1;break;}
                    continue;
                case 10:
                    if(':'==c_cur || '\r'==c_cur){str_break = 1;break;}
                    if(!isprint(c_cur)) return -1;
                    if('A'<=c_cur&&'Z'>=c_cur){src[i]|=32;}
                    continue;
                case 12:
                    if('\r'==c_cur){str_break = 1;break;}
                    //if(!isprint(c_cur)) return -1;
                    continue;
                default:
                    str_break = 1;
            }
            if(str_break) break;
        }
        i_end = i;
        str_len = i_end - i_start;
        if(i==src_max)
            end_char = 0;
        else
            end_char = src[i];
        goto code_after_get;

code_after_get:
        switch(status){
            case 0: // method
                if(0==str_len) return -1;
                hhc->method_len = str_len;
                if(0 == hc_bin_cmp(src, str_len, "get", 3)){
                    hhc->method = HTTP_GET;
                }else if( 0 == hc_bin_cmp(src, str_len, "post", 4)){
                    hhc->method = HTTP_POST;
                }else{
                    dict_num = hc_dict_get_num(g_http_methods, src, str_len, &ret);
                    if(-1==ret){
                        hr->srv_code = 501;
                        hhc->method = HTTP_NOMETHOD;
                    }else{
                        hhc->method = dict_num;
                    }
                }
                ret = _http_until_(src,&i,src_max,'/');
                if(ret!=0){
                    hhc->status = status;
                    hr->tmp = i;
                    return ret;
                }
                status = 1;
                goto code_for;

            case 2: // app 
                hhc->path_index = i_start;
                status = 3;
            case 3: // path 
                hhc->path_len += str_len;
                if(!end_char){
                    goto code_for;
                }
                switch(end_char){
                    case '?':
                        if(0!=hhc->path_len && '/'==src[i-1])
                            hhc->path_len -= 1;
                        status = 6;
                        hhc->url_query_index = ++i;
                        goto code_for;
                    case ' ':
                        if(1<hhc->path_len && '/'==src[i-1])
                            hhc->path_len -= 1;
                        ret = _http_escape_(src,&i,src_max,' ');
                        if(ret!=0){
                            hhc->status = status;
                            hr->tmp = i;
                            return ret;
                        }
                        status = 7;
                        hhc->url_version_index = i_start;
                        goto code_for;
                }
            case 6: // query param 
                hhc->url_query_len+=str_len;
                if(' '==end_char){
                    ret = _http_escape_(src,&i,src_max,' ');
                    if(ret!=0){
                        hhc->status = status;
                        hr->tmp = i;
                        return ret;
                    }
                    hhc->url_version_index = i_start;
                    status=7;
                }
                break;
            case 7:
                hhc->url_version_len += str_len;
                if(' '==end_char){
                    ret = _http_escape_(src,&i,src_max,' ');
                    if(ret!=0){
                        hhc->status = status;
                        hr->tmp = i;
                        return ret;
                    }
                    status = 8;
                    hhc->tmp_var1=0;
                }
                if('\r'==end_char){
                    status = 8;
                    hhc->tmp_var1=0;
                }
                break;
            case 8:
                ret = _http_is_end_(src,&i,src_max,&hhc->tmp_var1);
                if(ret!=0){
                    hhc->status = status;
                    hr->tmp = i;
                    return ret;
                }
                i++;
                status = 9;
            case 9: //header
                ret = _http_escape_(src,&i,src_max,' ');
                if(ret!=0){
                    hhc->status = status;
                    hr->tmp = i;
                    return ret;
                }
                status = 10;
                hhc->tmp_var1 = i;
                hhc->tmp_var2 = 0;
                break;
            case 10:
                hhc->tmp_var2 += str_len;
                if(':'==end_char){
                    _http_escape_right_(src,hhc->tmp_var1,&hhc->tmp_var2,' ');
                    i++;
                    status = 11;
                    goto code_after_get;
                }else if('\r'==end_char){
                    _http_escape_right_(src,hhc->tmp_var1,&hhc->tmp_var2,' ');
                    if(hhc->tmp_var2!=0) return -1;
                    hhc->tmp_var1 = 0;
                    status = 14;
                }
                break;
            case 11: //header
                ret = _http_escape_(src,&i,src_max,' ');
                if(ret!=0){
                    hhc->status = status;
                    hr->tmp = i;
                    return ret;
                }

                status = 12;
                hhc->tmp_var3 = i;
                hhc->tmp_var4 = 0;
                break;
            case 12: //header value
                hhc->tmp_var4 += str_len;
                if('\r'==end_char){
                    dict_num = hhc->tmp_var4;
                    dict_num<<=32;
                    dict_num|=hhc->tmp_var3;
                    hc_dict_set_num(header_indexs,src+hhc->tmp_var1,hhc->tmp_var2, dict_num, 8);
                    status = 13;
                    hhc->tmp_var1=0;
                }
                break;
            case 13:
                ret = _http_is_end_(src,&i,src_max,&hhc->tmp_var1);
                if(ret!=0){
                    hhc->status = status;
                    hr->tmp = i;
                    return ret;
                }
                i++;
                status = 9;
                break;
            case 14:
                ret = _http_is_end_(src,&i,src_max,&hhc->tmp_var1);
                if(ret!=0){
                    hhc->status = status;
                    hr->tmp = i;
                    return ret;
                }
                goto code_end;
                break;
        }
    }

code_end:
    if(14!=status){
        hr->tmp = i;
        hhc->status = status;
        return 1;
    }
    hhc->header_len = ++i;

    if(HTTP_NOMETHOD==hhc->method){
        return -2;
    }
    hhc->status = 20;
    return 0;
}

int hc_http_check_body(HC_HttpRequest *hr)
{
    HC_HttpRequestRemote *hhc = &hr->remote;
    HC_Dict *header_indexs = hhc->header_indexs;
    uint64_t hdata;
    int ret=0;
    if(hhc->method != HTTP_POST && HTTP_PUT!=hhc->method){
        hhc->body_len= 0;
        return 0;
    }
    if( -1==hhc->body_len){
        hdata = hc_dict_get_num(header_indexs,"content-length",-1,&ret);
        if(-1==ret){
            hr->srv_code = 411; //content-length required
            return -1;
        }
        int len = hc_str_to_num(10, hhc->raw_buf.buf+(int)hdata, (int)(hdata>>32), &ret);
        if(-1==ret || 0>len){
            hr->srv_code = 400;
            return -1;
        }
        hhc->body_len = len;
    }
    if(hhc->header_len+hhc->body_len > hhc->raw_buf.len){
        return 1;
    }
    hhc->raw_buf.len = hhc->header_len + hhc->body_len;
    hhc->body_ptr = hhc->raw_buf.buf+hhc->header_len;

    return 0;
}

inline int hc_http_decode_realloc(const char *src,int src_len,char **dst, int *dst_top,int *dst_max)
{
    int dst_len=0;
    char c;
    for(int i=0;i<src_len;i++){
        c = src[i];
        hc_check_buf_space(dst,*dst_top,dst_max,1);
        switch(c){
            case '+':
                (*dst)[dst_len++] = ' ';
                break;
            case '%':
                if(i+2>=src_len) return -1;
                c = src[++i]-48;
                if(c<0||c>54) return -1;
                c = g_char_to_hex[c];
                if(-1==c) return -1;
                (*dst)[dst_len] = c;
                (*dst)[dst_len]<<=4;
                c = src[++i]-48;
                if(c<0||c>54) return -1;
                c = g_char_to_hex[c];
                if(-1==c) return -1;
                (*dst)[dst_len] |= c;
                dst_len++;
                break;
            default :
                (*dst)[dst_len++] = c;
        }
    }
    return dst_len;
};

inline int hc_http_decode(const char *src,int src_len,char *dst)
{
    int dst_len=0;
    char c;
    for(int i=0;i<src_len;i++){
        c = src[i];
        switch(c){
            case '+':
                dst[dst_len++] = ' ';
                break;
            case '%':
                if(i+2>=src_len) return -1;
                c = src[++i]-48;
                if(c<0||c>54) return -1;
                c = g_char_to_hex[c];
                if(-1==c) return -1;
                dst[dst_len] = c;
                dst[dst_len]<<=4;
                c = src[++i]-48;
                if(c<0||c>54) return -1;
                c = g_char_to_hex[c];
                if(-1==c) return -1;
                dst[dst_len] |= c;
                dst_len++;
                break;
            default :
                dst[dst_len++] = c;
        }
    }
    return dst_len;
};

inline int hc_alpha_to(char *src,int len,int type)
{
    for(int i=0;i<len;i++){
        if('A'<=src[i]&&'Z'>=src[i]){
            src[i]|=32;
        }
    }
    return 0;
}

static int str_check_httpheader(char *buf,int *i_ptr,int max,int *status_ptr)
{
    char c = buf[*i_ptr];
    switch(*status_ptr){
        case 0:
            if(' '==c) return 1;
            break;
        case 1:
            break;
    }
    return 0;

}

inline int hc_check_buf_space(void *src,int top,int *max,int len)
{
    top+=len;
    if(top>*max){
        top += top;
        *(void**)src = realloc(*(void**)src, top);
        *max = top;
    }
    return 0;
}

#define _HTTP_MEMCOPY_(dst,dst_top,src,src_top,len) \
    memcpy(dst+dst_top, src+src_top, len);\
    src_top = dst_top; dst_top+=str_len 

inline int _http_check_url(HC_HttpRequestRemote *hhc, char *src,int *src_top,int src_max,int *status_ptr)
{
    return 0;
}


int hc_http_remote_get_data(HC_HttpRequest *hr)
{
    HC_HttpRequestRemote *hhc = &(hr->remote);
    char *raw = hhc->raw_buf.buf;
    HC_Array * arr = hhc->decode_datas;
    char *data_buf = arr->data_buf;
    int len; 
    int data_top;
    HC_ArrayData *arr_data;
    int ret ;

    ret = hc_http_remote_get_data_path(hhc);
    if(0!=ret) return -1;

    ret = hc_http_remote_get_data_urldecoded(hhc, raw, hhc->url_query_index, hhc->url_query_len);
    if(0!=ret) return -1;

    uint64_t u64 = hc_dict_get_num(hhc->header_indexs,"content-type",12,&ret);
    int src_top;
    if(0<ret){
        src_top = (int)u64;
        len = (int)(u64>>32);
        hc_alpha_to(raw+src_top, len, 0);
        if(0 == hc_bin_cmp(raw+src_top, len>33?33:len, "application/x-www-form-urlencoded", 33)){
            len = hhc->body_len;
            if(0<len){
                ret = hc_http_remote_get_data_urldecoded(hhc, raw, 
                        hhc->header_len, len);
                if(0!=ret) return -1;
            }
        }
    }
    //hc_http_check_header(hhc);
    u64 = hc_dict_get_num(hhc->header_indexs,"cookie",6,&ret);
    if(0<ret){
        src_top = (int)u64;
        len = (int)(u64>>32);
        ret = hc_http_check_cookie(hhc, src_top, len);
        if(0!=ret) return -1;
    }
    u64 = hc_dict_get_num(hhc->header_indexs,"x-real-ip",9,&ret);
    if(0<ret){
        src_top = (int)u64;
        len = (int)(u64>>32);
        ret = inet_addr(raw + src_top);
        if(INADDR_NONE == ret){
            hr->real_ipv4 = hr->remote_ipv4;
            data_top = hc_array_set_item(arr, HTTP_INDEX_REALIP, 
                    NULL, 16, TYPE_RAW);
            arr_data = (HC_ArrayData*)(data_buf+data_top);
            ret = strlen(inet_ntop(AF_INET, (void*)&hr->remote_ipv4, arr_data->data, 16));
            arr_data->len = ret;
        }
        else{
            hr->real_ipv4 = ret;
            data_top = hc_array_set_item(arr, HTTP_INDEX_REALIP, 
                    raw+src_top, len, TYPE_RAW);
        }
    }else{
        hr->real_ipv4 = hr->remote_ipv4;
        data_top = hc_array_set_item(arr, HTTP_INDEX_REALIP, 
                NULL, 16, TYPE_RAW);
        arr_data = (HC_ArrayData*)(data_buf+data_top);
        ret = strlen(inet_ntop(AF_INET, (void*)&hr->remote_ipv4, arr_data->data, 16));
        arr_data->len = ret;

    }
    u64 = hc_dict_get_num(hhc->header_indexs,"connection",10,&ret);
    if(0<ret){
        src_top = (int)u64;
        len = (int)(u64>>32);
        hc_alpha_to(raw+src_top, len, 0);
        if(0 == hc_bin_cmp(raw+src_top, len, "keep-alive", 10)){
            hr->keep_alive = 1;
        }else if(0==hc_bin_cmp(raw+src_top, len, "upgrade",7)){
            //hr->keep_fd = 1;
            hr->keep_alive = 0;
        }else
            hr->keep_alive = 0;
    }else{
        hc_http_set_header(hr, "Connection", 10, "close", 5);
        hr->keep_alive = 0;
    }
    return 0;
}

int hc_http_remote_get_data_path(HC_HttpRequestRemote *hhc)
{
    HC_Array * arr = hhc->decode_datas;
    char *path_buf = arr->data_buf;
    int ret = hhc->path_len;
    int path_top = hc_array_set_item(arr, HTTP_INDEX_PATH, NULL, ret, TYPE_RAW);
    HC_ArrayData *path_arr_data = (HC_ArrayData*)(path_buf+path_top);
    path_buf = path_arr_data->data;
    path_top = 0;
    arr = hhc->path_arr;
    HC_ArrayData *arr_data;
    int i_start,len, path_param, i=hhc->path_index, src_max=i+ret, data_top;
    int app_path_len=0;
    hhc->app_path_len = 0;
    char * src = hhc->raw_buf.buf, c;
    for(i++, i_start = i; i<src_max; i++){
        c = src[i];
        if('/'==c){
            len = i - i_start;
            if(0==len&&0==app_path_len){
                i_start = i+1;
                continue;
            }
code_split:
            if('='==src[i_start] && 1==len && 0 == app_path_len){
                hc_array_append_item(arr,"=",1,TYPE_RAW);
                i_start = i+1;
                hhc->path_param_index = arr->item_num;
                if(50<arr->item_num) return -1;
                memcpy(path_buf+path_top, "/=", 2);
                app_path_len = path_top;
                path_top+=2;
                continue;
            }
            data_top = hc_array_append_item(arr, NULL, len, TYPE_RAW);
            if(50<arr->item_num) return -1;
            data_top = arr->data_indexs[data_top];
            arr_data = (HC_ArrayData*)(arr->data_buf+data_top);
            ret = hc_http_decode(src+i_start, len, arr_data->data);
            if(0>ret) return -1;
            arr_data->len = ret;
            path_buf[path_top++] = '/';
            memcpy(path_buf+path_top, arr_data->data, ret);
            path_top+=ret;
            i_start = i+1;
        }
    }
    len = i - i_start;
    if(0<len){
        goto code_split;
    }
    if(0==arr->item_num){
        path_buf[path_top++] = '/';
        hhc->app_path_index = 0;
        hhc->app_path_len = 0;
        hhc->decode_path_len = 1;
    }else{
        if('/'==path_buf[path_top-1]&&path_top>1){
            path_top--;
            path_buf[path_top] = '\0';
        }
        src = hc_array_get_item_retptr(arr,0,&ret);
        if(0==app_path_len){
            app_path_len = path_top;
        }
        hhc->app_path_index = ret + 2;
        ret = app_path_len - hhc->app_path_index;
        if(0>ret) ret = 0;
        hhc->app_path_len = ret;
        hhc->decode_path_len = path_top;
    }
    path_arr_data->len = path_top;
    path_buf[path_top] = '\0';
    return 0;
}

int hc_http_remote_get_data_urldecoded(HC_HttpRequestRemote *hhc,char *src, int i,int src_len)
{
    HC_Array *param_arr = hhc->param_arr;
    HC_ArrayData *arr_data;
    HC_Dict *param_dict = hhc->param_dict;
    HC_DictItem *ditem;
    HC_DictData *ddata;
    int i_start,str_len, type, data_top,
        ret = 0, src_max = i+src_len;

    char c;
    int status=0, count=0, key_i, key_len, param_index;
code_for:
    for(; i<src_max; i++){
        c = src[i];
        switch(status){
            case 0:
                i_start = i;
                status = 1;
            case 1:
                if('='==c) goto code_after_get;
                if('&'==c){
                    status = 0;
                    continue;
                }
                break;
            case 2:
                i_start = i;
                status = 3;
            case 3:
                if('&'==c){
                    goto code_after_get;
                }
                break;
            case 4:
                if('&'!=c) continue;
                status = 0;
                break;
        }
    }
    if(3!=status)
        goto code_end;

code_after_get:
    count++;
    str_len = i - i_start;
    switch(status){
        case 1:
            status = 2;
            key_i = i_start;
            key_len = str_len;
            if(3<str_len || 2>str_len || 'a'!=src[i_start]){ 
                //status = 4; goto code_for;
                type = 1;
                break;
            }
            param_index = hc_str_to_num(10, src+i_start+1, str_len-1, &ret);
            if(0>param_index || 0>ret){ 
                //status = 4; goto code_for;
                type = 1;
                break;
            }
            type = 0;
            break;
        case 3:
            status = 0;
            if(0==type){
                data_top = hc_array_set_item(param_arr,param_index,NULL,str_len,TYPE_RAW);
                arr_data = (HC_ArrayData*)(param_arr->data_buf+data_top);
                ret = hc_http_decode(src+i_start, str_len, arr_data->data);
                if(0>ret) return -1;
                arr_data->len = ret;
                if(param_arr->data_top==data_top+arr_data->max_len+1){
                    ret = arr_data->max_len - ret;
                    arr_data->max_len -= ret;
                    param_arr->data_top -= ret;
                }
                break;
            }
            ditem = hc_dict_set_item(param_dict, src+key_i,key_len,NULL,
                    str_len,TYPE_RAW);
            if(!ditem) return -1;
            if(ditem->data_index==0) break;
            ddata = (HC_DictData*)(param_dict->data_mem.buf+ditem->data_index)-1;
            ret = hc_http_decode(src+i_start, str_len, 
                    ddata->data);
            if(0>ret) return -1;
            ddata->len = ret;
            if(param_dict->data_mem.len == ditem->data_index+ddata->max_len+1){
                ret = ddata->max_len - ret;
                ddata->max_len -= ret;
                param_dict->data_mem.len -= ret;
            }
            break;
    }
    i++;
    if(i<src_max)
        goto code_for;
code_end:
    return 0;

}

//int hc_http_check_header(HC_HttpRequestRemote *hhc)
//{
//    HC_Dict *header_indexs = hhc->header_indexs;
//    HC_Dict *headers = hhc->headers;
//    HC_MemBuf *key_mem = header_indexs->key_mem;
//    HC_DictKey *dkey;
//    HC_DictItem *ditem;
//    char *key_buf= key_mem->buf, *raw = hhc->raw;
//    int key_top=key_mem->len;
//    uint64_t hindex;
//    for(int i=0;i<key_top;){
//        dkey = (HC_DictKey*)(key_buf+i);
//        ditem = hc_dict_has(headers, dkey->data, dkey->len);
//        if(ditem){
//            hindex = (header_indexs->items + dkey->item_index)->data;
//
//            printf("key len:%d,:%s, ",dkey->len, dkey->data);
//            hc_dict_push_data(headers, ditem, raw+(int)hindex, 
//                    (int)(hindex>>32), TYPE_RAW);
//            printf("item:i:%d,%s\n",ditem->data_index,headers->data_mem->buf+ditem->data_index);
//        }
//        i = i + sizeof(HC_DictKey) + dkey->len + 1;
//    }
//
//    return 0;
//}

int hc_http_check_cookie(HC_HttpRequestRemote *hhc, int i, int cookies_len)
{
    char *src=hhc->raw_buf.buf;
    int src_max=i + cookies_len, str_len,i_end,ret=0;
    int status = 0, i_start;
    char c, end_char,str_break = 0;
    HC_Dict *cookie_indexs = hhc->cookie_indexs;
    uint64_t u64;
    int key_index, key_len, count=0;
code_for:
    for(; i<src_max; i++){
        c = src[i];
        switch(status){
            case 0:
                if('='==c||';'==c) continue;
                i_start = i;
                status = 1;
            case 1:
                if('='==c) goto code_after_get;
                if(';'==c){
                    status = 0;
                    continue;
                }
                break;
            case 2:
                i_start = i;
                status = 3;
            case 3:
                if(';'==c){
                    goto code_after_get;
                }
                break;
            case 4:
                if(';'!=c) continue;
                status = 0;
                break;
        }
    }
    if(3!=status)
        goto code_end;
code_after_get:
    switch(status){
        case 1:
            _http_escape_(src, &i_start, i,' ');
            str_len = i - i_start;
            _http_escape_right_(src, i_start, &str_len,' ');
            if(1>str_len){ status = 4; goto code_for;}
            status = 2;
            key_index = i_start;
            key_len = str_len;
            break;
        case 3:
            str_len = i - i_start;
            status = 0;
            _http_escape_(src, &i_start, i_start+str_len,' ');
            _http_escape_right_(src, i_start, &str_len,' ');
            if(1>str_len){ goto code_for;}
            u64 = str_len;
            u64<<=32;
            u64|=i_start;
            hc_dict_set_num(cookie_indexs,src+key_index, key_len, u64, 8);
            if(count++>200) return -1;
            break;
    }
    i++;
    if(i<src_max)
        goto code_for;
code_end:
    return 0;

}

char * hc_http_get_pathdata(HC_HttpRequestRemote *hhc,int index,int *ret)
{
    return hc_array_get_item_retptr(hhc->path_arr, index, ret);
}

char * hc_http_get_pathparam(HC_HttpRequestRemote *hhc,int index,int *ret)
{
    return hc_array_get_item_retptr(hhc->path_arr, index+hhc->path_param_index, ret);
}

char * hc_http_get_httpdata(HC_HttpRequestRemote *hhc,int index,int *ret)
{
    return hc_array_get_item_retptr(hhc->decode_datas, index, ret);
}

char * hc_http_get_param_by_index(HC_HttpRequestRemote *hhc,int index,int *ret)
{
    return hc_array_get_item_retptr(hhc->param_arr, index, ret);
}

char * hc_http_get_param_by_key(HC_HttpRequestRemote *hhc, char *key,int key_len,int *ret)
{
    return hc_dict_get_ptr(hhc->param_dict, key, key_len, ret);
}

char * hc_http_get_cookie(HC_HttpRequestRemote *hhc, char *key,int key_len,int *ret)
{
    uint64_t u64 = hc_dict_get_num(hhc->cookie_indexs, key, key_len, ret);
    if(0>*ret||!u64) return NULL;
    int index = (int)u64;
    *ret = (int)(u64>>32);
    return hhc->raw_buf.buf+index;
}

char * hc_http_get_header(HC_HttpRequestRemote *hhc, char *key,int key_len,int *ret)
{

    uint64_t u64 = hc_dict_get_num(hhc->header_indexs, key, key_len, ret);
    if(0>ret||!u64) return NULL;
    int index = (int)u64;
    *ret = (int)(u64>>32);
    return hhc->raw_buf.buf+index;
}

int hc_http_set_code(HC_HttpRequest *hr, short int code, char *msg, int msg_len)
{
    hr->srv_code = code;
    if(msg){
        hc_dict_set_item(hr->srv_codes, (char*)&code, 2, msg, 
                msg_len, TYPE_RAW);
    }
    return 0;
}

int hc_http_set_header(HC_HttpRequest *hr, char *key, int key_len,
        char *data, int data_len)
{
    if(data){
        hc_dict_set_item(hr->srv_headers, key, key_len, data, 
                data_len, TYPE_RAW);
    }
    return 0;
}

int hc_httpsrv_gen_header(HC_HttpRequest *hr, int content_len)
{
    char *buf = hr->srv_buf;
    int top, max=hr->srv_buf_max, ret;
    memcpy(buf, "HTTP/1.1 ", 9);
    top = 9;
    short int code = hr->srv_code;
    if(code<100 || code>600){
        code = 200;
        hr->srv_code = 200;
    }
    top+=2;
    hc_num_to_str(code,10,NULL,buf+top);
    buf[++top] = ' ';
    top++;
    
    char *ptmp = hc_dict_get_ptr(hr->srv_codes, (char*)&code, 2, &ret);
    if(ptmp){
        if(50-top<ret) ret = 50-top;
        memcpy(buf+top, ptmp, ret);
        top+=ret;
    }
    memcpy(buf+top, "\r\n", 2);
    top+=2;
    HC_Dict *dict = hr->srv_headers;
    int need_len = dict->key_mem.len + dict->data_mem.len +32;
    hc_check_buf_space(&buf, top, &max, need_len);
    ret = hc_dict_format(hr->srv_headers, buf+top, max - top,"%0:%1\r\n",0,NULL); 
    top += ret;
    if(hr->srv_tran_enc){
        memcpy(buf+top, "Transfer-Encoding:chunked\r\n",27);
        top+=27;
    }else{
        memcpy(buf+top, "Content-Length:",15);
        top+=15;
        ptmp = hc_num_to_str(content_len,10,&ret,NULL);
        memcpy(buf+top, ptmp, ret);
        top+=ret;
        
        memcpy(buf+top, "\r\n", 2);
        top+=2;
    }
    if(hr->srv_cookies){
        HC_Array *arr = hr->srv_cookies;
        ptmp = arr->data_buf;
        ret = arr->item_num;
        int *indexs = arr->data_indexs;
        HC_ArrayData *arr_data;
        hc_check_buf_space(&buf, top, &max, ret+arr->data_top);
        for(int i=0; i<ret ;i++){
            memcpy(buf+top, "Set-Cookie:",11);
            top+=11;
            arr_data = (HC_ArrayData*)(ptmp+indexs[i]);
            memcpy(buf+top, arr_data->data, arr_data->len);
            top+=arr_data->len;
            memcpy(buf+top, "\r\n",2);
            top+=2;
        }
    }
    memcpy(buf+top, "\r\n", 2);
    top+=2;
    hr->srv_buf = buf;
    hr->srv_buf_len = top;
    hr->srv_buf_max = max;
    return 0;
}

char * hc_http_get_rawdata(HC_HttpRequestRemote *hhc,int index,int *ret)
{
    char *ptmp;
    switch(index){
        case HTTP_RAWDATA_URL:
            ptmp = hhc->raw_buf.buf+hhc->path_index;
            *ret = hhc->path_len;
            if(hhc->url_query_len)
                *ret = *ret+hhc->url_query_len+1;
            break;
        case HTTP_RAWDATA_PATH:
            ptmp = hhc->raw_buf.buf+hhc->path_index;
            *ret = hhc->path_len;
            break;
        case HTTP_RAWDATA_QUERY:
            ptmp = hhc->raw_buf.buf+hhc->url_query_index;
            *ret = hhc->url_query_len;
            break;
        default:
            *ret=0;
            return NULL;
    }
    return ptmp;
}

