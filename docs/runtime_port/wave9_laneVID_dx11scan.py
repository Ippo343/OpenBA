import os, re
REPO = os.getcwd()
REG = re.compile(r'RegisterRuntimeExport\s*\(\s*"([^"]+)"')
MINT = re.compile(r'"(\?[^"\s]+)"')

def strip_comments(text):
    out, i, n = [], 0, len(text)
    while i < n:
        if text.startswith("//", i):
            j = text.find("\n", i); i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2); i = n if j < 0 else j + 2
        elif text[i] in '"\'':
            q = text[i]; out.append(text[i]); i += 1
            while i < n and text[i] != q:
                if text[i] == "\\":
                    out.append(text[i]); i += 1
                if i < n:
                    out.append(text[i]); i += 1
            if i < n:
                out.append(text[i]); i += 1
        else:
            out.append(text[i]); i += 1
    return "".join(out)

ported, minted = set(), set()
for d in ["runtime", "standalone", "engine", "code", "dx11"]:
    for root, _, fs in os.walk(os.path.join(REPO, d)):
        for f in fs:
            if not f.endswith((".cpp", ".h", ".inc")):
                continue
            p = os.path.join(root, f)
            rel = os.path.relpath(p, REPO).replace(os.sep, "/")
            t = strip_comments(open(p, encoding="utf-8", errors="replace").read())
            for m in REG.finditer(t):
                ported.add(m.group(1))
            if rel == "standalone/rt_data_mint_tables.inc":
                for m in MINT.finditer(t):
                    minted.add(m.group(1))

ENTRY = re.compile(r'"(\?[^"\s]+)"\s*,\s*0x[0-9a-fA-F]+')
DIRECT = re.compile(r'Dx11ResolveRuntimeImport\s*\(\s*"(\?[^"\s]+)"')
req = {}
for root, _, fs in os.walk(os.path.join(REPO, "dx11")):
    for f in fs:
        if not f.endswith((".cpp", ".h")):
            continue
        p = os.path.join(root, f)
        rel = os.path.relpath(p, REPO).replace(os.sep, "/")
        t = strip_comments(open(p, encoding="utf-8", errors="replace").read())
        for m in list(ENTRY.finditer(t)) + list(DIRECT.finditer(t)):
            req.setdefault(m.group(1), set()).add(rel)

unsat = {n: s for n, s in req.items() if n not in ported and n not in minted}
print("dx11 Runtime-import names seen: %d ; UNSATISFIED (no port, no mint): %d"
      % (len(req), len(unsat)))
for n in sorted(unsat):
    print("   UNSAT  %-72s %s" % (n, ",".join(sorted(unsat[n]))))
