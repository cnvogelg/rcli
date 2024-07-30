/* a tget tool to parse console control codes */

#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>

#include <string.h>

#include "termio.h"

/* list of key codes */
static struct termio_key_code key_codes[] = {
  { "F1", CSI "0~" },
  { "F2", CSI "1~" },
  { "F3", CSI "2~" },
  { "F4", CSI "3~" },
  { "F5", CSI "4~" },
  { "F6", CSI "5~" },
  { "F7", CSI "6~" },
  { "F8", CSI "7~" },
  { "F9", CSI "8~" },
  { "F10", CSI "9~" },
  { "F11", CSI "20~" },
  { "F12", CSI "21~" },

  { "Shift+F1", CSI "10~" },
  { "Shift+F2", CSI "11~" },
  { "Shift+F3", CSI "12~" },
  { "Shift+F4", CSI "13~" },
  { "Shift+F5", CSI "14~" },
  { "Shift+F6", CSI "15~" },
  { "Shift+F7", CSI "16~" },
  { "Shift+F8", CSI "17~" },
  { "Shift+F9", CSI "18~" },
  { "Shift+F10", CSI "19~" },
  { "Shift+F11", CSI "30~" },
  { "Shift+F12", CSI "31~" },

  { "HELP", CSI "?~" },

  { "Insert",      CSI "40~" },
  { "Page Up",     CSI "41~" },
  { "Page Down",   CSI "42~" },
  { "Pause/Break", CSI "43~" },
  { "Home",        CSI "44~" },
  { "End",         CSI "45~" },

  { "Shift+Insert",      CSI "50~" },
  { "Shift+Page Up",     CSI "51~" },
  { "Shift+Page Down",   CSI "52~" },
  { "Shift+Pause/Break", CSI "53~" },
  { "Shift+Home",        CSI "54~" },
  { "Shift+End",         CSI "55~" },

  { "Up",     CSI "A" },
  { "Down",   CSI "B" },
  { "Right",  CSI "C" },
  { "Left",   CSI "D" },

  { "Shift+Up",     CSI "A" },
  { "Shift+Down",   CSI "B" },
  { "Shift+Right",  CSI "C" },
  { "Shift+Left",   CSI "D" },

  { NULL, NULL }
};

/* list of console codes */
static struct termio_cmd_code cmd_codes[] = {
  { "bell",         BELL, NULL },
  { "backspace",    BACKSPACE, NULL },
  { "hor_tab",      HOR_TAB, NULL },
  { "line_feed",    LINE_FEED, NULL },
  { "ver_tab",      VER_TAB, NULL },
  { "form_feed",    FORM_FEED, NULL },
  { "return",       RETURN, NULL },
  { "shift_in",     SHIFT_IN, NULL },
  { "shift_out",    SHIFT_OUT, NULL },

  { "index",        INDEX, NULL },
  { "next_line",    NEXT_LINE, NULL },
  { "hor_tab_set",  HOR_TAB_SET, NULL },
  { "rev_index",    REV_INDEX, NULL },

  { "esc",          ESC, NULL },
  { "csi",          CSI, NULL },

  { "raw",          "", "s" },
  { "reset",        ESC "c", NULL },

  { "insert",       CSI "@", "n" },
  { "up",           CSI "A", "n" },
  { "down",         CSI "B", "n" },
  { "forward",      CSI "C", "n" },
  { "backward",     CSI "D", "n" },
  { "next_line",    CSI "E", "n" },
  { "prev_line",    CSI "F", "n" },

  { "set_cursor",   CSI "H", "nn" },
  { "hor_tab",      CSI "I", "n" },
  { "erase_screen", CSI "J", NULL },
  { "erase_line",   CSI "K", NULL },
  { "insert_line",  CSI "L", NULL },
  { "delete_line",  CSI "M", NULL },

  { "delete",       CSI "P", "n" },

  { "scroll_up",    CSI "S", "n" },
  { "scroll_down",  CSI "T", "n" },

  { "tab_control",  CSI "W", "n" },
  { "back_tab",     CSI "Z", "n" },

  { "get_cursor",   CSI "6n", NULL },
  { "crlf",         CSI "20h", NULL },
  { "lf",           CSI "20l", NULL },

  { "mode",         CSI "m", "n" },
  { "fg",           CSI "m", "f" },
  { "cc",           CSI "m", "c" },
  { "bg",           CSI "m", "b" },
  { "attr",         CSI "m", "nfcb" },

  // Amiga specific
  { "scroll_on",    CSI ">1h", NULL },
  { "scroll_off",   CSI ">1l", NULL },
  { "wrap_on",      CSI "?1h", NULL },
  { "wrap_off",     CSI "?1l", NULL },
  { "cursor_on",    CSI " p", NULL },
  { "cursor_off",   CSI "0 p", NULL },

  { "get_size",     CSI "0 q", NULL },
  { "paste",        CSI "0 v", NULL },

  { NULL, NULL, NULL }
};

struct termio_key_code *termio_find_key_code(const UBYTE *seq, LONG len)
{
  struct termio_key_code *code = &key_codes[0];
  while(code->name != NULL) {
    if(strncmp(seq, code->seq, strlen(code->seq)) == 0) {
      return code;
    }
    code++;
  }
  return NULL;
}

void termio_dump_buf(const UBYTE *buf, LONG len)
{
  // hex
  for(LONG i=0;i<len;i++) {
    Printf("%02lx ", (LONG)buf[i]);
  }
  PutStr("\n");

  // decoded string
  for(LONG i=0;i<len;i++) {
    LONG data = buf[i];
    if(data == ESC_CODE) {
      PutStr("ESC ");
    } else if(data == CSI_CODE) {
      PutStr("CSI ");
    } else if(data < 32) {
      Printf("%02lx ", data);
    } else {
      Printf("%lc ", data);
    }
  }
  PutStr("\n");
}

int termio_parse_csi(const UBYTE *buf, LONG size, struct termio_cmd_seq *seq)
{
  LONG orig_size = size;

  /* starts with CSI */
  if(*buf == CSI_CODE) {
    size--;
    buf++;
  }
  /* starts with ESC + '[' */
  else if(*buf == ESC_CODE) {
    buf++;
    size--;
    if(*buf == '[') {
      buf++;
      size--;
    } else {
      return -1;
    }
  }
  else {
    return TERMIO_ERR_NO_CSI;
  }

  seq->num_args = 0;
  UWORD cur_param = 0;
  BOOL in_param = FALSE;

  while(size > 0) {
    UBYTE ch = *buf;
    // is seq terminator?
    BOOL is_cmd = (ch >= 0x40) && (ch <= 0x7f);
    BOOL is_sep = (ch == ';');
    BOOL is_num = (ch >= '0') && (ch <= '9');

    // end param?
    if((is_cmd || (is_sep)) && in_param) {
      // too many args..
      if(seq->num_args == TERMIO_MAX_CMD_ARGS) {
        return TERMIO_ERR_TOO_MANY_ARGS;
      }
      // store param
      seq->args[seq->num_args] = cur_param;
      seq->num_args ++;
      in_param = FALSE;
    }

    // cmd ends scan
    if(is_cmd) {
      seq->cmd = ch;
      UWORD len = orig_size - size;
      seq->len_bytes = len;
      return len;
    }
    // parsing a number
    else if(is_num) {
      if(!in_param) {
        in_param = TRUE;
        cur_param = 0;
      } else {
        cur_param *= 10;
      }
      cur_param += (ch - '0');
    }
    // other chars are invalid
    else if(!is_sep && (ch != ' ')) {
      // invalid char
      return TERMIO_ERR_INVALID_CHAR;
    }

    buf++;
    size--;
  }

  return TERMIO_ERR_SEQ_TOO_SHORT;
}

void termio_list_cmd_codes(void)
{
  struct termio_cmd_code *code = &cmd_codes[0];
  while(code->name != NULL) {
    PutStr(code->name);
    PutStr("\n");
    code++;
  }
}

struct termio_cmd_code *termio_find_cmd_code(const UBYTE *name)
{
  struct termio_cmd_code *code = &cmd_codes[0];
  while(code->name != NULL) {
    if(strcmp(name, code->name) == 0) {
      return code;
    }
    code++;
  }
  return NULL;
}

static LONG get_digit(const UBYTE *buf)
{
  UBYTE in = *buf;
  if(in < '0' || in > '9') {
    return TERMIO_ERR_PARAM_NO_NUMBER;
  }
  return (LONG)(in - '0');
}

static LONG expand_param(UBYTE code, const UBYTE *input, UBYTE *output, LONG out_len)
{
  LONG res;

  if(*input == 0) {
    return TERMIO_ERR_PARAM_EMPTY;
  }

  // foreground
  if(code == 'f') {
    res = get_digit(input);
    if(res < 0) {
      return res;
    }
    *output++ = '3';
    *output = '0' + res;
    res = 2;
  }
  // foreground
  else if(code == 'c') {
    res = get_digit(input);
    if(res < 0) {
      return res;
    }
    *output++ = '4';
    *output = '0' + res;
    res = 2;
  }
  // background
  else if(code == 'b') {
    res = get_digit(input);
    if(res < 0) {
      return res;
    }
    *output++ = '>';
    *output = '0' + res;
    res = 2;
  }
  else {
    res = 0;
    while(*input && (res < out_len)) {
      if(code == 'n') {
        LONG digit = get_digit(input);
        if(digit < 0) {
          return digit;
        }
        *output++ = '0' + digit;
        input++;
      }
      else {
        *output++ = *input++;
      }
      res++;
    }
  }

  return res;
}

LONG termio_expand_cmd_code(struct termio_cmd_code *code, const UBYTE **args,
                            UBYTE *buf, LONG buf_len)
{
  LONG len = 0;
  const UBYTE *seq = code->seq;

  /* copy CSI prefix of code */
  if(*seq == CSI_CODE) {
    *buf++ = CSI_CODE;
    seq++;
    len++;
  }

  /* go through args */
  const UBYTE *params = code->params;
  BOOL first = TRUE;
  while(*args != NULL) {
    UBYTE param_code = *params;
    /* no more params - abort */
    if(param_code == 0) {
      break;
    }

    /* add separator */
    if(!first) {
      *buf++ = ';';
      len++;
    }

    /* expand single param */
    LONG n = expand_param(param_code, *args, buf, buf_len - len);
    if(n < 0) {
      return n;
    }
    len += n;
    buf += n;

    args++;
    params++;
    first = FALSE;
  }

  /* copy final prefix */
  while(*seq && (len < buf_len)) {
    *buf++ = *seq++;
    len++;
  }
  *buf = 0;
  return len;
}


