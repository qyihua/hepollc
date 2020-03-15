#pragma once
#ifndef uint32_t
    #include<stdint.h>
#endif

typedef struct HC_CoMemPool{
    uint64_t top;
    uint64_t max;
    char *data;
}HC_CoMemPool;

typedef struct HC_CoMemBlock{
    uint8_t status:4; 
    uint8_t type:4; 
    uint32_t len_data;
    uint64_t len_block;
    uint64_t top_pre;
    uint64_t check;
    char data[0];
}HC_CoMemBlock;

int hc_co_memblock_free(HC_CoMemPool *mpool,uint64_t st);
HC_CoMemPool * hc_co_memblock_create(uint64_t size);
uint64_t hc_co_memblock_malloc(HC_CoMemPool *mpool,uint32_t len_data,int type);
