#include "dagedit.h"
#include "rbtree.h"
#include <assert.h>
#include "inline_meta.h"
#include "util.h"
#define ksiEngineNodesDefaultCapacity 256

#define CHECK_ID(id,err) if(id>=e->nodes.size||!e->nodes.data[id])\
                return err
#define CHECK_SRC_PORT(node,port,err) if(!(port<node->outputCount))  \
                return err
#define CHECK_DES_PORT(node,port,err) if(!(port<node->inputCount))  \
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
KsiError ksiEngineMakeAdjustableWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort,float gain){
        CHECK_ID(srcId, ksiErrorSrcIdNotFound);
        CHECK_ID(desId, ksiErrorDesIdNotFound);
        KsiNode *src = (KsiNode *)e->nodes.data[srcId];
        KsiNode *des = (KsiNode *)e->nodes.data[desId];
        CHECK_SRC_PORT(src, srcPort, ksiErrorSrcPortNotFound);
        CHECK_DES_PORT(des, desPort, ksiErrorDesPortNotFound);
        if((des->type&ksiNodeTypeInputMask)!=ksiNodeTypeInputMixer){
                return ksiErrorDesMixReq;
        }
        KsiMixerEnvEntry *me = des->env.mixer.predecessors;
        while(me){
                if(me->src == src&&me->srcPort==srcPort&&me->inputPort==desPort){
                        me->gain+=gain;
                        return ksiErrorNone;
                }
                me=me->next;
        }
        if(!ksiVecIdlistSearch(src->successors, desId)){
                ksiVecIdlistPush(&src->successors, desId);
                des->depNum++;
        }
        assert((des->type&ksiNodeTypeInputMask)==ksiNodeTypeInputMixer);
        assert((src->type&ksiNodeTypeOutputMask)!=ksiNodeTypeOutputFinal);
        KsiMixerEnvEntry *newEntry = (KsiMixerEnvEntry *)malloc(sizeof(KsiMixerEnvEntry));
        newEntry->src=src;
        newEntry->srcPort=srcPort;
        newEntry->gain = gain;
        newEntry->next = des->env.mixer.predecessors;
        newEntry->inputPort = desPort;
        des->env.mixer.predecessors = newEntry;
        return ksiErrorNone;
}
KsiError ksiEngineMakeAdjustableBias(KsiEngine *e,int32_t desId,int32_t desPort,float bias){
        CHECK_ID(desId, ksiErrorIdNotFound);
        KsiNode *des = (KsiNode *)e->nodes.data[desId];
        CHECK_DES_PORT(des, desPort, ksiErrorPortNotFound);
        if((des->type&ksiNodeTypeInputMask)!=ksiNodeTypeInputMixer){
                return ksiErrorDesMixReq;
        }
        KsiMixerEnvEntry *me = des->env.mixer.predecessors;
        while(me){
                if(me->src == NULL&&me->srcPort==srcPort&&me->inputPort==desPort){
                        me->gain+=gain;
                        return ksiErrorNone;
                }
                me=me->next;
        }
        if(!ksiVecIdlistSearch(src->successors, desId)){
                ksiVecIdlistPush(&src->successors, desId);
                des->depNum++;
        }
        assert((des->type&ksiNodeTypeInputMask)==ksiNodeTypeInputMixer);
        assert((src->type&ksiNodeTypeOutputMask)!=ksiNodeTypeOutputFinal);
        KsiMixerEnvEntry *newEntry = (KsiMixerEnvEntry *)malloc(sizeof(KsiMixerEnvEntry));
        newEntry->src=src;
        newEntry->srcPort=srcPort;
        newEntry->gain = gain;
        newEntry->next = des->env.mixer.predecessors;
        newEntry->inputPort = desPort;
        des->env.mixer.predecessors = newEntry;
        return ksiErrorNone;
}
KsiError ksiEngineMakeDirectWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort){
        CHECK_ID(srcId, ksiErrorSrcIdNotFound);
        CHECK_ID(desId, ksiErrorDesIdNotFound);
        KsiNode *src = (KsiNode *)e->nodes.data[srcId];
        KsiNode *des = (KsiNode *)e->nodes.data[desId];
        CHECK_SRC_PORT(src, srcPort, ksiErrorSrcPortNotFound);
        CHECK_DES_PORT(des, desPort, ksiErrorDesPortNotFound);
        if((des->type&ksiNodeTypeInputMask)!=ksiNodeTypeInputFixed)
                return ksiErrorDesFixReq;
        if(des->env.fixed.predecessors[desPort])
                return ksiErrorDesPortOccupied;
        if(!ksiVecIdlistSearch(src->successors,desId)){
                ksiVecIdlistPush(&src->successors, desId);
                des->depNum++;
        }
        assert((des->type&ksiNodeTypeInputMask)==ksiNodeTypeInputFixed);
        assert((src->type&ksiNodeTypeOutputMask)!=ksiNodeTypeOutputFinal);
        des->env.fixed.predecessorBuffers[desPort]=src->outputBuffer+srcPort*e->framesPerBuffer;
        des->env.fixed.predecessors[desPort]=src;
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
                switch(s->type&ksiNodeTypeInputMask){
                case ksiNodeTypeInputFixed:
                        for(int32_t i=0;i<s->inputCount;i++){
                                if(s->env.fixed.predecessors[i]==n){
                                        s->env.fixed.predecessorBuffers[i]=NULL;
                                        s->env.fixed.predecessors[i]=NULL;
                                }
                        }
                        break;
                case ksiNodeTypeInputMixer:{
                        ksiVecListDelete(s->env.mixer.predecessors, ->src == n,,KsiMixerEnvEntry);
                        break;
                }
                }
                sn=sn->next;
        }
        KsiVecNodelistNode *precs = NULL;
#define PUT_TO_LIST(precIds,p) if(!(p->type&ksiNodeTypeScratchPredecessor)){ \
        ksiVecNodelistPush(&precs, p);\
        p->type|=ksiNodeTypeScratchPredecessor;\
}
        switch(n->type&ksiNodeTypeInputMask){
        case ksiNodeTypeInputFixed:
                for(int32_t i=0;i<n->inputCount;i++){
                        KsiNode *p = n->env.fixed.predecessors[i];
                        if(!p)
                                continue;
                        PUT_TO_LIST(precIds, p);
                }
                break;
        case ksiNodeTypeInputMixer:{
                KsiMixerEnvEntry *me = n->env.mixer.predecessors;
                while(me){
                        KsiNode *p = me->src;
                        PUT_TO_LIST(precIds, p);
                        me=me->next;
                }
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
        KsiMixerEnvEntry *me = des->env.mixer.predecessors;
        KsiMixerEnvEntry *pre = NULL;
        int foundEntries = 0;
        int deleted = 0;
        switch(des->type&ksiNodeTypeInputMask){
        case ksiNodeTypeInputFixed:
                for(int32_t i=0;i<des->inputCount;i++){
                        if(des->env.fixed.predecessors[i]==src){
                                if(des->env.fixed.predecessorBuffers[i]==src->outputBuffer+e->framesPerBuffer*srcPort){
                                        des->env.fixed.predecessors[i]=NULL;
                                        des->env.fixed.predecessorBuffers[i]=NULL;
                                        deleted = 1;
                                        if(foundEntries)
                                                break;
                                }
                                else{
                                        foundEntries++;
                                        if(deleted)
                                                break;
                                }
                        }
                }
                break;
        case ksiNodeTypeInputMixer:
                while(me){
                        if(me->src==src){
                                if(me->srcPort == srcPort && me->inputPort == desPort){
                                        if(pre){
                                                pre->next=me->next;
                                                free(me);
                                                me = pre->next;
                                        }
                                        else{
                                                KsiMixerEnvEntry *d = me;
                                                me = me->next;
                                                des->env.mixer.predecessors = me;
                                                free(d);
                                        }
                                        deleted = 1;
                                        if(foundEntries)
                                                break;
                                }
                                else{
                                        foundEntries++;
                                        if(deleted)
                                                break;
                                        pre = me;
                                        me = me->next;
                                }
                        }
                        else{
                                pre = me;
                                me = me->next;
                        }
                }
                break;
        }
        if(!foundEntries&&deleted){
                ksiVecListDelete(src->successors,->loc==des->id , break, KsiVecIdlistNode);
                des->depNum --;
        }
        if(deleted)
                return ksiErrorNone;
        else
                return ksiErrorWireNotFound;
}
KsiError ksiEngineDetachNodes(KsiEngine *e,int32_t srcId,int32_t desId){
        CHECK_ID(srcId, ksiErrorSrcIdNotFound);
        CHECK_ID(desId, ksiErrorDesIdNotFound);
        KsiNode *src = (KsiNode *)e->nodes.data[srcId];
        KsiNode *des = (KsiNode *)e->nodes.data[desId];
        des->depNum --;
        switch(des->type&ksiNodeTypeInputMask){
        case ksiNodeTypeInputFixed:
                for(int32_t i=0;i<des->inputCount;i++){
                        if(des->env.fixed.predecessors[i]==src){
                                des->env.fixed.predecessors[i]=NULL;
                                des->env.fixed.predecessorBuffers[i]=NULL;
                        }
                }
                break;
        case ksiNodeTypeInputMixer:
                ksiVecListDelete(des->env.mixer.predecessors, ->src==src, , KsiMixerEnvEntry);
                break;
        }
        ksiVecListDelete(src->successors, ->loc == desId, break, KsiVecIdlistNode);
        return ksiErrorNone;
}
KsiNode *ksiNodeInit(KsiNode *n,KsiNodeFunc f,int32_t inputCount,int32_t outputCount,int32_t typeFlags,KsiEngine *e,void *args,int32_t gatingOutput){
        atomic_init(&n->depCounter,0);
        n->successors = NULL;
        n->outputCount = outputCount;
        n->type = typeFlags;
        n->args = args;
        n->depNum = 0;
        n->e = e;
        n->f = f;
        n->inputCount = inputCount;
        n->gatingOutput = gatingOutput;
        n->outputTypes = (int8_t *)malloc(sizeof(int8_t)*outputCount);
        if(!(gatingOutput<0)){
                n->outputTypes[gatingOutput] = ksiNodeOutputTypeGating;
        }
        switch(typeFlags&ksiNodeTypeInputMask){
        case ksiNodeTypeInputFixed:
                n->env.fixed.predecessorBuffers = (KsiData **)malloc(sizeof(KsiData *)*inputCount);
                n->env.fixed.predecessors = (KsiNode **)malloc(sizeof(KsiNode *)*inputCount);
                memset(n->env.fixed.predecessors, 0, sizeof(KsiNode *)*inputCount);
                break;
        case ksiNodeTypeInputMixer:
                n->env.mixer.predecessors = NULL;
                n->env.mixer.internalBuffer = (KsiData *)malloc(sizeof(KsiData)*e->framesPerBuffer*inputCount);
                n->env.mixer.internalBufferPtr = (KsiData **)malloc(sizeof(KsiData *)*inputCount);
                for(int32_t i=0;i<inputCount;i++){
                        n->env.mixer.internalBufferPtr[i] = n->env.mixer.internalBuffer + e->framesPerBuffer*i;
                }
        }
        switch(typeFlags&ksiNodeTypeOutputMask){
        case ksiNodeTypeOutputNormal:
                n->outputBuffer = (KsiData *)malloc(sizeof(KsiData)*e->framesPerBuffer*outputCount);
                break;
        case ksiNodeTypeOutputFinal:
                e->outputBufferPointer = (void **)&n->outputBuffer;
                break;
        }
        switch(ksiNodeTypeInlineId(n->type)){
#define INLINE_CASE(id,name,reqrs,dm)                 \
                case id:                            \
                        if(dm){                  \
                                CAT(name,Init)(n); \
                        }                           \
                        break;
                INLINE_LIST(INLINE_CASE);
#undef INLINE_CASE
        default:
                break;
        }
        return n;
}
KsiNode *ksiNodeDestroy(KsiNode *n){
        free(n->outputTypes);
        switch(n->type&ksiNodeTypeOutputMask){
        case ksiNodeTypeOutputNormal:
                free(n->outputBuffer);
                break;
        case ksiNodeTypeOutputFinal:
                break;
        }
        ksiVecListDestroy(n->successors, KsiVecIdlistNode);
        switch(n->type&ksiNodeTypeInputMask){
        case ksiNodeTypeInputFixed:
                free(n->env.fixed.predecessorBuffers);
                free(n->env.fixed.predecessors);
                break;
        case ksiNodeTypeInputMixer:
                ksiVecListDestroy(n->env.mixer.predecessors, KsiMixerEnvEntry);
                free(n->env.mixer.internalBuffer);
                free(n->env.mixer.internalBufferPtr);
        }
        switch(ksiNodeTypeInlineId(n->type)){
#define INLINE_CASE(id,name,reqrs,dm)               \
                case id:                            \
                        if(dm){                     \
                                CAT(name,Destroy)(n);  \
                        }                           \
                        break;
                INLINE_LIST(INLINE_CASE);
#undef INLINE_CASE
        default:
                break;
        }

        return n;
}
void ksiNodeSerialize(KsiNode *n,FILE *fp){
        fprintf(fp, "%"PRId32, n->id);
        int32_t t;
        switch(t = n->type&ksiNodeTypeInputMask){
        case ksiNodeTypeInputFixed:
                fprintf(fp, " f%"PRId32, n->inputCount);
                break;
        case ksiNodeTypeInputMixer:
                fprintf(fp, " m%"PRId32, n->inputCount);
                break;
        default:
                fprintf(fp, " %"PRIx32"?", t);
                break;
        }
        switch(t= n->type&ksiNodeTypeOutputMask){
        case ksiNodeTypeOutputNormal:
                fputs(">n",fp);
                break;
        case ksiNodeTypeOutputFinal:
                fputs(">f",fp);
                break;
        default:
                fprintf(fp, ">%"PRIx32"?", t);
        }
        fputs(" {", fp);
        switch(n->type&ksiNodeTypeInputMask){
        case ksiNodeTypeInputFixed:
                for(int32_t i=0;i<n->inputCount;i++){
                        KsiNode* p = n->env.fixed.predecessors[i];
                        if(p){
                                fprintf(fp, "%"PRId32":%"PRId32, p->id,(int32_t)((n->env.fixed.predecessorBuffers[i]-p->outputBuffer)/n->e->framesPerBuffer));
                        }
                        else{
                                fputc(' ', fp);
                        }
                        fputc(',',fp);
                }
                break;
        case ksiNodeTypeInputMixer:
        {
                KsiMixerEnvEntry *me = n->env.mixer.predecessors;
                while(me){
                        fprintf(fp, "%"PRId32":%"PRId32"x%f>%"PRId32",", me->src->id,me->srcPort,me->gain,me->inputPort);
                        me = me->next;
                }
        }
        break;
        default:
                fputs("?",fp);
        }
        fputs("}>{",fp);
        KsiVecIdlistNode *sn = n->successors;
        while(sn){
                fprintf(fp, "%"PRId32",", sn->loc);
                sn=sn->next;
        }
        fputc('}', fp);
}
void ksiEngineSerialize(KsiEngine *e,FILE *fp){
        fprintf(fp, "%"PRId32",%"PRId32",%zu\n", e->framesPerBuffer,e->framesPerSecond,e->timeStamp);
        ksiVecBeginIterate(&e->nodes, i);
        KsiNode *n = (KsiNode *)i;
        ksiNodeSerialize(n, fp);
        fputc('\n',fp);
        ksiVecEndIterate();
}
