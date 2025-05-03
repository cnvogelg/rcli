import sys
import os
import curses
import curses.ascii
import logging

from .screen import Screen
from .console import Console


class UnixTermScreen(Screen):
    def __init__(self):
        self.win = curses.initscr()
        # init
        curses.cbreak()
        curses.noecho()
        self.win.timeout(0)
        self.win.keypad(True)
        logging.info("unix term=%s", curses.termname())

    def get_fd(self):
        return sys.stdin.fileno()

    def handle_input(self, console: Console):
        ch = self.win.getch()
        logging.debug("unix: got ch: %s", ch)
        if curses.ascii.isascii(ch):
            console.press_key(ch)

    def exit(self):
        curses.nocbreak()
        curses.echo()
        self.win.keypad(False)
        curses.endwin()

    def write_text(self, txt):
        self.win.addstr(txt)

    def refresh(self):
        self.win.refresh()
