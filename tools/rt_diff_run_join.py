#!/usr/bin/env python3
"""Join the armed run's per-site table against the static site classification.

SUBJECT: the 630 sites RtDiffSummary printed in run1_error.log, matched BY NAME
to the RT_DIFF_BEGIN names found in source.  For each source site we record
which comparison macros its enclosing function actually uses:

  RT_DIFF_SCALAR / RT_DIFF_FLOAT / RT_DIFF_BYTES -- these EVALUATE the original
      (SCALAR/FLOAT) or require the caller to have produced `theirs` (BYTES).
      A site using one of these is an ORACLE by construction.
  RT_DIFF_SHADOW -- classified separately by rt_diff_site_classify.py into
      ATTENDANCE / SELF-CHECK / ORACLE(-candidate).

A site may use several.  The population it lands in is the STRONGEST evidence
it carries: ORACLE > SELF-CHECK > ATTENDANCE > NONE.
"""
import os, re, sys, json, collections

REPO = sys.argv[1]
RUN = sys.argv[2]
CLS = sys.argv[3]

sys.path.insert(0, os.path.join(REPO, 'tools'))
import importlib.util
spec = importlib.util.spec_from_file_location(
    'cls', os.path.join(REPO, 'tools', 'rt_diff_site_classify.py'))

# re-implement the small bits we need rather than importing (the module runs main())
DOOR_RE = re.compile(r'RT_ORIGINAL_ONCE|RtOriginal\s*<|g_orig_\w+|RT_ORIGINAL')
ROOTS = [os.path.join(REPO, d) for d in ("runtime", "code", "standalone", "engine", "dx11")]


def strip(src):
    out = []; i = 0; n = len(src); st = 0
    while i < n:
        c = src[i]; d = src[i:i + 2]
        if st == 0:
            if d == '//': st = 1; i += 2; out.append('  '); continue
            if d == '/*': st = 2; i += 2; out.append('  '); continue
            if c == '"': st = 3; out.append('"'); i += 1; continue
            if c == "'": st = 4; out.append("'"); i += 1; continue
            out.append(c); i += 1; continue
        if st == 1:
            out.append('\n' if c == '\n' else ' ')
            if c == '\n': st = 0
            i += 1; continue
        if st == 2:
            if d == '*/': st = 0; i += 2; out.append('  '); continue
            out.append('\n' if c == '\n' else ' '); i += 1; continue
        if c == '\\': out.append('  '); i += 2; continue
        q = '"' if st == 3 else "'"
        if c == q: st = 0; out.append(q); i += 1; continue
        out.append('\n' if c == '\n' else ' '); i += 1
    return ''.join(out)


# --- which macros follow each RT_DIFF_BEGIN, up to the next RT_DIFF_BEGIN -----
macros = collections.defaultdict(set)
for root in ROOTS:
    for dp, _, fns in os.walk(root):
        for fn in fns:
            if not fn.endswith('.cpp') or fn.startswith('rt_diff'):
                continue
            full = os.path.join(dp, fn)
            raw = open(full, encoding='utf-8', errors='replace').read()
            code = strip(raw)          # same length, so offsets align
            # Find the macro in STRIPPED code (so a commented-out one is not a
            # site), but read its NAME from RAW -- strip() blanks string
            # literal contents, which silently turned every name into spaces
            # and made this join report 630/630 unmatched.
            begins = []
            for m in re.finditer(r'RT_DIFF_BEGIN\s*\(\s*"', code):
                nm = re.match(r'RT_DIFF_BEGIN\s*\(\s*"([^"]*)"', raw[m.start():])
                if nm:
                    begins.append((m.start(), nm.group(1)))
            for i, (off, name) in enumerate(begins):
                end = begins[i + 1][0] if i + 1 < len(begins) else len(code)
                seg = code[off:end]
                # Touch the entry even when the site uses NO comparison macro:
                # a defaultdict populated only on a hit reports a site that
                # exists but compares nothing as "not in source", which is a
                # different and much more alarming claim.
                macros[name]
                for mac in ('RT_DIFF_SCALAR', 'RT_DIFF_FLOAT', 'RT_DIFF_BYTES',
                            'RT_DIFF_SHADOW'):
                    if re.search(r'\b%s\b' % mac, seg):
                        macros[name].add(mac)

            # HAND-ROLLED SITES.  101 sites in the tree never use RT_DIFF_BEGIN
            # at all: they declare `static RtDiffSite s = { "name", ... };` and
            # call RtDiffReport/RtDiffReportBytes directly.  A macro-only scan
            # reports every one of them as "not in source" -- which is how this
            # join first printed 268 unmatched sites.
            for m in re.finditer(r'RtDiffSite\s+\w+\s*=\s*\{\s*"', code):
                nm = re.match(r'RtDiffSite\s+\w+\s*=\s*\{\s*"([^"]*)"', raw[m.start():])
                if not nm:
                    continue
                name = nm.group(1)
                # scope: the enclosing brace block, found by walking outward
                depth = 0; start = None
                for j in range(m.start(), -1, -1):
                    if code[j] == '}': depth += 1
                    elif code[j] == '{':
                        if depth == 0: start = j; break
                        depth -= 1
                depth = 0; end = len(code)
                for j in range(m.start(), len(code)):
                    if code[j] == '{': depth += 1
                    elif code[j] == '}':
                        if depth == 0: end = j; break
                        depth -= 1
                seg = code[start if start is not None else 0:end]
                if re.search(r'\bRtDiffReportBytes\b', seg):
                    macros[name].add('RT_DIFF_BYTES')
                if re.search(DOOR_RE, seg):
                    macros[name].add('HANDROLLED-DOOR')
                elif re.search(r'\bRtDiffReport\b', seg):
                    macros[name].add('HANDROLLED-SELFCHECK')
                macros[name].add('HANDROLLED')

# --- shadow classification, by site name -------------------------------------
shadow = collections.defaultdict(set)
for r in json.load(open(CLS)):
    shadow[r['name']].add(r['kind'])

# --- the run -----------------------------------------------------------------
run = []
for L in open(RUN, encoding='utf-8', errors='replace'):
    m = re.match(r'\[RtDiff\]\s+(.*?)\s+calls=(\d+)\s+worked=(\S+)\s+divergences=(\d+)', L)
    if m:
        run.append((m.group(1).strip(), int(m.group(2)), m.group(3), int(m.group(4))))


def population(name):
    mac = macros.get(name)
    if mac is None:
        return 'NO-SOURCE-MATCH'
    hard = mac & {'RT_DIFF_SCALAR', 'RT_DIFF_FLOAT', 'RT_DIFF_BYTES', 'HANDROLLED-DOOR'}
    kinds = shadow.get(name, set())
    if hard or any(k.startswith('ORACLE') for k in kinds):
        return 'ORACLE'
    if 'SELF-CHECK' in kinds or 'HANDROLLED-SELFCHECK' in mac:
        return 'SELF-CHECK'
    if 'ATTENDANCE' in kinds:
        return 'ATTENDANCE'
    return 'NO-COMPARISON'


agg = collections.defaultdict(lambda: [0, 0, 0, 0, 0])  # sites, calls, div, calls0, calls0worked
for (name, calls, worked, div) in run:
    p = population(name)
    a = agg[p]
    a[0] += 1; a[1] += calls; a[2] += div
    if calls == 0:
        a[3] += 1
        if worked not in ('?', '0'):
            a[4] += 1

print("THE ARMED RUN, SPLIT BY WHAT EACH SITE CAN ACTUALLY PROVE")
print("(subject: the %d sites RtDiffSummary printed; joined BY NAME to source)" % len(run))
print()
print("%-16s %6s %14s %6s %8s %10s" % ('population', 'sites', 'calls', 'diverg', 'calls=0', 'c=0,work>0'))
order = ['ORACLE', 'SELF-CHECK', 'ATTENDANCE', 'NO-COMPARISON', 'NO-SOURCE-MATCH']
for p in order:
    if p in agg:
        a = agg[p]
        print("%-16s %6d %14d %6d %8d %10d" % (p, a[0], a[1], a[2], a[3], a[4]))
tot = [sum(agg[p][i] for p in agg) for i in range(5)]
print("%-16s %6d %14d %6d %8d %10d" % ('TOTAL', *tot))

print()
print("NO-SOURCE-MATCH sites (site name in the run that no RT_DIFF_BEGIN literal")
print("in source produces -- a name built at runtime, or a stale binary):")
miss = [r for r in run if population(r[0]) == 'NO-SOURCE-MATCH']
for r in miss[:25]:
    print("   %-58s calls=%-9d div=%d" % (r[0][:58], r[1], r[3]))
if len(miss) > 25:
    print("   ... and %d more" % (len(miss) - 25))
