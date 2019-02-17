#include <stdio.h>
#include "cache.h"
int main(int argc, const char **argv){
        FILE *output = fopen(argv[1], "w");
        fputs("#ifndef _CONFIG\n"
              "#define _CONFIG\n", output);
        fprintf(output, "#define _CONFIG_CACHE_SIZE %zu\n", cache_line_size());
        fputs("#endif",output);
        return 0;
}
