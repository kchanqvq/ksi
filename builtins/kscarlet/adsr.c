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
        int32_t currentStage;
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
        kscarletADSRReset(n);
}
void kscarletADSRDestroy(KsiNode *n){
        free(n->args);
}
void kscarletADSRReset(KsiNode *n){
        plugin_env *env = (plugin_env *)n->args;
        env->currentStage = stageHalt;
        env->currentMod = 0.0f;
}

void kscarletADSR(KsiNode *n){
        plugin_env *env = (plugin_env *)n->args;
        int32_t bufsize = n->e->framesPerBuffer;
        ksiNodeBeginIterateEvent(n, 0);
        KsiEvent *nextEvent = ksiNodeGetNextEvent(n, 0);
        if(((env->currentStage==stageHalt)||(env->currentStage==stageSustain))&&!(nextEvent)){
                ksiNodePortIOClear(n->outputTypes[0]);
                ksiNodePortIOClear(n->outputTypes[1]);
                return;
        }
        for(int32_t i=0;i<bufsize;i++){
                if(nextEvent&&(nextEvent->timeStamp == n->e->timeStamp + i)){
                        if((env->currentStage==stageHalt||env->currentStage==stageRelease)&&(nextEvent->data.i==0)){
                                env->currentTime = 1;
                                env->currentStage = stageAttack;
                                //printf("attack\n");
                                env->stageDuration = round(ksiNodeGetInput(n,bufsize,1,i).f*n->e->framesPerSecond);
                                env->currentStageStartMod = env->currentMod;
                                env->currentBias = (M_E-env->currentStageStartMod)/(M_E-1);
                                //printf("%f",env->currentBias);
                        }
                        else if((env->currentStage!=stageHalt&&env->currentStage!=stageRelease)&&(nextEvent->data.i==1)){
                                env->currentTime = 1;
                                env->currentStage = stageRelease;
                                //printf("release\n");
                                env->stageDuration = round(ksiNodeGetInput(n,bufsize,4,i).f*n->e->framesPerSecond);
                                env->currentStageStartMod = env->currentMod;
                                env->currentBias = env->currentStageStartMod/(M_E*M_E*M_E-1);
                        }
                        nextEvent = ksiNodeGetNextEvent(n, 0);
                }
                switch(env->currentStage){
                case stageAttack:
                        env->currentMod = env->currentBias-(env->currentBias-env->currentStageStartMod)*expf(-env->currentTime/(float)env->stageDuration);
                        if(!(env->currentTime<env->stageDuration)){
                                env->currentStage = stageDecay;
                                //printf("decay\n");
                                env->currentStageStartMod=ksiNodeGetInput(n,bufsize,3,i).f;
                                env->stageDuration=round(ksiNodeGetInput(n,bufsize,2,i).f*n->e->framesPerSecond);
                                env->currentBias = (M_E*M_E*M_E*env->currentStageStartMod-1)/(M_E*M_E*M_E-1);
                                env->currentTime = 0;
                        }
                        break;
                case stageDecay:
                        env->currentMod = env->currentStageStartMod + (1.0f-env->currentStageStartMod)*expf(-env->currentTime/(float)env->stageDuration*3);
                        if(!(env->currentTime<env->stageDuration)){
                                env->currentStage=stageSustain;
                                //printf("sustain\n");
                        }
                        break;
                case stageRelease:
                        env->currentMod = (env->currentStageStartMod+env->currentBias)*(expf(-env->currentTime/(float)env->stageDuration*3)-1/(M_E*M_E*M_E));
                        if(!(env->currentTime<env->stageDuration)){
                                env->currentStage=stageHalt;
                                env->currentMod = 0;
                                //printf("halt\n");
                        }
                        break;
                }
                env->currentTime++;
                if(env->currentStage!=stageHalt){
                        n->outputBuffer[0].d[i].f=env->currentMod;
                        //printf("put %f \n",env->currentMod);
                        n->outputBuffer[1].d[i].i=0;
                }
                else{
                        n->outputBuffer[0].d[i].f=0.0;
                        n->outputBuffer[1].d[i].i=1;
                }
        }
        ksiNodePortIOSetDirty(n->outputTypes[0]);
        ksiNodePortIOSetDirty(n->outputTypes[1]);
}
