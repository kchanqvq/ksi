#include <portaudio.h>
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
        int32_t fb;
        int32_t fs;
        int nproc;

        PaError perr = Pa_Initialize();
        const char *errtxt;
        if( perr != paNoError ) goto pa_error;

        PaStream *stream;
        char *line = linenoise("Provide framesPerBuffer,framesPerSecond,nprocs to initialize KSI engine: ");
        if(!line)
                return -1;
        int p = sscanf(line,"%"SCNd32",%"SCNd32",%d", &fb,&fs,&nproc);
        linenoiseFree(line);
        if(p-3)
                goto input_error;
        ksiEngineInit(&e, fb, fs, nproc);
        perr = Pa_OpenDefaultStream( &stream,
                                    0,
                                    2,
                                    paFloat32,
                                    fs,
                                    fb,

                                    ksiEngineAudioCallback,
                                    &e );
        if( perr != paNoError ) goto pa_error;
        KsiError err;

        while(1){
                line = linenoise("KSID> ");
                if(!line||consume_line(&e,stream, line, &err, &errtxt)){
                        free(line);
                        break;
                }
                err=ksiErrorNone;
                free(line);
        }
        if(err==ksiErrorAudio)
                goto pa_error_rep;
        fputs("Finalizing KSI Engine.\n",stdout);
        ksiEngineDestroy(ksiEngineDestroyChild(&e));
        perr = Pa_CloseStream(stream);
        if( perr != paNoError ) goto pa_error;
        perr = Pa_Terminate();
        if( perr != paNoError ) goto pa_error;

        return 0;
input_error:
        fputs("FATAL ERROR: Illegal input\n",stderr);
        return -1;
pa_error:
        errtxt = Pa_GetErrorText(perr);
pa_error_rep:
        fprintf(stderr,"FATAL ERROR: Port Audio: %s %d\n",errtxt,perr);
        return -1;
}
