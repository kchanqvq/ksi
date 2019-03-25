#include <errno.h>
#include <math.h>
#include "engine.h"
#include "dag.h"
#include "dagedit.h"
#include "cmd.h"
#include "resource.h"
#include "io/pa_io.h"
#include <libgen.h>
//static counter;
int consume_line(KsiEngine *e,char *line,KsiError *ptrerr,const char **pcli_err_str,int32_t *idRet,log_function lf,void *lf_args){
        //counter++;
        //printf("Command #%d\n",counter);
        //fputs(line,stdout);
        char *lptr = line;
        KsiError err;
        const char *cli_err_str=NULL;
        char *cmd;
        switch(lptr[0]){
        case 'n':{
                lptr++;
                char o;
                int32_t type;
                cmd = "New node";
                int16_t funcId;
                int readcount = sscanf(lptr, "%c%"SCNd16, &o,&funcId);
                type = funcId;
#define CHECK_READ(n)                                           \
                if(readcount - n){                              \
                        cli_err_str="Invalid argument format."; \
                        goto cli_err;                           \
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
                int32_t id;
                err = ksiEngineAddNode(e, type, &id, NULL, 0);
                if(err)
                        goto rt_err;
                if(idRet)
                        *idRet = id;
                lf(lf_args,"Node created with id: %"PRId32"\n",id);
        }
                break;
        case 'w':{
                lptr++;
                int32_t srcId,srcPort,desId,desPort;
                int consumed;
                int readcount = sscanf(lptr,"%"SCNd32":%"SCNd32">%"SCNd32":%"SCNd32"x%n",&srcId,&srcPort,&desId,&desPort,&consumed);
                lptr+=consumed;
                cmd = "Make wire";
                CHECK_READ(4);
                int8_t t;
                err=ksiEngineGetInputType(e, desId, desPort, &t);
                if(err)
                        goto rt_err;
#define INPUT_G(g,t)                                                \
                KsiData g;                                          \
                if((t&ksiNodePortTypeDataMask)==ksiNodePortTypeFloat){ \
                        float gain;                                 \
                        readcount = sscanf(lptr,"%f",&gain);        \
                        CHECK_READ(1);                              \
                        g.f = gain;                                 \
                }                                                   \
                else{                                               \
                        int32_t gain;                               \
                        readcount = sscanf(lptr, "%"SCNd32, &gain); \
                        CHECK_READ(1);                              \
                        g.i=gain;                                   \
                }
                INPUT_G(g,t);
                err = ksiEngineMakeWire(e, srcId, srcPort, desId, desPort, g);
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
                err = ksiEngineRemoveNode(e, id);
                if(err)
                        goto rt_err;
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
        case '.':{
                lptr++;
                cmd = "Set time";
                size_t newt;
                int readcount = sscanf(lptr, "%zu", &newt);
                CHECK_READ(1);
                e->timeStamp = newt;
                ksiEngineReset(e);
                break;
        }
        case '-':{
                lptr++;
                double t;
                cmd = "Set time";
                int readcount = sscanf(lptr, "%lf", &t);
                CHECK_READ(1);
                e->timeStamp = round(t/e->framesPerSecond);
                ksiEngineReset(e);
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
                err = ksiEngineMakeBias(e, id, port, g);
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
        case 'e':{
                lptr++;
                cmd = "Edit node";
                int32_t id;
                int consumed;
                int readcount = sscanf(lptr, "%"SCNd32" %n",&id,&consumed);
                CHECK_READ(1);
                lptr+=consumed;
                err = ksiEngineSendEditingCommand(e, id, lptr, pcli_err_str, strlen(lptr)+1);
                if(err==ksiErrorSyntax)
                        goto cli_err;
                else if(err)
                        goto rt_err;
                break;
        }
        case 'P':{
                cmd = "Pause";
                err = ksiEnginePause(e);
                if(err)
                        goto rt_err;
                break;
        }
        case 'R':{
                cmd = "Resume";
                err = ksiEngineResume(e);
                if(err)
                        goto rt_err;
                break;
        }
        case 'i':{
                cmd = "Initialize engine";
                lptr++;
                int nprocs;
                int32_t fb,fs;
                int readcount = sscanf(lptr,"%"SCNd32",%"SCNd32",%d",&fb,&fs,&nprocs);
                CHECK_READ(3);
                if(e->nprocs){
                        if(e->playing){
                                ksiEngineStop(e);
                        }
                        paIODestroy(e);
                        ksiEngineDestroy(e);
                }
                ksiEngineInit(e, fb, fs, nprocs);
                break;
        }
        case 'a':{
                cmd = "Initialize audio";
                lptr++;
                if(*lptr!='\0'&&*lptr!='\n')
                        goto cli_err;
                if(e->playing){
                        ksiEngineStop(e);
                }
                err = paIOInit(e,&cli_err_str);
                if(err)
                        goto rt_err;
                err = ksiEngineLaunch(e);
                if(err)
                        goto rt_err;
                break;
        }
        case 'h':{
                lf(lf_args,"COMMAND LIST\n"
                      "Initialize engine (this will clear previous audio setup): i[Frames per buffer],[Frames per second],[Nprocs]\n"
                      "Initialize audio: a\n"
                      "New node: n[Output type][Built-in type]\n"
                      " Output type: n for normal and f for final output of the whole engine\n"
                      " Note: Will print the ID for the new node\n"
                      "Make wire: w[Source ID]:[Source port]>[Destination ID]:[Destination port]x[Gain]\n"
                      "Make bias: b[Node ID]:[Port]x[Bias]\n"
                      "Remove node: r[ID]\n"
                      "Remove wire: u[Source ID]:[Source port]>[Destination ID]:[Destination port]\n"
                      "Load time sequence resource file: o[file name]\n"
                      "Unload time sequence resource file: t[resource id]\n"
                      "Edit node:e[Node ID] [Node-spercified editing command]\n"
                      "Pause:P\n"
                      "Resume:R\n"
                      "Load command list from file: l[file name]\n"
                      "Set time: .[Time in frames] or -[Time in seconds]\n"
                      "Dump DAG: >\n"
                      "Quit: q\n"
                      "Help: h\n");
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
                if(consume_file(e, fp, ptrerr, pcli_err_str,lf,lf_args)){
                        fputs("In command list file: ", stderr);
                        fputs(lptr, stderr);
                        fputc('\n',stderr);
                        return 0;
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
                        if(idRet)
                                *idRet = id;
                        else
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
        lf(lf_args,"Illegal command. Type h for help.\n");
        lf(lf_args,cmd);
        if(cli_err_str){
                lf(lf_args,": ");
                lf(lf_args,cli_err_str);
                *pcli_err_str = cli_err_str;
        }
        lf(lf_args,"\n");
        *ptrerr = ksiErrorSyntax;
        return 0;
rt_err:
        lf(lf_args,cmd);
        lf(lf_args,": ");
        lf(lf_args,ksiErrorMsg(err));
        lf(lf_args,".\n");
        *ptrerr = err;
        if(cli_err_str){
                lf(lf_args,"Detail: ");
                lf(lf_args,cli_err_str);
                lf(lf_args,"\n");
                *pcli_err_str=cli_err_str;
        }
        return 0;
quit:
        return 1;
}
#define BUFFER_SIZE 64
int consume_file(KsiEngine *e,FILE *fp,KsiError *perr,const char **pcli_err_str,log_function lf,void *lf_args){
        char *line = NULL;
        size_t cap = 0;
        ssize_t len;
        int ferr = 0;
        ssize_t ln=0;
        while((len=getline(&line, &cap, fp))>0){
                ln++;
                if(consume_line(e,line, perr, pcli_err_str, NULL,lf,lf_args)){
                        break;
                }
                if(*perr){
                        ferr = 1;
                        ssize_t li = strlen(line);
                        lf(lf_args, "*** line %zd | ", ln);
                        if(li>0){
                                lf(lf_args,line);
                                if(line[li]!='\n')
                                        lf(lf_args,"\n");
                        }
                        else
                                lf(lf_args,"\n");
                        break;
                }
        }
        if(line)
        	free(line);
        return ferr;
}
