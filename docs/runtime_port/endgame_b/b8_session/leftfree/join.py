"""Join inv.tsv (declarations) with LEFT_FREE.md's reason column, Ghidra's name at the address,
and the class the first parameter names (with the header that defines that class)."""
import re, os, sys, glob
ROOT = r'C:/code/GIT/untitled_rev_eng_project'
S = os.path.dirname(os.path.abspath(__file__))
os.chdir(ROOT)
inv = [l.rstrip('\n').split('\t') for l in open(os.path.join(S, 'inv.tsv'), encoding='utf-8')][1:]
gh = {}
for l in open(os.path.join(S, 'ghidra_names.tsv'), encoding='utf-8'):
    f = l.rstrip('\n').split('\t')
    gh[f[0]] = f[1:]
# LEFT_FREE reasons by name
reason = {}
for l in open('docs/runtime_port/endgame_b/b4_session/LEFT_FREE.md', encoding='utf-8'):
    m = re.match(r'^\| `([A-Za-z_]\w*)` \| ([^|]*)\| ([^|]*)\| (.*)\|\s*$', l)
    if m:
        reason[m.group(1)] = (m.group(3).strip(), m.group(4).strip())
# class -> header
classes = {}
for h in glob.glob('src/game/**/*.h', recursive=True) + glob.glob('runtime/types/**/*.h', recursive=True):
    txt = open(h, encoding='utf-8', errors='replace').read()
    for m in re.finditer(r'(?m)^\s*(?:class|struct)\s+([A-Za-z_]\w*)\s*(?:final\s*)?(?::|\{)', txt):
        classes.setdefault(m.group(1), h.replace('\\', '/'))
    for m in re.finditer(r'(?m)^\s*(?:class|struct)\s+([A-Za-z_]\w*)\s*(?:final\s*)?\n\s*(?::|\{)', txt):
        classes.setdefault(m.group(1), h.replace('\\', '/'))
out = open(os.path.join(S, 'inventory.tsv'), 'w', encoding='utf-8', newline='\n')
out.write('name\tdeclared_in\tline\tretail\tghidra_name\tfirst_param\tclass\tclass_header\tcallers_n\tleftfree_class\tleftfree_reason\tcallers\n')
for name, rel, line, addr, first, cn, callers, params in inv:
    g = gh.get(addr, ['', '', '', ''])
    t = re.sub(r'\b(const|struct|class)\b', '', first).strip()
    t = re.sub(r'\s*[*&]+\s*[A-Za-z_]\w*\s*$', '', t).strip()   # type before the name
    t = t.split('::')[-1].strip()
    cls = t if t in classes else ''
    if not cls and first:
        mm = re.match(r'\s*(?:const\s+)?([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*[*&]', first)
        if mm and mm.group(1).split('::')[-1] in classes:
            cls = mm.group(1).split('::')[-1]
    lf = reason.get(name, ('', ''))
    out.write('\t'.join([name, rel, str(line), addr, g[0], first, cls, classes.get(cls, ''), cn, lf[0], lf[1], callers]) + '\n')
out.close()
print(len(inv), 'rows')
