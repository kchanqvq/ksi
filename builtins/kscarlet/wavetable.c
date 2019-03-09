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
void kscarletWavetable(KsiNode *n){
        int32_t bufsize = n->e->framesPerBuffer;
        plugin_env *env = (plugin_env *)n->args;
        if(!(n->inputTypes[1]&ksiNodePortIODirty)){
                if(ksiNodeGetInput(n, bufsize, 1, 0).i)
                ksiNodePortIOClear(n->outputTypes[0]);
                return;
        }
        for(int32_t i=0;i<bufsize;i++){
#define $freq ksiNodeGetInput(n, bufsize, 0, i).f
#define $gate ksiNodeGetInput(n, bufsize, 1, i).i
#define $wf ksiNodeGetInput(n, bufsize, 2, i).i
#define $mod ksiNodeGetInput(n, bufsize, 3, i).f
                if($gate){
                        continue;
                }
                env->currentPos+=$freq/n->e->framesPerSecond;
                /*
                static count = 0;
                if(env->currentPos!=env->currentPos&&!count){
                        count++;
                }*/
                env->currentPos = remainderf(env->currentPos, 1.0f);
                float mod = $mod;
                if(env->currentPos<0.0f)
                        env->currentPos+=1.0f;
                switch($wf){
                case 1:
                        n->outputBuffer[0].d[i].f=sinf(env->currentPos*2*M_PI);
                        break;
                case 2:
                        n->outputBuffer[0].d[i].f=-2*env->currentPos+1.0f;
                        break;
                case 3:
                        n->outputBuffer[0].d[i].f=env->currentPos<0.25f?env->currentPos*4:env->currentPos<0.75f?(-env->currentPos*4+2.0f):(env->currentPos*4-4.0f);
                        break;
                case 4:
                        n->outputBuffer[0].d[i].f=(mod<0.5f)?env->currentPos<mod?1.0f:(-mod/(1-mod)):env->currentPos<mod?((1-mod)/mod):-1.0f;
                        break;
                }
        }
        ksiNodePortIOSetDirty(n->outputTypes[0]);
}
