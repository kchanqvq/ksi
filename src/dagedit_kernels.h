// Actual implementation for topological edit on dag
#include "mvcc_utils.h"
#include "inline_meta.h"
#include "linear_builtins.h"
// flag = 0 for editing
// flag = 1 for commiting
#define CHECK_SRC_PORT(node,port,err,...) if(!(port<node->outputCount)){ \
                __VA_ARGS__;/*hook for cleanup*/                        \
                return err;                                             \
        }
#define CHECK_DES_PORT(node,port,err,...) if(!(port<node->inputCount)){ \
                __VA_ARGS__;/*hook for cleanup*/                        \
        return err;                                                     \
        }

static inline void refreshCopying(KsiEngine *e, KsiNode *n, int32_t port, int flag){
        if(!n->env.portEnv[port].mixer){
                n->env.internalBufferPtr[port] = NULL;
        }
        else if(!n->env.portEnv[port].mixer->next&&ksiDataIsUnit(n->env.portEnv[port].mixer->gain, n->inputTypes[port])){
                if(n->env.portEnv[port].buffer){
                        free(n->env.portEnv[port].buffer);
                        n->env.portEnv[port].buffer = NULL;
                }
                if(n->env.portEnv[port].mixer->src){ // Not a bias term
                        n->env.internalBufferPtr[port] = n->env.portEnv[port].mixer->src->outputBuffer + n->env.portEnv[port].mixer->srcPort*e->framesPerBuffer;
                }
        }
        else{
                if(!n->env.portEnv[port].buffer){
                        n->env.portEnv[port].buffer = ksiMVCCMonitoredMalloc(e, sizeof(KsiData)*e->framesPerBuffer, flag);
                }
                n->env.internalBufferPtr[port] = n->env.portEnv[port].buffer;
        }
}
// Set nlist variable to the node list of the dag version to be updated
#define hotList(nlist) int epoch = atomic_load_explicit(&e->epoch, memory_order_relaxed); \
        int hotEpoch = (1+epoch)%2;\
        KsiVec* nlist = &(e->nodes[hotEpoch]);//

static inline KsiError impl_ksiEngineAddNode(KsiEngine *e,int32_t typeFlags,void *args,int flag,int32_t *idRet,KsiNode **nRet){
        hotList(nlist);
        KsiNode *n = ksiMalloc(sizeof(KsiNode)); // Ice cream
        *nRet = n;
        atomic_init(&n->depCounter,0);
        n->successors = NULL;
        n->type = typeFlags;
        n->extArgs = args;
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
        default:
                return ksiErrorNoPlugin;
        }
        n->inputCache = ksiMVCCMonitoredMalloc(e, sizeof(KsiData)*n->inputCount, flag);
        n->outputCache = ksiMVCCMonitoredMalloc(e, sizeof(KsiData)*n->outputCount, flag);
        if(!flag){
                memset(n->inputCache, 0, sizeof(KsiData)*n->inputCount);
                memset(n->outputCache, 0, sizeof(KsiData)*n->outputCount);
        }
#undef DEF_INPORT
        n->env.portEnv = (KsiPortEnv *)ksiMalloc(sizeof(KsiPortEnv)*n->inputCount);// Ice cream
        n->env.internalBufferPtr = (KsiData **)ksiMalloc(sizeof(KsiData *)*n->inputCount);
        for(int32_t i=0;i<n->inputCount;i++){
                n->env.portEnv[i].mixer = NULL;
                n->env.portEnv[i].buffer = NULL;
                n->env.internalBufferPtr[i] = NULL;
        }
        switch(typeFlags&ksiNodeTypeOutputMask){
        case ksiNodeTypeOutputNormal:
                n->outputBuffer = (KsiData *)ksiMVCCMonitoredMalloc(e, sizeof(KsiData)*e->framesPerBuffer*n->outputCount, flag);
                break;
        case ksiNodeTypeOutputFinal:
                e->outputBufferPointer[(1+epoch)%2] = (void **)&n->outputBuffer;
                break;
        }
        if(!flag){
                switch(ksiNodeTypeInlineId(n->type)){
#define INLINE_CASE(id,name,reqrs,dm,...)                           \
                        case id:                                    \
                                CONDITIONAL(dm,CAT(name,Init)(n));  \
                                break;
                        INLINE_PROPERTY(INLINE_CASE);
#undef INLINE_CASE
                default:
                        break;
                }
        }
        int32_t ret = ksiVecInsert(nlist, n);
        n->id = ret;
        *idRet = ret;
        return ksiErrorNone;
}
static inline KsiError impl_ksiEngineMakeWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort,KsiData gain,int flag){
        hotList(nlist);
        CHECK_VEC(srcId, *nlist, ksiErrorSrcIdNotFound);
        CHECK_VEC(desId, *nlist, ksiErrorDesIdNotFound);
        KsiNode *src = nlist->data[srcId];
        KsiNode *des = nlist->data[desId];
        CHECK_SRC_PORT(src, srcPort, ksiErrorSrcPortNotFound);
        CHECK_DES_PORT(des, desPort, ksiErrorDesPortNotFound);
        KsiMixerEnvEntry *me = des->env.portEnv[desPort].mixer;
        int8_t type = des->inputTypes[desPort]&ksiNodePortTypeMask;
        if(type!=(src->outputTypes[srcPort]&ksiNodePortTypeMask))
                return ksiErrorType;
        while(me){
                if(me->src == src&&me->srcPort==srcPort){
                        debug_check_node(e, me->src, hotEpoch);
                        ksiDataIncrease(&me->gain, gain, type);
                        refreshCopying(e, des, desPort, flag);
                        return ksiErrorNone;
                }
                me=me->next;
        }
        if(!ksiVecIdlistSearch(src->successors, desId)){
                ksiVecIdlistPush(&src->successors, desId); // Yes, there is allocation, but in this case we don't want the 2 version to share it. (later referenced as "ice cream")
                des->depNum++;
        }
        KsiMixerEnvEntry *newEntry = ksiMalloc(sizeof *newEntry);
        newEntry->src=src;
        newEntry->srcPort=srcPort;
        newEntry->gain = gain;
        newEntry->next = des->env.portEnv[desPort].mixer;
        des->env.portEnv[desPort].mixer = newEntry;
        refreshCopying(e, des, desPort, flag);
        return ksiErrorNone;
}
static inline KsiError impl_ksiEngineMakeBias(KsiEngine *e,int32_t desId,int32_t desPort,KsiData bias,int flag){
        hotList(nlist);
        CHECK_VEC(desId, *nlist, ksiErrorIdNotFound);
        KsiNode *des = nlist->data[desId];
        CHECK_DES_PORT(des, desPort, ksiErrorPortNotFound);
        KsiMixerEnvEntry *me = des->env.portEnv[desPort].mixer;
        int8_t type = des->inputTypes[desPort];
        while(me){
                if(me->src == NULL){
                        ksiDataIncrease(&me->gain, bias, type);
                        refreshCopying(e, des, desPort, flag);
                        return ksiErrorNone;
                }
                debug_check_node(e, me->src, hotEpoch);
                me=me->next;
        }
        KsiMixerEnvEntry *newEntry = ksiMalloc(sizeof *newEntry);
        newEntry->src=NULL;
        newEntry->srcPort=0;
        newEntry->gain = bias;
        newEntry->next = des->env.portEnv[desPort].mixer;
        des->env.portEnv[desPort].mixer = newEntry;
        refreshCopying(e, des, desPort, flag);
        return ksiErrorNone;
}
static inline KsiError impl_ksiEngineDetachNodes(KsiEngine *e,int32_t srcId,int32_t desId,int flag){
        hotList(nlist);
        CHECK_VEC(srcId, *nlist, ksiErrorSrcIdNotFound);
        CHECK_VEC(desId, *nlist, ksiErrorDesIdNotFound);
        KsiNode *src = nlist->data[srcId];
        KsiNode *des = nlist->data[desId];
        des->depNum --;
        for(int32_t i=0;i<des->inputCount;i++){
                ksiVecListDelete(des->env.portEnv[i].mixer, ->src==src, , KsiMixerEnvEntry);// Ice cream
                refreshCopying(e, des, i, flag);
        }
        ksiVecListDelete(src->successors, ->loc == desId, break, KsiVecIdlistNode);// Ice cream
        return ksiErrorNone;
}
static inline KsiError impl_ksiEngineRemoveNode(KsiEngine *e,int32_t id,KsiNode **nodeRet,int flag){
        hotList(nlist);
        CHECK_VEC(id, *nlist, ksiErrorIdNotFound);
        KsiNode *n = nlist->data[id];
        *nodeRet = n;
        ksiVecDelete(nlist, id);// Ice cream
        KsiVecIdlistNode *sn = n->successors;
        while(sn){
                KsiNode *s = nlist->data[sn->loc];
                s->depNum --;
                for(int32_t i=0;i<s->inputCount;i++){
                        ksiVecListDelete(s->env.portEnv[i].mixer, ->src == n,, KsiMixerEnvEntry);
                        refreshCopying(e, s, i, flag);
                }
                sn=sn->next;
        }
        KsiVecNodelistNode *precs = NULL;
#define PUT_TO_LIST(precs,p) if(!(p->type&ksiNodeTypeScratchPredecessor)){ \
        ksiVecNodelistPush(&precs, p);\
        p->type|=ksiNodeTypeScratchPredecessor;\
        } // Ice cream
        for(int32_t i=0;i<n->inputCount;i++){
                KsiMixerEnvEntry *me = n->env.portEnv[i].mixer;
                while(me){
                        KsiNode *p = me->src;
                        debug_check_node(e, me->src, hotEpoch);
                        if(p)
                                PUT_TO_LIST(precs, p);
                        me=me->next;
                }
        }
        while(precs){
                KsiNode *p = ksiVecNodelistPop(&precs);
                p->type&=~ksiNodeTypeScratchPredecessor;
                ksiVecListDelete(p->successors, ->loc==n->id, break,KsiVecIdlistNode); // Ice cream
        }
        switch(n->type&ksiNodeTypeOutputMask){
        case ksiNodeTypeOutputNormal:
                break;
        case ksiNodeTypeOutputFinal:
                *e->outputBufferPointer[(1+epoch)%2] = NULL;
                break;
        }
        return ksiErrorNone;
}
static inline KsiError impl_ksiEngineRemoveWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort,int flag){
        hotList(nlist);
        CHECK_VEC(srcId, *nlist, ksiErrorSrcIdNotFound);
        CHECK_VEC(desId, *nlist, ksiErrorDesIdNotFound);
        KsiNode *src = nlist->data[srcId];
        KsiNode *des = nlist->data[desId];
        CHECK_SRC_PORT(src, srcPort, ksiErrorSrcPortNotFound);
        CHECK_DES_PORT(des, desPort, ksiErrorDesPortNotFound);
        KsiMixerEnvEntry *me = des->env.portEnv[desPort].mixer;
        KsiMixerEnvEntry **preloc = &des->env.portEnv[desPort].mixer;
        int foundEntries = 0;
        int deleted = 0;
        while(me){
                if(me->src==src){
                        debug_check_node(e, me->src, hotEpoch);
                        if(me->srcPort == srcPort){
                                *preloc = me->next;
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
                ksiVecListDelete(src->successors,->loc==des->id , break, KsiVecIdlistNode);// Ice cream
                des->depNum --;
        }
        if(deleted){
                refreshCopying(e, des, desPort, flag);
                return ksiErrorNone;
        }
        else
                return ksiErrorWireNotFound;
}
static inline KsiError impl_ksiEngineSendEditingCommand(KsiEngine *e,int32_t id,const char *args,const char **pcli_err_str,int flag){
        hotList(nlist);
        CHECK_VEC(id, *nlist, ksiErrorIdNotFound);
        KsiNode *n = nlist->data[id];
        switch(ksiNodeTypeInlineId(n->type)){
#define INLINE_CASE(id,name,reqrs,dm,ecmd,...)                          \
                case id:                                                \
                        return BRANCH(ecmd,CAT(name,EditCmd)(n,args,pcli_err_str,flag),ksiErrorNoEditCmd);
                INLINE_PROPERTY(INLINE_CASE);
#undef INLINE_CASE
        default:
                break;
        }
        return ksiErrorNone;
}
static inline KsiNode *ksiEngineDestroyNode(KsiEngine *e, KsiNode *n,int flag){
        switch(n->type&ksiNodeTypeOutputMask){
        case ksiNodeTypeOutputNormal:
                ksiMVCCDeferredFree(e, n->outputBuffer, flag);
                break;
        case ksiNodeTypeOutputFinal:
                break;
        }
        ksiMVCCDeferredFree(e, n->inputCache, flag);
        ksiMVCCDeferredFree(e, n->outputCache, flag);
        ksiVecListDestroy(n->successors, KsiVecIdlistNode);
        for(int32_t i=0;i<n->inputCount;i++){
                ksiVecListDestroy(n->env.portEnv[i].mixer,KsiMixerEnvEntry);
                if(n->env.portEnv[i].buffer)
                        ksiMVCCDeferredFree(e, n->env.portEnv[i].buffer, flag);
        }
        free(n->env.portEnv);
        free(n->env.internalBufferPtr);
        if(flag){
                switch(ksiNodeTypeInlineId(n->type)){
#define INLINE_CASE(id,name,reqrs,dm,...)                               \
                        case id:                                        \
                                CONDITIONAL(dm,CAT(name,Destroy)(n));   \
                                break;
                        INLINE_PROPERTY(INLINE_CASE);
#undef INLINE_CASE
                default:
                        break;
                }
        }
        free(n->extArgs);
        if(n->type & ksiNodeTypeDynamicInputType)
                free(n->inputTypes);
        if(n->type & ksiNodeTypeDynamicOutputType)
                free(n->outputTypes);
        return n;
}
