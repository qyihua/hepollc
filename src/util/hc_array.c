#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "hc_array.h"
#include "hc_util.h"

extern char gHexChar[55];


HC_Array *hc_array_create(int size)
{
    if(1>size) size=32;
    HC_Array *array = calloc(1,sizeof(HC_Array));
    array->data_top = 1;
    array->max_size=size;
    array->data_max=size*30;
    array->data_buf=calloc(array->data_max,sizeof(char));
    array->data_indexs=calloc(size,sizeof(int));
    return array;
}

int hc_array_get_item_retlen(HC_Array *array,int index,void *data)
{
    char *data_buf=array->data_buf;
    int num=array->item_num;
    if(0>index||num<=index) return -1;
    
    index = array->data_indexs[index];
    if(1>index) return -1;
    
    HC_ArrayData *array_data=(HC_ArrayData*)(data_buf+index);
    data_buf = array_data->data;
    int len = array_data->len;
    char type = array_data->type;
    if(TYPE_NUM == type){
        if(4==len) *(int*)data=*(int*)data_buf;
        else if(1==len) *(char*)data=*data_buf;
        else if(2==len) *(short*)data=*(short*)data_buf;
        else *(long*)data=*(long*)data_buf;
    }
    else{
       *(char**)data = data_buf;
    }
    return len;
}

HC_ArrayData* hc_array_get_item(HC_Array *array,int index)
{
    if(!array){
        return NULL;
    }
    char *data_buf=array->data_buf, *data=NULL;
    int num=array->item_num;
    if(0>index||num<=index){
        return NULL;
    }
    
    index = array->data_indexs[index];
    if(1>index){
        return NULL;
    }
    return (HC_ArrayData*)(data_buf+index);
}

char* hc_array_get_item_retptr(HC_Array *array,int index,int *len_ptr)
{
    if(!array){
        *len_ptr=0;
        return NULL;
    }
    char *data_buf=array->data_buf, *data=NULL;
    int num=array->item_num;
    if(0>index||num<=index){
        *len_ptr=0;
        return NULL;
    }
    
    index = array->data_indexs[index];
    if(1>index){
        *len_ptr=0;
        return NULL;
    }
    
    
    HC_ArrayData *array_data=(HC_ArrayData*)(data_buf+index);
    data_buf = array_data->data;
    int len = array_data->len;
    //char type = array_data->type;
    //if(TYPE_NUM == type){
    //    if(4==len) *(long*)&data=*(int*)data_buf;
    //    else if(1==len) *(long*)&data=*data_buf;
    //    else if(2==len) *(long*)&data=*(short*)data_buf;
    //    else *(long*)&data=*(long*)data_buf;
    //}
    //else{
       data = data_buf;
    //}
    *len_ptr=len;
    return data;
}

int hc_array_set_item(HC_Array *array,int index,void* data,int data_len,short type)
{
    int size = array->max_size,num=array->item_num, tmp;
    int *pIndex=NULL;
    HC_ArrayData *array_data=NULL;
    if(-1==index) index=num;
    else if(index > num){
        num = index+1;
        if(num>=size){
            size = num*2;
            tmp = size*sizeof(int);
            array->data_indexs = realloc(array->data_indexs, tmp);
            memset(array->data_indexs+array->max_size,0,tmp - sizeof(int)*array->max_size);
            array->max_size=size;
        }
        array->item_num=num;
    }

    if(index==num){
        num++;
        if(num>size-10){
            size = num+num+20;
            tmp = size*sizeof(int);
            array->data_indexs = realloc(array->data_indexs, tmp);
            memset(array->data_indexs+array->max_size,0, tmp - sizeof(int)*array->max_size);
            array->max_size=size;
        }
        array->item_num=num;
    }
    if(0>data_len)
        data_len = strlen((char*)data);
    pIndex = array->data_indexs+index;
    int top=*pIndex;
    
    array_data = (HC_ArrayData*)(array->data_buf+top);
    char *data_buf=array_data->data;
    if(1>top || array_data->max_len <data_len ){
        top=array->data_top;
        if(top+data_len+sizeof(HC_ArrayData)>array->data_max){
            array->data_max=2*(top+data_len+sizeof(HC_ArrayData));
            array->data_buf = realloc(array->data_buf,array->data_max);
        }
        array_data = (HC_ArrayData*)(array->data_buf+top);
        *pIndex=top;
        top+=sizeof(HC_ArrayData);
        data_buf=array_data->data;
        array_data->type=type;
        array_data->type2=type>>8;
        if(array_data->type==TYPE_NUM){
            array_data->len=data_len;
            switch(data_len){
                case 4:
                    *(int*)data_buf = (long)data;
                    break;
                case 8:
                    *(long*)data_buf = (long)data;
                    break;
                case 2:
                    *(short int*)data_buf = (long)data;
                    break;
                case 1:
                    data_buf[0] = (long)data;
                    break;
            }
        }else if(data){
            memcpy(data_buf,(char*)data,data_len);
            array_data->len=data_len;
        }else{
            array_data->len=0;
        }
        array_data->max_len=data_len;
        array_data->status=0;
        top+=data_len;
        data_buf[data_len]='\0';
        top++;
        array->data_top=top;
        if(top>array->data_max/2){
            array->data_max=2*array->data_max;
            array->data_buf = realloc(array->data_buf,array->data_max);
        }
    }
    else{
        if(data){
            memcpy(data_buf,(char*)data,data_len);
            array_data->len=data_len;
        }else
            array_data->len=0;
        data_buf[data_len]='\0';
    }
    return *pIndex;
}

int hc_array_append_item(HC_Array *array,void *data,int data_len,short type)
{
    int size = array->max_size,num=array->item_num;
    int index=num, tmp;
    num++;
    HC_ArrayData *array_data;
    void *ptr;
    if(num>size-10){
        size = num+num+20;
        tmp = size*sizeof(int);
        ptr = realloc(array->data_indexs,tmp);
        if(!ptr) return -1;
        array->data_indexs = ptr;
        memset(array->data_indexs+array->max_size,0,tmp - sizeof(int)*array->max_size);
        array->max_size=size;
    }
    array->item_num=num;
    int top=array->data_top;
    if(-1==data_len)
        data_len = strlen((char*)data);
    if(top+data_len+sizeof(HC_ArrayData)>(uint64_t)array->data_max){
        array->data_max=2*(top+data_len+sizeof(HC_ArrayData));
        ptr = realloc(array->data_buf,array->data_max);
        if(!ptr) return -1;
        array->data_buf = ptr;
    }
    char *data_buf=array->data_buf;
    array_data = (HC_ArrayData*)(data_buf+top);
    array->data_indexs[index]=top;
    top+=sizeof(HC_ArrayData);
    data_buf += top;
    array_data->type=type;
    array_data->type2=type>>8;
    array_data->max_len=data_len;
    if(array_data->type==TYPE_NUM){
        array_data->len=data_len;
        switch(data_len){
            case 4:
                *(int*)data_buf = (long)data;
                break;
            case 8:
                *(long*)data_buf = (long)data;
                break;
            case 2:
                *(short int*)data_buf = (long)data;
                break;
            case 1:
                data_buf[0] = (long)data;
                break;
        }
    }else if(data){
        array_data->len=data_len;
        memcpy(data_buf,(char*)data,data_len);
    }else
        array_data->len=0;
    top+=data_len;
    data_buf[data_len]='\0';
    top++;
    array->data_top=top;
    if(top>array->data_max/2){
        array->data_max=top+top+array->data_max;
        ptr = realloc(array->data_buf,array->data_max);
        if(!ptr) return -1;
        array->data_buf = ptr;
    }
    return index;
}

HC_ArrayData* hc_array_append_data(HC_Array *array,int index,void *data,int data_len,short type)
{
    int size = array->max_size,num=array->item_num,tmp;
    if(-1==index) index=num;
    else if(-2==index) index=num-1;
    else if(index > num){
        num = index+1;
        if(num>=size){
            size = num*2;
            tmp = size*sizeof(int);
            array->data_indexs = realloc(array->data_indexs, tmp);
            memset(array->data_indexs+array->max_size,0,tmp - sizeof(int)*array->max_size);
            array->max_size=size;
        }
        array->item_num=num;
    }
    if(index==num){
        num++;
        if(num>=size){
            size = num+num;
            tmp = size*sizeof(int);
            array->data_indexs = realloc(array->data_indexs, tmp);
            memset(array->data_indexs+array->max_size,0, tmp - sizeof(int)*array->max_size);
            array->max_size=size;
        }
        array->item_num=num;
    }
    HC_ArrayData *array_data, *pAH_tmp;

    if(-1==data_len)
        data_len = strlen((char*)data);
    int *pIndex = array->data_indexs+index;
    int top=*pIndex,topTmp;
    char isLast=0, needCopy=0;

    array_data = (HC_ArrayData*)(array->data_buf+top);
    char *data_buf=NULL;
    if(0<top){
        topTmp = top+array_data->len;
        if(top+sizeof(HC_ArrayData)+array_data->max_len+1==array->data_top)
            isLast=1;
        else if(0!=array_data->len)
            needCopy=1;
    }
    if( 1>top || array_data->max_len-array_data->len < data_len ){
        if(isLast){
            top = top + sizeof(HC_ArrayData)+array_data->len;
            if(top+data_len>=array->data_max){
                array->data_max=2*(top+data_len);
                array->data_buf = realloc(array->data_buf,array->data_max);
            }
            array_data = (HC_ArrayData*)(array->data_buf+*pIndex);
            if(data){
                memcpy(array->data_buf+top, (char*)data, data_len);
                top+=data_len;
                array_data->len+=data_len;
                array->data_buf[top]='\0';
                if(array_data->len>array_data->max_len){
                    array_data->max_len=array_data->len;
                    array->data_top = *pIndex+sizeof(HC_ArrayData)+array_data->max_len+1;
                }
            }else{
                if(array_data->len+data_len>array_data->max_len){
                    array_data->max_len=array_data->len+data_len;
                    array->data_top = *pIndex+sizeof(HC_ArrayData)+array_data->max_len+1;
                }
            }
            array_data->type=type;
            array_data->type2=type>>8;

        }
        else{
            top = array->data_top;
            if(!needCopy){
                if(top+data_len+sizeof(HC_ArrayData)>=array->data_max){
                    array->data_max=2*(top+data_len+sizeof(HC_ArrayData));
                    array->data_buf = realloc(array->data_buf,array->data_max);
                }
                array_data = (HC_ArrayData*)(array->data_buf+top);
                *pIndex=top;
                top+=sizeof(HC_ArrayData);
                data_buf=array->data_buf+top;
                array_data->type=type;
                array_data->type2=type>>8;
                if(data){
                    memcpy(data_buf,(char*)data,data_len);
                    array_data->len=data_len;
                }else{
                    array_data->len=0;
                }
                array_data->max_len=data_len;
                array_data->status=0;
                top+=data_len;
                data_buf[data_len]='\0';
                top++;
                array->data_top=top;
            }
            else{
                topTmp = *pIndex;
                if(top+data_len+sizeof(HC_ArrayData)+array_data->len>=array->data_max){
                    array->data_max=2*(top+data_len+sizeof(HC_ArrayData)+array_data->len);
                    array->data_buf = realloc(array->data_buf,array->data_max);
                    pAH_tmp = (HC_ArrayData*)(array->data_buf+topTmp);
                }else
                    pAH_tmp = array_data;
                array_data = (HC_ArrayData*)(array->data_buf+top);
                data_buf=array->data_buf+top;
                memcpy(data_buf,array->data_buf+topTmp, pAH_tmp->len+1+sizeof(HC_ArrayData));
                *pIndex=top;
                top+=sizeof(HC_ArrayData);
                //array_data->type=type;
                //array_data->len=pAH_tmp->len;
                if(data){
                    data_buf=data_buf+sizeof(HC_ArrayData)+pAH_tmp->len;
                    memcpy(data_buf,(char*)data,data_len);
                    array_data->len+=data_len;
                    array_data->max_len = array_data->len;
                }else{
                    array_data->max_len = array_data->len+data_len;
                }
            }
        }
    }
    else{
        data_buf=array_data->data;
        if(data){
            memcpy(data_buf+array_data->len,(char*)data,data_len);
            array_data->len+=data_len;
        }
        data_buf[array_data->len+data_len]='\0';
    }
    return array_data;
}

int hc_array_set_item_as_num(HC_Array *array,int index,long data,int data_len)
{
    int size = array->max_size,num=array->item_num, tmp;
    if(-1==index) index=num;
    else if(index > num){
        num = index+1;
        if(num>=size){
            size = num*2;
            tmp = size*sizeof(int);
            array->data_indexs = realloc(array->data_indexs,tmp);
            memset(array->data_indexs+array->max_size,0,tmp - sizeof(int)*array->max_size);
            array->max_size=size;
        }
        array->item_num=num;
    }

    if(index==num){
        num++;
        if(num>size-10){
            size=num+num+20;
            tmp = size*sizeof(int);
            array->data_indexs = realloc(array->data_indexs,tmp);
            memset(array->data_indexs+array->max_size,0,tmp - sizeof(int)*array->max_size);
            array->max_size=size;
        }
        array->item_num=num;
    }

    int *pIndex = array->data_indexs+index;
    int top=*pIndex;
    HC_ArrayData *array_data=(HC_ArrayData*)(array->data_buf+top);
    char *data_buf=NULL;
    if(1>top || array_data->max_len <data_len ){
        top=array->data_top;
        if(top+data_len+sizeof(HC_ArrayData)>array->data_max){
            array->data_max=2*(top+data_len+sizeof(HC_ArrayData));
            array->data_buf = realloc(array->data_buf,array->data_max);
        }
        array_data = (HC_ArrayData*)(array->data_buf+top);
        array->data_indexs[index]=top;
        top+=sizeof(HC_ArrayData);
        data_buf=array->data_buf+top;
        array_data->type = TYPE_NUM;
        if(sizeof(int)==data_len)
            *(int*)data_buf=data;
        else if(1==data_len)
            data_buf[0]=data;
        else if(sizeof(short)==data_len)
            *(short*)data_buf=data;
        else if(sizeof(long)==data_len)
            *(long*)data_buf=data;
        else
            return -1;
        array_data->max_len=data_len;
        array_data->len=data_len;
        array_data->status=0;
        top+=data_len;
        data_buf[data_len]='\0';
        top++;
        array->data_top=top;
        if(top>array->data_max/2){
            array->data_max=2*array->data_max;
            array->data_buf = realloc(array->data_buf,array->data_max);
        }
    }
    else{
        data_buf=array_data->data;
        array_data->type = TYPE_NUM;
        if(sizeof(int)==data_len)
            *(int*)data_buf=data;
        else if(1==data_len)
            data_buf[0]=data;
        else if(sizeof(short)==data_len)
            *(short*)data_buf=data;
        else if(sizeof(long)==data_len)
            *(long*)data_buf=data;
        else
            return -1;
        array_data->len=data_len;
        data_buf[data_len]='\0';
    }
    return 0;
}

int hc_array_append_item_as_num(HC_Array *array,long data,int data_len)
{
    int size = array->max_size,num=array->item_num;
    int index=num, tmp;
    num++;
    if(num>size-10){
        size=num+num+20;
        tmp = sizeof(int)*size;
        array->data_indexs = realloc(array->data_indexs,tmp);
        memset(array->data_indexs+array->max_size,0,tmp - 4*array->max_size);
        array->max_size=size;
    }
    array->item_num=num;

    int top=array->data_top;
    if(top+data_len+sizeof(HC_ArrayData)>array->data_max){
        array->data_max=2*(top+data_len+sizeof(HC_ArrayData));
        array->data_buf = realloc(array->data_buf,array->data_max);
    }
    unsigned char *data_buf=array->data_buf;
    HC_ArrayData *array_data = (HC_ArrayData*)(data_buf+top);
    array->data_indexs[index]=top;
    top+=sizeof(HC_ArrayData);
    array_data->type = TYPE_NUM;
    data_buf += top;
    if(sizeof(int)==data_len)
        *(int*)data_buf=data;
    else if(1==data_len)
        data_buf[0]=data;
    else if(sizeof(short)==data_len)
        *(short*)data_buf=data;
    else if(sizeof(long)==data_len)
        *(long*)data_buf=data;
    else
        return -1;
    array_data->len=data_len;
    array_data->max_len=data_len;
    top+=data_len;
    data_buf[data_len]='\0';
    top++;
    array->data_top=top;
    if(top>array->data_max/2){
        array->data_max=2*array->data_max;
        array->data_buf = realloc(array->data_buf,array->data_max);
    }
    return 0;
}

int hc_array_free(HC_Array *array)
{
    free(array->data_indexs);
    free(array->data_buf);
    free(array);
    return 0;
}

HC_MemBuf* hc_array_to_json(HC_Array *array,HC_MemBuf *mem,int a_start,int a_end)
{
    if(!array){
        return NULL;
    }
    int num=array->item_num, maxSize=array->data_top, i, index,
        *pIndex = array->data_indexs, dLen;
    long n=0;
    char *pbuf = NULL, *data_buf=array->data_buf;
    i=a_start;
    if(0<a_end){num=a_end;}
    if(!mem){
        mem = hc_membuf_create(maxSize);
    }else{
        hc_membuf_check_size(mem, maxSize);
    }
    mem->cur_len = mem->len;

    mem->buf[mem->len++]='[';
    HC_ArrayData *array_data=NULL;
    char *pTmp=NULL;
    int ret;
    for(;i<num;i++){
        index = pIndex[i];
        if(1>index){
            hc_membuf_check_size(mem, 5);
            memcpy(mem->buf+mem->len,"null,",5);
            mem->len+=5;
            continue;
        }
        array_data=(HC_ArrayData*)(data_buf+index);
        pTmp = array_data->data;
        dLen = array_data->len;
        if(TYPE_RAW==array_data->type){
            hc_membuf_check_size(mem, dLen+1);
            mem->buf[mem->len++]='"';
            hc_json_escape(pTmp,dLen,mem);
            hc_membuf_check_size(mem, dLen+1);
            mem->buf[mem->len++]='"';
        }else if(TYPE_BIN==array_data->type){
            hc_membuf_check_size(mem,dLen); 
            memcpy(mem->buf+mem->len,pTmp,dLen);
            mem->len+=dLen;
        }else if(TYPE_NUM==array_data->type){
            switch(dLen){
                case 1:
                    n=pTmp[0];
                    break;
                case 2:
                    n=*(short*)pTmp;
                    break;
                case 4:
                    n=*(int*)pTmp;
                    break;
                default:
                    n=*(long*)pTmp;
                    break;
            }
            pTmp=hc_num_to_str(n,10,&ret,NULL);
            hc_membuf_check_size(mem, ret);
            memcpy(mem->buf+mem->len,pTmp,ret);
            mem->len+=ret;
        }
        hc_membuf_check_size(mem, 1);
        mem->buf[mem->len++]=',';
    }
    if(num>0){
        mem->buf[--mem->len]=']';
        mem->buf[++mem->len]='\0';
    }else{
        mem->buf[mem->len++]=']';
        mem->buf[mem->len]='\0';
    }
    mem->cur_len = mem->len-mem->cur_len;
    return mem;
}

char* hc_array_to_str(HC_Array *array,int *a_len,char **a_pStr,int *a_maxSize,int a_start,int a_end)
{
    if(!array){
        *a_len=0;
        return NULL;
    }
    int num=array->item_num, maxSize=array->data_top, top=1, i, index,
        *pIndex = array->data_indexs, n=0,dLen;
    char *pStr = NULL, *data_buf=array->data_buf;
    i=a_start;
    if(0<a_end){num=a_end;}
    if(!a_pStr){
        pStr = malloc(maxSize);
        top=0;
    }else{
        pStr = *a_pStr;
        if(*a_len+maxSize>*a_maxSize){
            maxSize = *a_len+maxSize;
            pStr = realloc(pStr,maxSize);
        }
        top=*a_len;
    }

    pStr[top++]='[';
    HC_ArrayData *array_data=NULL;
    char *pTmp=NULL;
    for(;i<num;i++){
        index = pIndex[i];
        if(1>index){
            memcpy(pStr+top,"null,",5);
            top+=5;
            continue;
        }
        
        array_data=(HC_ArrayData*)(data_buf+index);
        pTmp=array_data->data;
        dLen = array_data->len;
        if(TYPE_RAW==array_data->type){
            if(maxSize<=dLen+top){
                maxSize = dLen+dLen+top+top;
                pStr = realloc(pStr, maxSize);
            }
            memcpy(pStr,pTmp,dLen);
            top+=dLen;
        }else if(TYPE_NUM==array_data->type){
            switch(dLen){
                case 1:
                    n=data_buf[index];
                    break;
                case 2:
                    n=*(short*)pTmp;
                    break;
                case 4:
                    n=*(int*)pTmp;
                    break;
                default:
                    n=*(long*)pTmp;
                    break;
            }
            pTmp=hc_num_to_str(n,10,&n,NULL);
            if(maxSize<=n+top){
                maxSize = n+n+top+top;
                pStr = realloc(pStr, maxSize);
            }
            memcpy(pStr,pTmp,n);
            top+=n;
        }
        pStr[top++]=',';
    }
    pStr[--top]=']';
    pStr[++top]='\0';
    top--;
    if(a_pStr){
        *a_pStr=pStr;
        if(maxSize>*a_maxSize) *a_maxSize=maxSize;
        dLen = top-*a_len;
        *a_len=top;
        return (char*)(long)dLen;
    }
    *a_len = top;
    return pStr;
}

int hc_array_reset_to_zero(HC_Array *arr)
{
    memset(arr->data_indexs, 0, arr->item_num*sizeof(int));
    arr->item_num = 0;
    arr->data_top = 1;
    return 0;
}

HC_MemBuf* hc_array_to_jsbuf(HC_Array *array, HC_MemBuf *mem, 
        int start, int slice_len, uint8_t *indexs)
{
    if(!mem){
        mem = hc_membuf_create(6);
    }
    char *ptmp;
    HC_ArrayData *arrdata;
    int cur_len = mem->len;
    hc_jsbuf_set_header(mem, 4, 0, JS_TYPE_ARRAY);
    HC_JsBuf *jsbuf;
    int i=0,max=array->item_num;
    if(indexs){
        max = indexs[0];
        int index_i=1;
        start = array->start_index;
        for( ; index_i<max ; index_i++){
            i = indexs[index_i] + start;
            arrdata = hc_array_get_item(array,i);
            if(!arrdata){
                hc_membuf_check_size(mem, 1);
                mem->buf[mem->len++] = JS_TYPE_NULL;
                continue;
            }

            hc_jsbuf_set_data(mem, arrdata->data, arrdata->len, arrdata->type);
        }

    }else{
        i = start+array->start_index;
        if(0<slice_len) max = i+slice_len;
        else max = array->item_num;
        for(;i<max;i++){
            arrdata = hc_array_get_item(array,i);
            if(!arrdata){
                hc_membuf_check_size(mem, 1);
                mem->buf[mem->len++] = JS_TYPE_NULL;
                continue;
            }
            hc_jsbuf_set_data(mem, arrdata->data, arrdata->len, arrdata->type);
        }
    }
    mem->cur_len = mem->len - cur_len;
    jsbuf = (HC_JsBuf*)(mem->buf+ cur_len);
    jsbuf->len4 = mem->cur_len - 5;
    return mem;
}

