#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "../util/hc_util.h"
#include "http.h"

HC_Array *hc_read_app_from_file(char *file_name)
{
    FILE *fp;
    
    if((fp=fopen(file_name,"rt"))==NULL){
        perror("read app file:");
        return NULL;
    }
    char line[1024];
    int line_len;
    HC_Array *arr= hc_array_create(32);
    int cur_num, cur_top;
    for(;;){
        if ( fgets (line , 1024 , fp) == NULL ){
            break;
        }
        line_len = strlen(line);
        if(!feof(fp)){
            line_len--;
        }
        if(7>line_len) continue;
        if(line[0]=='#') continue;
        cur_num = arr->item_num;
        cur_top = arr->data_top;
        if(NULL==hc_split_trim(arr, line, line_len, ',', 3, ' ')){
            perror(" hc_read_app_from_file ");
            break;
        }
        if(arr->item_num-cur_num!=3){
            arr->data_top = cur_top;
            arr->item_num = cur_num;
        }
    }
    fclose(fp);
    return arr;
}

int hc_load_app(HC_HttpServer *srv, char *app_conf_file)
{
    void *plib = NULL,*handle=NULL;
    int (* app_init_handle)(HC_HttpServer *);
    HC_Array *arr = hc_read_app_from_file(app_conf_file);
    if(!arr) return -1;
    if(arr->item_num==0){
        hc_array_free(arr);
        return -1;
    }
    HC_Dict *app_dict = srv->app;
    HC_Dict *app_lib = srv->app_lib;
    char *key, *val, *ptmp;
    int tmp=0, index=0;
    int type, i=0;
    for(;NULL!=(ptmp = hc_array_get_item_retptr(arr, index, &tmp)); i++, index++){
        if(i==0){
            if(0==tmp) type = 0;
            else type = ptmp[0];
            continue;
        }
        if(1==i){
            key = ptmp;
            if(tmp == 0){
                index++;
                i=-1;
            }
            continue;
        }
        val = ptmp;
        i=-1;
        if(0==tmp){
            continue;
        }
        plib = dlopen(val, RTLD_NOW | RTLD_GLOBAL);
        hc_printf_once_warn("----app lib loading : %s \n",val);
        if(NULL==plib){
            hc_printf_error("  lib error: %s , %s\n",val,dlerror());
            continue;
        }
        app_init_handle = dlsym(plib, "app_init_handle");
        if(app_init_handle){
            if(0!=app_init_handle(srv)){
                dlclose(plib);
                hc_printf_error("  lib error: %s , return 1\n",val);
                continue;
            }
        }
        if(type=='1'){
            handle = dlsym(plib, "app_get_handle");
            if(handle){
                hc_dict_set_num(app_dict, key, ((HC_DictData*)key-1)->len,
                        (uint64_t)handle, 8);
            }
        }
        hc_dict_set_num(app_lib, key, ((HC_DictData*)key-1)->len,
                (uint64_t)plib, 8);
        hc_printf_once_green("      app lib load success : %s \n",val);
    }
    hc_array_free(arr);
    return 0;
}
