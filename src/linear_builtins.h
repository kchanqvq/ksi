#ifndef __linear_builtins__
#define __linear_builtins__
#include "engine.h"
#include "mvcc_utils.h"
#include <inttypes.h>

void ksiBuiltinNodeFuncId(KsiNode *n,KsiData **inputBuffers,KsiData *outputBuffer);
KsiError ksiBuiltinNodeFuncIdEditCmd(KsiNode *n,const char *args,const char **pcli_err_str,int flag);
void ksiBuiltinNodeFuncTestOsc(KsiNode *n,KsiData **inputBuffers,KsiData *ob);
void ksiBuiltinNodeFuncModulator(KsiNode *n,KsiData **inputBuffers,KsiData *ob);
#endif
