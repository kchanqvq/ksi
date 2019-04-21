// Actual implementation for topological edit on dag
#include "mvcc_utils.h"
#include "inline_meta.h"
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
// Maintain env[port].d->buffer and env[port].d->internalBufferPtr for signal input ports
static inline void impl_delete_signal_env(KsiEngine *e, KsiNode *n, int32_t i,int flag){
        ksiVecListDestroy(n->env[i].d->mixer,KsiMixerEnvEntry);
        if(n->env[i].d->buffer)
                ksiMVCCDeferredFree(e, n->env[i].d->buffer, flag);
        free(n->env[i].d);
}
static inline void impl_delete_event_env(KsiEngine *e, KsiNode *n,int32_t i,int flag){
        ksiVecListDestroy(n->env[i].e->ees,KsiEventEnvEntry);
        free(n->env[i].e);
}
static inline void impl_delete_env(KsiEngine *e, KsiNode *n,int32_t i,int flag){
        if(n->inputTypes[i]&ksiNodePortTypeEventFlag){
                impl_delete_event_env(e, n, i, flag);
        }
        else{
                impl_delete_signal_env(e, n, i, flag);
        }
}
static inline void impl_delete_output(KsiEngine *e, KsiNode *n,int32_t i,int flag){
        if(n->outputTypes[i]&ksiNodePortTypeEventFlag){
                if(flag){
                        ksiEventClearQueue(n->outputBuffer[i].e);
                }
                ksiMVCCDeferredFree(e, n->outputBuffer[i].e, flag);
        }
        else{
                ksiMVCCDeferredFree(e, n->outputBuffer[i].d, flag);
        }
}
static inline void refreshCopying(KsiEngine *e, KsiNode *n, int32_t port, int flag){
        if(n->inputTypes[port]&ksiNodePortTypeEventFlag){
                return;
        }
        else{
                if(!n->env[port].d->mixer){
                        n->env[port].d->internalBufferPtr = NULL;
                }
                else if(!n->env[port].d->mixer->next&&ksiDataIsUnit(n->env[port].d->mixer->gain, n->inputTypes[port])){
                        if(n->env[port].d->buffer){
                                ksiMVCCDeferredFree(e, n->env[port].d->buffer, flag);
                                n->env[port].d->buffer = NULL;
                        }
                        if(n->env[port].d->mixer->src){ // Not a bias term
                                n->env[port].d->internalBufferPtr = n->env[port].d->mixer->src->outputBuffer[n->env[port].d->mixer->srcPort].d;
                        }
                }
                else{
                        if(!n->env[port].d->buffer){
                                n->env[port].d->buffer = ksiMVCCMonitoredMalloc(e, sizeof(KsiData)*e->framesPerBuffer, flag);
                        }
                        n->env[port].d->internalBufferPtr = n->env[port].d->buffer;
                }
        }
}
// Set nlist variable to the node list of the dag version to be updated
#define hotList(nlist) int epoch = atomic_load_explicit(&e->epoch, memory_order_relaxed); \
        int hotEpoch = (1+epoch)%2;                                     \
        KsiVec* nlist = &(e->nodes[hotEpoch]);//

static inline KsiError impl_ksiEngineAddNode(KsiEngine *e,int32_t typeFlags,void *args,int flag,int32_t *idRet,KsiNode **nRet){
        CHECK_INITIALIED(e);
        hotList(nlist);
        if((typeFlags&ksiNodeTypeOutputMask) == ksiNodeTypeOutputFinal && e->finalNode[(1+epoch)%2])
                return ksiErrorMultipleFinal;
        //printf("Work on flag %d epoch %d\n", flag, epoch);
        KsiNode *n = ksiMalloc(sizeof(KsiNode)); // Ice cream
        *nRet = n;
        atomic_init(&n->depCounter,0);
        n->successors = NULL;
        n->type = typeFlags;
        n->extArgs = args;
        n->depNum = 0;
        n->e = e;
#define INLINE_INIT_MF(__,id,name,prop,in,out,...) __ _N()(id,name,LISTNONNIL in,LISTNONNIL out,LISTCOUNT in,LISTCOUNT out,_E prop)
#define INLINE_INIT(_) _E(INLINE_LIST(INLINE_INIT_MF,_,INLINE_INPORT_END))
        switch(ksiNodeTypeInlineId(typeFlags)){
#define DEF_INIT(id,name,nni,nno,ni,no,res,dm,...) case id:             \
                n->inputCount = ni;                                     \
                BRANCH(nni,n->inputTypes = CAT(name,InPorts),n->inputTypes=NULL); \
                n->outputCount = no;                                    \
                BRANCH(nno,n->outputTypes = CAT(name,OutPorts),n->outputTypes=NULL); \
                break;
                INLINE_INIT(DEF_INIT);
        default:
                return ksiErrorNoPlugin;
        }
#undef DEF_INPORT
        n->env = ksiMalloc(sizeof(KsiEnvPtr)*n->inputCount);// Ice cream
        for(int32_t i=0;i<n->inputCount;i++){
                if(n->inputTypes[i]&ksiNodePortTypeEventFlag){
                        n->env[i].e = ksiMalloc(sizeof(KsiEventEnv));
                        n->env[i].e->ees = NULL;
                }
                else{
                        n->env[i].d = ksiMalloc(sizeof(KsiSignalEnv));
                        n->env[i].d->buffer = NULL;
                        n->env[i].d->internalBufferPtr = NULL;
                        n->env[i].d->mixer = NULL;
                }
        }
        n->outputBuffer = ksiMalloc(sizeof(KsiOutputPtr)*n->outputCount);
        switch(typeFlags&ksiNodeTypeOutputMask){
        case ksiNodeTypeOutputNormal:
                for(int32_t i=0;i<n->outputCount;i++){
                        if(n->outputTypes[i]&ksiNodePortTypeEventFlag){
                                n->outputBuffer[i].e = ksiMVCCMonitoredMalloc(e, sizeof(KsiEventQueue), flag);
                                if(!flag){
                                        n->outputBuffer[i].e->head = NULL;
                                        n->outputBuffer[i].e->tail = NULL;
                                }
                                //This initialization is not necessary
                                //Just do it for clarity
                        }
                        else{
                                n->outputBuffer[i].d = ksiMVCCMonitoredMalloc(e, (sizeof(KsiData) * e->framesPerBuffer), flag);
                        }
                }
                break;
        case ksiNodeTypeOutputFinal:
                e->finalNode[(1+epoch)%2] = n;
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
        CHECK_INITIALIED(e);
        hotList(nlist);
        CHECK_VEC(srcId, *nlist, ksiErrorSrcIdNotFound);
        CHECK_VEC(desId, *nlist, ksiErrorDesIdNotFound);
        KsiNode *src = nlist->data[srcId];
        KsiNode *des = nlist->data[desId];
        CHECK_SRC_PORT(src, srcPort, ksiErrorSrcPortNotFound);
        CHECK_DES_PORT(des, desPort, ksiErrorDesPortNotFound);
        int8_t type = des->inputTypes[desPort]&ksiNodePortTypeMask;
        if(type!=(src->outputTypes[srcPort]&ksiNodePortTypeMask))
                return ksiErrorType;
        if(!ksiVecIdlistSearch(src->successors, desId)){
                ksiVecIdlistPush(&src->successors, desId); // Yes, there is allocation, but in this case we don't want the 2 version to share it. (later referenced as "ice cream")
                des->depNum++;
        }
        if(type&ksiNodePortTypeEventFlag){
                KsiEventEnvEntry *ee = des->env[desPort].e->ees;
                while(ee){
                        if(ee->src == src && ee->srcPort == srcPort){
                                return ksiErrorAlreadyWire;
                        }
                        ee = ee->next;
                }
                KsiEventEnvEntry *newEntry = ksiMalloc(sizeof *newEntry);
                newEntry->src = src;
                newEntry->srcPort = srcPort;
                newEntry->next = des->env[desPort].e->ees;
                des->env[desPort].e->ees = newEntry;
        }
        else{
                KsiMixerEnvEntry *me = des->env[desPort].d->mixer;
                while(me){
                        if(me->src == src&&me->srcPort==srcPort){
                                debug_check_node(e, me->src, hotEpoch);
                                me->gain = gain;
                                refreshCopying(e, des, desPort, flag);
                                return ksiErrorNone;
                        }
                        me=me->next;
                }
                KsiMixerEnvEntry *newEntry = ksiMalloc(sizeof *newEntry);
                newEntry->src=src;
                newEntry->srcPort=srcPort;
                newEntry->gain = gain;
                newEntry->next = des->env[desPort].d->mixer;
                des->env[desPort].d->mixer = newEntry;
                refreshCopying(e, des, desPort, flag);
        }
        return ksiErrorNone;
}
static inline KsiError impl_ksiEngineMakeBias(KsiEngine *e,int32_t desId,int32_t desPort,KsiData bias,int flag){
        CHECK_INITIALIED(e);
        hotList(nlist);
        CHECK_VEC(desId, *nlist, ksiErrorIdNotFound);
        KsiNode *des = nlist->data[desId];
        CHECK_DES_PORT(des, desPort, ksiErrorPortNotFound);
        int8_t type = des->inputTypes[desPort];
        if(type&ksiNodePortTypeEventFlag){
                return ksiErrorNeedSignal;
        }
        KsiMixerEnvEntry *me = des->env[desPort].d->mixer;
        while(me){
                if(me->src == NULL){
                        me->gain = bias;
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
        newEntry->next = des->env[desPort].d->mixer;
        des->env[desPort].d->mixer = newEntry;
        refreshCopying(e, des, desPort, flag);
        return ksiErrorNone;
}
static inline void delete_src(KsiEngine *e, KsiNode *des,int32_t i/*the port*/,KsiNode *src,int flag){
        if(des->inputTypes[i]&ksiNodePortTypeEventFlag){
                ksiVecListDelete(des->env[i].e->ees, ->src==src, , KsiEventEnvEntry);// Ice cream
        }
        else{
                ksiVecListDelete(des->env[i].d->mixer, ->src==src, , KsiMixerEnvEntry);// Ice cream
                refreshCopying(e, des, i, flag);
        }
}
static inline KsiError impl_ksiEngineDetachNodes(KsiEngine *e,int32_t srcId,int32_t desId,int flag){
        CHECK_INITIALIED(e);
        hotList(nlist);
        CHECK_VEC(srcId, *nlist, ksiErrorSrcIdNotFound);
        CHECK_VEC(desId, *nlist, ksiErrorDesIdNotFound);
        KsiNode *src = nlist->data[srcId];
        KsiNode *des = nlist->data[desId];
        des->depNum --;
        for(int32_t i=0;i<des->inputCount;i++){
                delete_src(e, des, i, src, flag);
        }
        ksiVecListDelete(src->successors, ->loc == desId, break, KsiVecIdlistNode);// Ice cream
        return ksiErrorNone;
}
static inline KsiError impl_ksiEngineRemoveNode(KsiEngine *e,int32_t id,KsiNode **nodeRet,int flag){
        CHECK_INITIALIED(e);
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
                        delete_src(e, s, i, n, flag);
                }
                sn=sn->next;
        }
        KsiVecNodelistNode *precs = NULL;
#define PUT_TO_LIST(precs,p) if(!(p->type&ksiNodeTypeScratchPredecessor)){ \
                ksiVecNodelistPush(&precs, p);                          \
                p->type|=ksiNodeTypeScratchPredecessor;                 \
        } // Ice cream
        for(int32_t i=0;i<n->inputCount;i++){
#define REG_PRECS(me)                                           \
                while(me){                                      \
                        KsiNode *p = me->src;                   \
                        debug_check_node(e, me->src, hotEpoch); \
                        if(p)                                   \
                                PUT_TO_LIST(precs, p);          \
                        me=me->next;                            \
                }
                if(n->inputTypes[i]&ksiNodePortTypeEventFlag){
                        KsiEventEnvEntry *ee = n->env[i].e->ees;
                        REG_PRECS(ee);
                }
                else{
                        KsiMixerEnvEntry *me = n->env[i].d->mixer;
                        REG_PRECS(me);
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
                e->finalNode[(1+epoch)%2] = NULL;
                break;
        }
        return ksiErrorNone;
}
static inline KsiError impl_ksiEngineRemoveWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort,int flag){
        CHECK_INITIALIED(e);
        hotList(nlist);
        CHECK_VEC(srcId, *nlist, ksiErrorSrcIdNotFound);
        CHECK_VEC(desId, *nlist, ksiErrorDesIdNotFound);
        KsiNode *src = nlist->data[srcId];
        KsiNode *des = nlist->data[desId];
        CHECK_SRC_PORT(src, srcPort, ksiErrorSrcPortNotFound);
        CHECK_DES_PORT(des, desPort, ksiErrorDesPortNotFound);
        KsiMixerEnvEntry **preloc = (des->inputTypes[desPort]&ksiNodePortTypeEventFlag)?
                &des->env[desPort].e->ees:
                &des->env[desPort].d->mixer;
        KsiMixerEnvEntry *me = *preloc;
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
        if(deleted&&!(des->inputTypes[desPort]&ksiNodePortTypeEventFlag)){
                refreshCopying(e, des, desPort, flag);
                return ksiErrorNone;
        }
        else
                return ksiErrorWireNotFound;
}
static inline KsiError impl_ksiEngineSendEditingCommand(KsiEngine *e,int32_t id,const char *args,const char **pcli_err_str,int flag){
        CHECK_INITIALIED(e);
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
        ksiVecListDestroy(n->successors, KsiVecIdlistNode);
        for(int32_t i=0;i<n->inputCount;i++){
                impl_delete_env(e, n, i, flag);
        }
        free(n->env);
        if((n->type&ksiNodeTypeOutputMask) == ksiNodeTypeOutputNormal){
                for(int32_t i=0;i<n->outputCount;i++){
                        impl_delete_output(e, n, i, flag);
                }
        }
        free(n->outputBuffer);
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
static inline void impl_ksiNodeChangeInputPortCount(KsiNode *n,int32_t port_count,int8_t* newTypes,int flag){
        if(!(n->type & ksiNodeTypeDynamicInputType)){
                int8_t* new_types = ksiMalloc(sizeof(int8_t) * port_count);
                memcpy(new_types, n->inputTypes, sizeof(int8_t) * MIN(n->inputCount,port_count));
                n->inputTypes = new_types;
                n->type |= ksiNodeTypeDynamicInputType;
        }
        else if (port_count > n->inputCount)
                n->inputTypes = ksiRealloc(n->inputTypes, sizeof(int8_t) * port_count);
        if(port_count < n->inputCount){
                for(int32_t i = port_count; i < n->inputCount; i++){
                        impl_delete_env(n->e, n, i, flag);
                }
        }
        n->env = ksiRealloc(n->env, sizeof(KsiEnvPtr) * port_count);
        if(port_count > n->inputCount){
                for(int32_t i = n->inputCount; i < port_count; i++){
                        if(newTypes[i-n->inputCount]&ksiNodePortTypeEventFlag){
                                n->env[i].e = ksiMalloc(sizeof(KsiEventEnv));
                                n->env[i].e->ees = NULL;
                        }
                        else{
                                n->env[i].d = ksiMalloc(sizeof(KsiSignalEnv));
                                KsiSignalEnv *se = n->env[i].d;
                                se->mixer = NULL;
                                se->buffer = NULL;
                                se->internalBufferPtr = NULL;
                                n->inputTypes[i] = newTypes[i-n->inputCount];
                        }
                }
        }
        n->inputCount = port_count;
        if (port_count < n->inputCount)
                n->inputTypes = ksiRealloc(n->inputTypes, sizeof(int8_t) * port_count);
}
static inline void impl_ksiNodeChangeOutputPortCount(KsiNode *n,int32_t port_count,int8_t* newTypes,int flag){
        if(!(n->type & ksiNodeTypeDynamicOutputType)){
                int8_t* new_types = ksiMalloc(sizeof(int8_t) * port_count);
                memcpy(new_types, n->outputTypes, sizeof(int8_t) * MIN(n->outputCount,port_count));
                n->outputTypes = new_types;
                n->type |= ksiNodeTypeDynamicOutputType;
        }
        else if (port_count > n->outputCount)
                n->outputTypes = ksiRealloc(n->outputTypes, sizeof(int8_t) * port_count);
        if((n->type&ksiNodeTypeOutputMask) == ksiNodeTypeOutputNormal){
                if(port_count < n->outputCount){
                        for(int32_t i = port_count; i < n->outputCount; i++){
                                impl_delete_output(n->e, n, i, flag);
                        }
                }
        }
        n->outputBuffer = ksiRealloc(n->outputBuffer, sizeof(KsiOutputPtr) * port_count);
        if(port_count > n->outputCount){
                for(int32_t i = n->outputCount; i < port_count; i++){
                        n->outputTypes[i] = newTypes[i-n->outputCount];
                        if((n->type&ksiNodeTypeOutputMask) == ksiNodeTypeOutputNormal){
                                if(n->outputTypes[i]&ksiNodePortTypeEventFlag){
                                        n->outputBuffer[i].e = ksiMVCCMonitoredMalloc(n->e, sizeof(KsiEventQueue), flag);
                                        if(!flag){
                                                n->outputBuffer[i].e->head = NULL;
                                                n->outputBuffer[i].e->tail = NULL;
                                        }
                                }
                                else{
                                        n->outputBuffer[i].d = ksiMVCCMonitoredMalloc(n->e, sizeof(KsiData)*n->e->framesPerBuffer, flag);
                                }
                        }
                }
        }
        n->outputCount = port_count;
        if (port_count < n->outputCount)
                n->outputTypes = ksiRealloc(n->outputTypes, sizeof(int8_t) * port_count);
}
