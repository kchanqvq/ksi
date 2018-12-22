#include "dagedit.h"
#include "rbtree.h"
#include <assert.h>
#include "inline_meta.h"
#include "util.h"
#include "data.h"
#define ksiEngineNodesDefaultCapacity 256

#define CHECK_ID(id,err) if(id>=e->nodes.size||!e->nodes.data[id])\
                return err
#define CHECK_SRC_PORT(node,port,err) if(!(port<node->outputCount))  \
                return err
#define CHECK_DES_PORT(node,port,err) if(!(port<node->inputCount))  \
                return err
#define CHECK_PARA(node,pid,err) if(!(pid<node->paramentCount))    \
                return err
void ksiEngineInit(KsiEngine *e,int32_t framesPerBuffer,int32_t framesPerSecond,int nprocs){
        e->framesPerBuffer = framesPerBuffer;
        e->framesPerSecond = framesPerSecond;
        e->outputBufferPointer = NULL;
        e->workers = (pthread_t *)malloc(sizeof(pthread_t)*nprocs);
        e->nprocs = nprocs;
        ksiVecInit(&e->nodes, ksiEngineNodesDefaultCapacity);
        ksiVecInit(&e->timeseqResources, ksiEngineNodesDefaultCapacity);
        queue_init(&e->tasks, nprocs+1);//NPROCS + MASTER
        queue_register(&e->tasks, &e->masterHandle, 0);
        ksiSemInit(&e->masterSem, 0, 0);
        e->playing = 0;
}
//Call ksiEngineDestroyChild before calling ksiEngineDestroy
void ksiEngineDestroy(KsiEngine *e){
        ksiVecDestroy(&e->nodes);

        ksiVecBeginIterate(&e->timeseqResources, i);
        KsiRBNode *n = (KsiRBNode *)i;
        ksiRBNodeDestroy(n);
        ksiVecEndIterate();

        ksiVecDestroy(&e->timeseqResources);
        queue_free(&e->tasks, &e->masterHandle);
        handle_free(&e->masterHandle);
        ksiSemDestroy(&e->masterSem);
        free(e->workers);
}
KsiEngine *ksiEngineDestroyChild(KsiEngine *e){
        ksiVecBeginIterate(&e->nodes, i);
        KsiNode *n = (KsiNode *)i;
        free(ksiNodeDestroy(n));
        ksiVecEndIterate();
        return e;
}
int32_t ksiEngineAddNode(KsiEngine *e,KsiNode *n){
        int32_t ret = ksiVecInsert(&e->nodes, n);
        n->id = ret;
        return ret;
}
static inline void refreshCopying(KsiEngine *e,KsiNode *n,int32_t port){
        if(!n->env.portEnv[port].mixer){
                n->env.internalBufferPtr[port] = NULL;
        }
        else if(!n->env.portEnv[port].mixer->next&&ksiDataIsUnit(n->env.portEnv[port].mixer->gain, n->inputTypes[port])){
                if(n->env.portEnv[port].buffer){
                        free(n->env.portEnv[port].buffer);
                        n->env.portEnv[port].buffer = NULL;
                }
                n->env.internalBufferPtr[port] = n->env.portEnv[port].mixer->src->outputBuffer + n->env.portEnv[port].mixer->srcPort*e->framesPerBuffer;
        }
        else{
                if(!n->env.portEnv[port].buffer){
                        n->env.portEnv[port].buffer = (KsiData *)malloc(sizeof(KsiData)*e->framesPerBuffer);
                }
                n->env.internalBufferPtr[port] = n->env.portEnv[port].buffer;
        }
}
KsiError ksiEngineMakeWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort,KsiData gain){
        CHECK_ID(srcId, ksiErrorSrcIdNotFound);
        CHECK_ID(desId, ksiErrorDesIdNotFound);
        KsiNode *src = (KsiNode *)e->nodes.data[srcId];
        KsiNode *des = (KsiNode *)e->nodes.data[desId];
        CHECK_SRC_PORT(src, srcPort, ksiErrorSrcPortNotFound);
        CHECK_DES_PORT(des, desPort, ksiErrorDesPortNotFound);
        KsiMixerEnvEntry *me = des->env.portEnv[desPort].mixer;
        int8_t type = des->inputTypes[desPort]&ksiNodePortTypeMask;
        if(type!=src->outputTypes[srcPort]&ksiNodePortTypeMask)
                return ksiErrorType;
        while(me){
                if(me->src == src&&me->srcPort==srcPort&&me->inputPort==desPort){
                        ksiDataIncrease(&me->gain, gain, type);
                        refreshCopying(e, des, desPort);
                        return ksiErrorNone;
                }
                me=me->next;
        }
        if(!ksiVecIdlistSearch(src->successors, desId)){
                ksiVecIdlistPush(&src->successors, desId);
                des->depNum++;
        }
        KsiMixerEnvEntry *newEntry = (KsiMixerEnvEntry *)malloc(sizeof(KsiMixerEnvEntry));
        newEntry->src=src;
        newEntry->srcPort=srcPort;
        newEntry->gain = gain;
        newEntry->next = des->env.portEnv[desPort];
        des->env.portEnv[desPort].mixer = newEntry;
        refreshCopying(e, des, desPort);
        return ksiErrorNone;
}
KsiError ksiEngineMakeBias(KsiEngine *e,int32_t desId,int32_t desPort,KsiData bias){
        CHECK_ID(desId, ksiErrorIdNotFound);
        KsiNode *des = (KsiNode *)e->nodes.data[desId];
        CHECK_DES_PORT(des, desPort, ksiErrorPortNotFound);
        KsiMixerEnvEntry *me = des->env.portEnv[desPort].mixer;
        int8_t type = des->inputTypes[desPort];
        while(me){
                if(me->src == NULL&&me->inputPort==desPort){
                        ksiDataIncrease(&me->gain, bias, type);
                        refreshCopying(e, des, desPort);
                        return ksiErrorNone;
                }
                me=me->next;
        }
        assert((des->type&ksiNodeTypeInputMask)==ksiNodeTypeInputMixer);
        KsiMixerEnvEntry *newEntry = (KsiMixerEnvEntry *)malloc(sizeof(KsiMixerEnvEntry));
        newEntry->src=NULL;
        newEntry->srcPort=0;
        newEntry->gain = bias;
        newEntry->next = des->env.portEnv[desPort].mixer;
        newEntry->inputPort = desPort;
        des->env.portEnv[desPort].mixer = newEntry;
        refreshCopying(e, des, desPort);
        return ksiErrorNone;
}
KsiError ksiEngineGetInputType(KsiEngine *e,int32_t id,int32_t port,int8_t *t){
        CHECK_ID(id, ksiErrorIdNotFound);
        KsiNode *n = (KsiNode *)e->nodes.data[id];
        CHECK_DES_PORT(n, port, ksiErrorPortNotFound);
        *t = n->inputTypes[port];
        return ksiErrorNone;
}
KsiError ksiEngineGetParamentType(KsiEngine *e,int32_t id,int32_t pid,int8_t *t){
        CHECK_ID(id, ksiErrorIdNotFound);
        KsiNode *n = (KsiNode *)e->nodes.data[id];
        CHECK_PARA(n, pid, ksiErrorParamentNotFound);
        *t = n->paramentTypes[pid];
        return ksiErrorNone;
}
KsiError ksiEngineSetParament(KsiEngine *e,int32_t id,int32_t pid,KsiData d){
        CHECK_ID(id, ksiErrorIdNotFound);
        KsiNode *n = (KsiNode *)e->nodes.data[id];
        CHECK_PARA(n, pid, ksiErrorParamentNotFound);
        n->paraments[pid] = d;
        return ksiErrorNone;
}
KsiError ksiEngineRemoveNode(KsiEngine *e,int32_t id,KsiNode **nodeRet){
        CHECK_ID(id, ksiErrorIdNotFound);
        KsiNode *n = (KsiNode *)e->nodes.data[id];
        *nodeRet = n;
        ksiVecDelete(&e->nodes, id);
        KsiVecIdlistNode *sn = n->successors;
        while(sn){
                KsiNode *s = e->nodes.data[sn->loc];
                s->depNum --;
                for(int32_t i=0;i<s->inputCount;i++){
                        ksiVecListDelete(s->env.portEnv[i].mixer, ->src == n,, KsiMixerEnvEntry);
                        refreshCopying(e, s, i);
                }
                sn=sn->next;
        }
        KsiVecNodelistNode *precs = NULL;
#define PUT_TO_LIST(precIds,p) if(!(p->type&ksiNodeTypeScratchPredecessor)){ \
        ksiVecNodelistPush(&precs, p);\
        p->type|=ksiNodeTypeScratchPredecessor;\
}
        for(int32_t i=0;i<n->inputCount;i++){
                KsiMixerEnvEntry *me = n->env.mixer.predecessors;
                while(me){
                        KsiNode *p = me->src;
                        PUT_TO_LIST(precIds, p);
                        me=me->next;
                }
        }
        while(precs){
                KsiNode *p = ksiVecNodelistPop(&precs);
                p->type&=~ksiNodeTypeScratchPredecessor;
                ksiVecListDelete(p->successors, ->loc==n->id, break,KsiVecIdlistNode);
        }
        switch(n->type&ksiNodeTypeOutputMask){
        case ksiNodeTypeOutputNormal:
                break;
        case ksiNodeTypeOutputFinal:
                *e->outputBufferPointer = NULL;
                break;
        }
        return ksiErrorNone;
}
KsiError ksiEngineRemoveWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort){
        CHECK_ID(srcId, ksiErrorSrcIdNotFound);
        CHECK_ID(desId, ksiErrorDesIdNotFound);
        KsiNode *src = (KsiNode *)e->nodes.data[srcId];
        KsiNode *des = (KsiNode *)e->nodes.data[desId];
        CHECK_SRC_PORT(src, srcPort, ksiErrorSrcPortNotFound);
        CHECK_DES_PORT(des, desPort, ksiErrorDesPortNotFound);
        KsiMixerEnvEntry *me = des->env.portEnv[desPort].mixer;
        KsiMixerEnvEntry **preloc = &des->env.portEnv[desPort].mixer;
        int foundEntries = 0;
        int deleted = 0;
        while(me){
                if(me->src==src){
                        if(me->srcPort == srcPort){
                                preloc = &(me->next);
                                free(me);
                                me = *preloc;
                                deleted = 1;
                                if(foundEntries)
                                        break;
                        }
                        else{
                                foundEntries++;
                                if(deleted)
                                        break;
                                preloc = &(me->next);
                                me = *preloc;
                        }
                }
                else{
                        preloc = &(me->next);
                        me = *preloc;
                }
        }
        if(!foundEntries&&deleted){
                ksiVecListDelete(src->successors,->loc==des->id , break, KsiVecIdlistNode);
                des->depNum --;
        }
        if(deleted){
                refreshCopying(e, des, desPort);
                return ksiErrorNone;
        }
        else
                return ksiErrorWireNotFound;
}
KsiError ksiEngineDetachNodes(KsiEngine *e,int32_t srcId,int32_t desId){
        CHECK_ID(srcId, ksiErrorSrcIdNotFound);
        CHECK_ID(desId, ksiErrorDesIdNotFound);
        KsiNode *src = (KsiNode *)e->nodes.data[srcId];
        KsiNode *des = (KsiNode *)e->nodes.data[desId];
        des->depNum --;
        for(int32_t i=0;i<des->inputCount;i++){
                ksiVecListDelete(des->env.portEnv[i].mixer, ->src==src, , KsiMixerEnvEntry);
                refreshCopying(e, des, i);
        }
        ksiVecListDelete(src->successors, ->loc == desId, break, KsiVecIdlistNode);
        return ksiErrorNone;
}
KsiNode *ksiNodeInit(KsiNode *n,int32_t typeFlags,KsiEngine *e,void *args){
        atomic_init(&n->depCounter,0);
        n->successors = NULL;
        n->type = typeFlags;
        n->args = args;
        n->depNum = 0;
        n->e = e;
#define INLINE_INIT_MF(__,id,name,prop,in,out,...) __ _N()(id,name,LISTCOUNT in,LISTCOUNT out,_E prop)
#define INLINE_INIT(_) _E(INLINE_LIST(INLINE_INIT_MF,_,INLINE_INPORT_END))
        switch(ksiNodeTypeInlineId(typeFlags)){
#define DEF_INIT(id,name,ni,no,res,dm,...) case id: \
                n->inputCount = ni;      \
                n->inputTypes = CAT(name,InPorts);    \
                n->outputCount = no;\
                n->outputTypes = CAT(name,OutPorts);    \
                break;
                INLINE_INIT(DEF_INIT);
        }
        n->inputCache = (KsiData *)malloc(sizeof(KsiData)*n->inputCount);
        n->outputCache = (KsiData *)malloc(sizeof(KsiData)*n->outputCount);
        memset(n->inputCache, 0, sizeof(KsiData)*n->inputCount);
        memset(n->outputCache, 0, sizeof(KsiData)*n->outputCount);
#undef DEF_INPORT
        n->env.portEnv = (KsiPortEnv *)malloc(sizeof(KsiPortEnv)*n->inputCount);
        n->env.internalBufferPtr = (KsiData **)malloc(sizeof(KsiData *)*n->inputCount);
        for(int32_t i=0;i<n->inputCount;i++){
                n->env.portEnv[i].mixer = NULL;
                n->env.internalBufferPtr[i] = NULL;
        }
        switch(typeFlags&ksiNodeTypeOutputMask){
        case ksiNodeTypeOutputNormal:
                n->outputBuffer = (KsiData *)malloc(sizeof(KsiData)*e->framesPerBuffer*n->outputCount);
                break;
        case ksiNodeTypeOutputFinal:
                e->outputBufferPointer = (void **)&n->outputBuffer;
                break;
        }
        switch(ksiNodeTypeInlineId(n->type)){
#define INLINE_CASE(id,name,reqrs,dm,...)              \
                case id:                            \
                        CONDITIONAL(dm,CAT(name,Init)(n));  \
                        break;
                INLINE_PROPERTY(INLINE_CASE);
#undef INLINE_CASE
        default:
                break;
        }
        return n;
}
KsiNode *ksiNodeDestroy(KsiNode *n){
        switch(n->type&ksiNodeTypeOutputMask){
        case ksiNodeTypeOutputNormal:
                free(n->outputBuffer);
                break;
        case ksiNodeTypeOutputFinal:
                break;
        }
        free(n->inputCache);
        free(n->outputCache);
        ksiVecListDestroy(n->successors, KsiVecIdlistNode);
        for(int32_t i=0;i<n->inputCount;i++){
                ksiVecListDestroy(n->env.portEnv[i].mixer);
                if(n->env.portEnv[i].buffer)
                        free(n->env.portEnv[i].buffer);
        }
        free(n->env.internalBufferPtr);
        switch(ksiNodeTypeInlineId(n->type)){
#define INLINE_CASE(id,name,reqrs,dm,...)     \
                case id:                            \
                        CONDITIONAL(dm,CAT(name,Destroy)(n));   \
                        free(n->outputTypes);   \
                        free(n->inputTypes);\
                        break;
                INLINE_PROPERTY(INLINE_CASE);
#undef INLINE_CASE
        default:
                break;
        }

        return n;
}
void ksiNodeSerialize(KsiNode *n,FILE *fp){
        fprintf(fp, "dummy");
}
void ksiEngineSerialize(KsiEngine *e,FILE *fp){
        fprintf(fp, "%"PRId32",%"PRId32",%zu\n", e->framesPerBuffer,e->framesPerSecond,e->timeStamp);
        ksiVecBeginIterate(&e->nodes, i);
        KsiNode *n = (KsiNode *)i;
        ksiNodeSerialize(n, fp);
        fputc('\n',fp);
        ksiVecEndIterate();
}
