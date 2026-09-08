#!/usr/bin/env python3
"""detour_target_audit.py -- a detour must aim at the SHIPPED body, never at ours.

WHY THIS EXISTS
---------------
`standalone/resolve.h` states the rule in as many words:

    Anything that patches, detours, trampolines to, or diff-tests against the
    ORIGINAL DLL code must resolve through the *Raw* variants below, or it will
    happily patch our own reimplementation once the port registers.

and `standalone/runtime_detour.h` repeats it.  Wave 9 lane FNHOOK found that
**every one of the ten detour installations in the tree broke it**: each passed
`ResolveAddr(Module::Runtime, 0x...)`, the door that consults the port registry
FIRST.  Five of the ten targets had since been ported, so the installer aimed at
our own GCC prologue, the installer's byte guard refused the patch, and the
instrument went dead with one line of stderr:

    [FNHook] Filename::~Filename prologue at 00007ff6dea82508 mismatch — refusing to patch
                                            ^^^^^^^^^^^^^^^^ the EXE's base, not the DLL's

`BANISHED_READ_GUARD` had been completely inert since the `CompressedStream`
port landed; after the fix its wrapper served 104,069,297 calls in one run.

WHY THE EXISTING CHECKERS DO NOT CATCH IT
-----------------------------------------
`port_bypass_audit.py` classifies a `ResolveAddr(Module::Runtime, X)` whose X is
a registered port as **ADDR-REGISTERED**, i.e. correct -- and for an ordinary
caller it is exactly correct, that is the whole point of the seam.  A detour
installer is the one caller for which reaching our port is the DEFECT.  The
audit's model has no way to tell the two apart, so it reported 0 bypasses on a
tree with five mis-aimed detours.  `rawdoor_audit.py` looks at the other
direction (raw doors called on a live path) and cannot see this either.

WHAT IT CHECKS
--------------
1. The target argument of a detour installer -- `RuntimeDetourInstall`,
   `DetourRuntimeFunc`, `InstallDetour`, `InstallDetourRSPSafe`,
   `InstallVtableDetour` -- must not be produced by a NON-raw resolve, whether
   spelled inline or through a local assigned earlier in the same function.
2. A pointer handed to `VirtualProtect(..., PAGE_EXECUTE_*)` -- the hand-rolled
   patch shape, which is how `filename_runtime_hook.cpp`'s `MemFreeStatic` watch
   is written -- must not come from a non-raw resolve either.

FAILING CLOSED: exit 1 on any finding.  Zero findings is the expected state and
the check is cheap, so wire it next to the other audits.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIRS = ["standalone", "runtime", "code", "engine", "dx11"]

INSTALLERS = (
    "RuntimeDetourInstall",
    "DetourRuntimeFunc",
    "InstallDetour",
    "InstallDetourRSPSafe",
    "InstallVtableDetour",
)
# A resolve that consults the port registry. ResolveExportRaw / ResolveOrdinalRaw /
# ResolveRuntimeRaw are the correct doors and must NOT match, hence the (?!Raw).
NONRAW = re.compile(r"\bResolve(?:Addr|Export|Ordinal)\b(?!Raw)")
ASSIGN = re.compile(r"(?:^|[;{}\s])(?:const\s+)?(?:uintptr_t|void\s*\*|auto)\s+"
                    r"(\w+)\s*=\s*([^;]*);", re.S)


def first_arg(text, open_paren):
    """The first argument of the call whose '(' is at open_paren."""
    depth, i, n = 0, open_paren, len(text)
    start = open_paren + 1
    while i < n:
        c = text[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return text[start:i]
        elif c == "," and depth == 1:
            return text[start:i]
        i += 1
    return text[start:]


def enclosing_body(text, pos):
    """Rough brace-matched body containing pos: back up to the previous
    column-0 '}' or start of file. Good enough -- these are all file-scope
    functions and a too-large window only makes the check MORE conservative."""
    prev = text.rfind("\n}\n", 0, pos)
    return text[prev + 1 if prev >= 0 else 0: pos]


def nonraw_locals(body):
    """Locals in `body` assigned from a non-raw resolve."""
    out = {}
    for m in ASSIGN.finditer(body):
        name, rhs = m.group(1), m.group(2)
        if NONRAW.search(rhs):
            out[name] = rhs.strip()
    return out


def scan(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    rel = path.relative_to(ROOT).as_posix()
    findings = []

    def line_of(pos):
        return text.count("\n", 0, pos) + 1

    for name in INSTALLERS:
        for m in re.finditer(r"\b%s\s*\(" % name, text):
            # A declaration or the definition itself is not a call site.
            head = text[max(0, m.start() - 80):m.start()]
            if re.search(r"(?:void\s*\*|bool)\s*$", head):
                continue
            arg = first_arg(text, m.end() - 1)
            body = enclosing_body(text, m.start())
            if NONRAW.search(arg):
                findings.append((rel, line_of(m.start()), name,
                                 "target argument uses a non-raw resolve: "
                                 + " ".join(arg.split())))
                continue
            tok = arg.strip()
            if re.fullmatch(r"\w+", tok):
                loc = nonraw_locals(body)
                if tok in loc:
                    findings.append((rel, line_of(m.start()), name,
                                     "target local '%s' assigned from a non-raw "
                                     "resolve: %s" % (tok, " ".join(loc[tok].split()))))

    for m in re.finditer(r"\bVirtualProtect\s*\(", text):
        call = first_arg(text, m.end() - 1)
        window = text[m.start():m.start() + 400]
        if "PAGE_EXECUTE" not in window:
            continue
        body = enclosing_body(text, m.start())
        loc = nonraw_locals(body)
        for var, rhs in loc.items():
            if re.search(r"\b%s\b" % re.escape(var), call):
                findings.append((rel, line_of(m.start()), "VirtualProtect",
                                 "patch target '%s' assigned from a non-raw "
                                 "resolve: %s" % (var, " ".join(rhs.split()))))
    return findings


def main():
    all_findings = []
    for d in SCAN_DIRS:
        base = ROOT / d
        if not base.is_dir():
            continue
        for p in sorted(list(base.rglob("*.cpp")) + list(base.rglob("*.h"))):
            all_findings += scan(p)

    if all_findings:
        print("detour_target_audit: %d MIS-AIMED DETOUR TARGET(S)\n" % len(all_findings))
        for rel, line, who, why in all_findings:
            print("  %s:%d  %s -- %s" % (rel, line, who, why))
        print("\nUse ResolveRuntimeRaw / ResolveExportRaw, or hand the installer the")
        print("GHIDRA address and let it resolve raw itself (the current contract).")
        return 1
    print("detour_target_audit: clean -- every detour target resolves RAW")
    return 0


if __name__ == "__main__":
    sys.exit(main())
