#ifndef __dummy_linenoise_h__
#define __dummy_linenoise_h__
#include <stdio.h>
static inline char *linenoise(const char *prompt){
        char *linep = NULL;
        size_t linecapp = 0;//just to look more safe
        fputs(prompt, stdout);
        getline(&linep, &linecapp, stdin);
        if(linecapp == 1){
                free(linep);
                linep = NULL;
        }
        return linep;
}
static inline void linenoiseFree(char *l){
        free(l);
}
#endif
