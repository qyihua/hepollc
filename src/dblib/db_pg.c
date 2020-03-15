#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <string.h>
#include <arpa/inet.h>

#include "db_pg.h"

extern HC_Dict *g_confs;
static long l_mt_pgvals[7];

int hc_pg_wait_resetpoll(PGconn *conn);
int hc_pg_wait_connpoll(PGconn *conn);
int hc_pg_recycle(HC_ConnItem *ci, int nokeep);

int libdb_init_handle(HC_DBPool *pool)
{
    pool->check_conn = hc_pg_check_conn;
    pool->recycle_conn = hc_pg_recycle;
    pool->sql_exe = hc_pg_sql_exe_hc;
    return 0;
}

int hc_pg_recycle(HC_ConnItem *conn, int nokeep)
{
    if(0==nokeep) return 0;
    PQfinish(conn->ptr);
    conn->ptr = NULL;
    conn->fd= -1;
    return 0;
}

int hc_pg_check_conn(HC_ConnItem *conn_item)
{
    PGconn *conn=conn_item->ptr;
    int ret;
    if(!conn){
        HC_ConnPool *cp=conn_item->pool;
        conn = hc_pg_connect(cp->param,
                cp->param?hc_dict_get_ptr(cp->param, "connstr",7,&ret):NULL);
        if(!conn){
            conn_item->ptr=NULL;
            conn_item->fd = -1;
            return -1;
        }
        conn_item->fd = PQsocket(conn);
        conn_item->ptr=conn;
        return 0;
    }
    ConnStatusType connStatusType;
    connStatusType = PQstatus(conn);
    if(CONNECTION_OK!=connStatusType){
        ret = PQresetStart(conn);
        if(0==ret){
            conn_item->ptr=NULL;
            PQfinish(conn);
            conn_item->fd = -1;
            return -1;
        }
        ret = hc_pg_wait_connpoll(conn);
        if(-1==ret){
            conn_item->ptr=NULL;
            conn_item->fd = -1;
            return -1;
        }
        conn_item->fd=ret;
        conn_item->ptr=conn;
    }
    return 0;
}

PGconn* hc_pg_start_param(HC_Dict *dict, char *conn_str)
{
    char **keys=NULL, **vals=NULL;
    int i, ret=0;
    if(dict){
        i = dict->item_num+2;
        keys = calloc(sizeof(char*),i);
        vals = calloc(sizeof(char*),i);
        i=0;
        if(conn_str){
            keys[0] = "dbname";
            vals[0] = conn_str;
            ret = 1;
            i=1;
        }
        int kindex=0;
        HC_DictItem *di;
        char *key_buf = dict->key_mem.buf;
        char *data_buf = dict->data_mem.buf;
        do{
            di = hc_dict_get_item_by_kindex(dict, &kindex);
            if(!di) break;
            if(1>di->data_index)
                continue;
            if(di->user_flag) continue;
            keys[i] = key_buf+di->key_index;
            vals[i++] = data_buf+di->data_index;
        }while(di);
    }else{
        if(conn_str){
            keys = calloc(sizeof(char*),2);
            vals = calloc(sizeof(char*),2);
            keys[0] = "dbname";
            vals[0] = conn_str;
            ret = 1;
        }

    }
    PGconn *conn=NULL;
    conn =  PQconnectStartParams((const char*const*)keys, (const char*const*)vals, ret);
    if(keys){
        free(keys);
        free(vals);
    }
    return conn;
}

PGconn* hc_pg_connect(HC_Dict *param, char *conn_str)
{
    PGconn *conn=hc_pg_start_param(param,conn_str);
    if(!conn){
        return NULL;
    }
    if(-1==PQsetnonblocking(conn, 1)){
        PQfinish(conn);
        return NULL;
    }
    ConnStatusType connStatusType;
    connStatusType = PQstatus(conn);
    if(CONNECTION_BAD==connStatusType){
        printf(" pg conn error:%d,%d,:%s\n",connStatusType,CONNECTION_BAD,PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }else if(CONNECTION_OK==connStatusType){
        return conn;
    }
    int fd = hc_pg_wait_connpoll(conn);
    if(-1==fd) return NULL;
    if(-1==PQsetnonblocking(conn, 1)){
        PQfinish(conn);
        return NULL;
    }
    return conn;
}

int hc_pg_wait_connpoll(PGconn *conn)
{
    PostgresPollingStatusType state=PQconnectPoll(conn);
    uint32_t ev;
    int fd = PQsocket(conn);
    for(;;){
        if(PGRES_POLLING_READING == state){
            ev = hc_co_wait_fd_timeout(fd,EPOLLIN|EPOLLRDHUP,10);
        }else if(PGRES_POLLING_WRITING==state){
            ev = hc_co_wait_fd_timeout(fd,EPOLLOUT|EPOLLRDHUP,10);
        }else if(PGRES_POLLING_FAILED==state){
            printf("pg conn error:%s\n",PQerrorMessage(conn));
            PQfinish(conn);
            return -1;
        }else{
            break;
        }
        if(!ev){
            PQfinish(conn);
            return -1;
        }
        state=PQconnectPoll(conn);
        if(ev&EPOLLRDHUP){
            printf("pg conn error:%s\n",PQerrorMessage(conn));
            PQfinish(conn);
            return -1;
        }
        fd = PQsocket(conn);
    }
    return fd;
}

int hc_pg_wait_resetpoll(PGconn *conn)
{
    PostgresPollingStatusType state=PQresetPoll(conn);
    uint32_t ev;
    int fd = PQsocket(conn);
    for(;;){
        if(PGRES_POLLING_READING == state){
            ev = hc_co_wait_fd_timeout(fd,EPOLLIN|EPOLLRDHUP,5);
        }else if(PGRES_POLLING_WRITING==state){
            ev = hc_co_wait_fd_timeout(fd,EPOLLOUT|EPOLLRDHUP,5);
        }else if(PGRES_POLLING_FAILED==state){
            printf("pg conn reset error:%s\n",PQerrorMessage(conn));
            PQfinish(conn);
            return -1;
        }else{
            break;
        }
        if(!ev){
            PQfinish(conn);
            return -1;
        }
        state=PQresetPoll(conn);
        if(ev&EPOLLRDHUP){
            printf("pg conn reset error:%s\n",PQerrorMessage(conn));
            PQfinish(conn);
            return -1;
        }
        fd = PQsocket(conn);
    }
    return fd;
}

const char *g_pg_paramValues[100];
int g_pg_paramLengths[100];
int g_pg_paramFormats[100]={0};

int hc_pg_sql_exe_hc(HC_ConnItem *ci,char *sql, int sql_len,void *params,
        int param_len, int param_start)
{
    return hc_pg_sql_exe(ci, sql, params, param_len, param_start);
}

int hc_pg_sql_exe(HC_ConnItem *pCI,char *sql_str,void *params,
        int param_len, int param_start)
{
    PGconn *pgConn=pCI->ptr;
    int ret,i;
    if(params){
        if(-1==param_start){
            if(0==param_len)
                i=0;
            else{
                if(-1==param_len) param_len=strlen((char*)params);
                g_pg_paramValues[0]=params;
                g_pg_paramLengths[0]=param_len;
                g_pg_paramFormats[0]=1;
                i=1;
            }
        }else{
            if(-1==param_len) param_len=((HC_Array*)params)->item_num;
            int end=param_start+param_len,pLen;
            char *pTmp;
            HC_ArrayData *pAH;
            if(end>((HC_Array*)params)->item_num) 
                end=((HC_Array*)params)->item_num;
            for(i=0;param_start<end;param_start++,i++){
                pTmp=hc_array_get_item_retptr((HC_Array*)params,
                        param_start,&pLen);
                g_pg_paramValues[i]=pTmp;
                g_pg_paramLengths[i]=pLen;
                pAH=(HC_ArrayData*)pTmp-1;
                switch(pAH->type){
                    case TYPE_NUM:
                        switch(pLen){
                            case 4://int4
                                *(int*)pTmp=htonl(*(int*)pTmp);
                            case 8://int8
                                ret=ntohl(*(int*)pTmp);
                                *(int*)pTmp=htonl(*((int*)pTmp+1));
                                *((int*)pTmp+1)=ret;
                            case 2://int2
                                *(short*)pTmp=ntohs(*(short*)pTmp);
                        }
                    case TYPE_BIN:
                        ret=1;
                        break;
                    default:
                        if(1==pAH->type2){
                            ret=1;
                        }
                        else
                            ret=0;
                        break;
                }
                g_pg_paramFormats[i]=ret;
            }
        }
        if(0==PQsendQueryParams(pgConn,sql_str,i,NULL,g_pg_paramValues,g_pg_paramLengths,g_pg_paramFormats,1)){
            goto ErrorTag;
        }
    }
    else{
        if(0==PQsendQuery(pgConn,sql_str)){
            goto ErrorTag;
        }
    }
    int fd= pCI->fd;
    uint32_t ev;
    for(;1==(ret = PQflush(pgConn));){
        ev = hc_co_wait_fd_timeout(fd,EPOLLOUT|EPOLLRDHUP,60);
        if(ev&EPOLLRDHUP){
            printf("pg flush error:%s\n",PQerrorMessage(pgConn));
            PQfinish(pgConn);
            return -1;
        }
        continue;
    }
    for(;;){
        ret=PQconsumeInput(pgConn);
        if(0==ret) goto ErrorTag;
        ret=PQisBusy(pgConn);
        if(0==ret) break;
        ev = hc_co_wait_fd_timeout(fd,EPOLLIN|EPOLLRDHUP,60);
        if(ev&EPOLLRDHUP){
            printf("pg consume error:%s\n",PQerrorMessage(pgConn));
            PQfinish(pgConn);
            goto ErrorTag;
        }
    }
    if(2==pCI->custom_flg){
        return 0;
    }else{
        HC_DBData *pdb=pCI->dbdata;
        if(!pdb){
            pdb = hc_dbdata_alloc();
            pCI->dbdata = pdb;
        }else{
            hc_dbdata_reset(pdb);
        }
        ret = hc_pg_get_data(pgConn,pdb);
        if(0>ret){
            printf("pg database error :%s\n", PQerrorMessage(pgConn));
            hc_dbdata_free(pdb);
            return -1;
        }
    }
    return 0;
ErrorTag:
    if(pCI->dbdata){
        hc_dbdata_free(pCI->dbdata);
        pCI->dbdata=NULL;
    }
    printf("database error:%s\n", PQerrorMessage(pgConn));
    return -1;
}

int hc_pg_get_data(PGconn *pgConn,HC_DBData *pdb)
{
    PGresult *result=PQgetResult(pgConn);
    if(PGRES_FATAL_ERROR==PQresultStatus(result)){
        for(;result!= NULL;result=PQgetResult(pgConn)) {
            PQclear(result);
        }
        return -1;
    }
    int cols=0, rows=0, i,c,i2;
    HC_Array *pA=pdb->data;
    int type, len;
    char *pTmp;
    long num;
    for(i2=0;result!=NULL;i2++,result=PQgetResult(pgConn)){
        if(0<i2){
            PQclear(result);
            continue;
        }
        cols=PQnfields(result);
        pdb->col_num=cols;
        for(c=0;c<cols;c++){
            pTmp=PQfname(result,c);
            hc_array_append_item(pA,pTmp,-1,TYPE_RAW);
        }
        rows = PQntuples(result);
        pdb->row_num=rows;
        if(0==rows){
            PQclear(result);
            continue;
        }
        for(i=0;i<rows;i++){
            for(c=0;c<cols;c++){
                type=PQgetisnull(result,i,c);
                if(type){
                    type=TYPE_NULL;
                    len=0;
                    pTmp=NULL;
                }
                else{
                    pTmp = PQgetvalue(result,i,c);
                    len = PQgetlength(result,i,c);
                    type = PQfformat(result,c);
                    if(0==type) type=TYPE_RAW;
                    else{
                        type = PQftype(result,c);
                        switch(type){
                            case 23://int4
                            case 1082://date
                                num=be32toh(*(int*)pTmp);
                                type=TYPE_NUM;
                                break;
                            case 20://int8
                            case 1083://time
                            case 1114://timestamp
                                num=be64toh(*(long*)pTmp);
                                type=TYPE_NUM;
                                break;
                            case 21://int2
                                num=be16toh(*(short*)pTmp);
                                type=TYPE_NUM;
                                break;
                            case 17:
                                type=TYPE_BIN;
                                break;
                            default:
                                type=TYPE_RAW;
                                break;
                        }
                    }
                }
                if(TYPE_NUM==type)
                    hc_array_append_item(pA, (void*)num, len, type);
                else
                    hc_array_append_item(pA, pTmp, len, type);
            }
        }
        PQclear(result);
    }
    return 0;
}
