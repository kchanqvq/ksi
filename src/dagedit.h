#ifndef __dagedit_h__
#define __dagedit_h__
#include "dag.h"
#include "err.h"
void ksiEngineInit(KsiEngine *e,int32_t framesPerBuffer,int32_t framesPerSecond,int nprocs);
void ksiEngineDestroy(KsiEngine *e);
KsiEngine *ksiEngineDestroyChild(KsiEngine *e);
int32_t ksiEngineAddNode(KsiEngine *e,KsiNode *n);


KsiError ksiEngineMakeWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort,KsiData gain);
KsiError ksiEngineMakeBias(KsiEngine *e,int32_t desId,int32_t desPort,KsiData bias);
KsiError ksiEngineDetachNodes(KsiEngine *e,int32_t srcId,int32_t desId);
KsiError ksiEngineGetInputType(KsiEngine *e,int32_t id,int32_t port,int8_t *t);
KsiError ksiEngineGetParamentType(KsiEngine *e,int32_t id,int32_t pid,int8_t *t);
KsiError ksiEngineSetParament(KsiEngine *e,int32_t id,int32_t pid,KsiData d);
KsiError ksiEngineRemoveNode(KsiEngine *e,int32_t id,KsiNode **n);

KsiError ksiEngineRemoveWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort);

KsiNode *ksiNodeInit(KsiNode *n,int32_t typeFlags,KsiEngine *e,void *args);
KsiNode *ksiNodeDestroy(KsiNode *n);

void ksiNodeSerialize(KsiNode *n,FILE *fp);
void ksiEngineSerialize(KsiEngine *e,FILE *fp);

#endif
