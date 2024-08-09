from amicon.stream import SeqParser, ControlSeq, ControlChar, Text, ConsoleStream

# ----- control seq -----


def test_amicon_ctlseq_no_param():
    cs = ControlSeq("v")
    assert cs.to_bytes(csi=True) == b"\x9bv"
    assert cs.to_bytes(csi=False) == b"\x1b[v"
    assert cs.get_key() == "v"


def test_amicon_ctlseq_param():
    cs = ControlSeq("v", 1)
    assert cs.to_bytes(csi=True) == b"\x9b1v"
    assert cs.to_bytes(csi=False) == b"\x1b[1v"
    assert cs.get_param(0) == 1
    assert cs.get_param(1) == 0  # out of bounds
    assert cs.get_key() == "v"
    assert cs.get_special("<", 17) == 17


def test_amicon_ctlseq_param_two():
    cs = ControlSeq("v", 1, 42)
    assert cs.to_bytes(csi=True) == b"\x9b1;42v"
    assert cs.to_bytes(csi=False) == b"\x1b[1;42v"
    assert cs.get_param(0) == 1
    assert cs.get_param(1) == 42
    assert cs.get_key() == "v"


def test_amicon_ctlseq_param_skip():
    cs = ControlSeq("v", None, 42)
    assert cs.to_bytes(csi=True) == b"\x9b;42v"
    assert cs.to_bytes(csi=False) == b"\x1b[;42v"
    assert cs.get_param(0, 12) == 12
    assert cs.get_param(1) == 42


def test_amicon_ctlseq_param_special():
    cs = ControlSeq("v", special={">": 17})
    assert cs.to_bytes(csi=True) == b"\x9b>17v"
    assert cs.to_bytes(csi=False) == b"\x1b[>17v"
    assert cs.get_key() == "v"
    assert cs.get_special(">", 42) == 17


# ----- seqparser -----


def test_amicon_seqparser_simple_seq():
    # end sequence
    p = SeqParser()
    assert p.feed("V") == [ControlSeq("V")]


def test_amicon_seqparser_param():
    # param
    p = SeqParser()
    assert p.feed("1") is None
    assert p.feed("H") == [ControlSeq("H", 1)]


def test_amicon_seqparser_param_pos_sign():
    # param with pos sign
    p = SeqParser()
    assert p.feed("+") is None
    assert p.feed("1") is None
    assert p.feed("H") == [ControlSeq("H", 1)]


def test_amicon_seqparser_param_net_sign():
    # param with neg sign
    p = SeqParser()
    assert p.feed("-") is None
    assert p.feed("5") is None
    assert p.feed("H") == [ControlSeq("H", -5)]


def test_amicon_seqparser_param_spaces():
    # param with spaces
    p = SeqParser()
    assert p.feed(" ") is None
    assert p.feed("5") is None
    assert p.feed(" ") is None
    assert p.feed("H") == [ControlSeq("H", 5)]


def test_amicon_seqparser_param_two():
    # 2 param
    p = SeqParser()
    assert p.feed("1") is None
    assert p.feed("1") is None
    assert p.feed(";") is None
    assert p.feed("9") is None
    assert p.feed("H") == [ControlSeq("H", 11, 9)]


def test_amicon_seqparser_invalid_term():
    # invalid - aborted seq
    p = SeqParser()
    assert p.feed("?") == [ControlChar(0x9B), Text("?")]


def test_amicon_seqparser_invalid_term_param():
    # invalid with param - aborted seq
    p = SeqParser()
    assert p.feed("1") is None
    assert p.feed(" ") is None
    assert p.feed("?") == [ControlChar(0x9B), Text("1 ?")]


def test_amicon_seqparser_param_special():
    # 2 param
    p = SeqParser()
    assert p.feed(">") is None
    assert p.feed("1") is None
    assert p.feed("H") == [ControlSeq("H", special={">", 1})]


def test_amicon_seqparser_param_special_norm():
    # 2 param
    p = SeqParser()
    assert p.feed(">") is None
    assert p.feed("1") is None
    assert p.feed(";") is None
    assert p.feed("9") is None
    assert p.feed("H") == [ControlSeq("H", 9, special={">", 1})]


# ----- stream -----


def test_amicon_stream_text():
    # write some text and wait for flush
    s = ConsoleStream()
    assert s.feed_bytes("hello") is None
    assert s.flush() == [Text("hello")]


def test_amicon_stream_control_char():
    # write control char
    s = ConsoleStream()
    # return text and control char
    assert s.feed_bytes("hello\nworld") == [
        Text("hello"),
        ControlChar(0xA),
    ]
    # remaining text after flush
    assert s.flush() == [Text("world")]


def test_amicon_stream_esc_no_seq():
    s = ConsoleStream()
    # if its no seq begin then return raw seq
    assert s.feed_bytes(b"\x1ba") == [ControlChar(0x1B)]
    assert s.flush() == [Text("a")]


def test_amicon_stream_esc_8bit():
    s = ConsoleStream()
    # map ESC + 7bit -> 8bit control char
    assert s.feed_bytes(b"\x1b@") == [ControlChar(0x80)]
    assert s.feed_bytes(b"\x1b[") is None  # CSI waits for following chars


def test_amicon_stream_esc_seq():
    s = ConsoleStream()
    assert s.feed_bytes(b"\x1b[a") == [ControlSeq("a")]


def test_amicon_stream_esc_invalid():
    s = ConsoleStream()
    assert s.feed_bytes(b"\x1b[1;?") == [ControlChar(0x1B), Text("[1;?")]


def test_amicon_stream_csi_seq():
    s = ConsoleStream()
    assert s.feed_bytes(b"\x9b>4a") == [ControlSeq("a", special={">": 4})]


def test_amicon_stream_csi_seq_invalid():
    s = ConsoleStream()
    # invalid seq will be reported as ctrlchar and text
    assert s.feed_bytes(b"\x9b1;?") == [ControlChar(0x9B), Text("1;?")]


def test_amicon_stream_text_seq():
    s = ConsoleStream()
    assert s.feed_bytes(b"hello") is None
    assert s.feed_bytes(b"\x9bH") == [Text("hello"), ControlSeq("H")]
