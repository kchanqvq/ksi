#include <portaudio.h>
#include "io/pa_io.h"
#include <unistd.h>
#include "engine.h"
#include "dagedit.h"
//#define KSI_USE_LINENOISE
#ifdef KSI_USE_LINENOISE
#include "linenoise/linenoise.h"
#else
#include "dummy_linenoise.h"
#endif
#include "cmd.h"
/*
void fn(void *input,void *output,void *args){
        int *t = (int *)args;
        float *o = (float *)output;
        for(int i = 0;i<256;i++){
                o[i*2] = sinf(2*M_PI*((float)(*t)+i)/441.0);
                o[i*2+1] = sinf(2*M_PI*((float)(*t)+i)/441.0);
        }
        *t+=256;
        //usleep(5000);
}
static int testAudioCallback( const void *input,
                                   void *output,
                                   unsigned long frameCount,
                                   const PaStreamCallbackTimeInfo* timeInfo,
                                   PaStreamCallbackFlags statusFlags,
                                   void *userData ){
        fn(input,output,userData);
        return 0;
        }*/
int main(){
        KsiEngine e;
        e.nprocs=0;
        PaError perr;
        const char *errtxt;
        char *line;

        KsiError err;

        while(1){
                line = linenoise("KSI> ");
                if(!line||consume_line(&e, line, &err, &errtxt,NULL)){
                        free(line);
                        break;
                }
                err=ksiErrorNone;
                ksiEngineCommit(&e); //Make the change visible to audio workers
                free(line);
        }
        if(e.nprocs){
                fputs("Finalizing KSI Engine.\n",stdout);
                if(e.playing)
                        ksiEngineStop(&e);
                ksiEngineDestroy(ksiEngineDestroyChild(&e));
                if(pa_initialized){
                        perr = Pa_Terminate();
                        if( perr != paNoError ) goto pa_error;
                }
        }
        return 0;
pa_error:
        errtxt = Pa_GetErrorText(perr);
        fprintf(stderr,"FATAL ERROR: Port Audio: %s %d\n",errtxt,perr);
        return -1;
}
