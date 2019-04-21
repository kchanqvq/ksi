#include "queue.h"
#include <stdlib.h>
static inline KsiRingBuffer *nth(KsiWorkQueue *wq,int tid){
        int nprocs = wq->nprocs;
        return (KsiRingBuffer *)(((char *)wq->rbs) + (sizeof(KsiRingBuffer) + sizeof(KsiRingBufferEBREntry)*nprocs)*tid);
}
void ksiWorkQueueInit(KsiWorkQueue *wq,int nprocs){
        wq->nprocs = nprocs;
        wq->rbs = malloc((sizeof(KsiRingBuffer)+sizeof(KsiRingBufferEBREntry)*nprocs)*nprocs);

        wq->seeds = malloc(sizeof(KsiPackedUnsigned)*nprocs);
        int i = nprocs;
        ksiLCRQInit(&wq->masterQueue, nprocs);
        while(i--){
                ksiRingBufferInit(nth(wq, i),nprocs);
        }
}
void ksiWorkQueueDestroy(KsiWorkQueue *wq){
        free(wq->seeds);
        int i = wq->nprocs;
        while(i--){
                ksiRingBufferDestroy(nth(wq,i), wq->nprocs);
        }
        free(wq->rbs);
        ksiLCRQDestroy(&wq->masterQueue);
}
void ksiWorkQueueCommit(KsiWorkQueue *wq,int tid,void *data){
        ksiRingBufferPush(nth(wq, tid), data,tid);
}
static inline int fast_rand(unsigned *seed) {
        *seed = (214013* (*seed)+2531011);
        return (*seed>>16)&0x7FFF;
}
void *ksiWorkQueueGet(KsiWorkQueue *wq,int tid){
        return ksiRingBufferPop(nth(wq, tid),wq->nprocs,tid);
}
void ksiWorkQueueBeginTake(KsiWorkQueue *wq,int tid){
        ksiLCRQConsumerEnter(&wq->masterQueue, tid);
        ksiRingBufferConsumerEnter(nth(wq,tid), tid);
}
void ksiWorkQueueEndTake(KsiWorkQueue *wq,int tid){
        ksiRingBufferConsumerLeave(nth(wq,tid), tid);
        ksiLCRQConsumerLeave(&wq->masterQueue, tid);
}
void *ksiWorkQueueTake(KsiWorkQueue *wq,int tid){
        wq->seeds[tid].val ++;
        int sel = fast_rand(&wq->seeds[tid].val);
        sel = sel%(wq->nprocs+1);
        KsiLCRQ *q = &wq->masterQueue;
        void *ret;
        if(sel == wq->nprocs){
                LCRQ_DEQUEUE_START(q, tid);
                ret = ksiLCRQDequeue(q, tid);
                LCRQ_DEQUEUE_END(q, tid);
        }
        else{
                ret = ksiRingBufferTake(nth(wq, sel),wq->nprocs,tid);
        }
        return ret;
}
void ksiWorkQueueTryFree(KsiWorkQueue *wq,int tid){
        for(int i=0;i<wq->nprocs;i++){
                ksiRingBufferTryFree(nth(wq,i), wq->nprocs, tid);
        }
        ksiDynamicEBRTryFree(wq->masterQueue.ebr, wq->nprocs, tid);
}
