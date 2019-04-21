#ifndef __queue_h__
#define __queue_h__
#include "ring_buffer.h"
#include <inttypes.h>
#include "lcrq/lcrq.h"
typedef struct _KsiPackedUnsigned{
        unsigned val;
        unsigned pad[15];
} KsiPackedUnsigned;
typedef struct _KsiWorkQueue{
        int nprocs;
        KsiLCRQ masterQueue;
        KsiRingBuffer *rbs;
        KsiPackedUnsigned *seeds;
} KsiWorkQueue;
void ksiWorkQueueInit(KsiWorkQueue *wq,int nprocs);
void ksiWorkQueueDestroy(KsiWorkQueue *wq);
void ksiWorkQueueCommit(KsiWorkQueue *wq,int tid,void *data);
void *ksiWorkQueueGet(KsiWorkQueue *wq,int tid);
void ksiWorkQueueBeginTake(KsiWorkQueue *wq,int tid);
void *ksiWorkQueueTake(KsiWorkQueue *wq,int tid);
void ksiWorkQueueEndTake(KsiWorkQueue *wq,int tid);
void ksiWorkQueueTryFree(KsiWorkQueue *wq,int tid);// For GC
#endif
