#pragma once
#ifndef uint32_t
    #include<stdint.h>
#endif

#define TYPE_NUM 2
#define TYPE_RAW 1
#define TYPE_BIN 3
#define TYPE_NULL 0
#define TYPE_LIST 4

#define BUF_TYPE_BUF 1
#define BUF_TYPE_PTR 0

#define ARG_TYPE_PTR 0
#define ARG_TYPE_2PTR 1

#define JS_TYPE_NULL 0
#define JS_TYPE_BUF 1
#define JS_TYPE_ARRAY 2
#define JS_TYPE_DICT 3
#define JS_TYPE_NUM 4
#define JS_TYPE_STR 5
#define JS_TYPE_BIN 6


typedef struct HC_MemBuf{
    uint8_t type;
    uint32_t status;
    union{
        uint32_t status2;
        uint32_t event;
    };
    uint32_t max;
    uint32_t len;
    uint32_t cur_len;
    char *buf;
}HC_MemBuf;

typedef struct __attribute__((packed, scalar_storage_order("big-endian")))
HC_JsBuf {
    uint8_t len_len:3;
    uint8_t type:5;
    union __attribute__((packed, scalar_storage_order("big-endian"))){
        uint8_t len1;
        uint16_t len2;
        uint32_t len4;
        uint64_t len8;
    };
}HC_JsBuf;
