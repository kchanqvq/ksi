
#ifndef __dag_h__
#define __dag_h__
#include <inttypes.h>
#include <stdatomic.h>
#include <semaphore.h>
#include "vec.h"
#include "sem.h"
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
typedef struct{
        KsiData *buffer;//when NULL, direct store src output ptr into internalBufferPtr
        KsiMixerEnvEntry *mixer;
} KsiPortEnv;
struct _KsiEngine;
typedef struct _KsiNode{
        _Atomic int depCounter;
        int depNum;
        KsiNodeFunc f;
        struct _KsiEngine *e;

        int32_t id;

        KsiVecIdlistNode *successors;
        //For gating: 1 for bypass, 0 for enable
        int32_t outputCount;
        KsiData *outputBuffer;

#define ksiNodePortTypeFloat 0x0
#define ksiNodePortTypeGate 0x1
#define ksiNodePortTypeInt32 0x2
#define ksiNodePortTypeMask 0xF
#define ksiNodePortIOMask 0xF0
#define ksiNodePortIODirty 0x10 //A buffer of updates
#define ksiNodePortIOClear(t) t=((t)&~ksiNodePortIOMask)
#define ksiNodePortIOSetDirty(t) t=((t)&~ksiNodePortIOMask)|ksiNodePortIODirty
#define ksiNodeRefreshCache(n,p,i) if(n->inputTypes[p]&ksiNodePortIODirty) \
        n->inputCache[p] = inputBuffers[p][i];
        int8_t *outputTypes;
        int8_t *inputTypes;

        KsiData *outputCache;
        KsiData *inputCache;

        int32_t type;//type flags
#define ksiNodeTypeOutputMask 0xF0
#define ksiNodeTypeOutputNormal 0x00
#define ksiNodeTypeOutputFinal 0x10
#define ksiNodeTypeScratchMask 0xF00
#define ksiNodeTypeScratchPredecessor 0x100
#define ksiNodeTypeInlineNodeFuncMask 0xFFFF0000
#define ksiNodeTypeInlineId(t) (((t)&ksiNodeTypeInlineNodeFuncMask)>>16)
        void *args;
        int32_t inputCount;
        struct {
                KsiPortEnv *portEnv;//one portEnv for each input port
                KsiData **internalBufferPtr;
        } env;
} KsiNode;
static inline KsiData ksiNodeGetInput(KsiNode *n,KsiData **ib,int32_t port,int32_t i){
        return (n->inputTypes[port]&ksiNodePortIODirty)?ib[port][i]:n->inputCache[port];
}
typedef struct _KsiVecNodelistNode{
        struct _KsiVecNodelistNode *next;
        KsiNode *node;
} KsiVecNodelistNode;
ksiVecDeclareList(VecNodelist, KsiNode*, node);
#endif
