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
        float freq;
} plugin_env;
KsiError kscarletMidiSegEditCmd(KsiNode *n,const char *args,const char **pcli_err_str,int flag){
        plugin_env *env = (plugin_env *)n->args;
#define $freq env->freq
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
                "Usage:m[Notes Time Sequence ID]\n"
                "      p[Playlist Time Sequence ID]\n"
                "      P[Poly count]";
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
        env->current = NULL;
}
void kscarletMidiSegDestroy(KsiNode *n){
        free(n->args);
}
void kscarletMidiSeg(KsiNode *n){
        plugin_env *env = (plugin_env *)n->args;
        int32_t bufsize = n->e->framesPerBuffer;
        if(!env->current){
                if(!env->tree)
                        goto bypass;
                env->current = ksiRBTreeNextForKey(env->tree, n->e->timeStamp);
        }
        if(!env->current)
                goto bypass;
#define current_dt (env->current->key) - n->e->timeStamp
        int32_t localNextEvent = current_dt;
        if(localNextEvent>bufsize)
                goto bypass;
        ksiEventClearQueue(n->outputBuffer[1].e);
        for(int32_t i=0;i<bufsize;i++){
                while(localNextEvent == i){
                        $freq = 440.0f*powf(2, ((float)env->current->data.note.tone-69)/12);
                        ksiEventEnqueue(n->outputBuffer[1].e, (KsiData){.i = (env->current->data.note.velocity == 0)},n->e->timeStamp + i);
                        env->current = ksiRBTreeNext(env->tree,env->current);
                        if(env->current){
                                localNextEvent = current_dt;
                        }
                        else
                                localNextEvent = -1;
                }
                n->outputBuffer[0].d[i].f = $freq;
        }
        ksiNodePortIOSetDirty(n->outputTypes[1]);
        return;
bypass:
        n->outputBuffer[0].d[bufsize-1].f = $freq;
        ksiNodePortIOClear(n->outputTypes[1]);
        return;
}
