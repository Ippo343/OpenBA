"""Enumerate every fn_* assignment in engine/funcs/engine_funcs.cpp that is fed
by ResolveAddr(Module::Exe, 0x...), classify its SHAPE (slot-deref vs direct
cast), pull the matching typedef out of engine_funcs.h, and emit a CSV the
Ghidra pass can walk.

This is the extractor half of lane RETSLOT. It deliberately does NOT decide
anything about the ABI -- that needs the real signature from Ghidra. Its only
job is to produce a corpus that is provably the WHOLE corpus, because the
failure mode this lane exists to avoid is a clean sweep that is an artefact of
a regex.

SELF-CHECK (printed every run): every `ResolveAddr(Module::Exe` occurrence in
the file is either inside a matched fn_* assignment or listed line-by-line as
residue, so a site this tool cannot see is IMPOSSIBLE to hide.
"""
import os, re, sys, csv
from collections import Counter

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
SRC = os.path.join(ROOT, "engine", "funcs", "engine_funcs.cpp")
HDR = os.path.join(ROOT, "engine", "funcs", "engine_funcs.h")
OUT = sys.argv[2] if len(sys.argv) > 2 else "retslot_corpus.csv"

text = open(SRC, encoding="utf-8", errors="replace").read()
hdr = open(HDR, encoding="utf-8", errors="replace").read()
lines = text.splitlines()

TOTAL = len(re.findall(r"ResolveAddr\s*\(\s*Module::Exe", text))

PAT = re.compile(
    r"\b(fn_[A-Za-z0-9_]+)\s*=\s*([^;]*?ResolveAddr\s*\(\s*Module::Exe\s*,\s*(0x[0-9a-fA-F]+)[^;]*?);",
    re.S)

rows = []
spans = []
for m in PAT.finditer(text):
    name, rhs, addr = m.group(1), m.group(2), m.group(3)
    line = text.count("\n", 0, m.start()) + 1
    shape = "slot-deref" if re.search(r"\*\s*reinterpret_cast", rhs) else "direct-cast"
    rows.append(dict(fn=name, addr=addr.lower(), line=line, shape=shape))
    spans.append((m.start(), m.end()))

residue = []
for m in re.finditer(r"ResolveAddr\s*\(\s*Module::Exe", text):
    if any(s <= m.start() < e for s, e in spans):
        continue
    line = text.count("\n", 0, m.start()) + 1
    residue.append((line, lines[line - 1].strip()[:110]))


def typedef_body(tname):
    td = re.search(
        r"typedef\s+([^;]*?)\(\s*(?:__[a-z]+\s+)?\*\s*%s\s*\)\s*\(([^;]*?)\)\s*;"
        % re.escape(tname), hdr, re.S)
    if not td:
        return None, None
    return " ".join(td.group(1).split()), " ".join(td.group(2).split())


def typedef_for(name):
    """Resolve fn_NAME to its typedef body. Most sites use the same-named
    fn_NAME_t, but a substantial minority (17 of 254) DECLARE the variable with
    a SHARED typedef under a different name -- `extern fn_Array_Resize_t
    fn_Array_Resize_Stride08;`. Looking only for fn_NAME_t reported those 17 as
    "<NO TYPEDEF FOUND>", i.e. as a hole in the corpus, when the type was there
    all along under another name."""
    ret, args = typedef_body(name + "_t")
    if ret is not None:
        return ret, args, name + "_t"
    ext = re.search(r"\bextern\s+([A-Za-z_][A-Za-z0-9_]*)\s+%s\s*;" % re.escape(name), hdr)
    if ext:
        ret, args = typedef_body(ext.group(1))
        if ret is not None:
            return ret, args, ext.group(1)
    return None, None, None


for r in rows:
    ret, args, tname = typedef_for(r["fn"])
    r["typedef"] = tname or ""
    r["ret"] = ret if ret is not None else "<NO TYPEDEF FOUND>"
    r["args"] = args if args is not None else ""
    if args is None or args.strip() in ("", "void"):
        r["nargs"] = "0" if args is not None else ""
    else:
        r["nargs"] = str(len(re.split(r",(?![^<]*>)", args)))


def bucket(a):
    v = int(a, 16)
    if 0x140150000 <= v < 0x140160000:
        return "import-slot"
    if 0x140001000 <= v < 0x140150200:
        return "exe-text"
    return "other-data"


for r in rows:
    r["bucket"] = bucket(r["addr"])

print("ResolveAddr(Module::Exe occurrences in file : %d" % TOTAL)
print("fn_* assignments matched                   : %d" % len(rows))
print("unmatched residue occurrences              : %d" % len(residue))
print("  by bucket : %s" % dict(Counter(r["bucket"] for r in rows)))
print("  by shape  : %s" % dict(Counter(r["shape"] for r in rows)))
print("  rows with NO typedef found: %d"
      % sum(1 for r in rows if r["ret"] == "<NO TYPEDEF FOUND>"))

if "-v" in sys.argv:
    print("\n--- residue ---")
    for line, src in residue:
        print("  %5d  %s" % (line, src))

with open(OUT, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=["fn", "addr", "line", "shape", "bucket",
                                      "typedef", "ret", "nargs", "args"])
    w.writeheader()
    for r in sorted(rows, key=lambda r: r["addr"]):
        w.writerow(r)
print("\nwrote %s (%d rows)" % (OUT, len(rows)))
