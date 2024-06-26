#ifndef BUF_H
#define BUF_H

struct buf {
  UBYTE *data;
  LONG  size;
};
typedef struct buf buf_t;

#endif