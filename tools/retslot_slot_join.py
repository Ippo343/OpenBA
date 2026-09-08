"""Join dispatch-slot fn_* assignments to their MANGLED NAMES and flag any whose
return type needs a hidden return slot the typedef does not declare.

WHY THIS EXISTS. tools/abi_retslot_audit.py reports 0 suspicious of 48 and that
number is structurally blind to this whole class: its RE_ASSIGN requires
    fn_X = reinterpret_cast<fn_X_t>(ResolveExport("<mangled>"))
and these are
    fn_X = *reinterpret_cast<fn_X_t*>(ResolveAddr(Module::Exe, 0x140...))
read out of a PE dispatch slot, with NO mangled name anywhere in the expression
to classify. Lane UIF found two of them already wrong (both SetScissor typedefs
omitted the slot, which would write 16 bytes through a sign-extended int) and
noted the fix is not a better regex: the slot address has to be resolved to a
name, which tools/dispatch_slot_map.py already does from the binary's import
table. This is that join.
"""
import os, re, subprocess, sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
SRC = os.path.join(ROOT, "engine", "funcs", "engine_funcs.cpp")
HDR = os.path.join(ROOT, "engine", "funcs", "engine_funcs.h")
MAPPER = os.path.join(ROOT, "tools", "dispatch_slot_map.py")
PY = r"C:/code/GIT/untitled_rev_eng_project/tools/ghidra_mcp/.venv/Scripts/python.exe"

text = open(SRC, encoding="utf-8", errors="replace").read()

# fn_NAME ... = ... ResolveAddr(Module::Exe, 0xADDR)   -- may span lines.
PAT = re.compile(
    r"\b(fn_[A-Za-z0-9_]+)\s*=\s*[^;]*?ResolveAddr\s*\(\s*Module::Exe\s*,\s*(0x[0-9a-fA-F]+)[^;]*?;",
    re.S)

sites = []
for m in PAT.finditer(text):
    name, addr = m.group(1), m.group(2)
    line = text.count("\n", 0, m.start()) + 1
    sites.append((name, addr, line))

print("dispatch-slot fn_* assignments found: %d" % len(sites))
if not sites:
    sys.exit(0)

hdr = open(HDR, encoding="utf-8", errors="replace").read()

# A mangled return of a CLASS BY VALUE is "?A" + V/U/T sitting in the RETURN
# POSITION: immediately after the access+convention code (QEAA / QEBA / AEAA /
# SA / CA / UEAA ...), which always ends in 'A' for __cdecl/__thiscall here.
#
# *** MY FIRST VERSION OF THIS WAS WRONG AND A KNOWN-POSITIVE CAUGHT IT. *** It
# took `mangled.split("@@")[-1]` and searched that, i.e. the LAST @@-segment --
# which for
#     ?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@HHHH@Z
# is "HHHH@Z". The `?AV` sits mid-name, so the check silently returned "no slot
# needed" for the two typedefs already PROVEN wrong. It would have reported
# 0 flagged and read as a clean sweep.
BYVAL = re.compile(r"@@[A-Z]{1,2}E?[AB]?A\?A[VUT]")

flagged, ok, unknown = [], [], []
for name, addr, line in sites:
    try:
        out = subprocess.run([PY, MAPPER, addr], cwd=ROOT, capture_output=True,
                             text=True, timeout=60).stdout
    except Exception as e:
        unknown.append((name, addr, line, "mapper failed: %s" % e))
        continue
    mm = re.search(r"(\?[A-Za-z0-9_@\$\?]+)", out)
    if not mm:
        unknown.append((name, addr, line, out.strip().splitlines()[:1]))
        continue
    mangled = mm.group(1)

    # Does the typedef for this fn declare a slot? Find "typedef RET (*fn_X_t)(ARGS)"
    td = re.search(r"typedef\s+[^;]*?\(\s*\*\s*%s_t\s*\)\s*\(([^;]*?)\)\s*;" % re.escape(name), hdr, re.S)
    args = td.group(1) if td else None

    # Heuristic: a by-value class return needs the slot as arg 2 (after this) or
    # arg 1 (free function). We only FLAG; a human reads the row.
    needs = bool(BYVAL.search(mangled))
    row = (name, addr, line, mangled, (args or "").replace("\n", " ")[:90])
    if needs:
        flagged.append(row)
    else:
        ok.append(row)

print("\n=== BY-VALUE CLASS RETURN -> needs a hidden slot: %d ===" % len(flagged))
for r in flagged:
    print("  %-42s %s  engine_funcs.cpp:%d" % (r[0], r[1], r[2]))
    print("      %s" % r[3])
    print("      typedef args: %s" % (r[4] or "<typedef not found>"))
print("\n=== scalar/void return, no slot needed: %d ===" % len(ok))
print("\n=== unresolved by the mapper: %d ===" % len(unknown))
for u in unknown:
    print("  %-42s %s  line %d  %s" % (u[0], u[1], u[2], u[3]))
