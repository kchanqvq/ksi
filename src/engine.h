#ifndef __engine_h__
#define __engine_h__
#include "dag.h"
#include "err.h"
#include "queue.h"
#include <pthread.h>
#include "rbtree.h"
#include "spsc_buffer.h"
#include "_config.h"
#include "cmd_queue.h"
#ifndef ALIGN
#define ALIGN __attribute__((aligned(_CONFIG_CACHE_SIZE*2)))
#endif
typedef struct _KsiEngine{
        int32_t framesPerBuffer;
        int32_t framesPerSecond;
        size_t timeStamp;

        /* The approach here:
           [When the engine is online]
           1) The 2 versions are all synchronized (sharing any buffer), audio thread is using nodes[epoch]. epoch == audioEpoch.
           2) The editor thread perform edit on nodes[(epoch+1)%2]. Any new allocated memory is stored into a buffer. Then, a commitment is made.
           -- From this step on, the work may be done in another thread to avoid blocking the editor thread.
           3) Update epoch to (1+epoch)%2
           4) Wait until epoch == audioEpoch which means the audio thread has migrated to the new version.
           5) Perform the same editing sequence on nodes[(epoch+1)%2] (it is the old version!). Any RT data allocation here is overriden by the pointers in the allocating buffer to make sure the two version shares the same buffers.
           6) Now the 2 versions are synchronized again.

           [When the engine is offline (stopped/paused)]
           Step 5) doesn't wait for any condition.
         */
        KsiVec nodes[2];//2 epoches MVCC
        KsiVec timeseqResources;// use RBTree for midi and automation
        int nprocs;// not included master thread!
        pthread_t *workers;
        pthread_t committer;
        KsiSem committingSem; // editor -> committer: I've finished a bunch of editing
        KsiSem migratedSem; // audio -> committer: I've updated audioEpoch
        KsiWorkQueue tasks;
        KsiSem masterSem;
        //_Atomic int waitingCount;
        //pthread_mutex_t waitingMutex;
        //pthread_cond_t waitingCond;
        void **outputBufferPointer[2];//One for each version
#define ksiEngineStopped 0
#define ksiEnginePlaying 1
#define ksiEnginePaused 2
#define ksiEngineFinalizing -1
        int playing;
        pthread_mutex_t playingMutex;// Protecting playing. Since playing is read-only for audio threads this won't block audio threads.
        // Both editing and commitment thread try to obtain this lock when editing so it ensures no 2 editings are performing on the cold version at the same time.
        // The commitment thread have to update audioEpoch when it is not playing.

        atomic_flag notRequireReset;
        // _Atomic int cleanupCounter;
        KsiBSem hanging;

        KsiSPSCCmdList syncCmds;// Editing to be synchronized
        KsiSPSCPtrList mallocBufs;// Alocated buffers
        KsiSPSCPtrList freeBufs;// Buffers to be freed (when the previous epoch has been given up by audio threads)

        // Put the hot variables in the end and aligned
        // Epoch choosing the 2 nodes list
        _Atomic int epoch ALIGN; // Owned by low priority editing thread. \in {0,1}.
        _Atomic int audioEpoch ALIGN;// Owned by audio callback thread. \in {0,1}.
        // When epoch != audioEpoch, epoch cannot be updated by commit.
        // Wait until epoch == audioEpoch.
} KsiEngine;
static inline void ksiEnginePlayingLock(KsiEngine *e){
        pthread_mutex_lock(&e->playingMutex);
}
static inline void ksiEnginePlayingUnlock(KsiEngine *e){
        pthread_mutex_unlock(&e->playingMutex);
}
void ksiEngineInit(KsiEngine *e,int32_t framesPerBuffer,int32_t framesPerSecond,int nprocs);
void ksiEngineDestroy(KsiEngine *e);

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
