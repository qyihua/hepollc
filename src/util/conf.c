#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hc_dict.h"
#include "hc_util.h"
#include "util/hc_base_type.h"

HC_Dict *g_confs=NULL;
#define MAX_CONF_FILE_SIZE 4096000

int hc_read_conf_from_file(char *file_name);
int hc_read_conf_from_shell(int argc, char *argv[]);
int hc_conf_init_handle(int argc, char *argv[])
{
    g_confs = hc_dict_create(TYPE_RAW,64);

    hc_dict_set_item(g_confs,"process_num",-1,(void*)(long)1,4,TYPE_NUM);
    hc_dict_set_item(g_confs,"reuseport",-1,(void*)(long)0,4,TYPE_NUM);
    hc_dict_set_item(g_confs,"max_cofd",-1,(void*)(long)1000000,4,TYPE_NUM);
    hc_dict_set_item(g_confs,"max_callback",-1,(void*)(long)1000000,4,TYPE_NUM);
    hc_dict_set_item(g_confs,"max_mempool",-1,(void*)(long)20000000,8,TYPE_NUM);
    hc_dict_set_item(g_confs,"session_max",-1,(void*)(long)100000,4,TYPE_NUM);
    hc_dict_set_item(g_confs,"session_timeout",-1,(void*)(long)3600,4,TYPE_NUM);
    hc_dict_set_item(g_confs,"login_error_maxip",-1,(void*)(long)200000,4,TYPE_NUM);
    hc_dict_set_item(g_confs,"login_error_timeout",-1,(void*)(long)60,4,TYPE_NUM);
    hc_dict_set_item(g_confs,"port",4,(void*)(long)8088,4,TYPE_NUM);
    hc_dict_set_item(g_confs,"session_srv_port",-1,(void*)(long)8878,4,TYPE_NUM);
    hc_dict_set_item(g_confs,"address",-1,"127.0.0.1",-1,TYPE_RAW);
    hc_dict_set_item(g_confs,"debug",5,0,1,TYPE_NUM);
    hc_dict_set_item(g_confs,"session_url",-1, "http://127.0.0.1:8878/session", -1,TYPE_RAW);
    hc_dict_set_item(g_confs,"session_srv_url",-1, "/session", -1,TYPE_RAW);
    hc_dict_set_item(g_confs,"server_mode",-1,"http",-1,TYPE_RAW);
    hc_dict_set_item(g_confs,"app_file",-1,"conf/app.conf",-1,TYPE_RAW);
    hc_dict_set_item(g_confs,"static_path",-1,"static",-1,TYPE_RAW);
    hc_dict_set_item(g_confs,"conf",4,"conf/conf.conf",-1,TYPE_RAW);

    hc_dict_set_item(g_confs,"token_max",-1,(void*)(long)1000,4,TYPE_NUM);

    hc_read_conf_from_shell(argc, argv);
    int ret;
    char *conf_file = hc_dict_get_ptr(g_confs, "conf", 4,&ret);

    hc_read_conf_from_file(conf_file);
    hc_read_conf_from_shell(argc, argv);

}

int hc_read_conf_from_shell(int argc, char *argv[])
{
    int i, key_len, val_len, ret;
    char *key=NULL, *ptmp;
    int64_t tmp_num;
    HC_DictData *ddata;
    for(i=1;i<argc;i++){
        ptmp = argv[i];
        if(!key){
            key_len = strlen(ptmp+1);
            if(1>key_len) continue;
            key = ptmp+1;
        }else{
            val_len = strlen(ptmp);
            ddata = (HC_DictData*)hc_dict_get_ptr(g_confs, key, key_len,&ret);
            if(ddata){
                ddata-=1;
                if(TYPE_NUM==ddata->type){
                    tmp_num = hc_str_to_num(10, ptmp, val_len, &ret);
                    if(0==ret){
                        hc_dict_set_num(g_confs, key, key_len, tmp_num, ddata->len);
                        key=NULL;
                    }
                    continue;
                }
            }
            hc_dict_set_item(g_confs, key, key_len, ptmp, val_len, TYPE_RAW);
            key=NULL;
        }
    }
    return 0;
}

int hc_read_conf_from_file(char *file_name)
{
    FILE *fp;
    
    if((fp=fopen(file_name,"rt"))==NULL){
        perror("read conf:");
        return -1;
    }
    char *src = malloc(MAX_CONF_FILE_SIZE);
    int src_max = fread(src, sizeof(char), MAX_CONF_FILE_SIZE, fp);
    fclose(fp);
    if(1>src_max){
        free(src);
        return -1;
    }
    char c_cur, end_char;
    int i=0, i_start, status=0, i_end, str_len, key_i,key_len,ret;
    long tmp;
    HC_Array *arr;
    HC_DictData *ddata;
    int is_num, num_len;
code_for:
    hc_utl_escape(src,&i,src_max,' ');
    for(;i<src_max;){
        for(; i<src_max; i++){
            c_cur = src[i];
            switch(status){
                case 0:
                    if('\r'==c_cur||'\n'==c_cur){continue;}
                    if('#'==c_cur){status=-1;continue;}
                    status = 1;
                    i_start = i;
                    break;
                case 1:
                    if('='==c_cur){ status=2;goto code_str_end;}
                    if('\r'==c_cur||'\n'==c_cur){ status=0; goto code_for;}
                    break;
                case 3: 
                    i_start = i;
                    status = 4;
                case 4:
                    if('\r'==c_cur||'\n'==c_cur){goto code_str_end;}
                    continue;
                case -1:
                    if('\r'==c_cur||'\n'==c_cur){
                        status=0;
                        goto code_for;
                    }
                    continue;
                default:
                    break;
            }
        }
        if(2>status){
            free(src);
            return i;
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
            case 2: // key
                hc_utl_escape_right(src, i_start, &str_len, ' ');
                key_i = i_start;
                key_len = str_len;
                status = 3;
                i++;
                goto code_for;

            case 4: // value
                status = 0;
                hc_utl_escape_right(src, i_start, &str_len, ' ');
                if(key_len>6){
                    if(0==memcmp(src+key_i+key_len-6, "[num4]", 6)){
                        is_num=1;
                        key_len-=6;
                        num_len=4;
                    }else if(0==memcmp(src+key_i+key_len-6, "[num8]", 6)){
                        is_num=1;
                        key_len-=6;
                        num_len=8;
                    }else
                        is_num=0;
                }else
                    is_num=0;
                ddata = (HC_DictData*)hc_dict_get_ptr(g_confs, src+key_i, key_len,&ret);
                if(ddata){
                    ddata-=1;
                    if(TYPE_NUM==ddata->type){
                        tmp = hc_str_to_num(10, src+i_start, str_len, &ret);
                        if(0==ret){
                            hc_dict_set_num(g_confs, src+key_i, key_len, tmp, ddata->len);
                        }
                        goto code_for;
                    }
                    if(TYPE_LIST == ddata->type){
                        arr = (HC_Array*)(*(uint64_t*)(ddata+1));
                        hc_array_append_item(arr, src+i_start, str_len, TYPE_RAW);
                        goto code_for;
                    }else{
                        hc_dict_set_item(g_confs, src+key_i, key_len, src+i_start, str_len, TYPE_RAW);
                    }
                }else{
                    if(key_len>2 && 0==memcmp(src+key_i+key_len-2, "[]", 2)){
                        arr = hc_array_create(16);
                        hc_dict_set_item(g_confs, src+key_i, key_len, arr, 8, TYPE_LIST);
                        hc_array_append_item(arr, src+i_start, str_len, TYPE_RAW);
                    }else if(is_num){
                        tmp = hc_str_to_num(10, src+i_start, str_len, &ret);
                        if(0==ret){
                            hc_dict_set_num(g_confs, src+key_i, key_len, tmp, num_len);
                        }
                    }
                    else{
                        hc_dict_set_item(g_confs, src+key_i, key_len, src+i_start, str_len, TYPE_RAW);
                    }
                }
                goto code_for;
        }
    }
    free(src) ;
    return 0;
}

