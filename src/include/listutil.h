#ifndef LISTUTIL_H
#define LISTUTIL_H

#define GetHead(list) (((list) && (list)->lh_Head && (list)->lh_Head->ln_Succ) \
        ? (list)->lh_Head : (struct Node *)NULL)
#define GetSucc(node) (((node) && (node)->ln_Succ && (node)->ln_Succ->ln_Succ) \
        ? (node)->ln_Succ : (struct Node *)NULL)

#endif
