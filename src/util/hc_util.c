#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

#include "hc_util.h"

char gpTimeStr[]={"0000-00-00 00:00:00.000000\0"};
char iTmp[101]={0};
extern char *g_hex_to_char;
extern char g_char_to_hex[55];
static int randomFd = 0;

inline char* hc_byte_to_hex(void *raw,int len,HC_MemBuf *mem,int small)
{
   char *data=NULL;
   char *pStr = g_hex_to_char;
   unsigned char b;
   if(small) pStr = pStr+16;
   int top=0,maxSize=len+len,i;
   if(!mem){
       data=malloc(maxSize);
       for(i=0;i<len;i++){
           b=((char*)raw)[i];
           data[top++]=pStr[b>>4];
           data[top++]=pStr[0b1111&b];
       }
       return data;
   }else{
       data = hc_membuf_check_size(mem, maxSize);
       top=mem->len;
       maxSize=top;
       for(i=0;i<len;i++){
           b=((char*)raw)[i];
           data[top++]=pStr[b>>4];
           data[top++]=pStr[0b1111&b];
       }
       mem->len=top;
       return data+maxSize;
   }
}


inline char *hc_num_to_str_pad(int num,char type,int *a_len,char *pTmp,int len)
{
    int i=0,n1;
    char isF=0;
    if(!pTmp) pTmp=iTmp+99;
    if(0==num){
        if(1<len){
            pTmp = pTmp - (len-1);
            memset(pTmp,'0',len);
            if(a_len) *a_len = len;
        }else{
            pTmp[0]='0';
            if(a_len)
                *a_len=1;
        }
        return pTmp;
    }
    if(10==type){
        if(0>num){
            num = 0-num;
            isF=1;
        }
        for(;num;i--){
            pTmp[i]=num%10+'0';
            num/=10;
        }
    }else if(16==type){
        if(0>num){
            num = 0-num;
            isF=1;
        }
        for(;num;i--){
            n1 = num & 0b1111;
            pTmp[i] = g_hex_to_char[n1];
            num>>=4;
        }

    }
    for(n1=len+i; n1>0; n1--){
        pTmp[i--] = '0';
    }
    if(isF){
        pTmp[i--]='-';
    }
    if(a_len)
        *a_len=0-i;
    return pTmp+i+1;
}
inline char *hc_num_to_str(long num,char type,int *a_len,char *pTmp)
{
    int i=0,n1;
    char isF=0;
    if(!pTmp) pTmp=iTmp+99;
    if(0==num){
        pTmp[0]='0';
        if(a_len)
            *a_len=1;
        return pTmp;
    }
    if(10==type){
        if(0>num){
            num = 0-num;
            isF=1;
        }
        for(;num;i--){
            pTmp[i]=num%10+'0';
            num/=10;
        }
        if(isF){
            pTmp[i--]='-';
        }
    }else if(16==type){
        if(0>num){
            num = 0-num;
            isF=1;
        }
        for(;num;i--){
            n1 = num & 0b1111;
            pTmp[i] = g_hex_to_char[n1];
            num>>=4;
        }
        if(isF){
            pTmp[i--]='-';
        }
    }
    if(a_len)
        *a_len=0-i;
    return pTmp+i+1;
}

inline unsigned long strToByteL(char type,char *str,int len,int *ret){
    char c;
    int i;
    unsigned long num=0;
    switch(type){
        case 16:
            for(i=0;i<len;i++){
                c = str[i]-48;
                if(0>c||54<c){
                    *ret = -1;
                    return num;
                }
                c = g_char_to_hex[(unsigned char)c];
                if(-1==c){
                    *ret=-1;
                    return num;
                }
                num = num<<4|c;
            }
            break;
    }
    *ret = 0;
    return num;
}

inline long hc_str_to_num(char type,char *str,int len,int *ret)
{
    char c;
    int i=0;
    int64_t num=0,flag=0;

    if('-'==str[0]){
        flag=-1;
        i=1;
    }
    switch(type){
        case 16:
            for(;i<len;i++){
                c = str[i]-48;
                if(0>c||54<c){
                    *ret = -1;
                    return num;
                }
                c = g_char_to_hex[(unsigned char)c];
                if(-1==c){
                    *ret=-1;
                    return num;
                }
                num = num<<4|c;
            }
            break;
        case 10:
            for(;i<len;i++){
                c = str[i]-'0';
                if(0>c||9<c){
                    *ret = -1;
                    return num;
                }
                num = num*10+c;
            }
            break;
    }
    *ret = 0;
    if(flag) num = 0-num;
    return num;
}


char *hc_get_time_str()
{
    struct tm * timeinfo;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    timeinfo = localtime ( &tv.tv_sec );
    char *pTmp=gpTimeStr, i;
    //printf("%04d-%02d-%02d_%02d:%02d:%02d ",timeinfo->tm_year+1900, timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    int num = timeinfo->tm_year+1900;
    for(i=3;i>-1;i--){
        pTmp[i]=num%10+'0';
        num/=10;
    }
    num = timeinfo->tm_mon+1;
    for(i=6;i>4;i--){
        pTmp[i]=num%10+'0';
        num/=10;
    }

    num = timeinfo->tm_mday;
    for(i=9;i>7;i--){
        pTmp[i]=num%10+'0';
        num/=10;
    }

    num = timeinfo->tm_hour;
    for(i=12;i>10;i--){
        pTmp[i]=num%10+'0';
        num/=10;
    }

    num = timeinfo->tm_min;
    for(i=15;i>13;i--){
        pTmp[i]=num%10+'0';
        num/=10;
    }

    num = timeinfo->tm_sec;
    for(i=18;i>16;i--){
        pTmp[i]=num%10+'0';
        num/=10;
    }

    num = tv.tv_usec;
    for(i=25;i>19;i--){
        pTmp[i]=num%10+'0';
        num/=10;
    }
    return pTmp;
}

long hc_get_random_64()
{
    long ret=0;
    if(0==randomFd)
        randomFd = open("/dev/urandom", 0);
    read (randomFd, &ret, sizeof (ret));
    return ret;
}

inline char* strlen_(char *str,int*len)
{
    *len = strlen(str);
    return str;
}

inline int hc_bin_cmp(char *src,int src_len,char *dst, int dst_len)
{
    if(src_len!=dst_len || 0>src_len || 0>dst_len) return -1;
    int i=0, cur_len;
    for(;i<dst_len;){
        cur_len = dst_len-i;
        if(cur_len>8) cur_len = 8;
        switch(cur_len){
            case 8:
                if( *(uint64_t*)(src+i) != *(uint64_t*)(dst+i))
                    return -1;
                i+=8;
                break;
            case 7:
                if( src[i] != dst[i])
                    return -1;
                i++;
            case 6:
                if( *(uint16_t*)(src+i) != *(uint16_t*)(dst+i))
                    return -1;
                i+=2;
                goto code_4;
            case 5:
                if( src[i] != dst[i])
                    return -1;
                i++;
            case 4:
code_4:
                if( *(uint16_t*)(src+i) != *(uint16_t*)(dst+i))
                    return -1;
                return 0;
            case 3:
                if( src[i] != dst[i])
                    return -1;
                i++;
            case 2:
                if( *(uint16_t*)(src+i) != *(uint16_t*)(dst+i))
                    return -1;
                return 0;
            case 1:
                if( src[i] != dst[i])
                    return -1;
                return 0;
        }
    }
    return 0;
}

inline int hc_bin_casecmp(char *src,int src_len,char *dst, int dst_len)
{
    if(src_len!=dst_len) return -1;
    int i=0, cur_len, tmp;
    char c;
    uint64_t s, d;
    for(;i<dst_len;i+=8){
        cur_len = dst_len-i;
        if(cur_len>8) cur_len = 8;
        for(tmp=cur_len-1, s=0, d=0; tmp>-1; tmp--){
            c = src[i+tmp];
            if(c>='A'&&c<='Z') c|=0x20;
            s<<=8;
            s|=c;
            c = dst[i+tmp];
            if(c>='A'&&c<='Z') c|=0x20;
            d<<=8;
            d|=c;
        }
        if(s!=d) return -1;
        if(8>cur_len)
            return 0;
    }
    return 0;
}

inline int hc_mem_move(char *src,int len,int offset)
{
    int new_top=offset,i, cur_len;
    uint64_t u64;
    if(0>offset){
        for(i=0;i<len;){
            cur_len = len-i;
            if(8<cur_len) cur_len = 8;
            switch(cur_len){
                case 8:
                    u64 = *(uint64_t*)(src+i);
                    *(uint64_t*)(src+new_top) = u64;
                    i+=8;
                    new_top+=8;
                    break;
                case 7:
                    src[new_top++] = src[i++];
                case 6:
                    u64 = *(uint16_t*)(src+i);
                    *(uint16_t*)(src+new_top) = u64;
                    new_top+=2;
                    i+=2;
                    goto code_4_1;
                case 5:
                    src[new_top++] = src[i++];
                case 4:
code_4_1:
                    u64 = *(uint32_t*)(src+i);
                    *(uint32_t*)(src+new_top) = u64;
                    return 0;
                case 3:
                    src[new_top++] = src[i++];
                case 2:
                    u64 = *(uint16_t*)(src+i);
                    *(uint16_t*)(src+new_top) = u64;
                    return 0;
                case 1:
                    src[new_top] = src[i];
                    return 0;
            }
        }
    }else{
        for(i=len;i>-1;){
            cur_len = i+1;
            if(8<cur_len) cur_len = 8;
            switch(cur_len){
                case 8:
                    i-=8;
                    new_top-=8;
                    u64 = *(uint64_t*)(src+i);
                    *(uint64_t*)(src+new_top) = u64;
                    break;
                case 7:
                    src[new_top--] = src[i--];
                case 6:
                    new_top-=2;
                    i-=2;
                    u64 = *(uint16_t*)(src+i);
                    *(uint16_t*)(src+new_top) = u64;
                    goto code_4_2;
                case 5:
                    src[new_top--] = src[i--];
                case 4:
code_4_2:
                    i-=4;
                    new_top-=4;
                    u64 = *(uint32_t*)(src+i);
                    *(uint32_t*)(src+new_top) = u64;
                    return 0;
                case 3:
                    src[new_top--] = src[i--];
                case 2:
                    new_top-=2;
                    i-=2;
                    u64 = *(uint16_t*)(src+i);
                    *(uint16_t*)(src+new_top) = u64;
                    return 0;
                case 1:
                    src[new_top] = src[i];
                    return 0;
            }
        }

    }
    return 0;

}

inline int hc_utl_until(char *src,int *i,int max,char until_chr)
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

inline int hc_utl_escape(char *src,int *i,int max,char escape_chr)
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

inline int hc_utl_escape_right(char *src,int start,int *len,char escape_chr)
{
    src+=start;
    start=*len-1;
    for(;start>-1;start--){
        if(src[start]==escape_chr){
            continue;
        }
        *len = start+1;
        return 0;
    }
    *len = start+1;
    return 0;
}


int hc_json_decode(char *p,char *desc,int *i_desc)
{
    char c = *p, cn;
    char i=1,i2, bit=0, tmp=0;
    unsigned short hex=0, tmpHex=0;
    if('\\'!=c){
        *i_desc=*i_desc+1;
        *desc = *p;
        return 0;
    }
    c = p[i++];
    if('u'==c){
        for(i2=0,hex=0;i2<4;i2++){
            c = p[i++]-48;
            if(c<0||c>54)
                return -1;
            c = g_hex_to_char[c];
            if(-1==c)
                return -1;
            if(0!=hex) hex*=16;
            hex+=c;
        }
        i=0;
        if(hex<0x0080){
            *i_desc=*i_desc+1;
            *desc=hex;
            return 5;
        }
        if(hex<0x0800){
            desc[i++]= 0xc0 | hex>>6;
            desc[i++]= 0x80 | hex&0x3f;
            *i_desc=*i_desc+2;
        }
        else{
            desc[i++]= 0xe0 | hex>>12;
            desc[i++]= 0x80 | (hex>>6)&0x3f;
            desc[i++]= 0x80 | hex&0x3f;
            *i_desc=*i_desc+3;
        }
        //if(0x80000000&tmpHex){
        //    *(short*)desc=hex;
        //    *i_desc = *i_desc+2;
        //    return 5;
        //}
        //hex<<=1;
        //for(tmpHex = hex, bit=6; !(tmpHex&0xfc000000) ;tmpHex<<=5){ // 0xfc = 1111 1100 0000 0000
        //    bit--;
        //}
        //i=0;
        //switch(bit){
        //    case 6:
        //        desc[i++]=0xfc| hex>>14;
        //        break;
        //    case 5:
        //        desc[i++]=0xf8;
        //        break;
        //    case 4:
        //        desc[i++]=0xf0;
        //        break;
        //    case 3:
        //        desc[i++]=0xe0;
        //        break;
        //    case 2:
        //        desc[i++]=0xc0;
        //        break;
        //}
        return 5;
    }
    if('\\'==c){
        *desc='\\';
        *i_desc=*i_desc+1;
    }
    else if('"'==c){
        *desc='"';
        *i_desc=*i_desc+1;
    }
    return 1;
}

inline int hc_json_escape(char *pRaw,int vLen,HC_MemBuf *mb)
{
    int  i,top;
    for(i=0;i<vLen;i++){
        hc_membuf_check_size(mb,2);
        if('"'==pRaw[i]){
            mb->buf[mb->len++]='\\';
            mb->buf[mb->len++]='"';
        }
        else{
            mb->buf[mb->len++]=pRaw[i];
        }
    }
    return 0;
}

HC_Array * hc_split_trim(HC_Array *arg_arr, char *src, int src_max, char split_chr, int split_num, char trim_chr)
{
    HC_Array *arr = arg_arr;
    if(!arr){
        arr = hc_array_create(6);
    }
    int i_start, i_end, ele_len;
    int i=0, status=0, str_len;
    char c_cur;
    int ret, num=0;
code_for:
    hc_utl_escape(src,&i,src_max,trim_chr);
    for(;i<src_max;){
        for(; i<src_max;i++){
            c_cur = src[i];
            switch(status){
                case 0:
                    i_start = i;
                    status = 1;
                case 1:
                    if(c_cur==split_chr){
                        status = 2;
                        goto code_str_end;
                    }
                default:
                    break;
            }
        }
code_str_end:
        i_end = i;
        str_len = i_end - i_start;
        hc_utl_escape_right(src, i_start, &str_len, trim_chr);
        status = 0;
        i++;
        if(-1==hc_array_append_item(arr, src+i_start, str_len, 0)){
            if(!arg_arr){
                hc_array_free(arr);
            }
            return NULL;
        }
        if(split_num>0&&++num == split_num){
            return arr;
        }
        goto code_for;
    }
    return arr;
}

void* hc_membuf_check_size(HC_MemBuf *ptr, int len)
{
    int top;
    top = ptr->len+len;
    if(top>ptr->max){
        top+=top;
        void *buf = realloc(ptr->buf, top);
        if(!buf) return NULL;
        ptr->buf = buf;
        ptr->max = top;
        return buf;
    }
    return ptr->buf;
}

void* hc_membuf_check_max(HC_MemBuf *ptr, int max)
{
    if(max>ptr->max){
        max+=max;
        void *buf = realloc(ptr->buf, max);
        if(!buf) return NULL;
        ptr->buf = buf;
        ptr->max = max;
        return buf;
    }
    return ptr->buf;
}

inline int hc_membuf_init(HC_MemBuf *mem,int len)
{
    memset(mem, 0, sizeof(HC_MemBuf));
    mem->buf = malloc(len);
    mem->max = len;
    return 0;
}

HC_MemBuf *hc_membuf_create(int len)
{
    HC_MemBuf *mem = malloc(sizeof(HC_MemBuf));
    hc_membuf_init(mem,len);
    return mem;
}

void hc_membuf_free(HC_MemBuf *mem)
{
    if(mem->buf) free(mem->buf);
    free(mem);
}

void hc_jsbuf_set_header(HC_MemBuf *mem, int len_len, int len, int type)
{
    hc_membuf_check_size(mem, 6);
    HC_JsBuf *jsbuf = (HC_JsBuf*)(mem->buf+mem->len);
    jsbuf->type = type;
    jsbuf->len_len=4;
    jsbuf->len_len=len_len;
    jsbuf->len1=len;
    mem->len = mem->len + len_len + 1;
    return;
}

void hc_jsbuf_set_data(HC_MemBuf *mem, void *data, int len, int type)
{
    HC_JsBuf *jsbuf;
    uint64_t num;
    if(data){
        hc_membuf_check_size(mem, 5+len);
        jsbuf = (HC_JsBuf*)(mem->buf+ mem->len);
        if(0x100>len){
            jsbuf->len_len=1;
            jsbuf->len1=len;
            mem->len += 2;
        }else if(0x10000>len){
            jsbuf->len_len=2;
            jsbuf->len2=len;
            mem->len += 3;
        }else{
            jsbuf->len_len=4;
            jsbuf->len4=len;
            mem->len += 5;
        }
        switch(type){
            case TYPE_RAW:
                memcpy(mem->buf+mem->len, data, len);
                mem->len+=len;
                jsbuf->type=JS_TYPE_STR;
                break;
            case TYPE_NUM:
                switch(len){
                    case 1:
                        num = ((char*)data)[0];
                        break;
                    case 2:
                        num = htobe16(*(short*)data);
                        break;
                    case 4:
                        num = htobe32(*(int*)data);
                        break;
                    default:
                        num = htobe64(*(long*)data);
                        break;
                }
                jsbuf->type=JS_TYPE_NUM;
                memcpy(mem->buf+mem->len,&num,len);
                mem->len+=len;
                break;
            case TYPE_BIN:
                jsbuf->type=JS_TYPE_BIN;
                memcpy(mem->buf+mem->len,data,len);
                mem->len+=len;
                break;
            default:
                jsbuf->type=JS_TYPE_NULL;
                mem->len+=len;
                break;
        }
    }else{
        hc_membuf_check_size(mem, 1);
        mem->buf[mem->len++] = JS_TYPE_NULL;
    }
}
