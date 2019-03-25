#include "pa_io.h"
#include "engine.h"
#include <portaudio.h>
int pa_initialized = 0;
static inline void ksiPAError(PaError perr, const char *errtxt){
        fprintf(stderr,"FATAL ERROR: Port Audio: %s %d\n",errtxt,perr);
        exit(1);
}

int paAudioCallback( const void *input,
                            void *output,
                            unsigned long frameCount,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData ){
        return ksiEngineAudioCallback(input, output, frameCount, userData);
}
KsiError paIOInit(void *engine,const char **perrtxt){
        KsiEngine *e = (KsiEngine *)engine;
        KsiError err = ksiErrorNone;
        CHECK_INITIALIED(e);
        ksiEnginePlayingLock(e);
        if(!pa_initialized){
                Pa_Initialize();
                pa_initialized = 1;
        }
        if(e->driver_env)
                if((err = paIODestroy(e)))
                        goto ret;
        PaStreamParameters ip = {Pa_GetDefaultInputDevice(),1,paFloat32|paNonInterleaved,0,NULL};
        PaStreamParameters op = {Pa_GetDefaultOutputDevice(),2,paFloat32|paNonInterleaved,0,NULL};
        PaError perr = Pa_OpenStream(&(e->driver_env),
                                     &ip,
                                     &op,
                                     e->framesPerSecond,
                                     e->framesPerBuffer,
                                     paClipOff,
                                     paAudioCallback,
                                     e);
        if( perr != paNoError ){
                *perrtxt = Pa_GetErrorText(perr);
                err = ksiErrorAudio;
                goto ret;
        }
        perr = Pa_StartStream((PaStream *)e->driver_env);
        if( perr != paNoError ){
                *perrtxt = Pa_GetErrorText(perr);
                err = ksiErrorAudio;
                goto ret;
        }
ret:
        ksiEnginePlayingUnlock(e);
        return err;
}
KsiError paIODestroy(void *engine){
        KsiEngine *e = (KsiEngine *)engine;
        CHECK_INITIALIED(e);
        ksiEnginePlayingLock(e);
        if(e->driver_env){
                PaError perr;
                perr = Pa_StopStream((PaStream *)e->driver_env);
                if( perr != paNoError ) ksiPAError(perr, Pa_GetErrorText(perr));
                perr = Pa_CloseStream(((PaStream *)e->driver_env));
                if( perr != paNoError ) ksiPAError(perr, Pa_GetErrorText(perr));
                e->driver_env = NULL;
        }
        ksiEnginePlayingUnlock(e);
        return ksiErrorNone;
}
