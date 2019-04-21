#ifndef ebr_h
#define ebr_h
#include "ring_buffer.h"
typedef struct _KsiDynamicEBROverlooker{
        _Atomic int64_t active;
        uint64_t epoch;
} KsiDynamicEBROverlooker;
#include "vec.h"
#include <pthread.h>
typedef struct _KsiRingBufferDynamicEBR{
        int nprocs;
        _Atomic int64_t lock;
        KsiVec dynamicOverlookers;
        _Atomic uint64_t epoch;
        KsiRingBufferEBREntry ebrEntries[];
} KsiDynamicEBR;

int ksiDynamicEBRTryFree(KsiDynamicEBR *ebr,int nprocs,int tid);
static inline int32_t ksiDynamicEBROverlookerRegister(KsiDynamicEBR *ebr){
        int64_t dummy = 0;
        int32_t result;
        KsiDynamicEBROverlooker *ol = malloc(sizeof *ol);
        atomic_init(&ol->active,0);
        ol->epoch = atomic_load_explicit(&ebr->epoch,memory_order_relaxed);
        while(1){
                if(atomic_compare_exchange_weak_explicit(&ebr->lock,&dummy,1,
                                                           memory_order_acquire,memory_order_relaxed)){
                        result = ksiVecInsert(&ebr->dynamicOverlookers, ol);
                        break;
                }
                else{
                        dummy = 0;
                        pthread_yield_np();
                }
        }
        atomic_store_explicit(&ebr->lock,0,memory_order_release);
        return result;
}
static inline void ksiDynamicEBROverlookerUnregister(KsiDynamicEBR *ebr,int olId){
        int64_t dummy = 0;
        KsiDynamicEBROverlooker *ol = ebr->dynamicOverlookers.data[olId];
        while(1){
                if(atomic_compare_exchange_weak_explicit(&ebr->lock,&dummy,1,
                                                           memory_order_acquire,memory_order_relaxed)){
                        free(ol);
                        ksiVecDelete(&ebr->dynamicOverlookers, olId);
                        break;
                }
                else{
                        dummy = 0;
                        pthread_yield_np();
                }
        }
        atomic_store_explicit(&ebr->lock,0,memory_order_release);
}
KsiDynamicEBR *ksiDynamicEBRCreate(int nprocs);
void ksiDynamicEBRFree(KsiDynamicEBR *ebr);
static inline void ksiDynamicEBROverlookerEnter(KsiDynamicEBR *ebr,int32_t overlookerId){
        KsiDynamicEBROverlooker *ol = ebr->dynamicOverlookers.data[overlookerId];
        atomic_store_explicit(&ol->active, 1, memory_order_release);
        ol->epoch = atomic_load_explicit(&ebr->epoch,memory_order_relaxed);
}
static inline void ksiDynamicEBROverlookerLeave(KsiDynamicEBR *ebr,int32_t overlookerId){
        KsiDynamicEBROverlooker *ol = ebr->dynamicOverlookers.data[overlookerId];
        atomic_store_explicit(&ol->active, 0, memory_order_release);
}
static inline void ksiDynamicEBREnter(KsiDynamicEBR *ebr,int tid){
        atomic_store_explicit(&ebr->ebrEntries[tid].active, 1, memory_order_release);
        ebr->ebrEntries[tid].epoch = atomic_load_explicit(&ebr->epoch,memory_order_relaxed);
}
static inline void ksiDynamicEBRLeave(KsiDynamicEBR *ebr,int tid){
        atomic_store_explicit(&ebr->ebrEntries[tid].active, 0, memory_order_release);
}
void ksiDynamicEBRRetire(KsiDynamicEBR *ebr,KsiRingBufferSegment *ptr,int tid);
#endif
