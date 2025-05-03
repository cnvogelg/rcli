from .const import AmiSpecialKeys, AmiSpecialKeysShifted, CSI


class KeyHelper:
    def cursor_report(self, x, y):
        return CSI + f"{y};{x}R".encode("ascii")

    def window_bounds(self, w, h):
        return CSI + f"1;1;{h};{w}r".encode("ascii")

    def paste_clipboad(self):
        return CSI + f"0 v".encode("ascii")

    shift_map = {
        AmiSpecialKeys.F1: AmiSpecialKeysShifted.F1,
        AmiSpecialKeys.F2: AmiSpecialKeysShifted.F2,
        AmiSpecialKeys.F3: AmiSpecialKeysShifted.F3,
        AmiSpecialKeys.F4: AmiSpecialKeysShifted.F4,
        AmiSpecialKeys.F5: AmiSpecialKeysShifted.F5,
        AmiSpecialKeys.F6: AmiSpecialKeysShifted.F6,
        AmiSpecialKeys.F7: AmiSpecialKeysShifted.F7,
        AmiSpecialKeys.F8: AmiSpecialKeysShifted.F8,
        AmiSpecialKeys.F9: AmiSpecialKeysShifted.F9,
        AmiSpecialKeys.F10: AmiSpecialKeysShifted.F10,
        AmiSpecialKeys.F11: AmiSpecialKeysShifted.F11,
        AmiSpecialKeys.F12: AmiSpecialKeysShifted.F12,
        AmiSpecialKeys.INSERT: AmiSpecialKeysShifted.INSERT,
        AmiSpecialKeys.PAGE_UP: AmiSpecialKeysShifted.PAGE_UP,
        AmiSpecialKeys.PAGE_DOWN: AmiSpecialKeysShifted.PAGE_DOWN,
        AmiSpecialKeys.PAUSE_BREAK: AmiSpecialKeysShifted.PAUSE_BREAK,
        AmiSpecialKeys.HOME: AmiSpecialKeysShifted.HOME,
        AmiSpecialKeys.END: AmiSpecialKeysShifted.END,
        AmiSpecialKeys.UP: AmiSpecialKeysShifted.UP,
        AmiSpecialKeys.DOWN: AmiSpecialKeysShifted.DOWN,
        AmiSpecialKeys.RIGHT: AmiSpecialKeysShifted.RIGHT,
        AmiSpecialKeys.LEFT: AmiSpecialKeysShifted.LEFT,
    }

    def shift_key(self, key):
        return self.shift_map.get(key)
