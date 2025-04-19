import sys
import os
import curses

from amicon import AmiConsoleBackend


class UnixTerm:
    def __init__(self):
        self.win = curses.initscr()
        self.win.keypad(True)
        print("term=", curses.termname())

    def get_fd(self):
        return sys.stdin.fileno()

    def exit(self):
        self.mode_line()
        curses.endwin()

    def mode_raw(self):
        curses.noecho()
        curses.cbreak()

    def mode_line(self):
        curses.nocbreak()
        curses.echo()


class UnixConsoleBackend(AmiConsoleBackend):
    def __init__(self, term: UnixTerm):
        self.term = term

    def handle_event(self, ev):
        print("EVENT:", ev)

    def write_text(self, txt):
        os.write(self.term.get_fd(), txt)


if __name__ == "__main__":
    ut = UnixTerm()
    ut.exit()
