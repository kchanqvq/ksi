#ifndef __sem_h__
#define __sem_h__
//#define __sem_experimental__
#ifdef __sem_experimental__
#include <stdatomic.h>
typedef _Atomic int KsiSem;
static inline int ksiSemInit(KsiSem *s, int flags, unsigned n){
        atomic_init(s,n);
        return 0;
}
static inline int ksiSemDestroy(KsiSem *s){
        return 0;
}
static inline int ksiSemPost(KsiSem *s){
        atomic_fetch_add(s,1);
        return 0;
}
static inline int ksiSemWait(KsiSem *s){
        int expected = 0;
        while(!expected){
                expected = 1;
                atomic_compare_exchange_weak(s,&expected,0);
        }
        return 0;
}
#elif defined(__APPLE__)
#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/mach_traps.h>
typedef semaphore_t KsiSem;
extern mach_port_t mach_task_self();
static inline int ksiSemInit(KsiSem *s, int flags, unsigned n){
        return (int)semaphore_create(mach_task_self(), s, SYNC_POLICY_FIFO, n);
}
static inline int ksiSemDestroy(KsiSem *s){
        return (int)semaphore_destroy(mach_task_self(), *s);
}
static inline int ksiSemPost(KsiSem *s){
        return (int)semaphore_signal(*s);
}
static inline int ksiSemWait(KsiSem *s){
        return (int)semaphore_wait(*s);
}
typedef semaphore_t KsiBSem;
static inline int ksiBSemInit(KsiBSem *s, int flags, unsigned nprocs){
        return (int)semaphore_create(mach_task_self(), s, SYNC_POLICY_FIFO, 0);
}
static inline int ksiBSemDestroy(KsiBSem *s){
        return (int)semaphore_destroy(mach_task_self(), *s);
}
static inline int ksiBSemPost(KsiBSem *s){
        return (int)semaphore_signal_all(*s);
}
static inline int ksiBSemWait(KsiBSem *s){
        return (int)semaphore_wait(*s);
}

#else
#include <pthread.h>
typedef pthread_barrier_t KsiBSem;
static inline int ksiBSemInit(KsiBSem *s, int flags, unsigned nprocs){
        return pthread_barrier_init(s,NULL,nprocs);
}
static inline int ksiBSemDestroy(KsiBSem *s){
        return pthread_barrier_destroy(s);
}
static inline int ksiBSemPost(KsiBSem *s){
        return pthread_barrier_wait(s);
}
static inline int ksiBSemWait(KsiBSem *s){
        return pthread_barrier_wait(s);
}
#endif
#endif
