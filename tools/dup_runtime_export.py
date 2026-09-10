#!/usr/bin/env python3
"""Find DUPLICATE RegisterRuntimeExport / RegisterRuntimeAddr registrations.

WHY THIS EXISTS, and it is the whole point of the file:

`tools/dispatch_slot_map.py --check` reports "0 address(es) registered more
than once" and that green is about `RegisterDispatchSlot(...)` ONLY -- the
exe's .rdata IAT slots. It is STRUCTURALLY BLIND to `RegisterRuntimeExport`,
which is how a ported Runtime body is bound by NAME, and to
`RegisterRuntimeAddr`, which binds by ADDRESS.

In Wave 19 the integrator quoted that green TWICE as evidence that a
duplicated `?GetSource@Stream@IO@@` port had not produced a shadowed
registration. It could never have shown that. Lane IOBASE caught it.
(WAVE19_INTEGRATOR_CORRECTIONS.md C29.)

A duplicate here is not cosmetic: the later registration wins, and the
earlier one becomes DEAD CODE that still reads as a live port to a census
and to a reviewer.

Exit 0 clean, 3 duplicates found.

USAGE
  tools/dup_runtime_export.py [--root DIR]

This walks the tree relative to --root (default: the repo the script lives
in), NOT the current directory -- several scanners in this project derive
their root from cwd and silently measure a different tree
(reference_scanner_walks_cwd_not_script.md).
"""
import argparse
import os
import re
import sys

# RegisterRuntimeExport("mangled", ptr)  -- the NAME door
NAME_RE = re.compile(r'RegisterRuntimeExport\(\s*((?:"[^"]*"\s*)+)')
# RegisterRuntimeAddr(0x..., ptr)        -- the ADDRESS door
ADDR_RE = re.compile(r'RegisterRuntimeAddr\(\s*(0x[0-9a-fA-F]+)')

SKIP_DIRS = {'.git', 'build', 'build-perf', 'obj', '.venv', 'node_modules'}
# tools/fixtures holds BEFORE-snapshots of real source, kept for regression
# tests. Nothing there is compiled (it appears 0 times in
# standalone/build/link_objects.rsp), so a "duplicate" against a fixture is a
# false positive -- verified, not assumed.
SKIP_RELDIRS = {'tools/fixtures'}
EXTS = ('.cpp', '.cc', '.h', '.hpp', '.inc')


def strip_comments(text):
    """Remove // and /* */ so PROSE naming a symbol is not counted as code.

    A checker with no comment stripper reports prose as code, and the prose
    most likely to name a symbol is a comment saying that code was DELETED
    (reference_uncommented_checker_manufactures_work.md).
    """
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text = re.sub(r'//[^\n]*', '', text)
    return text


def joined(lit):
    """Concatenate adjacent C string literals into the one symbol they form."""
    return ''.join(re.findall(r'"([^"]*)"', lit))


def scan(root):
    names, addrs = {}, {}
    files = 0
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        rel_dir = os.path.relpath(dirpath, root).replace('\\', '/')
        if rel_dir in SKIP_RELDIRS:
            dirnames[:] = []
            continue
        for fn in filenames:
            if not fn.endswith(EXTS):
                continue
            p = os.path.join(dirpath, fn)
            try:
                with open(p, 'r', encoding='utf-8', errors='replace') as f:
                    raw = f.read()
            except OSError:
                continue
            files += 1
            code = strip_comments(raw)
            rel = os.path.relpath(p, root).replace('\\', '/')
            # Report the line number from the ORIGINAL text, not the stripped
            # one -- stripping shifts every line and would misreport locations.
            for m in NAME_RE.finditer(code):
                sym = joined(m.group(1))
                if sym:
                    line = code.count('\n', 0, m.start()) + 1
                    names.setdefault(sym, []).append('%s:~%d' % (rel, line))
            for m in ADDR_RE.finditer(code):
                va = m.group(1).lower()
                line = code.count('\n', 0, m.start()) + 1
                addrs.setdefault(va, []).append('%s:~%d' % (rel, line))
    return names, addrs, files


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.dirname(here))
    a = ap.parse_args()

    # PRINT THE SUBJECT FIRST, BEFORE ANY FINDING.
    #
    # This line used to sit at the BOTTOM, under up to a thousand finding
    # lines. Lane PERSIST ran the script from a copy in /tmp, so --root
    # defaulted to the parent of THAT directory and it scanned 68,287 files
    # under C:\Users\USPC\AppData\Local -- counting every agent's scratchpad
    # copy of the repo as a duplicate of every other, and reporting
    # "1029 of 1066 names duplicated". A confident, catastrophically wrong
    # answer with the one line that would expose it buried beneath it.
    #
    # An instrument that names its subject only AFTER its findings has not
    # really named it: the reader has already formed a conclusion. Same defect,
    # and same fix, as the abi_retslot_audit headline (Wave 19, 1ad9e8e), where
    # the computation had to be hoisted above the print.
    print('SUBJECT ROOT: %s' % a.root)
    if not os.path.isdir(os.path.join(a.root, '.git')):
        print('WARNING: no .git here -- is this the repo you meant to measure?')

    names, addrs, files = scan(a.root)
    dup_n = {k: v for k, v in names.items() if len(v) > 1}
    dup_a = {k: v for k, v in addrs.items() if len(v) > 1}

    for label, dups in (('NAME', dup_n), ('ADDRESS', dup_a)):
        for k in sorted(dups):
            print('DUPLICATE %s  %s' % (label, k))
            for site in dups[k]:
                print('    %s' % site)
            print('    -> the LAST registration wins; the others are DEAD CODE')

    # Quote every green WITH its denominator: a bare "0 duplicates" is
    # quotable out of context and says nothing about what was examined.
    print('scanned %d files under %s' % (files, a.root))
    print('%d distinct RegisterRuntimeExport names, %d duplicated' % (len(names), len(dup_n)))
    print('%d distinct RegisterRuntimeAddr addresses, %d duplicated' % (len(addrs), len(dup_a)))
    print('SUBJECT: RegisterRuntimeExport/RegisterRuntimeAddr ONLY. This does '
          'NOT cover RegisterDispatchSlot (see dispatch_slot_map.py --check) '
          'nor RegisterExeAddr.')
    return 3 if (dup_n or dup_a) else 0


if __name__ == '__main__':
    sys.exit(main())
