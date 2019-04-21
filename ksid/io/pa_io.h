#ifndef pa_io_h
#define pa_io_h
#include "err.h"
extern int pa_initialized;
KsiError paIOInit(void *engine,const char **perrtxt);
KsiError paIODestroy(void *engine);
#endif
