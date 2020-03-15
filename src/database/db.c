#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include "../util/hc_util.h"
#include "db.h"

HC_Dict *g_hc_dblib=NULL;

HC_ConnItem* hc_db_get_conn(HC_ConnPool *cp)
{
    HC_ConnItem *conn = hc_connpool_get(cp);
    int ret =0;
    if(!conn) 
        return NULL;
    conn->dbdata= NULL;
    HC_DBPool *dbpool = cp->ptr;
    ret = dbpool->check_conn(conn);
    if(0!=ret){
        hc_connpool_recycle(conn);
        return NULL;
    }
    return conn;
}

int hc_db_recycle_conn(HC_ConnItem *conn)
{
    int ret = hc_connpool_recycle(conn);
    ((HC_DBPool*)conn->pool->ptr)->recycle_conn(conn,ret);
    return ret;
}

HC_ConnPool* hc_db_create_pool(char *libname, int keep_num, int max_num, int wait_num)
{
    if(!g_hc_dblib) return NULL;
    int ret;
    void *dblib = (void*)hc_dict_get_num(g_hc_dblib, libname, -1, &ret);
    if(8!=ret) return NULL;
    HC_ConnPool *pool = hc_connpool_create(0,keep_num, max_num, wait_num);
    if(!pool) return NULL;
    HC_DBPool *dbpool = malloc(sizeof(HC_DBPool));
    if(!dbpool){
        hc_connpool_free(pool);
        return NULL;
    }
    pool->ptr = dbpool;
    int (* handle)(HC_DBPool *);
    handle = dlsym(dblib, "libdb_init_handle");
    if(!handle){
        hc_db_free_pool(pool);
        return NULL;
    }
    handle(dbpool);

    return pool;
}

void hc_db_free_pool(HC_ConnPool *pool)
{
    free(pool->ptr);
    hc_connpool_free(pool);
}

int hc_db_sql_exe(HC_ConnItem *ci,char *sql, int sql_len,void *params,
        int param_len, int param_start)
{
    return ((HC_DBPool*)ci->pool->ptr)->sql_exe(
            ci, sql, sql_len, params, param_len, param_start);
}

int hc_db_load_lib(char *name, char *path)
{
    if(!g_hc_dblib)
        g_hc_dblib = hc_dict_create(TYPE_NUM, 16);
    int name_len = strlen(name);
    if(hc_dict_has(g_hc_dblib, name, name_len)){
        return 0;
    }
    void *plib;
    plib = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if(NULL==plib){
        hc_printf_error("  dblib error: %s , %s\n",path,dlerror());
        return 1;
    }
    hc_dict_set_num(g_hc_dblib, name, name_len,
            (uint64_t)plib, 8);
    return 0;
}
