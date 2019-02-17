#include "engine.h"
#include <pthread.h>
#include "util.h"
#include "linear_builtins.h"
#include "data.h"
#include "engine.h"
#include "dagedit_commit.h"
#include "dagedit.h"
#define ksiEngineNodesDefaultCapacity 256
#include <unistd.h>
#include <time.h>

#define TRIALS_BEFORE_HANGING 16
#define SLEEP_NSEC_BASE 10000UL
#define SLEEP_NSEC_MAX_FAIL 7
//#define SLEEP_ENABLED
KsiError _ksiEngineReset(KsiEngine *e);

void ksiEngineInit(KsiEngine *e,int32_t framesPerBuffer,int32_t framesPerSecond,int nprocs){
        e->framesPerBuffer = framesPerBuffer;
        e->framesPerSecond = framesPerSecond;
        e->outputBufferPointer[0] = NULL;
        e->outputBufferPointer[1] = NULL;
        e->workers = (pthread_t *)ksiMalloc(sizeof(pthread_t)*nprocs);
        e->nprocs = nprocs;
        e->timeStamp = 0;
        ksiVecInit(&e->nodes[0], ksiEngineNodesDefaultCapacity);
        ksiVecInit(&e->nodes[1], ksiEngineNodesDefaultCapacity);
        atomic_init(&e->epoch, 0);
        atomic_init(&e->audioEpoch, 0);
        ksiVecInit(&e->timeseqResources, ksiEngineNodesDefaultCapacity);

        ksiSPSCCmdListInit(&e->syncCmds);
        ksiSPSCPtrListInit(&e->mallocBufs);
        ksiSPSCPtrListInit(&e->freeBufs);

        ksiWorkQueueInit(&e->tasks, nprocs+1);//NPROCS + MASTER
        ksiSemInit(&e->masterSem, 0, 0);
        ksiSemInit(&e->committingSem, 0, 0);
        ksiSemInit(&e->migratedSem, 0, 0);
        ksiBSemInit(&e->hanging, 0, nprocs);

        PERROR_GUARDED("Init engine playing state pthread mutex",
                       pthread_mutex_init(&e->playingMutex, NULL));
        PERROR_GUARDED("Create DAG modification committing POSIX thread",
                       pthread_create(&e->committer, NULL, ksiMVCCCommitter, e));
        e->playing = 0;
        atomic_flag_test_and_set(&e->notRequireReset);
        //atomic_init(&e->waitingCount,0);
        //PERROR_GUARDED("Init pthread mutex",
        //               pthread_mutex_init(&e->waitingMutex, NULL));
        //PERROR_GUARDED("Init pthread conditional variable",
        //               pthread_cond_init(&e->waitingCond, NULL));
        //atomic_init(&e->cleanupCounter,0);
}
static inline KsiError impl_ksiEngineStop(KsiEngine *e,int state){
        ksiEnginePlayingLock(e);
        if(!(e->playing>0)){
                ksiEnginePlayingUnlock(e);
                return ksiErrorAlreadyStopped;
        }
        if(e->playing==ksiEnginePaused)
                ksiBSemPost(&e->hanging);
        e->playing = state;
        //pthread_mutex_lock(&e->waitingMutex);
        //pthread_cond_broadcast(&e->waitingCond);
        //pthread_mutex_unlock(&e->waitingMutex);
        //ksiSemPost(&e->masterSem);
        for(int i=0;i<e->nprocs;i++){
                PERROR_GUARDED("Stop audio worker POSIX thread",
                               pthread_join(e->workers[i], NULL));
        }
        ksiEnginePlayingUnlock(e);
        return ksiErrorNone;
}
//Call ksiEngineDestroyChild before calling ksiEngineDestroy
void ksiEngineDestroy(KsiEngine *e){
        if(e->playing)
                impl_ksiEngineStop(e,ksiEngineFinalizing);
        else{
                e->playing = ksiEngineFinalizing;
        }
        ksiVecDestroy(&e->nodes[0]);
        ksiVecDestroy(&e->nodes[1]);

        ksiSemPost(&e->committingSem); // Get the committer not waiting
        PERROR_GUARDED("Stop DAG modification committing POSIX thread",
                       pthread_join(e->committer, NULL));

        ksiEnginePlayingLock(e);
        ksiEnginePlayingUnlock(e); // Make sure it has been unlocked when destroying
        PERROR_GUARDED("Destroy engine playing state pthread mutex",
                       pthread_mutex_destroy(&e->playingMutex));

        ksiVecBeginIterate(&e->timeseqResources, i);
        KsiRBTree *n = (KsiRBTree *)i;
        ksiRBTreeDestroy(n);
        free(n);
        ksiVecEndIterate();

        ksiVecDestroy(&e->timeseqResources);
        //while(!atomic_compare_exchange_weak(&e->cleanupCounter, &e->nprocs, 0));

        ksiWorkQueueDestroy(&e->tasks);
        ksiSPSCCmdListDestroy(&e->syncCmds);
        ksiSPSCPtrListDestroy(&e->freeBufs);
        ksiSPSCPtrListDestroy(&e->mallocBufs);

        ksiSemDestroy(&e->masterSem);
        ksiSemDestroy(&e->committingSem);
        ksiSemDestroy(&e->migratedSem);
        ksiBSemDestroy(&e->hanging);
        free(e->workers);
}

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
        if(!(e->playing>0)){
                return 0;
        }
        //while(!atomic_compare_exchange_weak(&e->cleanupCounter, &e->nprocs, 0));
        if(e->playing==ksiEnginePaused){
                memset(output, 0, sizeof(KsiData)*2*frameCount);
                return 0;
        }
        int32_t epoch = atomic_load_explicit(&e->epoch, memory_order_acquire);
        int32_t audioEpoch = atomic_load_explicit(&e->audioEpoch, memory_order_consume);
        if(epoch != audioEpoch){
                atomic_store_explicit(&e->audioEpoch, epoch, memory_order_release);
                ksiSemPost(&e->migratedSem);
        }
        ksiVecBeginIterate(&e->nodes[epoch], i);
        KsiNode *n = (KsiNode *)i;
        *e->outputBufferPointer[epoch] = output;
        if(!n->depNum)
                enqueue_signaled(e,&e->tasks, 0, n);
        ksiVecEndIterate();
        ksiSemWait(&e->masterSem);
        e->timeStamp+=frameCount;
        return 0;
}
static _Atomic int thCounter = 1;
static inline KsiNode *dequeue_blocked(KsiEngine *e,int tid){
        void *result = ksiRingBufferFailedVal;
        int trials = 0;
        int failed = 0;
        result = ksiWorkQueueGet(&e->tasks, tid);
        while(result == ksiRingBufferFailedVal){
                if(e->playing!=ksiEnginePlaying)
                        break;
                result = ksiWorkQueueTake(&e->tasks, tid);
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
                ((KsiNodeFunc)n->extArgs)(n,inputBuffers,outputBuffer);
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
                if(n==ksiRingBufferFailedVal){
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
                                                ksiNodePortIOClear(n->inputTypes[i]);
                                                n->inputCache[i]=me->gain;
                                        }
                                }
                                else{
                                        ksiNodePortIOClear(n->inputTypes[i]);//Unused port
                                }
                        }
                }
                ksiNodeFuncWrapper(n,n->env.internalBufferPtr,n->outputBuffer);
                switch(n->type&ksiNodeTypeOutputMask){
                case ksiNodeTypeOutputNormal:{
                        KsiVecIdlistNode *sn = n->successors;
                        int foundContinuation = 0;
                        while(sn){
                                int32_t epoch = atomic_load_explicit(&e->audioEpoch, memory_order_acquire);
                                KsiNode *s = e->nodes[epoch].data[sn->loc];
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
        int32_t epoch = atomic_load_explicit(&e->audioEpoch, memory_order_acquire);
        ksiVecBeginIterate(&(e->nodes[epoch]), i);
        KsiNode *n = (KsiNode *)i;
        switch(ksiNodeTypeInlineId(n->type)){
#define INLINE_CASE(id,name,reqrs,dm,...)           \
                case id:                            \
                        CONDITIONAL(reqrs,CAT(name,Reset)(n));  \
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
        ksiEngineCommit(e);
        ksiEnginePlayingLock(e);
        if(e->playing>0){
                ksiEnginePlayingUnlock(e);
                return ksiErrorAlreadyPlaying;
        }
        int epoch = atomic_load_explicit(&e->epoch,memory_order_acquire);
        if(!e->outputBufferPointer[epoch]){
                ksiEnginePlayingUnlock(e);
                return ksiErrorNoFinal;
        }
        _ksiEngineReset(e);
        e->playing = 1;
        pthread_attr_t attr;
        struct sched_param param;
        PERROR_GUARDED("Init default POSIX thread attribute for audio worker",
                       pthread_attr_init(&attr));
        PERROR_GUARDED("Get default scheduling param for audio worker",
                       pthread_attr_getschedparam(&attr, &param));
        param.sched_priority = sched_get_priority_max(SCHED_FIFO);
        PERROR_GUARDED("Set FIFO scheduling policy for audio worker",
                       pthread_attr_setschedpolicy(&attr, SCHED_FIFO));
        PERROR_GUARDED("Set scheduling param for audio worker",
                       pthread_attr_setschedparam(&attr, &param));
        for(int i=0;i<e->nprocs;i++){
                PERROR_GUARDED("Create audio worker POSIX thread",
                               pthread_create(e->workers+i, &attr, ksiEngineAudioWorker, e));
        }
        PERROR_GUARDED("Destroy POSIX thread attribute for audio worker",
                       pthread_attr_destroy(&attr));
        ksiEnginePlayingUnlock(e);
        return ksiErrorNone;
}

KsiError ksiEngineStop(KsiEngine *e){
        atomic_store(&thCounter, 1);
        return impl_ksiEngineStop(e, 0);
}
KsiError ksiEnginePause(KsiEngine *e){
        ksiEnginePlayingLock(e);
        if(!(e->playing>0)){
                ksiEnginePlayingUnlock(e);
                return ksiErrorAlreadyStopped;
        }
        e->playing = ksiEnginePaused;
        ksiEnginePlayingUnlock(e);
        return ksiErrorNone;
}
KsiError ksiEngineResume(KsiEngine *e){
        ksiEnginePlayingLock(e);
        if(!(e->playing>0)){
                ksiEnginePlayingUnlock(e);
                return ksiErrorAlreadyStopped;
        }
        if(e->playing==1){
                ksiEnginePlayingUnlock(e);
                return ksiErrorAlreadyPlaying;
        }
        e->playing = ksiEnginePlaying;
        ksiBSemPost(&e->hanging);
        //ksiSemPost(&e->masterSem);
        ksiEnginePlayingUnlock(e);
        return ksiErrorNone;
}
