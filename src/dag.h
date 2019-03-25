#ifndef __dag_h__
#define __dag_h__
#include <inttypes.h>
#include <stdatomic.h>
#include <semaphore.h>
#include "vec.h"
#include "sem.h"
#include "dag_members.h"
struct _KsiEngine;
typedef struct _KsiNode{
        _Atomic int depCounter;
        int depNum;
        struct _KsiEngine *e;
        KsiVecIdlistNode *successors;

#define ksiNodeTypeOutputMask 0xF0
#define ksiNodeTypeOutputNormal 0x00
#define ksiNodeTypeOutputFinal 0x10
#define ksiNodeTypeScratchMask 0xF00
#define ksiNodeTypeScratchPredecessor 0x100
#define ksiNodeTypeDynamicInputType 0x1000
#define ksiNodeTypeDynamicOutputType 0x2000
#define ksiNodeTypeInlineNodeFuncMask 0xFFFF0000
#define ksiNodeTypeInlineId(t) (((t)&ksiNodeTypeInlineNodeFuncMask)>>16)
        int32_t id;
        int32_t type;//type flags

#define ksiNodePortIOMask 0xF0
#define ksiNodePortIODirty 0x10 //A buffer of updates
#define ksiNodePortIOClear(t) t=((t)&~ksiNodePortIOMask)
#define ksiNodePortIOSetDirty(t) t=((t)&~ksiNodePortIOMask)|ksiNodePortIODirty
        //Port Id: Positive - signal port; Negative - event port
        //For gating: 1 for bypass, 0 for enable
        int32_t outputCount;
        int8_t *outputTypes;
        KsiOutputPtr *outputBuffer;

        int32_t inputCount;
        int8_t *inputTypes;
        KsiEnvPtr *env;

        void *args; // For internal audio processing use only!
        void *extArgs; // Editing command should only access extArg, whose synchronization is managed by KSI
        // extArgs should be read only for audio threads, otherwise the design will be broken
} KsiNode;
static inline KsiData ksiNodeGetInput(KsiNode *n,int32_t framesPerBuffer, int32_t port,int32_t i){
        KsiSignalEnv *se = n->env[port].d;
        if(n->inputTypes[port]&ksiNodePortIODirty)
                return se->internalBufferPtr[i];
        if(se->internalBufferPtr)
                return se->internalBufferPtr[framesPerBuffer-1];
        if(se->mixer)
                return se->mixer->gain;
        return (KsiData){.i=0};

}
static inline void ksiNodeBeginIterateEvent(KsiNode *n,int32_t port){
        KsiEventEnvEntry *ee = n->env[port].e->ees;
        while(ee){
                ee->currentNode = ee->src->outputBuffer[ee->srcPort].e->tail;
                ee = ee->next;
        }
}
static inline KsiEvent *ksiNodeGetNextEvent(KsiNode *n, int32_t port){
        /*This is not elegant, forgive me*/
        KsiEventEnvEntry *ee = n->env[port].e->ees;
        if(!ee)
                return NULL;
        KsiEventNode **cn = NULL; //Soonest event node
        size_t ct = 0; //Soonest timestamp
        int flag = 0; //iterate through at least 1 event
        while(ee){
                if(ee->currentNode && (!flag || (ee->currentNode->e.timeStamp < ct))){
                        ct = ee->currentNode->e.timeStamp;
                        cn = &(ee->currentNode);
                        flag = 1;
                }
                ee = ee->next;
        }
        KsiEvent *res = NULL;
        if(cn){
                res = &((*cn)->e);
                *cn = (*cn)->next;
        }
        return res;
}
ksiVecDeclareList(VecNodelist, KsiNode*, node);
#endif
