#ifndef dag_members_h
#define dag_members_h
#include "events.h"
#include "data.h"
struct _KsiNode;
typedef void (*KsiNodeFunc)(struct _KsiNode *n);
typedef struct _KsiEventEnvEntry{
        int32_t srcPort;
        struct _KsiNode *src;
        union{
                KsiEventNode *currentNode;
                KsiData gain;
        };
        struct _KsiEventEnvEntry *next;
} KsiEventEnvEntry;
typedef KsiEventEnvEntry KsiMixerEnvEntry;
typedef struct{
        KsiEventEnvEntry *ees;
} KsiEventEnv;
typedef struct{
        KsiData *buffer;//when NULL, direct store src output ptr into internalBufferPtr
        KsiData *internalBufferPtr;
        //KsiData cache; //instead of using explicit cache variable, just use the last element of buffer as the cached value
        KsiMixerEnvEntry *mixer;
} KsiSignalEnv;
typedef union {
        KsiEventEnv *e;
        KsiSignalEnv *d;
} KsiEnvPtr;

typedef union {
        KsiEventQueue *e;
        KsiData *d;
} KsiOutputPtr;
#endif
