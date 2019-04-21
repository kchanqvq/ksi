#include "ebr.h"
int ksiDynamicEBRTryFree(KsiDynamicEBR *ebr,int nprocs,int tid){
        uint64_t epoch = atomic_load_explicit(&ebr->epoch,memory_order_acquire);
        int i = nprocs;
        uint64_t dummy = 0;
        while(i--){
                if(atomic_load_explicit(&ebr->ebrEntries[i].active,memory_order_acquire) && (ebr->ebrEntries[i].epoch != epoch))
                        return -1;
        }
        while(1){
                if(atomic_compare_exchange_weak_explicit(&ebr->lock,&dummy,1,
                                                         memory_order_acquire,memory_order_relaxed)){
                        ksiVecBeginIterate(&ebr->dynamicOverlookers, i);
                        KsiRingBufferEBREntry *ee = i;
                        if(atomic_load_explicit(&ee->active,memory_order_acquire) && (ee->epoch != epoch)){
                                atomic_store_explicit(&ebr->lock,0,memory_order_release);
                                return -1;
                        }
                        ksiVecEndIterate();
                        break;
                }
                else{
                        dummy = 0;
                        pthread_yield_np();
                }
        }
        dummy=epoch;
        atomic_store_explicit(&ebr->lock,0,memory_order_release);
        if(atomic_compare_exchange_strong_explicit(&ebr->epoch,&dummy,(epoch+1)%3,memory_order_acquire,memory_order_relaxed)){
                int i = nprocs;
                while(i--){
                        destroyFromTail(ebr->ebrEntries[i].freelist[(epoch+2)%3]);
                        ebr->ebrEntries[i].freelist[(epoch+2)%3]=NULL;
                }
                ebr->ebrEntries[tid].epoch = (epoch+1)%3;
                return 0;
        }
        else{
                return -1;
        }
}
KsiDynamicEBR *ksiDynamicEBRCreate(int nprocs){
        KsiDynamicEBR *ret = malloc(sizeof(KsiDynamicEBR)+sizeof(KsiRingBufferEBREntry)*nprocs);
        ret->nprocs = nprocs;
        atomic_init(&ret->lock,0);
        ksiVecInit(&ret->dynamicOverlookers, 16);
        atomic_init(&ret->epoch,0);
        while(nprocs --){
                ksiRingBufferEBREntryInit(ret->ebrEntries + nprocs);
        }
        return ret;
}
void ksiDynamicEBRFree(KsiDynamicEBR *ebr){
        ksiVecDestroy(&ebr->dynamicOverlookers);
        free(ebr);
}
void ksiDynamicEBRRetire(KsiDynamicEBR *ebr,KsiRingBufferSegment *ptr,int tid){
        KsiRingBufferEBREntry *ee = ebr->ebrEntries + tid;
        ptr->prev = ee->freelist[ee->epoch];
        ee->freelist[ee->epoch] = ptr;
}
