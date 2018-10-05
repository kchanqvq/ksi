#ifndef __resource_h__
#define __resource_h__
#include "err.h"
#include "rbtree.h"
#include "dag.h"
KsiError ksiTimeSeqLoadFromTextFile(KsiRBNode **root,FILE *fp,int32_t framesPerSecond);
KsiError ksiTimeSeqLoadToEngineFromTextFilePath(KsiEngine *e,const char * restrict path,int32_t *id);
KsiError ksiTimeSeqUnloadFromEngine(KsiEngine *e,int32_t id);
#endif
