class AmiControlChars:
    """define the keyboard sequences for the console"""

    BELL = 0x07
    BACKSPACE = 0x08
    HOR_TAB = 0x09
    LINEFEED = 0x0A
    VERTICAL_TAB = 0x0B
    FORMFEED = 0x0C
    RETURN = 0x0D
    SHIFT_IN = 0x0E
    SHIFT_OUT = 0x0F
    ESC = 0x1B
    INDEX = 0x84
    NEXT_LINE = 0x85
    HOR_TAB_SET = 0x88
    REVERSE_INDEX = 0x8D
    CSI = 0x9B

    valid_control_chars = (
        BELL,
        BACKSPACE,
        HOR_TAB,
        LINEFEED,
        VERTICAL_TAB,
        FORMFEED,
        RETURN,
        SHIFT_IN,
        SHIFT_OUT,
        ESC,
        INDEX,
        NEXT_LINE,
        HOR_TAB_SET,
        REVERSE_INDEX,
        CSI,
    )


class AmiKeys:
    CSI = b"\x9B"
    F1 = CSI + b"0~"
