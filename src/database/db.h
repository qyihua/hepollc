#pragma once
#include "../util/hc_dict.h"
#include "../util/hc_data.h"

#define DB_PG 1
#define DB_OCI 2

typedef struct HC_DBData{
    int row_num;
    int col_num;
    int count;
    void *ptr;
    //int *index;
    HC_Array *data;
}HC_DBData;

typedef struct HC_DBPool{
    HC_ConnPool *pool;
    int (*check_conn)(struct HC_ConnItem*);
    int (*recycle_conn)(struct HC_ConnItem*,int);
    int (*sql_exe)(HC_ConnItem *ci,char *sql, int sql_len,void *params,
        int param_len, int param_start);
}HC_DBPool;

HC_DBData *hc_dbdata_alloc();
int hc_dbdata_reset(HC_DBData *p);
void hc_dbdata_free(HC_DBData *p);
int hc_db_init_handle(int arg);
int hc_db_init_data_handle(int arg);
char *hc_db_get_col(HC_DBData *pdb,int row,int col,int *ret);
HC_Array *hc_db_get_row(HC_DBData *pdb,int row);
HC_ConnItem* hc_db_get_conn(HC_ConnPool *cp);
int hc_db_recycle_conn(HC_ConnItem *conn);
HC_ConnPool* hc_db_create_pool(char *libname,int keep_num, int max_num, int wait_num);
void hc_db_free_pool(HC_ConnPool *pool);
int hc_db_sql_exe(HC_ConnItem *ci,char *sql, int sql_len,void *params,
        int param_len, int param_start);
int hc_db_load_lib(char *name, char *path);
HC_MemBuf* hc_dbdata_to_jsbuf(HC_DBData *dbdata, HC_MemBuf *mem, int start,
        int slice_len, uint8_t *indexs);
