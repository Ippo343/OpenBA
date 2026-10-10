"""Regenerate docs/runtime_port/endgame_b/b4_session/LEFT_FREE.md sections A and B from the tree
(section C comes from leftfree_c.py), so that acceptance.sh lines 08 / 09 measure the SURVIVORS of
the B8 quality pass: every declaration left in a code/Game exe_bodies.h / *_funcs.h header, with the
ruling written on its declaration line, and every code/Game function with a void* first parameter.
usage (repo root): python docs/runtime_port/endgame_b/b8_session/leftfree/regen.py
"""
import os, re, subprocess, sys
ROOT = r'C:/code/GIT/untitled_rev_eng_project'
B4 = os.path.join(ROOT, 'docs/runtime_port/endgame_b/b4_session')
sys.path.insert(0, B4)
os.chdir(ROOT)
import castmetric as CM
import leftfree_c as LC
PROD = ('code', 'runtime', 'standalone', 'dx11', 'engine')
CM.run(list(PROD))
KW = {'return', 'if', 'while', 'for', 'switch', 'sizeof', 'alignof', 'decltype', 'static_assert',
      'alignas', 'typeid', 'noexcept', 'operator', 'new', 'delete', 'case', 'else', 'do', 'throw',
      'defined', 'catch', 'using', 'typedef', 'template', 'goto', 'co_return'} | CM.BUILTIN | CM.QUAL
TYPEISH = re.compile(r'[A-Za-z_0-9*&>]$')
PARAMS = r'((?:[^;{}()]|\([^;{}()]*\))*)'
RE_DECL = re.compile(r'(?<![\w.:>~])([A-Za-z_]\w*)\s*\(' + PARAMS + r'\)\s*(?:const\s*)?(?:noexcept\s*)?;')
RE_VOIDFIRST = re.compile(r'(?<![\w.:>~])([A-Za-z_]\w*)\s*\(\s*(?:const\s+)?void\s*(?:const\s*)?\*\s*'
                          r'(?:const\s*)?(?:[A-Za-z_]\w*)?\s*[,)]')


def declarer(code, start):
    j = start - 1
    while j >= 0 and code[j] in ' \t\r\n':
        j -= 1
    if j < 0 or not TYPEISH.match(code[j]):
        return False
    k = j
    while k >= 0 and (code[k].isalnum() or code[k] == '_'):
        k -= 1
    tok = code[k + 1:j + 1]
    return not tok or tok not in {'return', 'case', 'else', 'throw', 'delete', 'new', 'goto',
                                  'co_return', 'do', 'sizeof', 'operator'}


def head():
    return subprocess.run(['git', 'rev-parse', '--short', 'HEAD'], capture_output=True, text=True).stdout.strip()


# ---- section A
rowsA = []
for rel, (src, code) in sorted(CM._CACHE.items()):
    base = rel.rsplit('/', 1)[-1]
    if not (rel.startswith('code/Game/') and (base == 'exe_bodies.h' or base.endswith('_funcs.h'))):
        continue
    code2 = re.sub(r'(?m)^\s*#.*$', '', code)
    srclines = src.split('\n')
    for m in RE_DECL.finditer(code2):
        n = m.group(1)
        if n in KW or not declarer(code2, m.start(1)):
            continue
        line, tag, addr = 0, '', ''
        for k, sl in enumerate(srclines):
            if re.search(r'(?<![\w.:>~])' + re.escape(n) + r'\s*\(', sl.split('//')[0]):
                line = k + 1
                span = []
                j = k
                while j < len(srclines) and len(span) < 8:
                    span.append(srclines[j])
                    if ';' in srclines[j].split('//')[0]:
                        break
                    j += 1
                txt = ' '.join(span)
                a = re.findall(r'0x1[48]0[0-9a-fA-F]{6}', txt)
                if not a:
                    a = re.findall(r'0x1[48]0[0-9a-fA-F]{6}', ' '.join(srclines[max(0, k - 4):k]))
                addr = a[0] if a else ''
                cm = re.findall(r'//\s*(.*)', txt)
                # the ruling: a `B8:` tag on the declaration's lines, else the comment lines above it
                tag = next((c for c in cm if 'B9:' in c), '') or next((c for c in cm if 'B8:' in c), '')
                if not tag:
                    above = []
                    j = k - 1
                    while j >= 0 and srclines[j].strip().startswith('//') and len(above) < 6:
                        above.insert(0, srclines[j].strip()[2:].strip())
                        j -= 1
                    tag = next((c for c in above + cm if 'B9:' in c), '') or next((c for c in above + cm if 'B8:' in c), ' '.join(cm) or ' '.join(above))
                break
        tag = re.sub(r'\s+', ' ', tag).strip()
        rowsA.append((rel, line, n, addr, tag))
byhdr = {}
for r in rowsA:
    byhdr.setdefault(r[0], []).append(r)

# ---- section B
vf = {}
for rel, (src, code) in sorted(CM._CACHE.items()):
    if not rel.startswith('code/Game/'):
        continue
    for m in RE_VOIDFIRST.finditer(code):
        n = m.group(1)
        if n in KW or not declarer(code, m.start(1)):
            continue
        line = code.count('\n', 0, m.start(1)) + 1
        sig = re.sub(r'\s+', ' ', code[max(0, code.rfind('\n', 0, m.start(1)) + 1):code.find(')', m.end(1)) + 1]).strip()
        vf.setdefault(n, []).append((rel, line, sig[:140]))
namesA = {r[2] for r in rowsA}

# ---- section C (leftfree_c.py's markdown)
secC = subprocess.run([sys.executable, os.path.join(B4, 'leftfree_c.py')], capture_output=True, text=True).stdout
secC = secC[secC.find('## C.'):] if '## C.' in secC else secC

out = []
out.append('# LEFT_FREE: declarations left as free functions, and `void*`-first parameters in code/Game\n')
out.append(f'Regenerated at main `{head()}` by `b8_session/leftfree/regen.py` (B8, the quality pass: the generating script is KEPT; B9 re-read every survivor: a `B9:` tag on the line wins over the B8 one). '
           'Checked mechanically by `acceptance.sh` lines 08 and 09 (an identifier counts as listed when it appears anywhere in this file). '
           'History: first written at `239c7591d` (B4 close); not regenerated through S4 .. B7 (411 declared names at the B8 kit); the B8 pass '
           'moved every member of a class in retail into that class and deleted the forwarders and erased cores -- what is left here is a '
           'SURVIVOR with its ruling on its declaration line (`// B8: free in retail -- <RCX evidence>`, `// B8: stays free -- <measurement>`).\n')
out.append(f'Totals: **A = {len(namesA)} declared names** ({len(rowsA)} declarations) in `exe_bodies.h` / `*_funcs.h`; **B = {len(vf)} functions** with a `void*` first parameter in code/Game.\n')
out.append('## A. Declarations left in exe_bodies.h / *_funcs.h\n')
out.append('One row per declaration: the name, where, the retail address on the line, and the ruling written on the line (a `B8: free in retail` ruling quotes what RCX is at a retail caller; a `B8: stays free` tag quotes the measurement -- a function-pointer site, a B11 DIFF, a port-only body with no retail symbol, a harness route). A row with no B8 tag carries its own comment.\n')
for hdr in sorted(byhdr):
    rows = byhdr[hdr]
    out.append(f'\n### {hdr} ({len(rows)})\n')
    out.append('| name | line | retail | ruling / comment on the line |\n|---|---:|---|---|')
    for rel, line, n, addr, tag in sorted(rows, key=lambda r: r[1]):
        out.append(f'| `{n}` | {line} | {addr} | {tag[:260].replace("|", "/")} |')
out.append('\n## B. code/Game functions with a `void*` first parameter\n')
out.append('One row per name (every definition / declaration found). "in A" = declared in an exe_bodies.h / *_funcs.h header (section A). Those not in A are the harness / port-only bodies and the dead-RCX free functions B8 MISC ruled (their callers pass six different types; retail never reads RCX).\n')
out.append('| name | file:line | signature | in A |\n|---|---|---|---|')
for n in sorted(vf):
    locs = vf[n]
    rel, line, sig = locs[0]
    extra = f' +{len(locs) - 1}' if len(locs) > 1 else ''
    out.append(f'| `{n}` | {rel.replace("code/Game/", "")}:{line}{extra} | `{sig.replace("|", "/")}` | {"yes" if n in namesA else "no"} |')
out.append('')
out.append(secC.rstrip('\n'))
out.append('')
open(os.path.join(B4, 'LEFT_FREE.md'), 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
print(f'A: {len(namesA)} names / {len(rowsA)} declarations in {len(byhdr)} headers; B: {len(vf)}')
