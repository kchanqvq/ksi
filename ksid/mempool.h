#ifndef mempool_h
#define mempool_h
#include <stdlib.h>
// Dummy
static inline void *ksiPoolMalloc(size_t size){
        return malloc(size);
}
static inline void ksiPoolFree(void *ptr,size_t size){
        free(ptr);
}
#endif
