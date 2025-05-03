class AmiControlChars:
    """define the keyboard sequences for the console"""

    BELL = 0x07
    BACKSPACE = 0x08
    H_TAB = 0x09
    LINEFEED = 0x0A
    V_TAB = 0x0B
    FORMFEED = 0x0C
    RETURN = 0x0D
    SHIFT_IN = 0x0E
    SHIFT_OUT = 0x0F
    ESC = 0x1B
    INDEX = 0x84
    NEXT_LINE = 0x85
    H_TAB_SET = 0x88
    REV_INDEX = 0x8D
    CSI = 0x9B

    valid_control_chars = (
        BELL,
        BACKSPACE,
        H_TAB,
        LINEFEED,
        V_TAB,
        FORMFEED,
        RETURN,
        SHIFT_IN,
        SHIFT_OUT,
        ESC,
        INDEX,
        NEXT_LINE,
        H_TAB_SET,
        REV_INDEX,
        CSI,
    )


CSI = b"\x9B"


class AmiSpecialKeys:
    F1 = CSI + b"0~"
    F2 = CSI + b"1~"
    F3 = CSI + b"2~"
    F4 = CSI + b"3~"
    F5 = CSI + b"4~"
    F6 = CSI + b"5~"
    F7 = CSI + b"6~"
    F8 = CSI + b"7~"
    F9 = CSI + b"8~"
    F10 = CSI + b"9~"
    F11 = CSI + b"20~"
    F12 = CSI + b"21~"

    HELP = CSI + b"?~"

    INSERT = CSI + b"40~"
    PAGE_UP = CSI + b"41~"
    PAGE_DOWN = CSI + b"42~"
    PAUSE_BREAK = CSI + b"43~"
    HOME = CSI + b"44~"
    END = CSI + b"45~"

    UP = CSI + b"A"
    DOWN = CSI + b"B"
    RIGHT = CSI + b"C"
    LEFT = CSI + b"D"

    all_keys = [
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        HELP,
        INSERT,
        PAGE_UP,
        PAGE_DOWN,
        PAUSE_BREAK,
        HOME,
        END,
        UP,
        DOWN,
        RIGHT,
        LEFT,
    ]


class AmiSpecialKeysShifted:
    F1 = CSI + b"10~"
    F2 = CSI + b"11~"
    F3 = CSI + b"12~"
    F4 = CSI + b"13~"
    F5 = CSI + b"14~"
    F6 = CSI + b"15~"
    F7 = CSI + b"16~"
    F8 = CSI + b"17~"
    F9 = CSI + b"18~"
    F10 = CSI + b"19~"
    F11 = CSI + b"30~"
    F12 = CSI + b"31~"

    INSERT = CSI + b"50~"
    PAGE_UP = CSI + b"51~"
    PAGE_DOWN = CSI + b"52~"
    PAUSE_BREAK = CSI + b"53~"
    HOME = CSI + b"54~"
    END = CSI + b"55~"

    UP = CSI + b"T"
    DOWN = CSI + b"S"
    RIGHT = CSI + b" @"
    LEFT = CSI + b" A"

    all_keys = [
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        INSERT,
        PAGE_UP,
        PAGE_DOWN,
        PAUSE_BREAK,
        HOME,
        END,
        UP,
        DOWN,
        RIGHT,
        LEFT,
    ]
