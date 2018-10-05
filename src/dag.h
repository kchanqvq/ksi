
#ifndef __dag_h__
#define __dag_h__
#include <inttypes.h>
#include <stdatomic.h>
#include <semaphore.h>
#include "vec.h"
#include "sem.h"
#include "lcrq/queue.h"
#include <portaudio.h>
typedef union{int32_t i;float f;} KsiData;
struct _KsiNode;
typedef void (*KsiNodeFunc)(struct _KsiNode *n,KsiData **inputBuffers,KsiData *outputBuffer);
typedef struct _KsiMixerEnvEntry{
        int32_t srcPort;
        struct _KsiNode *src;
        float gain;
        int32_t inputPort;
        struct _KsiMixerEnvEntry *next;
} KsiMixerEnvEntry;
struct _KsiEngine;
typedef struct _KsiNode{
        _Atomic int depCounter;
        int depNum;
        KsiNodeFunc f;
        struct _KsiEngine *e;

        int32_t id;

        KsiVecIdlistNode *successors;
        int32_t gatingOutput;//1 for bypass, 0 for enable
        int32_t outputCount;
        KsiData *outputBuffer;

#define ksiNodePortTypeFloat 0x0
#define ksiNodePortTypeGating 0x1
#define ksiNodePortTypeInt32 0x2
        int8_t *outputTypes;
        int8_t *inputTypes;

        int32_t type;//type flags
#define ksiNodeTypeInputMask 0xF
#define ksiNodeTypeInputFixed 0
#define ksiNodeTypeInputMixer 1
#define ksiNodeTypeOutputMask 0xF0
#define ksiNodeTypeOutputNormal 0x00
#define ksiNodeTypeOutputFinal 0x10
#define ksiNodeTypeScratchMask 0xF00
#define ksiNodeTypeScratchPredecessor 0x100
#define ksiNodeTypeInlineNodeFuncMask 0xFFFF0000
#define ksiNodeTypeInlineId(t) (((t)&ksiNodeTypeInlineNodeFuncMask)>>16)
        void *args;
        int32_t inputCount;
        union{
                struct {
                        KsiData **predecessorBuffers;
                        struct _KsiNode **predecessors;
                } fixed;
                struct {
                        KsiMixerEnvEntry *predecessors;
                        KsiData *gatingBuffer;
                        KsiData *internalBuffer;
                        KsiData **internalBufferPtr;
                } mixer;
        } env;
} KsiNode;
typedef struct _KsiVecNodelistNode{
        struct _KsiVecNodelistNode *next;
        KsiNode *node;
} KsiVecNodelistNode;
ksiVecDeclareList(VecNodelist, KsiNode*, node);

typedef struct _KsiEngine{
        int32_t framesPerBuffer;
        int32_t framesPerSecond;
        size_t timeStamp;
        KsiVec nodes;
        KsiVec timeseqResources;//use RBTree for midi and automation
        int nprocs;//not included master thread!
        pthread_t *workers;
        queue_t tasks;
        handle_t masterHandle;
        KsiSem masterSem;
        void **outputBufferPointer;
        int playing;
} KsiEngine;
#endif
