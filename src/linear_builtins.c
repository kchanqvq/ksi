#include "linear_builtins.h"
#include <math.h>
// 2 -> 2
void ksiBuiltinNodeFuncId(KsiNode *n,KsiData **inputBuffers,KsiData *outputBuffer){
        switch(n->type&ksiNodeTypeOutputMask){
        case ksiNodeTypeOutputFinal:
                for(int32_t i=0;i<n->e->framesPerBuffer;i++){
                        int32_t j = n->inputCount;
                        while(j--){
                                ksiNodeRefreshCache(n, j, i);
                                ((KsiData** )outputBuffer)[j][i].f=n->inputCache[j].f;
                        }
                }
                break;
        case ksiNodeTypeOutputNormal:
                for(int32_t i=0;i<n->e->framesPerBuffer;i++){
                        int32_t j = n->inputCount;
                        while(j--){
                                ksiNodeRefreshCache(n, j, i);
                                outputBuffer[j*n->e->framesPerBuffer+j].f=n->inputCache[j].f;
                        }
                }
                break;
        }
}
KsiError ksiBuiltinNodeFuncIdEditCmd(KsiNode *n,const char *args,const char **pcli_err_str,int flag){
        int32_t port_count;
        int ret = sscanf(args, "%"SCNd32, &port_count);
        if(ret != 1)
                goto syn_err;
        if(!(port_count>0)){
                *pcli_err_str = "Number of ports must be greater than 0.";
                return ksiErrorPlugin;
        }
        if(port_count>n->inputCount){
                int32_t i = port_count - n->inputCount;
                int8_t newTypes[i];
                while(i--)
                        newTypes[i] = ksiNodePortTypeFloat;
                impl_ksiNodeChangeInputPortCount(n, port_count, newTypes, flag);
                impl_ksiNodeChangeOutputPortCount(n, port_count, newTypes, flag);
        }
        else{
                impl_ksiNodeChangeInputPortCount(n, port_count, NULL, flag);
                impl_ksiNodeChangeOutputPortCount(n, port_count, NULL, flag);
        }
        return ksiErrorNone;
syn_err:
        *pcli_err_str = "Invalid argument.\n"
                "Usage:[Number of ports to change to]";
        return ksiErrorSyntax;
}
// 1 -> 1
void ksiBuiltinNodeFuncTestOsc(KsiNode *n,KsiData **inputBuffers,KsiData *ob){
        int32_t bufsize = n->e->framesPerBuffer;
        for(int32_t i = 0;i<bufsize;i++){
                ksiNodeRefreshCache(n, 0, i);
                float freq = n->inputCache[0].f==0?100.0:n->inputCache[0].f;
                float dfreq = freq/n->e->framesPerSecond;
                ob[i].f = sinf(2*M_PI*((float)(n->e->timeStamp)+i)*dfreq);
        }
        ksiNodePortIOSetDirty(n->outputTypes[0]);
}
// 2 -> 1
void ksiBuiltinNodeFuncModulator(KsiNode *n,KsiData **inputBuffers,KsiData *ob){
        int32_t bufsize = n->e->framesPerBuffer;
        for(int32_t i=0;i<bufsize;i++){
                ksiNodeRefreshCache(n, 0, i);
                ksiNodeRefreshCache(n, 1, i);
                ob[i].f=n->inputCache[0].f*n->inputCache[1].f;
                //printf("%f,%f ", n->inputCache[0].f,n->inputCache[1].f);
        }
        ksiNodePortIOSetDirty(n->outputTypes[0]);
}
