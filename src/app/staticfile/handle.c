#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>

#include "handle.h"

extern HC_Dict *g_mime;

int hc_home(HC_HttpRequest *hr)
{

    hc_http_finish(hr, "<h1>HepollC Server</h1>", 23);
    return 0;
}

int hc_static_file_handle(HC_HttpRequest *hr)
{
    HC_HttpRequestRemote *hremote = &hr->remote;
    int ret,i;
    char *ptmp, *file_path, c_cur;
    file_path = hc_http_get_httpdata(hremote, HTTP_INDEX_PATH, &ret);
    if(ret>1000){
        hc_http_set_code(hr, 404, NULL,0);
        return 0;
    }
    int dot=0;
    for(i=0;i<ret;i++){
        c_cur=file_path[i];
        if('/'==c_cur){
            if(2==dot){
                break;
            }
            dot=0;
            continue;
        }
        if(2>dot&&'.'==c_cur){
            dot++;
        }else{
            dot = 5;
        }
    }
    if(2==dot){
        hc_http_set_code(hr, 404, NULL,0);
        return 0;
    }
    char *tmp_buf=((HC_MemBuf*)hc_http_get_tmpbuf(hr,1024))->buf;
    tmp_buf[0]='.';
    memcpy(tmp_buf+1, file_path, ret);
    tmp_buf[++ret]=0;
    struct stat statBuf;
    int tmp = stat(tmp_buf, &statBuf);
    if(0!=tmp){
        hc_http_set_code(hr, 404, NULL,0);
        return 0;
    }
    char suffix[20];
    int suffix_top=19;
    for(i=ret-1;i>-1&&suffix_top>-1;i--){
        c_cur = tmp_buf[i];
        if('.'==c_cur)
            break;
        suffix[--suffix_top] = tolower(c_cur);
    }
    ptmp = hc_dict_get_ptr(g_mime, suffix+suffix_top, 19-suffix_top,
            &ret);
    if(ptmp){
        hc_http_set_header(hr, "Content-Type",12, ptmp, ret);
    }else{
        hc_http_set_header(hr, "Content-Type",12, "application/*", 13);
    }
    int fd = open(tmp_buf, O_RDONLY);
    for(i=0;i<statBuf.st_size;){
        ret = read(fd,tmp_buf,1024);
        if(ret<1) break;
        hc_http_write(hr,tmp_buf,ret);
        i+=ret;
    }
    close(fd);
    return 0;

}


