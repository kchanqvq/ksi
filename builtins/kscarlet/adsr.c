#include "dag.h"
//input 0: gate (g)
//input 1: attack (f)
//input 2: decay (f)
//input 3: sustain (f)
//input 4: release (f)
//output 0: modulation (f)
//output 1: gate (g)
#include "adsr.h"
#include <math.h>
typedef struct{
        int32_t currentTime;
        int8_t currentStage;
        float currentStageStartMod;
        float currentMod;
        float currentBias;
        int32_t stageDuration;
} plugin_env;
#define stageAttack 1
#define stageDecay 2
#define stageSustain 3
#define stageRelease 4
#define stageHalt 0
void kscarletADSRInit(KsiNode *n){
        n->args=malloc(sizeof(plugin_env));
}
void kscarletADSRDestroy(KsiNode *n){
        free(n->args);
}
void kscarletADSRReset(KsiNode *n){
        plugin_env *env = (plugin_env *)n->args;
        env->currentStage = stageHalt;
}
void kscarletADSR(KsiNode *n,KsiData **inputBuffers,KsiData *outputBuffer){
        plugin_env *env = (plugin_env *)n->args;
        int32_t bufsize = n->e->framesPerBuffer;
        if(inputBuffers[0][0].i==-1){
                if(env->currentStage == stageHalt){
                        outputBuffer[bufsize].i=-1;
                        return;
                }
                outputBuffer[bufsize].i=-1;
                return;//TODO DUMMY
        }
        for(int32_t i=0;i<bufsize;i++){
                if((env->currentStage==stageHalt||env->currentStage==stageRelease)&&inputBuffers[0][i].i==0){
                        env->currentTime = 1;
                        env->currentStage = stageAttack;
                        env->stageDuration = round(inputBuffers[1][i].f*n->e->framesPerSecond);
                        env->currentStageStartMod = env->currentMod;
                        env->currentBias = (M_E-env->currentStageStartMod)/(M_E-1);
                }
                else if((env->currentStage!=stageHalt||env->currentStage!=stageRelease)&&inputBuffers[0][i].i==1){
                        env->currentTime = 1;
                        env->currentStage = stageRelease;
                        env->stageDuration = round(inputBuffers[4][i].f*n->e->framesPerSecond);
                        env->currentStageStartMod = env->currentMod;
                        env->currentBias = env->currentStageStartMod/(M_E*M_E*M_E-1);
                }
                switch(env->currentStage){
                case stageAttack:
                        env->currentMod = env->currentBias-(env->currentBias-env->currentStageStartMod)*expf(-env->currentTime/(float)env->stageDuration);
                        if(!(env->currentTime<env->stageDuration)){
                                env->currentStage = stageDecay;
                                env->currentStageStartMod=inputBuffers[3][i].f;
                                env->stageDuration=round(inputBuffers[2][i].f*n->e->framesPerSecond);
                                env->currentBias = (M_E*M_E*M_E*env->currentStageStartMod-1)/(M_E*M_E*M_E-1);
                                env->currentTime = 0;
                        }
                        break;
                case stageDecay:
                        env->currentMod = env->currentStageStartMod + (1.0f-env->currentStageStartMod)*expf(-env->currentTime/(float)env->stageDuration*3);
                        if(!(env->currentTime<env->stageDuration)){
                                env->currentStage=stageSustain;
                        }
                        break;
                case stageRelease:
                        env->currentMod = (env->currentStageStartMod+env->currentBias)*expf(-env->currentTime/(float)env->stageDuration*3);
                        if(!(env->currentTime<env->stageDuration)){
                                env->currentStage=stageHalt;
                        }
                        break;
                }
                env->currentTime++;
                if(env->currentStage!=stageHalt){
                        outputBuffer[i].f=env->currentMod;
                        outputBuffer[i+bufsize].i=0;
                }
                else{
                        outputBuffer[i+bufsize].i=1;
                }
        }
}
