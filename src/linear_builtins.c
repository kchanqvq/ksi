#include "linear_builtins.h"
#include <math.h>
void ksiBuiltinNodeFuncId(KsiNode *n){
        for(int32_t i=0;i<n->e->framesPerBuffer;i++){
                int32_t j = n->inputCount;
                while(j--){
                        n->outputBuffer[j].d[i] = ksiNodeGetInput(n, n->e->framesPerBuffer, j, i);
                }
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
void ksiBuiltinNodeFuncTestOsc(KsiNode *n){
        int32_t bufsize = n->e->framesPerBuffer;
        for(int32_t i = 0;i<bufsize;i++){
                float freq = ksiNodeGetInput(n, bufsize, 0, i).f;
                if(freq == 0)
                        freq = 441.0;
                float dfreq = freq/n->e->framesPerSecond;
                n->outputBuffer[0].d[i].f = sinf(2*M_PI*((float)(n->e->timeStamp)+i)*dfreq);
        }
        ksiNodePortIOSetDirty(n->outputTypes[0]);
}
// 2 -> 1
void ksiBuiltinNodeFuncModulator(KsiNode *n){
        int32_t bufsize = n->e->framesPerBuffer;
        if((n->inputTypes[0]&ksiNodePortIODirty)||(n->inputTypes[1]&ksiNodePortIODirty)){
                for(int32_t i=0;i<bufsize;i++){
                        n->outputBuffer[0].d[i].f = ksiNodeGetInput(n, bufsize, 0, i).f * ksiNodeGetInput(n, bufsize, 1, i).f;
                        //printf("%f,%f ", n->inputCache[0].f,n->inputCache[1].f);
                }
                ksiNodePortIOSetDirty(n->outputTypes[0]);
        }
        else{
                n->outputBuffer[0].d[bufsize - 1].f = ksiNodeGetInput(n, bufsize, 0, 0).f * ksiNodeGetInput(n, bufsize, 1, 0).f;
                ksiNodePortIOClear(n->outputTypes[0]);
        }
}
