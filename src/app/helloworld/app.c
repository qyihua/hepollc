#include <stdio.h>

#include "handle.h"

static HC_Dict *url_dict;
extern HC_Dict *g_confs;

void* app_get_handle(char *str,int len)
{
    int ret;
    void *ptr = (void*)hc_dict_get_num(url_dict, str, len, &ret);
    if(8!=ret) return NULL;
    return ptr;
}

//void* app_get_handle(char *str,int len)
//{
//    return hello_world_app_path;
//}


int app_init_handle(HC_HttpServer *srv)
{
    int ret;
    url_dict = hc_dict_create(TYPE_NUM, 16);
    hc_dict_set_num(url_dict, "apphello", -1, (uint64_t)hello_world_app_path, 8);
    hc_http_srv_set_url(srv, "/roothello", -1, hello_world_root_path);

    return 0;
}
