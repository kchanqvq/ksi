/****************************************
 KSI Audio Engine
 Please see LICENSE in this folder for
 copyright information.
 ========================================
 @file vec.h
 @brief A dynamic vector that maintains\n
 a unique id for each element.
 @author BlueFlo0d
 @email qhong@mit.edu
*****************************************/
#ifndef __vec_h__
#define __vec_h__
#include "util.h"
#define ksiVecDeclareList(name,int32_t,loc)        \
typedef struct _Ksi##name##Node{\
        struct _Ksi##name##Node *next;\
        int32_t loc;\
} Ksi##name##Node;\
static inline int32_t ksi##name##Pop(Ksi##name##Node **head){\
        Ksi##name##Node *f = *head;\
        int32_t ret = f->loc;\
        *head = f->next;\
        free(f);\
        return ret;\
}\
static inline void ksi##name##Push(Ksi##name##Node **head,int32_t id){\
        Ksi##name##Node *f = (Ksi##name##Node *)ksiMalloc(sizeof(Ksi##name##Node));\
        f->loc = id;\
        f->next = *head;\
        *head = f;\
}\
static inline int ksi##name##Search(Ksi##name##Node *head,int32_t id){\
        while(head){\
                if(head->loc==id)\
                        return 1;\
                head=head->next;\
        }\
        return 0;\
}//
#define impl_ksiVecListDelete(list,predict,break,KsiMixerEnvEntry,free) do{  \
                KsiMixerEnvEntry *me = (list);                      \
                KsiMixerEnvEntry *pre = NULL;                       \
                while(me){                                          \
                        if(me predict){                           \
                                if(pre){                            \
                                        pre->next=me->next;         \
                                        free(me);                   \
                                        me = pre->next;             \
                                }                                   \
                                else{                               \
                                        KsiMixerEnvEntry *d = me;   \
                                        me = me->next;              \
                                        (list) = me;                \
                                        free(d);                    \
                                }                                   \
                                break;                              \
                        }                                           \
                        else{                                       \
                                pre = me;                           \
                                me = me->next;                      \
                        }                                           \
                }                                                   \
        }while(0)
#define ksiVecListDelete(list,predict,break,KsiMixerEnvEntry) impl_ksiVecListDelete(list,predict,break,KsiMixerEnvEntry,free)
#define ksiVecListDestroy(list,type)            \
        do{                                     \
                type *l = (list);               \
                while(l){                       \
                        type *n = l->next;      \
                        free(l);                \
                        l=n;                    \
                }                               \
        }while(0)
ksiVecDeclareList(VecIdlist, int32_t, loc);

//@brief A dynamic vector that maintains a unique id for each element.
typedef struct _KsiVec{
        int32_t size;
        int32_t capacity;
        void **data;
        KsiVecIdlistNode *freelist;
} KsiVec;
//@brief Insert a void* pointer to a KsiVec
//@return A unique id for the element
static inline void ksiVecInit(KsiVec *v,int32_t capacity){
        v->size = 0;
        v->capacity = capacity;
        v->data = ksiMalloc(sizeof(void*)*capacity);
        v->freelist = NULL;
}
static inline void ksiVecDestroy(KsiVec *v){
        free(v->data);
        ksiVecListDestroy(v->freelist, KsiVecIdlistNode);
}
static inline int32_t ksiVecInsert(KsiVec *v,void *item){
        int32_t ret;
        if(v->freelist){
                v->data[ret = ksiVecIdlistPop(&v->freelist)]=item;
        }
        else{
                if(!(v->size<v->capacity-1)){
                        v->capacity*=2;
                        v->data = (void **)realloc(v->data, sizeof(void*)*v->capacity);
                }
                v->data[v->size] = item;
                ret = v->size;
                v->size++;
        }
        return ret;
}
//@brief Delete the element with spercified ID in a KsiVec
static inline void ksiVecDelete(KsiVec *v,int32_t id){
        if(id==v->size-1){
                v->size--;
        }
        else{
                v->data[id] = NULL;
                ksiVecIdlistPush(&v->freelist,id);
        }
}
#define ksiVecBeginIterate(v,i) for(int32_t i##_=0;i##_<(v)->size;i##_++){ \
        void *i=(v)->data[i##_];                                              \
        if(!i)                                                          \
                continue;                                               \
        //

#define ksiVecEndIterate() }//

#define CHECK_VEC(id,vec,err,...) if((id)>=(vec).size||!(vec).data[(id)]){ \
                __VA_ARGS__;/*hook for cleanup*/                        \
        return (err);\
        }

#endif
