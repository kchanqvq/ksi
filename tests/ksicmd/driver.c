#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <libgen.h>
int main(int argc, char **argv){
        assert(argc == 3);
        FILE *target_stdio = popen(argv[1], "w");
        FILE *input_file = fopen(argv[2],"r");
        char *wd = dirname(argv[2]);
        size_t len;
        char *line = fgetln(input_file, &len);
        while(len){
                if(*line == 'l'){
                        line ++;
                        fputc('l', target_stdio);
                        fputs(wd, target_stdio);
                        fputc('/', target_stdio);
                        fwrite(line, 1, len - 1, target_stdio);
                        if(!(line[len-2] == '\n'))
                        fputc('\n',target_stdio);
                }
                else{
                        fwrite(line, 1, len, target_stdio);
                        if(!(line[len-1] == '\n'))
                                fputc('\n',target_stdio);
                }
                line = fgetln(input_file, &len);
        }
        int ret = pclose(target_stdio);
        printf("ret %d\n", ret);
        assert(ret == 0);
        return 0;
}
