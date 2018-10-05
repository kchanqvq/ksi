#ifndef __dummy_linenoise_h__
#define __dummy_linenoise_h__
#include <stdio.h>
static inline char *linenoise(char *prompt){
        char *linep = NULL;
        size_t *linecapp;
        fputs(prompt, stdout);
        getline(&linep, &linecapp, stdin);
        return linep;
}
#endif
