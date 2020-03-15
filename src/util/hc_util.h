#pragma once
#include "hc_array.h"
#include "hc_base_type.h"

#ifndef g_process_id
extern int g_process_id;
#endif

#define hc_printf_warn(str,...) printf("\033[1;33m"str"\033[0m",##__VA_ARGS__)

#define hc_printf_error(str,...) printf("\033[0;31m"str"\033[0m",##__VA_ARGS__)

#define hc_printf_green(str,...) printf("\033[0;32m"str"\033[0m",##__VA_ARGS__)

#define hc_printf_once_green(str,...) if(0!=g_process_id)printf("\033[0;32m"str"\033[0m",##__VA_ARGS__)
#define hc_printf_once_error(str,...) if(0!=g_process_id)printf("\033[0;31m"str"\033[0m",##__VA_ARGS__)
#define hc_printf_once_warn(str,...) if(0!=g_process_id)printf("\033[1;33m"str"\033[0m",##__VA_ARGS__)

int hc_json_decode(char *p,char *desc,int *i_desc);
int hc_json_escape(char *pRaw,int vLen,HC_MemBuf*mb);
long hc_str_to_num(char type,char *str,int len,int *ret);
char *hc_num_to_str(long num,char type,int *a_len,char *pTmp);
char *hc_num_to_str_pad(int num,char type,int *a_len,char *pTmp,int len);
//int hc_http_listen_handle(HC_CoFd *cofd, uint32_t events);

int hc_mem_move(char *src,int len,int offset);
int hc_utl_until(char *src,int *i,int max,char until_chr);
int hc_utl_escape(char *src,int *i,int max,char escape_chr);
int hc_utl_escape_right(char *src,int start,int *len,char escape_chr);
int hc_bin_casecmp(char *src,int src_len,char *dst, int dst_len);
int hc_bin_cmp(char *src,int src_len,char *dst, int dst_len);
HC_Array * hc_split_trim(HC_Array *arg_arr, char *src, int src_max, char split_chr, int split_num, char trim_chr);
char *hc_get_time_str();
long hc_get_random_64();
char* hc_byte_to_hex(void *raw,int len,HC_MemBuf *mp,int small);
int hc_membuf_init(HC_MemBuf *src,int len);
void* hc_membuf_check_size(HC_MemBuf *src, int len);
HC_MemBuf *hc_membuf_create(int len);
void* hc_membuf_check_max(HC_MemBuf *ptr, int max);
void hc_membuf_free(HC_MemBuf *mem);
void hc_jsbuf_set_data(HC_MemBuf *mem, void *data, int len, int type);
void hc_jsbuf_set_header(HC_MemBuf *mem, int len_len,int len, int type);
