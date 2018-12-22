#include "dag.h"
//output 0: freq
//output 1: gating
//input 0: resource ID
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
} plugin_env;
#define $freq (n->outputCache[0].f)
#define $gating (n->outputCache[1].i)
void kscarletMidiSegInit(KsiNode *n){
        n->args=malloc(sizeof(plugin_env));
        plugin_env *env = (plugin_env *)n->args;
        env->current=NULL;
}
void kscarletMidiSegReset(KsiNode *n){
        plugin_env *env = (plugin_env *)n->args;
        env->root = n->e->timeseqResources.data[n->inputCache[0].i];
        env->offset = 0;
        env->segLength = 44100;
        env->cycleEnd = 44100;
        $gating = 1;
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
                        $gating = (env->current->data.note.velocity == 0);
                        $freq = 440.0f*powf(2, ((float)env->current->data.note.tone-69)/12);
                        env->current = ksiRBNodeNext(env->current);
                        if(env->current){
                                localNextEvent = env->current->key - n->e->timeStamp;
                        }
                        else
                                localNextEvent = -1;
                }
                outputBuffer[i+bufsize].i = $gating;
                outputBuffer[i].f = $freq;
        }
        ksiNodePortIOSetDirty(n->outputTypes[0]);
        ksiNodePortIOSetDirty(n->outputTypes[1]);
        return;
bypass:
        ksiNodePortIOClear(n->outputTypes[0]);
        ksiNodePortIOClear(n->outputTypes[1]);
        return;
}
