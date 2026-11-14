#!/usr/bin/env python3
"""Read files out of a PlayStation disc image (raw 2352-byte sectors, .bin with optional .cue; or an ISO).

    discfs.py DISC.cue|DISC.bin ls [PATH]
    discfs.py DISC.cue|DISC.bin extract PATH OUTFILE      e.g.  extract 'MGS\\SLPM_862.47' out.exe
"""
import os
import re
import struct
import sys


def open_image(path):
    if path.lower().endswith('.cue'):
        base = os.path.dirname(os.path.abspath(path))
        for line in open(path, errors='replace'):
            m = re.search(r'FILE\s+"([^"]+)"', line)
            if m:
                path = os.path.join(base, m.group(1))
                break
    return open(path, 'rb')


class Disc:
    def __init__(self, path):
        self.f = open_image(path)
        self.f.seek(0, 2)
        size = self.f.tell()
        # raw sectors have a sync pattern at the start; a plain ISO does not
        self.f.seek(0)
        head = self.f.read(12)
        self.raw = head == b'\x00' + b'\xff' * 10 + b'\x00'
        self.ss = 2352 if self.raw else 2048
        self.off = 24 if self.raw else 0

    def sector(self, n):
        self.f.seek(n * self.ss + self.off)
        return self.f.read(2048)

    def read(self, lba, size):
        out = b''.join(self.sector(lba + i) for i in range((size + 2047) // 2048))
        return out[:size]

    def listdir(self, lba, size):
        data = self.read(lba, size)
        i, out = 0, {}
        while i < len(data):
            n = data[i]
            if n == 0:
                i = (i // 2048 + 1) * 2048
                continue
            l = struct.unpack_from('<I', data, i + 2)[0]
            s = struct.unpack_from('<I', data, i + 10)[0]
            name = data[i + 33:i + 33 + data[i + 32]].decode('latin-1')
            if name not in ('\x00', '\x01'):
                out[name.split(';')[0]] = (l, s, bool(data[i + 25] & 2))
            i += n
        return out

    def root(self):
        pvd = self.sector(16)
        return struct.unpack_from('<I', pvd, 158)[0], struct.unpack_from('<I', pvd, 166)[0]

    def find(self, path):
        lba, size = self.root()
        parts = [p for p in re.split(r'[\\/]', path) if p]
        if parts and parts[0].lower().startswith('cdrom:'):
            parts[0] = parts[0][6:]
            parts = [p for p in parts if p]
        entry = (lba, size, True)
        for p in parts:
            d = self.listdir(entry[0], entry[1])
            key = p.split(';')[0]
            if key not in d:
                raise SystemExit('%s: not found on the disc' % path)
            entry = d[key]
        return entry


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    d = Disc(argv[1])
    if argv[2] == 'ls':
        e = d.find(argv[3]) if len(argv) > 3 else (*d.root(), True)
        for name, (l, s, isdir) in sorted(d.listdir(e[0], e[1]).items()):
            print('%-24s %10d %s lba=%d' % (name, s, 'dir' if isdir else '   ', l))
    elif argv[2] == 'extract' and len(argv) == 5:
        l, s, _ = d.find(argv[3])
        open(argv[4], 'wb').write(d.read(l, s))
        print('%s: %d bytes' % (argv[4], s))
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
