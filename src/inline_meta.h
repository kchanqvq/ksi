#include "linear_builtins.h"

#define REQUIRE_RESET 1
#define NOT_REQUIRE_RESET 0

#define DYNAMIC_MEMORY 1
#define NO_DYNAMIC_MEMORY 0
#define INLINE_LIST(_)                          \
        _(1,ksiBuiltinNodeFunc2toStereo,NOT_REQUIRE_RESET,NO_DYNAMIC_MEMORY) \
        _(2,ksiBuiltinNodeFuncTestOsc,NOT_REQUIRE_RESET,NO_DYNAMIC_MEMORY)               \
        _(3,kscarletMidiSeg,REQUIRE_RESET,DYNAMIC_MEMORY)\
        _(4,kscarletWavetable,NOT_REQUIRE_RESET,DYNAMIC_MEMORY)
#define INLINE_PORT(_)
