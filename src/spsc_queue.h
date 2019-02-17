#ifndef __spsc_queue_h__
#define __spsc_queue_h__
#include <assert.h>
#include <stdatomic.h>
#include "util.h"
#include "_config.h"
#ifndef ALIGN
#define ALIGN __attribute__((aligned(_CONFIG_CACHE_SIZE*2)))
#endif
#define ksiSPSCDeclareList(name,datatype,dataname,invalidVal) \
typedef struct _KsiSPSC##name##Node{\
        _Atomic(struct _KsiSPSC##name##Node *) next;\
        datatype dataname;\
} KsiSPSC##name##Node;\
typedef struct _KsiSPSC##name{\
        KsiSPSC##name##Node *head;\
        /*Node Cache*/\
        KsiSPSC##name##Node *cacheHead;\
        KsiSPSC##name##Node *cacheTail ALIGN;\
\
        /*Consumer part*/\
        _Atomic(KsiSPSC##name##Node *) tail ALIGN;   \
} KsiSPSC##name;\
static inline KsiSPSC##name##Node *ksiSPSC##name##NodeAlloc(KsiSPSC##name *q){\
        KsiSPSC##name##Node *ret;\
        if(q->cacheHead != q->cacheTail){\
                ret = q->cacheHead;\
                q->cacheHead = q->cacheHead->next;\
                return ret;\
        }\
        q->cacheTail = atomic_load_explicit(&q->tail,memory_order_consume);\
        if(q->cacheHead != q->cacheTail){\
                ret = q->cacheHead;\
                q->cacheHead = q->cacheHead->next;\
                return ret;\
        }\
        ret = ksiMalloc(sizeof *ret);\
        atomic_init(&ret->next,NULL);\
        return ret;\
}\
static inline void ksiSPSC##name##Init(KsiSPSC##name *q){\
        KsiSPSC##name##Node *n = ksiMalloc(sizeof *n);\
        atomic_init(&n->next, NULL);\
        n->dataname = invalidVal;                   \
        q->head = q->cacheHead = q->cacheTail = n;\
        atomic_init(&q->tail, n);\
\
}\
static inline void ksiSPSC##name##Destroy(KsiSPSC##name *q){\
        KsiSPSC##name##Node *n = q->cacheHead;\
        while(n){\
                KsiSPSC##name##Node *next = atomic_load_explicit(&n->next,memory_order_relaxed);\
                free(n);\
                n = next;\
        }\
}\
static inline void ksiSPSC##name##Enqueue(KsiSPSC##name *q,datatype dataname){\
        KsiSPSC##name##Node *n = ksiSPSC##name##NodeAlloc(q);\
        *n = (KsiSPSC##name##Node){NULL, dataname};\
        atomic_store_explicit(&q->head->next, n, memory_order_release);\
        q->head = n;\
}\
static inline datatype ksiSPSC##name##Dequeue(KsiSPSC##name *q){\
        KsiSPSC##name##Node *n = atomic_load_explicit(&q->tail,memory_order_consume);\
        KsiSPSC##name##Node *nn = atomic_load_explicit(&n->next,memory_order_consume); \
        if(nn){\
                datatype ret = nn->dataname;\
                atomic_store_explicit(&q->tail,nn,memory_order_release);   \
                return ret;\
        }\
        else\
                return invalidVal;\
} //
ksiSPSCDeclareList(PtrList, void *, ptr, NULL);

#endif
