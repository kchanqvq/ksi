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
        KsiEvent *next;
        KsiEvent e;
} KsiEventNode;
// tail->node->node->node->head
// The producer node enqueue to head
// Consumer nodes iterate from tail to head
typedef struct _KsiEventQueue{
        KsiEventNode* head;
        KsiEventNode* tail;
} KsiEventQueue;
#define ksiEventEnqueue(eq, ndata) do{\
                KsiEventNode *n = ksiPoolMalloc(sizeof(KsiEventNode));  \
                n->next = NULL;                                         \
                n->e._Generic((ndata),\
                              KsiData:data,\
                              char *:str,\
                              void *:ptr) = (ndata);                        \
                if((eq)->tail){                                        \
                        (eq)->head->next = n;                          \
                }                                                      \
                else{\
                        (eq)->tail = n;         \
                }                               \
                (eq)->head = n;                 \
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
