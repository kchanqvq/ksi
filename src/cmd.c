#include <errno.h>
#include "dag.h"
#include "dagedit.h"
#include "engine.h"
#include "cmd.h"
#include "resource.h"
int consume_line(KsiEngine *e,PaStream *stream,char *line,KsiError *ptrerr,char **pcli_err_str){
        char *lptr = line;
        KsiError err;
        char *cli_err_str=NULL;
        char *cmd;
        PaError perr;
        switch(lptr[0]){
        case 'n':{
                lptr++;
                char i,o;
                int32_t type;
                cmd = "New node";
                int readcount = sscanf(lptr, "%c%"SCNd16, &o,&type);
#define CHECK_READ(n)                                   \
                if(readcount - n){                      \
                        cli_err_str="Invalid argument format."; \
                        goto cli_err;                   \
                }
                CHECK_READ(2);
                type<<=16;
                switch(o){
                case 'n':
                        type|=ksiNodeTypeOutputNormal;
                        break;
                case 'f':
                        type|=ksiNodeTypeOutputFinal;
                        break;
                default:
                        cli_err_str="Invalid output type.";
                        goto cli_err;
                }
                printf("Node created with id: %"PRId32"\n",ksiEngineAddNode(e, ksiNodeInit((KsiNode *)malloc(sizeof(KsiNode)), type, e, NULL)));
        }
                break;
        case 'w':{
                lptr++;
                int32_t srcId,srcPort,desId,desPort;
                int readcount = sscanf(lptr,"%"SCNd32":%"SCNd32">%"SCNd32":%"SCNd32,&srcId,&srcPort,&desId,&desPort);
                cmd = "Make direct wire";
                CHECK_READ(4);
                err = ksiEngineMakeDirectWire(e, srcId, srcPort, desId, desPort);
                if(err)
                        goto rt_err;
        }
                break;
        case 'a':{
                lptr++;
                int32_t srcId,srcPort,desId,desPort;
                int consumed;
                int readcount = sscanf(lptr,"%"SCNd32":%"SCNd32">%"SCNd32":%"SCNd32"x%n",&srcId,&srcPort,&desId,&desPort,&consumed);
                lptr+=consumed;
                cmd = "Make adjustable wire";
                CHECK_READ(4);
                int8_t t;
                err=ksiEngineGetInputType(e, desId, desPort, &t);
                if(err)
                        goto rt_err;
#define INPUT_G(g,t) \
                KsiData g;\
                if(t==ksiNodePortTypeFloat){\
                        float gain;\
                        readcount = sscanf(lptr,"%f",&gain);\
                        CHECK_READ(1);\
                        g.f = gain;\
                }\
                else{\
                        int32_t gain;\
                        readcount = sscanf(lptr, "%"SCNd32, &gain);\
                        CHECK_READ(1);\
                        g.i=gain;\
                }
                INPUT_G(g,t);
                err = ksiEngineMakeAdjustableWire(e, srcId, srcPort, desId, desPort, g);
                if(err)
                        goto rt_err;
        }
                break;
        case 'd':{
                lptr++;
                cmd = "Detach node connection";
                int32_t srcId,desId;
                int readcount = sscanf(lptr, "%"SCNd32">%"SCNd32, &srcId,&desId);
                CHECK_READ(2);
                err = ksiEngineDetachNodes(e, srcId, desId);
                if(err)
                        goto rt_err;
        }
                break;
        case 'r':{
                lptr++;
                cmd = "Remove node";
                int32_t id;
                int readcount = sscanf(lptr,"%"SCNd32,&id);
                CHECK_READ(1);
                KsiNode *n;
                err = ksiEngineRemoveNode(e, id,&n);
                if(err)
                        goto rt_err;
                free(ksiNodeDestroy(n));
        }
                break;
        case 'u':{
                lptr++;
                cmd = "Remove wire";
                int32_t srcId,srcPort,desId,desPort;
                int readcount = sscanf(lptr,"%"SCNd32":%"SCNd32">%"SCNd32":%"SCNd32,&srcId,&srcPort,&desId,&desPort);
                CHECK_READ(4);
                err = ksiEngineRemoveWire(e, srcId, srcPort, desId, desPort);
                if(err)
                        goto rt_err;
        }
                break;
        case 'p':{
                lptr++;
                cmd = "Start playing";
                err = ksiEngineLaunch(e);
                if(err)
                        goto rt_err;
                perr = Pa_StartStream( stream );
                if( perr != paNoError ){
                        err = ksiErrorAudio;
                        *pcli_err_str = Pa_GetErrorText(perr);
                        goto rt_err;
                }
        }
                break;
        case 's':{
                lptr++;
                cmd = "Stop playing";
                err = ksiEngineStop(e);
                if(err)
                        goto rt_err;
                perr = Pa_StopStream( stream );
                if( perr != paNoError ){
                        err = ksiErrorAudio;
                        *pcli_err_str = Pa_GetErrorText(perr);
                        goto rt_err;
                }
        }
                break;
        case '.':{
                lptr++;
                cmd = "Set time";
                size_t newt;
                int readcount = sscanf(lptr, "%zu", &newt);
                CHECK_READ(1);
                e->timeStamp = newt;
                break;
        }
        case '-':{
                lptr++;
                double t;
                cmd = "Set time";
                int readcount = sscanf(lptr, "%lf", &t);
                CHECK_READ(1);
                e->timeStamp = round(t/e->framesPerSecond);
                break;
        }
        case 'b':{
                lptr++;
                cmd = "Make bias";
                int32_t id,port;
                int consumed;
                int readcount = sscanf(lptr, "%"SCNd32":%"SCNd32"x%n",&id,&port,&consumed);
                CHECK_READ(2);
                lptr+=consumed;
                int8_t t;
                err = ksiEngineGetInputType(e, id, port, &t);
                if(err)
                        goto rt_err;
                INPUT_G(g, t);
                err = ksiEngineMakeAdjustableBias(e, id, port, g);
                if(err)
                        goto rt_err;
                break;
        }
        case 'S':{
                lptr++;
                cmd = "Set parament";
                int32_t id,pid;
                int consumed;
                int readcount = sscanf(lptr, "%"SCNd32":%"SCNd32"x%n", &id,&pid,&consumed);
                CHECK_READ(2);
                lptr+=consumed;
                int8_t t;
                err = ksiEngineGetParamentType(e, id, pid, &t);
                if(err)
                        goto rt_err;
                INPUT_G(g, t);
                err = ksiEngineSetParament(e, id, pid, g);
                if(err)
                        goto rt_err;
                break;
        }
        case 'q':
                goto quit;
        case '>':{
                ksiEngineSerialize(e, stdout);
                break;
        }
        case 'h':{
                fputs("COMMAND LIST\n"
                      "New node: n[Input type][Output type][Built-in type]\n"
                      " Input type: f for fixed number of input wire and m for mixer-support input\n"
                      " Output type: n for normal and f for final output of the whole engine\n"
                      " Note: Will print the ID for the new node\n"
                      "Make direct wire:w[Source ID]: [Source port]>[Destination ID]:[Destination port]\n"
                      " Note: Destination node's input type must be f\n"
                      "Make adjustable wire: a[Source ID]:[Source port]>[Destination ID]:[Destination port]x[Gain]\n"
                      "Make bias: b[Node ID]:[Port]x[Bias]\n"
                      " Note: Destination node's input type must be m\n"
                      "       Duplicated wire will be automatically merged with a warning message\n"
                      "Remove node: r[ID]\n"
                      "Remove wire: u[Source ID]:[Source port]>[Destination ID]:[Destination port]\n"
                      "Load time sequence resource file: o[file name]\n"
                      "Unload time sequence resource file: t[resource id]\n"
                      "Set parament:S[Node ID]:[Parament ID]x[Value]"
                      "Start playing: p\n"
                      "Stop playing: s\n"
                      "Load command list from file: l[file name]\n"
                      "Set time: .[Time in frames] or -[Time in seconds]\n"
                      "Dump DAG: >\n"
                      "Quit: q\n"
                      "Help: h\n", stdout);
        }
        case '\0':
        case '\n':
                break;
        case 'l':{
                lptr++;
                cmd="Load command list from file";
                size_t li = strlen(lptr)-1;
                if(lptr[li]=='\n')
                        lptr[li]='\0';
                FILE *fp = fopen(lptr, "r");
                if(fp&&ferror(fp)){
                        fclose(fp);
                        fp=NULL;
                }
                if(!fp){
                        err = ksiErrorFileSystem;
                        cli_err_str = strerror(errno);
                        errno = 0;
                        goto rt_err;
                }
                if(consume_file(e, stream, fp, ptrerr, pcli_err_str)){
                        fputs("Encountered error while executing: ", stderr);
                        fputs(lptr, stderr);
                        fputc('\n',stderr);
                        err=*ptrerr;
                        cli_err_str=*pcli_err_str;
                        goto rt_err;
                }
        }
                break;
        case 'o':{
                lptr++;
                cmd="Load time sequence resource file";
                size_t li = strlen(lptr)-1;
                if(lptr[li]=='\n')
                        lptr[li]='\0';
                int32_t id;
                err = ksiTimeSeqLoadToEngineFromTextFilePath(e, lptr, &id);
                if(err){
                        if(errno){
                                cli_err_str = strerror(errno);
                                errno = 0;
                        }
                        goto rt_err;
                }
                else{
                        printf("Resource loaded with resource id %"PRId32"\n", id);
                }
        }
                break;
        case 't':{
                lptr++;
                int32_t id;
                cmd="Unload time sequence resource file";
                int readcount = sscanf(lptr,"%"SCNd32,&id);
                CHECK_READ(1);
                err = ksiTimeSeqUnloadFromEngine(e, id);
                if(err)
                        goto rt_err;
        }
                break;
        default:
                cmd = "Unknown command";
                cli_err_str = "Invalid verb.";
                goto cli_err;
        }
        return 0;
cli_err:
        fputs("Illegal command. Type h for help.\n", stderr);
        fputs(cmd,stderr);
        fputs(": ",stderr);
        fputs(cli_err_str,stderr);
        fputc('\n',stderr);
        *pcli_err_str = cli_err_str;
        *ptrerr = ksiErrorSyntax;
        return 0;
rt_err:
        fputs("Error executing command.\n",stderr);
        fputs(cmd,stderr);
        fputs(": ",stderr);
        fputs(ksiErrorMsg(err),stderr);
        fputs(".\n",stderr);
        *ptrerr = err;
        if(cli_err_str){
                fputs("Detail: ",stderr);
                fputs(cli_err_str,stderr);
                fputs("\n",stderr);
                *pcli_err_str=cli_err_str;
        }
        return 0;
quit:
        return 1;
}
#define BUFFER_SIZE 64
int consume_file(KsiEngine *e,PaStream *stream,FILE *fp,KsiError *perr,char **pcli_err_str){
        char *line = NULL;
        size_t cap = 0;
        ssize_t len;
        int ferr = 0;
        ssize_t ln=-1;
        while((len=getline(&line, &cap, fp))>0){
                ln++;
                if(consume_line(e,stream,line, perr, pcli_err_str)){
                        break;
                }
                if(*perr||*pcli_err_str){
                        ferr = 1;
                        ssize_t li = strlen(line)-1;
                        fprintf(stderr, "*** line %zd | ", li);
                        if(li>0){
                                fputs(line, stderr);
                                if(line[li]!='\n')
                                        fputc('\n',stderr);
                        }
                        else
                                fputc('\n',stderr);
                        break;
                }
        }
        if(line)
        	free(line);
        return ferr;
}
