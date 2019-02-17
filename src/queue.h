#ifndef __queue_h__
#define __queue_h__
#include "ring_buffer.h"
#include <inttypes.h>
typedef struct _KsiPackedUnsigned{
        unsigned val;
        unsigned pad[15];
} KsiPackedUnsigned;
typedef struct _KsiWorkQueue{
        int nprocs;
        KsiRingBuffer *rbs;
        KsiPackedUnsigned *seeds;
} KsiWorkQueue;
void ksiWorkQueueInit(KsiWorkQueue *wq,int nprocs);
void ksiWorkQueueDestroy(KsiWorkQueue *wq);
void ksiWorkQueueCommit(KsiWorkQueue *wq,int tid,void *data);
void *ksiWorkQueueGet(KsiWorkQueue *wq,int tid);
void *ksiWorkQueueTake(KsiWorkQueue *wq,int tid);
#endif
