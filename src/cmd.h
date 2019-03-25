#ifndef __cmd_h__
#define __cmd_h__
#include "dag.h"
#include "err.h"
#include "engine.h"
int consume_line(KsiEngine *e,char *line,KsiError *ptrerr,const char **pcli_err_str,int32_t *idRet);
int consume_file(KsiEngine *e,FILE *fp,KsiError *perr,const char **pcli_err_str);
#endif
