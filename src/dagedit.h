#ifndef __dagedit_h__
#define __dagedit_h__
#include "dag.h"
#include "err.h"
void ksiEngineInit(KsiEngine *e,int32_t framesPerBuffer,int32_t framesPerSecond,int nprocs);
void ksiEngineDestroy(KsiEngine *e);
KsiEngine *ksiEngineDestroyChild(KsiEngine *e);
int32_t ksiEngineAddNode(KsiEngine *e,KsiNode *n);


KsiError ksiEngineMakeAdjustableWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort,float gain);
KsiError ksiEngineMakeAdjustableBias(KsiEngine *e,int32_t desId,int32_t desPort,float bias);
KsiError ksiEngineMakeDirectWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort);
KsiError ksiEngineDetachNodes(KsiEngine *e,int32_t srcId,int32_t desId);

KsiError ksiEngineRemoveNode(KsiEngine *e,int32_t id,KsiNode **n);

KsiError ksiEngineRemoveWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort);//0 for no wire. 1 for success.

KsiNode *ksiNodeInit(KsiNode *n,KsiNodeFunc f,int32_t inputCount,int32_t outputCount,int32_t typeFlags,KsiEngine *e,void *args,int32_t gatingOutput);
KsiNode *ksiNodeDestroy(KsiNode *n);

void ksiNodeSerialize(KsiNode *n,FILE *fp);
void ksiEngineSerialize(KsiEngine *e,FILE *fp);
#endif
