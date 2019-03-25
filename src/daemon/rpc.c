#include <unistd.h>
#include <sys/stat.h>
#include <portaudio.h>
#include "cmd.h"
#include "engine.h"
#include "dagedit.h"
#include "util.h"
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
        KsiEngine e;
        e.nprocs = 0;
        KsiError err;
        const char *errtxt;
        int32_t idRet;
        while(1){
                getline(&line, &linecapp, ipf);
                if(linecapp == 1){
                        free(line);
                        line = NULL;
                        linecapp = 0;
                        continue;
                }
                if(!line||consume_line(&e, line, &err, &errtxt, &idRet)){
                        free(line);
                        break;
                }
                fprintf(opf, "%d\n", err);
                err = ksiErrorNone;
                ksiEngineCommit(&e);
                free(line);
        }
        fclose(ipf);
        fclose(opf);
}
int main(int argc,char **argv){
        fifo_server("/tmp/ksid_in", "/tmp/ksid_out");
}
