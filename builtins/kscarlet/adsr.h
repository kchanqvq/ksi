#ifndef __kscarlet_adsr_h__
#define __kscarlet_adsr_h__
void kscarletADSR(KsiNode *n,KsiData **inputBuffers,KsiData *outputBuffer);
void kscarletADSRReset(KsiNode *n);
void kscarletADSRInit(KsiNode *n);
void kscarletADSRDestroy(KsiNode *n);
#endif
