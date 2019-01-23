#include "ring_buffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "queue.h"
#define FREE
#include <unistd.h>

static inline void memalign(void **obj,size_t s){
        int ret = posix_memalign(obj, 128, s);
        if(ret){
                fprintf(stderr,"FATAL ERROR: Aligned memory allocation failed: %d\n",ret);
                abort();
        }
}

KsiRingBufferSegment *ksiRingBufferSegmentNew(void *data){
        KsiRingBufferSegment *obj;
        memalign((void **)&obj, sizeof(KsiRingBufferSegment));
        atomic_init(&obj->head_idx,1);
        atomic_init(&obj->tail_idx,0);
        obj->buffer[0].val = data;
        obj->next = NULL;
        return obj;
}
void ksiRingBufferInit(KsiRingBuffer *rb,int nprocs){
        KsiRingBufferSegment *obj;
        memalign((void **)&obj, sizeof(KsiRingBufferSegment));
        atomic_init(&obj->head_idx,0);
        atomic_init(&obj->tail_idx,0);
        obj->next = NULL;
        obj->prev = NULL;
        rb->head = obj;
        atomic_init(&rb->tail,obj);
        atomic_init(&rb->epoch,0);
        //printf("init RB %lld\n",rb);
        while(nprocs --){
                atomic_init(&rb->ebrEntries[nprocs].active, 0);
                rb->ebrEntries[nprocs].epoch = 0;
                rb->ebrEntries[nprocs].years = 0;
                int i = 3;
                while(i --)
                        rb->ebrEntries[nprocs].freelist[i] = NULL;
        }
}
//#define CNT
static inline
#ifdef CNT
int
#else
void
#endif
destroyFromTail(KsiRingBufferSegment *tail){
        KsiRingBufferSegment *cur = tail;
#ifdef CNT
        int cnt = 0;
#endif
        while(cur){
                KsiRingBufferSegment *tmp = cur;
                cur = cur->prev;
                free(tmp);
#ifdef CNT
                cnt ++;
#endif
        }
#ifdef CNT
        return cnt;
#endif
}
void ksiRingBufferDestroy(KsiRingBuffer *rb,int nprocs){
        int i = nprocs;
#ifdef CNT
        int cnt = 0;
#endif
        while(i--){
                int epoch = 3;
                while(epoch--){
#ifdef CNT
                        int ccnt =
#endif
                        destroyFromTail(rb->ebrEntries[i].freelist[epoch]);
#ifdef CNT
                        cnt += ccnt;
                        printf("%d objects destroyed for epoch %d tid %d\n", ccnt, epoch,i);
#endif
                }
        }
#ifdef CNT
        int acnt =
#endif
        destroyFromTail(atomic_load_explicit(&rb->tail, memory_order_relaxed));
#ifdef CNT
        cnt += acnt;
        printf("%d objects freed during finalization. %d live objects.\n", cnt, acnt);
#endif
}
static inline void tryFree(KsiRingBufferEBREntry *ee,KsiRingBuffer *rb,int nprocs,int tid){
#ifdef FREE
        if(ee->years > ksiRingBufferEBRThreshold){
                ee->years = 0;
                uint64_t epoch = atomic_load_explicit(&rb->epoch,memory_order_acquire);
                int i = nprocs;
                int canFree = 1;
                while(i--){
                        if(atomic_load_explicit(&rb->ebrEntries[i].active,memory_order_acquire) && (rb->ebrEntries[i].epoch != epoch))
                                canFree = 0;
                }
                if(!canFree)
                        return;
                uint64_t dummy = epoch;
                if(atomic_compare_exchange_strong_explicit(&rb->epoch,&dummy,(epoch+1)%3,memory_order_release,memory_order_relaxed)){
                        int i = nprocs;
#ifdef CNT
                        int cnt = 0;
#endif
                        while(i--){
#ifdef CNT
                                cnt +=
#endif
                                destroyFromTail(rb->ebrEntries[i].freelist[(epoch+2)%3]);
                                rb->ebrEntries[i].freelist[(epoch+2)%3]=NULL;
                        }
#ifdef CNT
                        printf("%d objects freed during GC\n", cnt);
#endif
                }
                rb->ebrEntries[tid].epoch = (epoch+1)%3;
        }
#endif
}
void ksiRingBufferPush(KsiRingBuffer *rb,void *data,int tid){
        KsiRingBufferSegment *head = rb->head;
#ifdef FREE
        atomic_store_explicit(&rb->ebrEntries[tid].active, 1, memory_order_relaxed);
        rb->ebrEntries[tid].years ++;
        rb->ebrEntries[tid].epoch = atomic_load_explicit(&rb->epoch,memory_order_relaxed);
#endif
        uint64_t _head_idx = atomic_load_explicit(&head->head_idx,memory_order_relaxed);
        uint64_t _tail_idx = atomic_load_explicit(&head->tail_idx,memory_order_relaxed);
        if(_tail_idx + ksiRingBufferNodeLength <= _head_idx){
                //printf("hit\n");
                KsiRingBufferSegment *n = ksiRingBufferSegmentNew(data);
                n->prev = rb->head;
                rb->head = n;
                atomic_store_explicit(&head->next,n,memory_order_release);
        }
        else{
                head->buffer[_head_idx%ksiRingBufferNodeLength].val = data;
                atomic_store_explicit(&head->head_idx,_head_idx+1,memory_order_release);
        }
#ifdef FREE
        atomic_store_explicit(&rb->ebrEntries[tid].active, 0, memory_order_release);
#endif
}
void *ksiRingBufferTake(KsiRingBuffer *rb,int nprocs,int tid){
#ifdef FREE
        atomic_store_explicit(&rb->ebrEntries[tid].active, 1, memory_order_relaxed);
        rb->ebrEntries[tid].years ++;
        rb->ebrEntries[tid].epoch = atomic_load_explicit(&rb->epoch,memory_order_acquire);
#endif
        KsiRingBufferSegment *tail = atomic_load_explicit(&rb->tail,memory_order_acquire);
        KsiRingBufferSegment *prevTail;
        while(tail){
                uint64_t _tail_idx = atomic_load_explicit(&tail->tail_idx,memory_order_relaxed);
                uint64_t _head_idx = atomic_load_explicit(&tail->head_idx,memory_order_acquire);
                if(_tail_idx+1 < _head_idx+1){
                        void *ret = tail->buffer[_tail_idx%ksiRingBufferNodeLength].val;
                        if(atomic_compare_exchange_strong_explicit(&tail->tail_idx, &_tail_idx, _tail_idx+1,memory_order_release,memory_order_relaxed)){
#ifdef FREE
                                atomic_store_explicit(&rb->ebrEntries[tid].active, 0, memory_order_release);
#endif

                                return ret;
                        }
                        else
                                continue;
                }
                else{
                        prevTail = tail;
                        tail = atomic_load_explicit(&tail->next,memory_order_acquire);
                        if(tail){
                                KsiRingBufferSegment *dummy = prevTail;
                                atomic_store_explicit(&prevTail->head_idx,-1,memory_order_relaxed);
                                KsiRingBufferEBREntry *ee = rb->ebrEntries + tid;
                                if(atomic_compare_exchange_strong_explicit(&rb->tail,&dummy,tail,memory_order_release,memory_order_relaxed)){
                                        tail->prev = NULL;
#ifdef FREE
                                        prevTail->prev = ee->freelist[ee->epoch];
                                        ee->freelist[ee->epoch] = prevTail;
#endif
                                }
                                tryFree(ee, rb, nprocs, tid);
                        }
                }
        }
#ifdef FREE
        atomic_store_explicit(&rb->ebrEntries[tid].active, 0, memory_order_release);
#endif
        return ksiRingBufferFailedVal;
}
void *ksiRingBufferPop(KsiRingBuffer *rb,int nprocs,int tid){
#ifdef FREE
        atomic_store_explicit(&rb->ebrEntries[tid].active, 1, memory_order_relaxed);
        rb->ebrEntries[tid].years ++;
        rb->ebrEntries[tid].epoch = atomic_load_explicit(&rb->epoch,memory_order_acquire);
#endif
        KsiRingBufferSegment *head = rb->head;
        while(1){

                uint64_t _head_idx = atomic_load_explicit(&head->head_idx,memory_order_relaxed);
                _head_idx --;

                uint64_t _tail_idx = atomic_load_explicit(&head->tail_idx,memory_order_relaxed);
                if(_tail_idx + 1 < _head_idx + 1){
                        atomic_store_explicit(&head->head_idx,_head_idx,memory_order_relaxed);
                        void *ret = head->buffer[_head_idx%ksiRingBufferNodeLength].val;
#ifdef FREE
                        atomic_store_explicit(&rb->ebrEntries[tid].active, 0, memory_order_release);
#endif
                        return ret;
                }
                else if(_tail_idx == _head_idx){
                        _head_idx ++;
                        void *ret = head->buffer[_tail_idx%ksiRingBufferNodeLength].val;
                        if(atomic_compare_exchange_strong_explicit(&head->tail_idx, &_tail_idx, _tail_idx+1,memory_order_release,memory_order_relaxed)){
#ifdef FREE
                                atomic_store_explicit(&rb->ebrEntries[tid].active, 0, memory_order_release);
#endif

                                return ret;
                        }
                        else
                                continue;
                }
                else{
                        atomic_store_explicit(&head->head_idx,_tail_idx,memory_order_relaxed);
                        if(head->prev){
                                atomic_store_explicit(&head->prev->next,NULL,memory_order_relaxed);
                                KsiRingBufferSegment *tmp = head;
                                head = head->prev;
                                rb->head = head;
#ifdef FREE
                                KsiRingBufferEBREntry *ee = rb->ebrEntries + tid;
                                tmp->prev = ee->freelist[ee->epoch];
                                ee->freelist[ee->epoch] = tmp;
                                tryFree(ee, rb, nprocs, tid);
#endif
                                continue;
                        }
#ifdef FREE
                        atomic_store_explicit(&rb->ebrEntries[tid].active, 0, memory_order_release);
#endif
                        return ksiRingBufferFailedVal;
                }
        }
}
