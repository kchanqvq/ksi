#include "inline_meta.h"
#include "dag.h"
#define DEF_PORTS(id,name,n,i) int8_t CAT(name,InPorts)[] = {_E i};
#define DEF_PORTS(id,name,vi,ni,i,vo,no,o)    \
        CONDITIONAL(vi, int8_t CAT(name,InPorts)[] = {_E i});           \
        CONDITIONAL(vo, int8_t CAT(name,OutPorts)[]) = {_E o};
INLINE_PORTS(DEF_PORTS)
