#ifndef __dag_h__
#define __dag_h__
#include <inttypes.h>
#include <stdatomic.h>
#include <semaphore.h>
#include "vec.h"
#include "sem.h"
#include "events.h"
#include <portaudio.h>
struct _KsiNode;
typedef union{int32_t i;float f;} KsiData;
typedef void (*KsiNodeFunc)(struct _KsiNode *n,KsiData **inputBuffers,KsiData *outputBuffer);
typedef struct _KsiMixerEnvEntry{
        int32_t srcPort;
        struct _KsiNode *src;
        KsiData gain;
        struct _KsiMixerEnvEntry *next;
} KsiMixerEnvEntry;
ksiVecDeclareList(EventQueueList, KsiEventQueue *, eq);

typedef struct{
        int32_t numOfInputs;
        KsiEventQueueListNode *eqs;
} KsiEventEnv;
typedef struct{
        KsiData *buffer;//when NULL, direct store src output ptr into internalBufferPtr
        KsiData *internalBufferPtr;
        KsiData cache;
        KsiMixerEnvEntry *mixer;
} KsiSignalEnv;
typedef union {
        KsiEventEnv *e;
        KsiSignalEnv *d;
} KsiEnvPtr;

typedef union {
        KsiEventQueue e;
        KsiData *d;
} KsiOutputPtr;
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

#define ksiNodePortTypeFloat 0x0
#define ksiNodePortTypeGate 0x1
#define ksiNodePortTypeInt32 0x2
#define ksiNodePortTypeEventFlag 0x8
#define ksiNodePortTypeMask 0xF
#define ksiNodePortIOMask 0xF0
#define ksiNodePortIODirty 0x10 //A buffer of updates
#define ksiNodePortIOClear(t) t=((t)&~ksiNodePortIOMask)
#define ksiNodePortIOSetDirty(t) t=((t)&~ksiNodePortIOMask)|ksiNodePortIODirty
#define ksiNodeRefreshCache(n,p,i) if(n->inputTypes[p]&ksiNodePortIODirty) \
                n->inputCache[p] = inputBuffers[p][i];
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
static inline KsiData ksiNodeGetInput(KsiNode *n,KsiData **ib,int32_t port,int32_t i){
        return (n->inputTypes[port]&ksiNodePortIODirty)?ib[port][i]:n->inputCache[port];
}
ksiVecDeclareList(VecNodelist, KsiNode*, node);
#endif
