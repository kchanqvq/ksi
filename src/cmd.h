#ifndef __cmd_h__
#define __cmd_h__
#include "dag.h"
#include "err.h"
#include "engine.h"
typedef int (*log_function) (void *args, const char * format, ...);
int consume_line(KsiEngine *e,char *line,KsiError *ptrerr,const char **pcli_err_str,
                 int32_t *idRet,log_function lf,void *lf_args);
int consume_file(KsiEngine *e,FILE *fp,KsiError *perr,const char **pcli_err_str,
                 log_function lf,void *lf_args);
#endif
