#!/usr/bin/env python3
r"""Enumerate every in-DLL reference to each `?_singleton@` slot (Wave 15 lane INSTR).

WHAT QUESTION THIS ANSWERS

A minted `_singleton` row is a LIVE MUTABLE POINTER, so promoting it moves OUR
readers to OUR storage while every in-DLL RIP-relative reference stays on
retail's slot -- we intercept only at DOORS and a RIP-relative MOV is not one.
The triage question for a row is therefore NOT how many references it has, it is

    of this row's in-DLL references, how many are READS in bodies we have NOT
    ported?

because a ported body reaches the slot through RT_DATA_SLOT / ResolveExport and
moves with the promotion, while an unported body's load cannot move and gets
stranded on a slot nothing writes. That is the entire difference between the two
rows that already have verdicts:

    Application  26 refs, 24 READS in bodies nobody had ported  -> HARMFUL
    SaveLoad      5 refs, 4 in ported bodies + 1 in a DEAD one  -> SAFE

This tool produces the per-reference table those verdicts are made from, for all
20 rows, so phase 2 classifies instead of re-deriving. It renders NO verdicts.

WHY NOT THE EXISTING SCAN

WAVE15_KICKOFF_SINGLETONS.md's per-row column is a calibrated UPPER-BOUND scan
(every disp32 at every byte offset x 5 assumed instruction lengths), overcounting
4-5x, and it says twice that its numbers must never be quoted as measurements.
`tools/rt_mint_writer_audit.py` measures exactly, but only STORES, from a
hand-written list of ten forms -- a form it misses is a silently dropped
reference, which is the FALSE-SAFE direction. This tool needs reads, writes,
read-modify-writes and pointer-valued references, so it does not extend that
form list; it uses a real decoder and keeps the form-free scan only as the
CANDIDATE GENERATOR, where completeness is what matters.

HOW IT WORKS -- two decode routes plus one external route

  route SCAN (candidates, decoder-independent and COMPLETE BY CONSTRUCTION)
      Every RIP-relative reference on x86-64 encodes its target as a 4-byte
      disp32 field followed by 0, 1, 2 or 4 immediate bytes and then the end of
      the instruction. So for every byte offset p in .text and every trailing
      immediate size t in {0,1,2,4}, `base + p + 4 + t + disp32(p)` is a
      candidate target. No instruction form can escape that enumeration. It
      over-generates (a disp32 field can be interior bytes of something else),
      which is the safe direction: candidates are a strict SUPERSET.

  route BACKWARD (confirmation)
      For each candidate, disassemble one instruction at every start s in
      [p-15, p-1] and keep the decode whose own RIP target is the slot. This is
      what makes the imm32 quirk a non-issue: the decoder computes the end of
      the instruction rather than assuming it. Ambiguity is resolved against
      route SWEEP's instruction boundaries and counted.

  route SWEEP (independent corroboration + boundary map)
      A linear disassembly of .text from its first byte, restarting one byte
      later on an undecodable byte, collecting every RIP-relative reference to a
      slot. Independent of the candidate scan: it can find a reference the
      candidate scan's parametrisation missed, and vice versa. Set differences
      between BACKWARD and SWEEP are reported per row.

  route GHIDRA (external, and it CAN disagree -- which is the point)
      `get_xrefs_to` over the MCP bridge's HTTP endpoint, out of Ghidra's own
      xref database, which is built by a different disassembler on a different
      code model. The kickoff is explicit that a second route only helps if it
      can actually disagree; the hand scan it calibrates against disagreed
      4-vs-5. Every per-row set difference is printed with its cause. Ghidra
      also names the CONTAINING FUNCTION, which is an independent check on this
      tool's nearest-preceding-export attribution, and those disagreements are
      printed too.

A candidate that NO route can decode is emitted as kind=UNKNOWN-FORM rather than
dropped. A dropped reference is a false SAFE and that is the dangerous
direction. A candidate that route SWEEP places INSIDE a successfully decoded
instruction that is not a slot reference is a proven interior-bytes coincidence
and is rejected with that reason (counted, not silent).

THE owner_port_state COLUMN, and why it has two screens

This column is where the project has repeatedly gone wrong: a body PORTED but
never REGISTERED reads exactly like an ABSENT one, and registrations spelled
through a named constant or a macro are invisible to a literal grep. So it is
computed by TWO screens that are SCORED SEPARATELY (a control scored on the
union of two screens reads green while one screen is blind):

    R1  tools/runtime_census.py --csv, `ported` column. Handles the gated-off
        files/blocks and the non-literal spellings, but its population is only
        what OUR TREE CALLS -- an export we registered and never call is absent
        from it, so R1 alone reads such a row as UNPORTED.
    R2  this tool's own comment-stripped scan for RegisterRuntimeExport(<name>)
        (literal, adjacent-literal-concatenated, or named-constant resolved) and
        RegisterRuntimeAddr(<that export's VA>), plus RtMintPromotionStorage.

PORTED+REGISTERED is R1 or R2, and the CSV records WHICH screens fired.
`--no-strip-comments` is the stripper's control: it must not move the reference
counts at all (they come from the binary) and it must not move the port states
either; whatever it does move is printed.

USAGE
    python tools/rt_singleton_refs.py                      # needs capstone
    python tools/rt_singleton_refs.py --no-ghidra          # skip route GHIDRA
    python tools/rt_singleton_refs.py --no-strip-comments  # stripper control
    python tools/rt_singleton_refs.py --out <dir>

NOTE ON THE INTERPRETER: this needs `capstone` and `pefile`. The project venv
(tools/ghidra_mcp/.venv) has pefile but NOT capstone and has no pip, so this is
run with the system Python 3.12, which has capstone 5.0.7. It refuses to run
rather than falling back to a hand-written form list, because a form list is
exactly the blindness the tool exists to remove.

EXIT CODES
    0  every control green
    2  a control is RED
    3  environment problem (missing module, unparsable table)
    4  a control could not be RUN (e.g. Ghidra not reachable) -- not the same
       thing as green, and deliberately not exit 0
    5  every REFERENCE fact is green and a ground truth's CLASSIFICATION is not.
       Today this is the normal outcome: WAVE12_APPLICATION.md's "24 reads in
       unported bodies" does not reproduce, because 15 of those 24 read owners
       are ported AND registered in this tree -- six of them registered
       2026-09-08, the day BEFORE that wave closed. See the lane report. A
       broken instrument and a stale ground truth must not share an exit code.
"""
import argparse
import bisect
import collections
import csv
import importlib
import os
import re
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(REPO, "tools")
DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
TABLES = os.path.join(REPO, "standalone", "rt_data_mint_tables.inc")
CENSUS_TOOL = os.path.join(TOOLS, "runtime_census.py")
GHIDRA_URL = os.getenv("GHIDRA_SERVER_URL", "http://127.0.0.1:8089/")
GHIDRA_PROGRAM = "Runtime-x64.dll"

# Out of scope for the whole wave (WAVE15.md), and ~80x the next row, so its
# rows go to their own CSV and are excluded from every summary and control.
OUT_OF_SCOPE = "?_singleton@MemoryAllocator@System@@0PEAV12@EA"

# ---------------------------------------------------------------------------
# GROUND TRUTH -- the two rows that were enumerated by hand, by routes that
# could have disagreed. These are the only exact counts that existed before
# this tool, and they are the controls that decide whether it succeeded.
# ---------------------------------------------------------------------------

# standalone/reg_mint_promoted.cpp, the enumeration block above line 667.
# 0x1800c3ca6 is the one that matters: `MOV qword ptr [rip+d],0`, where RIP is
# measured PAST the imm32. The hand scan reported FOUR until that was fixed.
SAVELOAD_TRUTH = {
    0x1800c3d9b: ("WRITE", "??0SaveLoad@Save@@QEAA@XZ", "PORTED+REGISTERED"),
    0x1800c3ca6: ("WRITE", "??1SaveLoad@Save@@QEAA@XZ", "PORTED+REGISTERED"),
    0x1800dc3e9: ("READ", "?OnVideoRestart@MainLoop@Engine@@QEAAXXZ", "PORTED+REGISTERED"),
    0x1800dcc1d: ("READ", "?OnVideoError@MainLoop@Engine@@QEAAXXZ", "PORTED+REGISTERED"),
    0x180030d40: ("READ", "?Get@SaveLoad@Save@@SAPEAV12@XZ", "UNPORTED"),
}
SAVELOAD_SLOT = 0x1801fc6f0
SAVELOAD_IMM32_REF = 0x1800c3ca6

# WAVE12_APPLICATION.md, from Ghidra get_xrefs_to(0x1801fc678) run 2026-09-09.
APPLICATION_SLOT = 0x1801fc678
APPLICATION_TOTAL = 26
APPLICATION_WRITES = {0x18009f5b8, 0x1800dbbe2}
APPLICATION_READS_IN_UNPORTED = 24


def load_modules():
    try:
        import capstone  # noqa: F401
        import pefile    # noqa: F401
    except ImportError as exc:
        sys.stderr.write(
            "%s\nThis tool needs capstone AND pefile. The project venv has no "
            "capstone and no pip; run it with the system Python 3.12:\n"
            "    python tools/rt_singleton_refs.py\n"
            "It deliberately does NOT fall back to a hand-written instruction "
            "form list -- a missed form is a silently dropped reference.\n" % exc)
        raise SystemExit(3)
    return importlib.import_module("capstone"), importlib.import_module("pefile")


# ---------------------------------------------------------------------------
# the rows
# ---------------------------------------------------------------------------

ROW_RE = re.compile(
    r'\{ (nullptr|"[^"]*"), (0x[0-9a-f]+)ull, (\d+), &kStorageBytes\[\d+\], ')


def load_rows():
    text = open(TABLES, encoding="utf-8").read()
    rows = []
    for m in ROW_RE.finditer(text):
        if m.group(1) == "nullptr":
            continue
        sym = m.group(1).strip('"')
        if "?_singleton@" not in sym:
            continue
        rows.append({"sym": sym, "va": int(m.group(2), 16), "size": int(m.group(3))})
    return rows


# ---------------------------------------------------------------------------
# route SCAN -- candidate generation, complete by construction
# ---------------------------------------------------------------------------

TRAILING_IMM_SIZES = (0, 1, 2, 4)


def candidates(code, base, slots, imm_sizes=TRAILING_IMM_SIZES):
    """(disp_field_offset, trailing_imm_size, target) for every possible
    RIP-relative reference to a slot. Superset; no form can escape it."""
    out = []
    n = len(code)
    frombytes = int.from_bytes
    for p in range(0, n - 4):
        disp = frombytes(code[p:p + 4], "little", signed=True)
        for t in imm_sizes:
            tgt = base + p + 4 + t + disp
            if tgt in slots:
                out.append((p, t, tgt))
    return out


# ---------------------------------------------------------------------------
# routes SWEEP and BACKWARD -- decoding
# ---------------------------------------------------------------------------

def rip_target(cs, insn):
    for op in insn.operands:
        if op.type == cs.x86.X86_OP_MEM and op.mem.base == cs.x86.X86_REG_RIP:
            return insn.address + insn.size + op.mem.disp, op.access
    return None, None


def kind_of(cs, insn, access):
    if insn.mnemonic == "lea":
        return "LEA"
    read = bool(access & cs.CS_AC_READ)
    write = bool(access & cs.CS_AC_WRITE)
    if read and write:
        return "RMW"
    if write:
        return "WRITE"
    if read:
        return "READ"
    return "UNKNOWN-FORM"


def sweep(cs, code, base, slots):
    """Linear disassembly with one-byte restart. Returns (refs, starts, gaps)."""
    md = cs.Cs(cs.CS_ARCH_X86, cs.CS_MODE_64)
    md.detail = True
    refs, starts, gaps = {}, set(), 0
    off, n = 0, len(code)
    while off < n:
        got = False
        for insn in md.disasm(code[off:], base + off, count=4096):
            got = True
            starts.add(insn.address)
            for cur in range(insn.address, insn.address + insn.size):
                pass  # boundary map is derived from starts+size below
            tgt, access = rip_target(cs, insn)
            if tgt in slots:
                refs[insn.address] = (tgt, kind_of(cs, insn, access),
                                      "%s %s" % (insn.mnemonic, insn.op_str),
                                      insn.size)
            off = insn.address - base + insn.size
        if not got:
            gaps += 1
            off += 1
    return refs, starts, gaps


def decoded_span_map(cs, code, base):
    """addr -> owning instruction start, for every byte route SWEEP decoded.
    Used only to REJECT candidates proven to be interior bytes."""
    md = cs.Cs(cs.CS_ARCH_X86, cs.CS_MODE_64)
    md.detail = False
    owner = {}
    off, n = 0, len(code)
    while off < n:
        got = False
        for insn in md.disasm(code[off:], base + off, count=4096):
            got = True
            for a in range(insn.address, insn.address + insn.size):
                owner[a] = insn.address
            off = insn.address - base + insn.size
        if not got:
            off += 1
    return owner


def backward(cs, code, base, cands, slots, sweep_starts):
    """Confirm each candidate by decoding one instruction at every plausible
    start. Returns (refs, unconfirmed, ambiguous)."""
    md = cs.Cs(cs.CS_ARCH_X86, cs.CS_MODE_64)
    md.detail = True
    refs, unconfirmed, ambiguous = {}, [], 0
    for p, t, tgt in cands:
        hits = []
        for s in range(max(0, p - 15), p):
            for insn in md.disasm(code[s:s + 16], base + s, count=1):
                itgt, access = rip_target(cs, insn)
                if itgt == tgt and s + insn.size == p + 4 + t:
                    hits.append((insn.address, kind_of(cs, insn, access),
                                 "%s %s" % (insn.mnemonic, insn.op_str),
                                 insn.size, t))
                break
        if not hits:
            unconfirmed.append((p, t, tgt))
            continue
        # Almost every candidate decodes at MORE THAN ONE start, and the reason
        # is benign and universal: starting one byte later drops the REX prefix
        # and yields the 32-bit form of the same instruction, ending at the same
        # byte (`mov rcx,[rip+d]` at s vs `mov ecx,[rip+d]` at s+1). Route
        # SWEEP's instruction boundaries settle it. What is worth counting is
        # the residue: candidates NO sweep boundary agrees with, where this tool
        # falls back to the longest (most-prefixed) decode.
        preferred = [h for h in hits if h[0] in sweep_starts]
        if not preferred:
            ambiguous += 1
        chosen = (preferred or hits)[0]
        refs[chosen[0]] = (tgt,) + chosen[1:]
    return refs, unconfirmed, ambiguous


# ---------------------------------------------------------------------------
# route GHIDRA
# ---------------------------------------------------------------------------

GH_LINE = re.compile(r"^From ([0-9a-fA-F]+)(?: in (.+?))? \[([A-Z_]+)\]\s*$")


def ghidra_xrefs(slot_va):
    """(code_refs {va: (kind, function)}, other_lines [str]) or raises."""
    code_refs, other = {}, []
    off, limit = 0, 2000
    while True:
        url = ("%sget_xrefs_to?address=0x%x&offset=%d&limit=%d&program=%s"
               % (GHIDRA_URL, slot_va, off, limit, GHIDRA_PROGRAM))
        with urllib.request.urlopen(url, timeout=60) as fh:
            body = fh.read().decode("utf-8", "replace")
        lines = [l for l in body.splitlines() if l.strip()]
        for line in lines:
            m = GH_LINE.match(line.strip())
            if not m or m.group(2) is None:
                other.append(line.strip())
                continue
            code_refs[int(m.group(1), 16)] = (m.group(3), m.group(2))
        if len(lines) < limit:
            break
        off += limit
    return code_refs, other


# ---------------------------------------------------------------------------
# attribution and port state
# ---------------------------------------------------------------------------

def build_owner(pe, cs, code, base, census):
    """nearest-preceding-export attribution, with the CRT-initialiser carve-out
    that tools/rt_mint_writer_audit.py exists to document: the initialisers are
    UNEXPORTED and merely sit after an exported function in address order, so
    without the carve-out their stores are credited to a PORTED function, which
    moves rows out of exactly the bucket that matters."""
    ib = pe.OPTIONAL_HEADER.ImageBase
    by_rva = {e.address: e.name.decode()
              for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
    srt = sorted(by_rva)
    mwa = importlib.import_module("rt_mint_writer_audit")
    inits = mwa.find_crt_init_table(pe, ib)
    lo, hi = (inits[0], inits[-1] + 0x70) if inits else (0, 0)

    def owner(va):
        if lo <= va < hi:
            return "(CRT initialiser)"
        i = bisect.bisect_right(srt, va - ib) - 1
        return by_rva[srt[i]] if i >= 0 else "(before first export)"

    va_of = {n: ib + r for r, n in by_rva.items()}
    return owner, va_of


RE_NAME_CONST = re.compile(
    r'(?:const\s+char\s*\*\s*const|constexpr\s+char(?:\s+const)?)\s+'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[\s*\])?\s*=\s*(?=")')

# A NAME IN OUR SOURCES IS NOT EVIDENCE OF A PORT. Most mangled names in the
# tree are CALL-SIDE DOORS: our code asking the DLL to run RETAIL's body. Both
# ground truths caught this tool getting it wrong before the distinction was
# drawn -- `?Get@SaveLoad@Save@@SAPEAV12@XZ` (truth: UNPORTED, and dead) is
# named in standalone/rt_orphan_probe.cpp only as the constant the probe
# RESOLVES so it can call retail's Get, and `?PlatformShutdown@Device@Input@@`
# is named only inside `ResolveFn(...)` and `RT_EXPORT_ONCE(...)`. A plain
# "does the name appear anywhere" screen calls both of those PORTED, which is
# the exact false-safe this column must not produce.
DOOR_TOKENS = ("ResolveExportRaw", "ResolveExport", "ResolveOrdinal", "ResolveFn",
               "ResolveRuntimeRaw", "RT_EXPORT_ONCE", "RT_ORIGINAL_ONCE",
               "RT_ORIGINAL", "RT_EXPORT", "RT_DATA_SLOT", "RT_PREDICT",
               "Dx11ResolveRuntimeImport", "RtOriginal")
DOOR_WINDOW = 160


def _is_door_context(text, pos):
    """True if the literal at `pos` is the argument of a call-side door."""
    win = text[max(0, pos - DOOR_WINDOW):pos]
    return any(tok in win for tok in DOOR_TOKENS)


def scan_registrations(strip=True):
    """Screen R2: our own comment-stripped scan of the tree for registrations.

    Returns (registered_names, registered_addrs, mentioned_names, notes).
    """
    census = importlib.import_module("runtime_census")
    reg_names, reg_addrs, mentioned = set(), set(), set()
    notes = collections.Counter()
    gated_files = set(census.GATED_OFF_FILES)
    for path in census.iter_sources():
        rel = os.path.relpath(path, REPO).replace(os.sep, "/")
        raw = open(path, encoding="utf-8", errors="replace").read()
        text = census.strip_comments(raw) if strip else raw
        # named constants -> literal, so a registration spelled through an
        # identifier is not invisible (the census has had this bug in three
        # distinct forms).
        consts = {}
        for m in RE_NAME_CONST.finditer(text):
            consts[m.group(1)] = census.concat_literal(text, m.end())
        # port-evidence mentions: the name appears somewhere that is NOT a
        # call-side door and NOT a registration argument.
        door_only = {}
        for m in re.finditer(r'"(\?[A-Za-z0-9_?$@]*@@[A-Za-z0-9_?$@]*)"', text):
            name = m.group(1)
            if _is_door_context(text, m.start()):
                door_only.setdefault(name, True)
                continue
            # a named constant is judged by how the IDENTIFIER is used, not by
            # its definition site, which never has a door token before it.
            # NB: pick the constant definition that ends exactly AT this
            # literal. A plain .search() over the preceding window returns the
            # FIRST definition in it, which is a neighbouring constant when
            # several are declared together -- that near-miss is what made
            # `?Get@SaveLoad@` read as port evidence and control 1 go red.
            cm = None
            for c in RE_NAME_CONST.finditer(text, max(0, m.start() - 200), m.start() + 1):
                if c.end() == m.start():
                    cm = c
            if cm:
                ident = cm.group(1)
                uses = [u.start() for u in re.finditer(r'\b%s\b' % re.escape(ident), text)
                        if u.start() != cm.start(1)]
                if uses and all(_is_door_context(text, u) for u in uses):
                    door_only.setdefault(name, True)
                    continue
            if re.search(r'Register(RuntimeExport|RuntimeAddr|DispatchSlot)\s*\(\s*$',
                         text[max(0, m.start() - 60):m.start()]):
                continue
            door_only[name] = False
        for name, is_door in door_only.items():
            if not is_door:
                mentioned.add(name)
        if rel in gated_files:
            notes["skipped gated-off file: " + rel] += 1
            continue
        for m in re.finditer(r'RegisterRuntimeExport\s*\(\s*', text):
            pos = m.end()
            if pos < len(text) and text[pos] == '"':
                reg_names.add(census.concat_literal(text, pos))
                continue
            im = re.match(r'([A-Za-z_][A-Za-z0-9_:]*)', text[pos:])
            if im:
                ident = im.group(1).split("::")[-1]
                if ident in consts:
                    reg_names.add(consts[ident])
                    notes["named-constant registration resolved in " + rel] += 1
                else:
                    notes["NON-LITERAL registration NOT resolved in " + rel] += 1
        for m in re.finditer(r'RegisterRuntimeAddr\s*\(\s*(0x[0-9a-fA-F]+)', text):
            reg_addrs.add(int(m.group(1), 16))
        for m in re.finditer(r'RtMintPromotionStorage\s*\(\s*"([^"]+)"', text):
            reg_names.add(m.group(1))
    return reg_names, reg_addrs, mentioned, notes


def load_census():
    out = os.path.join(tempfile.gettempdir(), "rt_singleton_census.csv")
    r = subprocess.run([sys.executable, CENSUS_TOOL, "--csv", out],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode != 0:
        sys.stderr.write(r.stdout.decode("utf-8", "replace"))
        return None
    ported = {}
    with open(out, encoding="utf-8", newline="") as fh:
        for row in csv.DictReader(fh):
            ported[row["key"]] = row["ported"] == "1"
    return ported


def port_state(name, va_of, census_ported, reg_names, reg_addrs, mentioned):
    """(state, screens) -- screens says WHICH screen fired, so a control is
    never scored on the union of two screens."""
    if name.startswith("("):
        return "N/A-INTERNAL", ""
    r1 = bool(census_ported and census_ported.get(name))
    r2 = name in reg_names or va_of.get(name) in reg_addrs
    if r1 or r2:
        return "PORTED+REGISTERED", ("R1" if r1 else "") + ("R2" if r2 else "")
    if name in mentioned:
        return "PORTED-UNREGISTERED", "mention-only"
    return "UNPORTED", "none"


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(REPO, "docs", "runtime_port", "wave15"))
    ap.add_argument("--no-ghidra", action="store_true")
    ap.add_argument("--no-strip-comments", action="store_true",
                    help="stripper control: must move NO reference count and NO port state")
    args = ap.parse_args()
    sys.path.insert(0, TOOLS)
    cs, pefile = load_modules()

    rows = load_rows()
    print("rows parsed from %s: %d" % (os.path.relpath(TABLES, REPO), len(rows)))
    if len(rows) != 20:
        print("expected 20 ?_singleton@ rows, got %d -- table format changed?"
              % len(rows), file=sys.stderr)
        return 3
    slots = {r["va"]: r["sym"] for r in rows}

    pe = pefile.PE(DLL)
    ib = pe.OPTIONAL_HEADER.ImageBase
    text = next(s for s in pe.sections if s.Name.decode().rstrip("\0") == ".text")
    code = pe.get_data(text.VirtualAddress, text.SizeOfRawData)
    base = ib + text.VirtualAddress
    print("Runtime-x64.dll .text %#x + %d bytes" % (base, len(code)))

    cands = candidates(code, base, slots)
    sw_refs, sw_starts, sw_gaps = sweep(cs, code, base, slots)
    bw_refs, unconfirmed, ambiguous = backward(cs, code, base, cands, slots, sw_starts)
    span = decoded_span_map(cs, code, base)
    print("route SCAN     : %d candidates (superset, form-free)" % len(cands))
    print("route SWEEP    : %d refs, %d instruction starts, %d undecodable bytes"
          % (len(sw_refs), len(sw_starts), sw_gaps))
    print("route BACKWARD : %d refs confirmed, %d candidates unconfirmed, "
          "%d with no sweep-boundary agreement" % (len(bw_refs), len(unconfirmed), ambiguous))

    # unconfirmed candidates: interior bytes of a decoded instruction (proven
    # coincidence, reject with reason) vs bytes route SWEEP could not decode
    # (emit UNKNOWN-FORM -- a dropped reference is a false SAFE).
    rejected, unknown = 0, []
    for p, t, tgt in unconfirmed:
        if base + p in span:
            rejected += 1
        else:
            unknown.append((p, t, tgt))
    print("                 of those: %d rejected as interior bytes of a decoded "
          "instruction, %d emitted as UNKNOWN-FORM" % (rejected, len(unknown)))

    # route agreement, as a SET difference and not a count
    only_bw = set(bw_refs) - set(sw_refs)
    only_sw = set(sw_refs) - set(bw_refs)
    print("BACKWARD vs SWEEP: %d only-BACKWARD, %d only-SWEEP" % (len(only_bw), len(only_sw)))
    for a in sorted(only_bw):
        print("   only BACKWARD %#x -> %#x" % (a, bw_refs[a][0]))
    for a in sorted(only_sw):
        print("   only SWEEP    %#x -> %#x" % (a, sw_refs[a][0]))

    # POINTER-VALUED REFERENCES OUTSIDE .text. The kickoff's method item 5 asks
    # for pointer-valued references, and a RIP scan of .text cannot see one that
    # is stored as an ABSOLUTE 8-byte pointer in a data section (a dispatch
    # table, an initialised global). Cheap to close, so it is closed here rather
    # than left as "a route we did not enumerate".
    ptr_hits = []
    for sec in pe.sections:
        name = sec.Name.decode().rstrip("\0")
        data = pe.get_data(sec.VirtualAddress, sec.SizeOfRawData)
        sbase = ib + sec.VirtualAddress
        for off in range(0, len(data) - 8, 4):
            qw = int.from_bytes(data[off:off + 8], "little")
            if qw in slots:
                ptr_hits.append((name, sbase + off, qw))
    print("absolute 8-byte pointers to a slot, all sections: %d%s"
          % (len(ptr_hits), "" if not ptr_hits else " " + repr(
              [(n, hex(a), hex(t)) for n, a, t in ptr_hits[:10]])))

    census_ported = load_census()
    if census_ported is None:
        print("screen R1 (runtime_census.py) FAILED to run", file=sys.stderr)
        return 3
    reg_names, reg_addrs, mentioned, notes = scan_registrations(
        strip=not args.no_strip_comments)
    print("screen R1 (census)     : %d keys, %d registered"
          % (len(census_ported), sum(1 for v in census_ported.values() if v)))
    print("screen R2 (own scan)   : %d registered names, %d registered addrs, "
          "%d mangled names mentioned anywhere"
          % (len(reg_names), len(reg_addrs), len(mentioned)))
    for k, v in notes.most_common():
        if k.startswith("NON-LITERAL"):
            print("   R2 BLIND SPOT x%d: %s" % (v, k))

    owner, va_of = build_owner(pe, cs, code, base, census_ported)

    # ---- assemble ---------------------------------------------------------
    refs = {}   # ref_va -> dict
    for va, (tgt, kind, disasm, size, *rest) in bw_refs.items():
        refs[va] = {"slot": tgt, "kind": kind, "disasm": disasm,
                    "imm": rest[0] if rest else "", "route": "BACKWARD"}
    for va, (tgt, kind, disasm, size) in sw_refs.items():
        if va in refs:
            refs[va]["route"] = "BACKWARD+SWEEP"
        else:
            refs[va] = {"slot": tgt, "kind": kind, "disasm": disasm,
                        "imm": "", "route": "SWEEP"}
    for p, t, tgt in unknown:
        va = base + p
        refs[va] = {"slot": tgt, "kind": "UNKNOWN-FORM",
                    "disasm": "(disp32 field at %#x, trailing imm %d, in a "
                              "region no route decoded)" % (va, t),
                    "imm": t, "route": "SCAN-ONLY"}

    gh_ok, gh = True, {}
    gh_other = {}
    if not args.no_ghidra:
        for r in rows:
            if r["sym"] == OUT_OF_SCOPE:
                continue
            try:
                gh[r["va"]], gh_other[r["va"]] = ghidra_xrefs(r["va"])
            except (urllib.error.URLError, OSError) as exc:
                print("route GHIDRA UNAVAILABLE (%s) -- control 3 NOT RUN" % exc,
                      file=sys.stderr)
                gh_ok = False
                break
    else:
        gh_ok = False
        print("route GHIDRA skipped by --no-ghidra -- control 3 NOT RUN")

    out_rows = []
    for va in sorted(refs):
        d = refs[va]
        own = owner(va)
        state, screens = port_state(own, va_of, census_ported, reg_names,
                                    reg_addrs, mentioned)
        ghk, ghfn = gh.get(d["slot"], {}).get(va, ("", ""))
        # ATTRIBUTION, and why it gets its own column. This tool credits a
        # reference to the NEAREST PRECEDING EXPORT. That is wrong whenever the
        # reference is inside an UNEXPORTED function that merely sits after an
        # export in address order -- the exact error tools/rt_mint_writer_audit.py
        # carved the CRT initialisers out for. Ghidra knows the containing
        # FUNCTION independently, so it is used as the check, and where Ghidra
        # has no function at all the port state is withheld rather than
        # attributed to a neighbour that would carry a PORTED state it has not
        # earned.
        if not gh_ok or d["slot"] not in gh:
            attribution = "nearest-export (Ghidra route NOT RUN for this row)"
        elif ghfn:
            fun = re.fullmatch(r"FUN_([0-9a-fA-F]+)", ghfn)
            if fun and int(fun.group(1), 16) != va_of.get(own):
                # Ghidra has a function here and it is UNNAMED, i.e. it is not
                # in the export table: the reference sits in an unexported
                # internal that merely follows `own` in address order. The
                # nearest-preceding-export attribution is therefore WRONG for
                # this row, so the port state is withheld instead of being
                # borrowed from a neighbour.
                attribution = ("GHIDRA-UNEXPORTED-INTERNAL:%s (nearest preceding "
                               "export shown in owning_export)" % ghfn)
                state, screens = "UNKNOWN-OWNER", "attribution-disputed"
            else:
                toks = [t for t in re.split(r"[_~]+", ghfn) if t]
                hit = any(t in own for t in toks) or ghfn.strip("~") in own
                attribution = ("GHIDRA-CONFIRMED" if hit
                               else "GHIDRA-NAMES-OTHER:%s" % ghfn)
        else:
            attribution = "NO-GHIDRA-FUNCTION (unexported internal; owner shown " \
                          "is only the nearest preceding export)"
            state, screens = "UNKNOWN-OWNER", "attribution-disputed"
        out_rows.append({
            "symbol": slots[d["slot"]], "slot_va": "%#x" % d["slot"],
            "ref_va": "%#x" % va, "kind": d["kind"], "owning_export": own,
            "owner_port_state": state, "port_state_screens": screens,
            "route": d["route"], "trailing_imm": d["imm"],
            "disasm": d["disasm"], "ghidra_kind": ghk, "ghidra_function": ghfn,
            "attribution": attribution,
        })

    os.makedirs(args.out, exist_ok=True)
    cols = ["symbol", "slot_va", "ref_va", "kind", "owning_export",
            "owner_port_state", "port_state_screens", "route", "trailing_imm",
            "disasm", "ghidra_kind", "ghidra_function", "attribution"]
    main_csv = os.path.join(args.out, "rt_singleton_refs.csv")
    oos_csv = os.path.join(args.out, "rt_singleton_refs_memoryallocator.csv")
    for path, keep in ((main_csv, False), (oos_csv, True)):
        with open(path, "w", encoding="utf-8", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=cols)
            w.writeheader()
            for r in out_rows:
                if (r["symbol"] == OUT_OF_SCOPE) == keep:
                    w.writerow(r)
    print("\nwrote %s and %s" % (os.path.relpath(main_csv, REPO),
                                 os.path.relpath(oos_csv, REPO)))

    # ---- per-row table ----------------------------------------------------
    per = collections.defaultdict(lambda: collections.Counter())
    unported_reads = collections.Counter()
    unknown_owner = collections.Counter()
    for r in out_rows:
        per[r["symbol"]][r["kind"]] += 1
        per[r["symbol"]]["TOTAL"] += 1
        if r["kind"] in ("READ", "RMW") and r["owner_port_state"] == "UNPORTED":
            unported_reads[r["symbol"]] += 1
        if r["owner_port_state"] == "UNKNOWN-OWNER":
            unknown_owner[r["symbol"]] += 1
    print("\nFIRST MEASUREMENT of these rows. The kickoff's per-row column is a")
    print("calibrated upper-bound scan and is NOT a measurement; these are.\n")
    hdr = ("%-52s %5s %5s %5s %4s %4s %4s  %s"
           % ("symbol", "total", "READ", "WRITE", "RMW", "LEA", "UNK",
              "reads-in-UNPORTED / unknown-owner"))
    print(hdr)
    print("-" * len(hdr))
    for r in sorted(rows, key=lambda x: -per[x["sym"]]["TOTAL"]):
        s, c = r["sym"], per[r["sym"]]
        tag = "  [OUT OF SCOPE]" if s == OUT_OF_SCOPE else ""
        print("%-52s %5d %5d %5d %4d %4d %4d  %d / %d%s"
              % (s.replace("?_singleton@", "").replace("@@0PEAV12@EA", "")
                  .replace("@@1PEAV12@EA", "").replace("@@2PEAV12@EA", "")
                  .replace("@0PEAV11@EA", ""),
                 c["TOTAL"], c["READ"], c["WRITE"], c["RMW"], c["LEA"],
                 c["UNKNOWN-FORM"], unported_reads[s], unknown_owner[s], tag))

    # ---- CONTROL 1: SaveLoad ---------------------------------------------
    print("\n=== CONTROL 1 -- SaveLoad ground truth (5 refs at 5 known VAs) ===")
    got = {r["ref_va"]: r for r in out_rows if int(r["slot_va"], 16) == SAVELOAD_SLOT}
    c1_fail = []
    if len(got) != len(SAVELOAD_TRUTH):
        c1_fail.append("count %d != 5" % len(got))
    for va, (kind, export, state) in SAVELOAD_TRUTH.items():
        r = got.get("%#x" % va)
        if r is None:
            c1_fail.append("MISSING %#x (%s)" % (va, export))
            continue
        if r["kind"] != kind:
            c1_fail.append("%#x kind %s != %s" % (va, r["kind"], kind))
        if r["owning_export"] != export:
            c1_fail.append("%#x owner %s != %s" % (va, r["owning_export"], export))
        if r["owner_port_state"] != state:
            c1_fail.append("%#x port_state %s != %s"
                           % (va, r["owner_port_state"], state))
        print("   %#x %-5s %-45s %-19s screens=%s"
              % (va, r["kind"], r["owning_export"], r["owner_port_state"],
                 r["port_state_screens"] or "-"))
    for va in sorted(int(k, 16) for k in got):
        if va not in SAVELOAD_TRUTH:
            c1_fail.append("EXTRA %#x" % va)
    # the imm32 quirk, as a control that can go RED: re-run candidate
    # generation with NO trailing immediate and show it LOSES that reference.
    imm_row = got.get("%#x" % SAVELOAD_IMM32_REF)
    print("   imm32 quirk: %#x trailing immediate bytes = %s (%s)"
          % (SAVELOAD_IMM32_REF, imm_row["trailing_imm"] if imm_row else "MISSING",
             imm_row["disasm"] if imm_row else "-"))
    t0 = candidates(code, base, {SAVELOAD_SLOT: "x"}, imm_sizes=(0,))
    t0_refs, _, _ = backward(cs, code, base, t0, {SAVELOAD_SLOT: "x"}, sw_starts)
    print("   NEGATIVE CONTROL: candidate generation restricted to trailing "
          "imm=0 finds %d of the 5 (%s) -- this reproduces the historical "
          "4-vs-5 disagreement, so the quirk handling is load-bearing and not "
          "decoration" % (len(t0_refs),
                          "MISSES the imm32 ref" if SAVELOAD_IMM32_REF not in t0_refs
                          else "STILL finds it -- control did NOT go red, "
                               "investigate before believing control 1"))
    c1_red = bool(c1_fail) or SAVELOAD_IMM32_REF in t0_refs
    print("   CONTROL 1: %s" % ("GREEN" if not c1_red else "RED"))
    for f in c1_fail:
        print("      MISS: %s" % f)

    # ---- CONTROL 2: Application ------------------------------------------
    print("\n=== CONTROL 2 -- Application ground truth (26 refs, 24 reads in "
          "unported bodies) ===")
    app = [r for r in out_rows if int(r["slot_va"], 16) == APPLICATION_SLOT]
    writes = {int(r["ref_va"], 16) for r in app if r["kind"] == "WRITE"}
    reads = [r for r in app if r["kind"] == "READ"]
    reads_unported = [r for r in reads if r["owner_port_state"] == "UNPORTED"]
    print("   total %d (truth %d)" % (len(app), APPLICATION_TOTAL))
    print("   WRITE %d at %s (truth %s)"
          % (len(writes), sorted("%#x" % v for v in writes),
             sorted("%#x" % v for v in APPLICATION_WRITES)))
    print("   READ  %d, of which owner_port_state=UNPORTED: %d (truth %d)"
          % (len(reads), len(reads_unported), APPLICATION_READS_IN_UNPORTED))
    split = collections.Counter(r["owner_port_state"] for r in reads)
    for k, v in split.most_common():
        print("      reads by owner_port_state: %-20s %d" % (k, v))
    if len(reads_unported) != APPLICATION_READS_IN_UNPORTED:
        print("   read owners NOT classified UNPORTED, with their state:")
        for r in reads:
            if r["owner_port_state"] != "UNPORTED":
                print("      %s %-45s %s screens=%s"
                      % (r["ref_va"], r["owning_export"], r["owner_port_state"],
                         r["port_state_screens"]))
    c2_count_red = len(app) != APPLICATION_TOTAL or writes != APPLICATION_WRITES
    c2_split_red = len(reads_unported) != APPLICATION_READS_IN_UNPORTED
    print("   CONTROL 2 count/kind: %s" % ("GREEN" if not c2_count_red else "RED"))
    print("   CONTROL 2 classification split: %s"
          % ("GREEN" if not c2_split_red else "RED -- see mechanism in the report"))

    # ---- CONTROL 3: Ghidra ------------------------------------------------
    print("\n=== CONTROL 3 -- Ghidra get_xrefs_to, a route that CAN disagree ===")
    c3_red = False
    if not gh_ok:
        print("   NOT RUN")
    else:
        for r in sorted(rows, key=lambda x: x["sym"]):
            if r["sym"] == OUT_OF_SCOPE:
                continue
            mine = {int(x["ref_va"], 16) for x in out_rows
                    if int(x["slot_va"], 16) == r["va"]}
            theirs = set(gh[r["va"]])
            only_mine, only_gh = mine - theirs, theirs - mine
            kinddiff = []
            for va in sorted(mine & theirs):
                mk = next(x["kind"] for x in out_rows
                          if int(x["ref_va"], 16) == va and int(x["slot_va"], 16) == r["va"])
                gk = gh[r["va"]][va][0]
                norm = {"READ": "READ", "WRITE": "WRITE", "READ_WRITE": "RMW",
                        "DATA": "LEA"}.get(gk, gk)
                if mk != norm:
                    kinddiff.append((va, mk, gk))
            # Ghidra's containing-FUNCTION name is an independent check on the
            # nearest-preceding-export attribution, and this is where it pays:
            # a reference Ghidra places in an UNNAMED function is inside an
            # UNEXPORTED internal, so crediting it to the preceding export
            # would hand it that export's port state.
            internal = sum(1 for x in out_rows
                           if int(x["slot_va"], 16) == r["va"]
                           and x["attribution"].startswith("GHIDRA-UNEXPORTED"))
            renamed = sum(1 for x in out_rows
                          if int(x["slot_va"], 16) == r["va"]
                          and x["attribution"].startswith("GHIDRA-NAMES-OTHER"))
            flag = "" if not (only_mine or only_gh or kinddiff) else "  <-- DISAGREES"
            print("   %-50s mine %4d  ghidra %4d%s"
                  % (r["sym"].replace("?_singleton@", ""), len(mine), len(theirs), flag))
            for va in sorted(only_mine):
                print("        only MINE   %#x" % va)
            for va in sorted(only_gh):
                print("        only GHIDRA %#x %s" % (va, gh[r["va"]][va]))
            for va, mk, gk in kinddiff:
                print("        KIND differs %#x mine=%s ghidra=%s" % (va, mk, gk))
            if internal:
                print("        %d ref(s) sit in an UNEXPORTED internal (Ghidra names a "
                      "FUN_ function there): attribution withheld, "
                      "owner_port_state=UNKNOWN-OWNER" % internal)
            if renamed:
                print("        %d ref(s) where Ghidra's function name simply differs "
                      "from the mangled export name (a project rename); attribution "
                      "kept" % renamed)
            if only_mine or only_gh or kinddiff:
                c3_red = True
        print("   CONTROL 3: %s" % ("no set differences" if not c3_red
                                    else "DISAGREEMENTS above, causes in the report"))
        print("   (excluded from every row: the export-directory DATA ref in .rdata "
              "and the entry-point sweep, which are not .text references)")

    print("\nSUMMARY")
    print("   control 1 SaveLoad          : %s" % ("GREEN" if not c1_red else "RED"))
    print("   control 2 count/kind        : %s" % ("GREEN" if not c2_count_red else "RED"))
    print("   control 2 classification    : %s" % ("GREEN" if not c2_split_red else "RED"))
    print("   control 3 Ghidra            : %s"
          % ("NOT RUN" if not gh_ok else ("no differences" if not c3_red
                                          else "differences reported")))
    if args.no_strip_comments:
        print("\n   --no-strip-comments was in effect: compare this run's reference")
        print("   counts and port states against a normal run. The reference counts")
        print("   come from the BINARY and must not move at all.")
    if c1_red or c2_count_red:
        return 2
    if not gh_ok:
        return 4
    if c2_split_red:
        # The reference FACTS are green and one GROUND TRUTH's classification is
        # not. That is a different thing from a broken instrument and gets its
        # own code so neither can be mistaken for the other.
        return 5
    return 0


if __name__ == "__main__":
    sys.exit(main())
