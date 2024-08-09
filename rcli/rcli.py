"""rcli - remote Amiga CLI shell"""

import argparse
import sys
import logging
import socket
import select
import os
import pprint

from rcli.unixterm import UnixTerm, UnixConsoleBackend
from rcli.amicon import AmiConsole

LOGGING_FORMAT = "%(message)s"
DESC = "remote Amiga CLI shell"


def rcli(host, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((host, port))
    except OSError as e:
        print(f"Error connecting '{host}:{port}': {e}")

    term = UnixTerm()
    term_fd = term.get_fd()

    backend = UnixConsoleBackend(term)
    con = AmiConsole(backend)

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
                # user entered a message
                else:
                    msg = os.read(fd, 1)
                    if msg == b"\r":
                        msg = b"\n"
                    s.send(msg)

        print("\r\nDisconnect.")
    except KeyboardInterrupt:
        print("\r\nBreak.")
    finally:
        term.exit()

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
    parser.add_argument(
        "-L", "--log-file", help="log file to write", default="rcli.log"
    )
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
    logging.basicConfig(format=LOGGING_FORMAT, level=level, filename=opts.log_file)

    # call main
    result = rcli(opts.host, opts.port)
    sys.exit(result)
