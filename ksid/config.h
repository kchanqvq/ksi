#ifndef config_h
#define config_h
static inline int get_ncpus(){
        unsigned int eax=11,ebx=0,ecx=1,edx=0;
        __asm__ volatile("cpuid"
                     : "=a" (eax),
                       "=b" (ebx),
                       "=c" (ecx),
                       "=d" (edx)
                     : "0" (eax), "2" (ecx)
                     : );
        return eax;
}
#endif
