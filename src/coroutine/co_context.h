#pragma once
#ifndef uint32_t
    #include<stdint.h>
#endif
#include "co_ioloop.h"

typedef struct HC_CoContext{
    uint64_t rsp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rbx;
    char data[0];
}HC_CoContext;

uint64_t hc_co_context_calloc(uint64_t rsp_cur);
void* hc_co_context_save_after_calloc(uint64_t top_block);
void* hc_co_context_save_after_calloc2(uint64_t top_block,uint64_t *regs);
void* hc_co_context_save_with_regs(uint64_t rsp_cur, uint64_t *top_ptr, uint64_t *regs);
void* hc_co_context_save(uint64_t rsp_cur, uint64_t *top_ptr);
uint64_t hc_co_context_save_with_base(uint64_t rbp_base, uint64_t rsp_cur);
int hc_co_return(uint64_t rbp_pre, uint64_t ret);
uint64_t hc_co_context_save_pre();
int hc_co_context_restore_pre(uint64_t top_block);
int hc_co_context_resume(uint64_t top_block,uint64_t ret);
int hc_co_context_resume_cofd(HC_CoFd *cofd);
