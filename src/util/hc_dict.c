#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>

#include "hc_dict.h"
#include "hc_util.h"

extern char gHexChar[55];

static int hc_dict_key_to_index(HC_Dict *dict,const char *str,int len)

{
    short int hex=0, hex_cur=0, size=dict->item_max_size;
    uint16_t cur=0;
    int top=0, bit=dict->bit;
    if(-1==len) len=strlen(str);
    for(top=0;top<len;){
        if(1==len) cur=str[0];
        else cur = *(short int*)(str+top);
        for(hex_cur = 0;cur;){
            hex_cur += cur;
            cur>>=bit;
        }
        top+=2;
        if(len-1==top) top-=1;
        hex_cur = hex_cur+bit+top; 
        hex ^= hex_cur;
    }
    cur = hex&(size-1);
    return cur;
}


HC_Dict *hc_dict_create(uint8_t type,uint32_t size_a)
{
    uint32_t size=0b100000,data_size=0;
    uint8_t bit=5;
    if(size<size_a){
        size_a>>=bit;
        for(;size_a;){
            size<<=1;
            size_a>>=1;
            bit++;
        }
    }
    HC_Dict *dict = calloc(1,sizeof(HC_Dict));
    dict->type=type;
    dict->item_max_size = size;
    dict->items=(HC_DictItem*)calloc(size,sizeof(HC_DictItem));
    dict->nxt_items=(HC_DictItem*)calloc(size,sizeof(HC_DictItem));
    data_size = size*20;
    if(TYPE_RAW==type){
        hc_membuf_init(&dict->data_mem, data_size);
    }
    hc_membuf_init(&dict->key_mem, data_size);
    dict->nxt_link_top=1;
    dict->bit=bit;
    dict->key_to_index=(void*)&hc_dict_key_to_index;
    return dict;
}

HC_DictItem* hc_dict_has(HC_Dict *dict,const char *str,int len)
{
    int (* key_to_index)(HC_Dict*,const char*,int);
    int index,key_index, nxt_item_index;
    HC_DictItem *item = dict->items;
    HC_MemBuf *key_buf=&dict->key_mem;
    if(-1==len) len=strlen(str);
    if(0==len) return NULL;
    key_to_index = (int (*)(HC_Dict*,const char*,int))dict->key_to_index;

    index=key_to_index(dict,str,len);
    if(-1==index) return NULL;
    item+=index;
    key_index = item->key_index;
    char *tmp_ptr = key_buf->buf;
    HC_DictKey *key=(HC_DictKey*)(tmp_ptr+key_index)-1;
    if(key_index){
        if(len!=key->len) return NULL;
        if(1==key->status) return NULL;
        if(0==memcmp(str,key->data,len)){
            return item;
        }
        else{
            nxt_item_index = item->nxt_item_index;
            for(;nxt_item_index;){
                item = dict->nxt_items + nxt_item_index;
                key_index = item->key_index;
                key=(HC_DictKey*)(tmp_ptr+key_index)-1;
                if(1==key->status) return NULL;
                if(0==memcmp(str,key->data,len)){
                    return item;
                }
                nxt_item_index = item->nxt_item_index;
            }
        }
    }
    return NULL;
}

HC_Array* hc_dict_get_array(HC_Dict *dict, char *str, int len)
{
    int ret;
    HC_DictData *ddata = (HC_DictData*)hc_dict_get_ptr(dict,str,len,&ret);
    if(!ddata) return NULL;
    HC_Array *arr = (HC_Array*)(*(uint64_t*)ddata);
    ddata -= 1;
    if(ddata->type!=TYPE_LIST) return NULL;
    return arr;
}

HC_DictItem* hc_dict_get_item(HC_Dict *dict,const char *str,int len)
{
    int (* key_to_index)(HC_Dict*,const char*,int);
    int index,key_index,nxt_item_index;
    HC_DictItem *item=dict->items;
    HC_MemBuf *key_buf=&dict->key_mem;
    if(-1==len) len=strlen(str);
    key_to_index=(int(*)(HC_Dict*,const char*,int))dict->key_to_index;

    index=key_to_index(dict,str,len);
    if(-1==index) return NULL;
    item+=index;
    key_index = item->key_index;
    HC_DictKey *dict_key; 
    char *ptr = key_buf->buf;
    if(key_index){
      //  ret = pKey[key_index-3];
      //  if(!ret) ret=1;
        dict_key = (HC_DictKey*)(ptr+key_index)-1;
        if(dict_key->len == len && 0==memcmp(str,ptr+key_index,len)){
            if(1==dict_key->status) return NULL;
            return item;
        }
        else{
            nxt_item_index=item->nxt_item_index;
            for(;nxt_item_index;){
                item = dict->nxt_items + nxt_item_index;
                key_index = item->key_index;
                dict_key = (HC_DictKey*)(ptr+key_index)-1;
                if(dict_key->len==len && 0==memcmp(str,ptr+key_index,len)){
                    if(1==dict_key->status) return NULL;
                    return item;
                }
                nxt_item_index = item->nxt_item_index;
            }
        }
    }
    return NULL;
}


//ret -1:not found, -2:vType error
long hc_dict_get_num(HC_Dict *dict,const char *str,int len,int *ret)
{
    int (* key_to_index)(HC_Dict*,const char*,int);
    int index,key_index,nxt_item_index;
    HC_DictItem *item=dict->items;
    HC_MemBuf *key_buf=&dict->key_mem;
    char *key_data = key_buf->buf, has=0,type=dict->type;
    *ret = -1;
    if(-1==len) len=strlen(str);
    key_to_index=(int(*)(HC_Dict*,const char*,int))dict->key_to_index;

    index=key_to_index(dict,str,len);
    if(-1==index){
        return -1;
    }
    item+=index;
    key_index = item->key_index;
    
    HC_DictKey *dict_key; 
    if(key_index){
        dict_key = (HC_DictKey*)(key_data+key_index)-1;
        if(dict_key->len==len && 0==memcmp(str,key_data+key_index,len)){
            if(1==dict_key->status){return -1;}
            has=1;
        }
        else{
            nxt_item_index=item->nxt_item_index;
            for(;nxt_item_index;){
                item=dict->nxt_items+nxt_item_index;
                key_index = item->key_index;
                dict_key = (HC_DictKey*)(key_data+key_index)-1;
                if(dict_key->len==len && 0==memcmp(str,key_data+key_index,len)){
                    if(1==dict_key->status){return -1;}
                    has=1;
                    break;
                }
                nxt_item_index = item->nxt_item_index;
            }
            if(!has){
                return -1;
            }
        }
    }
    else{
        return -1;
    }
    if(TYPE_NUM==type){
        *ret=8;
        return item->data;
    }else{
        index = item->data;
        if(!index){return -1;}
        key_data = dict->data_mem.buf;
        *ret=((HC_DictData*)(key_data+index)-1)->len;
        if(TYPE_NUM==((HC_DictData*)(key_data+index)-1)->type){
        switch(*ret){
                case 1:
                    return key_data[index];
                case 2:
                    return *(short*)(key_data+index);
                case 4:
                    return *(int*)(key_data+index);
                case 8:
                    return *(long*)(key_data+index);
            }
        }
    }
    *ret = -1;
    return -1;
}

//ret -1:not found, -2:vType error
char *hc_dict_get_ptr(HC_Dict *dict,const char *str,int len,int *ret)
{
    int (* key_to_index)(HC_Dict*,const char*,int);
    int index,key_index,nxt_item_index;
    HC_DictItem *item=dict->items;
    HC_MemBuf *key_buf=&dict->key_mem, *data_buf = &dict->data_mem;
    char *key_data=key_buf->buf, has=0,type=dict->type,*data_ptr = data_buf->buf;
    *ret = 0;
    if(-1==len) len=strlen(str);

    key_to_index=(int(*)(HC_Dict*,const char*,int))dict->key_to_index;

    index=key_to_index(dict,str,len);
    if(-1==index) return NULL;
    item += index;
    key_index = item->key_index;
    
    HC_DictKey *dict_key; 
    if(key_index){
        dict_key = (HC_DictKey*)(key_data+key_index)-1;
        if(dict_key->len==len && 0==memcmp(str,key_data+key_index,len)){
            if(1==dict_key->status) return NULL;
            has=1;
        }
        else{
            nxt_item_index=item->nxt_item_index;
            for(;nxt_item_index;){
                item=dict->nxt_items + nxt_item_index;
                key_index = item->key_index;
                dict_key = (HC_DictKey*)(key_data+key_index)-1;
                if(dict_key->len==len && 0==memcmp(str,key_data+key_index,len)){
                    has=1;
                    break;
                }
                nxt_item_index = item->nxt_item_index;
            }
            if(!has){
                return NULL;
            }
        }
    }
    else
        return NULL;
    HC_DictData *data=NULL;
    if(TYPE_NUM==type){
        return NULL;
    }
    else{
        index = item->data_index;
        if(!index) return NULL;
        data = (HC_DictData*)(data_ptr+index)-1;
        *ret=data->len; //dataLen
        return data_ptr+index;
    }
}

int hc_dict_get_data_len(HC_Dict *dict,const char *str,int len)
{
    int (* key_to_index)(HC_Dict*,const char*,int);
    int index,key_index,nxt_item_index,ret;
    HC_DictItem *item=dict->items;
    HC_MemBuf *key_buf=&dict->key_mem, *data_buf = &dict->data_mem;
    char *key_data=key_buf->buf, has=0,type=dict->type,*data_ptr=data_buf->buf;
    if(-1==len) len=strlen(str);
    if(0==len) return -1;
    key_to_index=(int(*)(HC_Dict*,const char*,int))dict->key_to_index;

    index=key_to_index(dict,str,len);
    if(-1==index) return -1;
    item+=index;
    key_index = item->key_index;
    HC_DictKey *dict_key; 
    if(key_index){
        dict_key = (HC_DictKey*)(key_data+key_index)-1;
        if(dict_key->len==len && 0==memcmp(str,key_data+key_index,len)){
            if(1==dict_key->status) return -1;
            has=1;
        }
        else{
            nxt_item_index=item->nxt_item_index;
            for(;nxt_item_index;){
                item=dict->nxt_items+nxt_item_index;
                key_index = item->key_index;
                dict_key = (HC_DictKey*)(key_data+key_index)-1;
                if(dict_key->len==len && 0==memcmp(str,key_data+key_index,len)){
                    has=1;
                    break;
                }
                nxt_item_index = item->nxt_item_index;
            }
            if(!has){
                return -1;
            }
        }
    }
    else
        return -1;
    if(0==type){
        return sizeof(long);
    }
    else{
        index = item->data;
        if(!index) return 0;
        ret=((HC_DictData*)(data_ptr+index)-1)->len; //dataLen
    }
    return ret;
}

HC_DictItem * hc_dict_set_item_by_num(HC_Dict *dict,uint64_t key,int len,void* data,int data_len,char data_type)
{
     return hc_dict_set_item(dict,(char*)&key,len,data,data_len,data_type);
}

HC_DictItem * hc_dict_set_item(HC_Dict *dict,const char *str,int len,void* data,int data_len,char data_type)
{ 
    int (* key_to_index)(HC_Dict*,const char*,int),  item_index=0;
    uint32_t size;
    int index,key_index,nxt_item_index,key_top,data_top ;
    int tmp;
    HC_DictItem *item=dict->items, *nxt_items, *last_item;
    HC_MemBuf *key_buf=&dict->key_mem, *data_buf = &dict->data_mem;
    char *key_ptr=key_buf->buf, has=0,*data_ptr=data_buf->buf,flg_empty=0,flg_nxt=0;
    HC_DictKey *dict_key=NULL;
    HC_DictData *dict_data=NULL;
    if(0>len) len=strlen(str);
    key_to_index=(int(*)(HC_Dict*,const char*,int))dict->key_to_index;
    size = dict->item_max_size;

    index=key_to_index(dict,str,len);
    if(-1==index){
        return NULL;
    }
    item+=index;
    key_index = item->key_index;
    
    key_top = key_buf->len;
    nxt_item_index=item->nxt_item_index;
    item_index = index;
    if(nxt_item_index || key_index){
        dict_key = (HC_DictKey*)(key_ptr + key_index)-1;
        if(key_index&& dict_key->len==len && 0==memcmp(str, key_ptr+key_index, len))
            has=1;
        else{
            last_item = item;
            if(0==key_index){
                flg_empty = 1;
            }
            for(;nxt_item_index;){
                nxt_items=dict->nxt_items+nxt_item_index;
                last_item = nxt_items;
                key_index = nxt_items->key_index;
                if(0==key_index&&0==flg_empty){ item_index=nxt_item_index; flg_empty=-1;flg_nxt=1;}
                dict_key = (HC_DictKey*)(key_ptr+key_index)-1;
                if(dict_key->len==len && 0==memcmp(str,key_ptr+key_index,len)){
                    has=1;
                    item = nxt_items;
                    break;
                }
                nxt_item_index = nxt_items->nxt_item_index;
            }
            if(!has){
                if(flg_empty){
                    if(-1==flg_empty){
                        item = dict->nxt_items+item_index;
                    }
                }
                else{
                    flg_nxt = 1;
                    last_item->nxt_item_index = dict->nxt_link_top;
                    item_index = last_item->nxt_item_index;
                    dict->nxt_link_top++;
                    item = dict->nxt_items+last_item->nxt_item_index;
                }
            }
        }
    }

    if(!has){
        if(dict->item_num >= size && !dict->flg_fix){
            return NULL;
        }

        tmp = len+1+sizeof(HC_DictKey)+key_buf->len;
        if(tmp > (int)key_buf->max ){
            if(dict->flg_fix){
                key_top = 0;
            }else{
                tmp+=tmp;
                key_ptr = hc_membuf_check_size(key_buf,tmp-key_buf->max);
            }
        }
        dict_key = (HC_DictKey*)(key_ptr+key_top);
        key_top+=sizeof(HC_DictKey);
        item->key_index = key_top;
        dict_key->item_index = item_index;
        // 4:itemindex 1:cumstom 1:1=vtype 1:1=pitem,-1=pnitem,0=del 1:len key 0
        dict_key->type=data_type;
        dict_key->flg_nxt=flg_nxt;
        dict_key->len=len;
        dict_key->status=0;
        dict_key->no_format=0;
        memcpy(key_ptr+key_top, str, len);
        key_top += len;
        key_ptr[key_top++]='\0';

        key_buf->len = key_top;
    }
    else{
        ((HC_DictKey*)(key_ptr+key_index)-1)->type=data_type;
    }
    if(TYPE_NUM==dict->type){
        item->data = (uint64_t)data;
    }
    //else if(data||TYPE_NUM==data_type){
    else{
        data_top = item->data_index;
        if(0>data_len&&data) data_len = strlen((char*)data);
        
        if(0<data_len||data){ 
            dict_data = (HC_DictData*)(data_ptr+data_top)-1;
            if(1>data_top || dict_data->max_len<(unsigned int)data_len){
                data_top = data_buf->len;
                tmp = data_len + data_top +sizeof(HC_DictData)+1;
                if(tmp > data_buf->max){
                    if(dict->flg_fix){
                        data_top = 0;
                    }else{
                        tmp+=tmp;
                        data_ptr = hc_membuf_check_size(data_buf,tmp-data_buf->len);
                    }
                }
                item->data_index = data_top+sizeof(HC_DictData);
                dict_data = (HC_DictData*)(data_ptr+data_top);
                dict_data->max_len = data_len;
                dict_data->type=data_type;
                data_top+=sizeof(HC_DictData);
                if(TYPE_NUM==data_type){
                    dict_data->len=data_len;
                    switch(data_len){
                        case 1:
                            data_ptr[data_top]=(long)data;
                            break;
                        case sizeof(short):
                            *(short*)(data_ptr + data_top)=(long)data;
                            break;
                        case sizeof(int):
                            *(int*)(data_ptr + data_top)=(long)data;
                            break;
                        case sizeof(long):
                            *(long*)(data_ptr + data_top)=(long)data;
                            break;
                        default:
                            return NULL;
                    }
                }else if(TYPE_LIST == data_type){
                    *(long*)(data_ptr + data_top)=(long)data;
                }else if(data){
                    dict_data->len=data_len;
                    memcpy(data_ptr+data_top,(char*)data,data_len);
                }else
                    dict_data->len=0;
                data_top+=data_len;
                data_ptr[data_top++]='\0';
                data_buf->len = data_top;
            }
            else{
code_has_data_index:
                if(TYPE_NUM==data_type){
                    dict_data->len=data_len;
                    switch(data_len){
                        case 1:
                            data_ptr[data_top]=(long)data;
                            break;
                        case sizeof(short):
                            *(short*)(data_ptr+data_top)=(long)data;
                            break;
                        case sizeof(int):
                            *(int*)(data_ptr+data_top)=(long)data;
                            break;
                        case sizeof(long):
                            *(long*)(data_ptr+data_top)=(long)data;
                            break;
                        default:
                            return NULL;
                    }
                }else if(TYPE_LIST == data_type){
                    *(long*)(data_ptr + data_top)=(long)data;
                }else if(data){
                    dict_data->len=data_len;
                    memcpy(data_ptr+data_top,(char*)data,data_len);
                }else
                    dict_data->len=0;
                dict_data->type=data_type;
                data_top+=data_len;
                data_ptr[data_top++]='\0';
            }
        }else{
            if(0<data_top){
                dict_data = (HC_DictData*)(data_ptr+data_top)-1;
                goto code_has_data_index;
            }
            item->data_index = 0;
        }
    }
    //ret = (int)item->data;
    if(has) return item;
    if(0!=flg_empty) return item;
    if(dict->item_num>=dict->item_max_size && dict->flg_fix){
        return item;
    }
    dict->item_num++;
    if(!dict->flg_fix && dict->item_num>size/2){
        size<<=1;
        if(1>size) return item;
        item = hc_dict_get_item(dict,str,len);
        hc_dict_remap(dict,size,dict->bit+1);
        item = hc_dict_get_item(dict,str,len);

    }
    return item;
}

HC_DictItem * hc_dict_set_num(HC_Dict *dict,const char *str,int len,long data,int data_len)
{ 
    return hc_dict_set_item(dict,str,len,(void*)(long)data,data_len,TYPE_NUM);
}

int hc_dict_push_data(HC_Dict *dict,HC_DictItem *item,void* data,int data_len,char data_type)
{ 
    int data_top,data_max, ret=0;
    HC_MemBuf *data_buf = &dict->data_mem;
    char *data_ptr=data_buf->buf, resize=0;
    if(!data && data_type!=TYPE_NUM)
        return 0;
    data_top = data_buf->len;
    HC_DictData *dict_data=(HC_DictData*)(data_ptr+data_top); 
    if(0>data_len && data) data_len = strlen((char*)data);
    if(item->data && data_top>(int)item->data){
        dict_data=(HC_DictData*)(data_ptr+item->data)-1;
        if(dict_data->max_len>=(unsigned int)data_len){
            data_top = item->data;
            goto SetData;
        }
        dict_data=(HC_DictData*)(data_ptr+data_top); 
    }

    data_max = data_len+data_top+sizeof(HC_DictData)+1;
    if(data_max>data_buf->max ){
        data_max += data_max;
        data_ptr = hc_membuf_check_size(data_buf,data_max-data_buf->max);
        dict_data = (HC_DictData*)(data_ptr+data_top);
    }
    data_top+=sizeof(HC_DictData);
    item->data_index = data_top;
    dict_data->max_len=data_len;
    data_buf->len = data_top+data_len+1;
    goto SetData;

SetData:
    ret = data_top;
    if(TYPE_NUM==data_type){
        dict_data->len=data_len;
        switch(data_len){
            case 1:
                data_ptr[data_top]=(long)data;
                break;
            case sizeof(short):
                *(short*)(data_ptr+data_top)=(long)data;
                break;
            case sizeof(int):
                *(int*)(data_ptr+data_top)=(long)data;
                break;
            case sizeof(long):
                *(long*)(data_ptr+data_top)=(long)data;
                break;
            default:
                return -1;
        }
    }else{
        if(data){
            dict_data->len=data_len;
            memcpy(data_ptr+data_top,(char*)data,data_len);
        }else
            dict_data->len=0;
    }
    data_top += data_len;
    data_ptr[data_top++]='\0';
    return ret;
}


int hc_dict_remap(HC_Dict *dict,int size,char bit)
{
    int (* key_to_index)(HC_Dict*,char*,char), index_old, index_new,num=0;
    
    HC_MemBuf *key_buf = &dict->key_mem;
    char *key_ptr = key_buf->buf;
    char *key_cur, key_len_cur, has;
    int key_top, top_cur, key_index,  key_index_o;

    HC_DictItem *item_new, *nxt_item_new, *item_tmpo,*item_tmpn, *pLItem, *item_o, *nxt_items_o;
    HC_Dict tmpDict;
    tmpDict.item_max_size = size;
    tmpDict.bit = bit;
    item_new = calloc(size,sizeof(HC_DictItem));
    nxt_item_new = calloc(size,sizeof(HC_DictItem));
    item_o = dict->items;
    nxt_items_o = dict->nxt_items;
    key_to_index=dict->key_to_index;
    int nxt_link_top = 1;
    int nxt_item_index;
    
    key_top = key_buf->len;
    HC_DictKey *dict_key,*dict_key_tmp;
    for(top_cur=sizeof(HC_DictKey);top_cur<key_top;){
        key_cur = key_ptr+top_cur;
        dict_key=(HC_DictKey*)key_cur-1;
        
        key_len_cur = dict_key->len;
        if(1==dict_key->status){
            top_cur = top_cur+key_len_cur+1+sizeof(HC_DictKey);
            continue;
        }
        
        index_new = key_to_index(&tmpDict,key_cur,key_len_cur);
    
        item_tmpn = item_new+index_new;
        key_index = item_tmpn->key_index;
        has = 0; 
        if(key_index){
            dict_key_tmp = (HC_DictKey*)(key_ptr+key_index)-1;
            if(dict_key_tmp->len==key_len_cur && 0==memcmp(key_cur,key_ptr+key_index,key_len_cur))
                continue;
            nxt_item_index=item_tmpn->nxt_item_index;
            pLItem = item_tmpn;
            for(;nxt_item_index;){
                pLItem=nxt_item_new+nxt_item_index;
                key_index = pLItem->key_index;
                dict_key_tmp = (HC_DictKey*)(key_ptr+key_index)-1;
                if(dict_key_tmp->len==key_len_cur && 0==memcmp(key_cur,key_ptr+key_index,key_len_cur)){
                    has=1;
                    break;
                }
                nxt_item_index = pLItem->nxt_item_index;
            }
            if(has) continue;
            pLItem->nxt_item_index = nxt_link_top;
            item_tmpn = nxt_item_new+nxt_link_top;
            dict_key->item_index = nxt_link_top;
            dict_key->flg_nxt = 1;
            nxt_link_top++;
        }
        else{
            dict_key->item_index = index_new;
            dict_key->flg_nxt = 0;
        }


        index_old = key_to_index(dict,key_cur,key_len_cur);
    
        item_tmpo = item_o+index_old;
        key_index_o = item_tmpo->key_index;
        
        if(key_index_o!=top_cur){
            nxt_item_index=item_tmpo->nxt_item_index;
            for(;nxt_item_index;){
                item_tmpo=nxt_items_o+nxt_item_index;
                key_index_o = item_tmpo->key_index;
                if(key_index_o==top_cur){
                    has=1;
                    break;
                }
                nxt_item_index = item_tmpo->nxt_item_index;
            }
        }
        item_tmpn->key_index = top_cur;
        item_tmpn->data = item_tmpo->data;
        top_cur = top_cur+key_len_cur+sizeof(HC_DictKey)+1;
        num++;
    }
    free(dict->items);
    free(dict->nxt_items);
    dict->nxt_link_top = nxt_link_top;
    dict->items = item_new;
    dict->nxt_items = nxt_item_new;
    dict->item_max_size = size;
    dict->bit = bit;
    dict->item_num = num;

    return 0;
}

int hc_dict_free(HC_Dict *dict)
{
    if(dict->type==TYPE_RAW) free(dict->data_mem.buf);
    free(dict->key_mem.buf);
    free(dict->items);
    free(dict->nxt_items);
    free(dict);
    return 0;
}

int hc_dict_format(HC_Dict *dict,char *buf,int bufLen,const char *pFormat,int a_len,char *pNull)
{
    char c_cur,c_nxt,i_start, zt=0;
    char line[8][2];
    HC_MemBuf *key_buf = &dict->key_mem;
    int i,i_line=0,key_top=key_buf->len,i2,top,i_len;
    const char *p_sour;
    int lenNull = 0;
    char *key_ptr = key_buf->buf, *data_ptr=dict->data_mem.buf;
    HC_DictItem *item=dict->items,*nxt_items=dict->nxt_items,*itemCur;
    if(pNull) lenNull = strlen(pNull);
    if(1>a_len) a_len=strlen(pFormat);
    for(i=0,i_line=0;i<a_len&&8>i_line;i++){
        c_cur = pFormat[i];
        if('%'==c_cur){
            c_nxt = pFormat[i+1];
            if('0'<=c_nxt&&'2'>c_nxt){
                c_nxt-='0';
                zt+=2;
            }
        }
        if(2==zt){
            line[i_line][0] = c_nxt;
            line[i_line][1] = 0 ;
            i_line++;
            i++;
            zt = 0;
        }
        else if(3==zt){
            line[i_line][1] = i - line[i_line][0];
            i_line++;
            line[i_line][0] = c_nxt;
            line[i_line][1] = 0 ;
            i_line++;
            i++;
            zt = 0;
        }
        else if(0==zt){
            line[i_line][0] = i;
            zt = 1;
        }
    }
    if(1==zt&&8>i_line){
        line[i_line][1] = i - line[i_line][0];
        i_line++;
    }
    top = 0;
    HC_DictKey *dict_key=NULL;
    for(i=sizeof(HC_DictKey);i<key_top&&bufLen>top;){
        dict_key = (HC_DictKey*)(key_ptr+i)-1;
        if(1==dict_key->status||dict_key->no_format){
            i=i+dict_key->len+sizeof(HC_DictKey)+1;
            continue;
        }
        if(0<dict_key->flg_nxt){
            itemCur = nxt_items+dict_key->item_index;
        }
        else{
            itemCur = item+dict_key->item_index;
        }
        for(i2=0;i2<i_line;i2++){
            i_start = line[i2][0];
            i_len = line[i2][1];
            if(0==i_len){
                if(0==i_start){
                    p_sour=key_ptr+i;
                    i_len = ((HC_DictKey*)p_sour-1)->len;
                }else{
                    if(1>itemCur->data_index){
                        i_len = 0;
                    }else{
                        p_sour=data_ptr + itemCur->data_index;
                        i_len = ((HC_DictData*)p_sour-1)->len;
                    }
                }
            }
            else if(-1==i_len) break;
            else
                p_sour = pFormat+i_start;
            if(0==i_len && lenNull){
                memcpy(buf+top,pNull,lenNull);
                i_len = lenNull;
            }
            else{
                memcpy(buf+top,p_sour,i_len);
            }
            top+=i_len;
        }
        i=i+dict_key->len+sizeof(HC_DictKey)+1;
    }
    return top;
}

HC_Dict* hc_dict_copy(HC_Dict *src)
{
    int size = src->item_max_size;
    HC_Dict *dict= calloc(1,sizeof(HC_Dict));
    memcpy(dict,src,sizeof(HC_Dict));
    int item_size = size*sizeof(HC_DictItem);
    dict->items=malloc(item_size);
    memcpy(dict->items,src->items,item_size);
    dict->nxt_items=malloc(item_size);
    memcpy(dict->nxt_items,src->nxt_items,item_size);
    HC_MemBuf *buf_tmp;
    int num_tmp;
    if(1==src->type){
        buf_tmp = &src->data_mem;
        num_tmp = buf_tmp->max;
        dict->data_mem.buf=malloc(num_tmp);
        memcpy(dict->data_mem.buf,buf_tmp->buf, num_tmp);
    }
    buf_tmp = &src->key_mem;
    num_tmp = buf_tmp->max;
    dict->key_mem.buf=malloc(num_tmp);
    memcpy(dict->key_mem.buf,buf_tmp->buf, num_tmp);
    return dict;
}

int hc_dict_copy_to_dst(HC_Dict *src,HC_Dict *dst)
{
    memcpy(dst, src, 12);
    dst->key_to_index = src->key_to_index;
    
    dst->item_max_size = src->item_max_size;
    dst->items = realloc(dst->items, sizeof(HC_DictItem)*src->item_max_size);
    memcpy(dst->items,src->items, sizeof(HC_DictItem)*src->item_max_size);
    
    dst->nxt_items = realloc(dst->nxt_items, sizeof(HC_DictItem)*src->item_max_size);
    memcpy(dst->nxt_items,src->nxt_items, sizeof(HC_DictItem)*src->item_max_size);
    
    HC_MemBuf *src_mem = &src->key_mem;
    HC_MemBuf *dst_mem = &dst->key_mem;
    
    if(dst_mem->max<src_mem->max){
        hc_membuf_check_size(dst_mem, src_mem->max-dst_mem->max);
    }
    memcpy(dst_mem->buf, src_mem->buf, src_mem->len);
    dst_mem->len = src_mem->len;
    
    src_mem = &src->data_mem;
    dst_mem = &dst->data_mem;
    if(dst_mem->max<src_mem->max){
        hc_membuf_check_size(dst_mem, src_mem->max-dst_mem->max);
    }
    
    memcpy(dst_mem->buf, src_mem->buf, src_mem->len);
    dst_mem->len = src_mem->len;
    
    return 0;
}

int hc_dict_reset_to_zero(HC_Dict *dst)
{
    dst->item_num = 0;
    dst->nxt_link_top = 1;
    dst->key_mem.len=0;
    memset(dst->items, 0, sizeof(HC_DictItem)*dst->item_max_size);
    memset(dst->nxt_items, 0, sizeof(HC_DictItem)*dst->item_max_size);
    
    if(dst->type!=TYPE_NUM)
        dst->data_mem.len=0;
    return 0;
}

inline HC_DictItem *hc_dict_get_item_by_kindex(HC_Dict *dict,int *kindex)
{
    if((uint32_t)*kindex>=dict->key_mem.len) return NULL;
    HC_DictKey *dk = (HC_DictKey*)(dict->key_mem.buf+*kindex);
    *kindex = *kindex+sizeof(HC_DictKey)+dk->len+1;
    if(dk->flg_nxt){
        return dict->nxt_items+dk->item_index;
    }
    return dict->items+dk->item_index;
}


int hc_json_to_dict(HC_Dict *p_dict,char *pStr,int a_len)
{
    int zt=0,top=0,i=0, k_start, k_len,v_start,v_len, num, tmp2, tmp3,tmp;
    char c_cur, vType;

    if(1>a_len) return -1;
    HC_DictItem *p_item=NULL;
    HC_DictKey *pDK;
    HC_DictData *pDH;
    HC_MemBuf *key_mem = &p_dict->key_mem;
    HC_MemBuf *data_mem = &p_dict->data_mem;
    char *dst = malloc(a_len);
    memcpy(dst, pStr, a_len);
    pStr = dst;
    for(i=0;i<a_len;i++){
        c_cur = pStr[i];
        switch(zt){
            case -8:
                if(isspace(c_cur))
                    continue;
                if(':'!=c_cur){
                    free(pStr);
                    return -1;
                }
                zt = -7;
                break;
            case -7:
                if(isspace(c_cur))
                    continue;
                if('"'==c_cur){
                    zt = -6;
                    break;
                }
                else
                    zt = -2;
                break;
            case -6:
                if('\\'==c_cur && '"'==pStr[i+1]){
                    i++;
                    continue;
                }
                if('"'==c_cur)
                    zt=-2;
                break;
            case -5:
                if('0'<=c_cur&&'9'>=c_cur){
                    zt = -5;
                }
            case -2:
                if(','==c_cur||'}'==c_cur)
                    zt=1;
                break;
            case 0:
                if(isspace(c_cur))
                    continue;
                if('{'!=c_cur){
                    free(pStr);
                    return -1;
                }
                zt=1;
                break;
            case 1:
                if(isspace(c_cur)||','==c_cur)
                    continue;
                if('}'==c_cur){
                    free(pStr);
                    return 0;
                }
                if('"'!=c_cur){
                    free(pStr);
                    return -1;
                }
                zt=2;
                break;
            case 2: //key
                top=i;
                k_start = top;
                zt=3;
            case 3:
                if('"'==c_cur){
                    k_len = top-k_start;
                    if(0==k_len){
                        free(pStr);
                        return -1;
                    }
                    if(NULL==(p_item=hc_dict_get_item(p_dict,pStr+k_start,k_len))){
                        zt =-8;
                        continue;
                    }
                    pDK = (HC_DictKey*)(key_mem->buf+p_item->key_index)-1;
                    vType = pDK->type;
                    zt = 4;
                }
                tmp2 = hc_json_decode(pStr+i, pStr+top, &top);
                if(0>tmp2){
                    free(pStr);
                    return -1;
                }
                i+=tmp2;
                break;
            case 4:
                if(isspace(c_cur))
                    continue;
                if(':'!=c_cur){
                    free(pStr);
                    return -1;
                }
                zt=5;
                break;
            case 5:
                if(isspace(c_cur))
                    continue;
                if('"'==c_cur){
                    if(TYPE_RAW!=vType)
                        zt = -6;
                    else
                        zt = 6;
                }
                else if('0'<=c_cur&&'9'>=c_cur){
                    if(2!=vType)
                        zt = -2;
                    else
                        zt = 8;
                    num=c_cur-'0';
                }
                else{
                    zt = -2;
                }
                break;
            case 6:
                top = i;
                v_start = top;
                zt=7;
            case 7:
                if('"'==c_cur){
                    v_len = top-v_start;
                    if(0==v_len){
                        zt = -2;
                        continue;
                    }
                //    printf("--------------vlen:%d,%s\n",v_len,pStr+v_start);
                    tmp = hc_dict_push_data(p_dict, p_item, pStr+v_start, v_len, 1);
                    p_item->data=tmp;
                    zt = 1;
                }
                tmp2 = hc_json_decode(pStr+i, pStr+top, &top);
                if(0>tmp2){
                    free(pStr);
                    return -1;
                }
                i+=tmp2;
                break;
            case 8:
                if('0'<=c_cur&&'9'>=c_cur){
                    num = num *10 + c_cur-'0';
                    break;
                }
                else if(','==c_cur||'}'==c_cur){
                    zt=1;
                }
                else{
                    zt = -2;
                }
                tmp = hc_dict_push_data(p_dict, p_item, (void*)(long)num, 4, 2);
                p_item->data=tmp;
                zt =1;
                break;

        }
    }
    free(pStr);
    return 0;
}

HC_MemBuf* hc_dict_to_json(HC_Dict *dict, HC_MemBuf *mem)
{
    HC_DictItem *di;
    int i=0;
    HC_MemBuf *keymem=&dict->key_mem;
    HC_MemBuf *datamem=&dict->data_mem;
    if(!mem){
        mem = hc_membuf_create(keymem->len+datamem->len);
    }else{
        hc_membuf_check_size(mem, 1);
    }
    mem->buf[mem->len++]='{';
    int len=0;
    char *ptmp;
    long n;
    HC_DictData *ddata;
    for(;(di=hc_dict_get_item_by_kindex(dict,&i))!=NULL;){
        if(di->user_flag) continue;
        len=1;
        hc_membuf_check_size(mem, 1);
        mem->buf[(mem->len)++]='"';
        ptmp = keymem->buf+di->key_index;
        hc_json_escape(ptmp, ((HC_DictKey*)ptmp-1)->len, mem);
        hc_membuf_check_size(mem, 2);
        mem->buf[mem->len++]='"';
        mem->buf[(mem->len)++]=':';
        if(di->data_index>0){
            ptmp = datamem->buf+di->data_index;
            ddata = (HC_DictData*)ptmp-1;
            switch(ddata->type){
                case TYPE_RAW:
                    hc_membuf_check_size(mem, 1);
                    mem->buf[mem->len++]='"';
                    hc_json_escape(ptmp, ddata->len, mem);
                    hc_membuf_check_size(mem, 1);
                    mem->buf[mem->len++]='"';
                    break;
                case TYPE_NUM:
                    switch(ddata->len){
                        case 1:
                            n=ptmp[0];
                            break;
                        case 2:
                            n=*(short*)ptmp;
                            break;
                        case 4:
                            n=*(int*)ptmp;
                            break;
                        default:
                            n=*(long*)ptmp;
                            break;
                    }
                    ptmp=hc_num_to_str(n,10,&len,NULL);
                    hc_membuf_check_size(mem, len);
                    memcpy(mem->buf+mem->len,ptmp,len);
                    mem->len+=len;
                    break;
                case TYPE_BIN:
                    hc_membuf_check_size(mem,ddata->len); 
                    memcpy(mem->buf+mem->len,ptmp,ddata->len);
                    mem->len+=ddata->len;
                    break;
            }
        }else{
            hc_membuf_check_size(mem, 5);
            memcpy(mem->buf+mem->len,"null,",5);
            mem->len+=5;
            continue;
        }
        hc_membuf_check_size(mem, 1);
        mem->buf[(mem->len)++]=',';
    }
    if(len>0){
        mem->buf[--mem->len]='}';
        mem->buf[++mem->len]='\0';
    }else{
        hc_membuf_check_size(mem, 1);
        mem->buf[mem->len++]='}';
        mem->buf[mem->len]='\0';
    }
    mem->cur_len = mem->len-mem->cur_len;
    return mem;
}

HC_MemBuf* hc_dict_to_jsbuf(HC_Dict *dict, HC_MemBuf *mem)
{
    HC_DictItem *di;
    int i=0;
    HC_MemBuf *keymem=&dict->key_mem;
    HC_MemBuf *datamem=&dict->data_mem;
    if(!mem){
        mem = hc_membuf_create(keymem->len+datamem->len);
    }
    int len=0;
    char *ptmp;
    long n;
    HC_DictData *ddata;
    int cur_len = mem->len;
    hc_jsbuf_set_header(mem, 4, 0, JS_TYPE_DICT);
    HC_JsBuf *jsbuf;
    mem->len+=5;
    for(;(di=hc_dict_get_item_by_kindex(dict,&i))!=NULL;){
        if(di->user_flag) continue;
        ptmp = keymem->buf+di->key_index;
        len = ((HC_DictKey*)ptmp-1)->len;
        
        hc_jsbuf_set_data(mem, ptmp, len, JS_TYPE_STR);

        if(di->data_index>0){
            ptmp = datamem->buf+di->data_index;
            ddata = (HC_DictData*)ptmp-1;
            len = ddata->len;
            hc_jsbuf_set_data(mem, ptmp, len, ddata->type);
        }else{
            hc_jsbuf_set_data(mem, NULL, 0, 0);
            continue;
        }
    }
    mem->cur_len = mem->len - cur_len;
    jsbuf = (HC_JsBuf*)(mem->buf+ cur_len);
    jsbuf->len4 = mem->cur_len - 5;
    return mem;
}

