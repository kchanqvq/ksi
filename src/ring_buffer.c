#include "ring_buffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "lcrq/queue.h"
#include "queue.h"
//#define memory_order_relaxed memory_order_seq_cst
//#define memory_order_release memory_order_seq_cst
//#define memory_order_acquire memory_order_seq_cst
//#define memory_order_seq_cst memory_order_release
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
/*
  // A simple test routine for development.
  // Kept for convinience.
#include "profile.h"
struct timespec _start;
struct timespec _end;

//#define LCRQ
#define TOTAL
#define TAKES 4
#define MODULO 2
#define N1 (6500*MODULO/TAKES)
#ifdef LCRQ
#define N2 100000
#else
#define N2 N1/MODULO
#endif
_Atomic int cnt;
_Atomic int tcount;
void *worker(void *args){
#ifdef LCRQ
        queue_t *rb = (queue_t *)args;
        handle_t t;
        queue_register(rb, &t, 0);
#else
        KsiWorkQueue *wq = (KsiWorkQueue *)args;
#endif
        int tid;
        tid = atomic_fetch_add(&tcount, 1);
        int64_t result = 0;
        for(int e=0;e<10000;e++){
        if(!(tid%MODULO)){
        for(int64_t i=0;i<N1;i++){
#ifdef LCRQ
                enqueue(rb,&t,(void *)i);
#else
                ksiWorkQueueCommit(wq, tid, (void *)i);
#endif
        }
        }
        for(int i=0;i<N2;i++){
#ifdef LCRQ
                int64_t res = dequeue(rb,&t);
#else

                int64_t res = (int64_t)ksiWorkQueueGet(wq, tid);
#endif
                if(res>=0){
                        result += res;
                        //printf("%lld\n",res);
                        int c = atomic_fetch_add(&cnt, 1);
                        if(c == TAKES*N1/MODULO - 1){
                                clock_gettime(CLOCK_REALTIME, &_end);
                                atomic_store(&cnt, 0);
                                //printf("benchmark : %f ms\n", ((double)(_end.tv_nsec - _start.tv_nsec)/1000000));
                                clock_gettime(CLOCK_REALTIME, &_start);
                        }
                }
        }
        }
        //printf("fin\n");
        return (void *)result;
}
int main(){

#ifdef LCRQ
        queue_t rb;
        queue_init(&rb, TAKES + 1);
#else
        //KsiRingBuffer rb;
        //ksiRingBufferInit(&rb);
        KsiWorkQueue rb;
        ksiWorkQueueInit(&rb, TAKES);
#endif
        pthread_t _pusher;
        pthread_t _taker[TAKES];
        int64_t r[TAKES] = {0};
        //pthread_create(&_pusher, NULL, pusher, &rb);
        //nanosleep(&dt,NULL);
        //pthread_join(_pusher, NULL);
        for(int i=0;i<TAKES;i++)
                pthread_create(&_taker[i], NULL, worker, &rb);
        clock_gettime(CLOCK_REALTIME, &_start);
        //pthread_join(_pusher, NULL);
        for(int i=0;i<TAKES;i++)
                pthread_join(_taker[i], (void **)&r[i]);
        int64_t res = 0;
        for(int i=0;i<TAKES;i++)
                res+=r[i];
        printf("%lld\n", res);
        int cc = atomic_load(&cnt);
        printf("%d\n",cc);
        printf("Mem usage: cur %zu max %zu\n", getCurrentRSS(), getPeakRSS());
        ksiWorkQueueDestroy(&rb);
        return 0;
}
//*/
