#ifndef __err_h__
#define __err_h__
#include <inttypes.h>
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
                _(ksiErrorSrcPortOccupied,"Source port already occupied. Remove current wire to it first") \
                _(ksiErrorDesPortOccupied,"Destination port already occupied. Remove current wire to it first") \
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
                _(ksiErrorPlugin,"Plugin error")
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