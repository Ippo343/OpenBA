#!/usr/bin/env python3
"""Row-level diff of the resave image's reference table (tools/resave_gate/README.md).

    reftable_diff.py <golden.bin> <new.bin>

Layout, confirmed against the golden: +0x00 AAAAAAAA, +0x04 len, +0x08 version,
+0x0c AAAAAAAA (the table record), +0x10 len, +0x18 u32 count, then from +0x1c rows of
{u32 id, u32 (len | 0x80000000), path[len] NUL-terminated ASCII}. ids run 1..N.
README flake 1: `count` can be one higher than the rows actually written, so rows are
parsed until `count` is reached OR the next 8 bytes stop looking like a row.

The point: a DIFF caused by the loader race changes WHICH RESOURCES ARE RESIDENT -- extra or
missing path rows, plus the id shift behind them -- and nothing else. So the discriminator
is not the byte count, it is (a) which paths differ and (b) whether those rows account for
the WHOLE size change. If the payload after the table changed size too, it is a real
serializer change no matter how flake-like the first verdict looked.
Exit: 0 identical, 10 race-shaped, 20 REAL-shaped, 2 usage/parse error.
"""
import struct
import sys


def parse(path):
    with open(path, 'rb') as f:
        b = f.read()
    if len(b) < 0x20 or b[0:4] != b'\xaa\xaa\xaa\xaa' or b[0x0c:0x10] != b'\xaa\xaa\xaa\xaa':
        raise SystemExit('%s: not a resave image (no AAAAAAAA markers)' % path)
    count = struct.unpack_from('<I', b, 0x18)[0]
    pos, rows = 0x1c, []
    while len(rows) < count and pos + 8 <= len(b):
        rid, ln = struct.unpack_from('<II', b, pos)
        if not (ln & 0x80000000) or rid != len(rows) + 1:
            break
        ln &= 0x7fffffff
        if ln == 0 or ln > 1024 or pos + 8 + ln > len(b):
            break
        raw = b[pos + 8:pos + 8 + ln]
        rows.append(raw.rstrip(b'\x00').decode('latin-1'))
        pos += 8 + ln
    return {'size': len(b), 'count': count, 'rows': rows, 'table_end': pos, 'bytes': b}


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    g, n = parse(sys.argv[1]), parse(sys.argv[2])
    print('golden: size=%d count=%d rows=%d table_end=0x%x payload=%d' % (
        g['size'], g['count'], len(g['rows']), g['table_end'], g['size'] - g['table_end']))
    print('new   : size=%d count=%d rows=%d table_end=0x%x payload=%d' % (
        n['size'], n['count'], len(n['rows']), n['table_end'], n['size'] - n['table_end']))
    if g['bytes'] == n['bytes']:
        print('VERDICT: identical')
        return 0
    # README flake 1 in EITHER direction: the images are the same size and the ONLY bytes that
    # differ are the u32 count at +0x18. (A count one LOWER than the rows written makes the row
    # parser stop early and report a phantom "missing row" plus a phantom payload growth of the
    # same size -- seen 2026-09-19, TunnelMaterial.rsc -- so test the bytes before the rows.)
    if g['size'] == n['size']:
        diff_pos = [i for i, (a, c) in enumerate(zip(g['bytes'], n['bytes'])) if a != c]
        if diff_pos and all(0x18 <= i < 0x1c for i in diff_pos):
            print('count delta=%+d; %d differing byte(s), all inside the count field at +0x18' % (
                n['count'] - g['count'], len(diff_pos)))
            print('VERDICT: RACE-SHAPED (README flake 1: only the count field differs)')
            return 10
    from collections import Counter
    gc, nc = Counter(g['rows']), Counter(n['rows'])
    dup_n = sorted((nc - gc).elements())
    dup_g = sorted((gc - nc).elements())
    if dup_n or dup_g:
        print('  multiset row delta: +%s  -%s' % (dup_n[:6], dup_g[:6]))
        # a row present TWICE is the double-load of one resource by the two loaders
        row_delta_ms = sum(8 + len(p) + 1 for p in dup_n) - sum(8 + len(p) + 1 for p in dup_g)
        payload_delta_ms = (n['size'] - n['table_end']) - (g['size'] - g['table_end'])
        if 1 <= len(dup_n) + len(dup_g) <= 2 and payload_delta_ms == 0 and \
                n['size'] - g['size'] == row_delta_ms:
            print('VERDICT: RACE-SHAPED (resident-set difference of %d row(s) incl. duplicates; '
                  'payload size unchanged). Re-run. The SAME row every run would be semantic.' % (
                      len(dup_n) + len(dup_g)))
            return 10
    gs, ns = set(g['rows']), set(n['rows'])
    only_g, only_n = sorted(gs - ns), sorted(ns - gs)
    for p in only_n[:12]:
        print('  + only in NEW   : %s' % p)
    for p in only_g[:12]:
        print('  - only in GOLDEN: %s' % p)
    if len(only_n) > 12 or len(only_g) > 12:
        print('  ... (%d extra, %d missing in total)' % (len(only_n), len(only_g)))
    row_delta = sum(8 + len(p) + 1 for p in only_n) - sum(8 + len(p) + 1 for p in only_g)
    size_delta = n['size'] - g['size']
    payload_delta = (n['size'] - n['table_end']) - (g['size'] - g['table_end'])
    print('size delta=%+d  explained by differing rows=%+d  payload delta=%+d  count delta=%+d' % (
        size_delta, row_delta, payload_delta, n['count'] - g['count']))
    nrows = len(only_n) + len(only_g)
    if nrows == 0 and size_delta == 0 and abs(n['count'] - g['count']) == 1:
        ndiff = sum(1 for a, c in zip(g['bytes'], n['bytes']) if a != c)
        if ndiff <= 4:
            print('VERDICT: RACE-SHAPED (README flake 1: count off by one, no row change, %d byte(s) differ)' % ndiff)
            return 10
    if 1 <= nrows <= 2 and payload_delta == 0 and size_delta == row_delta:
        print('VERDICT: RACE-SHAPED (resident-set difference of %d row(s); payload size unchanged). '
              'Re-run. The SAME row on every run would be a semantic change.' % nrows)
        return 10
    print('VERDICT: REAL-SHAPED (payload size changed, or the rows do not explain the size, or >2 rows). '
          'Do not re-run it away: localise it.')
    return 20


if __name__ == '__main__':
    sys.exit(main())
