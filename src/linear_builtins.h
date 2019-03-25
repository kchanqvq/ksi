#ifndef __linear_builtins__
#define __linear_builtins__
#include "engine.h"
#include <inttypes.h>

//For audio output only
void ksiBuiltinNodeFuncId(KsiNode *n);
KsiError ksiBuiltinNodeFuncIdEditCmd(KsiNode *n,const char *args,const char **pcli_err_str,int flag);
void ksiBuiltinNodeFuncTestOsc(KsiNode *n);
void ksiBuiltinNodeFuncModulator(KsiNode *n);
#endif
