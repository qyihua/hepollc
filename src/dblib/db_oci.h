#include <oci.h>
#include "../include/hepollc.h"

typedef struct HC_OCIconn{
    int fd;
    int commit;
    OCIEnv *env;
    OCIError *err;
    OCIServer *srv;
    OCISvcCtx *svc;
    OCIStmt *stmt;
    OCIDefine *dfn;
    OCIBind *bnd;
    OCISession *session;
    OCIAuthInfo *auth;
    HC_Dict *param;
    long ret;
}HC_OCIconn;

HC_OCIconn* hc_oci_connect(HC_Dict*);
int hc_oci_reconnect(HC_ConnItem *conn_item);
int hc_oci_sql_exe(HC_ConnItem *pCI,char *sql_text,int sql_len,void *params,
        int param_len, int param_start);
int hc_oci_get_data(HC_ConnItem *conn_item);
int hc_oci_commit(HC_ConnItem *ci);
int hc_oci_check_conn(HC_ConnItem *conn_item);
int hc_oci_close(HC_ConnItem *ci, int nokeep);
