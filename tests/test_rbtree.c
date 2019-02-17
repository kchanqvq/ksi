#include "rbtree.h"
#include <stdio.h>
#include <stdlib.h>
int main(){
        KsiRBTree root;
        ksiRBTreeInit(&root);
        KsiRBNode *current = malloc(sizeof(KsiRBNode));
        current->key = 1;
        current->data.note.tone = 64;
        ksiRBTreeAttach(&root, current);
        current = malloc(sizeof(KsiRBNode));
        current->key = 3;
        current->data.note.tone = 32;
        ksiRBTreeAttach(&root, current);
        current = ksiRBTreeNextForKey(&root, 0);
        current = malloc(sizeof(KsiRBNode));
        current->key = 2;
        current->data.note.tone = 37;
        ksiRBTreeAttach(&root, current);
        current = ksiRBTreeNextForKey(&root, 0);
        current = malloc(sizeof(KsiRBNode));
        current->key = 7;
        current->data.note.tone = 14;
        ksiRBTreeAttach(&root, current);
        current = ksiRBTreeNextForKey(&root, 0);
        current = malloc(sizeof(KsiRBNode));
        current->key = 5;
        current->data.note.tone = 52;
        ksiRBTreeAttach(&root, current);
        current = ksiRBTreeNextForKey(&root, 3);
        printf("Got %d\n", current->data.note.tone);
}
