#include "util.h"
#include <inttypes.h>

//Will call yourPluginReset(1) when playback position changes
#define REQUIRE_RESET 1
#define NOT_REQUIRE_RESET 0

//Will call yourPluginInit(1) when making new node and yourPluginDestroy(1) when destroying
#define DYNAMIC_MEMORY 1
#define NO_DYNAMIC_MEMORY 0

#define EDIT_CMD 1
#define NO_EDIT_CMD 0

//Each item: _(__,id,symbol name,(require reset?,require dynamic memory?,have editing commands?),(list of input),(list of output))
/* #define INLINE_LIST(_,__,nil)                                           \ */
/*         _(__,1,ksiBuiltinNodeFuncId,(NOT_REQUIRE_RESET,NO_DYNAMIC_MEMORY,EDIT_CMD), \ */
/*           ((Float)(nil)),                                               \ */
/*           ((Float)(nil)));                                              \ */
/*         _(__,2,ksiBuiltinNodeFuncTestOsc,(NOT_REQUIRE_RESET,NO_DYNAMIC_MEMORY,NO_EDIT_CMD), \ */
/*           ((Float)(nil)),                                               \ */
/*           ((Float)(nil)));                                              \ */
/*         _(__,3,kscarletMidiSeg,(REQUIRE_RESET,DYNAMIC_MEMORY,EDIT_CMD), \ */
/*           ((nil)),                                                      \ */
/*           ((FloatEvent)(GateEvent)(nil)));                              \ */
/*         _(__,4,kscarletWavetable,(NOT_REQUIRE_RESET,DYNAMIC_MEMORY,NO_EDIT_CMD), \ */
/*           ((Float)(Int32)(Float)(nil)),                                 \ */
/*           ((Float)(GateEvent)(nil)));                                   \ */
/*         _(__,5,kscarletADSR,(REQUIRE_RESET,DYNAMIC_MEMORY,NO_EDIT_CMD), \ */
/*           ((GateEvent)(nil)),                                                    \ */
/*           ((GateEvent)(Float)));                                        \ */
/*         _(__,6,ksiBuiltinNodeFuncModulator,(NOT_REQUIRE_RESET,NO_DYNAMIC_MEMORY,NO_EDIT_CMD), \ */
/*           ((Float)(Float)(nil)),                                        \ */
/*           ((Float)(nil))); */
#define INLINE_LIST(_,__,nil)                                           \
        _(__,1,ksiBuiltinNodeFuncId,(NOT_REQUIRE_RESET,NO_DYNAMIC_MEMORY,EDIT_CMD), \
          ((Float)(nil)),                                               \
          ((Float)(nil)));                                              \
        _(__,2,ksiBuiltinNodeFuncTestOsc,(NOT_REQUIRE_RESET,NO_DYNAMIC_MEMORY,NO_EDIT_CMD), \
          ((Float)(nil)),                                               \
          ((Float)(nil)));                                              \
        _(__,3,kscarletMidiSeg,(REQUIRE_RESET,DYNAMIC_MEMORY,EDIT_CMD), \
          ((nil)),                                                      \
          ((Float)(Gate)(nil)));                              \
        _(__,4,kscarletWavetable,(NOT_REQUIRE_RESET,DYNAMIC_MEMORY,NO_EDIT_CMD), \
          ((Float)(Gate)(Int32)(Float)(nil)),                               \
          ((Float)(Gate)(nil)));                                   \
        _(__,5,kscarletADSR,(REQUIRE_RESET,DYNAMIC_MEMORY,NO_EDIT_CMD), \
          ((Gate)(Float)(Float)(Float)(Float)(nil)),                         \
          ((Float)(Gate)(nil)));                                        \
        _(__,6,ksiBuiltinNodeFuncModulator,(NOT_REQUIRE_RESET,NO_DYNAMIC_MEMORY,NO_EDIT_CMD), \
          ((Float)(Float)(nil)),                                        \
          ((Float)(nil)));

#define INLINE_PROPERTY_MF(__,id,name,prop,...) __ _N()(id,name,_E prop)
//_(id,symbol name,require reset?,...)
#define INLINE_PROPERTY(_) _E(INLINE_LIST(INLINE_PROPERTY_MF,_,nil))
#define INLINE_INPORT_END _E _N _N _N()()() (_E(_N,_N))
#define INLINE_INPORT_END2 _E _N _N ()() (_E(_N,_N))
//_(id,symbol name,number of inputs,(port1 type,port2 type,...))
#define INLINE_PORTS_MF(__,id,name,prop,in,out,...) \
        __(id,name,                                                     \
           LISTNONNIL in, LISTCOUNT in,(LIST2COMMA ( LIST2PORTTYPE in (LIST_END))),\
           LISTNONNIL out, LISTCOUNT out,(LIST2COMMA ( LIST2PORTTYPE out (LIST_END)))\
           )
#define INLINE_PORTS(_) INLINE_LIST(INLINE_PORTS_MF,_,INLINE_INPORT_END)
//_(id,symbol name,number of outputs,(port1 type,port2 type,...))

#define DEF_PORTS(id,name,vi,ni,i,vo,no,o)   \
        CONDITIONAL(vi,extern int8_t CAT(name,InPorts)[]);          \
        CONDITIONAL(vo,extern int8_t CAT(name,OutPorts)[]);
INLINE_PORTS(DEF_PORTS);
#undef DEF_PORTS
#include "dag.h"
#include "err.h"
#define INLINE_CASE(id,name,res,dm,cmd,...)             \
        CONDITIONAL(res,void CAT(name,Reset)(KsiNode *n);)   \
        CONDITIONAL(dm,void CAT(name,Init)(KsiNode *n);) \
        CONDITIONAL(dm,void CAT(name,Destroy)(KsiNode *n);) \
        CONDITIONAL(cmd,KsiError CAT(name,EditCmd)(KsiNode *n,const char *args,const char **pcli_err_str,int flag);) \
        void name(KsiNode *n);
INLINE_PROPERTY(INLINE_CASE);
#undef INLINE_CASE
