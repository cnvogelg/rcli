"""rcli - remote Amiga CLI shell"""

import argparse
import sys
import logging
import socket
import select
import tty
import termios
import os

LOGGING_FORMAT = "%(message)s"
DESC = "remote Amiga CLI shell"


def rcli(host, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))

    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    tty.setraw(fd)

    try:
        connected = True
        while connected:
            socket_list = [fd, s]

            # Get the list sockets which are readable
            read_sockets, write_sockets, error_sockets = select.select(
                socket_list, [], []
            )

            for sock in read_sockets:
                # incoming message from remote server
                if sock == s:
                    data = sock.recv(1024)
                    if not data:
                        connected = False
                    else:
                        # print data
                        data = data.replace(b"\n", b"\r\n")
                        os.write(fd, data)
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
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)

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
    logging.basicConfig(format=LOGGING_FORMAT, level=level)

    # call main
    result = rcli(opts.host, opts.port)
    sys.exit(result)
