#include "dagedit.h"
#include "rbtree.h"
#include <assert.h>
#include "inline_meta.h"
#include "util.h"
#include "data.h"
#include "engine.h"
#include "dag.h"
#include "mvcc_utils.h"
#include "dagedit_kernels.h"

#define ksiEngineNodesDefaultCapacity 256
#define ksiEngineCmdQueueSize 256

KsiEngine *ksiEngineDestroyChild(KsiEngine *e){
        ksiCondWait(&e->committedCond);
        ksiEnginePlayingLock(e);
        ksiVecBeginIterate(&e->nodes[0], i);
        KsiNode *n = (KsiNode *)i;
        free(ksiEngineDestroyNode(e, n, 0));
        ksiVecEndIterate();
        ksiVecBeginIterate(&e->nodes[1], i);
        KsiNode *n = (KsiNode *)i;
        free(ksiEngineDestroyNode(e, n, 1));
        ksiVecEndIterate();
        ksiEnginePlayingUnlock(e);
        return e;
}
KsiError ksiEngineAddNode(KsiEngine *e,int32_t typeFlags,int32_t *idRet,void *extArgs,size_t len){
        ksiCondWait(&e->committedCond);
        ksiEnginePlayingLock(e);
        KsiNode *n;
        KsiError ret = impl_ksiEngineAddNode(e, typeFlags, extArgs, 0, idRet, &n);
        if(!ret){
                ksiSPSCCmdListEnqueue(&e->syncCmds, (KsiDagEditCmd){.cmd = ksiDagEditAddNode, .data =
                                        {.add = {
                                                        typeFlags,
                                                        extArgs,
                                                        len,
                                                        n->args
                                                }}});
        }
        ksiEnginePlayingUnlock(e);
        return ret;
}
KsiError ksiEngineMakeWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort,KsiData gain){
        ksiCondWait(&e->committedCond);
        ksiEnginePlayingLock(e);
        KsiError ret = impl_ksiEngineMakeWire(e, srcId, srcPort, desId, desPort, gain, 0);
        if(!ret){
                ksiSPSCCmdListEnqueue(&e->syncCmds, (KsiDagEditCmd){.cmd = ksiDagEditMakeWire, .data =
                                        {.topo = {
                                                        srcId,
                                                        srcPort,
                                                        desId,
                                                        desPort,
                                                        gain,
                                                }}});
        }
        ksiEnginePlayingUnlock(e);
        return ret;
}
KsiError ksiEngineMakeBias(KsiEngine *e,int32_t desId,int32_t desPort,KsiData bias){
        ksiCondWait(&e->committedCond);
        ksiEnginePlayingLock(e);
        KsiError ret = impl_ksiEngineMakeBias(e, desId, desPort, bias, 0);
        if(!ret){
                ksiSPSCCmdListEnqueue(&e->syncCmds, (KsiDagEditCmd){.cmd = ksiDagEditMakeBias, .data =
                                        {.topo = {
                                                        .desId = desId,
                                                        .desPort = desPort,
                                                        .gain = bias,
                                                }}});
        }
        ksiEnginePlayingUnlock(e);
        return ret;
}
KsiError ksiEngineGetInputType(KsiEngine *e,int32_t id,int32_t port,int8_t *t){
        ksiCondWait(&e->committedCond);
        ksiEnginePlayingLock(e);
        hotList(nlist);
        CHECK_VEC(id, *nlist, ksiErrorIdNotFound,ksiEnginePlayingUnlock(e));
        KsiNode *n = nlist->data[id];
        CHECK_DES_PORT(n, port, ksiErrorPortNotFound,ksiEnginePlayingUnlock(e));
        *t = n->inputTypes[port];
        ksiEnginePlayingUnlock(e);
        return ksiErrorNone;
}
KsiError ksiEngineDetachNodes(KsiEngine *e,int32_t srcId,int32_t desId){
        ksiCondWait(&e->committedCond);
        ksiEnginePlayingLock(e);
        KsiError ret = impl_ksiEngineDetachNodes(e, srcId, desId, 0);
        if(!ret){
                ksiSPSCCmdListEnqueue(&e->syncCmds, (KsiDagEditCmd){.cmd = ksiDagEditDetachNodes, .data =
                                        {.topo = {
                                                        .srcId = srcId,
                                                        .desId = desId,
                                                }}});
        }
        ksiEnginePlayingUnlock(e);
        return ret;
}
KsiError ksiEngineRemoveNode(KsiEngine *e,int32_t id){
        ksiCondWait(&e->committedCond);
        ksiEnginePlayingLock(e);
        KsiNode *nodeRet;
        KsiError ret = impl_ksiEngineRemoveNode(e, id, &nodeRet, 0);
        if(!ret){
                free(ksiEngineDestroyNode(e, nodeRet, 0));
                ksiSPSCCmdListEnqueue(&e->syncCmds, (KsiDagEditCmd){.cmd = ksiDagEditRemoveNode, .data =
                                        {.topo = {
                                                        .srcId = id,
                                                }}});
        }
        ksiEnginePlayingUnlock(e);
        return ret;
}
KsiError ksiEngineRemoveWire(KsiEngine *e,int32_t srcId,int32_t srcPort,int32_t desId,int32_t desPort){
        ksiCondWait(&e->committedCond);
        ksiEnginePlayingLock(e);
        KsiError ret = impl_ksiEngineRemoveWire(e, srcId, srcPort, desId, desPort, 0);
        if(!ret){
                ksiSPSCCmdListEnqueue(&e->syncCmds, (KsiDagEditCmd){.cmd = ksiDagEditRemoveWire, .data =
                                        {.topo = {
                                                        .srcId = srcId,
                                                        .srcPort = srcPort,
                                                        .desId = desId,
                                                        .desPort = desPort,
                                                }}});
        }
        ksiEnginePlayingUnlock(e);
        return ret;

}
KsiNode *ksiNodeInit(KsiNode *n,int32_t typeFlags,KsiEngine *e,void *args){

        return n;
}
void ksiNodeSerialize(KsiNode *n,FILE *fp){
        fprintf(fp, "dummy");
}
void ksiEngineSerialize(KsiEngine *e,FILE *fp){
        fprintf(fp, "dummy");
}
KsiError ksiEngineSendEditingCommand(KsiEngine *e,int32_t id,const char *args,const char **pcli_err_str,size_t len){
        ksiCondWait(&e->committedCond);
        ksiEnginePlayingLock(e);
        KsiError err = impl_ksiEngineSendEditingCommand(e, id, args, pcli_err_str, 0);
        char *cpArgs = malloc(len);
        memcpy(cpArgs, args, len);
        if(!err){
                ksiSPSCCmdListEnqueue(&e->syncCmds, (KsiDagEditCmd){.cmd = ksiDagEditSendEditingCmd, .data =
                                        {.cmd = {
                                                        id,
                                                        cpArgs,
                                                }}});
        }
        ksiEnginePlayingUnlock(e);
        return ksiErrorNone;
}
KsiError ksiEngineCommit(KsiEngine *e){
        if(!e->nprocs)
                return ksiErrorUninitialized;
        if(e->syncCmds.head == atomic_load_explicit(&e->syncCmds.tail, memory_order_relaxed)){
                return ksiErrorNone;
        }
        ksiCondWait(&e->committedCond);
        ksiEnginePlayingLock(e);
        ksiCondSetWait(&e->committedCond);
        int new_epoch = (atomic_load_explicit(&e->epoch,memory_order_consume)+1)%2;
        //printf("new epoch %d\n", new_epoch);
        atomic_store_explicit(&e->epoch,
                              new_epoch,memory_order_release);
        ksiSemPost(&e->committingSem);
        ksiEnginePlayingUnlock(e);
        return ksiErrorNone;
}
void ksiNodeChangeInputPortCount(KsiNode *n,int32_t port_count,int8_t* newTypes,int flag){
        impl_ksiNodeChangeInputPortCount(n, port_count, newTypes, flag);
}
void ksiNodeChangeOutputPortCount(KsiNode *n,int32_t port_count,int8_t* newTypes,int flag){
        impl_ksiNodeChangeOutputPortCount(n, port_count, newTypes, flag);
}
