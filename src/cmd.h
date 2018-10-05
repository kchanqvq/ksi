#ifndef __cmd_h__
#define __cmd_h__
#include "dag.h"
#include "err.h"
#include <portaudio.h>
int consume_line(KsiEngine *e,PaStream *stream,char *line,KsiError *ptrerr,char **pcli_err_str);
int consume_file(KsiEngine *e,PaStream *stream,FILE *fp,KsiError *perr,char **pcli_err_str);
#endif
