"""an Amiga console emulator"""


class ConsoleElement:
    pass


class ControlChar(ConsoleElement):
    def __init__(self, char):
        self.char = char

    def __repr__(self):
        return f"CtlChr({self.char:02x})"

    def __str__(self):
        return f"<{self.char:02x}>"

    def __eq__(self, other):
        return self.char == other.char

    def to_bytes(self, csi=False):
        return self.char


class ControlSeq(ConsoleElement):
    def __init__(self, key, *params, special=None):
        assert type(key) is str
        self.key = key
        self.params = params
        self.special = special

    def __repr__(self):
        return f"CtlSeq('{self.key}',{self.params},{self.special})"

    def __str__(self):
        res = "<CSI>" + ";".join(str(self.params)) + str(self.key)
        if self.special:
            first = len(self.params) == 0
            for key, val in enumerate(self.special):
                if not first:
                    res += ";"
                res += key + str(val)
        return res

    def __eq__(self, other):
        return self.key == other.key and self.params == other.params

    def get_param(self, pos, default=0):
        if pos < len(self.params):
            val = self.params[pos]
            if val is None:
                return default
            else:
                return val
        else:
            return default

    def to_bytes(self, csi=False):
        res = bytearray()
        if csi:
            res.append(0x9B)
        else:
            res.append(0x1B)
            res.append(ord("["))
        first = True
        if self.params:
            for p in self.params:
                if not first:
                    res.append(ord(";"))
                if p is not None:
                    res += str(p).encode("latin-1")
                first = False
        if self.special:
            if not first:
                res.append(ord(";"))
        res.append(ord(self.key))
        return bytes(res)


class Text(ConsoleElement):
    def __init__(self, txt):
        if type(txt) is str:
            txt = txt.encode("latin-1")
        self.txt = txt

    def __repr__(self):
        return f"ConTxt({self.txt})"

    def __str__(self):
        return self.txt

    def __eq__(self, other):
        return self.txt == other.txt

    def to_bytes(self, csi=False):
        return self.txt


class SeqParser:
    # characters for special params
    SPECIAL = b">"

    def __init__(self, with_csi=True):
        self.with_csi = with_csi
        self.raw_bytes = bytearray()
        self.params = []
        self.in_number = 0
        self.param_sign = 1
        self.cur_num = 0
        self.special = {}
        self.is_special = None

    def feed(self, b):
        """feed in a byte to the sequence.
        return None of List of ConsoleElements
        """
        if type(b) is str:
            b = ord(b[0])
        assert type(b) is int

        # terminate sequence
        if b >= 0x40 and b <= 0x7F:
            # complete last param?
            if self.in_number == 1:
                self._end_param()

            if len(self.params) == 0:
                seq = ControlSeq(chr(b))
            else:
                seq = ControlSeq(chr(b), *self.params)
            return [seq]

        # remember bytes if sequence gets aborted
        self.raw_bytes.append(b)

        # start of number
        if self.in_number == 0:
            if b == ord("-"):
                self.param_sign = -1
                self.in_number = 1
            elif b == ord("+"):
                self.param_sign = 1
                self.in_number = 1
            elif b >= ord("0") and b <= ord("9"):
                self.cur_num = b - ord("0")
                self.in_number = 1
            elif b == 32:
                pass
            elif b == ord(";"):
                self._end_param()
            elif b in self.SPECIAL:
                self.is_special = b
            else:
                # invalid char - abort sequence
                return self._abort()
        # in number
        else:
            if b >= ord("0") and b <= ord("9"):
                self.cur_num = self.cur_num * 10 + b - ord("0")
            elif b == 32:
                pass
            elif b == ord(";"):
                self._end_param()
            else:
                # invalid char - abort sequence
                return self._abort()

        # stay - no element yet
        return None

    def _end_param(self):
        num = self.cur_num * self.param_sign
        if self.is_special:
            self.special[self.is_special] = num
        else:
            self.params.append(num)
        self.in_number = 0
        self.cur_num = 0
        self.param_sign = 1
        self.is_special = None

    def _abort(self):
        """write out all seq bytes unmodified"""
        remainder = bytes(self.raw_bytes)
        result = []
        if self.with_csi:
            result.append(ControlChar(0x9B))
            result.append(Text(remainder))
        else:
            result.append(ControlChar(0x1B))
            result.append(Text(b"[" + remainder))
        return result


class ConsoleStream:
    def __init__(self):
        self.in_escape = 0
        self.text = bytearray()
        self.seq_parser = None

    def _flush_text(self):
        if len(self.text) > 0:
            res = Text(bytes(self.text))
            self.text = bytearray()
            return [res]

    def _add_byte(self, b):
        # is a control character
        if b < 32:
            result = []
            res = self._flush_text()
            if res:
                result += res
            result.append(ControlChar(b))
            return result
        else:
            self.text.append(b)

    def feed_bytes(self, data):
        if type(data) is str:
            data = data.encode("latin-1")
        result = []
        for b in data:
            res = self.feed(b)
            if res:
                result += res
        if len(result) != 0:
            return result

    def feed(self, b):
        """feed in bytes from application, parse it and write
        ConsoleElements to output"""
        if type(b) is str:
            b = ord(b[0])
        assert type(b) is int
        # out of escape sequence
        if self.in_escape == 0:
            # esc may start a sequence
            if b == 0x1B:
                self.in_escape = 1
            # CSI directly starts a sequence
            elif b == 0x9B:
                self.in_escape = 2
                self.seq_parser = SeqParser(True)
            else:
                res = self._add_byte(b)
                if res:
                    return res
        # a single escape was found
        elif self.in_escape == 1:
            # 7 bit CSI
            if b == ord("["):
                self.in_escape = 2
                self.seq_parser = SeqParser(False)
            else:
                # no CSI - emit ESC and data
                self.in_escape = 0
                result = [ControlChar(0x1B)]
                res = self._add_byte(b)
                if res:
                    result += res
                return result
        # in escape sequence
        else:
            # add byte to esc sequence
            res = self.seq_parser.feed(b)
            if res:
                # end or abort seq
                self.in_escape = 0
                self.seq_parser = None
                return res

    def flush(self):
        # abort a pending esc/csi sequence
        if self.seq_parser != None:
            self.seq_parser.abort()
            self.seq_parser = None
        self.in_escape = 0
        # flush out text
        return self._flush_text()
