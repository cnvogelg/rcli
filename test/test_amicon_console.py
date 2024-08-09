from amicon import AmiConsole, AmiConsoleBackend
from amicon.const import AmiControlChars as cc
from amicon.event import (
    TextEvent,
    CtlCharEvent,
    KeyValEvent,
    ResizeEvent,
    CharAttrEvent,
)


class MyBackend(AmiConsoleBackend):
    def __init__(self):
        self.events = []

    def handle_event(self, ev):
        self.events.append(ev)


def test_amicon_console_setup():
    be = MyBackend()
    con = AmiConsole(be)


def check(feed, ev):
    be = MyBackend()
    con = AmiConsole(be)
    con.feed_bytes(feed)
    assert be.events == ev


def test_amicon_console_resize():
    be = MyBackend()
    con = AmiConsole(be)
    con.resize(33, 42)
    assert be.events == [ResizeEvent(33, 42)]


def test_amicon_console_text():
    check("hello, world!\n", [TextEvent(b"hello, world!"), CtlCharEvent(cc.LINEFEED)])


def test_amicon_console_set_mode():
    check(
        "hi\x9b2Vwo\x9b0V\n",
        [
            TextEvent(b"hi"),
            KeyValEvent(KeyValEvent.MODE, 2),
            TextEvent(b"wo"),
            KeyValEvent(KeyValEvent.MODE, 0),
            CtlCharEvent(cc.LINEFEED),
        ],
    )


def test_amicon_console_char_attr():
    check("\x9b8m", [CharAttrEvent(attr=8)])
    check("\x9b33m", [CharAttrEvent(fg=3)])
    check("\x9b44m", [CharAttrEvent(cg=4)])
    check("\x9b>7m", [CharAttrEvent(bg=7)])
    check("\x9b8;33;44;>7m", [CharAttrEvent(attr=8, fg=3, cg=4, bg=7)])
