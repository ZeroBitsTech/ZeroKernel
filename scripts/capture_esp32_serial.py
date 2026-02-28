#!/usr/bin/env python3
import argparse
import os
import select
import sys
import termios
import time


def configure_port(fd: int, baud: int) -> None:
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    speed = getattr(termios, f"B{baud}", termios.B115200)
    attrs[4] = speed
    attrs[5] = speed
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    termios.tcflush(fd, termios.TCIOFLUSH)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=float, default=6.0)
    parser.add_argument("--start-delay", type=float, default=1.0)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output", default="-")
    args = parser.parse_args()

    time.sleep(max(args.start_delay, 0.0))

    fd = os.open(args.port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
    try:
      configure_port(fd, args.baud)
      deadline = time.monotonic() + max(args.seconds, 0.0)
      chunks = []

      while time.monotonic() < deadline:
          wait = max(0.0, min(0.25, deadline - time.monotonic()))
          readable, _, _ = select.select([fd], [], [], wait)
          if not readable:
              continue
          data = os.read(fd, 4096)
          if data:
              chunks.append(data)
    finally:
      os.close(fd)

    payload = b"".join(chunks)
    if args.output == "-":
        sys.stdout.buffer.write(payload)
        return 0

    with open(args.output, "wb") as handle:
        handle.write(payload)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
