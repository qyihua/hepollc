#include <libpq-fe.h>
#include "../include/hepollc.h"

PGconn* hc_pg_connect(HC_Dict*,char *);
int hc_pg_check_conn(HC_ConnItem *conn_item);
int hc_pg_get_data(PGconn *pgConn,HC_DBData *pdb);
int hc_pg_sql_exe_hc(HC_ConnItem *ci,char *sql, int sql_len,void *params,
        int param_len, int param_start);

int hc_pg_sql_exe(HC_ConnItem *pCI,char *sql_str,void *params,
        int param_len, int param_start);
