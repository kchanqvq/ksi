#ifndef __mvcc_utils_h__
#define __mvcc_utils_h__
#include "spsc_queue.h"
#include "engine.h"
#include "util.h"
// flag = 0 for editing
// flag = 1 for commiting
static inline void *ksiMVCCMonitoredMalloc(KsiEngine *e, size_t size, int flag){
        if(!size)
                return NULL;
        KsiSPSCPtrList *q = &e->mallocBufs;
        void *ret;
        switch(flag){
        case 0:
                ret = ksiMalloc(size);
                ksiSPSCPtrListEnqueue(q, ret);
                break;
        case 1:
                ret = ksiSPSCPtrListDequeue(q);
                break;
        default:
                assert(0);
        }
        assert(ret);
        return ret;
}
static inline void ksiMVCCDeferredFree(KsiEngine *e, void *ptr, int flag){
        if(!ptr)
                return;
        switch(flag){
        case 0:
                ksiSPSCPtrListEnqueue(&e->freeBufs, ptr);
                break;
        case 1:{
                void *fptr = ksiSPSCPtrListDequeue(&e->freeBufs);
                assert(fptr==ptr);
                free(fptr);
                break;
        }
        case 2:
                free(ptr);
                break;
        default:
                assert(0);
        }
}
static inline void *ksiMVCCRealloc(KsiEngine *e, void *ptr, size_t size, int flag){
        if(!ptr)
                return ksiMVCCMonitoredMalloc(e, size, flag);
        KsiSPSCPtrList *q = &e->mallocBufs;
        void *ret;
        switch(flag){
        case 0:
                ret = ksiRealloc(ptr, size);
                ksiSPSCPtrListEnqueue(q, ret);
                break;
        case 1:
                ret = ksiSPSCPtrListDequeue(q);
                break;
        default:
                assert(0);
        }
        assert(ret);
        return ret;

}
static inline void impl_ksiNodeChangeInputPortCount(KsiNode *n,int32_t port_count,int8_t* newTypes,int flag){
        if(!(n->type & ksiNodeTypeDynamicInputType)){
                int8_t* new_types = ksiMalloc(sizeof(int8_t) * port_count);
                memcpy(new_types, n->inputTypes, sizeof(int8_t) * MIN(n->inputCount,port_count));
                n->inputTypes = new_types;
                n->type |= ksiNodeTypeDynamicInputType;
        }
        else
                n->inputTypes = ksiRealloc(n->inputTypes, sizeof(int8_t) * port_count);
        n->inputCache = ksiMVCCRealloc(n->e, n->inputCache, sizeof(KsiData) * port_count, flag);
        n->env.portEnv = ksiRealloc(n->env.portEnv, sizeof(KsiMixerEnvEntry) * port_count);
        n->env.internalBufferPtr = ksiRealloc(n->env.internalBufferPtr, sizeof(KsiData *) * port_count);
        if(port_count > n->inputCount){
                for(int32_t i = n->inputCount; i < port_count; i++){
                        n->env.portEnv[i].mixer = NULL;
                        n->env.portEnv[i].buffer = NULL;
                        n->env.internalBufferPtr[i] = NULL;
                        n->inputTypes[i] = newTypes[i-n->inputCount];
                }
                if(!flag){
                        memset(n->inputCache+n->inputCount, 0, sizeof(KsiData)*(port_count - n->inputCount));
                }
        }
        n->inputCount = port_count;
}
static inline void impl_ksiNodeChangeOutputPortCount(KsiNode *n,int32_t port_count,int8_t* newTypes,int flag){
        if(!(n->type & ksiNodeTypeDynamicOutputType)){
                int8_t* new_types = ksiMalloc(sizeof(int8_t) * port_count);
                memcpy(new_types, n->outputTypes, sizeof(int8_t) * MIN(n->outputCount,port_count));
                n->outputTypes = new_types;
                n->type |= ksiNodeTypeDynamicOutputType;
        }
        else
                n->outputTypes = ksiRealloc(n->outputTypes, sizeof(int8_t) * port_count);
        n->outputCache = ksiMVCCRealloc(n->e, n->outputCache, sizeof(int8_t) * port_count, flag);
        if((n->type&ksiNodeTypeOutputMask) == ksiNodeTypeOutputNormal){
        n->outputBuffer = ksiMVCCRealloc(n->e, n->outputBuffer, sizeof(KsiData) * n->e->framesPerBuffer * port_count, flag);
        if(port_count > n->outputCount){
                for(int32_t i = n->inputCount; i < port_count; i++){
                        n->outputTypes[i] = newTypes[i-n->outputCount];
                }
                if(!flag){
                        memset(n->outputBuffer+n->outputCount*n->e->framesPerBuffer, 0, sizeof(KsiData)*n->e->framesPerBuffer*(port_count - n->outputCount));
                        memset(n->outputCache+n->outputCount, 0, sizeof(KsiData)*(port_count - n->outputCount));
                }
        }
        }
        n->outputCount = port_count;
}
#endif
