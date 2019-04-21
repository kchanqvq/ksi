#ifndef __spsc_buffer_h__
#define __spsc_buffer_h__
#include <inttypes.h>
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include "_config.h"
#include "util.h"
#ifndef ALIGN
#define ALIGN __attribute__((aligned(_CONFIG_CACHE_SIZE*2)))
#endif
typedef struct _KsiSPSCBuffer{
        _Atomic uint64_t head ALIGN;
        _Atomic uint64_t tail ALIGN;
        char buffer[];
} KsiSPSCBuffer;
static inline KsiSPSCBuffer *ksiSPSCBufferAlloc(size_t dataSize,size_t bufferSize){
        KsiSPSCBuffer *ret = ksiMalloc(sizeof *ret + dataSize * bufferSize);
        atomic_init(&ret->head, 0);
        atomic_init(&ret->tail, 0);
        return ret;
}
static inline void ksiSPSCBufferFree(KsiSPSCBuffer *buf){
        free(buf);
}
static inline int ksiSPSCBufferPush(KsiSPSCBuffer *buf,char *data,size_t dataSize,size_t bufferSize){
        uint64_t _head = atomic_load_explicit(&buf->head, memory_order_relaxed);
        uint64_t _tail = atomic_load_explicit(&buf->tail, memory_order_acquire);
        if(_head >= _tail + bufferSize)
                return -1;
        memcpy(&buf->buffer[0]+_head*dataSize, data, dataSize);
        atomic_store_explicit(&buf->head, _head+1, memory_order_release);
        return 0;
}
static inline int ksiSPSCBufferPop(KsiSPSCBuffer *buf,char *data,size_t dataSize,size_t bufferSize){
        uint64_t _head = atomic_load_explicit(&buf->head,memory_order_acquire);
        uint64_t _tail = atomic_load_explicit(&buf->tail,memory_order_relaxed);
        if(_head <= _tail)
                return -1;
        memcpy(data, &buf->buffer[0]+_tail*dataSize, dataSize);
        atomic_store_explicit(&buf->tail, _tail+1, memory_order_release);
        return 0;
}
#endif
