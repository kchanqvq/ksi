#include "dag.h"
//output 0: freq
//output 1: gating
//parament 0: resource ID
#include "midiseg.h"
#include <math.h>
#include "rbtree.h"
#include "resource.h"
#include <stdio.h>
typedef struct{
        //Data
        KsiRBNode *root;
        KsiRBNode *current;

        //Region position info
        int32_t offset;
        int32_t segLength;
        int32_t cycleEnd;

        //Local variables
        int32_t nextEvent;//Global timestamp
        int32_t currentGating;
        float currentFreq;
} plugin_env;
void kscarletMidiSegInit(KsiNode *n){
        n->args=malloc(sizeof(plugin_env));
        plugin_env *env = (plugin_env *)n->args;
        env->current=NULL;
}
void kscarletMidiSegReset(KsiNode *n){
        plugin_env *env = (plugin_env *)n->args;
        env->root = n->e->timeseqResources.data[n->paraments[0].i];
        env->offset = 0;
        env->segLength = 44100;
        env->cycleEnd = 44100;
        env->currentGating = 1;
}
void kscarletMidiSegDestroy(KsiNode *n){
        free(n->args);
}
void kscarletMidiSeg(KsiNode *n,KsiData **inputBuffers,KsiData *outputBuffer){
        plugin_env *env = (plugin_env *)n->args;
        if(!env->current){
                env->current = ksiRBNodeNextForKey(&env->root, n->e->timeStamp);
        }
        int32_t bufsize = n->e->framesPerBuffer;
        if(!env->current)
                goto bypass;
        int32_t localNextEvent = env->current->key - n->e->timeStamp;
        if(localNextEvent>bufsize)
                goto bypass;
        for(int32_t i=0;i<bufsize;i++){
                while(localNextEvent == i){
                        env->currentGating = (env->current->data.note.velocity == 0);
                        env->currentFreq = 440.0f*powf(2, ((float)env->current->data.note.tone-69)/12);
                        env->current = ksiRBNodeNext(env->current);
                        if(env->current){
                                localNextEvent = env->current->key - n->e->timeStamp;
                        }
                        else
                                localNextEvent = -1;
                }
                outputBuffer[i+bufsize].i = env->currentGating;
                outputBuffer[i].f = env->currentFreq;
        }
        return;
bypass:
        if(env->currentGating){
                outputBuffer[bufsize].i = -1;
        }
        else{
                for(int32_t i=0;i<bufsize;i++){
                        outputBuffer[i+bufsize].i = 0;
                        outputBuffer[i].f = env->currentFreq;
                }
        }
        return;
}
