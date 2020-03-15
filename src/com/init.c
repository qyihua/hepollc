#include <stdio.h>
#include "../include/hepollc.h"


int hc_init_all(long arg)
{
    hc_co_init_handle(0);
    hc_http_init_handle(0);
    hc_httpc_init_handle(0);
    hc_printf_once_green("init all\n");
    return 0;
}

