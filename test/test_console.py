from rcli.console import SeqParser, ControlSeq, ControlChar, Text, ConsoleStream

# ----- control seq -----


def test_ctlseq_no_param():
    cs = ControlSeq("v")
    assert cs.to_bytes(csi=True) == b"\x9bv"
    assert cs.to_bytes(csi=False) == b"\x1b[v"


def test_ctlseq_param():
    cs = ControlSeq("v", 1)
    assert cs.to_bytes(csi=True) == b"\x9b1v"
    assert cs.to_bytes(csi=False) == b"\x1b[1v"


def test_ctlseq_param_two():
    cs = ControlSeq("v", 1, 42)
    assert cs.to_bytes(csi=True) == b"\x9b1;42v"
    assert cs.to_bytes(csi=False) == b"\x1b[1;42v"


def test_ctlseq_param_skip():
    cs = ControlSeq("v", None, 42)
    assert cs.to_bytes(csi=True) == b"\x9b;42v"
    assert cs.to_bytes(csi=False) == b"\x1b[;42v"


# ----- seqparser -----


def test_seqparser_simple_seq():
    # end sequence
    p = SeqParser()
    assert p.feed("V") == [ControlSeq("V")]


def test_seqparser_param():
    # param
    p = SeqParser()
    assert p.feed("1") is None
    assert p.feed("H") == [ControlSeq("H", 1)]


def test_seqparser_param_pos_sign():
    # param with pos sign
    p = SeqParser()
    assert p.feed("+") is None
    assert p.feed("1") is None
    assert p.feed("H") == [ControlSeq("H", 1)]


def test_seqparser_param_net_sign():
    # param with neg sign
    p = SeqParser()
    assert p.feed("-") is None
    assert p.feed("5") is None
    assert p.feed("H") == [ControlSeq("H", -5)]


def test_seqparser_param_spaces():
    # param with spaces
    p = SeqParser()
    assert p.feed(" ") is None
    assert p.feed("5") is None
    assert p.feed(" ") is None
    assert p.feed("H") == [ControlSeq("H", 5)]


def test_seqparser_param_two():
    # 2 param
    p = SeqParser()
    assert p.feed("1") is None
    assert p.feed("1") is None
    assert p.feed(";") is None
    assert p.feed("9") is None
    assert p.feed("H") == [ControlSeq("H", 11, 9)]


def test_seqparser_invalid_term():
    # invalid - aborted seq
    p = SeqParser()
    assert p.feed("?") == [ControlChar(0x9B), Text("?")]


def test_seqparser_invalid_term_param():
    # invalid with param - aborted seq
    p = SeqParser()
    assert p.feed("1") is None
    assert p.feed(" ") is None
    assert p.feed("?") == [ControlChar(0x9B), Text("1 ?")]


def test_seqparser_param_special():
    # 2 param
    p = SeqParser()
    assert p.feed(">") is None
    assert p.feed("1") is None
    assert p.feed("H") == [ControlSeq("H", special={">", 1})]


def test_seqparser_param_special_norm():
    # 2 param
    p = SeqParser()
    assert p.feed(">") is None
    assert p.feed("1") is None
    assert p.feed(";") is None
    assert p.feed("9") is None
    assert p.feed("H") == [ControlSeq("H", 9, special={">", 1})]


# ----- constream -----


def test_constream_text():
    # write some text and wait for flush
    s = ConsoleStream()
    assert s.feed_bytes("hello") is None
    assert s.flush() == [Text("hello")]


def test_constream_control_char():
    # write control char
    s = ConsoleStream()
    # return text and control char
    assert s.feed_bytes("hello\nworld") == [
        Text("hello"),
        ControlChar(0xA),
    ]
    # remaining text after flush
    assert s.flush() == [Text("world")]


def test_constream_esc_no_seq():
    s = ConsoleStream()
    # if its no seq begin then return raw seq
    s.feed_bytes(b"\x1ba") == [ControlChar(0x1B), Text("a")]


def test_constream_esc_seq():
    s = ConsoleStream()
    s.feed_bytes(b"\x1b[a") == [ControlSeq("a")]


def test_constream_esc_invalid():
    s = ConsoleStream()
    s.feed_bytes(b"\x1b[1;?") == [ControlChar(0x9B), Text("1;?")]


def test_constream_csi_seq():
    s = ConsoleStream()
    s.feed_bytes(b"\x9ba") == [ControlSeq("a")]


def test_constream_csi_seq_invalid():
    s = ConsoleStream()
    # invalid seq will be reported as ctrlchar and text
    s.feed_bytes(b"\x9b1;?") == [ControlChar(0x9B), Text("1;?")]
