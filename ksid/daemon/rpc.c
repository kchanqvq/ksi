#include <unistd.h>
#include <sys/stat.h>
#include <portaudio.h>
#include "cmd.h"
#include "engine.h"
#include "dagedit.h"
#include "util.h"
#ifdef __APPLE__
#include <Availability.h>
#ifdef __MAC_OS_X_VERSION_MAX_ALLOWED
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101200
#include <os/log.h>
#define log_err(...) os_log_error(OS_LOG_DEFAULT,__VA_ARGS__)
#define PRIerr "{darwin.errno}d"
#else
#include <syslog.h>
#define log_err(...) sys_log(LOG_ERR,__VA_ARGS__)
#define PRIerr "m"
#endif
#else
#include <syslog.h>
#define log_err(...) sys_log(LOG_ERR,__VA_ARGS__)
#define PRIerr "m"
#endif //__MAC_OS_X_VERSION_MAX_ALLOWED
#else //__APPLE__
#include <syslog.h>
#define log_err(...) sys_log(LOG_ERR,__VA_ARGS__)
#define PRIerr "m"
#endif
#include <errno.h>
int log_eat(void *args,const char *fmt,...){
        return 0;
}
static KsiEngineWorkerPool wp;
static inline void report_type(FILE *opf, KsiEngine *e, int32_t id){
        int epoch = atomic_load_explicit(&e->epoch, memory_order_relaxed);
        KsiNode *node = e->nodes[(1+epoch)%2].data[id];
        for(int32_t i=0;i<node->inputCount;i++){
                fputc(ksiDataTypeToCharacter(node->inputTypes[i]), opf);
        }
        fputc(';', opf);
        for(int32_t i=0;i<node->outputCount;i++){
                fputc(ksiDataTypeToCharacter(node->outputTypes[i]), opf);
        }
        fputc('\n', opf);
}
void fifo_handler(const char *ipipe_path, const char *opipe_path){
        int ipipe = open(ipipe_path, O_RDONLY);
        if(ipipe == -1){
                log_err("Cannot open control pipe at path %s. Error: %m",
                        ipipe_path, errno);
                return;
        }
        int opipe = open(opipe_path, O_WRONLY);
        if(opipe == -1){
                log_err("Cannot open return message pipe at path %s. Error: %m",
                        opipe_path, errno);
                close(ipipe);
                return;
        }
        char *line = NULL;
        size_t linecapp = 0;
        FILE *ipf = fdopen(ipipe, "r");
        FILE *opf = fdopen(opipe, "w");
        setbuf(opf, NULL);
        KsiEngine e;
        e.playing = -2;
        KsiError err;
        const char *errtxt = NULL;
        int32_t idRet;
        int current_in_scope = 0;
        while(1){
                char cmd;
                if(getline(&line, &linecapp, ipf) == -1)
                        goto ret;
                printf("%zu\n", linecapp);
                if(linecapp == 1){
                        free(line);
                        line = NULL;
                        linecapp = 0;
                        continue;
                }
                if(!line){
                        free(line);
                        break;
                }
                cmd = *line;
                if(cmd == '{'){
                        if(current_in_scope){
                                err = ksiErrorIdempotent;
                                goto next;
                        }
                        else{
                                current_in_scope = 1;
                                goto next;
                        }
                }
                else if(cmd == '}'){
                        if(current_in_scope){
                                current_in_scope = 0;
                                goto next;
                        }
                        else{
                                err = ksiErrorNotInBatch;
                                goto next;
                        }
                }
                if(consume_line(&e, &wp, line, &err, &errtxt, &idRet,log_eat,NULL)){
                        if(e.playing != -2){
                                if(e.playing)
                                        ksiEngineStop(&e);
                                ksiEngineDestroy(ksiEngineDestroyChild(&e));
                        }
                        free(line);
                        break;
                }
        next:
                {
                const char *report_err = "\0";
                if(errtxt)
                        report_err = errtxt;
                if(cmd == 'n'){
                        if(err)
                                idRet = 0;
                        fprintf(opf, "%d;%s;%"PRId32";", err, report_err, idRet);
                        if(!err){
                                report_type(opf, &e, idRet);
                        }
                        else{
                                fputs(";\n", opf);
                        }
                }
                else if(cmd == 'e'){
                        fprintf(opf, "%d;%s;", err, report_err);
                        if(!err){
                                report_type(opf, &e, idRet);
                        }
                        else{
                                fputs(";\n", opf);
                        }
                }
                else{
                        fprintf(opf, "%d;%s\n", err, report_err);
                }
                if(!current_in_scope)
                        ksiEngineCommit(&e);
                err = ksiErrorNone;
                free(line);
                line = NULL;
                linecapp = 0;
                }
        }
ret:
        fclose(ipf);
        fclose(opf);
}
static inline char *to_nul_term_string(char *s,size_t l){
        char *ret = malloc(l+1);
        memcpy(ret, s, l);
        if(ret[l-1] == '\n')
                ret[l-1] = '\0';
        else
                ret[l] = '\0';
        char *pos = strchr(ret, '\n');
        if (pos)
                *pos = '\0';
        return ret;
}
void fifo_server(const char *lpipe_path){
        int lpipe = open(lpipe_path, O_RDONLY);
        if(lpipe == -1){
                log_err("Cannot open listen pipe at path %s. Error: %m",
                        lpipe_path, errno);
                return;
        }
        FILE *lpf = fdopen(lpipe, "r");
        char *ipath = NULL;
        size_t icap = 0;
        if(getline(&ipath,&icap,lpf) == -1){
                log_err("Message on listen pipe incomplete. Control pipe path missing.");
                goto ret;
        }
        char *opath = NULL;
        size_t ocap = 0;
        if(getline(&opath,&ocap,lpf) == -1){
                log_err("Message on listen pipe incomplete. Return pipe path missing.");
                goto ret2;
        }
        char *xipath = to_nul_term_string(ipath, icap);
        char *xopath = to_nul_term_string(opath, ocap);
        fifo_handler(xipath, xopath);
        free(xipath);
        free(xopath);
        free(opath);
ret2:
        free(ipath);
ret:
        fclose(lpf);
}
const char *lpipe_path = "/tmp/ksid_listen";
void term_handler(int sig){
        unlink(lpipe_path);
        exit(0);
}
int main(int argc,char **argv){
        signal(SIGTERM,term_handler);
        PERROR_GUARDED("Create listen FIFO",
                       mkfifo(lpipe_path, 0666));
        ksiEngineWorkerPoolInit(&wp,4);
        while(1){
            fifo_server(lpipe_path);
        }
}
