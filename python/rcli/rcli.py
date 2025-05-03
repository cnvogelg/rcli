"""rcli - remote Amiga CLI shell"""

import argparse
import sys
import logging
import socket
import select
import os
import pprint

from amicon import Console, Writer, UnixTermScreen

LOGGING_FORMAT = "%(message)s"
DESC = "remote Amiga CLI shell"


class SocketWriter(Writer):
    def __init__(self, socket):
        self.socket = socket

    def write(self, buffer):
        self.socket.send(buffer)


def rcli(host, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((host, port))
    except OSError as e:
        print(f"Error connecting '{host}:{port}': {e}")
        return 1

    # handshake
    handshake = b"CLI1"
    s.send(handshake)
    res = s.recv(6)
    if res != handshake:
        print("Wrong handshake... no rclid?")
        return 2

    # setup console
    screen = UnixTermScreen()
    term_fd = screen.get_fd()
    writer = SocketWriter(s)
    con = Console(screen, writer)

    # main loop
    try:
        connected = True
        while connected:
            # Get the list sockets which are readable
            fd_list = [term_fd, s]
            read_fds, _, _ = select.select(fd_list, [], [])

            for fd in read_fds:
                # incoming message from remote server
                if fd == s:
                    data = s.recv(1024)
                    if not data:
                        connected = False
                    else:
                        con.feed_bytes(data)
                # user typed some keys here in the console
                elif fd == term_fd:
                    screen.handle_input(con)

        print("\r\nDisconnect.")
    except KeyboardInterrupt:
        print("\r\nBreak.")
    finally:
        screen.exit()

    s.close()
    return 0


def parse_args():
    # parse args
    parser = argparse.ArgumentParser(
        description=DESC,
    )
    parser.add_argument(
        "-v", "--verbose", action="count", help="be more verbose", default=0
    )
    parser.add_argument("-L", "--log-file", help="log file to write")
    parser.add_argument("host", help="host name of Amiga with rclid server")
    parser.add_argument("-p", "--port", help="host port", type=int, default=2323)

    return parser.parse_args()


def main():
    opts = parse_args()

    # setup logging
    if opts.verbose == 0:
        level = logging.WARNING
    elif opts.verbose == 1:
        level = logging.INFO
    else:
        level = logging.DEBUG
    if opts.log_file:
        logging.basicConfig(
            format=LOGGING_FORMAT, level=level, filename=opts.log_file, filemode="w"
        )
    else:
        logging.basicConfig(format=LOGGING_FORMAT, level=level)

    # call main
    result = rcli(opts.host, opts.port)
    sys.exit(result)
