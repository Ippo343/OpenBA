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
their own column.

THAT PARAGRAPH USED TO END "An unattributed site is NOT OURS -- you cannot own a
function you cannot name."  **THAT WAS WRONG, and it was wrong in the DEFLATING
direction**, which makes work look impossible rather than cheap and is therefore
the polarity least likely to be challenged.  Corrected by WAVE 26 lane PROMOIO:

  * MSVC emits no `.pdata` for a FRAMELESS LEAF function -- which is the shape of
    every three-instruction companion ctor/dtor in this DLL.  So `.pdata` absence
    correlates with the exact function shape this tool is most often asked about.
  * MEASURED on the full 63-row set: **45 of 47** unattributed stamp sites sit at
    an address that IS an entry in the shipped export directory, and **24 of 47**
    are registered in our tree by NAME AND ADDRESS -- i.e. OURS, while this tool
    printed NOT OURS for every one.  The twelve `??_7*Data@Video@@6B@` rows read
    `ours-capable 0`; the true figure is 2 of 5 per row.

WAVE 29 LANE INSTR FOLDED THEM IN, AND THE COUNTS MOVED.
PROMOIO's paragraph ended "folding these into `ours-capable` would move a number
other lanes quote ... a deliberate decision for a lane that owns the board".
Wave 29 lane INSTR is that lane and took it.  A stamp site whose address IS an
export's ENTRY POINT is that export -- `site` is the LEA's own VA, so this is an
identity, not a nearest-neighbour guess -- and owner() now returns `EXPORT-LEAF`
for it and SCORES it exactly like any other export.

  MEASURED on wave29/base, all 64 rows, before and after:
      before   47 unattributed group(s), 45 of them resolving exactly, 0 scored
      after    45 EXPORT-LEAF group(s) scored, 24 of them ours-capable,
               2 group(s) still UNSCORED
  DIRECTION: the correction is INFLATING relative to the old output, because the
  old output was DEFLATING -- it printed NOT OURS for 24 functions we own.
  `ours-capable` is STILL an upper bound for the reason two paragraphs up.

A site with neither a `.pdata` range nor an exact export match still prints
UNSCORED with a NEAREST-EXPORT HINT, never RETAIL.  You cannot score a function
you cannot name; that is a reason to say UNSCORED, not to say NOT OURS.
Evidence: docs/runtime_port/WAVE26_LANE_PROMOIO.md S2, WAVE29_LANE_INSTR.md S3.

POSITIVE CONTROLS, both of which MUST read non-zero / must fire
--------------------------------------------------------------
  1. DECODER: mint_extent_audit's own three CONTROLS (known real stamped tables)
     must be found by the writer channel.  Inherited, re-evaluated here.
  2. OWNERSHIP SCREEN: `--control <name>` requires a named symbol to come back
     REGISTERED-BY-NAME.  A run where it reads NOT-REGISTERED means the scan is
     dead and every `retail` verdict below would be an artefact; exit 2.
  3. OWNERSHIP NEGATIVE: a synthetic symbol that cannot exist must read
     NOT-REGISTERED.  A scan that says yes to everything is equally dead.
  4. `--selftest` (Wave 29 lane INSTR): the EXPORT-LEAF branch is handed a
     KNOWN POSITIVE and a KNOWN NEGATIVE.  Its normal answer in a healthy run
     is a COUNT, but the branch is absence-shaped from the other side -- a dead
     exact-match lookup reports zero leaves and all 45 of those sites silently
     revert to UNSCORED, which is exactly the state this fold-in ended.

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
    ap.add_argument("--selftest", action="store_true",
                    help="Wave 29 lane INSTR: control the EXPORT-LEAF branch "
                         "with a known positive and a known negative, and exit.")
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
        """-> (kind, name, func_start_va or None, offset).

        WAVE 29 LANE INSTR -- THE `EXPORT-LEAF` BRANCH, AND WHY IT IS SAFE.

        Wave 26 lane PROMOIO measured that this function's `.pdata` lookup
        fails for 47 of the stamp sites on the full row set, 45 of which sit
        at an address that IS an entry in the shipped export directory. MSVC
        emits no `.pdata` for a FRAMELESS LEAF, which is the shape of every
        three-instruction companion ctor/dtor in this DLL -- so the failure
        correlates with exactly the function shape this tool is most often
        asked about. PROMOIO declined to fold them in because doing so moves a
        number other lanes quote; Wave 29 lane INSTR owns the board and does.

        THIS IS NOT A HINT AND NOT A HEURISTIC. `site` is the address of the
        LEA itself (tools/mint_extent_audit.disasm_channel records the LEA's
        VA, not the store's). If that address is an export's ENTRY POINT, the
        stamp is that function's FIRST instruction: the site does not merely
        lie near the export, it IS the export. The nearest-export search below
        -- which is a hint, and stays UNSCORED -- is only reached when that
        exact-match test fails.

        DIRECTION OF THE CORRECTION: the old behaviour printed NOT OURS for a
        function we own, i.e. it was DEFLATING, which makes work look
        impossible rather than cheap and is the polarity least likely to be
        challenged. Re-derived on wave29/base before the change: 47
        unattributed groups, 45 resolving exactly.
        """
        rva = va - ib
        j = X.bisect.bisect_right(pd_begin, rva) - 1
        if j >= 0 and pdata[j][0] <= rva < pdata[j][1]:
            b = pdata[j][0]
            nm = exp_at.get(b)
            if nm:
                return ("EXPORT", nm, ib + b, rva - b)
            return ("PRIVATE", "FUN_%08x" % (ib + b), ib + b, rva - b)
        exact = exp_at.get(rva)
        if exact:
            return ("EXPORT-LEAF", exact, va, 0)
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

    resolved_unattr = 0
    leaf_groups = leaf_owned = 0     # the Wave 29 fold-in, counted separately
    for nm, va, slots in sel:
        sites = dis.get(va, [])
        byfn = {}
        for site, insn in sites:
            k = owner(site)
            byfn.setdefault((k[0], k[1], k[2]), []).append((site, insn, k[3]))
        own = tot = unattr = 0
        # resolved_unattr accumulates ACROSS rows (initialised before the loop):
        # it is the subject count of the CAVEAT printed after the last row.
        print("\n%s   %#x   %d slots   %d stamp site(s) in %d function(s)"
              % (nm, va, slots, len(sites), len(byfn)))
        for (kind, fname, fva), ss in sorted(byfn.items(), key=lambda x: x[1][0][0]):
            tot += 1
            if kind == "UNATTRIBUTED":
                unattr += 1
                # WAVE 26 lane PROMOIO. This branch used to print "NOT OURS",
                # which is a VERDICT this evidence cannot support, and it is
                # wrong in 45 of 47 cases on the full row set. MSVC emits no
                # .pdata for a FRAMELESS LEAF -- the shape of every
                # three-instruction companion ctor/dtor -- so the hint is not a
                # guess: when the stamp address IS the export's entry point, the
                # site is that export. Tested here, with data already in scope.
                # WAVE 29 lane INSTR: the exact-match case now leaves owner()
                # as EXPORT-LEAF and is SCORED, so anything still arriving here
                # is genuinely unnameable. `resolved_unattr` counts what is
                # left UNSCORED, which is what the caveat below is about.
                resolved_unattr += 1
                verdict = ("UNSCORED -- no .pdata AND the stamp VA is not an "
                           "export entry; the name above is a NEAREST-EXPORT "
                           "HINT, not an identification")
            else:
                by = []
                if kind in ("EXPORT", "EXPORT-LEAF") and fname in names:
                    by.append("NAME")
                if fva in addrs:
                    by.append("ADDR")
                if kind == "EXPORT-LEAF":
                    leaf_groups += 1
                if by:
                    own += 1
                    if kind == "EXPORT-LEAF":
                        leaf_owned += 1
                    verdict = "OURS-CAPABLE via %s" % "+".join(by)
                    if kind == "EXPORT-LEAF":
                        verdict += "   (frameless leaf: no .pdata; the stamp IS "
                        verdict += "this export's entry point)"
                else:
                    verdict = "RETAIL (no registration by name or address)"
            head = fname if kind != "UNATTRIBUTED" else ("~" + fname)
            print("   %-11s %-72s %s" % (kind, head[:72], verdict))
            for site, insn, off in ss[:4]:
                print("        %#x +%#-6x %s" % (site, off, insn))
            if len(ss) > 4:
                print("        ... %d more site(s) in this function" % (len(ss) - 4))
        print("   STAMPERS %d ; ours-capable %d ; retail %d ; unattributed %d%s"
              % (tot, own, tot - own - unattr, unattr,
                 "  <- UNSCORED, NOT retail; see CAVEAT" if unattr else ""))

    # ------------------------------------------------------------------
    # THE CAVEAT, PRINTED RATHER THAN DOCUMENTED.  WAVE 26 lane PROMOIO.
    #
    # A comment cannot reach the person who is about to misread the run,
    # because that person is precisely the one who did not open the file.
    # The `unattributed` column is the one an ownership reading gets wrong,
    # and it gets it wrong in the DEFLATING direction -- it makes work look
    # IMPOSSIBLE rather than cheap, which is the polarity a lane is least
    # likely to challenge.
    # ------------------------------------------------------------------
    # ------------------------------------------------------------------
    # THE FOLD-IN, AND WHAT IS STILL UNSCORED.  WAVE 29 lane INSTR.
    #
    # Printed unconditionally, INCLUDING the zero case: every list here is
    # absence-shaped, and "no frameless leaves found" is indistinguishable
    # from a dead exact-match lookup. `--self-test` hands it a positive.
    # ------------------------------------------------------------------
    if a.selftest:
        # ---- THE EXPORT-LEAF CONTROL  (Wave 29 lane INSTR) ---------------
        # Each check prints its own verdict AND what it saw: a control scored
        # on the union of several screens reads green and hides a blind one,
        # and a green line that does not name its subject is not evidence.
        checks = []
        checks.append(("POSITIVE: the live run found EXPORT-LEAF group(s)",
                       leaf_groups > 0, "%d group(s)" % leaf_groups))
        checks.append(("POSITIVE: some are ours-capable, so the ownership join "
                       "actually reaches the new branch",
                       leaf_owned > 0, "%d of %d" % (leaf_owned, leaf_groups)))
        # HANDED ONE, explicitly rather than relying on the sweep above: a real
        # export that carries no .pdata, asked about by its entry point.
        pd_set = set(pd_begin)
        leaf_rva = next((r for r, _n in exps
                         if r not in pd_set and exp_at.get(r)), None)
        if leaf_rva is not None:
            k = owner(ib + leaf_rva)
            checks.append(("HANDED ONE: %s (no .pdata) is NAMED, not UNATTRIBUTED"
                           % exp_at[leaf_rva][:44],
                           k[0] == "EXPORT-LEAF" and k[1] == exp_at[leaf_rva],
                           "owner() said %s %s" % (k[0], k[1][:36])))
        else:
            checks.append(("HANDED ONE: an export with no .pdata exists to hand",
                           False, "none found -- the control could not run"))
        # NEGATIVE: neither .pdata nor an export entry must stay UNATTRIBUTED.
        # A branch that names everything is as dead as one that names nothing.
        bad = ib + max(exp_rvas) + 0x5
        kn = owner(bad)
        checks.append(("NEGATIVE: a non-export, non-.pdata address stays "
                       "UNATTRIBUTED", kn[0] == "UNATTRIBUTED",
                       "owner(%#x) said %s" % (bad, kn[0])))
        # NO REGRESSION: a .pdata-covered export must still take the old branch,
        # or the fold-in replaced attribution instead of extending it.
        pd_rva = next((r for r, _n in exps if r in pd_set), None)
        if pd_rva is not None:
            k2 = owner(ib + pd_rva)
            checks.append(("NO REGRESSION: a .pdata-covered export is still "
                           "plain EXPORT", k2[0] == "EXPORT",
                           "owner() said %s" % k2[0]))
        bad_n = 0
        print("\nSELFTEST  EXPORT-LEAF branch (Wave 29 lane INSTR)")
        print("  subject: %d export(s), %d .pdata range(s), %d selected row(s)"
              % (len(exps), len(pdata), len(sel)))
        for label, ok, saw in checks:
            bad_n += 0 if ok else 1
            print("  %-5s %s  [%s]" % ("ok" if ok else "*RED*", label, saw))
        print("  RESULT: %d of %d checks correct" % (len(checks) - bad_n, len(checks)))
        return 2 if bad_n else 0

    print("\nFRAMELESS-LEAF FOLD-IN  (Wave 29 lane INSTR; this MOVED a number)")
    print("  %d stamp-site group(s) carried NO .pdata range and sat EXACTLY at an"
          % leaf_groups)
    print("  export's entry point, so they are that export and are now SCORED.")
    print("  %d of them are ours-capable; they used to print NOT OURS."
          % leaf_owned)
    print("  The old text read `An unattributed site is NOT OURS`. That was wrong")
    print("  in the DEFLATING direction -- work looked impossible rather than")
    print("  cheap -- and Wave 26 lane PROMOIO measured it at 45 of 47 sites")
    print("  resolving exactly and 24 of 47 ours by name AND address.")
    print("  WHAT THIS DOES NOT CLAIM: `ours-capable` is still an UPPER BOUND.")
    print("  A registration is a DOOR; an in-DLL `call rel32` to that function")
    print("  still runs retail's body. The fold-in fixed WHO the site belongs to,")
    print("  not whether every caller comes through us.")
    if leaf_groups == 0:
        print("  *** ZERO leaf groups. On wave29/base there were 45 of them, so an")
        print("  *** empty result here means the exact-match lookup is DEAD, not")
        print("  *** that the shape is gone. Run --self-test.")
    if resolved_unattr:
        print("\nCAVEAT -- %d SITE(S) REMAIN UNSCORED" % resolved_unattr)
        print("  No .pdata AND no exact export match: this tool cannot NAME the")
        print("  owning function, so it says UNSCORED. You cannot score a function")
        print("  you cannot name; that is a reason to say UNSCORED, never RETAIL.")
        print("  Full evidence for the fold-in: docs/runtime_port/WAVE26_LANE_PROMOIO.md S2")
        print("  and docs/runtime_port/WAVE29_LANE_INSTR.md S3.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
