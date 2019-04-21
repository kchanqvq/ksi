#include "dagedit_commit.h"
#include "dagedit_kernels.h"
void *ksiMVCCCommitter(void *args){
        KsiEngine *e = args;
        while(1){
                ksiSemWait(&e->committingSem);
                ksiEnginePlayingLock(e);
                if(e->playing == ksiEnginePlaying){
                        if(atomic_load_explicit(&e->epoch,memory_order_consume)!=atomic_load_explicit(&e->audioEpoch,memory_order_consume)&&e->driver_env)
                                ksiSemWait(&e->migratedSem);
                }
                else{
                        atomic_store_explicit(&e->audioEpoch,(atomic_load_explicit(&e->epoch,memory_order_acquire)+1)%2,memory_order_relaxed);
                }
                ksiCondSignal(&e->committedCond);
                //printf("Commit\n");
                KsiDagEditCmd cmd = ksiSPSCCmdListDequeue(&e->syncCmds);
                while(cmd.cmd){
                        switch(cmd.cmd){
                        case ksiDagEditAddNode:{
                                void *extArgs;
                                if(cmd.data.add.extLen){
                                        extArgs = malloc(cmd.data.add.extLen);
                                        memcpy(extArgs, cmd.data.add.extArgs, cmd.data.add.extLen);
                                }
                                else{
                                        extArgs = NULL;
                                }
                                int32_t id;
                                KsiNode *n;
                                impl_ksiEngineAddNode(e, cmd.data.add.typeFlags, extArgs, 1, &id, &n);
                                n->args = cmd.data.add.args;
                                break;
                        }
                        case ksiDagEditMakeWire:
                                impl_ksiEngineMakeWire(e, cmd.data.topo.srcId, cmd.data.topo.srcPort, cmd.data.topo.desId, cmd.data.topo.desPort, cmd.data.topo.gain, 1);
                                break;
                        case ksiDagEditMakeBias:
                                impl_ksiEngineMakeBias(e, cmd.data.topo.desId, cmd.data.topo.desPort, cmd.data.topo.gain, 1);
                                break;
                        case ksiDagEditDetachNodes:
                                impl_ksiEngineDetachNodes(e, cmd.data.topo.srcId, cmd.data.topo.desId, 1);
                                break;
                        case ksiDagEditRemoveNode:{
                                KsiNode *n;
                                impl_ksiEngineRemoveNode(e, cmd.data.topo.srcId, &n, 1);
                                free(ksiEngineDestroyNode(e, n, 1));
                                break;
                        }
                        case ksiDagEditRemoveWire:
                                impl_ksiEngineRemoveWire(e, cmd.data.topo.srcId, cmd.data.topo.srcPort, cmd.data.topo.desId, cmd.data.topo.desPort, 1);
                                break;
                        case ksiDagEditSendEditingCmd:
                                impl_ksiEngineSendEditingCommand(e, cmd.data.cmd.id, cmd.data.cmd.cmd, NULL, 1);
                                free(cmd.data.cmd.cmd);
                                break;
                        default:
                                fputs("Invalid command in commiter command queue!\n", stderr);
                                abort();
                        }
                        cmd = ksiSPSCCmdListDequeue(&e->syncCmds);
                }
                //printf("Committed\n");
                ksiEnginePlayingUnlock(e);
                if(e->playing == ksiEngineFinalizing)
                        break;
        }
        return NULL;
}
