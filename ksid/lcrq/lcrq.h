#ifndef LCRQ_H
#define LCRQ_H

#include "align.h"
#include "../ring_buffer.h"
#include "../ebr.h"

#define EMPTY ((void *) -1)

#ifndef LCRQ_RING_SIZE
#define LCRQ_RING_SIZE (1ull << 12)
#endif

typedef struct RingNode {
        volatile uint64_t val;
        volatile uint64_t idx;
        uint64_t pad[14];
} RingNode DOUBLE_CACHE_ALIGNED;

typedef struct RingQueue {
        struct RingQueue *next DOUBLE_CACHE_ALIGNED;
        volatile int64_t head DOUBLE_CACHE_ALIGNED;
        volatile int64_t tail DOUBLE_CACHE_ALIGNED;
        RingNode array[LCRQ_RING_SIZE];
} RingQueue DOUBLE_CACHE_ALIGNED;

typedef struct {
        RingQueue * volatile head DOUBLE_CACHE_ALIGNED;
        RingQueue * volatile tail DOUBLE_CACHE_ALIGNED;
        KsiDynamicEBR *ebr;
} KsiLCRQ;

typedef struct {
        RingQueue *next;
        int32_t olId;
} KsiLCRQProducerHandle;

void ksiLCRQInit(KsiLCRQ * q, int nprocs);
void ksiLCRQProducerHandleInit(KsiLCRQ * q, KsiLCRQProducerHandle * th);
static inline void ksiLCRQProducerEnter(KsiLCRQ *q, KsiLCRQProducerHandle *th){
        ksiDynamicEBROverlookerEnter(q->ebr, th->olId);
}
static inline void ksiLCRQProducerLeave(KsiLCRQ *q, KsiLCRQProducerHandle *th){
        ksiDynamicEBROverlookerLeave(q->ebr, th->olId);
}
#define LCRQ_ENQUEUE_START(q,th) ksiLCRQProducerEnter(q, th);{//
#define LCRQ_ENQUEUE_END(q,th) }ksiLCRQProducerLeave(q, th);//
void ksiLCRQEnqueue(KsiLCRQ * q, KsiLCRQProducerHandle * th, void * v);

static inline void ksiLCRQConsumerEnter(KsiLCRQ *q, int tid){
        ksiDynamicEBREnter(q->ebr, tid);
}
static inline void ksiLCRQConsumerLeave(KsiLCRQ *q, int tid){
        ksiDynamicEBRLeave(q->ebr, tid);
}
#define LCRQ_DEQUEUE_START(q,tid) ksiLCRQConsumerEnter(q, tid);{//
#define LCRQ_DEQUEUE_END(q,tid) }ksiLCRQConsumerLeave(q, tid);//
void * ksiLCRQDequeue(KsiLCRQ * q, int tid);
void ksiLCRQDestroy(KsiLCRQ * q);
void ksiLCRQProducerHandleDestroy(KsiLCRQ *q, KsiLCRQProducerHandle *h);

#endif /* end of include guard: LCRQ_H */
