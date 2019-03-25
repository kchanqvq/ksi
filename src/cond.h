#ifndef cond_h
#define cond_h
#include <pthread.h>
typedef struct {
        pthread_cond_t committedCond;
        pthread_mutex_t committedMutex;
        int committedFlag;
} KsiCond;
static inline int ksiCondInit(KsiCond *c){
        int err;
        if((err = pthread_mutex_init(&c->committedMutex, NULL)))
                return err;
        if((err = pthread_cond_init(&c->committedCond, NULL)))
                return err;
        c->committedFlag = 0;
        return 0;
}
static inline void ksiCondSetWait(KsiCond *c){
        pthread_mutex_lock(&c->committedMutex);
        c->committedFlag = 1;
        pthread_mutex_unlock(&c->committedMutex);
}
static inline void ksiCondSignal(KsiCond *c){
        pthread_mutex_lock(&c->committedMutex);
        c->committedFlag = 0;
        pthread_cond_signal(&c->committedCond);
        pthread_mutex_unlock(&c->committedMutex);
}
static inline void ksiCondWait(KsiCond *c){
        pthread_mutex_lock(&c->committedMutex);
        if(c->committedFlag){
                //printf("Really wait\n");
                pthread_cond_wait(&c->committedCond, &c->committedMutex);
                c->committedFlag = 0;
                //printf("Done\n");
        }
        pthread_mutex_unlock(&c->committedMutex);
}
static inline void ksiCondDestroy(KsiCond *c){
        pthread_mutex_lock(&c->committedMutex);
        pthread_mutex_unlock(&c->committedMutex);
        pthread_mutex_destroy(&c->committedMutex);
        pthread_cond_destroy(&c->committedCond);
}
#endif
