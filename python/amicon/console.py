import logging
from enum import Enum
from abc import ABC, abstractmethod

from amicon.event import (
    TextEvent,
    CtlCharEvent,
    ParamEvent,
    CmdEvent,
    CharAttrEvent,
    ConsoleEventStream,
)
from amicon.const import AmiControlChars as cc
from amicon.screen import Screen
from amicon.key import KeyHelper
from amicon.line import LineInput


class Mode(Enum):
    Cooked = 0
    Raw = 1
    Medium = 2


class Writer(ABC):
    """Write some data back to the user of the console"""

    @abstractmethod
    def write(data: bytes):
        """Write all bytes or raise an error"""
        pass


class Console:
    """The AmiConsole handles a virtual AmigaOS console

    You attach a screen that is reponsible for screen output
    and a writer function that can be used to write some bytes
    back to the user of the console

    Input Flow:

    User (Shell) --> feed_bytes() --> Console --> Screen
                 <--------- Writer ------+ (auto answer)

    Output Flow:

                 <--- Writer -- Console <-- press_key()
    """

    def __init__(self, screen: Screen, writer: Writer):
        self.stream = ConsoleEventStream()
        self.screen = screen
        self.writer = writer
        self.mode = Mode.Cooked
        self.line_input = LineInput(screen)

    def get_input(self):
        return self.input

    def resize(self, w, h):
        logging.debug("resize w=%d h=%d", w, h)

    def press_key(self, key_code):
        if type(key_code) is int:
            key_code = bytes([key_code])
        logging.debug("press key: %s", key_code)
        if self.mode == Mode.Cooked:
            done = self.line_input.press_key(key_code)
            if done:
                line = self.line_input.get_line()
                self.writer.write(line)
        else:
            self.writer.write(key_code)

    def feed_bytes(self, data):
        """feed in bytes from the user of the console"""
        logging.debug("feed bytes: %r", data)
        events = self.stream.feed_bytes(data)
        if events:
            any_update = False
            for ev in events:
                update = False
                logging.debug("got event: %r", ev)
                tev = type(ev)
                if tev is TextEvent:
                    # write text to console
                    update = self._handle_text(ev.txt)
                elif tev is CtlCharEvent:
                    update = self._handle_ctl_char(ev.char)
                elif tev is ParamEvent:
                    update = self._handle_param(ev.key, ev.val)
                elif tev is CmdEvent:
                    update = self._handle_cmd(ev.cmd)
                elif tev is CharAttrEvent:
                    updata = self._handle_char_attr(ev)
                else:
                    logging.error("invalid event: %r", ev)
                if update:
                    any_update = True
            if any_update:
                self.screen.refresh()

    def _handle_text(self, txt):
        logging.debug("feed text: %s", txt)
        self.screen.write_text(txt)
        return True

    def _handle_ctl_char(self, ch):
        logging.debug("feed ctl char: %s", ch)
        return False

    def _handle_param(self, key, val):
        logging.debug("feed param: key=%s, val=%s", key, val)
        if key == ParamEvent.MODE:
            mode = Mode(val)
            self._set_mode(mode)
        return False

    def _handle_cmd(self, cmd):
        logging.debug("feed cmd: %s", cmd)
        return False

    def _handle_char_attr(self, attr):
        logging.debug("feed attr: %s", attr)
        return False

    def _set_mode(self, mode):
        logging.debug("set mode: %s", mode)
        # map Medium to Cooked until correctly supported
        if mode == Mode.Medium:
            mode = Mode.Cooked
        self.mode = mode
        self.line_input.reset()
