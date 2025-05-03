import logging


class LineInput:
    def __init__(self, screen):
        self.screen = screen
        self.reset()

    def reset(self):
        self.line = bytearray()

    def get_line(self):
        return self.line

    def press_key(self, key_code):
        """return True if line is done"""
        logging.debug("line: add key %s", key_code)
        self.line += key_code
        # local loop back
        self.screen.write_text(key_code)
        self.screen.refresh()
        # end line?
        if key_code == b"\n":
            return True
        else:
            return False
