#include "lcrq/queue.h"
#include <pthread.h>
#include <stdatomic.h>
static _Atomic int tcounter = 0;
void *worker(void *args){
        queue_t* pq=(queue_t *)args;
        handle_t t;
        int id = atomic_fetch_add(&tcounter, 1);
        queue_register(pq, &t, id);
        printf("enq %d\n",id);
        enqueue(pq, &t, (void *)id);
        printf("%d\n",(int)dequeue(pq, &t));
        return NULL;
}
#define NPROCS 4
int main(int argc,char **argv){
        queue_t q;
        queue_init(&q, NPROCS);
        pthread_t ts[NPROCS];
        for(int i=0;i<NPROCS;i++){
                pthread_create(ts+i, NULL, worker, &q);
        }
        for(int i=0;i<NPROCS;i++){
                pthread_join(ts[i], NULL);
        }
        return 0;
}
