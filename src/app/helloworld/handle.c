#include <stdio.h>

#include "handle.h"

int hello_world_app_path(HC_HttpRequest *hr)
{
    hc_http_finish(hr, "<h1>Hello, World!</h1><br/>This is app path", -1);
    return 0;
}

int hello_world_root_path(HC_HttpRequest *hr)
{
    hc_http_finish(hr, "<h1>Hello, World!</h1><br/>This is root path", -1);
    return 0;
}


