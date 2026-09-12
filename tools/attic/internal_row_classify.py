#!/usr/bin/env python3
"""internal_row_classify.py -- classify the address-keyed and ordinal-keyed rows
that tools/runtime_census.py is CORRECTLY unable to classify.

Background
----------
`runtime_census.py` classifies a gap row by decoding its MSVC mangled name
(`classify_symbol`).  Rows keyed by a raw virtual address (`kind=addr`,
`namespace=internal`) and by ordinal (`kind=ordinal`) have no name to decode, so
the census emits UNKNOWN and refuses to guess.  That refusal is right: bucketing
them into BODY would claim unexported .text and .rdata rows are all port work,
which is the INFLATING direction and manufactures work.

This tool answers the question the census cannot, from the BINARY rather than
from a name:

  SECTION   which PE section holds the address (.text vs .rdata/.data)
  EXPORT    does the address actually have an export name after all?
            ("AEAA means PRIVATE, not unexported" -- this DLL exports its
            private members, so an 'unexported, resolved by address' comment is
            frequently false.)
  PDATA     is there a RUNTIME_FUNCTION covering it, does it START one, and is
            its unwind info a CHAINED fragment (UNW_FLAG_CHAININFO)?  A .pdata
            entry is a LOWER bound on extent -- this tool follows the chain to
            the root and reports the chained extent, never the fragment's.
  RELOC     for a data address, how many consecutive 8-byte base-relocated slots
            start there and where do they point?  A run of slots into .text is
            vtable-shaped; zero relocs is a scalar/POD static.

Every screen has a positive control; run `--selftest` to execute them.

Usage
  internal_row_classify.py --csv w23.csv            # classify census UNKNOWN rows
  internal_row_classify.py --addr 0x180009c30 ...   # classify specific addresses
  internal_row_classify.py --selftest               # run the positive controls

Owner: Wave 23 lane INTERNAL.  Folding this into tools/runtime_census.py is a
follow-up for the Wave 24 integrator (runtime_census.py was owned by Wave 22
lane ALLOC while this was written; see WAVE23_LANE_INTERNAL.md).
"""

import argparse
import csv
import os
import struct
import sys

try:
    import pefile
except ImportError:
    sys.exit("pefile is required: use tools/ghidra_mcp/.venv/Scripts/python.exe")

RETAIL = r"C:\GOG Games\Banished\Runtime-x64.dll"
PROFILE = r"C:\code\GIT\untitled_rev_eng_project\ToolKit\bin\x64\Runtime-x64-profile.dll"

UNW_FLAG_CHAININFO = 0x4


class Image:
    """One loaded PE, with the screens this tool needs."""

    def __init__(self, path, label):
        self.path = path
        self.label = label
        self.pe = pefile.PE(path, fast_load=True)
        self.pe.parse_data_directories(directories=[
            pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT'],
            pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_BASERELOC'],
            pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION'],
        ])
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.data = self.pe.get_memory_mapped_image()

        # --- SECTION screen ---------------------------------------------
        self.sections = []
        for s in self.pe.sections:
            name = s.Name.decode('ascii', 'replace').rstrip('\0')
            size = max(s.Misc_VirtualSize, s.SizeOfRawData)
            self.sections.append((s.VirtualAddress, s.VirtualAddress + size, name))

        # --- EXPORT screen ----------------------------------------------
        self.exports = {}          # rva -> [names]
        self.by_ordinal = {}       # ordinal -> name
        exp = getattr(self.pe, 'DIRECTORY_ENTRY_EXPORT', None)
        self.export_ordinal_base = exp.struct.Base if exp else 0
        if exp:
            for sym in exp.symbols:
                nm = sym.name.decode('ascii', 'replace') if sym.name else None
                if sym.address is None:
                    continue
                self.exports.setdefault(sym.address, []).append(nm)
                self.by_ordinal[sym.ordinal] = (nm, sym.address)
        self.export_rvas = sorted(self.exports)

        # --- PDATA screen -----------------------------------------------
        # Parse .pdata by hand: pefile's exception-directory support is thin.
        self.pdata = []  # (begin, end, unwind)
        d = self.pe.OPTIONAL_HEADER.DATA_DIRECTORY[
            pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']]
        off, n = d.VirtualAddress, d.Size // 12
        for i in range(n):
            rec = self.data[off + i * 12: off + i * 12 + 12]
            if len(rec) < 12:
                break
            b, e, u = struct.unpack('<III', rec)
            if b == 0 and e == 0:
                continue
            self.pdata.append((b, e, u))
        self.pdata.sort()
        self.pdata_begins = [p[0] for p in self.pdata]

        # --- RELOC screen -----------------------------------------------
        self.relocs = set()
        for blk in getattr(self.pe, 'DIRECTORY_ENTRY_BASERELOC', []):
            for r in blk.entries:
                if r.type == 10:  # IMAGE_REL_BASED_DIR64
                    self.relocs.add(r.rva)

        # --- CHAIN RESOLUTION, forward ---------------------------------
        # THE TRAP, AS ACTUALLY OBSERVED (not as previously recorded):
        # MSVC splits a function into fragments.  The ENTRY fragment carries
        # the real UNWIND_INFO and is NOT chained; every CONTINUATION fragment
        # is chained and points BACK at the entry.  So the fragment you land on
        # when you look up a function's own address is precisely the one whose
        # chained flag reads False -- it looks complete and is not.  At
        # 0x180078ad0 the entry fragment is 13 bytes and the real function is
        # 265 (20x).  A walker that only follows chains UPWARD from the address
        # you asked about therefore NEVER widens anything for the case that
        # actually matters.  The extent of a function is the entry fragment
        # UNIONED with every fragment that chains back to it.
        self.chain_root = {}   # frag begin -> entry begin
        for (b, e, u) in self.pdata:
            root = self._resolve_root(u)
            if root is not None and root != b:
                self.chain_root[b] = root
        self.chain_members = {}
        for b, root in self.chain_root.items():
            self.chain_members.setdefault(root, []).append(b)

    def _resolve_root(self, unwind_rva):
        seen = set()
        cu = unwind_rva
        root = None
        while cu and cu not in seen:
            seen.add(cu)
            try:
                vf = self.data[cu]
            except Exception:
                return root
            if not ((vf >> 3) & UNW_FLAG_CHAININFO):
                return root
            cnt = self.data[cu + 2]
            tail = cu + 4 + ((cnt + 1) & ~1) * 2
            try:
                pb, pe_, pu = struct.unpack('<III', self.data[tail:tail + 12])
            except Exception:
                return root
            root = pb
            cu = pu
        return root


    # ------------------------------------------------------------------
    def section_of(self, rva):
        for lo, hi, name in self.sections:
            if lo <= rva < hi:
                return name
        return None

    def qword(self, rva):
        try:
            return struct.unpack('<Q', self.data[rva:rva + 8])[0]
        except Exception:
            return None

    def export_exact(self, rva):
        return list(self.exports.get(rva, []))

    def export_enclosing(self, rva):
        """Nearest export at or before rva, in the same section.

        NOTE this is an UPPER bound on 'belongs to': the adjacent-export size
        proof has swallowed unexported tables before.  Treat a hit whose delta
        is large as WEAK evidence, not as containment.
        """
        import bisect
        i = bisect.bisect_right(self.export_rvas, rva) - 1
        if i < 0:
            return None, None
        e = self.export_rvas[i]
        if self.section_of(e) != self.section_of(rva):
            return None, None
        return self.exports[e][0], rva - e

    def pdata_of(self, rva):
        """RUNTIME_FUNCTION covering rva, with the chain followed to its root.

        A .pdata entry is a LOWER bound on function extent (MSVC chains
        fragments via UNW_FLAG_CHAININFO); sizing from the fragment silently
        UNDERSTATES, and understating makes work look cheap.  Returns
        (begin, end, is_start, chained, root_begin, root_end).
        """
        import bisect
        i = bisect.bisect_right(self.pdata_begins, rva) - 1
        if i < 0:
            return None
        b, e, u = self.pdata[i]
        if not (b <= rva < e):
            return None
        # Which entry does this fragment belong to?
        root = self.chain_root.get(b, b)
        # Union the entry with every fragment chaining back to it.
        rb = root
        re_ = None
        for fb, fe, fu in self.pdata:
            if fb == root:
                re_ = fe if re_ is None else max(re_, fe)
        for fb in self.chain_members.get(root, []):
            for f2b, f2e, f2u in self.pdata:
                if f2b == fb:
                    re_ = f2e if re_ is None else max(re_, f2e)
        if re_ is None:
            re_ = e
        nfrag = 1 + len(self.chain_members.get(root, []))
        return (b, e, rva == root, nfrag > 1, rb, re_, nfrag)

    def padded_start(self, rva):
        """Is rva preceded by MSVC inter-function padding (0xCC / 0x90)?

        This is the screen that saves us from .pdata: 52.9% of this DLL's .text
        exports are LEAF functions with NO .pdata entry at all, so 'no .pdata'
        is NOT evidence against being a function.  MSVC pads between functions
        with int3; an address preceded by padding and 16-byte aligned is a
        function start whether or not it has unwind data.
        """
        if rva < 1:
            return None
        prev = self.data[rva - 1]
        pad = prev in (0xCC, 0x90)
        return {'prev_byte': '0x%02x' % prev, 'padded': pad,
                'aligned16': (rva % 16) == 0}

    def rtti_locator(self, rva):
        """Does rva look like an MSVC vtable?

        A "run of >= 3 relocated .text pointers" is a crude threshold: it calls
        a genuine 2-slot vtable a mere pointer pair.  MSVC puts a pointer to the
        RTTI CompleteObjectLocator in the slot IMMEDIATELY BEFORE the vtable, so
        this decides the question on structure rather than on length.  Returns
        the locator address, or None.
        """
        if rva < 8 or (rva - 8) not in self.relocs:
            return None
        q = self.qword(rva - 8)
        if q is None:
            return None
        t = q - self.base
        if self.section_of(t) != '.rdata':
            return None
        # A CompleteObjectLocator is {signature, offset, cdOffset, pTypeDescriptor,
        # pClassDescriptor, [pSelf]} -- signature is 0 (32-bit) or 1 (64-bit).
        sig = self.qword(t)
        if sig is None or (sig & 0xFFFFFFFF) not in (0, 1):
            return None
        return t + self.base

    def reloc_run(self, rva, limit=512):
        """Consecutive 8-byte relocated slots starting at rva, and where they go.

        Returns (count, targets[:8], sections_hit).
        """
        n = 0
        targets = []
        secs = set()
        while n < limit and (rva + n * 8) in self.relocs:
            q = self.qword(rva + n * 8)
            if q is None:
                break
            t = q - self.base
            targets.append(t)
            secs.add(self.section_of(t))
            n += 1
        return n, targets[:8], secs


def classify(img, rva_abs):
    rva = rva_abs - img.base
    out = {'addr': '0x%x' % rva_abs, 'rva': '0x%x' % rva}
    sec = img.section_of(rva)
    out['section'] = sec or '<none>'
    exact = img.export_exact(rva)
    out['export_exact'] = ';'.join(x for x in exact if x) if exact else ''
    enc, delta = img.export_enclosing(rva)
    out['export_enclosing'] = enc or ''
    out['export_delta'] = ('0x%x' % delta) if delta is not None else ''
    pd = img.pdata_of(rva)
    if pd:
        b, e, is_start, chained, rb, re_, nfrag = pd
        out['pdata'] = 'start' if is_start else 'inside'
        out['pdata_frag'] = '0x%x-0x%x (%d bytes)' % (b + img.base, e + img.base, e - b)
        out['pdata_chained'] = '%d frags' % nfrag if chained else 'no'
        out['pdata_root'] = '0x%x-0x%x (%d bytes)' % (rb + img.base, re_ + img.base, re_ - rb)
        out['understated_by'] = ('%.1fx' % ((re_ - rb) / max(1, e - b))) if (re_ - rb) > (e - b) else ''
    else:
        out['pdata'] = ''
        out['pdata_frag'] = ''
        out['pdata_chained'] = ''
        out['pdata_root'] = ''
        out['understated_by'] = ''
    ps = img.padded_start(rva) if sec == '.text' else None
    out['prev_byte'] = ps['prev_byte'] if ps else ''
    out['start_shape'] = ('' if not ps else
                          ('PADDED' if ps['padded'] else 'mid-stream') +
                          ('+A16' if ps['aligned16'] else ''))
    n, targets, secs = img.reloc_run(rva)
    out['reloc_run'] = str(n)
    out['reloc_targets'] = ';'.join('.text' if s == '.text' else (s or '?') for s in sorted(x or '?' for x in secs))
    out['first_slots'] = ';'.join('0x%x' % (t + img.base) for t in targets[:4])
    loc = img.rtti_locator(rva) if sec in ('.rdata', '.data') else None
    out['rtti_locator'] = ('0x%x' % loc) if loc else ''

    # --- the verdict --------------------------------------------------
    if sec == '.text':
        if exact:
            v = 'CODE/EXPORTED'          # a name door exists after all
        elif pd and pd[2]:
            v = 'CODE/FUNCTION'          # unexported function start
        elif pd:
            v = 'CODE/INSIDE-FUNCTION'   # address is mid-body (a jump target or
        elif ps and ps['padded']:        # a thunk sharing another's unwind)
            v = 'CODE/LEAF-FUNCTION'     # no .pdata, but padded: a LEAF fn.
        else:                            # 52.9% of .text exports look like this
            v = 'CODE/NO-PDATA-NO-PAD'
    elif sec in ('.rdata', '.data'):
        # NOTE: the RTTI CompleteObjectLocator test is INAPPLICABLE to
        # Runtime-x64.dll -- it is built /GR- (RTTI off).  0 of its 147 exported
        # ??_7 tables carry a locator and the whole image holds 12 `.?AV` type
        # descriptors (CRT exception machinery), not the ~hundreds a /GR build
        # would have.  The column is kept because it costs nothing and would
        # fire on a binary that has RTTI, but it must NOT drive the verdict
        # here: a dead screen returning 0 would read as "none of these are
        # vtables".  Length + target section is the only vtable screen
        # available in this binary.
        if exact:
            v = 'DATA/EXPORTED'
        elif n >= 3 and secs == {'.text'}:
            v = 'DATA/VTABLE-SHAPED'        # long run of .text ptrs, no locator
        elif n >= 1:
            v = 'DATA/POINTER'
        else:
            v = 'DATA/SCALAR-OR-STRUCT'     # NOTE: only says slot +0 is not a
            # relocated pointer.  A struct whose FIRST field is scalar and whose
            # later fields are pointers (DIDATAFORMAT) lands here too.
    else:
        v = 'OTHER/' + (sec or 'none')
    out['verdict'] = v
    return out


FIELDS = ['addr', 'rva', 'section', 'verdict', 'export_exact', 'export_enclosing',
          'export_delta', 'pdata', 'pdata_frag', 'pdata_chained', 'pdata_root', 'understated_by',
          'prev_byte', 'start_shape',
          'reloc_run', 'reloc_targets', 'first_slots', 'rtti_locator']


# ----------------------------------------------------------------------
# POSITIVE CONTROLS.  A screen looking for the ABSENCE of a shape must be
# HANDED one, or "0 hits" is indistinguishable from a dead parser.
# ----------------------------------------------------------------------
def selftest(img):
    ok = True

    def check(name, cond, detail):
        nonlocal ok
        print('  %-28s %-5s  %s' % (name, 'PASS' if cond else 'FAIL', detail))
        if not cond:
            ok = False

    print('positive controls against %s' % img.label)

    # SECTION: hand it one address of each kind we expect to see.
    text_rva = next(r for r in img.export_rvas if img.section_of(r) == '.text')
    rdata_rvas = [r for r in img.export_rvas if img.section_of(r) == '.rdata']
    check('SECTION .text', img.section_of(text_rva) == '.text', '0x%x' % (text_rva + img.base))
    check('SECTION .rdata', bool(rdata_rvas) and img.section_of(rdata_rvas[0]) == '.rdata',
          '0x%x' % ((rdata_rvas[0] if rdata_rvas else 0) + img.base))
    check('SECTION miss is None', img.section_of(0xFFFFFFF0) is None, 'rva past image')

    # EXPORT: a known export must resolve; a deliberately-shifted address must not.
    check('EXPORT exact hit', bool(img.export_exact(text_rva)),
          img.export_exact(text_rva)[0][:60])
    check('EXPORT exact miss', not img.export_exact(text_rva + 3),
          'export+3 correctly not an exact hit')

    # PDATA: an exported function start must be found and flagged 'start';
    # a .rdata address must NOT be found.
    #
    # WHAT THE FIRST VERSION OF THIS CONTROL CAUGHT: it handed the screen
    # `text_rva` -- the LOWEST .text export -- and the screen said "no .pdata".
    # That was not a dead parser, it was the truth: 52.9% of this DLL's .text
    # exports are LEAF functions with no unwind data at all.  The control was
    # wrong, not the screen, and the finding is load-bearing: any instrument
    # that treats "has .pdata" as "is a function" misses HALF the functions in
    # this binary.  The control now hands the screen a NON-leaf and separately
    # asserts the leaf population is large.
    with_pd = next(r for r in img.export_rvas
                   if img.section_of(r) == '.text' and img.pdata_of(r))
    pd = img.pdata_of(with_pd)
    check('PDATA finds a function', pd is not None and pd[2],
          ('0x%x-0x%x %s' % (pd[0] + img.base, pd[1] + img.base,
                             img.exports[with_pd][0][:40])) if pd else 'none')
    tx = [r for r in img.export_rvas if img.section_of(r) == '.text']
    leaf = [r for r in tx if img.pdata_of(r) is None]
    check('PDATA leaf population', len(leaf) > 0,
          '%d of %d .text exports (%.1f%%) have NO .pdata -- "no pdata" is NOT '
          '"not a function"' % (len(leaf), len(tx), 100.0 * len(leaf) / len(tx)))

    # PADDING: the screen that covers the leaf case.  Hand it a real function
    # start (must read PADDED) and a mid-body address (must not).
    ps_start = img.padded_start(with_pd)
    check('PADDING at function start', ps_start and ps_start['padded'],
          'prev byte %s at %s' % (ps_start['prev_byte'], img.exports[with_pd][0][:40]))
    mid = with_pd + 4
    ps_mid = img.padded_start(mid)
    check('PADDING rejects mid-body', ps_mid and not ps_mid['padded'],
          'prev byte %s at export+4' % ps_mid['prev_byte'])
    if rdata_rvas:
        check('PDATA rejects .rdata', img.pdata_of(rdata_rvas[0]) is None,
              '0x%x' % (rdata_rvas[0] + img.base))
    # CHAINING must actually be observed somewhere, or the chain walker is dead
    # code and every 'not chained' answer is meaningless.
    nchain = 0
    for (b, e, u) in img.pdata:
        try:
            if (img.data[u] >> 3) & UNW_FLAG_CHAININFO:
                nchain += 1
        except Exception:
            pass
    check('PDATA chain walker live', nchain > 0,
          '%d of %d .pdata entries are CHAINED fragments' % (nchain, len(img.pdata)))

    # THE HANDED-IN SHAPE.  A screen looking for the ABSENCE of understatement
    # has no natural positive case, so hand it one whose true extent is known
    # from an INDEPENDENT instrument (Ghidra's function body):
    #   0x180078ad0  Ghidra body 0x180078ad0-0x180078bd9 = 265 bytes
    #   0x180052640  Ghidra body 0x180052640-0x1800526fa = 186 bytes
    # Both have an ENTRY fragment of 13 bytes with chained=False.  The first
    # version of this walker reported 13 for both.
    for probe, truth in ((0x180078ad0, 265), (0x180052640, 186)):
        r = img.pdata_of(probe - img.base)
        got = (r[5] - r[4]) if r else 0
        check('PDATA understatement %#x' % probe, got == truth,
              'walker says %d bytes, Ghidra body is %d' % (got, truth))

    # ...and the mirror: a genuinely single-fragment function must NOT inflate.
    single = None
    for (b, e, u) in img.pdata:
        if b not in img.chain_root and not img.chain_members.get(b):
            single = (b, e)
            break
    if single:
        r = img.pdata_of(single[0])
        check('PDATA no false widening', r and (r[5] - r[4]) == (single[1] - single[0]),
              'single-fragment fn 0x%x stays %d bytes' % (single[0] + img.base,
                                                          single[1] - single[0]))
    else:
        check('PDATA no false widening', False, 'no single-fragment function found')

    nfrag_fns = len(img.chain_members)
    check('PDATA fragmented population', nfrag_fns > 0,
          '%d functions are split across >1 .pdata fragment (%d fragments total)'
          % (nfrag_fns, nchain))

    # RELOC: a ??_7 vtable export must show a run of .text pointers; a plain
    # scalar static must show zero.  Hand the screen both shapes.
    vt = [r for r in img.export_rvas
          if any(n and n.startswith('??_7') for n in img.exports[r])]
    if vt:
        n, t, secs = img.reloc_run(vt[0])
        check('RELOC vtable run', n >= 3 and secs == {'.text'},
              '%s -> %d slots into %s' % (img.exports[vt[0]][0][:40], n, secs))
    else:
        check('RELOC vtable run', False, 'no ??_7 export found -- screen unproven')
    zero = None
    for r in img.export_rvas:
        if img.section_of(r) in ('.data', '.rdata') and img.reloc_run(r)[0] == 0:
            zero = r
            break
    # RTTI: every exported ??_7 vtable must carry a CompleteObjectLocator; a
    # non-vtable static must not.  Hand the screen both.
    # ...and this one is the reason the RTTI screen does NOT drive the verdict.
    # It has no positive case in this binary, so it can only ever return 0, and
    # a 0 from a dead screen reads exactly like "none of these are vtables".
    nvt = sum(1 for r in vt if img.rtti_locator(r))
    ndesc = img.data.count(b'.?AV')
    check('RTTI screen INAPPLICABLE', nvt == 0 and ndesc < 50,
          '%d of %d exported ??_7 tables carry a locator and the image holds %d '
          '".?AV" descriptors -- this DLL is /GR- (RTTI OFF), so this screen has '
          'NO positive case and must not drive a verdict' % (nvt, len(vt), ndesc))
    false_loc = sum(1 for r in img.export_rvas[:3000]
                    if img.section_of(r) == '.text' and img.rtti_locator(r))
    check('RTTI rejects .text', false_loc == 0,
          '%d .text exports falsely read as vtables (expected 0)' % false_loc)

    check('RELOC scalar is zero', zero is not None,
          ('0x%x %s' % (zero + img.base, img.exports[zero][0][:40])) if zero else 'none found')

    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--csv', help='runtime_census.py --csv output to read UNKNOWN rows from')
    ap.add_argument('--addr', nargs='*', default=[], help='explicit addresses')
    ap.add_argument('--ordinal', nargs='*', default=[], help='explicit ordinals')
    ap.add_argument('--dll', default=RETAIL)
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--out', help='write a CSV here')
    a = ap.parse_args()

    img = Image(a.dll, os.path.basename(a.dll))

    if a.selftest:
        sys.exit(0 if selftest(img) else 1)

    addrs = [int(x, 0) for x in a.addr]
    ords_ = [int(x, 0) for x in a.ordinal]
    callers = {}
    if a.csv:
        with open(a.csv, newline='') as f:
            for row in csv.DictReader(f):
                if row['ported'] != '0':
                    continue
                if row['kind'] == 'addr' and row['namespace'] == 'internal':
                    addrs.append(int(row['key'], 16))
                    callers[int(row['key'], 16)] = row['callers']
                elif row['kind'] == 'ordinal':
                    ords_.append(int(row['key']))
                    callers['ord%d' % int(row['key'])] = row['callers']

    rows = [classify(img, x) for x in sorted(set(addrs))]
    for r in rows:
        r['callers'] = callers.get(int(r['addr'], 16), '')

    if ords_:
        print('=== ordinal rows: ordinal -> export name (read straight out of the '
              'export table; NOT a guess) ===')
        for o in sorted(set(ords_)):
            hit = img.by_ordinal.get(o)
            if hit:
                nm, rva = hit
                c = classify(img, rva + img.base)
                print('  ordinal %-6d 0x%-12x %-14s %s' %
                      (o, rva + img.base, c['verdict'], nm))
            else:
                print('  ordinal %-6d <not in export table>' % o)
        print()

    w = csv.DictWriter(sys.stdout, fieldnames=FIELDS + ['callers'],
                       extrasaction='ignore', lineterminator='\n')
    w.writeheader()
    for r in rows:
        w.writerow(r)

    if a.out:
        with open(a.out, 'w', newline='') as f:
            w2 = csv.DictWriter(f, fieldnames=FIELDS + ['callers'], lineterminator='\n')
            w2.writeheader()
            for r in rows:
                w2.writerow(r)

    tally = {}
    for r in rows:
        tally[r['verdict']] = tally.get(r['verdict'], 0) + 1
    print('\n=== verdict tally (%d address rows) ===' % len(rows), file=sys.stderr)
    for k in sorted(tally, key=lambda k: -tally[k]):
        print('  %-24s %d' % (k, tally[k]), file=sys.stderr)


if __name__ == '__main__':
    main()
