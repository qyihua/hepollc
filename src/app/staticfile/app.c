
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "handle.h"

extern HC_Dict *g_confs;
HC_Dict *g_mime;

int auth_init_db(int arg);


void* static_file(char *str,int len)
{
    return hc_static_file_handle;
}

int app_init_handle(HC_HttpServer *srv)
{
    int ret;
    char *ptmp = hc_dict_get_ptr(g_confs,"static_path",-1,&ret);
    if(ptmp){
        hc_dict_set_num(srv->app, ptmp, ret, (uint64_t)static_file, 8);
    }else{
        hc_dict_set_num(srv->app, "static", 6, (uint64_t)static_file, 8);
    }
    hc_http_srv_set_url(srv, "/", 1, hc_home);

    HC_Dict *mime= hc_dict_create(TYPE_RAW,30);
    hc_dict_set_item(mime, "txt", 3 , "text/plain; charset=UTF-8", 25 , TYPE_RAW);
    hc_dict_set_item(mime, "htm", 3 , "text/html; charset=UTF-8", 24 , TYPE_RAW);
    hc_dict_set_item(mime, "html", 4 , "text/html; charset=UTF-8", 24 , TYPE_RAW);
    hc_dict_set_item(mime, "js", 2 , "application/javascript", 22 , TYPE_RAW);
    hc_dict_set_item(mime, "css", 3 , "text/css", 8 , TYPE_RAW);
    hc_dict_set_item(mime, "jpg", 3 , "image/jpeg", 10 , TYPE_RAW);
    hc_dict_set_item(mime, "jpeg", 4 , "image/jpeg", 10 , TYPE_RAW);
    hc_dict_set_item(mime, "png", 3 , "image/png", 9 , TYPE_RAW);
    hc_dict_set_item(mime, "json", 3 , "application/json", 16 , TYPE_RAW);
    hc_dict_set_item(mime, "gif", -1 , "image/gif", -1 , TYPE_RAW);
    g_mime = mime;
    return 0;
}
