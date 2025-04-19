from abc import ABC, abstractmethod
import logging

from amicon.stream import ConsoleStream, Text, ControlSeq, ControlChar
from amicon.event import (
    TextEvent,
    CtlCharEvent,
    ParamEvent,
    CmdEvent,
    ResizeEvent,
    CharAttrEvent,
    MoveCursorEvent,
)
from amicon.const import AmiControlChars as cc


class AmiConsoleBackend(ABC):
    @abstractmethod
    def handle_event(self, ev):
        pass


class AmiConsole:
    def __init__(self, backend: AmiConsoleBackend):
        self.stream = ConsoleStream()
        self.backend = backend
        self.shifted = False

    def resize(self, w, h):
        logging.debug("resize w=%d h=%d", w, h)
        self.backend.handle_event(ResizeEvent(w, h))

    def feed_bytes(self, data):
        logging.debug("feed bytes: %r", data)
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
                    self.backend.handle_event(out_ev)

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
        elif char == cc.INDEX:
            return MoveCursorEvent(0, 1)
        elif char == cc.REV_INDEX:
            return MoveCursorEvent(0, -1)
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
