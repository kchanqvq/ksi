#ifndef __engine_h__
#define __engine_h__
#include "dag.h"
#include "err.h"
int ksiEngineAudioCallback( const void *input,
                            void *output,
                            unsigned long frameCount,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData ) ;
KsiError ksiEngineLaunch(KsiEngine *e);
KsiError ksiEngineStop(KsiEngine *e);
#endif
