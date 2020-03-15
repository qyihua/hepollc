#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>
#include <pthread.h>

#include "db_oci.h"


extern HC_Dict *g_confs;

OCIEnv *g_oci_env=NULL;
void oci_checkerr(OCIError *errhp, sword status);


int libdb_init_handle(HC_DBPool *pool)
{
    pool->check_conn = hc_oci_check_conn;
    pool->recycle_conn = hc_oci_close;
    pool->sql_exe = hc_oci_sql_exe;
    return 0;
}

int hc_oci_login_block(HC_CoFd *cofd)
{
    pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    
    HC_OCIconn *conn=cofd->ptr2;
    HC_Dict *param = conn->param;
    if(!param)  goto code_error;
    int conn_len;
    char *connstr = hc_dict_get_ptr(param, "connstr", 7, &conn_len);
    if(1>conn_len) goto code_error;
    int user_len, password_len;
    char *user = hc_dict_get_ptr(param, "user",4, &user_len);
    if(1>user_len) goto code_error;
    char *password = hc_dict_get_ptr(param, "password",8,&password_len);
    if(1>password_len) goto code_error;
    int ret;
    ret = OCILogon2(conn->env, conn->err, &conn->svc, 
            (text *)user, (ub4)user_len, (text*)password,(ub4)password_len,
        (text *)connstr, (ub4)conn_len,
        OCI_DEFAULT);
    if(OCI_SUCCESS != ret){
        cofd->my_status = -1;
        oci_checkerr(conn->err,ret);
    }else{
        OCIAttrGet(conn->svc,OCI_HTYPE_SVCCTX,&conn->srv,NULL,OCI_ATTR_SERVER,conn->err);
        OCIAttrSet ((dvoid *) conn->srv, (ub4) OCI_HTYPE_SERVER, NULL, 0, (ub4) OCI_ATTR_NONBLOCKING_MODE, conn->err);
        if(OCI_SUCCESS!=ret){
            cofd->my_status = -1;
        }
            cofd->my_status = 0;
    }
    if(COFD_USING == cofd->status){
        hc_set_timerfd(cofd->fd, 0, 1);
    }
    return 0;
code_error:
    cofd->my_status = -1;
    if(COFD_USING == cofd->status){
        hc_set_timerfd(cofd->fd, 0, 1);
    }
    return -1;
}

// x/x   *(*(*(**(ptr+0x70+0xd8)+0x168+0x8)+0x2b8)+0x30)+0xa98
int hc_oci_get_fd(uint64_t ptr){
    ptr = ptr + 0x70 + 0xd8;
    ptr = *(uint64_t*)ptr;
    ptr = *(uint64_t*)ptr + 0x168 + 0x8;
    ptr = *(uint64_t*)ptr + 0x2b8;
    ptr = *(uint64_t*)ptr + 0x30;
    ptr = *(uint64_t*)ptr + 0xa98;
    int fd = *(int*)ptr;
    return fd;
}

HC_OCIconn* hc_oci_connect(HC_Dict *param)
{
    int ret;
    OCIParam *col = 0;
    OCIDefine *defnp=NULL;
    HC_OCIconn *conn;
    if(!g_oci_env)
        OCIEnvCreate(&g_oci_env, OCI_THREADED, (void  *)0, 0, 0, 0, (size_t) 0, (void  **)0);
    conn = calloc(sizeof(HC_OCIconn),1);
    conn->env = g_oci_env;
    OCIHandleAlloc (conn->env, (void**)&(conn->err),OCI_HTYPE_ERROR, 0, (void  **) 0);
    OCIHandleAlloc (conn->env, (void  **)&conn->svc,OCI_HTYPE_SVCCTX, 0, (void  **) 0);
    conn->param = param;
    ret = hc_co_timeout_thread(40, hc_oci_login_block, conn);
    if(0!=ret){
        printf("connect oci error..\n");
        goto code_error;
    }
    int fd = hc_oci_get_fd((uint64_t)conn->srv);
    conn->fd = fd;
    ret = OCIHandleAlloc (conn->env, (void**)&(conn->stmt),OCI_HTYPE_STMT, 0, (void  **) 0);
    if(OCI_SUCCESS!=ret){
        oci_checkerr(conn->err,ret);
        goto code_error;
    }
    return conn;
code_error:
    OCIHandleFree(conn->svc,OCI_HTYPE_SVCCTX);
    OCIHandleFree(conn->err,OCI_HTYPE_ERROR);
    free(conn);
    return NULL;
}

void oci_checkerr(OCIError *errhp, sword status)
{
    text errbuf[512];
    sb4 errcode = 0;

    switch (status){
        case OCI_SUCCESS:
            break;
        case OCI_SUCCESS_WITH_INFO:
            (void) printf("Error - OCI_SUCCESS_WITH_INFO\n");
            break;
        case OCI_NEED_DATA:
            (void) printf("Error - OCI_NEED_DATA\n");
            break;
        case OCI_NO_DATA:
            (void) printf("Error - OCI_NODATA\n");
            break;
        case OCI_ERROR:
            (void) OCIErrorGet((dvoid *)errhp, (ub4) 1, (text *) NULL, &errcode,
                errbuf, (ub4) sizeof(errbuf), OCI_HTYPE_ERROR);
            (void) printf("Error - %.*s\n", 512, errbuf);
            break;
        case OCI_INVALID_HANDLE:
            (void) printf("Error - OCI_INVALID_HANDLE\n");
            break;
        case OCI_STILL_EXECUTING:
            (void) printf("Error - OCI_STILL_EXECUTE\n");
            break;
        case OCI_CONTINUE:
            (void) printf("Error - OCI_CONTINUE\n");
            break;
        default:
            printf("no err\n");
            break;
    }
}
int hc_oci_sql_exe(HC_ConnItem *conn_item,char *sql_text,int sql_len,void *params,
        int param_len, int param_start)
{
    HC_OCIconn *conn=conn_item->ptr;
    sword ret_oci;
    OCIBind *oci_bind=NULL;
    int i,ret=0,i2;
    char *ptmp=NULL;
    int tmp=0, fd=conn_item->fd;
    uint32_t ev;
    if(-1==param_len) param_len = strlen((char*)params);
    if(-1==sql_len) sql_len = strlen(sql_text);
code_for:
    for(i=0;i<3;i++){
        ret_oci = OCIStmtPrepare(conn->stmt, conn->err, (OraText *)sql_text,
                (ub4)sql_len,(ub4) OCI_NTV_SYNTAX, (ub4) OCI_DEFAULT);
        if(OCI_ERROR == ret_oci){
            goto code_retry;
        }
        if(0<param_len){
            if(-1==param_start){
                if(0<param_len){
                    ret_oci = OCIBindByPos(conn->stmt,&oci_bind,conn->err,1,(char*)params,
                            param_len,SQLT_CHR,NULL,NULL,NULL,0,NULL,OCI_DEFAULT);
                }
                //oci_checkerr(conn->err, ret_oci);
            }else{
                ret = param_start + param_len;
                for(i2=param_start;i2<ret;){
                    ptmp=hc_array_get_item_retptr((HC_Array*)params, i2, &tmp);
                    ret_oci = OCIBindByPos(conn->stmt,&oci_bind,conn->err,++i2,
                            ptmp,tmp,SQLT_CHR,NULL,NULL,NULL,0,NULL,OCI_DEFAULT);
                    //oci_checkerr(conn->err, ret_oci);
                }
            }
        }
        ret_oci=OCIAttrGet(conn->stmt,OCI_HTYPE_STMT,&tmp,NULL,OCI_ATTR_STMT_TYPE,conn->err);
        if(OCI_STMT_SELECT == tmp){
            tmp=0;
        }
        else{
            tmp = 1;
            conn->commit=1;
        }
        for(;;){
            ret_oci = OCIStmtExecute(conn->svc, conn->stmt, conn->err, (ub4) tmp, 
                    (ub4) 0,(CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT);
            if(OCI_STILL_EXECUTING == ret_oci){
                ev = hc_co_wait_fd_timeout(fd,EPOLLIN|EPOLLRDHUP,60);
                if(!ev||(ev&EPOLLRDHUP)){
                    goto code_retry;
                }
            }else
                break;
        }
        //printf("--------------exe has data or err:%d\n",ret_oci);
        if(OCI_ERROR == ret_oci){
            goto code_retry;
        }
        break;
    }
    ret = hc_oci_get_data(conn_item);
    return ret;

code_retry:
    oci_checkerr(conn->err, ret_oci);
    OCIErrorGet( conn->err, 1, NULL, &ret, NULL, 0, OCI_HTYPE_ERROR);
    printf("exe errocode %d\n",ret);
    hc_oci_reconnect(conn_item);
    if(2==i){
        goto code_error;
    }
    if(3135==ret||3114==ret){
        goto code_for;
    }
code_error:
    if(conn_item->dbdata){
        hc_dbdata_free(conn_item->dbdata);
        conn_item->dbdata=NULL;
    }
    return -1;
}

int hc_oci_close(HC_ConnItem *ci, int nokeep)
{
    
    hc_oci_commit(ci);
    if(0==nokeep) return 0;
    int fd =ci->fd;
    if(0<fd){
        close(fd);
        ci->fd=-1;
    }
    HC_OCIconn *conn=ci->ptr;
    if(!conn) return 0;
    OCIHandleFree(conn->svc,OCI_HTYPE_SVCCTX);
    OCIHandleFree(conn->err,OCI_HTYPE_ERROR);
    if(conn->stmt)
        OCIHandleFree(conn->stmt,OCI_HTYPE_STMT);
    free(conn);
    ci->ptr=NULL;
    return 0;
}

int hc_oci_reconnect(HC_ConnItem *conn_item)
{
    int fd = conn_item->fd;
    if(0<fd){
        close(fd);
        conn_item->fd=-1;
    }
    HC_OCIconn *conn=conn_item->ptr;
    if(conn_item->fd>0) close(conn_item->fd);
    int ret=0;
    OCIHandleFree(conn->svc,OCI_HTYPE_SVCCTX);
    OCIHandleFree(conn->stmt,OCI_HTYPE_STMT);

    OCIHandleAlloc (conn->env, (void **)&conn->svc,OCI_HTYPE_SVCCTX, 0, (void  **) 0);
    ret = hc_co_timeout_thread(12, hc_oci_login_block, conn);
    if(0!=ret){
        goto code_error;
    }
    fd = hc_oci_get_fd((uint64_t)conn->srv);
    conn_item->fd=fd;
    ret = OCIHandleAlloc (conn->env, (void**)&(conn->stmt),OCI_HTYPE_STMT, 0, (void  **) 0);
    if(OCI_SUCCESS!=ret)
        goto code_error;
    return 0;
code_error:
    OCIHandleFree(conn->svc,OCI_HTYPE_SVCCTX);
    oci_checkerr(conn->err,ret);
    return -1;
}

int hc_oci_get_data(HC_ConnItem *conn_item)
{
    HC_OCIconn *conn=conn_item->ptr;
    OCIDefine *defnp=NULL;
    sword ret_oci;
    int i,tmp;
    HC_DBData *pdb=conn_item->dbdata;
    OCIParam *colhd=NULL;
    ub4 pacnt=0,utmp=0;
    ub2 ub2Tmp;
    char *pTmp=NULL;
    int fd = conn_item->fd;
    uint32_t ev;
    oci_checkerr(conn->err, OCIAttrGet((void *)conn->stmt, OCI_HTYPE_STMT, (void *)&pacnt,0, OCI_ATTR_PARAM_COUNT, conn->err));
    if(1>pacnt){
        return 0;
    }
    else if(pacnt>256) pacnt=256;
    if(!pdb){
        pdb = hc_dbdata_alloc();
        conn_item->dbdata = pdb;
    }else{
        hc_dbdata_reset(pdb);
    }
    HC_Array *pa=pdb->data;
    HC_ArrayData *pAH = NULL;
    pdb->col_num=pacnt;
    for(i=0;i<pacnt;){
        ret_oci = OCIParamGet(conn->stmt, OCI_HTYPE_STMT, conn->err, (void**)&colhd, ++i);
        ret_oci=OCIAttrGet(colhd, (ub4)OCI_DTYPE_PARAM, &pTmp,&utmp, (ub4) OCI_ATTR_NAME, conn->err);
        hc_array_append_item(pa,pTmp,utmp,TYPE_RAW);
        ub2Tmp=0;
        hc_array_append_data(pa,-2,&ub2Tmp,1,TYPE_RAW);
        ret_oci = OCIAttrGet(colhd, (ub4)OCI_DTYPE_PARAM, &utmp,NULL, (ub4)OCI_ATTR_DATA_SIZE, conn->err);
        ret_oci = OCIAttrGet(colhd, (ub4)OCI_DTYPE_PARAM, &ub2Tmp,NULL, (ub4)OCI_ATTR_DATA_TYPE, conn->err);
        if(ub2Tmp==96||ub2Tmp==1) utmp+=utmp; //96:CHAR, NCHAR  1:VARCHAR2, NVARCHAR2
        if(0==utmp){utmp=2;}
        hc_array_append_data(pa,-2,&utmp,sizeof(ub4),TYPE_RAW);
        pAH = hc_array_append_data(pa,-2,&ub2Tmp,sizeof(ub2),TYPE_RAW);
        pAH->len-=7;
        OCIDefineByPos(conn->stmt, &defnp, conn->err, i, NULL,1000, SQLT_CHR, (dvoid *) 0, (ub2 *)0,(ub2 *)0, OCI_DYNAMIC_FETCH);
    }
    ub4 dtype,  len_tmp=1000;
    ub1 up;
    int itmp,zt=0,j,k;
    for(;;){
        ret_oci = OCIStmtFetch2(conn->stmt, conn->err, 1,OCI_DEFAULT,0,OCI_DEFAULT);
        if(ret_oci==OCI_STILL_EXECUTING){
            ev = hc_co_wait_fd_timeout(fd,EPOLLIN|EPOLLRDHUP,60);
            if(!ev||(ev&EPOLLRDHUP)){
                goto code_error;
            }
            continue;
        }else if(OCI_NO_DATA == ret_oci){
            return 0;
        }else if(OCI_NEED_DATA!=ret_oci){
            oci_checkerr(conn->err, ret_oci);
            goto code_error;
        }
        pdb->row_num++;
        for(j=0,itmp=0;OCI_NEED_DATA == ret_oci;j++){
            ret_oci=OCIStmtGetPieceInfo(conn->stmt,conn->err,(void**)&defnp,
                    &dtype,(ub1*)&utmp,&utmp,&utmp,&up);
            //oci_checkerr(conn->err,ret_oci);
            if(OCI_FIRST_PIECE==up){
                if(pa->item_num>pdb->col_num){
                    pa->data_top = pa->data_top - (pAH->max_len -pAH->len);
                    pAH->max_len=pAH->len;
                }
                pAH = (HC_ArrayData*)hc_array_get_item_retptr(pa,itmp,&len_tmp)-1;
                len_tmp = *(ub4*)((char*)(pAH+1)+pAH->len+1);
                pAH=hc_array_append_data(pa,-1,NULL,len_tmp,TYPE_RAW);
                pTmp = (char*)(pAH+1);
                itmp++;
                if(itmp==pdb->col_num) itmp=0;
            }else{
                pAH=hc_array_append_data(pa,-2,NULL,len_tmp,TYPE_RAW);
                pTmp = (char*)(pAH+1)+pAH->len;
            }
            ret_oci=OCIStmtSetPieceInfo((void*)defnp,dtype,conn->err,
                    pTmp,&len_tmp,up,NULL,NULL);
            //oci_checkerr(conn->err,ret_oci);

            ret_oci=OCIStmtFetch2(conn->stmt, conn->err, 1,OCI_DEFAULT,0,OCI_DEFAULT);
            //oci_checkerr(conn->err,ret_oci);
            pTmp[len_tmp]=0;
            pAH->len+=len_tmp;
        }
    }
    return 0;
code_error:
    hc_dbdata_free(pdb);
    conn_item->dbdata = NULL;
    return -1;
}

int hc_oci_commit(HC_ConnItem *ci)
{
    HC_OCIconn *conn = ci->ptr;
    if(conn->commit){
        OCITransCommit(conn->svc,conn->err,0);
        conn->commit = 0;
    }
    return 0;
}

int hc_oci_check_conn(HC_ConnItem *conn_item)
{
    HC_OCIconn *conn=conn_item->ptr;
    if(!conn){
        HC_ConnPool *cp=conn_item->pool;
        conn = hc_oci_connect(cp->param);
        if(!conn){
            return -1;
        }
        conn_item->fd = conn->fd;
        conn_item->ptr=conn;
        return 0;
    }
    return 0;
}

