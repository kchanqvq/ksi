#include "inline_meta.h"
#include "dag.h"
#define DEF_INPORT(id,name,n,i) int8_t CAT(name,InPorts)[] = {_E i};
INLINE_INPORT(DEF_INPORT)
#define DEF_OUTPORT(id,name,n,i) int8_t CAT(name,OutPorts)[] = {_E i};
INLINE_OUTPORT(DEF_OUTPORT)
