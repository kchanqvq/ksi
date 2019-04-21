#include "err.h"
#define __err_str(x,y) y,
const char * const __err_strings[] ={__err_list(__err_str)};
#undef __err_str
