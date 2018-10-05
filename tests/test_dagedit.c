#include "dagedit.h"
int main(){
        KsiEngine e;
        KsiNode n1;
        KsiNode n2;
        KsiNode n3;
        ksiEngineInit(&e, 256, 44100,2);
        ksiNodeInit(&n1, NULL, 1, 1, ksiNodeTypeInputFixed|ksiNodeTypeOutputNormal, &e,NULL,-1);
        ksiNodeInit(&n2, NULL, 1, 1, ksiNodeTypeInputFixed|ksiNodeTypeOutputNormal, &e,NULL,-1);
        ksiNodeInit(&n3, NULL, 2, 2, ksiNodeTypeInputMixer|ksiNodeTypeOutputFinal, &e,NULL,-1);
        int32_t i1 = ksiEngineAddNode(&e, &n1);
        int32_t i2 = ksiEngineAddNode(&e, &n2);
        int32_t i3 = ksiEngineAddNode(&e, &n3);
        ksiEngineMakeAdjustableWire(&e, i1, 0, i3, 0, 1.0f);
        ksiEngineMakeAdjustableWire(&e, i2, 0, i3, 1, 1.0f);
        ksiEngineMakeAdjustableWire(&e, i2, 0, i3, 0, 1.0f);
        /*
        ksiEngineRemoveWire(&e, i2, 0, i3, 1);
        ksiEngineRemoveWire(&e, i2, 0, i3, 0);*/
        KsiNode *n;
        ksiEngineRemoveNode(&e, 1,&n);
        ksiEngineSerialize(&e, stdout);
        return 0;
}
