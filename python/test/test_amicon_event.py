from amicon.const import AmiControlChars as cc
from amicon.event import (
    TextEvent,
    CtlCharEvent,
    ParamEvent,
    CmdEvent,
    CharAttrEvent,
    ConsoleEventStream,
)


def test_amicon_event_setup():
    con = ConsoleEventStream()


def check(feed, ev):
    con = ConsoleEventStream()
    got_ev = con.feed_bytes(feed)
    assert got_ev == ev


def test_amicon_event_text():
    check("hello, world!\n", [TextEvent(b"hello, world!"), CtlCharEvent(cc.LINEFEED)])


def test_amicon_event_set_mode():
    check(
        "hi\x9b2Vwo\x9b0V\n",
        [
            TextEvent(b"hi"),
            ParamEvent(ParamEvent.MODE, 2),
            TextEvent(b"wo"),
            ParamEvent(ParamEvent.MODE, 0),
            CtlCharEvent(cc.LINEFEED),
        ],
    )


def test_amicon_event_char_attr():
    check("\x9b8m", [CharAttrEvent(attr=8)])
    check("\x9b33m", [CharAttrEvent(fg=3)])
    check("\x9b44m", [CharAttrEvent(cg=4)])
    check("\x9b>7m", [CharAttrEvent(bg=7)])
    check("\x9b8;33;44;>7m", [CharAttrEvent(attr=8, fg=3, cg=4, bg=7)])


def test_amicon_event_ctl_chars():
    check("\x07", [CtlCharEvent(cc.BELL)])
    check("\x08", [CtlCharEvent(cc.BACKSPACE)])
    check("\x09", [CtlCharEvent(cc.H_TAB)])
    check("\x0a", [CtlCharEvent(cc.LINEFEED)])
    check("\x0b", [CtlCharEvent(cc.V_TAB)])
    check("\x0c", [CtlCharEvent(cc.FORMFEED)])
    check("\x0d", [CtlCharEvent(cc.RETURN)])


def test_amicon_event_shift_in_out():
    check(
        "hello\x0eABC\x0fworld\n",
        [
            TextEvent(b"hello"),
            TextEvent(b"\xc1\xc2\xc3"),
            TextEvent(b"world"),
            CtlCharEvent(cc.LINEFEED),
        ],
    )


def test_amicon_event_ctl_chars():
    check("\x84", [CtlCharEvent(cc.INDEX)])  # INDEX
    check("\x8d", [CtlCharEvent(cc.REV_INDEX)])  # REVERSE_INDEX
    check("\x85", [CtlCharEvent(cc.LINEFEED)])  # NEXT_LINE
    check("\x88", [CmdEvent(CmdEvent.H_TAB_SET)])  # HOR_TAB_SET
