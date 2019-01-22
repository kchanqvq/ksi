#ifndef __engine_h__
#define __engine_h__
#include "dag.h"
#include "err.h"
#include "queue.h"
#include <pthread.h>
typedef struct _KsiEngine{
        int32_t framesPerBuffer;
        int32_t framesPerSecond;
        size_t timeStamp;
        KsiVec nodes;
        KsiVec timeseqResources;//use RBTree for midi and automation
        int nprocs;//not included master thread!
        pthread_t *workers;
        KsiWorkQueue tasks;
        KsiSem masterSem;
        _Atomic int waitingCount;
        pthread_mutex_t waitingMutex;
        pthread_cond_t waitingCond;
        void **outputBufferPointer;
#define ksiEngineStopped 0
#define ksiEnginePlaying 1
#define ksiEnginePaused 2
        int playing;

        atomic_flag notRequireReset;
        pthread_mutex_t editMutex;
        //_Atomic int cleanupCounter;
        KsiBSem hanging;
} KsiEngine;

int ksiEngineAudioCallback( const void *input,
                            void *output,
                            unsigned long frameCount,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData ) ;
KsiError ksiEngineReset(KsiEngine *e);
KsiError ksiEngineLaunch(KsiEngine *e);
KsiError ksiEngineStop(KsiEngine *e);
KsiError ksiEnginePause(KsiEngine *e);
KsiError ksiEngineResume(KsiEngine *e);
#endif
