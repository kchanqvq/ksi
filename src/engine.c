#include "engine.h"
#include <pthread.h>
#include "util.h"
#include "linear_builtins.h"
#include "data.h"
#include <unistd.h>
#include <time.h>
#define QUEUE_EMPTY ((void *)-1ll)
#define TRIALS_BEFORE_HANGING 16
#define SLEEP_NSEC_BASE 10000UL
#define SLEEP_NSEC_MAX_FAIL 7
//#define SLEEP_ENABLED
KsiError _ksiEngineReset(KsiEngine *e);
static inline void enqueue_signaled(KsiEngine *e,KsiWorkQueue *q,int tid,void *v){
        ksiWorkQueueCommit(q, tid, v);
        //printf("enqueued\n", c);
#ifdef SLEEP_ENABLED
        if(atomic_load(&e->waitingCount)){
                pthread_mutex_lock(&e->waitingMutex);
                //printf("post\n");
                pthread_cond_signal(&e->waitingCond);
                pthread_mutex_unlock(&e->waitingMutex);
        }
#endif
}
int ksiEngineAudioCallback( const void *input,
                                   void *output,
                                   unsigned long frameCount,
                                   const PaStreamCallbackTimeInfo* timeInfo,
                                   PaStreamCallbackFlags statusFlags,
                                   void *userData ){
        KsiEngine *e = (KsiEngine *)userData;
        if(!atomic_flag_test_and_set(&e->notRequireReset))
                _ksiEngineReset(e);
        if(e->playing==ksiEngineStopped){
                return 0;
        }
        pthread_mutex_lock(&e->editMutex);
        //while(!atomic_compare_exchange_weak(&e->cleanupCounter, &e->nprocs, 0));
        if(e->playing==ksiEnginePaused){
                memset(output, 0, sizeof(KsiData)*2*frameCount);
                pthread_mutex_unlock(&e->editMutex);
                return 0;
        }
        ksiVecBeginIterate(&e->nodes, i);
        KsiNode *n = (KsiNode *)i;
        *e->outputBufferPointer = output;
        if(!n->depNum)
                enqueue_signaled(e,&e->tasks, 0, n);
        ksiVecEndIterate();
        ksiSemWait(&e->masterSem);
        e->timeStamp+=frameCount;
        pthread_mutex_unlock(&e->editMutex);
        return 0;
}
static _Atomic int thCounter = 1;
static inline KsiNode *dequeue_blocked(KsiEngine *e,int tid){
        void *result = QUEUE_EMPTY;
        int trials = 0;
        int failed = 0;
        while(result == QUEUE_EMPTY){
                if(e->playing!=ksiEnginePlaying)
                        break;
                result = ksiWorkQueueGet(&e->tasks, tid);
#ifdef SLEEP_ENABLED
                trials ++;
                if(trials > TRIALS_BEFORE_HANGING){
                        atomic_fetch_add(&e->waitingCount, 1);
                        //printf("waiting\n");
                        struct timeval now;
                        struct timespec wt;
                        gettimeofday(&now,NULL);
                        wt.tv_sec = now.tv_sec;
                        wt.tv_nsec = now.tv_usec*1000UL + (SLEEP_NSEC_BASE << failed);
                        pthread_mutex_lock(&e->waitingMutex);
                        //pthread_cond_wait(&e->waitingCond, &e->waitingMutex);
                        if(pthread_cond_timedwait(&e->waitingCond, &e->waitingMutex, &wt)){
                                pthread_mutex_unlock(&e->waitingMutex);
                                if(failed < SLEEP_NSEC_MAX_FAIL){
                                        failed ++;
                                }
                                //printf("%d\n",failed);
                        }
                        else {
                                pthread_mutex_unlock(&e->waitingMutex);
                                failed = 0;
                        }
                        atomic_fetch_sub(&e->waitingCount, 1);
                        //printf("wake up %d\n",atomic_load(&e->waitingCount));
                        //nanosleep(&sleepTime, NULL);
                        trials = 0;
                }
#endif
        }
        //printf("dequeued\n");
        return (KsiNode *)result;
}
#include "inline_meta.h"
static inline void ksiNodeFuncWrapper(KsiNode *n,KsiData **inputBuffers,KsiData *outputBuffer){
        switch(ksiNodeTypeInlineId(n->type)){
        case 0:
                n->f(n,inputBuffers,outputBuffer);
                break;
#define INLINE_CASE(id,name,reqrs,dm,...)                          \
                case id:                                        \
                        name(n,inputBuffers,outputBuffer); \
                        break;
                INLINE_PROPERTY(INLINE_CASE);
#undef INLINE_CASE
        }
}
static void* ksiEngineAudioWorker(void *args){
        KsiEngine *e = (KsiEngine *)args;
        int tid = atomic_fetch_add(&thCounter, 1);
        KsiNode *n = dequeue_blocked(e, tid);
        int32_t bufsize = e->framesPerBuffer;
        while(1){
                if(n==QUEUE_EMPTY){
                        if(e->playing == ksiEnginePaused){
                                ksiBSemWait(&e->hanging);
                                n = dequeue_blocked(e, tid);
                                continue;
                        }
                        else
                                break;
                }
                //break;
                for(int32_t i=0;i<n->inputCount;i++){
                        n->inputCache[i].i = 0;
                        int dirty = 0;
                        int8_t type = n->inputTypes[i];
                        KsiMixerEnvEntry *me = n->env.portEnv[i].mixer;
                        KsiData *buf = n->env.portEnv[i].buffer;
                        if(buf){
                                while(me){
                                        KsiNode *prec = me->src;
                                        if(!prec){ //src == NULL for bias
                                                ksiDataIncrease(n->inputCache+i, me->gain, type);
                                        }
                                        else{
                                                if(prec->outputTypes[me->srcPort]&ksiNodePortIODirty){
                                                        dirty=ksiNodePortIODirty;
                                                }
                                                else{
                                                        ksiDataWeightedIncrease(n->inputCache+i, prec->outputCache[me->srcPort], me->gain, type);
                                                }
                                        }
                                        me = me->next;
                                }
                                if(dirty){
                                        me = n->env.portEnv[i].mixer;
                                        for(int32_t j=0;j<bufsize;j++){
                                                buf[j] = n->inputCache[i];
                                        }
                                        while(me){
                                                KsiNode *prec = me->src;
                                                if(prec&&(prec->outputTypes[me->srcPort]&ksiNodePortIODirty)){
                                                        ksiDataBufferWeightedIncrease(buf, prec->outputBuffer+bufsize*me->srcPort, me->gain, bufsize, type);
                                                }
                                                me = me->next;
                                        }
                                }
                                n->inputTypes[i]=(n->inputTypes[i]&~ksiNodePortIOMask)|dirty;
                        }
                        else{
                                if(me){
                                        if(me->src){
                                                if(me->src->outputTypes[me->srcPort]&ksiNodePortIODirty){
                                                        ksiNodePortIOSetDirty(n->inputTypes[i]);
                                                }
                                                else{
                                                        ksiNodePortIOClear(n->inputTypes[i]);
                                                        n->inputCache[i]=me->src->outputCache[me->srcPort];
                                                }
                                        }
                                        else{//bias term
                                                n->inputCache[i]=me->gain;
                                        }
                                }
                        }
                }
                ksiNodeFuncWrapper(n,n->env.internalBufferPtr,n->outputBuffer);
                switch(n->type&ksiNodeTypeOutputMask){
                case ksiNodeTypeOutputNormal:{
                        KsiVecIdlistNode *sn = n->successors;
                        int foundContinuation = 0;
                        while(sn){
                                KsiNode *s = e->nodes.data[sn->loc];
                                int dep = atomic_fetch_add(&s->depCounter, 1);
                                if(dep == s->depNum - 1){
                                        atomic_store(&s->depCounter, 0);
                                        if(foundContinuation){
                                                enqueue_signaled(e,&e->tasks, tid, s);
                                        }
                                        else{
                                                foundContinuation = 1;
                                                n = s;
                                        }
                                }
                                sn = sn->next;
                        }
                        if(!foundContinuation)
                                n = dequeue_blocked(e, tid);
                        break;
                }
                case ksiNodeTypeOutputFinal:
                        ksiSemPost(&e->masterSem);
                        n = dequeue_blocked(e, tid);
                        break;
                }
        }
        //atomic_fetch_add(&e->cleanupCounter, 1);
        return NULL;
}
KsiError _ksiEngineReset(KsiEngine *e){
        ksiVecBeginIterate(&(e->nodes), i);
        KsiNode *n = (KsiNode *)i;
        switch(ksiNodeTypeInlineId(n->type)){
#define INLINE_CASE(id,name,reqrs,dm,...)           \
                case id:                            \
                        if(reqrs){                  \
                                CAT(name,Reset)(n); \
                        }                           \
                        break;
                INLINE_PROPERTY(INLINE_CASE);
        }
        ksiVecEndIterate();
        return ksiErrorNone;
}
KsiError ksiEngineReset(KsiEngine *e){
        atomic_flag_clear(&e->notRequireReset);
        return ksiErrorNone;
}
KsiError ksiEngineLaunch(KsiEngine *e){
        if(e->playing)
                return ksiErrorAlreadyPlaying;
        if(!e->outputBufferPointer)
                return ksiErrorNoFinal;
        _ksiEngineReset(e);
        e->playing = 1;
        for(int i=0;i<e->nprocs;i++){
                pthread_create(e->workers+i, NULL, ksiEngineAudioWorker, e);
        }
        return ksiErrorNone;
}
KsiError ksiEngineStop(KsiEngine *e){
        pthread_mutex_lock(&e->editMutex);
        if(!e->playing){
                pthread_mutex_unlock(&e->editMutex);
                return ksiErrorAlreadyStopped;
        }
        if(e->playing==ksiEnginePaused)
                ksiBSemPost(&e->hanging);
        e->playing = 0;
        pthread_mutex_lock(&e->waitingMutex);
        pthread_cond_broadcast(&e->waitingCond);
        pthread_mutex_unlock(&e->waitingMutex);
        pthread_mutex_unlock(&e->editMutex);
        //ksiSemPost(&e->masterSem);
        for(int i=0;i<e->nprocs;i++){
                pthread_join(e->workers[i], NULL);
        }
        return ksiErrorNone;
}
KsiError ksiEnginePause(KsiEngine *e){
        pthread_mutex_lock(&e->editMutex);
        if(e->playing==ksiEngineStopped){
                pthread_mutex_unlock(&e->editMutex);
                return ksiErrorAlreadyStopped;
        }
        e->playing = ksiEnginePaused;
        pthread_mutex_unlock(&e->editMutex);
        return ksiErrorNone;
}
KsiError ksiEngineResume(KsiEngine *e){
        pthread_mutex_lock(&e->editMutex);
        if(e->playing==0){
                pthread_mutex_unlock(&e->editMutex);
                return ksiErrorAlreadyStopped;
        }
        if(e->playing==1){
                pthread_mutex_unlock(&e->editMutex);
                return ksiErrorAlreadyPlaying;
        }
        e->playing = ksiEnginePlaying;
        pthread_mutex_unlock(&e->editMutex);
        ksiBSemPost(&e->hanging);
        //ksiSemPost(&e->masterSem);
        return ksiErrorNone;
}
