#include "mvcc_utils.h"
int main(void){
	KsiSPSCPtrList t;
	ksiSPSCPtrListInit(&t);
	printf("%zu\n", (size_t)ksiSPSCPtrListDequeue(&t));
	printf("%zu\n", (size_t)ksiSPSCPtrListDequeue(&t));
	ksiSPSCPtrListEnqueue(&t, (void*)10UL);
	ksiSPSCPtrListEnqueue(&t, (void*)10UL);
	ksiSPSCPtrListEnqueue(&t, (void*)20UL);
	printf("%zu\n", (size_t)ksiSPSCPtrListDequeue(&t));
	printf("%zu\n", (size_t)ksiSPSCPtrListDequeue(&t));
	printf("%zu\n", (size_t)ksiSPSCPtrListDequeue(&t));
	printf("%zu\n", (size_t)ksiSPSCPtrListDequeue(&t));
	ksiSPSCPtrListEnqueue(&t, (void*)10UL);
	printf("%zu\n", (size_t)ksiSPSCPtrListDequeue(&t));
	return 0;
}
