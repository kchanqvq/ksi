#ifndef __util_h__
#define __util_h__
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#define _CAT(x,y) x##y
#define CAT(x,y) _CAT(x,y)
#define _E(...) __VA_ARGS__
#define _N(...)

#define LIST_END _E _N() (_E(_N,_N))

#define LIST2COMMA_Z(x) x,
#define _LIST2COMMA_X(LIST2COMMA_num,LIST2COMMA_k,...) LIST2COMMA_k(LIST2COMMA_Z(LIST2COMMA_num) LIST2COMMA_Y)
#define _LIST2COMMA_Y(LIST2COMMA_num,LIST2COMMA_k,...) LIST2COMMA_k(LIST2COMMA_Z(LIST2COMMA_num) LIST2COMMA_X)
#define LIST2COMMA_X(LIST2COMMA_num,...) _LIST2COMMA_X(LIST2COMMA_num __VA_ARGS__,_E)
#define LIST2COMMA(x) LIST2COMMA_X x
#define LIST2COMMA_Y(LIST2COMMA_num,...) _LIST2COMMA_Y(LIST2COMMA_num __VA_ARGS__,_E)

#define LIST2PORTTYPE_Z(x) (ksiNodePortType##x)
#define _LIST2PORTTYPE_X(LIST2PORTTYPE_num,LIST2PORTTYPE_k,...) LIST2PORTTYPE_k(LIST2PORTTYPE_Z(LIST2PORTTYPE_num) LIST2PORTTYPE_Y)
#define _LIST2PORTTYPE_Y(LIST2PORTTYPE_num,LIST2PORTTYPE_k,...) LIST2PORTTYPE_k(LIST2PORTTYPE_Z(LIST2PORTTYPE_num) LIST2PORTTYPE_X)
#define LIST2PORTTYPE_X(LIST2PORTTYPE_num,...) _LIST2PORTTYPE_X(LIST2PORTTYPE_num __VA_ARGS__,_E)
#define LIST2PORTTYPE(x) LIST2PORTTYPE_X x
#define LIST2PORTTYPE_Y(LIST2PORTTYPE_num,...) _LIST2PORTTYPE_Y(LIST2PORTTYPE_num __VA_ARGS__,_E)


#define LISTCOUNT_Z(...) +1
#define _LISTCOUNT_X(LISTCOUNT_num,LISTCOUNT_k,...) LISTCOUNT_k(LISTCOUNT_Z(LISTCOUNT_num) LISTCOUNT_Y)
#define _LISTCOUNT_Y(LISTCOUNT_num,LISTCOUNT_k,...) LISTCOUNT_k(LISTCOUNT_Z(LISTCOUNT_num) LISTCOUNT_X)
#define LISTCOUNT_X(LISTCOUNT_num,...) _LISTCOUNT_X(LISTCOUNT_num __VA_ARGS__,_E)
#define LISTCOUNT(x) (0 LISTCOUNT_X x)
#define LISTCOUNT_Y(LISTCOUNT_num,...) _LISTCOUNT_Y(LISTCOUNT_num __VA_ARGS__,_E)
#define CONDITIONAL1(...) __VA_ARGS__
#define CONDITIONAL0(...)
#define CONDITIONAL(cond,...) CAT(CONDITIONAL,cond)(__VA_ARGS__)
#define BRANCH1(x,y) x
#define BRANCH0(x,y) y
#define BRANCH(cond,x,y) CAT(BRANCH,cond)(x,y)

#define PERROR_GUARDED(str,...) if(__VA_ARGS__){perror("FATAL ERROR: "str);abort();}
#define PRINT_OOM() fprintf(stderr,"FATAL ERROR: Out of memory\n")
static inline void* ksiMalloc(size_t s){
        void *ret;
        ret = malloc(s);
        if(ret)
                return ret;
        else{
                PRINT_OOM();
                abort();
        }
}
static inline void* ksiRealloc(void *ptr, size_t s){
        void *ret;
        ret = realloc(ptr,s);
        if(ret)
                return ret;
        else{
                PRINT_OOM();
                abort();
        }
}
#define MIN(x,y) ((x)<(y)?(x):(y))
#undef PRINT_OOM
#ifdef NDEBUG
#define debug_printf(...)
#else
#define debug_printf(...) printf(__VA_ARGS__)
#define debug_check_node(e,n,epoch) assert(!n||n==e->nodes[epoch].data[n->id])
#endif
#endif
