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
#define SLEEP_ENABLED
KsiError _ksiEngineReset(KsiEngine *e);
#include "lcrq/lcrq.h"
#define ksiEngineWorkerPoolStateRunning 1
#define ksiEngineWorkerPoolStateFinalizing 0
static void* ksiEngineAudioWorker(void *args);
#define GC_SEC 0
#define GC_NSEC 10000000
static void* ksiEngineWorkerPoolGC(void *args){
        KsiEngineWorkerPool *wp = args;
        while(1){
                if(!wp->state)
                        break;
                ksiSemTryWait(&wp->gcSem, GC_SEC, GC_NSEC);
                ksiWorkQueueTryFree(&wp->tasks, wp->nprocs);
        }
        return NULL;
}
void ksiEngineWorkerPoolInit(KsiEngineWorkerPool *wp, int nprocs){
        wp->nprocs = nprocs;
        wp->state = ksiEngineWorkerPoolStateRunning;
        wp->workers = (pthread_t *)ksiMalloc(sizeof(pthread_t)*nprocs);
        ksiWorkQueueInit(&wp->tasks, nprocs+1);//MASTER, GC
        ksiBSemInit(&wp->hanging, 0, nprocs);
        atomic_init(&wp->waitingCount,0);
        ksiSemInit(&wp->waitingSem, 0, 0);
        ksiSemInit(&wp->gcSem, 0, 0);
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
        PERROR_GUARDED("Create GC POSIX thread",
                       pthread_create(&wp->gc_thread, NULL, ksiEngineWorkerPoolGC, wp));
        for(int i=0;i<wp->nprocs;i++){
                PERROR_GUARDED("Create audio worker POSIX thread",
                               pthread_create(wp->workers+i, &attr, ksiEngineAudioWorker, wp));
        }
        PERROR_GUARDED("Destroy POSIX thread attribute for audio worker",
                       pthread_attr_destroy(&attr));
}
void ksiEngineWorkerPoolDestroy(KsiEngineWorkerPool *wp){
        wp->state = ksiEngineWorkerPoolStateFinalizing;
        if(!atomic_load_explicit(&wp->enginesAlive,memory_order_acquire))
                ksiBSemPost(&wp->hanging);
        int64_t i=atomic_load(&wp->waitingCount);
        while(i--)
                ksiSemPost(&wp->waitingSem);
        //ksiSemPost(&e->masterSem);
        for(int i=0;i<wp->nprocs;i++){
                PERROR_GUARDED("Stop audio worker POSIX thread",
                               pthread_join(wp->workers[i], NULL));
        }
        PERROR_GUARDED("Stop GC POSIX thread",
                       pthread_join(wp->gc_thread, NULL));
        ksiSemDestroy(&wp->gcSem);
        ksiSemDestroy(&wp->waitingSem);
        ksiWorkQueueDestroy(&wp->tasks);
        ksiBSemDestroy(&wp->hanging);
        free(wp->workers);
}
static inline void ksiEngineWorkerPoolOnlineIncrease(KsiEngineWorkerPool *wp){
        if(!atomic_load_explicit(&wp->enginesAlive, memory_order_seq_cst))
                ksiBSemPost(&wp->hanging);
        atomic_fetch_add_explicit(&wp->enginesAlive, 1, memory_order_seq_cst);
}
static inline void ksiEngineWorkerPoolOnlineDecrease(KsiEngineWorkerPool *wp){
        atomic_fetch_sub_explicit(&wp->enginesAlive, 1, memory_order_seq_cst);
}
void ksiEngineInit(KsiEngine *e,KsiEngineWorkerPool *wp,int32_t fb,int32_t fs){
        e->wp = wp;
        e->framesPerBuffer = fb;
        e->framesPerSecond = fs;
        e->driver_env = NULL;
        e->finalNode[0] = NULL;
        e->finalNode[1] = NULL;
        e->timeStamp = 0;
        ksiLCRQProducerHandleInit(&wp->tasks.masterQueue, &e->ph);
        ksiVecInit(&e->nodes[0], ksiEngineNodesDefaultCapacity);
        ksiVecInit(&e->nodes[1], ksiEngineNodesDefaultCapacity);
        atomic_init(&e->epoch, 0);
        atomic_init(&e->audioEpoch, 0);
        ksiVecInit(&e->timeseqResources, ksiEngineNodesDefaultCapacity);

        ksiSPSCCmdListInit(&e->syncCmds);
        ksiSPSCPtrListInit(&e->mallocBufs);
        ksiSPSCPtrListInit(&e->freeBufs);

        ksiSemInit(&e->masterSem, 0, 0);
        ksiSemInit(&e->committingSem, 0, 0);
        ksiSemInit(&e->migratedSem, 0, 0);
        ksiCondInit(&e->committedCond);

        pthread_mutexattr_t attr;
        PERROR_GUARDED("Init engine playing state pthread mutex attribute",
                       pthread_mutexattr_init(&attr));
        PERROR_GUARDED("Set engine playing state pthread mutex attribute to be recursive",
                       pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE));
        PERROR_GUARDED("Init engine playing state pthread mutex",
                       pthread_mutex_init(&e->playingMutex, &attr));
        PERROR_GUARDED("Create DAG modification committing POSIX thread",
                       pthread_create(&e->committer, NULL, ksiMVCCCommitter, e));
        e->playing = 0;
        atomic_flag_test_and_set(&e->notRequireReset);

        //atomic_init(&e->cleanupCounter,0);
}
//Call ksiEngineDestroyChild before calling ksiEngineDestroy
int impl_ksiEngineStop(KsiEngine *e,int state){
        ksiEnginePlayingLock(e);
        e->playing = state;
        ksiEngineWorkerPoolOnlineDecrease(e->wp);
        ksiEnginePlayingUnlock(e);
        return ksiErrorNone;
}
void ksiEngineDestroy(KsiEngine *e){
        if(e->playing)
                impl_ksiEngineStop(e,ksiEngineFinalizing);
        else{
                e->playing = ksiEngineFinalizing;
        }
        ksiSemPost(&e->committingSem); // Get the committer not waiting
        PERROR_GUARDED("Stop DAG modification committing POSIX thread",
                       pthread_join(e->committer, NULL));
        ksiVecDestroy(&e->nodes[0]);
        ksiVecDestroy(&e->nodes[1]);
        ksiLCRQProducerHandleDestroy(&e->wp->tasks.masterQueue, &e->ph);


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


        ksiSPSCCmdListDestroy(&e->syncCmds);
        ksiSPSCPtrListDestroy(&e->freeBufs);
        ksiSPSCPtrListDestroy(&e->mallocBufs);

        ksiSemDestroy(&e->masterSem);
        ksiSemDestroy(&e->committingSem);
        ksiSemDestroy(&e->migratedSem);
        ksiCondDestroy(&e->committedCond);

}

static inline void enqueue_signaled(KsiEngineWorkerPool *wp,KsiWorkQueue *q,int tid,void *v){
        ksiWorkQueueCommit(q, tid, v);
        //printf("enqueued\n", c);
#ifdef SLEEP_ENABLED
        int64_t i = atomic_load_explicit(&wp->waitingCount,memory_order_relaxed);
        if(i>0){
                //printf("post %d\n",i);
                atomic_fetch_sub_explicit(&wp->waitingCount, 1,memory_order_relaxed);
                ksiSemPost(&wp->waitingSem);
        }
#endif
}
int ksiEngineAudioCallback( const void *input,
                            void *output,
                            unsigned long frameCount,
                            void *userData ){
        KsiEngine *e = (KsiEngine *)userData;
        e->framesPerBuffer = frameCount;
        if(!atomic_flag_test_and_set(&e->notRequireReset))
                _ksiEngineReset(e);
        if(!(e->playing>0)){
                return 0;
        }
        //while(!atomic_compare_exchange_weak(&e->cleanupCounter, &e->nprocs, 0));
        int32_t epoch = atomic_load_explicit(&e->epoch, memory_order_acquire);
        int32_t audioEpoch = atomic_load_explicit(&e->audioEpoch, memory_order_consume);
        if(epoch != audioEpoch){
                atomic_store_explicit(&e->audioEpoch, epoch, memory_order_release);
                ksiSemPost(&e->migratedSem);
        }
        if(e->playing==ksiEnginePaused){
                for(int i=0;i<2;i++){
                        memset(((KsiData **)output)[i], 0, sizeof(KsiData)*frameCount);
                }
                return 0;
        }
        if(!e->finalNode[epoch]){
                for(int i=0;i<2;i++){
                        memset(((KsiData **)output)[i], 0, sizeof(KsiData)*frameCount);
                }
                return 0;
        }
        KsiNode *finalNode = e->finalNode[epoch];
        int finalNodeChans = finalNode->outputCount;
        int audioChans = 2;
        int fillChans = finalNodeChans<audioChans?finalNodeChans:audioChans;
        for(int i=0;i<fillChans;i++)
                 finalNode->outputBuffer[i].d = ((KsiData **)output)[i];
        ksiVecBeginIterate(&e->nodes[epoch], i);
        KsiNode *n = (KsiNode *)i;
        ksiLCRQProducerEnter(&e->wp->tasks.masterQueue, &e->ph);
        if(!n->depNum){
                ksiLCRQEnqueue(&e->wp->tasks.masterQueue, &e->ph, n);
        }
        ksiLCRQProducerLeave(&e->wp->tasks.masterQueue, &e->ph);
        ksiVecEndIterate();
        ksiSemWait(&e->masterSem);
        for(int i=fillChans+1;i<audioChans;i++)
                memset(((KsiData **)output)[i], 0, sizeof(KsiData)*frameCount);
        e->timeStamp+=frameCount;
        return 0;
}
static _Atomic int thCounter = 0;
static inline KsiNode *dequeue_blocked(KsiEngineWorkerPool *wp,int tid){
        void *result = ksiRingBufferFailedVal;
        int trials = 0;
        int failed = 0;
        result = ksiWorkQueueGet(&wp->tasks, tid);
        if(result == ksiRingBufferFailedVal){
                LCRQ_DEQUEUE_START(&wp->tasks.masterQueue, tid);
                result = ksiLCRQDequeue(&wp->tasks.masterQueue,
                                        tid);
                LCRQ_DEQUEUE_END(&wp->tasks.masterQueue, tid);
        }
        ksiWorkQueueBeginTake(&wp->tasks, tid);
        while(result == ksiRingBufferFailedVal){
                if(!atomic_load_explicit(&wp->enginesAlive,memory_order_seq_cst))
                        break;
                result = ksiWorkQueueTake(&wp->tasks, tid);
#ifdef SLEEP_ENABLED
                trials ++;
                if(trials > TRIALS_BEFORE_HANGING){
                        //printf("waiting\n");
                        atomic_fetch_add_explicit(&wp->waitingCount,1,memory_order_relaxed);
                        ksiWorkQueueEndTake(&wp->tasks, tid);
                        if(ksiSemTryWait(&wp->waitingSem,0,SLEEP_NSEC_BASE<<failed)){
                                if(failed < SLEEP_NSEC_MAX_FAIL){
                                        failed ++;
                                }
                        }
                        else{
                                failed=0;
                        }
                        trials = 0;
                        ksiWorkQueueBeginTake(&wp->tasks, tid);
                }
#endif
        }
        ksiWorkQueueEndTake(&wp->tasks, tid);
        //printf("dequeued\n");
        return (KsiNode *)result;
}
#include "inline_meta.h"
static inline void ksiNodeFuncWrapper(KsiNode *n){
        switch(ksiNodeTypeInlineId(n->type)){
        case 0:
                ((KsiNodeFunc)n->extArgs)(n);
                break;
#define INLINE_CASE(id,name,reqrs,dm,...)                          \
                case id:                                        \
                        name(n);                                \
                        break;
                INLINE_PROPERTY(INLINE_CASE);
#undef INLINE_CASE
        }
}
static void* ksiEngineAudioWorker(void *args){
        KsiEngineWorkerPool *wp = args;
        int tid = atomic_fetch_add(&thCounter, 1);
        KsiNode *n = dequeue_blocked(wp, tid);
        while(1){
                if(n==ksiRingBufferFailedVal){
                        if(!atomic_load_explicit(&wp->enginesAlive,memory_order_seq_cst)){
                                if(!wp->state)
                                        break;
                                ksiBSemWait(&wp->hanging);
                                n = dequeue_blocked(wp, tid);
                                continue;
                        }
                        else
                                break;
                }
                //break;
                KsiEngine *e = n->e;
                int32_t bufsize = e->framesPerBuffer;
                for(int32_t i=0;i<n->inputCount;i++){
#define cachePtr(buf) ((buf) + bufsize - 1)
                        int dirty = 0;
                        int8_t type = n->inputTypes[i];
                        // Mixer for each input port
                        if(type&ksiNodePortTypeEventFlag){
                        }
                        else{
                                KsiMixerEnvEntry *me = n->env[i].d->mixer;
                                KsiData *buf = n->env[i].d->buffer;
                                if(buf){
                                        (*cachePtr(buf)).i = 0;
                                        while(me){
                                                KsiNode *prec = me->src;
                                                if(!prec){ //src == NULL for bias
                                                        ksiDataIncrease(cachePtr(buf), me->gain, type);
                                                }
                                                else{
                                                        if(prec->outputTypes[me->srcPort]&ksiNodePortIODirty){
                                                                dirty=ksiNodePortIODirty;
                                                        }
                                                        else{
                                                                ksiDataWeightedIncrease(cachePtr(buf), *cachePtr(me->src->outputBuffer[me->srcPort].d), me->gain, type);
                                                        }
                                                }
                                                me = me->next;
                                        }
                                        if(dirty){
                                                me = n->env[i].d->mixer;//Reiterate the entry list and refresh buffer
                                                for(int32_t j=0;j<bufsize-1;j++){
                                                        buf[j] = *cachePtr(buf);
                                                }
                                                while(me){
                                                        KsiNode *prec = me->src;
                                                        if(prec&&(prec->outputTypes[me->srcPort]&ksiNodePortIODirty)){
                                                                ksiDataBufferWeightedIncrease(buf, prec->outputBuffer[me->srcPort].d, me->gain, bufsize, type);
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
                                                        }
                                                }
                                                else{//bias term
                                                        ksiNodePortIOClear(n->inputTypes[i]);
                                                }
                                        }
                                        else{
                                                ksiNodePortIOClear(n->inputTypes[i]);//Unused port
                                        }
                                }
                        }
                }
                ksiNodeFuncWrapper(n);
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
                                                enqueue_signaled(wp,&wp->tasks, tid, s);
                                        }
                                        else{
                                                foundContinuation = 1;
                                                n = s;
                                        }
                                }
                                sn = sn->next;
                        }
                        if(!foundContinuation)
                                n = dequeue_blocked(wp, tid);
                        break;
                }
                case ksiNodeTypeOutputFinal:
                        ksiSemPost(&n->e->masterSem);
                        n = dequeue_blocked(wp, tid);
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
                return ksiErrorIdempotent;
        }
        _ksiEngineReset(e);
        e->playing = 1;
        ksiEngineWorkerPoolOnlineIncrease(e->wp);
        ksiEnginePlayingUnlock(e);
        return ksiErrorNone;
}

KsiError ksiEngineStop(KsiEngine *e){
        return impl_ksiEngineStop(e, 0);
}
KsiError ksiEnginePause(KsiEngine *e){
        ksiEnginePlayingLock(e);
        KsiError err = ksiErrorNone;
        if(!(e->playing>0)){
                err = ksiErrorAudioNotStarted;
                goto ret;
        }
        if(e->playing == ksiEnginePaused){
                err = ksiErrorIdempotent;
                goto ret;
        }
        e->playing = ksiEnginePaused;
        ksiEngineWorkerPoolOnlineDecrease(e->wp);
ret:
        ksiEnginePlayingUnlock(e);
        return ksiErrorNone;
}
KsiError ksiEngineResume(KsiEngine *e){
        ksiEnginePlayingLock(e);
        KsiError err = ksiErrorNone;
        if(!(e->playing>0)){
                err = ksiErrorAudioNotStarted;
                goto ret;
        }
        if(e->playing==1){
                err = ksiErrorIdempotent;
                goto ret;
        }
        e->playing = ksiEnginePlaying;
        //ksiSemPost(&e->masterSem);
ret:
        ksiEnginePlayingUnlock(e);
        return ksiErrorNone;
}
