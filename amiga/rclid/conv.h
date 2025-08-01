#ifndef CONV_H
#define CONV_H

#include "buf.h"

#define NUM_SIGNALS 4
#define START_SIGNAL 3 // byte code of first signal

struct signal_info {
    int count[NUM_SIGNALS];
};
typedef struct signal_info signal_info_t;

extern LONG conv_extract_signals(buf_t *buf, signal_info_t *t);

#endif
