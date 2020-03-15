#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coroutine.h"

HC_CoMemPool *g_hc_co_mem_pool;
extern HC_CoLoop *g_co_loop;
uint64_t g_coroutine_base;

uint64_t hc_co_context_calloc(uint64_t rsp_cur)
{
    if(!rsp_cur)
        rsp_cur = (uint64_t)__builtin_frame_address(0);
    
    int len_frame = g_coroutine_base-rsp_cur;
    uint64_t top_block = hc_co_memblock_malloc(NULL,len_frame+sizeof(HC_CoContext),0);
    if(1>top_block){
        return 0;
    }
    HC_CoContext *co_stack=(HC_CoContext*)(g_hc_co_mem_pool->data+top_block+sizeof(HC_CoMemBlock));
//    *(uint64_t*)(g_hc_co_mem_pool->data+top_block+sizeof(HC_CoMemBlock)+offsetof(HC_CoContext, rsp)) = rsp_cur;
    co_stack->rsp = rsp_cur;
    co_stack->r12 = *(uint64_t*)rsp_cur;
    return top_block;
}

void* hc_co_context_save_after_calloc(uint64_t top_block)
{
    
    HC_CoMemBlock *stack_block = (HC_CoMemBlock*)(g_hc_co_mem_pool->data+top_block);
    HC_CoContext *co_stack=(HC_CoContext*)stack_block->data;
    uint64_t rsp_cur = co_stack->rsp;
    *(u_int64_t*)rsp_cur = co_stack->r12;
    __asm__(
           "movq %%r12,%0;" 
           "movq %%r13,%1;" 
           "movq %%r14,%2;" 
           "movq %%r15,%3;" 
           "movq %%rbx,%4;" 
           :"=m"(co_stack->r12),"=m"(co_stack->r13),"=m"(co_stack->r14),"=m"(co_stack->r15),"=m"(co_stack->rbx)
           :
           :"r12","r13","r14","r15","rbx"
            );
    
    co_stack->rsp = rsp_cur;
    int len_data = stack_block->len_data-sizeof(HC_CoContext);
    memcpy(co_stack->data, (void*)rsp_cur, len_data);
    __asm__(
            "movq %0,%%rbp;"
            "leave;"
            "ret;"
            :
            :"r"(g_coroutine_base)
            );
    return NULL;
}

void* hc_co_context_save_after_calloc2(uint64_t top_block,uint64_t *regs)
{
    
    HC_CoMemBlock *stack_block = (HC_CoMemBlock*)(g_hc_co_mem_pool->data+top_block);
    HC_CoContext *co_stack=(HC_CoContext*)stack_block->data;
    uint64_t rsp_cur = co_stack->rsp;
    *(u_int64_t*)rsp_cur = co_stack->r12;
        co_stack->r12 = regs[0];
        co_stack->r13 = regs[1];
        co_stack->r14 = regs[2];
        co_stack->r15 = regs[3];
        co_stack->rbx = regs[4];
    
    co_stack->rsp = rsp_cur;
    int len_data = stack_block->len_data-sizeof(HC_CoContext);
    memcpy(co_stack->data, (void*)rsp_cur, len_data);
    __asm__(
            "movq %0,%%rbp;"
            "leave;"
            "ret;"
            :
            :"r"(g_coroutine_base)
            );
    return NULL;
}

void* hc_co_context_save_with_regs(uint64_t rsp_cur, uint64_t *top_ptr, uint64_t *regs)
{
    if(0==rsp_cur)
        rsp_cur = (uint64_t)__builtin_frame_address(0);
    
    int len_frame = g_coroutine_base-rsp_cur;
    uint64_t top_block = hc_co_memblock_malloc(NULL,len_frame+sizeof(HC_CoContext),0);
    if(0==top_block) return NULL;
    HC_CoMemBlock *stack_block = (HC_CoMemBlock*)(g_hc_co_mem_pool->data+top_block);
    HC_CoContext *co_stack=(HC_CoContext*)stack_block->data;
    //uint64_t rip = *((uint64_t*)rsp_cur+1);
    co_stack->rsp = rsp_cur;
        co_stack->r12 = regs[0];
        co_stack->r13 = regs[1];
        co_stack->r14 = regs[2];
        co_stack->r15 = regs[3];
        co_stack->rbx = regs[4];
    if(top_ptr) *top_ptr = top_block;
    memcpy(co_stack->data, (void*)rsp_cur, len_frame);
    //*((uint64_t*)co_stack->data+1) = (uint64_t)rip;
    __asm__(
            "movq %0,%%rbp;"
            "leave;"
            "ret;"
            :
            :"r"(g_coroutine_base)
            );
    return stack_block;
}


void* hc_co_context_save(uint64_t rsp_cur, uint64_t *top_ptr)
{
    if(0==rsp_cur)
        rsp_cur = (uint64_t)__builtin_frame_address(0);
    
    int len_frame = g_coroutine_base-rsp_cur;
    uint64_t top_block = hc_co_memblock_malloc(NULL,len_frame+sizeof(HC_CoContext),0);
    if(0==top_block){
        if(top_ptr) *top_ptr = 0;
        return NULL;
    }
    HC_CoMemBlock *stack_block = (HC_CoMemBlock*)(g_hc_co_mem_pool->data+top_block);
    HC_CoContext *co_stack=(HC_CoContext*)stack_block->data;
    //uint64_t rip = *((uint64_t*)rsp_cur+1);
    co_stack->rsp = rsp_cur;
        __asm__(
               "movq %%r12,%0;" 
               "movq %%r13,%1;" 
               "movq %%r14,%2;" 
               "movq %%r15,%3;" 
               "movq %%rbx,%4;" 
               :"=m"(co_stack->r12),"=m"(co_stack->r13),"=m"(co_stack->r14),"=m"(co_stack->r15),"=m"(co_stack->rbx)
               :
               :"r12","r13","r14","r15","rbx"
                );
    if(top_ptr){*top_ptr = top_block;}
    
    memcpy(co_stack->data, (void*)rsp_cur, len_frame);
    //*((uint64_t*)co_stack->data+1) = (uint64_t)rip;
    __asm__(
            "movq %0,%%rbp;"
            "leave;"
            "ret;"
            :
            :"r"(g_coroutine_base)
            );
    return stack_block;
}

uint64_t hc_co_context_save_with_base(uint64_t rbp_base, uint64_t rsp_cur)
{
    if(0==rsp_cur)
        rsp_cur = (uint64_t)__builtin_frame_address(0);
    
    int len_frame = rbp_base-rsp_cur;
    uint64_t top_block = hc_co_memblock_malloc(NULL,len_frame+sizeof(HC_CoContext),0);
    if(0==top_block) return 0;
    HC_CoMemBlock *stack_block = (HC_CoMemBlock*)(g_hc_co_mem_pool->data+top_block);
    HC_CoContext *co_stack=(HC_CoContext*)stack_block->data;
        __asm__(
               "movq %%r12,%0;" 
               "movq %%r13,%1;" 
               "movq %%r14,%2;" 
               "movq %%r15,%3;" 
               "movq %%rbx,%4;" 
               :"=m"(co_stack->r12),"=m"(co_stack->r13),"=m"(co_stack->r14),"=m"(co_stack->r15),"=m"(co_stack->rbx)
               :
               :"r12","r13","r14","r15","rbx"
                );
    co_stack->rsp = g_coroutine_base - len_frame;
    memcpy(co_stack->data, (void*)rsp_cur, len_frame);
    *((uint64_t*)co_stack->data) = g_coroutine_base;
    return top_block;
}

int hc_co_return(uint64_t rbp_pre, uint64_t ret)
{
    uint64_t rbp_cur = (uint64_t)__builtin_frame_address(0);
    uint64_t top;
    top=hc_co_context_save_with_base(rbp_pre,rbp_cur);
    if(!top){
        return -1;
    }
    if(NULL==hc_co_add_callback(hc_co_context_resume,(void*)top, (void*)1)){
        hc_co_memblock_free(NULL,top);
        return -1;
    }
    return 0;
}

uint64_t hc_co_context_save_pre()
{
    uint64_t rbp_base = g_coroutine_base;
    uint64_t rbp_cur = (uint64_t)__builtin_frame_address(0);
    uint64_t rbp_pre = *(uint64_t*)rbp_cur;
    
    int len_frame_save = rbp_base - rbp_pre;
    uint64_t top_block = hc_co_memblock_malloc(NULL,len_frame_save+sizeof(HC_CoContext),0);
    if(1>top_block) return 0;
    HC_CoMemBlock *stack_block = (HC_CoMemBlock*)(g_hc_co_mem_pool->data+top_block);
    HC_CoContext *co_stack=(HC_CoContext*)stack_block->data;
    co_stack->rsp = rbp_pre;
    memcpy(co_stack->data, (void*)rbp_pre, len_frame_save);

    uint64_t rsp_cur;
    __asm__(
            "movq %%rsp,%0;"
            :"=r"(rsp_cur)
            );
    uint64_t rsp_new;

    int len_frame_cur = rbp_pre - rsp_cur;
    int offset = len_frame_save;

    *(uint64_t*)rbp_cur += offset;

    if(len_frame_cur>offset){
        offset = len_frame_cur;
        rsp_new = rsp_cur - offset;
        rbp_cur -= offset;
        __asm__(
                "movq %0,%%rsp;"
                :
                :"r"(rsp_new)
                );
        memcpy((void*)rsp_new, (void*)rsp_cur, len_frame_cur);
        __asm__(
                "movq %0,%%rbp;"
                :
                :"r"(rbp_cur)
                );
        offset+=len_frame_save;
        rsp_cur = rsp_new;
    }

    rbp_cur = rbp_cur + offset;
    rsp_new = rsp_cur + offset;

    memcpy((void*)rsp_new, (void*)rsp_cur, len_frame_cur);

    __asm__(
            "movq %0,%%rbp;"
            "movq %1,%%rsp;"
            :
            :"r"(rbp_cur),"r"(rsp_new)
            );
    return top_block;
}

int hc_co_context_restore_pre(uint64_t top_block)
{
    uint64_t rbp_base = g_coroutine_base;
    uint64_t rbp_cur = (uint64_t)__builtin_frame_address(0);

    HC_CoMemBlock *stack_block = (HC_CoMemBlock*)(g_hc_co_mem_pool->data+top_block);
    HC_CoContext *co_stack = (HC_CoContext*)stack_block->data;
    
    int len_frame_resotre = stack_block->len_data - sizeof(HC_CoContext);

    int offset=len_frame_resotre;
    uint64_t rsp_cur;
    __asm__(
            "movq %%rsp,%0;"
            :"=r"(rsp_cur)
            );
    int len_frame_cur = rbp_base - rsp_cur;
    uint64_t rsp_new;
    *(uint64_t*)rbp_cur -= offset;

    if(offset<len_frame_cur){
        offset += len_frame_cur;
        rsp_new = rsp_cur - offset;
        rbp_cur -= offset;
        __asm__(
                "movq %0,%%rsp;"
                :
                :"r"(rsp_new)
                );
        memcpy((void*)rsp_new, (void*)rsp_cur, len_frame_cur);
        __asm__(
                "movq %0,%%rbp;"
                :
                :"r"(rbp_cur)
                );
        offset = -len_frame_cur;
        rsp_cur = rsp_new;
    }
    
    rbp_cur -= offset;

    rsp_new = rsp_cur - offset;
    if(rsp_new>rsp_cur){
        memcpy((void*)rsp_new, (void*)rsp_cur, len_frame_cur);
        __asm__(
                "movq %0,%%rbp;"
                "movq %1,%%rsp;"
                :
                :"r"(rbp_cur),"r"(rsp_new)
                );
    }else{
        __asm__(
                "movq %0,%%rsp;"
                :
                :"r"(rsp_new)
                );
        memcpy((void*)rsp_new, (void*)rsp_cur, len_frame_cur);
        __asm__(
                "movq %0,%%rbp;"
                :
                :"r"(rbp_cur)
                );
    }

    uint64_t rsp_pre = co_stack->rsp;

    memcpy((void*)rsp_pre, co_stack->data, len_frame_resotre);
    hc_co_memblock_free(NULL,top_block);

    return 0;
}

int hc_co_context_resume(uint64_t top_block,uint64_t ret)
{
    uint64_t rbp_cur = (uint64_t)__builtin_frame_address(0);
    HC_CoMemBlock *stack_block = (HC_CoMemBlock*)(g_hc_co_mem_pool->data+top_block);
    HC_CoContext *co_stack=(HC_CoContext*)stack_block->data;
    short int len_data = stack_block->len_data;
    uint64_t rsp,rsp_new;
    if(stack_block->check!=top_block || 0==len_data || 0==stack_block->status){
        return 0;
    }
    
    hc_co_memblock_free(NULL,top_block);
    __asm__(
            "movq %%rsp, %0;"
            :"=r"(rsp)
           );
    int len_stack= rbp_cur - rsp;
    if(co_stack->rsp > rbp_cur){
        goto code_resume;
    }
    if(co_stack->rsp > rsp){
        rsp_new = rsp - len_stack;
    }else{
        rsp_new = co_stack->rsp-len_stack;
    }

    __asm__(
        "movq %0,%%rsp;"
        :
        :"r"(rsp_new)
    );
    memcpy((void*)(rsp_new), (void*)(rsp), len_stack);
    rbp_cur = rsp_new + len_stack;
    __asm__(
        "movq %0,%%rbp;"
        :
        :"r"(rbp_cur)
    );

code_resume:
    len_stack = stack_block->len_data - sizeof(HC_CoContext);
    rsp = co_stack->rsp;
    memcpy((void*)rsp, co_stack->data, len_stack);
    __asm__(
           "movq %0,%%r12;" 
           "movq %1,%%r13;" 
           "movq %2,%%r14;" 
           "movq %3,%%r15;" 
           "movq %4,%%rbx;" 
           :
           :"m"(co_stack->r12),"m"(co_stack->r13),"m"(co_stack->r14),"m"(co_stack->r15),"m"(co_stack->rbx)
           :"r12","r13","r14","r15","rbx"
            );
    __asm__(
        "movq %0,%%rax;"
        "movq %1,%%rsp;"
        "popq %%rbp;"
        "ret;"
        :
        :"r"(ret),"r"(rsp)
        :"%rax","rbx","r12","r13","r14","r15"
    );
}

int hc_co_context_resume_cofd(HC_CoFd *cofd)
{
    uint64_t rbp_cur = (uint64_t)__builtin_frame_address(0);
    uint64_t top_block = cofd->u64;
    HC_CoMemBlock *stack_block = (HC_CoMemBlock*)(g_hc_co_mem_pool->data+top_block);
    HC_CoContext *co_stack=(HC_CoContext*)stack_block->data;
    short int len_data = stack_block->len_data;
    uint64_t rsp, rsp_new;
    if(stack_block->check!=top_block || 0==len_data || 0==stack_block->status){
        return 0;
    }

    hc_co_memblock_free(NULL,top_block);
    __asm__(
            "movq %%rsp, %0;"
            :"=r"(rsp)
           );
    int len_stack= rbp_cur - rsp;
    if(co_stack->rsp > rbp_cur){
        goto code_resume;
    }
    if(co_stack->rsp > rsp){
        rsp_new = rsp - len_stack;

    }else{
        rsp_new = co_stack->rsp-len_stack;
    }

    __asm__(
        "movq %0,%%rsp;"
        :
        :"r"(rsp_new)
    );
    memcpy((void*)(rsp_new), (void*)(rsp), len_stack);
    rbp_cur = rsp_new + len_stack;


    __asm__(
        "movq %0,%%rbp;"
        :
        :"r"(rbp_cur)
    );

code_resume:
    len_stack = stack_block->len_data - sizeof(HC_CoContext);
    rsp = co_stack->rsp;
    memcpy((void*)rsp, co_stack->data, len_stack);
    __asm__(
           "movq %0,%%r12;" 
           "movq %1,%%r13;" 
           "movq %2,%%r14;" 
           "movq %3,%%r15;" 
           "movq %4,%%rbx;" 
           :
           :"m"(co_stack->r12),"m"(co_stack->r13),"m"(co_stack->r14),"m"(co_stack->r15),"m"(co_stack->rbx)
           :"r12","r13","r14","r15","rbx"
            );
    __asm__(
        "movq %0,%%rax;"
        "movq %1,%%rsp;"
        "popq %%rbp;"
        "ret;"
        :
        :"r"(cofd),"r"(rsp)
        :"%rax","rbx","r12","r13","r14","r15"
    );
}
