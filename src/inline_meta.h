#include "util.h"
#include <inttypes.h>

//Will call yourPluginReset(1) when playback position changes
#define REQUIRE_RESET 1
#define NOT_REQUIRE_RESET 0

//Will call yourPluginInit(1) when making new node and yourPluginDestroy(1) when destroying
#define DYNAMIC_MEMORY 1
#define NO_DYNAMIC_MEMORY 0

//Each item: _(__,id,symbol name,(require reset?,require dynamic memory?),(list of input),(list of output))
#define INLINE_LIST(_,__,nil)                                           \
        _(__,1,ksiBuiltinNodeFunc2toStereo,(NOT_REQUIRE_RESET,NO_DYNAMIC_MEMORY),((Float)(Float)(nil)),((Float)(Float)(nil))) \
                _(__,2,ksiBuiltinNodeFuncTestOsc,(NOT_REQUIRE_RESET,NO_DYNAMIC_MEMORY),((Float)(nil)),((Float)(nil))) \
                _(__,3,kscarletMidiSeg,(REQUIRE_RESET,DYNAMIC_MEMORY),((Int32)(nil)),((Float)(Gate)(nil))) \
                _(__,4,kscarletWavetable,(NOT_REQUIRE_RESET,DYNAMIC_MEMORY),((Float)(Gate)(Int32)(Float)(nil)),((Float)(nil))) \
                _(__,5,kscarletADSR,(REQUIRE_RESET,DYNAMIC_MEMORY),((Gate)(Float)(Float)(Float)(Float)(nil)),((Float)(Gate)(nil))) \
                _(__,6,ksiBuiltinNodeFuncModulator,(NOT_REQUIRE_RESET,NO_DYNAMIC_MEMORY),((Float)(Float)(nil)),((Float)(nil)))
#define INLINE_PROPERTY_MF(__,id,name,prop,...) __ _N()(id,name,_E prop)
//_(id,symbol name,require reset?,...)
#define INLINE_PROPERTY(_) _E(INLINE_LIST(INLINE_PROPERTY_MF,_,nil))
#define INLINE_INPORT_END _E _N _N _N()()() (_E(_N,_N))
#define INLINE_INPORT_END2 _E _N _N ()() (_E(_N,_N))
//_(id,symbol name,number of inputs,(port1 type,port2 type,...))
#define INLINE_INPORT_MF(__,id,name,prop,in,...) __(id,name,LISTCOUNT in,(LIST2COMMA ( LIST2PORTTYPE in (LIST_END))))
#define INLINE_INPORT(_) INLINE_LIST(INLINE_INPORT_MF,_,INLINE_INPORT_END)
//_(id,symbol name,number of outputs,(port1 type,port2 type,...))
#define INLINE_OUTPORT_MF(__,id,name,prop,in,out,...) __(id,name,LISTCOUNT out,(LIST2COMMA ( LIST2PORTTYPE out (LIST_END))))
#define INLINE_OUTPORT(_) INLINE_LIST(INLINE_OUTPORT_MF,_,INLINE_INPORT_END)

#define DEF_INPORT(id,name,n,i) extern int8_t CAT(name,InPorts)[];
INLINE_INPORT(DEF_INPORT)
#define DEF_OUTPORT(id,name,n,i) extern int8_t CAT(name,OutPorts)[];
INLINE_OUTPORT(DEF_OUTPORT)
#undef DEF_INPORT
#undef DEF_OUTPORT
