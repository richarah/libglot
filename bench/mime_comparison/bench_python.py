#!/usr/bin/env python3
"""Head-to-head MIME parsing speed benchmark: Python stdlib `email` driver.

Same protocol as bench_libglot.cpp: read file, parse, walk every header and
every part recursively, decode text parts. Times the whole loop.
"""
import sys
import time
import email
import email.policy
from email.parser import BytesParser

def walk(msg, sink):
    for k, v in msg.items():
        sink[0] += len(k) + len(v)
    if msg.get_content_maintype() == "text":
        try:
            payload = msg.get_payload(decode=True)
            if payload is not None:
                charset = msg.get_content_charset() or "utf-8"
                text = payload.decode(charset, errors="strict")
                sink[0] += len(text)
        except (LookupError, UnicodeDecodeError, ValueError):
            pass
    if msg.is_multipart():
        for part in msg.get_payload():
            walk(part, sink)


def main():
    if len(sys.argv) != 2:
        print("usage: bench_python.py <filelist>", file=sys.stderr)
        return 2
    with open(sys.argv[1]) as f:
        paths = [line.rstrip("\n") for line in f if line.strip()]

    parsed = failed = 0
    sink = [0]
    parser = BytesParser(policy=email.policy.default)
    start = time.perf_counter()
    for path in paths:
        with open(path, "rb") as f:
            raw = f.read()
        try:
            msg = parser.parsebytes(raw)
            parsed += 1
            walk(msg, sink)
        except Exception:
            failed += 1
    elapsed = time.perf_counter() - start

    print(f"python email: {len(paths)} files, {parsed} parsed, {failed} failed, "
          f"{elapsed:.3f}s, {len(paths)/elapsed:.0f} msg/s (sink={sink[0]})")


if __name__ == "__main__":
    sys.exit(main())
