//output 0: freq
//output 1: gating
//No inputs. Resources are passed by editing command.
#include "midiseg.h"
#include <math.h>
#include "rbtree.h"
#include "resource.h"
#include <stdio.h>
typedef struct{
        //Data
        KsiRBTree *tree;
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
KsiError kscarletMidiSegEditCmd(KsiNode *n,const char *args,const char **pcli_err_str){
        plugin_env *env = (plugin_env *)n->args;
        switch(args[0]){
        case 'm':{
                args ++;
                int mid;
                int readcount = sscanf(args,"%d",&mid);
                if(readcount!=1)
                        goto syn_err;
                CHECK_VEC(mid, n->e->timeseqResources, ksiErrorTimeSeqIdNotFound);
                env->tree = n->e->timeseqResources.data[mid];
                break;
        }
        case 'p':
                break;
        case 'P':
                break;
        default:
                goto syn_err;
        }
        return ksiErrorNone;
syn_err:
        *pcli_err_str = "Invalid argument.\n"
                "Usage:em[Notes Time Sequence ID]\n"
                "      ep[Playlist Time Sequence ID]\n"
                "      eP[Poly count]";
        return ksiErrorSyntax;
}
void kscarletMidiSegInit(KsiNode *n){
        n->args=malloc(sizeof(plugin_env));
        plugin_env *env = (plugin_env *)n->args;
        env->tree=NULL;
        env->current=NULL;
}
void kscarletMidiSegReset(KsiNode *n){
        plugin_env *env = (plugin_env *)n->args;
        $gating = 1;
        env->current = NULL;
}
void kscarletMidiSegDestroy(KsiNode *n){
        free(n->args);
}
void kscarletMidiSeg(KsiNode *n,KsiData **inputBuffers,KsiData *outputBuffer){
        plugin_env *env = (plugin_env *)n->args;
        if(!env->current){
                env->current = ksiRBTreeNextForKey(env->tree, n->e->timeStamp);
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
                        env->current = ksiRBTreeNext(env->tree,env->current);
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
