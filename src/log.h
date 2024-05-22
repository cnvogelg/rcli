#ifndef LOG_H
#define LOG_H

#ifdef LOG_ENABLED

#define LOG(fmt) Printf fmt

#else

#define LOG(fmt) do {} while(0)

#endif

#endif
