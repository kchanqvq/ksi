#include "ring_buffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "queue.h"
#include <unistd.h>
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
        //pthread_t _pusher;
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
