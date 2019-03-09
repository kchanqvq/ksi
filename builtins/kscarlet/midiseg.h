#ifndef __kscarlet_midiseg_h__
#define __kscarlet_midiseg_h__
#include "engine.h"
KsiError kscarletMidiSegEditCmd(KsiNode *n,const char *args,const char **pcli_err_str,int flag);
void kscarletMidiSegReset(KsiNode *n);
void kscarletMidiSegInit(KsiNode *n);
void kscarletMidiSegDestroy(KsiNode *n);
void kscarletMidiSeg(KsiNode *n);
#endif
