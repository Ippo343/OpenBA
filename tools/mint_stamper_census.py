#!/usr/bin/env python3
r"""WHO STAMPS EACH MINTED VTABLE, AND DO WE OWN THAT FUNCTION? (Wave 20 lane PROMOTE)

SUBJECT, stated before any finding
----------------------------------
For each `??_7` row of `standalone/rt_data_mint_tables.inc` this tool answers
ONE question, from the shipped Runtime-x64.dll and from this tree's sources:

    WHICH FUNCTIONS INSTALL THIS TABLE INTO AN OBJECT, and for each of them,
    does a body of OURS run at that site, or does retail's?

This is WAVE19_CLOSED.md 5.1's gate: *a row is promotable insofar as WE OWN THE
FUNCTION THAT STAMPS IT.*  A stamp is an in-DLL RIP-relative `LEA` followed by a
store -- NOT a door -- so a green readiness row (TRAP=0) says nothing at all
about who installs the table.  Lane CSTREAM's three rows read identically at
9/9/TRAP=0 and had stampers-owned 0 / 2 / 3.

CHANNEL, and it is BORROWED not rebuilt
---------------------------------------
The writer channel is `tools/mint_extent_audit.py`'s (lane EXTENT, Wave 20 phase
0d), imported as a module: objdump over the whole image, instruction-boundary
correct, `lea reg,[rip+d32]` / `movabs reg,imm64` followed within three
instructions by a store of that register.  Attribution is `.pdata`-bounded.
This tool adds NO new decoding; it adds GROUPING BY OWNING FUNCTION and the
ownership join against our sources.

WHAT `OURS` MEANS HERE, AND WHAT IT DOES NOT
--------------------------------------------
`ours` is decided by a LITERAL registration scan over the COMPILED source dirs
(runtime/ engine/ code/ standalone/ dx11/), with C and C++ COMMENTS STRIPPED
FIRST -- prose naming a symbol is not a registration, and the most likely prose
to name one is a comment saying the code was deleted.  Two tiers, reported
separately and never merged:

    NAME  RegisterRuntimeExport("<the function's exported name>", ...)
    ADDR  RegisterRuntimeAddr(0x<the function's start VA>ull, ...)

A function is OURS-CAPABLE if either tier fires.  That is a NECESSARY condition
for our body to run at that stamp site and it is NOT SUFFICIENT: it says the
door exists, not that every caller of that function goes through it.  An in-DLL
`call rel32` to that function still runs retail's body today.  This tool
therefore prints `ours-capable`, never `ours`, and the count is an UPPER BOUND
on ownership.  The error direction is OPTIMISTIC, which is the unsafe direction,
so it is said here and repeated in the output.

UNATTRIBUTED SITES.  A stamp site outside every `.pdata` range cannot be
attributed to a function at all (~6045 of ~15290 functions carry `.pdata`).
Those print as `UNATTRIBUTED` with a nearest-export HINT, and are counted in
their own column.  An unattributed site is NOT OURS -- you cannot own a
function you cannot name.

POSITIVE CONTROLS, both of which MUST read non-zero / must fire
--------------------------------------------------------------
  1. DECODER: mint_extent_audit's own three CONTROLS (known real stamped tables)
     must be found by the writer channel.  Inherited, re-evaluated here.
  2. OWNERSHIP SCREEN: `--control <name>` requires a named symbol to come back
     REGISTERED-BY-NAME.  A run where it reads NOT-REGISTERED means the scan is
     dead and every `retail` verdict below would be an artefact; exit 2.
  3. OWNERSHIP NEGATIVE: a synthetic symbol that cannot exist must read
     NOT-REGISTERED.  A scan that says yes to everything is equally dead.

Exit 0 report produced, 2 a control is red.

USAGE
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_stamper_census.py
    ... --rows ??_7Stream@IO@@6B@,??_7FileStream@IO@@6B@
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mint_extent_audit as X          # noqa: E402  the borrowed channel
import pefile                          # noqa: E402

REPO = X.REPO
SRC_DIRS = ["runtime", "engine", "code", "standalone", "dx11"]
SRC_EXT = (".cpp", ".c", ".h", ".hpp", ".inc")

POS_CONTROL_DEFAULT = "??0BinaryFileStream@IO@@QEAA@XZ"
NEG_CONTROL = "?ThisSymbolCannotExist@Wave20@PROMOTE@@QEAAXXZ"

BLOCK = re.compile(r"/\*.*?\*/", re.S)
LINE = re.compile(r"//[^\n]*")


def strip_comments(text):
    """Remove /*...*/ and //... .  Crude on string literals containing '//',
    which in this tree are paths in log strings, never registrations."""
    return LINE.sub("", BLOCK.sub("", text))


def load_sources():
    names, addrs, nfiles, nbytes = set(), set(), 0, 0
    exp = re.compile(r'RegisterRuntime(?:Export|Addr)\(\s*(?:"([^"]+)"|(0x[0-9a-fA-F]+))')
    for d in SRC_DIRS:
        root = os.path.join(REPO, d)
        for dp, _, fs in os.walk(root):
            for f in fs:
                if not f.endswith(SRC_EXT):
                    continue
                p = os.path.join(dp, f)
                try:
                    t = open(p, encoding="utf-8", errors="replace").read()
                except OSError:
                    continue
                nfiles += 1
                nbytes += len(t)
                for m in exp.finditer(strip_comments(t)):
                    if m.group(1):
                        names.add(m.group(1))
                    else:
                        addrs.add(int(m.group(2), 16))
    return names, addrs, nfiles, nbytes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll", default=X.DEFAULT_DLL)
    ap.add_argument("--inc", default=X.DEFAULT_INC)
    ap.add_argument("--objdump", default=X.shutil.which("objdump") or "objdump")
    ap.add_argument("--rows", default="",
                    help="comma-separated substrings; empty = every row")
    ap.add_argument("--control", default=POS_CONTROL_DEFAULT)
    a = ap.parse_args()

    print("SUBJECT  who STAMPS each minted ??_7 row, and is that function OURS")
    print("  dll  : %s" % a.dll)
    print("  inc  : %s" % a.inc)
    print("  tree : %s" % REPO)
    print("PARAMETERS  --rows=%s  --control=%s  --objdump=%s"
          % (a.rows or "(all rows)", a.control, a.objdump))

    pe = pefile.PE(a.dll)
    ib = pe.OPTIONAL_HEADER.ImageBase
    pe.parse_data_directories()
    exps = sorted((s.address, (s.name or b"").decode(errors="replace"))
                  for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.address)
    exp_rvas = [e[0] for e in exps]
    exp_at = dict(exps)
    # ---- A .pdata RANGE IS A FRAGMENT, NOT A FUNCTION (Wave 21) ----------
    # MSVC chains function fragments through UNW_FLAG_CHAININFO, so a .pdata
    # entry bounds ONE FRAGMENT.  Measured in this DLL by Wave 21 lane CLASS:
    # 932 of 6045 entries (15.4%) carry that flag, covering 116,964 bytes.
    #
    # TWO CONSEQUENCES, and the second is why this note is here:
    #   * END - BEGIN IS A LOWER BOUND ON A FUNCTION'S SIZE, not its size.  Lane
    #     GFXBODY measured AnimationManager::Update at 50 bytes from one record;
    #     it is 430, and 3 of its 13 rows were understated, the worst by 8.6x.
    #   * ATTRIBUTION by raw range names the FRAGMENT's start, which for a
    #     chained fragment is NOT the owning function's entry point.
    #
    # A SIZE-UNDERSTATING blind spot is the opposite polarity from an inflating
    # one: it does not manufacture work, it makes work look CHEAP -- and cheap is
    # what gets picked up without scrutiny.  This tool does NOT follow the chain.
    # It is DECLARED here rather than fixed: correcting attribution would move
    # numbers other lanes are quoting, and lane GFXBODY's corrected method has
    # only 4 of its 13 extents Ghidra-confirmed.  Wave 22 input.
    pdata = sorted((f.struct.BeginAddress, f.struct.EndAddress)
                   for f in getattr(pe, "DIRECTORY_ENTRY_EXCEPTION", []))
    pd_begin = [p[0] for p in pdata]

    def owner(va):
        """-> (kind, name, func_start_va or None, offset)."""
        rva = va - ib
        j = X.bisect.bisect_right(pd_begin, rva) - 1
        if j >= 0 and pdata[j][0] <= rva < pdata[j][1]:
            b = pdata[j][0]
            nm = exp_at.get(b)
            if nm:
                return ("EXPORT", nm, ib + b, rva - b)
            return ("PRIVATE", "FUN_%08x" % (ib + b), ib + b, rva - b)
        k = X.bisect.bisect_right(exp_rvas, rva) - 1
        return ("UNATTRIBUTED", exps[k][1] if k >= 0 else "?", None, 0)

    dis, n_lea, n_mov, n_insn = X.disasm_channel(a.dll, a.objdump)
    print("\nWRITER CHANNEL (borrowed from tools/mint_extent_audit.py)")
    print("  instructions parsed %d ; lea rip %d ; movabs %d ; addresses named %d"
          % (n_insn, n_lea, n_mov, len(dis)))
    red = 0
    for va, why in X.CONTROLS.items():
        ok = va in dis
        print("  DECODER CONTROL %#x %-9s %s" % (va, "FOUND" if ok else "*MISSING*", why))
        red += 0 if ok else 1
    print("  DECODER CONTROL: %d of %d found" % (len(X.CONTROLS) - red, len(X.CONTROLS)))
    if red:
        print("REFUSING TO REPORT: the writer channel is not seeing known stamps.")
        return 2

    names, addrs, nfiles, nbytes = load_sources()
    print("\nOWNERSHIP SCREEN (literal registration scan, COMMENTS STRIPPED)")
    print("  dirs %s ; %d files ; %d bytes scanned"
          % ("/".join(SRC_DIRS), nfiles, nbytes))
    print("  distinct RegisterRuntimeExport names %d ; RegisterRuntimeAddr addrs %d"
          % (len(names), len(addrs)))
    pos = a.control in names
    neg = NEG_CONTROL in names
    print("  POSITIVE CONTROL %-58s %s"
          % (a.control, "REGISTERED" if pos else "*NOT REGISTERED*"))
    print("  NEGATIVE CONTROL %-58s %s"
          % ("(synthetic symbol)", "not-registered OK" if not neg else "*FIRED*"))
    if not pos or neg:
        print("REFUSING TO REPORT: the ownership screen is not known to be alive.")
        return 2
    print("  LIMIT: a registration is a DOOR, not a guarantee every caller uses it.")
    print("         `ours-capable` is an UPPER BOUND on ownership (optimistic).")

    src = open(a.inc, encoding="utf-8").read()
    rows = [(m.group(1) or "(by address)", int(m.group(2), 16), int(m.group(3)))
            for m in X.ROW.finditer(src)]
    want = [w for w in a.rows.split(",") if w]
    sel = [r for r in rows if (not want or any(w in r[0] for w in want))]
    print("\nROW SET: %d of %d rows selected" % (len(sel), len(rows)))
    if not sel:
        print("SELECTED NOTHING -- I MEASURED NOTHING about any row.")
        return 2

    for nm, va, slots in sel:
        sites = dis.get(va, [])
        byfn = {}
        for site, insn in sites:
            k = owner(site)
            byfn.setdefault((k[0], k[1], k[2]), []).append((site, insn, k[3]))
        own = tot = unattr = 0
        print("\n%s   %#x   %d slots   %d stamp site(s) in %d function(s)"
              % (nm, va, slots, len(sites), len(byfn)))
        for (kind, fname, fva), ss in sorted(byfn.items(), key=lambda x: x[1][0][0]):
            tot += 1
            if kind == "UNATTRIBUTED":
                unattr += 1
                verdict = "UNATTRIBUTED (no .pdata) -- NOT OURS; nearest-export HINT only"
            else:
                by = []
                if kind == "EXPORT" and fname in names:
                    by.append("NAME")
                if fva in addrs:
                    by.append("ADDR")
                if by:
                    own += 1
                    verdict = "OURS-CAPABLE via %s" % "+".join(by)
                else:
                    verdict = "RETAIL (no registration by name or address)"
            head = fname if kind != "UNATTRIBUTED" else ("~" + fname)
            print("   %-11s %-72s %s" % (kind, head[:72], verdict))
            for site, insn, off in ss[:4]:
                print("        %#x +%#-6x %s" % (site, off, insn))
            if len(ss) > 4:
                print("        ... %d more site(s) in this function" % (len(ss) - 4))
        print("   STAMPERS %d ; ours-capable %d ; retail %d ; unattributed %d"
              % (tot, own, tot - own - unattr, unattr))
    return 0


if __name__ == "__main__":
    sys.exit(main())
