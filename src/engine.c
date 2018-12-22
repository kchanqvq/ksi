#include "engine.h"
#include <pthread.h>
#include "util.h"
#include "linear_builtins.h"
#include "data.h"
#define QUEUE_EMPTY ((void *)-1ll)
int ksiEngineAudioCallback( const void *input,
                                   void *output,
                                   unsigned long frameCount,
                                   const PaStreamCallbackTimeInfo* timeInfo,
                                   PaStreamCallbackFlags statusFlags,
                                   void *userData ){
        KsiEngine *e = (KsiEngine *)userData;
        if(!e->playing)
                return 0;
        ksiVecBeginIterate(&e->nodes, i);
        KsiNode *n = (KsiNode *)i;
        *e->outputBufferPointer = output;
        if(!n->depNum)
                enqueue(&e->tasks, &e->masterHandle, n);
        ksiVecEndIterate();
        ksiSemWait(&e->masterSem);
        e->timeStamp+=frameCount;
        return 0;
}
static _Atomic int thCounter = 1;
static inline KsiNode *dequeue_blocked(KsiEngine *e,handle_t *ph){
        void *result = QUEUE_EMPTY;
        while(result == QUEUE_EMPTY){
                if(!e->playing)
                        break;
                result = dequeue(&e->tasks, ph);
        }
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
        handle_t h;
        int tid = atomic_fetch_add(&thCounter, 1);
        queue_register(&e->tasks, &h, tid);
        KsiNode *n = dequeue_blocked(e, &h);
        int32_t bufsize = e->framesPerBuffer;
        while(1){
                if(n==QUEUE_EMPTY)
                        break;
                for(int32_t i=0;i<n->inputCount;i++){
                        n->inputCache[i].i = 0;
                        int dirty = 0;
                        int8_t type = n->inputTypes[i];
                        KsiMixerEnvEntry *me = n->env.portEnv[i].mixer;
                        KsiData *buf = n->env.portEnv[i].buffer;
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
                                                ksiDataBufferWeightedIncrease(buf, prec->outputBuffer+bufsize*i, me->gain, bufsize, type);
                                        }
                                        me = me->next;
                                }
                        }
                        n->inputTypes[i]=(n->inputTypes[i]&~ksiNodePortIOMask)|dirty;
                }
                ksiNodeFuncWrapper(n,n->env.mixer.internalBufferPtr,n->outputBuffer);
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
                                                enqueue(&e->tasks, &h, s);
                                        }
                                        else{
                                                foundContinuation = 1;
                                                n = s;
                                        }
                                }
                                sn = sn->next;
                        }
                        if(!foundContinuation)
                                n = dequeue_blocked(e, &h);
                        break;
                }
                case ksiNodeTypeOutputFinal:
                        ksiSemPost(&e->masterSem);
                        n = dequeue_blocked(e, &h);
                        break;
                }
        }
        handle_free(&h);
        return NULL;
}
KsiError ksiEngineReset(KsiEngine *e){
        ksiVecBeginIterate(&(e->nodes), i);
        KsiNode *n = (KsiNode *)i;
        switch(ksiNodeTypeInlineId(n->type)){
#define INLINE_CASE(id,name,reqrs,dm,...)                   \
                case id:                                 \
                        if(reqrs){                  \
                                CAT(name,Reset)(n); \
                        }                           \
                break;
                INLINE_PROPERTY(INLINE_CASE);
        }
        ksiVecEndIterate();
}
KsiError ksiEngineLaunch(KsiEngine *e){
        if(e->playing)
                return ksiErrorAlreadyPlaying;
        if(!e->outputBufferPointer)
                return ksiErrorNoFinal;
        ksiEngineReset(e);
        e->playing = 1;
        for(int i=0;i<e->nprocs;i++){
                pthread_create(e->workers+i, NULL, ksiEngineAudioWorker, e);
        }
        return ksiErrorNone;
}
KsiError ksiEngineStop(KsiEngine *e){
        if(!e->playing)
                return ksiErrorAlreadyStopped;
        e->playing = 0;
        //ksiSemPost(&e->masterSem);
        for(int i=0;i<e->nprocs;i++){
                pthread_join(e->workers[i], NULL);
        }
        return ksiErrorNone;
}
