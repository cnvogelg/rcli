#ifndef LINE_H
#define LINE_H

#define LINE_RET_TIMEOUT       -2
#define LINE_RET_ERROR         -1
#define LINE_RET_OK            0

/* try to read a char and check if its a special one

   return OK if no special char was found else the char itself

   a non special char is returned so it can be read again.
   a special char is consumed.
 */
extern int line_check_char(sockio_handle_t *sio, struct MsgPort *sockio_port,
    const char *special_chars, ULONG timeout_ms);

/* read a line of text until newline is received (or eof)

   return number or chars in buffer or ERROR, TIMEOUT
*/
extern int line_read_line(sockio_handle_t *sio, struct MsgPort *sockio_port,
    APTR buf, ULONG max_size, ULONG timeout_ms);

#endif
