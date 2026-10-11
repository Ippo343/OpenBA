"""Block 0 inventory, mechanical half: every declaration acceptance.sh line 08 counts in
code/Game exe_bodies.h / *_funcs.h, with its file:line, the retail address on the line (or the
nearest comment above), the first parameter, and the caller count over code/ runtime/ standalone/
(call forms `Name(` outside the declaring header, excluding comments). Output: TSV."""
import os, re, sys, glob
ROOT = r'C:/code/GIT/untitled_rev_eng_project'
HERE = os.path.join(ROOT, 'docs/runtime_port/endgame_b/b4_session')
sys.path.insert(0, HERE)
os.chdir(ROOT)
import castmetric as CM
PROD = ('code', 'runtime', 'standalone', 'dx11', 'engine')
CM.run(list(PROD))
KW = {'return', 'if', 'while', 'for', 'switch', 'sizeof', 'alignof', 'decltype', 'static_assert',
      'alignas', 'typeid', 'noexcept', 'operator', 'new', 'delete', 'case', 'else', 'do', 'throw',
      'defined', 'catch', 'using', 'typedef', 'template', 'goto', 'co_return'} | CM.BUILTIN | CM.QUAL
TYPEISH = re.compile(r'[A-Za-z_0-9*&>]$')
PARAMS = r'((?:[^;{}()]|\([^;{}()]*\))*)'
RE_DECL = re.compile(r'(?<![\w.:>~])([A-Za-z_]\w*)\s*\(' + PARAMS + r'\)\s*(?:const\s*)?(?:noexcept\s*)?;')


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


rows = []
for rel, (src, code) in sorted(CM._CACHE.items()):
    base = rel.rsplit('/', 1)[-1]
    if not (rel.startswith('src/game/') and (base == 'exe_bodies.h' or base.endswith('_funcs.h'))):
        continue
    code2 = re.sub(r'(?m)^\s*#.*$', '', code)
    srclines = src.split('\n')
    for m in RE_DECL.finditer(code2):
        n = m.group(1)
        if n in KW or not declarer(code2, m.start(1)):
            continue
        # the source line of the declaration (blanked code does not keep line numbers): the first
        # source line holding `name(` whose statement ends in `;`; the retail address is the first
        # 0x14.../0x18... on the declaration's lines, else in the 3 comment lines above it
        line = 0
        for k, sl in enumerate(srclines):
            if re.search(r'(?<![\w.:>~])' + re.escape(n) + r'\s*\(', sl.split('//')[0]):
                line = k + 1
                break
        addr = ''
        if line:
            span = []
            k = line - 1
            while k < len(srclines) and len(span) < 6:
                span.append(srclines[k])
                if ';' in srclines[k].split('//')[0]:
                    break
                k += 1
            a = re.findall(r'0x1[48]0[0-9a-fA-F]{6}', ' '.join(span))
            if not a:
                a = re.findall(r'0x1[48]0[0-9a-fA-F]{6}', ' '.join(srclines[max(0, line - 4):line - 1]))
            addr = a[0] if a else ''
        params = m.group(2).strip()
        first = params.split(',')[0].strip() if params else ''
        rows.append((n, rel, line, addr, first, re.sub(r'\s+', ' ', params)[:160]))

# caller counts: call forms outside the declaring header, in blanked (comment-free) code
names = {r[0] for r in rows}
callers = {n: [] for n in names}
pat = re.compile(r'(?<![\w.:>~])(' + '|'.join(sorted(map(re.escape, names), key=len, reverse=True)) + r')\s*\(')
for rel, (src, code) in CM._CACHE.items():
    if not rel.startswith(('code/', 'runtime/', 'standalone/')):
        continue
    for m in pat.finditer(code):
        n = m.group(1)
        line = code.count('\n', 0, m.start(1)) + 1
        callers[n].append(f'{rel}:{line}')

out = open(sys.argv[1], 'w', encoding='utf-8', newline='\n')
out.write('name\tdeclared_in\tline\tretail_addr\tfirst_param\tcallers_n\tcallers\tparams\n')
for n, rel, line, addr, first, params in sorted(rows, key=lambda r: (r[1], r[2])):
    cs = [c for c in callers[n] if not c.startswith(rel + ':')]
    # a definition `T Name(` in a .cpp is also matched; the agent reads the list
    out.write(f'{n}\t{rel}\t{line}\t{addr}\t{first}\t{len(cs)}\t{" ".join(cs[:12])}\t{params}\n')
print(len(rows), 'declarations;', len(names), 'names', file=sys.stderr)
