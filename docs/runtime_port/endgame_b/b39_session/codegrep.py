#!/usr/bin/env python3
"""codegrep.py PATTERN DIR... [--ext=.cpp,.h] [--count] -- grep C/C++ CODE only (comments stripped, strings kept)."""
import os, re, sys
EXCL = ('standalone/build', 'tests/unit/build', '.claude/worktrees', '/build/', '/build-')


def strip(src):
    out = []; i = 0; n = len(src); st = None
    while i < n:
        c = src[i]
        if st is None:
            if src.startswith('//', i):
                j = src.find('\n', i); j = n if j < 0 else j
                i = j; continue
            if src.startswith('/*', i):
                j = src.find('*/', i + 2); j = n if j < 0 else j + 2
                out.append('\n' * src.count('\n', i, j)); i = j; continue
            if c == '"' or c == "'":
                if c == '"' and i > 0 and src[i - 1] == 'R':
                    m = re.match(r'"([^(\s]*)\(', src[i:])
                    if m:
                        end = src.find(')' + m.group(1) + '"', i)
                        end = n if end < 0 else end + len(m.group(1)) + 2
                        out.append(src[i:end]); i = end; continue
                if c == "'" and i > 0 and (src[i - 1].isalnum()):
                    # digit separator (1'000) -- not a char literal
                    out.append(c); i += 1; continue
                st = c; out.append(c); i += 1; continue
            out.append(c); i += 1
        else:
            if c == '\\' and i + 1 < n:
                out.append(src[i:i + 2]); i += 2; continue
            if c == st or c == '\n':
                st = None
            out.append(c); i += 1
    return ''.join(out)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    exts = ('.cpp', '.h', '.hpp', '.inc', '.c')
    for a in sys.argv[1:]:
        if a.startswith('--ext='):
            exts = tuple(a[6:].split(','))
    quiet = '--count' in sys.argv
    pat = re.compile(args[0]); roots = args[1:]
    total = 0
    for r in roots:
        paths = [r] if os.path.isfile(r) else [os.path.join(dp, f) for dp, dn, fs in os.walk(r) for f in fs]
        for p in sorted(paths):
            q = p.replace('\\', '/')
            if any(e in q for e in EXCL) or not q.endswith(exts):
                continue
            try:
                src = open(p, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            for ln, line in enumerate(strip(src).split('\n'), 1):
                if pat.search(line):
                    total += 1
                    if not quiet:
                        print(f'{q}:{ln}:{line.strip()[:220]}')
    print(f'# {total} code line(s)')


if __name__ == '__main__':
    main()
