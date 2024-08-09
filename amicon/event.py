class ConsoleEvent:
    pass


class TextEvent(ConsoleEvent):
    def __init__(self, txt):
        self.txt = txt

    def __repr__(self):
        return f"Text({self.txt})"

    def __eq__(self, other):
        return self.txt == other.txt


class CtlCharEvent(ConsoleEvent):
    def __init__(self, char):
        self.char = char

    def __repr__(self):
        return f"CtlChar({self.char})"

    def __eq__(self, other):
        return self.char == other.char


class KeyValEvent(ConsoleEvent):
    MODE = 0

    def __init__(self, key, val):
        self.key = key
        self.val = val

    def __repr__(self):
        return f"KeyVal({self.key}, {self.val})"

    def __eq__(self, other):
        return self.key == other.key and self.val == other.val


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
