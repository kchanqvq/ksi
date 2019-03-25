#ifndef __cmd_queue_h__
#define __cmd_queue_h__
#include "spsc_queue.h"
#include "data.h"
#include "dag.h"
#include <inttypes.h>
typedef enum{
        ksiDagEditInvalid,
        ksiDagEditAddNode,
        ksiDagEditMakeWire,
        ksiDagEditMakeBias,
        ksiDagEditDetachNodes,
        ksiDagEditRemoveNode,
        ksiDagEditRemoveWire,
        ksiDagEditSendEditingCmd,
} KsiDagEditOpcode;
// Using the same struct type and pass by value will cause unnecessary memory consumption and copying overhead, however it should be minor.
// Currently just keep it simple.
typedef struct _KsiDagEditCmd{
        int cmd;
        union {
                struct {
                        int32_t srcId;
                        int32_t srcPort;
                        int32_t desId;
                        int32_t desPort;
                        KsiData gain;
                } topo;
                struct {
                        int32_t typeFlags;
                        void *extArgs;
                        size_t extLen;
                        void *args;
                } add;
                struct {
                        int32_t id;
                        char *cmd;
                } cmd;
        } data;
} KsiDagEditCmd;
ksiSPSCDeclareList(CmdList, KsiDagEditCmd, cmd, _E((KsiDagEditCmd){0,{{0,0,0,0,{0}}}}));
#endif
