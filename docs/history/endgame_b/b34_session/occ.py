"""Every non-comment, non-string OCCURRENCE of each zero-call global door."""
import os, re, sys
ns = {}
exec(open('sweep.py').read().split('def main()')[0], ns)
strip = ns['strip']; files = ns['files']; ROOT = ns['ROOT']
NAMES = sys.argv[1:] or ns['GLOBAL_DOORS']
hits = {n: [] for n in NAMES}
for path in files():
    try: src = open(path, encoding="utf-8", errors="replace").read()
    except OSError: continue
    rel = os.path.relpath(path, ROOT).replace("\\", "/")
    code = strip(src)
    lines = code.split('\n'); raw = src.split('\n')
    for n in NAMES:
        for i, ln in enumerate(lines):
            if re.search(r"\b" + re.escape(n) + r"\b", ln):
                hits[n].append((rel, i + 1, raw[i].strip()))
for n in NAMES:
    print(f"--- {n}  ({len(hits[n])} occurrences) ---")
    for rel, i, t in hits[n]:
        print(f"   {rel}:{i}  {t[:150]}")
