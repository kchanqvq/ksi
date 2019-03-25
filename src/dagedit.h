#ifndef __dagedit_h__
#define __dagedit_h__
#include <stdio.h>
#include "engine.h"
KsiEngine *ksiEngineDestroyChild(KsiEngine *e);
KsiError ksiEngineAddNode(KsiEngine *e,int32_t typeFlags,int32_t *idRet,void *extArgs,size_t len);

KsiError ksiEngineMakeWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort,KsiData gain);
KsiError ksiEngineMakeBias(KsiEngine *e,int32_t desId,int32_t desPort,KsiData bias);
KsiError ksiEngineDetachNodes(KsiEngine *e,int32_t srcId,int32_t desId);
KsiError ksiEngineGetInputType(KsiEngine *e,int32_t id,int32_t port,int8_t *t);
KsiError ksiEngineRemoveNode(KsiEngine *e,int32_t id);
//len should include the '\0' !!!
KsiError ksiEngineSendEditingCommand(KsiEngine *e,int32_t id,const char *args,const char **pcli_err_str,size_t len);
KsiError ksiEngineRemoveWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort);
KsiError ksiEngineCommit(KsiEngine *e);

void ksiNodeSerialize(KsiNode *n,FILE *fp);
void ksiEngineSerialize(KsiEngine *e,FILE *fp);

void ksiNodeChangeInputPortCount(KsiNode *n,int32_t port_count,int8_t* newTypes,int flag);
void ksiNodeChangeOutputPortCount(KsiNode *n,int32_t port_count,int8_t* newTypes,int flag);
#endif
