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


class ResizeEvent(ConsoleEvent):
    def __init__(self, w, h):
        self.w = w
        self.h = h

    def __repr__(self):
        return f"Resize({self.w}, {self.h})"

    def __eq__(self, other):
        return self.w == other.w and self.h == other.h


class MoveCursorEvent(ConsoleEvent):
    def __init__(self, dx, dy):
        self.dx = dx
        self.dy = dy

    def __repr__(self):
        return f"MoveCursor({self.dx}, {self.dy})"

    def __eq__(self, other):
        return self.dx == other.dx and self.dy == other.dy
