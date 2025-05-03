class Screen:
    def __init__(self):
        self.width = 80
        self.height = 25
        self.cursor_x = 0
        self.cursor_y = 0

    def get_size(self):
        return (self.width, self.height)

    def set_size(self, w, h):
        self.width = w
        self.height = h

    def get_cursor(self):
        return (self.cursor_x, self.cursor_y)

    def set_cursor(self, x, y):
        self.cursor_x = x
        self.cursor_y = y

    def write_text(self, txt):
        pass
