#!/usr/bin/env python3
"""b11cmp.py BASE_OBJ_DIR TIP_OBJ_DIR [name-substring ...] [--detail] [--out FILE]

B4_RULES B11: compare the object files of two builds FUNCTION BY FUNCTION.

  BASE_OBJ_DIR / TIP_OBJ_DIR   two object directories built with the SAME flags
                               (e.g. a copy of standalone/build-perf/obj taken at the block's
                               base, and standalone/build-perf/obj at its tip)
  name-substring               only objects whose file name contains one of these
                               (`code__Game__Work__cleararea`); default: every object present
                               in both directories whose bytes differ

Per function (symbol), three things are compared, in this order:
  1. the ORDERED CALL LIST: every direct call's target symbol (from the relocation) and every
     indirect call's operand with the base register blanked (`call *0x28(%REG)`): covers
     creation order, random-call order, a dropped or added call;
  2. the STORES to memory that is not the stack: multiset of (width, displacement), so a
     store that changed width, a merged store split in two, a field no longer zeroed shows;
  3. the COMPARES against immediates / memory (multiset of (width, displacement, immediate)):
     a dropped null test or a changed loop bound shows.
Verdicts: IDENTICAL (same normalised text) / SAME (text differs, 1-3 equal: register
allocation, a member call spelled differently) / DIFF (1, 2 or 3 differs: READ IT) /
ONLY-BASE / ONLY-TIP (a function that left or appeared: a C body that became a member is one
of each; pair them by hand and say so in the hand-in).

A DIFF is not a defect by itself and a SAME is not a proof: it is the list of functions to
READ. Run it on the -O2 objects (build-perf) as well as the debug ones: at -O0 every typed
access differs, -O2 shows the real changes.
Deterministic; needs `objdump` on PATH (msys2 ucrt64). Read-only.
"""
import collections
import os
import re
import subprocess
import sys

RE_FUNC = re.compile(r'^[0-9a-f]+ <(.+)>:$')
RE_INSN = re.compile(r'^\s*[0-9a-f]+:\t(.*)$')
RE_RELOC = re.compile(r'^\s*[0-9a-f]+: (?:R_X86_64_\S+|IMAGE_REL_\S+|\S*REL32\S*)\s+(\S+)')
RE_MEM = re.compile(r'(-?0x[0-9a-f]+|-?\d+)?\(%(\w+)(?:,%\w+(?:,\d)?)?\)')
STACK = {'rsp', 'rbp', 'esp', 'ebp'}
W = {'b': 1, 'w': 2, 'l': 4, 'q': 8}


def regwidth(r):
    r = r.lstrip('%')
    if r.startswith('xmm'):
        return 'x'
    if r.startswith('r') and r[1:].isdigit():
        return 8
    if r.startswith('r') and r.endswith('d'):
        return 4
    if r.startswith('r') and r.endswith('w') and r[1:-1].isdigit():
        return 2
    if r.startswith('r') and r.endswith('b') and r[1:-1].isdigit():
        return 1
    if r.startswith('r'):
        return 8
    if r.startswith('e'):
        return 4
    if r in ('al', 'bl', 'cl', 'dl', 'sil', 'dil', 'bpl', 'spl', 'ah', 'bh', 'ch', 'dh'):
        return 1
    return 2


def width(mn, ops):
    for o in ops:
        if o.startswith('%'):
            return regwidth(o)
    if mn in ('movss', 'movd'):
        return 4
    if mn in ('movsd', 'movq'):
        return 8
    if mn.startswith(('movap', 'movup', 'movdq')):
        return 16
    if mn[-1] in W and mn[:-1] in ('mov', 'cmp', 'test', 'add', 'sub', 'and', 'or', 'xor', 'inc', 'dec'):
        return W[mn[-1]]
    return '?'


def split_ops(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def parse(path):
    try:
        txt = subprocess.run(['objdump', '-d', '-r', '--no-show-raw-insn', path],
                             capture_output=True, text=True, errors='replace').stdout
    except OSError as e:
        sys.exit(f'objdump failed: {e}')
    funcs, cur = {}, None
    for ln in txt.split('\n'):
        m = RE_FUNC.match(ln)
        if m:
            cur = {'text': [], 'calls': [], 'stores': collections.Counter(),
                   'cmps': collections.Counter()}
            funcs[m.group(1)] = cur
            continue
        if cur is None:
            continue
        m = RE_RELOC.match(ln)
        if m:
            sym = re.sub(r'[-+]0x[0-9a-f]+$', '', m.group(1))
            if cur['calls'] and cur['calls'][-1] == '<pending>':
                cur['calls'][-1] = sym
                cur.pop('pend', None)
            cur['text'].append('  R ' + re.sub(r'^\.(text|rdata|data|bss|xdata|pdata)\S*', r'.\1', sym))
            continue
        m = RE_INSN.match(ln)
        if not m:
            continue
        ins = m.group(1).split('#')[0].strip()
        if cur['calls'] and cur['calls'][-1] == '<pending>':
            cur['calls'][-1] = cur.pop('pend', '<local>')
        if not ins or ins.startswith(('nop', 'data16', 'xchg   %ax,%ax', 'int3')):
            continue
        parts = ins.split(None, 1)
        mn = parts[0]
        ops = split_ops(parts[1]) if len(parts) > 1 else []
        norm = re.sub(r'\b[0-9a-f]+ <[^>]*>', '<T>', ins)
        cur['text'].append(norm)
        if mn.startswith('call') or (mn == 'jmp' and ops and ops[0].startswith('*')):
            if ops and ops[0].startswith('*'):
                cur['calls'].append(mn + ' ' + re.sub(r'%\w+', '%REG', ops[0]))
            elif mn.startswith('call'):
                # no relocation follows an intra-object call: keep the target objdump printed
                tm = re.search(r'<([^>+]+)(?:\+0x[0-9a-f]+)?>', ins)
                cur['calls'].append('<pending>')
                cur['pend'] = tm.group(1) if tm else '<local>'
            continue
        if mn.startswith(('mov', 'and', 'or', 'add', 'sub', 'inc', 'dec', 'xor', 'set')) and ops:
            dst = ops[-1]
            mm = RE_MEM.search(dst)
            if mm and mm.group(2) not in STACK and not mn.startswith(('movz', 'movs')) or \
               (mm and mm.group(2) not in STACK and mn in ('movss', 'movsd')):
                if not dst.startswith('%'):
                    cur['stores'][(str(width(mn, ops[:-1])), mm.group(1) or '0')] += 1
        if mn.startswith(('cmp', 'test')) and len(ops) == 2:
            imm = ops[0] if ops[0].startswith('$') else ''
            mm = RE_MEM.search(ops[1]) or RE_MEM.search(ops[0])
            if mm and mm.group(2) not in STACK:
                cur['cmps'][(str(width(mn, ops)), mm.group(1) or '0', imm)] += 1
            elif imm and imm not in ('$0x0',):
                cur['cmps'][('imm', '', imm)] += 1
    for f in funcs.values():
        if f['calls'] and f['calls'][-1] == '<pending>':
            f['calls'][-1] = f.get('pend', '<local>')
        f.pop('pend', None)
    return funcs


def cdiff(a, b):
    return ' '.join(f'{k}:{a.get(k, 0)}->{b.get(k, 0)}' for k in sorted(set(a) | set(b), key=str)
                    if a.get(k, 0) != b.get(k, 0))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    detail = '--detail' in sys.argv
    out = sys.stdout
    if '--out' in sys.argv:
        i = sys.argv.index('--out')
        out = open(sys.argv[i + 1], 'w', encoding='utf-8')
        args = [a for a in args if a != sys.argv[i + 1]]
    if len(args) < 2:
        sys.exit(__doc__)
    base, tip, subs = args[0], args[1], args[2:]
    names = sorted(n for n in os.listdir(tip) if n.endswith('.o'))
    tot = collections.Counter()
    for n in names:
        if subs and not any(s in n for s in subs):
            continue
        bp, tp = os.path.join(base, n), os.path.join(tip, n)
        if not os.path.exists(bp):
            print(f'## {n}: ONLY IN TIP (new object)', file=out)
            continue
        if not subs and open(bp, 'rb').read() == open(tp, 'rb').read():
            continue
        fb, ft = parse(bp), parse(tp)
        rows = []
        for fn in sorted(set(fb) | set(ft)):
            if fn not in ft:
                rows.append(('ONLY-BASE', fn, ''))
            elif fn not in fb:
                rows.append(('ONLY-TIP', fn, ''))
            elif fb[fn]['text'] == ft[fn]['text']:
                rows.append(('IDENTICAL', fn, ''))
            else:
                why = []
                if fb[fn]['calls'] != ft[fn]['calls']:
                    why.append('calls: ' + ' | '.join(fb[fn]['calls'][:40]) + '  ==>  ' + ' | '.join(ft[fn]['calls'][:40]))
                if fb[fn]['stores'] != ft[fn]['stores']:
                    why.append('stores(width,disp): ' + cdiff(fb[fn]['stores'], ft[fn]['stores']))
                if fb[fn]['cmps'] != ft[fn]['cmps']:
                    why.append('compares(width,disp,imm): ' + cdiff(fb[fn]['cmps'], ft[fn]['cmps']))
                rows.append(('DIFF' if why else 'SAME', fn, '\n      '.join(why)))
        c = collections.Counter(r[0] for r in rows)
        tot.update(c)
        print(f'## {n}: ' + ' '.join(f'{k} {v}' for k, v in sorted(c.items())), file=out)
        for v, fn, why in rows:
            if v in ('DIFF', 'ONLY-BASE', 'ONLY-TIP') or (detail and v == 'SAME'):
                print(f'  {v} {fn}' + (f'\n      {why}' if why else ''), file=out)
    print('# TOTAL ' + ' '.join(f'{k} {v}' for k, v in sorted(tot.items())), file=out)


if __name__ == '__main__':
    main()
