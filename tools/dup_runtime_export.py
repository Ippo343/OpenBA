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

WHAT WAVE 20 PHASE 0c ADDED, and why the count moved without the tree
getting safer by that amount:

  1. THE TARGET IS NOW PART OF THE FINDING. Two registrations of one key
     that name the SAME function pointer are a no-op -- the later one wins
     and wins with the identical body. Two that name DIFFERENT pointers are
     the shadowing defect. The old script could not tell those apart and
     graded both as "DEAD CODE", which is how a genuine split-brain
     (0x1800e0c80: the name door reached input.cpp's body and the address
     door reached input_config_startup.cpp's) sat in the same undifferentiated
     list as eight harmless conditionals for three waves.

     AND THE FIRST VERSION OF THAT REFINEMENT WAS UNSOUND, in exactly the
     place it mattered. It compared the target IDENTIFIER, and BOTH real
     defects this phase removed spell their two bodies with the SAME
     identifier -- `Video_RenderBuffer_PlatformDestruct` and
     `RtInput_Configuration_Load` each name two different functions in two
     translation units, which is precisely why both linked (one `extern "C"`,
     one C++). Grading by name called both of them harmless no-ops. The
     regression controls caught it: dup_runtime_export_controls.sh C2 and C3
     replay the real pre-fix source out of git, and they FAILED.
     So "same target" now additionally requires that the identifier have at
     most ONE definition anywhere in the scanned tree. Two definitions is the
     defect itself, not evidence against it.

  2. AN ALLOWLIST, because the remaining case is NOT statically decidable.
     A pair guarded by `if (shadow)` / `if (table)` is legitimate exactly
     when those two conditions are provably exclusive, and proving that
     means reading the definitions of both flags and any refusal guard
     between them -- a whole-program dataflow question, not a regex. So the
     allowlist is explicit, per-site, carries the reason and the evidence,
     and PRINTS ITSELF beside the verdict. An allowlist nobody can see is
     indistinguishable from a checker that was quietly weakened.

Exit 0 clean, 3 duplicates found that are neither same-target nor allowlisted.

USAGE
  tools/dup_runtime_export.py [--root DIR] [--show-benign]

This walks the tree relative to --root (default: the repo the script lives
in), NOT the current directory -- several scanners in this project derive
their root from cwd and silently measure a different tree
(reference_scanner_walks_cwd_not_script.md).
"""
import argparse
import os
import re
import sys

NAME_CALL = re.compile(r'RegisterRuntimeExport\s*\(')
ADDR_CALL = re.compile(r'RegisterRuntimeAddr\s*\(')

SKIP_DIRS = {'.git', 'build', 'build-perf', 'obj', '.venv', 'node_modules'}
# tools/fixtures holds BEFORE-snapshots of real source, kept for regression
# tests. Nothing there is compiled (it appears 0 times in
# standalone/build/link_objects.rsp), so a "duplicate" against a fixture is a
# false positive -- verified, not assumed.
SKIP_RELDIRS = {'tools/fixtures'}
EXTS = ('.cpp', '.cc', '.h', '.hpp', '.inc')

# ---------------------------------------------------------------------------
# THE ALLOWLIST. Every entry names the key, the two sites, and the REASON the
# duplicate is legitimate -- and the reason has to be a fact somebody checked,
# not "looks intentional". It is printed in full on every run.
# ---------------------------------------------------------------------------
ALLOWLIST = {
    # -- Object::Internal, four keys x two doors = eight rows. -------------
    # runtime/src/object/internal.cpp registers a SHADOW wrapper under
    # `if (shadow)` and the real TABLE port under `if (table)`. Those are
    # mutually exclusive, and it is provable in one file rather than argued:
    #
    #   internal.cpp:1161  table = noDll || (!tableKilled && (tableForced || !shadow))
    #   internal.cpp:1163  if (shadow && (tableForced || noDll)) { ...REFUSED...; return 0; }
    #
    # Take shadow == true. Either (tableForced || noDll) holds, and the
    # function returns 0 before registering anything at all; or both are
    # false, and table collapses to `!tableKilled && !shadow` == false. So
    # shadow and table are never both live. ClearPersistState does not even
    # need that argument -- its second site is spelled `if (!shadow)`.
    ('NAME', '??0Internal@Object@@QEAA@XZ'):
        'internal.cpp shadow-wrapper vs table-port; exclusive via the '
        'refusal at internal.cpp:1163 plus the !shadow term at :1161',
    ('NAME', '??1Internal@Object@@UEAA@XZ'):
        'internal.cpp shadow-wrapper vs table-port; same proof as ??0Internal',
    ('NAME', '?ClearPersistState@InternalFactory@Object@@QEAAXXZ'):
        'internal.cpp `if (shadow)` vs `if (!shadow)` -- exclusive on its face',
    ('NAME', '?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z'):
        'internal.cpp shadow-wrapper vs table-port; same proof as ??0Internal',
    ('ADDRESS', '0x1800bfd10'): 'address door of ??0Internal@Object@@ -- see that entry',
    ('ADDRESS', '0x1800bfdb0'): 'address door of ??1Internal@Object@@ -- see that entry',
    ('ADDRESS', '0x1800bfe30'): 'address door of ?ClearPersistState@ -- see that entry',
    ('ADDRESS', '0x1800bfe40'): 'address door of ?ResetPersistState@ -- see that entry',

    # -- The allocator's deliberate post-registration repoint. --------------
    # NOT a conditional: both registrations run, in a fixed order, and the
    # second is SUPPOSED to win. allocator.cpp binds the raw byte-faithful
    # bodies; standalone/rt_alloc_swap.cpp's RtAllocSwapPostRegister then
    # repoints both at the composed LOCKED stubs, guarded by `if
    # (!g_swapTaken) return;`. Without it the registry would hand callers an
    # unlocked entry point to the same free list retail's own callers reach
    # through the locked detour -- two entry points, one list, different
    # locking. The overwrite is the fix, not the bug (WAVE9_LANE_E.md
    # "Correction 3").
    ('NAME', '?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z'):
        'deliberate repoint: rt_alloc_swap.cpp:RtAllocSwapPostRegister '
        'replaces the raw body with the composed LOCKED stub, by design',
    ('NAME', '?Free@MemoryAllocator@System@@QEAAXPEAX@Z'):
        'deliberate repoint: same as ?Allocate@ above',
}


def strip_comments(text):
    """Remove // and /* */ so PROSE naming a symbol is not counted as code.

    A checker with no comment stripper reports prose as code, and the prose
    most likely to name a symbol is a comment saying that code was DELETED
    (reference_uncommented_checker_manufactures_work.md).
    """
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text = re.sub(r'//[^\n]*', '', text)
    return text


def split_args(code, open_paren):
    """Return the argument list of the call whose '(' is at open_paren.

    Hand-rolled rather than regexed because the second argument routinely
    contains its own parentheses -- `reinterpret_cast<void*>(&f)` -- and a
    lazy `[^)]*` would truncate it and make two different targets compare
    equal, which is precisely the distinction this script now rests on.
    """
    depth, i, start, args = 0, open_paren, open_paren + 1, []
    while i < len(code):
        c = code[i]
        if c == '"':                      # skip string literals wholesale
            i += 1
            while i < len(code) and code[i] != '"':
                i += 2 if code[i] == '\\' else 1
        elif c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                args.append(code[start:i])
                return args, i
        elif c == ',' and depth == 1:
            args.append(code[start:i])
            start = i + 1
        i += 1
    return None, i


def joined(lit):
    """Concatenate adjacent C string literals into the one symbol they form."""
    return ''.join(re.findall(r'"([^"]*)"', lit))


def norm_target(expr):
    """Normalise a target expression so equal pointers compare equal.

    Only spelling is normalised -- casts, address-of, whitespace. Two
    DIFFERENT identifiers stay different, which is the whole job.
    """
    t = ' '.join(expr.split())
    t = re.sub(r'reinterpret_cast\s*<[^>]*>\s*\(\s*(.*?)\s*\)$', r'\1', t)
    t = re.sub(r'static_cast\s*<[^>]*>\s*\(\s*(.*?)\s*\)$', r'\1', t)
    t = re.sub(r'^\(\s*void\s*\*\s*\)\s*', '', t)
    t = t.lstrip('&').strip()
    return t


# A PROTOTYPE is not a call. `void RegisterRuntimeAddr(uintptr_t ghidra_addr,
# void* our_fn);` appears in three headers and matched as a "call" whose first
# argument was the string "uintptr_t ghidra_addr" -- three sites, one key, a
# textbook self-inflicted false positive. The preceding token settles it: a
# declaration is preceded by its return type, a call never is.
DECL_BEFORE = re.compile(r'(?:^|[;{}\s(])(?:void|bool|int|extern|inline|"C")\s*$')


def is_declaration(code, call_start):
    return bool(DECL_BEFORE.search(code[max(0, call_start - 48):call_start]))


# A function DEFINITION: an identifier, a parameter list, then a brace. Used
# only to answer "does this name have more than one body in the tree?", which
# is the question that separates a harmless repeat registration from two
# distinct functions sharing a name across translation units.
DEF_RE = re.compile(r'\b([A-Za-z_]\w*)\s*\([^;()]*\)\s*(?:const\s*)?\{')


def scan(root):
    names, addrs, unresolved = {}, {}, []
    defs = {}
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
            for dm in DEF_RE.finditer(code):
                defs.setdefault(dm.group(1), set()).add(rel)
            # Report the line number from the ORIGINAL text, not the stripped
            # one -- stripping shifts every line and would misreport locations.
            for rx, bucket, keyof in ((NAME_CALL, names, joined),
                                      (ADDR_CALL, addrs, lambda a: a.strip().lower())):
                for m in rx.finditer(code):
                    if is_declaration(code, m.start()):
                        continue
                    args, _ = split_args(code, m.end() - 1)
                    if not args:
                        continue
                    key = keyof(args[0])
                    if not key:
                        continue
                    tgt = norm_target(args[1]) if len(args) > 1 else '<no-target>'
                    line = code.count('\n', 0, m.start()) + 1
                    site = '%s:~%d' % (rel, line)
                    # An address argument that is not a literal (rt_data_mint
                    # registers `(uintptr_t)r.dllva + 0x08` inside a loop) has
                    # no static value. Keying it by its EXPRESSION TEXT would
                    # both invent duplicates -- two iterations of one loop --
                    # and miss real ones written two ways. It is not decidable
                    # here, so it is REPORTED AS UNEXAMINED rather than
                    # silently compared or silently dropped.
                    if bucket is addrs and not key.startswith('0x'):
                        unresolved.append((site, args[0].strip()))
                        continue
                    bucket.setdefault(key, []).append((site, tgt))
    return names, addrs, files, unresolved, defs


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default=os.path.dirname(here))
    ap.add_argument('--show-benign', action='store_true',
                    help='list the same-target and allowlisted pairs in full')
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
    # A git WORKTREE has .git as a FILE, not a directory. The old isdir test
    # printed "is this the repo you meant?" on every worktree run -- a warning
    # that cried wolf on exactly the trees the lanes actually work in.
    if not os.path.exists(os.path.join(a.root, '.git')):
        print('WARNING: no .git here -- is this the repo you meant to measure?')

    names, addrs, files, unresolved, defs = scan(a.root)

    defects, same_target, allowed = [], [], []
    for label, bucket in (('NAME', names), ('ADDRESS', addrs)):
        for key in sorted(bucket):
            sites = bucket[key]
            if len(sites) < 2:
                continue
            row = (label, key, sites)
            tgts = {t for _, t in sites}
            # ONE spelling is not ONE FUNCTION. If that spelling has two or
            # more definitions in the tree, the two registrations reach two
            # different bodies -- the defect -- however identical they read.
            multi = any(len(defs.get(t, ())) > 1 for t in tgts)
            if len(tgts) == 1 and not multi:
                same_target.append(row)
            elif len(tgts) == 1 and multi:
                where = sorted(defs.get(next(iter(tgts)), ()))
                defects.append((label, key, sites,
                                'ONE NAME, %d DEFINITIONS: %s -- two distinct '
                                'functions share this identifier (different '
                                'linkage), so the doors reach different bodies'
                                % (len(where), ', '.join(where))))
                continue
            elif (label, key) in ALLOWLIST:
                allowed.append(row)
            else:
                defects.append((label, key, sites, None))

    def dump(rows, tail):
        for label, key, sites in rows:
            print('DUPLICATE %s  %s' % (label, key))
            for site, tgt in sites:
                print('    %-58s -> %s' % (site, tgt))
            print('    %s' % tail)

    for label, key, sites, note in defects:
        print('DUPLICATE %s  %s' % (label, key))
        for site, tgt in sites:
            print('    %-58s -> %s' % (site, tgt))
        if note:
            print('    !! %s' % note)
        print('    -> the LAST registration wins; the earlier body is DEAD CODE'
              ' that a census still counts as PORTED')

    if a.show_benign:
        if same_target:
            print('\n-- SAME-TARGET duplicates (no-ops: the winner is the same body) --')
            dump(same_target, '-> both name one function pointer; the overwrite changes nothing')
        if allowed:
            print('\n-- ALLOWLISTED duplicates --')
            dump(allowed, '-> see the allowlist reason printed below')

    # THE ALLOWLIST PRINTS ITSELF, every run, findings or not. An exemption
    # the reader cannot see is a checker that was quietly weakened.
    print('\nALLOWLIST (%d entries; %d of them matched a duplicate this run):'
          % (len(ALLOWLIST), len(allowed)))
    hit = {(l, k) for l, k, _ in allowed}
    for (label, key), reason in sorted(ALLOWLIST.items()):
        print('  [%s] %-4s %s' % ('MATCHED' if (label, key) in hit else '  ---  ', label, key))
        print('          %s' % reason)

    # Quote every green WITH its denominator: a bare "0 duplicates" is
    # quotable out of context and says nothing about what was examined.
    print('\nscanned %d files under %s' % (files, a.root))
    print('%d distinct RegisterRuntimeExport names, %d duplicated'
          % (len(names), sum(1 for r in defects + same_target + allowed if r[0] == 'NAME')))
    print('%d distinct RegisterRuntimeAddr addresses, %d duplicated'
          % (len(addrs), sum(1 for r in defects + same_target + allowed if r[0] == 'ADDRESS')))
    print('of those duplicates: %d DEFECT, %d same-target no-op, %d allowlisted'
          % (len(defects), len(same_target), len(allowed)))
    # THE BLIND SPOT NAMES ITSELF. A scanner that drops what it cannot parse
    # and prints only what it could reads as complete coverage.
    print('NOT EXAMINED: %d RegisterRuntimeAddr call(s) whose address argument is '
          'not a literal (computed at run time; no static value to compare)'
          % len(unresolved))
    for site, expr in unresolved:
        print('    %-58s <- %s' % (site, expr))
    print('SUBJECT: RegisterRuntimeExport/RegisterRuntimeAddr ONLY. This does '
          'NOT cover RegisterDispatchSlot (see dispatch_slot_map.py --check) '
          'nor RegisterExeAddr.')
    print('NOT CLAIMED: that an allowlisted or same-target pair is CORRECT -- '
          'only that its duplication is not a shadowed body. Nor that a '
          'SINGLE registration points at the right function.')
    return 3 if defects else 0


if __name__ == '__main__':
    sys.exit(main())
