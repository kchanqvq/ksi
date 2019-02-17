#include "resource.h"
#include <inttypes.h>
#define CHECK_RC(n) if(rc-n){err=ksiErrorSyntax;goto jerr;}
KsiError ksiTimeSeqLoadFromTextFile(KsiRBTree *tree,FILE *fp,int32_t framesPerSecond){
        ksiRBTreeInit(tree);
        char *line=NULL;
        size_t cap=0;
        ssize_t len;
        int16_t barpm=0;
        KsiError err = ksiErrorNone;
        if((len=getline(&line, &cap, fp))>0){
                int rc = sscanf(line,"%"SCNd16"\n",&barpm);
                CHECK_RC(1);
        }
        while((len=getline(&line, &cap, fp))>0){
                int16_t bar,div,num;
                int8_t note,vel;
                int rc = sscanf(line, "%"SCNd16" %"SCNd16" %"SCNd16" %"SCNd8" %"SCNd8"\n",&bar,&div,&num,&note,&vel);//BAR DIVIDE NUM NOTE VELOCITY
                //TODO: last line boundary condition
                CHECK_RC(5);
                int32_t time;
                int32_t fpb = (framesPerSecond*60/barpm);
                time = (bar/barpm)*framesPerSecond*60+bar%barpm*fpb+fpb*num/div;
                KsiRBNode *n = (KsiRBNode *)malloc(sizeof(KsiRBNode));
                n->key = time;
                n->data.note.tone = note;
                n->data.note.velocity = vel;
                ksiRBTreeAttach(tree, n);
        }
jerr:
        if(line)
                free(line);
        return err;
}
KsiError ksiTimeSeqLoadToEngineFromTextFilePath(KsiEngine *e,const char * restrict path,int32_t *id){
        KsiRBTree *n = (KsiRBTree *)malloc(sizeof(KsiRBTree));
        FILE *fp = fopen(path, "r");
        if(!fp)
                return ksiErrorFileSystem;
        KsiError err = ksiTimeSeqLoadFromTextFile(n, fp, e->framesPerSecond);
        if(!err){
                *id = ksiVecInsert(&e->timeseqResources, n);
        }
        fclose(fp);
        return err;
}
KsiError ksiTimeSeqUnloadFromEngine(KsiEngine *e,int32_t id){
        if(id>=e->timeseqResources.size||!e->timeseqResources.data[id]){
                return ksiErrorResIdNotFound;
        }
        KsiRBTree *n = (KsiRBTree *)e->timeseqResources.data[id];
        ksiVecDelete(&e->timeseqResources, id);
        ksiRBTreeDestroy(n);
        free(n);
        return ksiErrorNone;
}
