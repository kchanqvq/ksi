#include <unistd.h>
#include <sys/stat.h>
#include <portaudio.h>
#include "cmd.h"
#include "engine.h"
#include "dagedit.h"
#include "util.h"
int log_eat(void *args,const char *fmt,...){
        return 0;
}
void fifo_server(const char *ipipe_path, const char *opipe_path){
        PERROR_GUARDED("Create control FIFO",
                     mkfifo(ipipe_path, 0666));
        PERROR_GUARDED("Create return message FIFO",
                     mkfifo(opipe_path, 0666));
        int ipipe = open(ipipe_path, O_RDONLY);
        PERROR_GUARDED("Open control FIFO",
                     ipipe == -1);
        int opipe = open(opipe_path, O_WRONLY);
        PERROR_GUARDED("Open return message FIFO",
                     opipe == -1);
        char *line = NULL;
        size_t linecapp = 0;
        FILE *ipf = fdopen(ipipe, "r");
        FILE *opf = fdopen(opipe, "w");
        PERROR_GUARDED("Unlink control FIFO",
                       unlink(ipipe_path));
        PERROR_GUARDED("Unlink return message FIFO",
                       unlink(opipe_path));
        setbuf(opf, NULL);
        KsiEngine e;
        e.nprocs = 0;
        KsiError err;
        const char *errtxt;
        int32_t idRet;
        int current_in_scope = 0;
        while(1){
                char cmd;
                if(getline(&line, &linecapp, ipf) == -1)
                        break;
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
                if(consume_line(&e, line, &err, &errtxt, &idRet,log_eat,NULL)){
                        free(line);
                        break;
                }
        next:
                if(!current_in_scope)
                        ksiEngineCommit(&e);
                if(cmd == 'n'){
                        if(err)
                                idRet = 0;
                        fprintf(opf, "%d;%"PRId32"\n", err, idRet);
                }
                else{
                        fprintf(opf, "%d\n", err);
                }
                err = ksiErrorNone;
                free(line);
                line = NULL;
                linecapp = 0;
        }
        fclose(ipf);
        fclose(opf);
}
int main(int argc,char **argv){
        fifo_server("/tmp/ksid_in", "/tmp/ksid_out");
}
