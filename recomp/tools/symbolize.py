#!/usr/bin/env python3
"""Annotate hexadecimal addresses in a trace with symbol names from a linker map (psylink/SN .map format).

usage: symbolize.py MAP [FILE]   (reads stdin without FILE)
"""
import bisect
import re
import sys

syms = []
for line in open(sys.argv[1], errors='replace'):
    m = re.match(r'\s*([0-9A-Fa-f]{8})\s+(\S+)\s*$', line)
    if m:
        syms.append((int(m.group(1), 16), m.group(2)))
syms.sort()
addrs = [a for a, _ in syms]


def name(a):
    i = bisect.bisect_right(addrs, a) - 1
    if i < 0:
        return None
    off = a - addrs[i]
    return syms[i][1] + ('+0x%x' % off if off else '') if off < 0x4000 else None


src = open(sys.argv[2], errors='replace') if len(sys.argv) > 2 else sys.stdin
for line in src:
    def rep(m):
        a = int(m.group(0), 16)
        n = name(a)
        return '%s<%s>' % (m.group(0), n) if n else m.group(0)
    print(re.sub(r'\b8[0-9A-Fa-f]{7}\b', rep, line.rstrip('\n')))
