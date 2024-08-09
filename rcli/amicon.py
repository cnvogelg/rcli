from abc import ABC, abstractmethod
import logging

from rcli.console import ConsoleStream, Text, ControlSeq, ControlChar
from rcli.event import (
    TextEvent,
    CtlCharEvent,
    KeyValEvent,
    ResizeEvent,
    CharAttrEvent,
)
from rcli.const import AmiControlChars as cc


class AmiConsoleBackend(ABC):
    @abstractmethod
    def handle_event(self, ev):
        pass


class AmiConsole:
    def __init__(self, backend: AmiConsoleBackend):
        self.stream = ConsoleStream()
        self.backend = backend

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
                    out_ev = TextEvent(ev.txt)
                elif type(ev) is ControlChar:
                    out_ev = self._handle_ctl_char(ev.char)
                elif type(ev) is ControlSeq:
                    out_ev = self._handle_ctl_seq(ev)
                else:
                    logging.error("invalid event: %r", ev)
                if out_ev:
                    logging.debug("out event: %r", out_ev)
                    self.backend.handle_event(out_ev)

    def _handle_ctl_char(self, char):
        if char in cc.valid_control_chars:
            return CtlCharEvent(char)
        else:
            logging.info("unknown char: %r", char)

    def _handle_ctl_seq(self, seq):
        key = seq.get_key()
        if key == "V":  # our custom set mode command
            mode = seq.get_param(0)
            logging.debug("set mode: %d", mode)
            return KeyValEvent(KeyValEvent.MODE, mode)
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
