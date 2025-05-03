import logging

from .const import AmiControlChars as cc
from .stream import ConsoleStream, Text, ControlChar, ControlSeq


class ConsoleEvent:
    pass


class TextEvent(ConsoleEvent):
    def __init__(self, txt):
        self.txt = txt

    def __repr__(self):
        return f"Text({self.txt})"

    def __eq__(self, other):
        return self.txt == other.txt

    def raw(self):
        return self.txt


class CtlCharEvent(ConsoleEvent):
    def __init__(self, char):
        self.char = char

    def __repr__(self):
        return f"CtlChar({self.char:02x})"

    def __eq__(self, other):
        return self.char == other.char

    def raw(self):
        return bytes([self.char])


class ParamEvent(ConsoleEvent):
    MODE = 0

    def __init__(self, key, val):
        self.key = key
        self.val = val

    def __repr__(self):
        return f"Param({self.key}, {self.val})"

    def __eq__(self, other):
        return self.key == other.key and self.val == other.val

    def raw(self):
        return None


class CmdEvent(ConsoleEvent):
    H_TAB_SET = 0

    def __init__(self, cmd):
        self.cmd = cmd

    def __repr__(self):
        return f"Cmd({self.cmd})"

    def __eq__(self, other):
        return self.cmd == other.cmd


class CharAttrEvent(ConsoleEvent):
    def __init__(self, attr=None, fg=None, cg=None, bg=None):
        self.attr = attr
        self.fg = fg
        self.cg = cg
        self.bg = bg

    def __repr__(self):
        return f"CharAttr({self.attr}, {self.fg}, {self.cg}, {self.bg})"

    def __eq__(self, other):
        return (
            self.attr == other.attr
            and self.fg == other.fg
            and self.cg == other.cg
            and self.bg == other.bg
        )


class ConsoleEventStream:
    def __init__(self):
        self.stream = ConsoleStream()
        self.shifted = False

    def feed_bytes(self, data):
        """feed bytes and return console events or None"""
        result = []
        events = self.stream.feed_bytes(data)
        if events:
            for ev in events:
                out_ev = None
                logging.debug("got event: %r", ev)
                if type(ev) is Text:
                    if self.shifted:
                        txt = self._shift_txt(ev.txt)
                    else:
                        txt = ev.txt
                    out_ev = TextEvent(txt)
                elif type(ev) is ControlChar:
                    logging.debug("control char: %02x", ev.char)
                    out_ev = self._handle_ctl_char(ev.char)
                elif type(ev) is ControlSeq:
                    out_ev = self._handle_ctl_seq(ev)
                else:
                    logging.error("invalid event: %r", ev)
                if out_ev:
                    logging.debug("out event: %r", out_ev)
                    result.append(out_ev)
        if len(result) > 0:
            logging.debug(f"feed return {len(result)} events")
            return result

    def _shift_txt(self, txt):
        b = bytearray()
        for t in txt:
            if t < 0x80:
                b.append(t + 0x80)
            else:
                b.append(t)
        return bytes(b)

    def _handle_ctl_char(self, char):
        if char == cc.SHIFT_IN:
            self.shifted = True
        elif char == cc.SHIFT_OUT:
            self.shifted = False
        elif char == cc.NEXT_LINE:
            return CtlCharEvent(cc.LINEFEED)
        elif char == cc.H_TAB_SET:
            return CmdEvent(CmdEvent.H_TAB_SET)
        elif char in cc.valid_control_chars:
            return CtlCharEvent(char)
        else:
            logging.info("unknown char: %r", char)

    def _handle_ctl_seq(self, seq):
        key = seq.get_key()
        if key == "V":  # our custom set mode command
            mode = seq.get_param(0)
            logging.debug("set mode: %d", mode)
            return ParamEvent(ParamEvent.MODE, mode)
        elif key == "m":  # char attributes
            logging.debug("raw char attr: %r", seq)
            return self._handle_char_attr(
                seq.get_all_params(None), seq.get_special(">", None)
            )
        else:
            logging.error("invalid seq: %r", seq)

    def _handle_char_attr(self, params, back_col):
        attr = None
        fore_col = None
        cell_col = None
        for p in params:
            if p < 30:
                attr = p
            elif p < 40:
                fore_col = p - 30
            elif p < 50:
                cell_col = p - 40
        return CharAttrEvent(attr, fore_col, cell_col, back_col)
