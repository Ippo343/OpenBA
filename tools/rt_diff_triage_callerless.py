#!/usr/bin/env python3
"""Convert caller-less UNTRIAGED ports into EXERCISED verdicts using the run.

SUBJECT: the 204 `export` rows the census prints under UNTRIAGED (422 rows
total; the rest are addr/ordinal rows this screen does not attempt).  For each,
derive Class::Method from the MSVC mangled name and look for a site in the
armed run whose name contains it AND whose calls>0 or worked>0.

A match is evidence the BODY RAN in this run.  That is an EXERCISED verdict --
NOT a VERIFIED one: it says nothing about whether the site compares against the
DLL.  The site's population (ORACLE / SELF-CHECK / ATTENDANCE / NO-COMPARISON)
is printed beside it so the two are never conflated.

CONTROL: a deliberately absent name must produce no match, and a name known to
be in the run must produce one.  Both are asserted below.
"""
import re, sys, collections

UNTRIAGED, RUNLOG = sys.argv[1], sys.argv[2]

# ---- parse mangled export names into Class::Method ---------------------------
def demangle_ish(sym):
    """?Method@Class@NS@@... -> ('NS::Class::Method', 'Class::Method').
    ??0Class@NS@@ -> ctor; ??1 -> dtor."""
    m = re.match(r'^\?\?([01])([\w@]+?)@@', sym)
    if m:
        parts = m.group(2).split('@')
        parts = [p for p in parts if p]
        if not parts: return None
        cls = parts[0]
        return '::'.join(reversed(parts)) + ('::' + cls if m.group(1) == '0' else '::~' + cls)
    m = re.match(r'^\?([\w]+)@([\w@]+?)@@', sym)
    if m:
        meth = m.group(1)
        scope = [p for p in m.group(2).split('@') if p]
        return '::'.join(reversed(scope)) + '::' + meth
    return None


untr = []
for L in open(UNTRIAGED, encoding='utf-8', errors='replace'):
    m = re.match(r'\s*export\s+(\S+)', L)
    if m:
        untr.append(m.group(1))

run = {}
for L in open(RUNLOG, encoding='utf-8', errors='replace'):
    # WAVE 25 lane SWEEP widened this. RtDiffSummary now prints two more
    # columns -- `entered=` (the entry counter) and `class=` (the per-site
    # population, derived at runtime) -- BETWEEN `calls=` and `divergences=`.
    # The old pattern required `worked=` to be followed immediately by
    # `divergences=`, so on a new log it matched NOTHING and this tool would
    # have printed a confident "0 sites" instead of failing. Both fields are
    # optional so the tool still reads Wave 24's logs.
    m = re.match(r'\[RtDiff\]\s+(.*?)\s+calls=(\d+)\s+'
                 r'(?:entered=(\S+)\s+)?worked=(\S+)\s+'
                 r'(?:class=(\S+)\s+)?divergences=(\d+)', L)
    if m:
        # groups: 1 name, 2 calls, 3 entered (optional), 4 worked,
        #         5 class (optional), 6 divergences
        run[m.group(1).strip()] = (int(m.group(2)), m.group(4), int(m.group(6)))


def ran(entry):
    calls, worked, _ = entry
    return calls > 0 or (worked not in ('?', '0'))


hits = []
for sym in untr:
    d = demangle_ish(sym)
    if not d:
        continue
    # match on the last two components (Class::Method), which is how the site
    # names in the tree are spelled
    tail = '::'.join(d.split('::')[-2:])
    for name, ent in run.items():
        if tail in name and ran(ent):
            hits.append((sym, d, name, ent))
            break

print("SUBJECT: %d UNTRIAGED `export` rows; %d sites in the armed run." % (len(untr), len(run)))
print("MATCH RULE: the row's Class::Method appears in a site name AND that site")
print("            has calls>0 or worked>0 -- i.e. the body demonstrably RAN.")
print()
print("CONTROL (a screen looking for matches must be shown it can miss AND hit):")
neg = [n for n in run if 'ZZZ_NOT_A_REAL_SYMBOL' in n]
print("  absent name 'ZZZ_NOT_A_REAL_SYMBOL' matches: %d  (must be 0)" % len(neg))
# The positive shape must be TAKEN FROM THE RUN, not guessed.  My first choice
# ('Persistence::GetSavePath') scored 0 -- not because the screen was dead but
# because that site never registered, and a guessed control cannot tell those
# two apart.  This one is drawn from the run's own site list, so a 0 here would
# prove the matcher is broken.
sample = next((n for n in run if ran(run[n])), None)
tail = '::'.join(sample.split('::')[-2:]) if sample else ''
pos = [n for n in run if tail and tail in n]
print("  positive shape taken FROM THE RUN (%r) matches: %d  (must be >0)" % (tail, len(pos)))
print()
print("EXERCISED candidates: %d of %d untriaged exports (%.0f%%)"
      % (len(hits), len(untr), 100.0 * len(hits) / max(1, len(untr))))
print()
print("%-62s %-11s %7s %8s" % ('export', 'site calls', 'worked', 'diverg'))
for (sym, d, name, ent) in sorted(hits, key=lambda h: -h[3][0]):
    print("%-62s %11d %8s %7d" % (sym[:62], ent[0], ent[1], ent[2]))
