#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#ifdef DEBUG_CONV
#define DEBUG
#endif

#include "conv.h"
#include "log.h"

LONG conv_extract_signals(buf_t *buf, signal_info_t *sig_info)
{
  BYTE *src = buf->data;
  BYTE *dst = src;
  LONG  size = buf->size;
  BOOL  copy = FALSE;
  LONG  out_size = size;
  LONG  changes = 0;

  // reset signal count
  for(int s=0;s<NUM_SIGNALS;s++) {
    sig_info->count[s] = 0;
  }

  // run through buffer extract signals and copy remaining bytes
  LOG(("conv: size=%ld\n", size));
  for(LONG i=0;i<size;i++) {
    BYTE ch = *src++;
    // is a control signal?
    if((ch >= START_SIGNAL) && (ch < (START_SIGNAL + NUM_SIGNALS))) {
      WORD signal = ch - START_SIGNAL;
      sig_info->count[signal]++;
      copy = TRUE;
      out_size--;
      changes++;
    } else {
      if(copy) {
        *dst = ch;
      }
      dst++;
    }
  }

  // update buffer
  if(copy) {
    buf->size = out_size;
  }

  // correct size
  return changes;
}
