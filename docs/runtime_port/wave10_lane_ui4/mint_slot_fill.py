# Archived beside the numbers it produced (WAVE10_LANE_UI4.md section 7):
# the SpriteSheet minted-vtable slot fill, 1 ported/8 traps before this lane,
# 8 ported/1 trap after. Run from the repo root. It is NOT a general tool --
# it hard-codes the one vtable symbol this lane measured.
import re, os, sys
root = sys.argv[1] if len(sys.argv) > 1 else os.getcwd()
os.chdir(root)
inc = open('standalone/rt_data_mint_tables.inc', encoding='utf-8', errors='replace').read()
src = ""
for base in ('runtime', 'code', 'engine', 'standalone', 'dx11'):
    for dp, _, fs in os.walk(base):
        for f in fs:
            if f.endswith(('.cpp', '.h', '.inc')) and 'rt_data_mint_tables' not in f:
                src += open(os.path.join(dp, f), encoding='utf-8', errors='replace').read()

m = re.search(r'\{\s*"\?\?_7SpriteSheet@UI@@6B@",\s*(0x[0-9a-f]+)ull,\s*(\d+),\s*(kVSlots\d+)', inc)
print("mint row:", m.group(0) if m else "NOT FOUND")
arr = m.group(3)
b = re.search(r'\b' + arr + r'\[\] = \{(.*?)\n\};', inc, re.S).group(1)
rows = re.findall(r'\{\s*(?:"([^"]*)"|nullptr)\s*,\s*(0x[0-9a-f]+)ull\s*,\s*(RT_MINT_\w+)', b)
filled = trap = pure = 0
for i, (name, va, kind) in enumerate(rows):
    if kind == 'RT_MINT_PURECALL':
        pure += 1; print("  slot %d PURECALL" % i); continue
    reg = bool(name and re.search(r'RegisterRuntimeExport\(\s*"' + re.escape(name) + r'"', src)) \
          or bool(re.search(r'RegisterRuntimeAddr\(\s*' + va, src, re.I))
    print("  slot %d %-8s %s %s" % (i, "PORTED" if reg else "TRAP", va, name or "<unnamed>"))
    if reg: filled += 1
    else: trap += 1
print("slots=%d ported=%d TRAP=%d pure=%d" % (len(rows), filled, trap, pure))
