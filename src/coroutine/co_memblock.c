#include <stdio.h>
#include <stdlib.h>

#include "../util/hc_util.h"

#include "co_memblock.h"
extern HC_CoMemPool *g_hc_co_mem_pool;

int hc_co_memblock_free(HC_CoMemPool *mpool,uint64_t st)
{
    if(!mpool) mpool=g_hc_co_mem_pool;
    uint64_t blockLen=0;
    uint64_t newSt=st, newEnd=0;
    
    char *pStack=mpool->data;
    HC_CoMemBlock *mem_block=(HC_CoMemBlock*)(pStack+st), *pMB_pre, *pMB_next;
    blockLen=mem_block->len_block;
    newEnd=st+blockLen;
    if(0==mem_block->status) return 0;
    mem_block->check = 0;
    mem_block->status=0;
    pMB_next = (HC_CoMemBlock*)(pStack+newEnd);

    if(st>1){
        pMB_pre = (HC_CoMemBlock*)(pStack+mem_block->top_pre);
        if(0==pMB_pre->status){
            newSt = mem_block->top_pre;
            pMB_pre->len_block = pMB_pre->len_block + mem_block->len_block;
            pMB_next->top_pre = newSt;
            mem_block = pMB_pre;
        }
    }
    if(0==pMB_next->status){
        ((HC_CoMemBlock*)(pStack+newEnd+pMB_next->len_block))->top_pre = newSt;
        mem_block->len_block += pMB_next->len_block;
    }
    if(mpool->top>newSt && mpool->top<newSt+mem_block->len_block){
        mpool->top=newSt;
    }
    return 0;
}

HC_CoMemPool * hc_co_memblock_create(uint64_t size)
{
    HC_CoMemPool *pTmp=NULL;
    pTmp = calloc(sizeof(HC_CoMemPool),1);
    pTmp->data = calloc(1+sizeof(HC_CoMemBlock)+size+sizeof(HC_CoMemBlock),1);
    if(!pTmp->data){
        perror("create memblock error");
        hc_printf_error("need: %f MB\n",((float)size)/1024/1024);
        return NULL;
    }
    pTmp->top=1;
    HC_CoMemBlock *mem_block=(HC_CoMemBlock*)(pTmp->data+1);
    mem_block->len_block = size+sizeof(HC_CoMemBlock);
    ((HC_CoMemBlock*)((char*)(mem_block+1)+size))->status=-1;
    pTmp->max = 1+size+sizeof(HC_CoMemBlock);
    return pTmp;
}

uint64_t hc_co_memblock_malloc(HC_CoMemPool *mpool,uint32_t len_data,int type)
{
    char *buf_stack=NULL;
    if(!mpool) mpool=g_hc_co_mem_pool;
    buf_stack=mpool->data;
    uint64_t top=mpool->top;
    uint64_t len_block;
    uint64_t max_top=mpool->max;
    uint64_t top_old, index_nextbl_old, index_nextbl_new;
    if(top+len_data+sizeof(HC_CoMemBlock)>=max_top){
        top=1;
    }
    HC_CoMemBlock *stack_block=(HC_CoMemBlock*)(buf_stack+top);
    HC_CoMemBlock *stack_block_tmp;
    len_block = stack_block->len_block;
    char flag=0;
    top_old = top;
    int ci=0;
    for(;;){
        ci++;
        if(stack_block->status){
            top=top+len_block;
            if(top>=max_top){
                top=1; flag++;
            }
            else if(flag){
                if(top_old<=top){
                    hc_printf_error(" mempool no space!!!!\n");
                    return 0;
                }
            }
        }
        else if(len_data<=len_block-sizeof(HC_CoMemBlock)){
            break;
        }
        else{
            top=top+len_block;
            if(top>=max_top){
                top=1;flag++;
            }
            else if(1==flag&&top_old<=top){
                    hc_printf_error(" mempool no space!!!!\n");
                return 0;
                flag++;
            }
        }
        stack_block=(HC_CoMemBlock*)(buf_stack+top);
        len_block = stack_block->len_block;
    }
    top_old = top;
    stack_block->type=type; // 000000 timefd needread
    uint64_t size_left = len_block-len_data-sizeof(HC_CoMemBlock);
    stack_block->len_data = len_data;
    stack_block->status=1;
    if(sizeof(HC_CoMemBlock)<size_left){
        stack_block->len_block = len_data+sizeof(HC_CoMemBlock);
        index_nextbl_old = top_old+len_block;
        index_nextbl_new = top_old+len_data+sizeof(HC_CoMemBlock);
        ((HC_CoMemBlock*)(buf_stack+index_nextbl_old))->top_pre=index_nextbl_new;
        stack_block_tmp = (HC_CoMemBlock*)(buf_stack+index_nextbl_new);
        stack_block_tmp->top_pre=top_old;
        stack_block_tmp->status=0;
        stack_block_tmp->len_block = size_left;
        top=index_nextbl_new;
    }
    else{
        top = top_old+len_block;
    }
    mpool->top=top;
    ((HC_CoMemBlock*)(buf_stack+top))->top_pre = top_old;
    stack_block->check = top_old;
    return top_old;
}


