#ifndef __kscarlet_wavetable_h__
#define __kscarlet_wavetable_h__
void kscarletWavetableInit(KsiNode *n);
void kscarletWavetableDestroy(KsiNode *n);
void kscarletWavetable(KsiNode *n,KsiData **inputBuffers,KsiData *outputBuffer);
#endif
