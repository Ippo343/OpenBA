"""Read Banished .pkg directories (FastLZ-1 windows, 32 KB) and extract every SoundBuffer
PlatformSerialize run (xWMA: 0x0161 WAVEFORMATEX) by resource name.

Usage: python -I pkg_sounds.py <pkg...> --out <dir>
"""
import os
import struct
import sys


def fastlz1_decompress(src, dst_cap):
    out = bytearray()
    sp = 0
    n = len(src)
    while sp < n:
        c = src[sp]
        sp += 1
        if c < 0x20:
            cnt = c + 1
            out += src[sp:sp + cnt]
            sp += cnt
        else:
            ln = c >> 5
            off = ((c & 0x1f) << 8) + 1
            if ln == 7:
                ln += src[sp]
                sp += 1
            off += src[sp]
            sp += 1
            start = len(out) - off
            if start < 0:
                raise ValueError("match before buffer")
            for i in range(ln + 2):
                out.append(out[start + i])
        if len(out) >= dst_cap:
            break
    return bytes(out)


class CompressedBlob:
    def __init__(self, blob):
        self.blob = blob
        (count,) = struct.unpack_from('<i', blob, 0)
        self.headers = [struct.unpack_from('<IHH', blob, 4 + 8 * i) for i in range(count)]
        self.windows = {}

    def window(self, blk):
        if blk in self.windows:
            return self.windows[blk]
        off, csz, usz = self.headers[blk]
        if csz:
            data = fastlz1_decompress(self.blob[off:off + csz], 0x8000)[:usz]
        else:
            data = self.blob[off:off + usz]
        self.windows[blk] = data
        return data

    def read_all(self):
        return b''.join(self.window(i) for i in range(len(self.headers)))


class Reader:
    def __init__(self, data):
        self.d = data
        self.p = 0

    def i32(self):
        (v,) = struct.unpack_from('<i', self.d, self.p)
        self.p += 4
        return v

    def u8(self):
        v = self.d[self.p]
        self.p += 1
        return v

    def raw(self, n):
        v = self.d[self.p:self.p + n]
        self.p += n
        return v


def read_pkg(path):
    f = open(path, 'rb')
    data = f.read()
    (hdr,) = struct.unpack_from('<I', data, 0)
    compressed = bool(hdr & 0x80000000)
    diroff = hdr & 0x7fffffff
    (blobLen,) = struct.unpack_from('<i', data, diroff)
    blob = data[diroff + 4:diroff + 4 + blobLen]
    stream = CompressedBlob(blob).read_all()
    r = Reader(stream)
    fileCount = r.i32()
    formatVersion = 0
    if compressed:
        formatVersion = r.i32()
        r.i32(); r.i32(); r.i32(); r.i32()      # version, userVersion, priority, platform
        r.u8()                                   # overridable
        r.i32()                                  # crc
        w = r.i32(); h = r.i32()                 # iconW, iconH
        r.raw(4 * w * h)                         # the icon pixels
        if formatVersion >= 2:
            n90 = r.i32()
            r.raw(n90)
        for _ in range(3):                       # name, author, description
            ln = r.i32()
            if formatVersion <= 2:
                r.raw(2 * ln)
            else:
                r.raw(ln)
    entries = []
    for _ in range(fileCount):
        ln = r.i32()
        if formatVersion > 2:
            name = r.raw(ln).decode('utf-8', 'replace').rstrip(chr(0))
        else:
            name = r.raw(2 * ln).decode('utf-16-le', 'replace').rstrip(chr(0))
        off = r.i32()
        size = r.i32()
        entries.append((name, off, size))
    return data, entries, formatVersion


def find_soundbuffer(entry_bytes):
    """Return (lengthOffset, info) for the PlatformSerialize run inside a SoundBuffer resource:
    int32 _length, int32 _wmaLength, table[_wmaLength/4], 0x28 format window (0x0161 at +0), packets."""
    b = entry_bytes
    i = 0
    while True:
        i = b.find(b'\x61\x01', i)
        if i < 0:
            return None
        if i + 0x28 <= len(b):
            ch, rate, avg, align, bits, cb = struct.unpack_from('<HIIHHH', b, i + 2)
            if ch in (1, 2) and rate in (22050, 32000, 44100, 48000) and bits == 16 and cb == 0 and align > 0:
                # walk back: table of wmaLength bytes, preceded by wmaLength and length
                for tbl in range(4, 4096, 4):
                    p = i - tbl - 8
                    if p < 0:
                        break
                    length, wmaLength = struct.unpack_from('<ii', b, p)
                    if wmaLength == tbl and length > 0 and p + 8 + tbl + 0x28 + length <= len(b):
                        return p, dict(channels=ch, rate=rate, avg=avg, align=align,
                                       length=length, wmaLength=wmaLength)
        i += 2


def main():
    args = sys.argv[1:]
    out = None
    if '--out' in args:
        k = args.index('--out')
        out = args[k + 1]
        del args[k:k + 2]
        os.makedirs(out, exist_ok=True)
    for path in args:
        data, entries, fv = read_pkg(path)
        print(f'{path}: {len(entries)} entries, formatVersion {fv}')
        audio = [e for e in entries if 'udio' in e[0]]
        for name, off, size in audio:
            eb = data[off:off + size]
            try:                                 # a .crs is itself a CompressedStream
                (bc,) = struct.unpack_from('<i', eb, 0)
                if 0 < bc < 64 and 4 + 8 * bc <= len(eb):
                    eb = CompressedBlob(eb).read_all()
            except Exception as ex:
                print('  ', name, 'decompress failed:', ex)
            sb = find_soundbuffer(eb)
            if sb is None:
                print(f'  {name:60s} size={size}  (no xWMA window)')
                continue
            p, info = sb
            # the serialized SoundBuffer prefix: loopForever, streamed, two strings, duration float
            pre = eb[max(0, p - 24):p].hex(' ')
            print(f'  {name:60s} size={size} ch={info["channels"]} rate={info["rate"]} pre=[{pre}] '
                  f'align={info["align"]} packets={info["length"] // info["align"]} '
                  f'table={info["wmaLength"] // 4} lenB={info["length"]}')
            if out:
                run = eb[p:p + 8 + info['wmaLength'] + 0x28 + info['length']]
                safe = name.replace('/', '__').replace('\\', '__')
                with open(os.path.join(out, safe + '.bin'), 'wb') as fo:
                    fo.write(run)
                # also the whole entry, for the streamed / loop flags
                with open(os.path.join(out, safe + '.entry'), 'wb') as fo:
                    fo.write(eb)


if __name__ == '__main__':
    main()
