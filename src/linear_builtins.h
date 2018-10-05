#ifndef __linear_builtins__
#define __linear_builtins__
#include "dag.h"
#include <math.h>
// 2 -> 2
static inline void ksiBuiltinNodeFunc2toStereo(KsiNode *n,KsiData **inputBuffers,KsiData *outputBuffer){
        for(int32_t i=0;i<n->e->framesPerBuffer;i++){
                outputBuffer[i*2].f=inputBuffers[0][i].f;
                outputBuffer[i*2+1].f=inputBuffers[1][i].f;
        }
}
// 1 -> 1
static inline void ksiBuiltinNodeFuncTestOsc(KsiNode *n,KsiData **ib,KsiData *ob){
        float freq = 100;
        if(n->args)
                freq = ((KsiData *)n->args)->f;
        if(ib[0]){
                freq = ib[0][0].f;
        }
        int32_t bufsize = n->e->framesPerBuffer;
        float dfreq = freq/n->e->framesPerSecond;
        for(int32_t i = 0;i<bufsize;i++){
                ob[i].f = sinf(2*M_PI*((float)(n->e->timeStamp)+i)*dfreq);
        }
}
// 2 -> 1
static inline void ksiBuiltinNodeFuncModulator(KsiNode *n,KsiData **ib,KsiData *ob){
        int32_t bufsize = n->e->framesPerBuffer;
        for(int32_t i=0;i<bufsize;i++){
                ob[i].f=ib[0][i].f*ib[1][i].f;
        }
}
#endif
