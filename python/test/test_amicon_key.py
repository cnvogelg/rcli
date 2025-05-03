from amicon.key import KeyHelper
from amicon.const import AmiSpecialKeys, AmiSpecialKeysShifted


def test_amicon_key_type():
    ki = KeyHelper()
    assert ki.cursor_report(2, 3) == b"\x9b3;2R"
    assert ki.window_bounds(42, 21) == b"\x9b1;1;21;42r"
    assert ki.shift_key(AmiSpecialKeys.F2) == AmiSpecialKeysShifted.F2
