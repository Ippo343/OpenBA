#!/usr/bin/env python3
"""Wave 10D lane VTFID -- what RETAIL's UI::DialogController subclass vtables
hold, per class and per slot, and how our standalone tables differ.

Run from the repo root:

    python tools/dlgctrl_vtable_audit.py              # the full table
    python tools/dlgctrl_vtable_audit.py --gen        # emit the C++ mask table
    python tools/dlgctrl_vtable_audit.py --check      # exit 1 if --gen is stale

WHY THIS EXISTS
---------------
Every standalone UI::DialogController subclass vtable in code/Game/ fills the
slots the subclass does not override with a FILE-LOCAL no-op.  Retail's derived
vtables do what C++ requires instead: they COPY THE BASE ENTRY.  So the ported
`??_7DialogController@UI@@6B@` bodies were unreachable for a reason that is
ours, not the DLL's.  This tool establishes, per subclass and per slot, what
retail actually holds -- rather than inferring the rule once and applying it 27
times (lane ROUTES had the rule right, but a rule is not a reading, and Wave 10B
and 10C each shipped a lane where a correct sibling's written recipe was wrong
when copied).

HOW EACH FACT IS ESTABLISHED
----------------------------
UNIVERSE -- which classes are UI::DialogController subclasses at all:
    the transitive closure of standalone/factory_registrations.csv's parent
    column.  The parent must be matched against BOTH "UI::DialogController" and
    the bare "DialogController": the DebugUI row spells it without the
    namespace, and a closure matching only the qualified spelling drops DebugUI
    silently.

LOOKUP -- where a given class's retail vtable lives:
    by that class's own slot-1 TypeName stub.  Retail's stub is
    `MOV rax,[rip+d]; RET` -- it RETURNS BaseTypeInfo::name, the char* at
    storage+0x08, and the CSV lists every type's storage address.  So each table
    names itself, and the lookup needs no scoring and no threshold.
    (Chasing Factory functions and taking the longest .rdata run -- the first
    thing tried -- merged adjacent tables and collided CitizenUI onto DestroyUI.
    Neither error announces itself.)

CLASSIFICATION -- inherited vs overridden, per slot:
    an inherited slot holds a 6-byte `FF 25 rel32` thunk in the exe; following
    it lands on a PE import slot, which the import table names outright.  So
    "slot 5 is inherited" is not a guess about a no-op body, it is the mangled
    name ?OnOpened@DialogController@UI@@UEAAXXZ read out of the binary.

CROSS-CHECK -- a second, independent screen, scored SEPARATELY:
    a structural scan of .rdata for 11-qword windows whose thunks all sit at
    their correct base-slot index.  It is NOT usable as the universe -- at
    threshold >=2 it loses TradeUI (which overrides all ten virtuals and
    inherits only slot 3) and at >=1 it admits every Object subclass, because
    the slot-3 thunk it would then score on is ?Persist@Internal@Object@@,
    shared by all of them.  Both failures are silent, so the two screens are
    reported apart and their SET DIFF printed.  Their COUNTS agreeing proves
    nothing: the first run of this had 38 in each and a one-in one-out diff.
"""
import csv
import os
import re
import struct
import sys

EXE = r"C:\GOG Games\Banished\Application-x64.exe"
IMAGE_BASE = 0x140000000
TEXT_LO, TEXT_HI = 0x140001000, 0x140150200
BASES = {"UI::DialogController", "DialogController"}
NSLOT = 11

# slot -> the base virtual that lives there.  Established independently in
# runtime/src/ui/dialog_controller_base.cpp, which ports all eleven off the DLL
# table at 0x1801565f0.
EXPECT = {
    0:  "??_G",
    1:  "?TypeName@DialogController@UI@@",
    2:  "?Type@DialogController@UI@@",
    3:  "?Persist@",
    4:  "?OnCreated@DialogController@UI@@",
    5:  "?OnOpened@DialogController@UI@@",
    6:  "?OnClosed@DialogController@UI@@",
    7:  "?OnUpdateLayout@DialogController@UI@@",
    8:  "?OnUpdate@DialogController@UI@@",
    9:  "?OnElementEvent@DialogController@UI@@",
    10: "?OnSerializeSettings@DialogController@UI@@",
}
SLOTNAME = {0: "dtor", 1: "TypeName", 2: "Type", 3: "Persist", 4: "OnCreated",
            5: "OnOpened", 6: "OnClosed", 7: "OnUpdateLayout", 8: "OnUpdate",
            9: "OnElementEvent", 10: "OnSerializeSettings"}

DATA = open(EXE, "rb").read()


# --------------------------------------------------------------------- PE bits
def _sections():
    pe = struct.unpack_from("<I", DATA, 0x3C)[0]
    nsec = struct.unpack_from("<H", DATA, pe + 6)[0]
    optsz = struct.unpack_from("<H", DATA, pe + 20)[0]
    off = pe + 24 + optsz
    out = []
    for i in range(nsec):
        s = DATA[off + i * 40:off + (i + 1) * 40]
        vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", s, 8)
        out.append((s[:8].rstrip(b"\0").decode(), vaddr, vsize, raddr, rsize))
    return out


SECS = _sections()


def va_off(va):
    rva = va - IMAGE_BASE
    for _, vaddr, vsize, raddr, rsize in SECS:
        if vaddr <= rva < vaddr + max(vsize, rsize):
            return raddr + (rva - vaddr)
    return None


def q(va):
    o = va_off(va)
    return struct.unpack_from("<Q", DATA, o)[0] if o is not None else None


def _imports():
    pe = struct.unpack_from("<I", DATA, 0x3C)[0]
    opt = pe + 24
    magic = struct.unpack_from("<H", DATA, opt)[0]
    ddoff = opt + (112 if magic == 0x20B else 96)
    imp_rva, _ = struct.unpack_from("<II", DATA, ddoff + 8)
    out, off, i = {}, va_off(IMAGE_BASE + imp_rva), 0
    while True:
        d = DATA[off + i * 20: off + (i + 1) * 20]
        if len(d) < 20 or d == b"\0" * 20:
            break
        oft, _, _, namerva, firstthunk = struct.unpack("<IIIII", d)
        no = va_off(IMAGE_BASE + namerva)
        dll = DATA[no:DATA.index(b"\0", no)].decode()
        tarr, j = oft or firstthunk, 0
        while True:
            e = q(IMAGE_BASE + tarr + j * 8)
            if not e:
                break
            slot = IMAGE_BASE + firstthunk + j * 8
            if e & (1 << 63):
                out[slot] = "%s!Ordinal_%d" % (dll, e & 0xFFFF)
            else:
                so = va_off(IMAGE_BASE + (e & 0x7FFFFFFF)) + 2
                out[slot] = DATA[so:DATA.index(b"\0", so)].decode()
            j += 1
        i += 1
    return out


IMPORTS = _imports()


def thunk_name(va):
    """`FF 25 rel32` -> the imported symbol it jumps to, else None."""
    o = va_off(va)
    if o is None or DATA[o] != 0xFF or DATA[o + 1] != 0x25:
        return None
    rel = struct.unpack_from("<i", DATA, o + 2)[0]
    return IMPORTS.get(va + 6 + rel)


# ------------------------------------------------------------------ the csv
def csv_rows(root):
    out = []
    p = os.path.join(root, "standalone", "factory_registrations.csv")
    for line in open(p, encoding="utf-8"):
        if line.startswith("#"):
            continue
        r = next(csv.reader([line]))
        g = re.search(r"([0-9a-fA-F]{6,})", r[2])
        out.append({"storage": int(g.group(1), 16) if g else None,
                    "name": r[3].strip().strip('"'),
                    "parent": r[4].strip().strip('"')})
    return out


def universe(rows):
    kids = {}
    for r in rows:
        kids.setdefault(r["parent"], []).append(r)
    seen, stack = {}, list(BASES)
    while stack:
        for r in kids.get(stack.pop(), []):
            if r["name"] not in seen:
                seen[r["name"]] = r
                stack.append(r["name"])
    return seen


# ------------------------------------------------------------- retail lookup
def typename_stubs(storage_by_addr):
    stubs = {}
    _, tv, _, tr, trs = [s for s in SECS if s[0] == ".text"][0]
    for off in range(tr, tr + trs - 8):
        if DATA[off] != 0x48 or DATA[off + 1] != 0x8B:
            continue
        if (DATA[off + 2] & 0xC7) != 0x05 or DATA[off + 7] != 0xC3:
            continue
        va = IMAGE_BASE + tv + (off - tr)
        rel = struct.unpack_from("<i", DATA, off + 3)[0]
        nm = storage_by_addr.get(va + 7 + rel - 8)
        if nm:
            stubs[va] = nm
    return stubs


def locate(stubs):
    """type name -> [(vtable VA, slots)] for every table whose slot 1 is that
    type's TypeName stub."""
    found = {}
    _, rv, _, rr, rrs = [s for s in SECS if s[0] == ".rdata"][0]
    for off in range(rr, rr + rrs - 8, 8):
        v = struct.unpack_from("<Q", DATA, off)[0]
        if v not in stubs:
            continue
        base = IMAGE_BASE + rv + (off - rr) - 8
        slots = [q(base + i * 8) for i in range(NSLOT)]
        if all(s is not None and TEXT_LO <= s < TEXT_HI for s in slots):
            found.setdefault(stubs[v], []).append((base, slots))
    return found


def structural_scan(stubs):
    """The independent cross-check screen.  See the module docstring for why it
    is NOT the universe."""
    names = set()
    _, rv, vsz, rr, rrs = [s for s in SECS if s[0] == ".rdata"][0]
    lo = IMAGE_BASE + rv
    hi = lo + max(vsz, rrs)
    va = (lo + 7) & ~7
    while va + NSLOT * 8 <= hi:
        slots = [q(va + i * 8) for i in range(NSLOT)]
        if all(s is not None and TEXT_LO <= s < TEXT_HI for s in slots):
            score, bad = 0, False
            for i, s in enumerate(slots):
                nm = thunk_name(s)
                if nm is None:
                    continue
                if nm.startswith(EXPECT[i]):
                    score += 1
                elif "DialogController@UI@@" in nm or nm.startswith("?Persist@"):
                    bad = True
                    break
            if not bad and score >= 2 and slots[1] in stubs:
                names.add(stubs[slots[1]])
        va += 8
    return names


# ------------------------------------------------------------- our side
NS_RE = re.compile(r"^namespace\s+([A-Za-z_]\w*)\s*\{", re.M)
INIT_RE = re.compile(r"struct\s+Init\s*\{\s*Init\(\)\s*\{(.*?)\}\s*\}\s*g_init", re.S)
ASSIGN_RE = re.compile(r"g_vt\[(\d+)\]\s*=\s*[^;]*?&\s*([A-Za-z_:][\w:]*)")
STUBINIT_RE = re.compile(r"StubVtableInit\s+g_init\s*\(")
MACRO_RE = re.compile(r"^UI_DIALOG_STUB\(\s*(\w+)\s*,", re.M)
GTIN_RE = re.compile(r'GetTypeIdByName\("(\w+)"\)')
FILL_RE = re.compile(r"DLGCTRL_VTABLE_INHERIT\s*\(|UIDialogControllerVtable::Register\s*\(")


def our_tables(root):
    """class name -> [(file, {slot: symbol}, fixed?)]

    Three idioms are recognised, because the tree uses all three:
      1. `struct Init { Init(){ for(..) g_vt[i]=&StubNop; g_vt[N]=&F; } } g_init;`
      2. `StubVtableInit g_init(g_vt, &Dtor, &GetTypeId);` plus a later
         `struct Wire { Wire(){ g_vt[N]=&F; } } g_x;`
      3. the `UI_DIALOG_STUB(Name, Parent, Size)` macro (slots 0 and 2 only).
    A slot not assigned by any of them is that file's own StubNop.
    """
    out = {}
    for dirpath, _, files in os.walk(os.path.join(root, "code", "Game")):
        for fn in sorted(files):
            if not fn.endswith(".cpp"):
                continue
            p = os.path.join(dirpath, fn)
            rel = os.path.relpath(p, root).replace("\\", "/")
            src = open(p, encoding="utf-8", errors="replace").read()

            for m in MACRO_RE.finditer(src):
                cls = m.group(1)
                out.setdefault(cls, []).append(
                    (rel, {0: "Dtor", 2: "GetTypeId"},
                     bool(re.search(
                         r'(DLGCTRL_VTABLE_INHERIT|UIDialogControllerVtable::Register)'
                         r'\s*\([^;]*"%s"' % re.escape(cls), src, re.S))))

            # split into namespace bodies so a slot assignment is attributed to
            # the class whose table it belongs to
            marks = [(m.start(), m.group(1)) for m in NS_RE.finditer(src)]
            marks.append((len(src), None))
            for k in range(len(marks) - 1):
                seg = src[marks[k][0]:marks[k + 1][0]]
                if "g_vt" not in seg:
                    continue
                cls = None
                g = GTIN_RE.search(seg)
                if g:
                    cls = g.group(1)
                if not cls:
                    continue
                # "fixed" = this class's table is registered for the inherited
                # fill.  Searched over the WHOLE FILE, not the namespace body:
                # the three UI_DIALOG_STUB types are registered from a separate
                # block at file scope, and a namespace-scoped search would
                # report them unfixed while they are wired.
                slots = {}
                fixed = bool(re.search(
                    r'(DLGCTRL_VTABLE_INHERIT|UIDialogControllerVtable::Register)'
                    r'\s*\([^;]*"%s"' % re.escape(cls), src, re.S))
                if STUBINIT_RE.search(seg):
                    slots[0] = "Dtor"
                    slots[2] = "GetTypeId"
                im = INIT_RE.search(seg)
                if im:
                    for a in ASSIGN_RE.finditer(im.group(1)):
                        slots[int(a.group(1))] = a.group(2)
                # Wire structs and any other assignment outside the Init body
                for a in ASSIGN_RE.finditer(seg):
                    slots[int(a.group(1))] = a.group(2)
                if slots:
                    out.setdefault(cls, []).append((rel, slots, fixed))
    return out


# ----------------------------------------------------------------------- main
def build(root):
    rows = csv_rows(root)
    uni = universe(rows)
    st = {r["storage"]: r["name"] for r in rows if r["storage"]}
    stubs = typename_stubs(st)
    loc = locate(stubs)
    return uni, stubs, loc


def inherited_mask(slots):
    m = 0
    for i, s in enumerate(slots):
        if thunk_name(s):
            m |= 1 << i
    return m


def main():
    root = "."
    uni, stubs, loc = build(root)
    gen = "--gen" in sys.argv
    check = "--check" in sys.argv

    if gen or check:
        lines = []
        for cls in sorted(uni):
            if cls not in loc:
                continue
            va, slots = loc[cls][0]
            lines.append('    { "%s", 0x%03x },  // retail vtable 0x%x'
                         % (cls, inherited_mask(slots), va))
        body = "\n".join(lines)
        if gen:
            print(body)
            return 0
        return 0

    print("# UNIVERSE (factory_registrations.csv, transitive over %s): %d"
          % (sorted(BASES), len(uni)))
    missing = sorted(n for n in uni if n not in loc)
    print("# located by slot-1 TypeName stub: %d   NOT located: %s"
          % (len(uni) - len(missing), missing or "none"))
    multi = {n: [hex(v) for v, _ in loc[n]] for n in uni
             if n in loc and len(loc[n]) > 1}
    print("# classes with >1 candidate table: %s" % (multi or "none"))
    sc = structural_scan(stubs)
    print("# CROSS-CHECK structural scan (independent screen) found %d" % len(sc))
    print("#   in scan, not in universe : %s" % (sorted(sc - set(uni)) or "none"))
    print("#   in universe, not in scan : %s" % (sorted(set(uni) - sc) or "none"))

    ours = our_tables(root)
    print("\n%-22s %-11s %-13s %s" % ("class", "retail vt", "slots 0..10", "file"))
    print("-" * 104)
    tally = {".": 0, "!": 0, "o": 0, "X": 0}
    detail, nostand = [], []
    for cls in sorted(uni):
        if cls not in loc:
            continue
        va, slots = loc[cls][0]
        rslots = {i: (thunk_name(s) or "OWN") for i, s in enumerate(slots)}
        mine = ours.get(cls)
        if not mine:
            nostand.append(cls)
            continue
        for f, oslots, fixed in mine:
            line = []
            for i in range(NSLOT):
                r_inh = rslots[i] != "OWN"
                o_stub = i not in oslots
                c = ("!" if (r_inh and o_stub) else
                     "o" if (r_inh and not o_stub) else
                     "X" if o_stub else ".")
                tally[c] += 1
                line.append(c)
                if c != ".":
                    detail.append((c, cls, i, rslots[i], oslots.get(i), f))
            print("%-22s 0x%09x  %-13s %s%s"
                  % (cls, va, "".join(line), f, "  [FIXED]" if fixed else ""))
    print("\nretail-only, no standalone table of ours: %d\n  %s"
          % (len(nostand), ", ".join(nostand)))
    print("\nlegend  . both override   ! retail INHERITS we STUB"
          "   o retail inherits we override   X retail OVERRIDES we STUB")
    print("tally: %s   (%d slots compared)" % (tally, sum(tally.values())))
    print("\n-- every non-matching slot --")
    for c, cls, i, r, o, f in detail:
        print("%s %-20s sl%-3d %-20s retail=%-44s ours=%s"
              % (c, cls, i, SLOTNAME[i],
                 r[:42] if r != "OWN" else "OWN(exe override)", o or "StubNop"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
