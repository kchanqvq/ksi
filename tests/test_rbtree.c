#include "rbtree.h"
#include <stdio.h>
#include <stdlib.h>
int main(){
        KsiRBNode *root = NULL;
        KsiRBNode *current = (KsiRBNode *)malloc(sizeof(KsiRBNode));
        current->key = 1;
        current->data.note.tone = 64;
        ksiRBNodeAttach(&root, current);
        current = (KsiRBNode *)malloc(sizeof(KsiRBNode));
        current->key = 3;
        current->data.note.tone = 32;
        ksiRBNodeAttach(&root, current);
        current = ksiRBNodeNextForKey(&root, 0);
        current = (KsiRBNode *)malloc(sizeof(KsiRBNode));
        current->key = 2;
        current->data.note.tone = 37;
        ksiRBNodeAttach(&root, current);
        current = ksiRBNodeNextForKey(&root, 0);
        current = (KsiRBNode *)malloc(sizeof(KsiRBNode));
        current->key = 7;
        current->data.note.tone = 14;
        ksiRBNodeAttach(&root, current);
        current = ksiRBNodeNextForKey(&root, 0);
        current = (KsiRBNode *)malloc(sizeof(KsiRBNode));
        current->key = 5;
        current->data.note.tone = 52;
        ksiRBNodeAttach(&root, current);
        current = ksiRBNodeNextForKey(&root, 3);
        printf("Got %d\n", current->data.note.tone);
}
