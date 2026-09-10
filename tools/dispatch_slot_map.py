#!/usr/bin/env python3
"""Authoritative map: exe .rdata dispatch slot VA -> imported symbol name.

Application-x64.exe DOES have a normal PE import table (contrary to the old
note in CLAUDE.md): 833 imports from Runtime-x64.dll (slots 0x140151218 -
0x140152c18) plus 66 from KERNEL32.dll (0x140151000 - 0x140151208). Our
`RegisterDispatchSlot(addr, ResolveExport(name))` pairs are exactly this table,
so every unregistered slot in that span has a known, mechanical answer.

Usage:
  dispatch_slot_map.py                 # dump the whole table
  dispatch_slot_map.py 0x140151458 ... # look up specific slots
  dispatch_slot_map.py --check         # validate against standalone/*.cpp
"""
import sys, os, re, glob, pefile

EXE = r"C:\GOG Games\Banished\Application-x64.exe"

def build():
    pe = pefile.PE(EXE)
    m = {}
    for d in pe.DIRECTORY_ENTRY_IMPORT:
        dll = d.dll.decode()
        for i in d.imports:
            m[i.address] = (dll, i.name.decode() if i.name else f"#{i.ordinal}")
    return m

slotmap = build()

# Anchored on the SCRIPT, not the CURRENT DIRECTORY. A scanner that globs a
# relative path measures whichever checkout the shell happens to be sitting in,
# which has already made two checkouts' copies report on the same tree twice.
_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def _sources():
    return ([os.path.join(_REPO, "standalone", "address_registry.cpp")]
            + sorted(glob.glob(os.path.join(_REPO, "standalone", "reg_*.cpp"))))


def _strip_comments(text):
    """Remove // line comments and whole-line block-comment bodies.

    A COMMENT IS NOT A REGISTRATION -- the same rule _all_registrations() has
    enforced since Wave 14, which was never applied to its sibling below.
    That omission cost exactly what the rule predicts: reg_mint_promoted.cpp:688
    documents a registration in prose, spelling the symbol TRUNCATED with an
    ellipsis, and --check graded the prose as code and reported a permanent
    phantom MISMATCH at 0x140152870 against a name no source line ever passes.
    CLAUDE.md requires --check to stay at zero, so a phantom here does not just
    mislead -- it retires the gate.
    """
    out = []
    for line in text.splitlines():
        bare = line.lstrip()
        if bare.startswith("//") or bare.startswith("*") or bare.startswith("/*"):
            continue
        out.append(re.sub(r'//.*', '', line))
    return "\n".join(out)

def _registered_pairs():
    """(addr, name) for the ResolveExport form -- the only form whose intended
    symbol is stated in the source, so the only one that can be checked exactly."""
    txt = "".join(_strip_comments(open(f, encoding="utf8", errors="replace").read())
                  for f in _sources())
    # ADJACENT STRING LITERALS ARE ONE NAME. A long mangled symbol is routinely
    # wrapped across two source lines as "...@Object@@" "V?$ExternalPtr@...@Z",
    # which C++ concatenates. Capturing only the first literal reports the name
    # as TRUNCATED and manufactures a MISMATCH against a correct registration --
    # which is exactly what 0x1401528c0 (UI::Dialog::CreateElement) did the
    # moment comment-stripping made that call visible to this scan at all.
    pairs = []
    for addr, lits in re.findall(
            r'RegisterDispatchSlot\(\s*(0x[0-9a-fA-F]+)\s*,\s*'
            r'(?:\(uintptr_t\))?\s*ResolveExport\(\s*((?:"[^"]*"\s*)+)', txt):
        pairs.append((addr, "".join(re.findall(r'"([^"]*)"', lits))))
    return pairs

def _all_registrations():
    """Every RegisterDispatchSlot call in source order, whatever its second
    argument. The registry is a map, so for a repeated address the LAST call
    wins -- and if the winner is not a ResolveExport form, checking the
    ResolveExport lines alone grades DEAD CODE. That happened: 0x140152400 and
    0x140152618 were each registered twice, and both live bindings pointed at
    the same RtSerializer_SerializeBytes helper."""
    out = []
    for f in _sources():
        for i, line in enumerate(open(f, encoding="utf8", errors="replace"), 1):
            # A COMMENT IS NOT A REGISTRATION. Without this, prose that names a
            # RegisterDispatchSlot call -- and the prose most likely to name one
            # is a comment explaining where a slot IS registered -- is graded as
            # a second binding and reported as a SHADOWED address. Wave 14 hit
            # exactly that: lane SINGLETON's ordering note in reg_mint_promoted.cpp
            # mentioned RegisterDispatchSlot(0x140151610, ...) and --check went
            # from 0 shadowed to 1, naming a dead line that does not exist.
            bare = line.lstrip()
            if bare.startswith("//") or bare.startswith("*"):
                continue
            line = re.sub(r'//.*', '', line)
            m = re.search(r'RegisterDispatchSlot\(\s*(0x[0-9a-fA-F]+)\s*,\s*(.*)', line)
            if m:
                out.append((int(m.group(1), 16), f, i, m.group(2).strip().rstrip(');')))
    return out

def _report_shadowed():
    """Addresses registered more than once, where a later call overrides an
    earlier one."""
    seen = {}
    for addr, f, ln, arg in _all_registrations():
        seen.setdefault(addr, []).append((f, ln, arg))
    bad = 0
    for addr in sorted(seen):
        rows = seen[addr]
        if len(rows) < 2:
            continue
        bad += 1
        pe = slotmap.get(addr)
        print(f"SHADOWED 0x{addr:x} registered {len(rows)}x -- LAST WINS"
              f"   (PE: {pe[1] if pe else '-'})")
        for j, (f, ln, arg) in enumerate(rows):
            tag = "WINS " if j == len(rows) - 1 else "dead "
            print(f"    {tag} {f}:{ln}  {arg[:78]}")
    return bad

if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "--check":
        bad = 0
        shadowed = _report_shadowed()
        if shadowed:
            print()
        for addr, name in _registered_pairs():
            e = slotmap.get(int(addr, 16))
            if not e:
                print("NOT AN IMPORT SLOT", addr, name); bad += 1
            elif e[1] != name:
                print("MISMATCH", addr, "PE:", e[1], "| code:", name); bad += 1
        print(f"checked {len(_registered_pairs())} ResolveExport registrations, "
              f"{bad} name mismatch(es); {shadowed} address(es) registered more "
              f"than once (a non-ResolveExport winner means the checked line is "
              f"dead code)")
    elif args:
        for a in args:
            e = slotmap.get(int(a, 16))
            print(f"{a} -> {e[0] + ' ' + e[1] if e else '(NOT AN IMPORT SLOT)'}")
    else:
        for s in sorted(slotmap):
            dll, nm = slotmap[s]
            print(f"0x{s:09x} {dll:18s} {nm}")
