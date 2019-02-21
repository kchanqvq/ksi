#include "queue.h"
#include <stdlib.h>
static inline KsiRingBuffer *nth(KsiWorkQueue *wq,int tid){
        int nprocs = wq->nprocs;
        return (KsiRingBuffer *)(((char *)wq->rbs) + (sizeof(KsiRingBuffer) + sizeof(KsiRingBufferEBREntry)*nprocs)*tid);
}
void ksiWorkQueueInit(KsiWorkQueue *wq,int nprocs){
        wq->nprocs = nprocs;
        wq->rbs = (KsiRingBuffer *)malloc((sizeof(KsiRingBuffer)+sizeof(KsiRingBufferEBREntry)*nprocs)*nprocs);
        wq->seeds = (KsiPackedUnsigned *)malloc(sizeof(KsiPackedUnsigned)*nprocs);
        int i = nprocs;
        while(i--)
                ksiRingBufferInit(nth(wq, i),nprocs);
}
void ksiWorkQueueDestroy(KsiWorkQueue *wq){
        free(wq->seeds);
        int i = wq->nprocs;
        while(i--)
                ksiRingBufferDestroy(nth(wq,i), wq->nprocs);
        free(wq->rbs);
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
void *ksiWorkQueueTake(KsiWorkQueue *wq,int tid){
        wq->seeds[tid].val ++;
        int sel = fast_rand(&wq->seeds[tid].val);
        sel = sel%wq->nprocs;
        return ksiRingBufferTake(nth(wq, sel),wq->nprocs,tid);
}
void ksiWorkQueueTryFree(KsiWorkQueue *wq,int tid){
        for(int i=0;i<wq->nprocs;i++){
                ksiRingBufferTryFree(nth(wq,i), wq->nprocs, tid);
        }
}
