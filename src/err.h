#ifndef __err_h__
#define __err_h__
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#define __err_fuck_up_emacs_indent()
#define __err_list(_)        __err_fuck_up_emacs_indent()               \
                _(ksiErrorNone,"Success")                               \
                _(ksiErrorIdNotFound,"No node with provided ID")        \
                _(ksiErrorSrcIdNotFound,"No node with provided source ID") \
                _(ksiErrorDesIdNotFound,"No node with provided destination ID") \
                _(ksiErrorTimeSeqIdNotFound,"No time sequence resource with provided ID") \
                _(ksiErrorPortNotFound,"Port out of range")             \
                _(ksiErrorSrcPortNotFound,"Source port out of range")   \
                _(ksiErrorDesPortNotFound,"Destination port out of range") \
                _(ksiErrorAlreadyWire,"There's already a wire between the spercified ports.") \
                _(ksiErrorMultipleFinal,"There's already a final output") \
                _(ksiErrorRing,"The wire to add will form a ring")\
                _(ksiErrorWireNotFound,"No wire between provided ports")\
                _(ksiErrorFileSystem,"File system error")\
                _(ksiErrorAudio,"Audio error")\
                _(ksiErrorAlreadyPlaying,"Already playing")\
                _(ksiErrorAlreadyStopped,"Already stopped")\
                _(ksiErrorNoFinal,"No final output node")\
                _(ksiErrorSyntax,"Syntax error")\
                _(ksiErrorResIdNotFound,"No loaded time sequence resource with provided ID")\
                _(ksiErrorType,"Incompatible input and output type")\
                _(ksiErrorNeedSignal,"This operation is only allowed for signal ports.")                                  \
                _(ksiErrorNoEditCmd,"The plugin does not support editing")\
                _(ksiErrorNoPlugin,"No plugin with spercified plugin ID")\
                _(ksiErrorPlugin,"Plugin error")\
                _(ksiErrorUninitialized,"System uninitialized")
#define __err_enum(x,y) x,
typedef enum{
        __err_list(__err_enum)
} KsiError;
#undef __err_enum
extern const char * const __err_strings[];
static inline const char* ksiErrorMsg(KsiError err){
        return __err_strings[err];
}
#endif
