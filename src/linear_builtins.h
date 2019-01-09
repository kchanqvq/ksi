#ifndef __linear_builtins__
#define __linear_builtins__
#include "engine.h"
#include <math.h>
// 2 -> 2
static inline void ksiBuiltinNodeFunc2toStereo(KsiNode *n,KsiData **inputBuffers,KsiData *outputBuffer){
        for(int32_t i=0;i<n->e->framesPerBuffer;i++){
                ksiNodeRefreshCache(n, 0, i);
                ksiNodeRefreshCache(n, 1, i);
                outputBuffer[i*2].f=n->inputCache[0].f;
                outputBuffer[i*2+1].f=n->inputCache[1].f;
        }
}
// 1 -> 1
static inline void ksiBuiltinNodeFuncTestOsc(KsiNode *n,KsiData **inputBuffers,KsiData *ob){
        int32_t bufsize = n->e->framesPerBuffer;
        for(int32_t i = 0;i<bufsize;i++){
                ksiNodeRefreshCache(n, 0, i);
                float freq = n->inputCache[0].f==0?100.0:n->inputCache[0].f;
                float dfreq = freq/n->e->framesPerSecond;
                ob[i].f = sinf(2*M_PI*((float)(n->e->timeStamp)+i)*dfreq);
        }
        ksiNodePortIOSetDirty(n->outputTypes[0]);
}
// 2 -> 1
static inline void ksiBuiltinNodeFuncModulator(KsiNode *n,KsiData **inputBuffers,KsiData *ob){
        int32_t bufsize = n->e->framesPerBuffer;
        for(int32_t i=0;i<bufsize;i++){
                ksiNodeRefreshCache(n, 0, i);
                ksiNodeRefreshCache(n, 1, i);
                ob[i].f=n->inputCache[0].f*n->inputCache[1].f;
                //printf("%f,%f ", n->inputCache[0].f,n->inputCache[1].f);
        }
        ksiNodePortIOSetDirty(n->outputTypes[0]);
}
#endif
