/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 * Copyright (C) 2015, Leo Ma <begeekmyfriend@gmail.com>
 */

#ifndef _RBTREE_H
#define _RBTREE_H
#include <inttypes.h>
#include "data.h"
#define	RB_RED		0
#define	RB_BLACK	1
#define RB_COLORMASK 0x1
#define rbtree_red(_node)           ((_node)->type &= ~RB_COLORMASK)
#define rbtree_black(_node)         ((_node)->type |= RB_COLORMASK)
#define rbtree_is_black(_node)        ((_node)->type & RB_COLORMASK)
#define rbtree_is_red(_node)      (!rbtree_is_black(_node))
#define rbtree_copy_color(_n1, _n2) ((_n1)->type = ((_n1)->type&~RB_COLORMASK)|((_n2)->type&RB_COLORMASK))
typedef struct rbnode
{
        struct rbnode *parent;
        struct rbnode *right;
        struct rbnode *left;
        int32_t key;
        int32_t type;
        union{
                void *ptr;
                int64_t binData;
                struct{
                        int8_t tone;
                        int8_t fine;
                        int8_t velocity;
                        int8_t pan;
                        int16_t modulationX;
                        int16_t modulationY;
                } note;
                struct{
                        KsiData val;
                        int16_t curveType;
                        int16_t tension;
                } cc;
        } data;

} __attribute__((aligned(sizeof(long)))) KsiRBNode;

typedef struct rbtree {
        struct rbnode *root;     /* root node */
        struct rbnode *sentinel; /* nil node */
} KsiRBTree;

void ksiRBTreeInit(KsiRBTree *tree);
void ksiRBTreeAttach(KsiRBTree *tree,KsiRBNode* node);
KsiRBNode *ksiRBTreeNextForKey(KsiRBTree *tree,int32_t key);
KsiRBNode *ksiRBTreeNext(KsiRBTree *tree,KsiRBNode *n);
void ksiRBTreeDestroy(KsiRBTree *t);
#endif  /* _RBTREE_H */
