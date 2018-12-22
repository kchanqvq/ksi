#include "dag.h"
//input 0: freq (f)
//input 1: gate (g)
//input 2: waveform type (i32)
// 1 : sine
// 2 : sawtooth
// 3 : triangle
// 4 : square
//input 3: modulation (f)
//output 0: audio (f)
#include "wavetable.h"
#include <math.h>
typedef struct{
        float currentPos;
} plugin_env;
void kscarletWavetableInit(KsiNode *n){
        n->args = malloc(sizeof(plugin_env));
}
void kscarletWavetableDestroy(KsiNode *n){
        free(n->args);
}
#define $freq (n->inputCache[0].f)
#define $gate (n->inputCache[1].i)
#define $wf (n->inputCache[2].i)
#define $mod (n->inputCache[3].i)
void kscarletWavetable(KsiNode *n,KsiData **inputBuffers,KsiData *outputBuffer){
        plugin_env *env = (plugin_env *)n->args;
        if($gate&&!(n->inputTypes[1]&ksiNodePortIODirty)){
                ksiNodePortIOClear(n->outputTypes[0]);
                return;
        }
        for(int32_t i=0;i<n->e->framesPerBuffer;i++){
                ksiNodeRefreshCache(n, 0, i);
                ksiNodeRefreshCache(n, 1, i);
                ksiNodeRefreshCache(n, 2, i);
                ksiNodeRefreshCache(n, 3, i);
                if($gate){
                        continue;
                }
                env->currentPos+=$freq/n->e->framesPerSecond;
                static count = 0;
                if(env->currentPos!=env->currentPos&&!count){
                        count++;
                }
                env->currentPos = remainderf(env->currentPos, 1.0f);
                float mod = inputBuffers[3][i].f;
                if(env->currentPos<0.0f)
                        env->currentPos+=1.0f;
                switch($wf){
                case 1:
                        outputBuffer[i].f=sinf(env->currentPos*2*M_PI);
                        break;
                case 2:
                        outputBuffer[i].f=-2*env->currentPos+1.0f;
                        break;
                case 3:
                        outputBuffer[i].f=env->currentPos<0.25f?env->currentPos*4:env->currentPos<0.75f?(-env->currentPos*4+2.0f):(env->currentPos*4-4.0f);
                        break;
                case 4:
                        outputBuffer[i].f=(mod<0.5f)?env->currentPos<mod?1.0f:(-mod/(1-mod)):env->currentPos<mod?((1-mod)/mod):-1.0f;
                        break;
                }
        }
        ksiNodePortIOSetDirty(n->outputTypes[0]);
}
