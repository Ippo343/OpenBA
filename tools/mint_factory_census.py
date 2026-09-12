#!/usr/bin/env python3
# =============================================================================
# tools/mint_factory_census.py -- WAVE 26 LANE FACTORY
#
# HOW BIG IS THE FAMILY BEHIND THE ATTEMPT-26 CUTOVER WALL?
#
# Attempt 26 died on the mint's own per-slot trap, naming ONE symbol:
#     ?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ
# A cutover boot is a GATE, not a PLANNER (reference_the_boot_is_a_gate_not_a_
# planner.md): it can only name what it reached before faulting. So the question
# "is this wall one function, a dozen, or a hundred?" cannot be answered by
# booting again -- it has to be DERIVED.
#
# WHAT THIS EXAMINES, stated because a screen that cannot name its subject
# manufactures work (reference_a_screen_that_cannot_name_its_subject.md):
#   * population: every minted `?_typeInfo@...@` row in
#     standalone/rt_data_mint_tables.inc (the same file rt_mint_gen.py --check
#     counts), and the slot-0 `factoryFn` target of each.
#   * verdict: does that mangled name appear as a literal ANYWHERE in the source
#     roots runtime/ standalone/ code/ engine/ ?
#
# WHAT IT IS BLIND TO, and this is why it is the WEAKER of the lane's two
# derivations. It is a LITERAL grep:
#   * a registration spelled through a named constant is invisible
#     (reference_census_cannot_see_nonliteral_registrations.md)
#   * a name mentioned only in a COMMENT reads as present
#     (reference_uncommented_checker_manufactures_work.md) -- so a "present"
#     verdict here is an UPPER bound on ported-ness, not a proof of it.
# The authoritative derivation is the LIVE one: `[RtMint] BaseTypeInfo
# factories:` printed by RtMintReportTypeInfoFactories() in
# standalone/rt_data_mint.cpp, which reads the verdict the mint itself recorded
# through the same two doors the mint uses. Quote that one.
#
# BIAS CHECK. A screen reporting "0 unported" would be indistinguishable from a
# dead parser. This one is handed a shape from both sides in every run
# (reference_absence_screen_must_be_handed_a_shape.md):
#   * `Dialog@UI`      is KNOWN PORTED   -> must land in the present bucket
#   * `StringTable@UI` was KNOWN UNPORTED at wave26/base -> must land in absent
#     (it moves to the present bucket once lane FACTORY's port is in the tree;
#      --expect-unported lets a caller re-aim the control at any row)
# `--self-test` evaluates both and exits non-zero if either fails.
#
# USAGE
#   python tools/mint_factory_census.py [--root .] [--self-test]
#                                       [--expect-ported SYM] [--expect-unported SYM]
# =============================================================================
import argparse
import os
import re
import sys

SOURCE_ROOTS = ('runtime', 'standalone', 'code', 'engine')
SOURCE_EXTS = ('.cpp', '.h', '.inc', '.cc', '.hpp')
# The mint table is the POPULATION, not evidence of a port. Excluding it is the
# whole difference between "102 unported" and "0 unported".
EXCLUDE = ('rt_data_mint_tables.inc',)


def parse_rows(inc_path):
    """Every minted BaseTypeInfo row: index -> (typeinfo symbol, factory name, VA)."""
    lines = open(inc_path, encoding='utf-8', errors='replace').read().splitlines()
    syms, facs = {}, {}
    for ln in lines:
        m = re.search(r'&kTypeInfoStorage\[(\d+)\]', ln)
        if m:
            s = re.match(r'\s*\{\s*"(.*?)"', ln)
            syms[int(m.group(1))] = s.group(1) if s else None
        m = re.search(r'RtMintTiTrapStub_(\d+)_0', ln)
        if m:
            f = re.match(r'\s*(?:"(.*?)"|nullptr)\s*,\s*(0x[0-9a-fA-F]+)ull', ln)
            facs[int(m.group(1))] = (f.group(1) if f and f.group(1) else None,
                                     f.group(2) if f else '0x0')
    return syms, facs


def vtable_screen(inc_path):
    """THE PARALLEL FAMILY, found by lane ATTEND's attempt 28.

    Porting a factory is not enough: the object it constructs is stamped with
    `??_7<Class>@<NS>@@6B@`, and if the mint carries no row for that vtable the
    vptr is NULL and the next virtual call faults. Attempt 28 hit exactly that,
    on the class this lane had just ported, at `GetTypeIdVirtual+0x1f`.

    So the factory slots and the vtable rows are TWO gaps on the SAME 104
    classes, and this screen sizes the second one.

    Class key = the first mangled component, compared between `??_7X@...@6B@`
    and `?_typeInfo@X@...@`. The two symbol kinds compress enclosing-namespace
    back-references DIFFERENTLY (`?_typeInfo@Achievement@1@` vs
    `??_7Achievement@0@6B@`), so a whole-symbol comparison would report a false
    MISSING on every nested class -- that was found by inspection, not assumed.
    """
    txt = open(inc_path, encoding='utf-8', errors='replace').read()
    vt = set(re.findall(r'"(\?\?_7[^"]+)"', txt))
    vt_keys = {}
    for s in vt:
        body = s[4:-4] if s.endswith('@6B@') else s[4:]
        vt_keys[body.split('@')[0]] = s
    ti = re.findall(r'"\?_typeInfo@([^"]+?)@0VBaseTypeInfo', txt)
    have = [t for t in ti if t.split('@')[0] in vt_keys]
    miss = [t for t in ti if t.split('@')[0] not in vt_keys]
    print('')
    print('=== PARALLEL FAMILY: the vtable row each TI class needs ===')
    print('minted vtable rows (by name)          : %d' % len(vt))
    print('TI classes WITH a minted vtable row   : %d' % len(have))
    print('TI classes WITHOUT a minted vtable row: %d' % len(miss))
    print('--- missing (each is a null vptr waiting at cutover) ---')
    for t in miss:
        print('  ??_7%s@6B@' % t)
    return len(miss)


def load_sources(root):
    out = []
    for base in SOURCE_ROOTS:
        d = os.path.join(root, base)
        for dp, _dn, fn in os.walk(d):
            for f in fn:
                if not f.endswith(SOURCE_EXTS) or f in EXCLUDE:
                    continue
                p = os.path.join(dp, f)
                try:
                    out.append((p, open(p, encoding='utf-8', errors='replace').read()))
                except OSError:
                    pass
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default='.')
    ap.add_argument('--self-test', action='store_true')
    ap.add_argument('--vtables', action='store_true',
                    help='also size the parallel gap: TI classes with no minted vtable row')
    ap.add_argument('--expect-ported', default='?CreateNewObject@Dialog@UI@@SAPEAVBase@Object@@XZ')
    ap.add_argument('--expect-unported', default=None,
                    help='a factory mangled name that MUST come back absent; '
                         'omit once every row is ported and the control has no subject')
    a = ap.parse_args()

    inc = os.path.join(a.root, 'standalone', 'rt_data_mint_tables.inc')
    syms, facs = parse_rows(inc)
    hay = load_sources(a.root)
    print('minted BaseTypeInfo rows : %d' % len(syms))
    print('source files scanned     : %d  (roots: %s)'
          % (len(hay), ' '.join(SOURCE_ROOTS)))

    present, absent, nofactory = [], [], []
    for idx in sorted(facs):
        name, va = facs[idx]
        if name is None:
            nofactory.append((idx, syms.get(idx)))
            continue
        hits = [p for p, t in hay if name in t]
        (present if hits else absent).append((idx, syms.get(idx), name, va, hits))

    print('\nslot-0 factory name PRESENT in tree : %d' % len(present))
    print('slot-0 factory name ABSENT from tree : %d' % len(absent))
    print('rows with NO factory in retail either: %d  %s'
          % (len(nofactory), [s for _i, s in nofactory]))

    print('\n--- PRESENT (upper bound on ported; a comment counts as a hit) ---')
    for idx, _s, name, _va, hits in present:
        print('  [%3d] %-64s %s' % (idx, name, os.path.relpath(hits[0], a.root)))

    print('\n--- ABSENT (every one is a cutover wall of the attempt-26 shape) ---')
    for idx, _s, name, va, _h in absent:
        print('  [%3d] %-10s %s' % (idx, va, name))

    if a.vtables:
        vtable_screen(inc)

    if not a.self_test:
        return 0

    rc = 0
    pset = {n for _i, _s, n, _v, _h in present}
    aset = {n for _i, _s, n, _v, _h in absent}
    print('\n--- BIAS CHECK ---')
    if a.expect_ported:
        ok = a.expect_ported in pset
        print('  positive control (must be PRESENT): %s  %s'
              % ('PASS' if ok else '*** FAIL ***', a.expect_ported))
        rc |= 0 if ok else 1
    if a.expect_unported:
        ok = a.expect_unported in aset
        print('  positive control (must be ABSENT) : %s  %s'
              % ('PASS' if ok else '*** FAIL ***', a.expect_unported))
        rc |= 0 if ok else 1
    else:
        print('  absent-side control: NOT SUPPLIED. A zero in the ABSENT bucket '
              'is therefore NOT distinguishable from a dead parser in this run.')
    return rc


if __name__ == '__main__':
    sys.exit(main())
