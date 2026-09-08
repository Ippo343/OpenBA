"""Find raw-door resolve-AND-call sites whose argument list starts on a LATER
SOURCE LINE than the resolve.  rawdoor_audit's RESOLVE_CALL regex is per-line
(`[^;]*?` never crosses the newline in its line-at-a-time scan), so these are
invisible to it.  Lane V2 hit two by reading files it was editing
(compressedstream Create/Open, saveload GetDataDirectory); this asks how many
there are tree-wide.

Guard state is taken from rawdoor_audit's OWN per-line map, so a site this
prints is unguarded by exactly the same rule the audit uses.
"""
import os, re, sys

ROOT = r'C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a7786afda394461e0'
os.chdir(ROOT)
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import rawdoor_audit as rd

TOK = re.compile(r'RT_ORIGINAL_ONCE\s*\(|RtOriginal\s*<')


def match_paren(t, i):
    """i is at the '('; return index just past its match, or -1."""
    d = 0
    while i < len(t):
        if t[i] == '(':
            d += 1
        elif t[i] == ')':
            d -= 1
            if d == 0:
                return i + 1
        i += 1
    return -1


def match_angle(t, i):
    d = 0
    while i < len(t):
        if t[i] == '<':
            d += 1
        elif t[i] == '>':
            d -= 1
            if d == 0:
                return i + 1
        i += 1
    return -1


total = wrapped = unguarded = 0
for root in rd.ROOTS:
    for dp, _, fns in os.walk(root):
        for fn in fns:
            if not fn.endswith(rd.EXTS):
                continue
            path = os.path.join(dp, fn).replace('\\', '/')
            fs = rd.scan(path)
            if fs.error:
                print('SKIP %s: %s' % (path, fs.error))
                continue
            raw = open(path, encoding='utf-8', errors='replace').read()
            # comment-stripped, line-preserving, same as the audit's own text
            code_lines = []
            in_block = False
            for line in raw.split('\n'):
                if in_block:
                    if '*/' in line:
                        line = line.split('*/', 1)[1]
                        in_block = False
                    else:
                        code_lines.append('')
                        continue
                line = rd.strip_comments_and_strings(line)
                if '/*' in line:
                    b, _, a = line.partition('/*')
                    if '*/' in a:
                        line = b + a.split('*/', 1)[1]
                    else:
                        line, in_block = b, True
                code_lines.append(line)
            text = '\n'.join(code_lines)
            starts = [0]
            for l in code_lines:
                starts.append(starts[-1] + len(l) + 1)

            def lineno(off):
                lo, hi = 0, len(starts) - 1
                while lo < hi - 1:
                    mid = (lo + hi) // 2
                    if starts[mid] <= off:
                        lo = mid
                    else:
                        hi = mid
                return lo + 1

            for m in TOK.finditer(text):
                if m.group(0).startswith('RT_ORIGINAL_ONCE'):
                    end = match_paren(text, text.index('(', m.start()))
                else:
                    ae = match_angle(text, text.index('<', m.start()))
                    if ae < 0:
                        continue
                    j = ae
                    while j < len(text) and text[j].isspace():
                        j += 1
                    if j >= len(text) or text[j] != '(':
                        continue
                    end = match_paren(text, j)
                if end < 0:
                    continue
                k = end
                while k < len(text) and text[k].isspace():
                    k += 1
                if k >= len(text) or text[k] != '(':
                    continue          # resolve only, not an immediate call
                total += 1
                ln, lncall = lineno(m.start()), lineno(k)
                if ln == lncall:
                    continue          # the audit already sees this spelling
                wrapped += 1
                if not fs.line_guarded.get(ln, False):
                    unguarded += 1
                    print('WRAPPED-UNGUARDED %s:%d  (args open on line %d)'
                          % (path, ln, lncall))
                    print('    ' + code_lines[ln - 1].strip()[:120])

print('\nimmediate-call raw sites: %d;  argument list on a later line: %d;  '
      'of those UNGUARDED: %d' % (total, wrapped, unguarded))
