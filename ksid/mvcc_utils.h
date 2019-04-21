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
//When new count > old count, add new ports, with types in newTypes.
//Otherwise, delete ports.
#endif
