#ifndef LOG_H
#define LOG_H

#ifdef DEBUG

#include <proto/dos.h>

#define LOG(fmt) Printf fmt

#else

#define LOG(fmt) do {} while(0)

#endif

#endif
