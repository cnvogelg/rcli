#ifndef BUF_H
#define BUF_H

struct buf {
  UBYTE *data;
  LONG  size;
  LONG  capacity; // used for allocation
};
typedef struct buf buf_t;

#endif