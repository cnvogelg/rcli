/* some tool functions for terminal IO with
   the Amiga console
*/

#ifndef TERMIO_H
#define TERMIO_H

#define ESC      "\x1b"
#define CSI      "\x9b"
#define ESC_CODE 0x1b
#define CSI_CODE 0x9b

#define BELL            "\x07"
#define BACKSPACE       "\x08"
#define HOR_TAB         "\x09"
#define LINE_FEED       "\x0a"
#define VER_TAB         "\x0b"
#define FORM_FEED       "\x0c"
#define RETURN          "\x0d"
#define SHIFT_IN        "\x0e"
#define SHIFT_OUT       "\x0f"

#define INDEX           "\x84"
#define NEXT_LINE       "\x85"
#define HOR_TAB_SET     "\x88"
#define REV_INDEX       "\x8d"

#define TERMIO_MAX_CMD_ARGS 8

#define TERMIO_OK                   0
#define TERMIO_ERR_NO_CSI           -1
#define TERMIO_ERR_TOO_MANY_ARGS    -2
#define TERMIO_ERR_INVALID_CHAR     -3
#define TERMIO_ERR_SEQ_TOO_SHORT    -4
#define TERMIO_ERR_PARAM_NO_NUMBER  -5
#define TERMIO_ERR_PARAM_EMPTY      -6

/* define a input KEY sequence */
struct termio_key_code {
  UBYTE *name;
  UBYTE *seq;
};

struct termio_cmd_seq {
  UBYTE cmd;
  UBYTE num_args;
  WORD  args[TERMIO_MAX_CMD_ARGS];
  UWORD len_bytes;
};

/* params contain the parameter types encoded as a char:
   n     integer
   f     foreground color 0-9 -> 30-39
   c     cell color 0-9 -> 40-49
   b     background color 0-7 -> >0...>7
*/
struct termio_cmd_code {
  UBYTE *name;
  UBYTE *seq;
  UBYTE *params;
};

void termio_dump_buf(const UBYTE *buf, LONG len);

struct termio_key_code *termio_find_key_code(const UBYTE *seq, LONG len);
int termio_parse_csi(const UBYTE *buf, LONG size, struct termio_cmd_seq *seq);

void termio_list_cmd_codes(void);
struct termio_cmd_code *termio_find_cmd_code(const UBYTE *name);
LONG termio_expand_cmd_code(struct termio_cmd_code *code, const UBYTE **args,
                            UBYTE *buf, LONG buf_len);

#endif
