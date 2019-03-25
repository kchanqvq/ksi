#ifndef events_h
#define events_h
#include "data.h"
#include "spsc_queue.h"
#include "mempool.h"
typedef struct _KsiEvent {
        size_t timeStamp;
        union {
                KsiData data;
                char *str;
                void *ptr;
        };
} KsiEvent;
typedef struct _KsiEventNode{
        struct _KsiEventNode *next;
        KsiEvent e;
} KsiEventNode;
// tail->node->node->node->head
// The producer node enqueue to head
// Consumer nodes iterate from tail to head
typedef struct _KsiEventQueue{
        KsiEventNode* head;
        KsiEventNode* tail;
} KsiEventQueue;
#define ksiEventEnqueue(eq, ndata, t) do{                                 \
                KsiEventNode *n_ = ksiPoolMalloc(sizeof(KsiEventNode));  \
                n_->next = NULL;                                         \
                n_->e.timeStamp = (t);                                   \
                _Generic((ndata),                               \
                              KsiData:n_->e.data,\
                              char *:n_->e.str,\
                         void *:n_->e.ptr) = (ndata);                  \
                if((eq)->tail){                                        \
                        (eq)->head->next = n_;                          \
                }                                                      \
                else{\
                        (eq)->tail = n_;         \
                }                               \
                (eq)->head = n_;                 \
        }while(0)
#define ksiEventClearQueue(eq) do{\
                KsiEventNode *iter=(eq)->tail;          \
                while(iter){\
                        KsiEventNode *tmp = iter->next;   \
                        ksiPoolFree(iter,sizeof(KsiEventNode)); \
                        iter = tmp;                             \
                }                                                \
                (eq)->head=(eq)->tail=NULL;                      \
        }while(0)
#endif
