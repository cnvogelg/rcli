import curses


class UnixTerm:
    def __init__(self):
        self.win = curses.initscr()
        curses.noecho()
        curses.cbreak()
        # self.win.keypad(True)
        print("term=", curses.termname())
        print(curses.tigetstr("sc"))

    def exit(self):
        # self.win.keypad(False)
        curses.nocbreak()
        curses.echo()
        curses.endwin()


if __name__ == "__main__":
    ut = UnixTerm()
    ut.exit()
