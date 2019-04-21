#ifndef __ring_buffer_h__
#define __ring_buffer_h__
#include <inttypes.h>
#include <stdatomic.h>
#include "_config.h"
#define ALIGN __attribute__((aligned(_CONFIG_CACHE_SIZE*2)))
//#define ALIGN
#define ksiRingBufferNodeLength (1<<8)
#define ksiRingBufferFailedVal ((void *)-1)
#define ksiRingBufferEBRThreshold 10000
typedef struct _KsiRingBufferPtr{
        void *val;
        //void *pad[15];
} KsiRingBufferPtr ALIGN;
typedef struct _KsiRingBufferSegment{
        struct _KsiRingBufferSegment * prev ALIGN;
        KsiRingBufferPtr buffer[ksiRingBufferNodeLength];
        _Atomic uint64_t head_idx ALIGN;
        _Atomic uint64_t tail_idx ALIGN;
        _Atomic(struct _KsiRingBufferSegment *) next ALIGN;
} KsiRingBufferSegment ALIGN;
typedef struct _KsiRingBufferEBREntry{
        _Atomic int64_t active;
        uint64_t epoch;
        KsiRingBufferSegment *freelist[3];
        int64_t pad[(_CONFIG_CACHE_SIZE*2-8*3-sizeof(KsiRingBufferSegment *))/8];
} KsiRingBufferEBREntry ALIGN;
typedef struct _KsiRingBuffer{
        KsiRingBufferSegment * head;
        _Atomic(KsiRingBufferSegment *) tail ALIGN;
        _Atomic uint64_t epoch;
        KsiRingBufferEBREntry ebrEntries[];//Need to allocate sizeof(KsiRingBufferEBREntry)*nprocs space
} KsiRingBuffer ALIGN;
void ksiRingBufferInit(KsiRingBuffer *rb,int nprocs);
static inline void ksiRingBufferEBREntryInit(KsiRingBufferEBREntry *ee){
        atomic_init(&ee->active, 0);
        ee->epoch = 0;
        int i = 3;
        while(i --)
                ee->freelist[i] = NULL;
}
#ifdef CNT
int
#else
void
#endif
destroyFromTail(KsiRingBufferSegment *tail);
void ksiRingBufferPush(KsiRingBuffer *rb,void *data,int tid);
void ksiRingBufferConsumerEnter(KsiRingBuffer *rb,int tid);
void ksiRingBufferConsumerLeave(KsiRingBuffer *rb,int tid);
void *ksiRingBufferPop(KsiRingBuffer *rb,int nprocs,int tid);
void *ksiRingBufferTake(KsiRingBuffer *rb,int nprocs,int tid);
int ksiRingBufferTryFree(KsiRingBuffer *rb,int nprocs,int tid);


void ksiRingBufferDestroy(KsiRingBuffer *rb,int nprocs);
#endif
