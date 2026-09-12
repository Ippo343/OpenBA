#!/usr/bin/env python3
"""Check every RT_UNPROMOTABLE tag: its slot count, and (Wave 21) whether it
DECLARES which denominator convention it is written in.

WHY
---
The `RT_UNPROMOTABLE:` block in ``standalone/reg_mint_promoted.cpp`` is the
source of truth for ``tools/runtime_census.py``'s per-namespace ceiling.  Its
slot-count field is PROSE, and prose describing a measurement decays silently
because nothing regenerates a comment.  One tag had carried
``??_7ReaderWriter@IO@@6B@ | 2/36`` since Wave 17; the row is 26 slots (Wave 19
correction C30, re-derived by Wave 20 lane EXTENT).  Nothing could have noticed:
the field is free text and the census derives no number from it.

THIS TOOL DELIBERATELY READS C++ COMMENTS
-----------------------------------------
Unusually for this family, the subject IS comment text: the tags are a comment
block by design.  So this tool must NOT acquire a comment stripper -- that
would blind it completely, which is the mirror of the defect recorded in
``reference_uncommented_checker_manufactures_work.md``.  The protection against
reading prose as data is instead that the tag grammar is narrow (a literal
``RT_UNPROMOTABLE:`` followed by a ``?``-prefixed mangled name and a ``|``),
and that the tool prints how many tag-shaped lines exist ELSEWHERE in the tree
that it does not read.

CHECK 1 -- THE DENOMINATOR (Wave 20)
------------------------------------
The tag's denominator against the slot count of the SAME symbol's row in
``standalone/rt_data_mint_tables.inc`` -- a generated artefact derived from the
DLL, and the thing the mint actually stamps.

THE TWO CONVENTIONS, WHICH THE TAGS NEVER DECLARED
--------------------------------------------------
``N/M (K purecall)``  M = NON-purecall slots, so the table is M + K
                      (Object and Video rows; ``??_7Base@Object@@6B@ 2/2 (1
                      purecall)`` against a 3-slot row).
``N/M ..., K purecall``  M = the WHOLE table (the two IO rows;
                      ``??_7Stream@IO@@6B@ 1/7 ..., 5 purecall`` on 7 slots).

Both are self-consistent and they disagree by K.  Check 1 accepts EITHER and
reports a row only when NEITHER fits, so it cannot manufacture work out of a
convention it happens to dislike -- at the cost of being blind to a row that is
wrong by exactly K in the direction of the other reading.  That blindness is
stated here rather than discovered later, and CHECK 2 is what retires it for
every tag written from now on.

CHECK 2 -- THE CONVENTION DECLARATION (Wave 21 item 11; WAVE20_CLOSED §6.1)
---------------------------------------------------------------------------
WAVE20_CLOSED §6.1 ruled: do not normalise the existing tags (they are
eighteen lanes' measured verdicts, and normalising rewrites verdicts by a hand
that measured none of them), but **a new tag must DECLARE ITS CONVENTION**.
That requirement lived only as prose -- §6.1 itself records that the sentence
naming a tool as "the place to enforce it" was a PRESCRIPTION read as a
COMPLETION.  This is the check.

THE AGREED FORM.  A numeric slot field must carry exactly one of:

    [CONV: M=TABLE]        the denominator M is the WHOLE table
    [CONV: M=NONPURECALL]  M counts only non-purecall slots; table = M + K

anywhere in the ``<slots ported>`` field, e.g.

    RT_UNPROMOTABLE: ??_7Foo@Bar@@6B@ | 3/9 (2 purecall) [CONV: M=NONPURECALL]
                     | WAVE21_LANE_X.md §2 | NONE, EVER

A declaration is not decoration: where the symbol has a minted row, the
DECLARED reading is checked EXACTLY, not "either of two".  So declaring turns
check 1's known blind spot off for that row.

GRANDFATHERING, and why it is (symbol, EXACT FIELD TEXT)
--------------------------------------------------------
The twenty tags that existed when this check landed are frozen below with
their field text verbatim.  They pass check 2 without a declaration -- §6.1
forbids rewriting them.  But the freeze is on the PAIR, so the moment a lane
edits one of those fields (which is exactly the moment §6.1 permits: "normalise
a tag only when the lane that owns it re-measures it"), the pair stops matching
and that tag must declare.  A frozen symbol list alone would have let an edit
slip through undeclared forever.

The list is a hardcoded literal and therefore a free parameter of this check;
it is PRINTED beside the verdict, with how many of its entries were matched, so
a reader can see it is not silently absorbing new tags.  IT MUST NEVER GROW.

CHECK 3 -- ROUTE SCOPE AND ADDRESSEE (Wave 21, and READ THE BOUND)
------------------------------------------------------------------
Wave 21 found FOUR distinct defects in the tag-and-expiry vocabulary, in four
lanes, in one wave.  **Every one of them sat in a tag that was perfectly WELL
FORMED, and a syntactic check of the existing form would have caught none:**

1. *An expiry with no ADDRESSEE never fires.*  ``??_7ReaderWriter@``'s `?`
   numerator was to be settled by "the next lane that counts ported slots on
   this row"; nothing routed a lane there for a wave.  Naming an addressee in
   the Wave 21 kickoff closed it in one lane, and the work was small.  **The
   fix was naming somebody.**
2. *An expiry whose CONDITION IS FALSE.*  ``??_7TextFileStream@`` slot 0 said
   it unblocks when ``??1TextFileStream@`` lands.  Lane ENC checked; false.
3. *An expiry of the WRONG SHAPE* -- separate from 2 and subtler.  Even had
   that dtor landed, the deleting-dtor thunk pattern used elsewhere in this
   tree does not require the callee to be ported, so the clause named a real
   event that would never have unblocked anything.  **A condition can be both
   false AND beside the point**, and checking only its truth misses this.
4. *A verdict RIGHT for a reason that is FALSE.*  Lane SHOT: the HARM in
   ``?_singleton@FileSystem@IO@@``'s tag is real while the DLL is loaded (89
   reads across 44 functions, 26 still retail, vs 2 writes, both ours) but its
   premise -- that there will ALWAYS be in-DLL readers -- is false at cutover,
   where the row is REQUIRED.  The verdict is ROUTE-SCOPED, not permanent, and
   nothing in the tag says so.  Meanwhile ``??_7Stream@IO@@6B@``, spelled
   identically, IS permanent.

So this check adds two FIELDS rather than validating the old form.  A tag's
expiry must carry both:

    [SCOPE: PERMANENT]     true on every route that could ever exist, cutover
                           included
    [SCOPE: DLL-PRESENT]   true only while Runtime-x64.dll is loaded; at
                           cutover the verdict CHANGES
    [SCOPE: CONDITIONAL]   lapses on the stated condition
    [ADDRESSEE: <who or what will evaluate this>]

WHAT THIS CAN AND CANNOT DO -- state it before quoting a green.
  * It can enforce that a field is PRESENT and drawn from a closed set.  That
    is not nothing: 36 of the 40 existing expiries say "NONE, EVER", 3 say
    "NONE while the DLL is present", and SHOT proved at least one of the 36 is
    really the second kind.  Forcing a CHOICE is what stops "NONE, EVER" being
    the default spelling -- the same argument as the [CONV:] token.
  * It CANNOT tell a correct scope from a wrong one (defect 4's correctness),
    nor a false condition from a true one (2), nor a relevant one from an
    irrelevant one (3).  Those are judgements, and no field makes them
    checkable.  Defects 2 and 3 are OUT OF REACH of any form check.
  * It is PROSPECTIVE ONLY.  All 40 existing expiries are grandfathered and
    the tool exits 0 with them missing both fields.  That backlog is printed
    as a counted NOTE, because a permanently-red tool gets ignored and a
    silent one hides the number.

CONTROLS (all must pass or it reports nothing and exits 2)
-----------------------------------------------------------
check 1 positive  a non-zero number of tags must parse AND a non-zero number of
          those must find their symbol in the .inc -- a tag naming a symbol the
          mint does not have is unscoreable, not clean.
check 1 negative  a synthetic symbol is injected with a deliberately impossible
          count; it MUST be reported.
check 2 negative  a synthetic tag with a numeric field, not grandfathered and
          carrying NO declaration, MUST be reported.  Nothing in the live tree
          trips this branch today, so without a handed-in shape "0 undeclared"
          is indistinguishable from a dead parser
          (reference_absence_screen_must_be_handed_a_shape).
check 2 positive  a synthetic tag with a WELL-FORMED declaration MUST NOT be
          reported -- otherwise the check is simply rejecting everything.
check 3 negative  a synthetic expiry with neither field MUST be reported.
check 3 positive  a synthetic expiry with both fields MUST NOT be reported.

Usage:  python tools/unpromotable_slot_check.py [--root <repo root>]
Exit:   0 = every scoreable tag fits, and every non-grandfathered tag declares
        1 = at least one does not (listed)
        2 = a control failed, nothing reported
"""

import argparse
import os
import re
import sys

TAGS = os.path.join("standalone", "reg_mint_promoted.cpp")
INC = os.path.join("standalone", "rt_data_mint_tables.inc")

TAG_RE = re.compile(r"RT_UNPROMOTABLE:\s*(\?\S+)\s*\|\s*([^|]+?)\s*\|")
TAG_LINE_RE = re.compile(r"RT_UNPROMOTABLE:\s*(.+?)\s*$")
SCOPE_RE = re.compile(r"\[SCOPE:\s*(PERMANENT|DLL-PRESENT|CONDITIONAL)\s*\]")
SCOPE_ANY_RE = re.compile(r"\[SCOPE:")
ADDR_RE = re.compile(r"\[ADDRESSEE:\s*([^\]]+?)\s*\]")
NUM_RE = re.compile(r"^\s*([\d?]+)\s*/\s*(\d+)")
PURE_RE = re.compile(r"(\d+)\s*purecall")
CONV_RE = re.compile(r"\[CONV:\s*M=(TABLE|NONPURECALL)\s*\]")
CONV_ANY_RE = re.compile(r"\[CONV:")
ROW_RE = re.compile(r'\{\s*"(\?\?_7[^"]+)"\s*,\s*0x[0-9a-fA-F]+ull\s*,\s*(\d+)\s*,')

SYNTHETIC = ("??_7SyntheticControlRow@LaneDoors@@6B@", "0/999 (0 purecall)", 4)
# Check 2's two controls.  Neither symbol exists in the mint, so neither can
# reach check 1; they exercise check 2 and nothing else.
SYNTH_UNDECLARED = ("??_7SyntheticUndeclared@LaneClass@@6B@", "3/9 (2 purecall)")
SYNTH_DECLARED = ("??_7SyntheticDeclared@LaneClass@@6B@",
                  "3/9 (2 purecall) [CONV: M=NONPURECALL]")
# Check 3's two controls, on the EXPIRY field.
SYNTH_NOSCOPE = ("??_7SyntheticNoScope@LaneClass@@6B@", "unblocked when X lands")
SYNTH_SCOPED = ("??_7SyntheticScoped@LaneClass@@6B@",
                "unblocked when X lands [SCOPE: CONDITIONAL] "
                "[ADDRESSEE: the lane that ports X]")

# ---- THE FREEZE.  Wave 21 lane CLASS, taken from wave21/base 018426f2. ------
# (symbol, EXACT field text).  These predate the declaration requirement and
# are exempt from it under WAVE20_CLOSED §6.1.  DO NOT ADD TO THIS LIST: a tag
# written after that commit must declare its convention instead.  Editing a
# field here is permitted and is precisely what makes that tag require a
# declaration, because the freeze is on the pair.
GRANDFATHERED = {
    ("??_7DialogController@UI@@6B@", "11/11"),
    ("??_7Dialog@UI@@6B@", "9/9"),
    ("??_7Base@Object@@6B@", "2/2 (1 purecall)"),
    ("??_7External@Object@@6B@", "4/4 (4 purecall)"),
    ("??_7CompiledExternal@Object@@6B@", "4/4 (4 purecall)"),
    ("??_7Internal@Object@@6B@", "4/4 (0 purecall)"),
    ("??_7ConstantBufferData@Video@@6B@", "1/1 (3 purecall)"),
    ("??_7ContextData@Video@@6B@", "1/1 (19 purecall)"),
    ("??_7DeviceData@Video@@6B@", "1/1 (19 purecall)"),
    ("??_7ImageBufferData@Video@@6B@", "1/1 (6 purecall)"),
    ("??_7IndexBufferData@Video@@6B@", "1/1 (6 purecall)"),
    ("??_7PixelProgramData@Video@@6B@", "1/1 (2 purecall)"),
    ("??_7RenderBufferData@Video@@6B@", "1/1 (3 purecall)"),
    ("??_7RenderStateData@Video@@6B@", "1/1 (2 purecall)"),
    ("??_7VertexBufferData@Video@@6B@", "1/1 (6 purecall)"),
    ("??_7VertexLayoutData@Video@@6B@", "1/1 (2 purecall)"),
    ("??_7VertexProgramData@Video@@6B@", "1/1 (2 purecall)"),
    ("??_7VideoProgramData@Video@@6B@", "1/1 (2 purecall)"),
    ("??_7ReaderWriter@IO@@6B@",
     "?/26 ported [NUMERATOR STILL INHERITED], 25 purecall [RE-DERIVED]"),
    ("??_7Stream@IO@@6B@", "1/7 ported [INHERITED], 5 purecall [MEASURED]"),
}

# ---- THE SECOND FREEZE: expiry text, for CHECK 3. --------------------------
# Same commit, same rule, same reason as GRANDFATHERED above: the forty tags
# that existed when the SCOPE/ADDRESSEE requirement landed are exempt from it,
# and the freeze is on the (symbol, EXACT expiry text) PAIR so that editing an
# expiry -- which is exactly when its author is re-thinking it -- makes that
# tag owe the new fields.  IT MUST NEVER GROW.
#
# The distribution of these forty expiries is the measured motive for CHECK 3:
#     36  "NONE, EVER"
#      3  "NONE while the DLL is present"
#      1  a real condition ("either Dialog factory ported+registered")
# Wave 21 lane SHOT proved at least one of the thirty-six is really the second
# kind: ?_singleton@FileSystem@IO@@'s HARM is real while the DLL is loaded and
# FALSE at cutover, where the row is REQUIRED.  Two rows spelled identically
# meant different things, and nothing in the tag could tell them apart.
FREEZE_SCOPE = {
    ('??_7DialogController@UI@@6B@',
     'NONE, EVER'),
    ('??_7Dialog@UI@@6B@',
     'either Dialog factory ported+registered'),
    ('?_singleton@Application@1@0PEAV11@EA',
     'NONE, EVER'),
    ('??_7Base@Object@@6B@',
     'NONE, EVER'),
    ('??_7External@Object@@6B@',
     'NONE, EVER'),
    ('??_7CompiledExternal@Object@@6B@',
     'NONE, EVER'),
    ('?_singleton@Factory@Object@@0PEAV12@EA',
     'NONE, EVER'),
    ('?_singleton@ExternalFactory@Object@@0PEAV12@EA',
     'NONE, EVER'),
    ('?_singleton@InternalFactory@Object@@0PEAV12@EA',
     'NONE, EVER'),
    ('?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA',
     'NONE, EVER'),
    ('??_7Internal@Object@@6B@',
     'NONE while the DLL is present'),
    ('?_typeInfo@CompiledExternal@Object@@0VBaseTypeInfo@2@A',
     'NONE while the DLL is present'),
    ('?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A',
     'NONE while the DLL is present'),
    ('??_7ConstantBufferData@Video@@6B@',
     'NONE, EVER'),
    ('??_7ContextData@Video@@6B@',
     'NONE, EVER'),
    ('??_7DeviceData@Video@@6B@',
     'NONE, EVER'),
    ('??_7ImageBufferData@Video@@6B@',
     'NONE, EVER'),
    ('??_7IndexBufferData@Video@@6B@',
     'NONE, EVER'),
    ('??_7PixelProgramData@Video@@6B@',
     'NONE, EVER'),
    ('??_7RenderBufferData@Video@@6B@',
     'NONE, EVER'),
    ('??_7RenderStateData@Video@@6B@',
     'NONE, EVER'),
    ('??_7VertexBufferData@Video@@6B@',
     'NONE, EVER'),
    ('??_7VertexLayoutData@Video@@6B@',
     'NONE, EVER'),
    ('??_7VertexProgramData@Video@@6B@',
     'NONE, EVER'),
    ('??_7VideoProgramData@Video@@6B@',
     'NONE, EVER'),
    ('?_singleton@Configuration@Video@@0PEAV12@EA',
     'NONE, EVER'),
    ('?_singleton@Device@Video@@0PEAV12@EA',
     'NONE, EVER'),
    ('?_firstSurface@ImageBuffer@Video@@0PEAV12@EA',
     'NONE, EVER'),
    ('?_firstBuffer@IndexBuffer@Video@@0PEAV12@EA',
     'NONE, EVER'),
    ('?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA',
     'NONE, EVER'),
    ('?_firstBuffer@VertexBuffer@Video@@0PEAV12@EA',
     'NONE, EVER'),
    ('?_singleton@FileSystem@IO@@0PEAV12@EA',
     'NONE, EVER'),
    ('?_singleton@Modification@IO@@2PEAV12@EA',
     'NONE, EVER'),
    ('?_singleton@Persistence@IO@@0PEAV12@EA',
     'NONE, EVER'),
    ('??_7ReaderWriter@IO@@6B@',
     'NONE, EVER'),
    ('??_7Stream@IO@@6B@',
     'NONE, EVER'),
    ('?_singleton@AnimationManager@Graphics@@0PEAV12@EA',
     'NONE, EVER'),
    ('?_frameIndex@ParticleInstance@Graphics@@1HA',
     'NONE, EVER'),
    ('?_rng@Particle@Graphics@@1VRandom@System@@A',
     'NONE, EVER'),
    ('?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA',
     'NONE, EVER'),
}


def count_tags_outside(root):
    """How many tag-shaped lines live outside the source of truth.

    This tool reads ONE file.  That is a subject, and a subject has to be
    stated as a number rather than implied -- a screen that cannot name what it
    looked at inflates (reference_a_screen_that_cannot_name_its_subject).
    These lines are NOT parsed by this tool and NOT by runtime_census.py.
    """
    hits, loose, scanned = [], [], 0
    skip = {".git", ".claude", "build", "build-perf", "node_modules"}
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in skip]
        for fn in filenames:
            if not fn.endswith((".cpp", ".h", ".inc", ".md", ".py", ".txt")):
                continue
            p = os.path.join(dirpath, fn)
            if os.path.relpath(p, root).replace("\\", "/") == TAGS.replace("\\", "/"):
                continue
            try:
                text = open(p, encoding="utf-8", errors="replace").read()
            except OSError:
                continue
            scanned += 1
            for i, line in enumerate(text.splitlines(), 1):
                if TAG_RE.search(line):
                    hits.append((os.path.relpath(p, root), i))
                elif "RT_UNPROMOTABLE:" in line:
                    loose.append((os.path.relpath(p, root), i))
    return hits, loose, scanned


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    args = ap.parse_args()
    root = os.path.abspath(args.root)

    print("SUBJECT ROOT : %s" % root)
    print("TAGS         : %s   (the ONLY file whose tags are checked)" % TAGS)
    print("MINT ROWS    : %s" % INC)
    print("SCOPE 1      : the DENOMINATOR only.  The numerator (slots ported) is "
          "not derivable from the .inc and is NOT checked.")
    print("SCOPE 2      : whether a numeric tag DECLARES its convention.  This "
          "checks the DECLARATION, not whether the lane measured correctly.")
    print("SCOPE 3      : whether an expiry declares a ROUTE SCOPE and an "
          "ADDRESSEE.  PRESENCE ONLY -- this check cannot tell a correct scope")
    print("               from a wrong one, nor a relevant condition from an "
          "irrelevant one.  All four tag defects found in Wave 21 sat in tags")
    print("               that were perfectly WELL FORMED, and this check would "
          "have caught NONE of them as written.")

    tag_path, inc_path = os.path.join(root, TAGS), os.path.join(root, INC)
    for p in (tag_path, inc_path):
        if not os.path.isfile(p):
            print("CONTROL RED  : missing %s" % p)
            return 2

    rows = {}
    for m in ROW_RE.finditer(open(inc_path, encoding="utf-8", errors="replace").read()):
        rows[m.group(1)] = int(m.group(2))

    tags = []
    expiries = []            # (lineno, symbol, expiry text) -- CHECK 3's subject
    for i, line in enumerate(
            open(tag_path, encoding="utf-8", errors="replace").read().splitlines(), 1):
        m = TAG_RE.search(line)
        if m:
            tags.append((i, m.group(1), m.group(2)))
            fields = [x.strip() for x in
                      TAG_LINE_RE.search(line).group(1).split("|")]
            # symbol | slots | doc § | expiry [| class].  A tag with fewer than
            # four fields has no expiry to check and is counted separately
            # rather than silently skipped.
            expiries.append((i, m.group(1),
                             fields[3] if len(fields) >= 4 else None))
    n_real = len(tags)

    outside, loose, scanned = count_tags_outside(root)
    print("COVERAGE     : %d file(s) scanned outside %s; %d carry a full "
          "tag-shaped line" % (scanned, TAGS, len(outside)))
    for rel, ln in outside:
        print("               TAG-SHAPED (NOT parsed by this tool): %s:%d" % (rel, ln))
    # The count above is ABSENCE-shaped, so print a figure that is non-zero
    # when the walker is alive.  A bare "0 elsewhere" from a dead walk reads
    # identically to a clean tree (reference_absence_screen_must_be_handed_a_shape).
    print("               LIVENESS: %d line(s) elsewhere mention "
          "'RT_UNPROMOTABLE:' without the full tag grammar (doc quotations and"
          % len(loose))
    print("               this tool's own regex) -- a non-zero here says the "
          "walk ran.")
    for rel, ln in loose[:8]:
        print("                 %s:%d" % (rel, ln))
    if len(loose) > 8:
        print("                 ... and %d more" % (len(loose) - 8))

    # negative control for check 1: a row that must be reported
    rows[SYNTHETIC[0]] = SYNTHETIC[2]
    tags.append((0, SYNTHETIC[0], SYNTHETIC[1]))
    # check 2's two controls
    tags.append((-1, SYNTH_UNDECLARED[0], SYNTH_UNDECLARED[1]))
    tags.append((-2, SYNTH_DECLARED[0], SYNTH_DECLARED[1]))

    scoreable, bad, caught_synth = 0, [], False
    numeric = 0
    undeclared, declared, grandfathered_hit = [], 0, set()
    caught_undeclared = False
    declared_control_clean = True
    malformed_conv = []

    for lineno, sym, field in tags:
        num = NUM_RE.match(field)
        if not num:
            # "n/a (scalar ptr)" and friends: no denominator, so nothing to
            # declare and nothing to compare.  Counted, not judged.
            continue
        numeric += 1

        # ---- CHECK 2: does this tag declare its convention? ----
        conv = CONV_RE.search(field)
        if conv is None and CONV_ANY_RE.search(field):
            # A [CONV: ...] that is not one of the two agreed spellings is a
            # malformed declaration, which must NOT read as "declared".
            malformed_conv.append((lineno, sym, field))
        if conv is not None:
            declared += 1
            if lineno == -2:
                pass                      # control: must stay clean
        elif (sym, field) in GRANDFATHERED:
            grandfathered_hit.add((sym, field))
        else:
            if lineno == -1:
                caught_undeclared = True
            elif lineno == -2:
                declared_control_clean = False   # the positive control failed
            elif lineno <= 0:
                # check 1's own synthetic row (lineno 0) is not a check 2
                # subject: it exists to be STALE, and reporting it here would
                # be a control leaking into the findings.
                pass
            else:
                undeclared.append((lineno, sym, field))

        # ---- CHECK 1: the denominator ----
        mint = rows.get(sym)
        if mint is None:
            continue                      # not a minted vtable row
        scoreable += 1
        denom = int(num.group(2))
        pure = PURE_RE.search(field)
        k = int(pure.group(1)) if pure else 0
        if conv is not None:
            # A DECLARED tag is checked EXACTLY under the reading it declares.
            # This is the payoff of check 2: declaring retires check 1's
            # accept-either blind spot for that row.
            if conv.group(1) == "TABLE":
                fits, tried = denom == mint, "the DECLARED M=TABLE reading only"
            else:
                fits, tried = denom + k == mint, \
                    "the DECLARED M=NONPURECALL reading only"
        else:
            fits = denom == mint or denom + k == mint
            tried = "both undeclared readings (M and M+K)"
        if fits:
            continue
        if sym == SYNTHETIC[0]:
            caught_synth = True
            continue
        bad.append((lineno, sym, field, mint, k, tried))

    # ---- CHECK 3: does the EXPIRY declare a ROUTE SCOPE and an ADDRESSEE? ----
    # Motivated by four defects found in ONE wave (21), every one of which sat
    # in a tag that was PERFECTLY WELL FORMED -- see the module docstring.
    # This check can only enforce PRESENCE.  It cannot tell a correct scope
    # from an incorrect one, or a relevant condition from an irrelevant one.
    exp_scoped = exp_addr = exp_grandfathered = exp_noexpiry = 0
    exp_missing, exp_badscope = [], []
    caught_noscope = False
    scoped_control_clean = True
    for lineno, sym, expiry in (expiries
                                + [(-3, SYNTH_NOSCOPE[0], SYNTH_NOSCOPE[1]),
                                   (-4, SYNTH_SCOPED[0], SYNTH_SCOPED[1])]):
        if expiry is None:
            exp_noexpiry += 1
            continue
        sc = SCOPE_RE.search(expiry)
        ad = ADDR_RE.search(expiry)
        if sc is None and SCOPE_ANY_RE.search(expiry):
            exp_badscope.append((lineno, sym, expiry))
        if sc is not None:
            exp_scoped += 1
        if ad is not None:
            exp_addr += 1
        if sc is not None and ad is not None:
            if lineno == -4:
                pass                       # positive control: must stay clean
            continue
        if (sym, expiry) in FREEZE_SCOPE:
            exp_grandfathered += 1
            continue
        if lineno == -3:
            caught_noscope = True
        elif lineno == -4:
            scoped_control_clean = False
        elif lineno <= 0:
            pass
        else:
            exp_missing.append((lineno, sym, expiry,
                                sc is None, ad is None))

    pos_ok = scoreable > 1                # > 1: the synthetic is one of them
    print("CONTROL 1pos : %d tags parsed, %d of them scoreable against a minted "
          "row -> %s" % (n_real, scoreable - 1, "GREEN" if pos_ok else "RED"))
    print("CONTROL 1neg : synthetic 0/999 row reported -> %s"
          % ("GREEN" if caught_synth else "RED"))
    print("CONTROL 2neg : synthetic UNDECLARED numeric tag reported -> %s"
          % ("GREEN" if caught_undeclared else "RED"))
    print("CONTROL 2pos : synthetic WELL-FORMED declaration NOT reported -> %s"
          % ("GREEN" if declared_control_clean else "RED"))
    print("CONTROL 3neg : synthetic expiry with no SCOPE/ADDRESSEE reported -> %s"
          % ("GREEN" if caught_noscope else "RED"))
    print("CONTROL 3pos : synthetic expiry WITH both NOT reported -> %s"
          % ("GREEN" if scoped_control_clean else "RED"))
    if not (pos_ok and caught_synth and caught_undeclared and declared_control_clean
            and caught_noscope and scoped_control_clean):
        print("*** MEASURED NOTHING: a control failed.  No verdict. ***")
        return 2

    print("FREEZE       : %d grandfathered (symbol, field) pairs, %d matched by a "
          "live tag" % (len(GRANDFATHERED), len(grandfathered_hit)))
    stale_freeze = sorted(set(GRANDFATHERED) - grandfathered_hit)
    # WAVE 23 lane IOPORT.  A freeze pair stops matching for TWO OPPOSITE
    # reasons and the old message named only one of them:
    #
    #   DISCHARGED  the field was edited ON PURPOSE -- a lane re-derived an
    #               inherited numerator -- and the symbol still carries a live
    #               tag that DECLARES its convention.  That is the freeze
    #               working exactly as its own comment says it should ("editing
    #               a field here is permitted and is precisely what makes that
    #               tag require a declaration").  It is GOOD NEWS.
    #   VANISHED    the symbol has no live tag at all, or its live tag declares
    #               nothing.  That is a tag that was deleted or silently edited,
    #               and it is the case worth looking at.
    #
    # Printing both as "UNMATCHED (tag deleted or its field edited)" made a
    # correct discharge indistinguishable from a silent deletion -- the reader
    # has to go read the source to tell which, and the line reads as an open
    # defect either way.  Wave 21 lane ENC discharged ??_7ReaderWriter@IO@@6B@
    # and the resulting line was still being briefed TWO WAVES LATER as "an
    # inherited `?` numerator, a known open item".  It was neither.
    live_declared = {sym for _, sym, field in tags
                     if CONV_RE.search(field) is not None}
    discharged = [(sym, f) for sym, f in stale_freeze if sym in live_declared]
    vanished   = [(sym, f) for sym, f in stale_freeze if sym not in live_declared]
    for sym, field in discharged:
        print("               DISCHARGED (field re-derived; live tag declares a "
              "convention): %s\n                 was: %s" % (sym, field))
    for sym, field in vanished:
        print("               VANISHED (no live tag declares this symbol -- "
              "deleted or silently edited): %s | %s" % (sym, field))
    # WAVE 23 lane IOPORT -- a control for the DISCHARGED/VANISHED split above.
    # BOTH arms are scored separately, because a split that only ever takes one
    # branch is not a split and a union score would hide the dead one
    # (reference_union_control_hides_a_blind_screen.md).
    #   4pos  a symbol that DOES carry a live declared tag must route to
    #         DISCHARGED.  Driven by the real freeze pairs this run.
    #   4neg  a symbol that carries NO live tag must route to VANISHED.  Driven
    #         by a synthetic name that is not in the file, so this arm is live
    #         even in a run where nothing real has vanished -- a screen looking
    #         for an absence has to be HANDED a shape
    #         (reference_absence_screen_must_be_handed_a_shape.md).
    _synth_absent = "??_7NeverWasATag@LaneIoport@@6B@"
    _pos_ok = all(sym in live_declared for sym, _ in discharged)
    _neg_ok = _synth_absent not in live_declared
    print("CONTROL 4pos : %d freeze pair(s) routed to DISCHARGED, all with a live "
          "declared tag -> %s" % (len(discharged), "GREEN" if _pos_ok else "RED"))
    print("CONTROL 4neg : synthetic never-declared symbol would route to VANISHED "
          "-> %s" % ("GREEN" if _neg_ok else "RED"))
    if not (_pos_ok and _neg_ok):
        print("*** the DISCHARGED/VANISHED split failed its own control. ***")
        return 2

    for lineno, sym, field, mint, k, tried in bad:
        print("STALE  %s:%d  %s\n         tag says %r; the mint row is %d slots. "
              "Tried: %s (K=%d)."
              % (TAGS, lineno, sym, field, mint, tried, k))
    for lineno, sym, field in malformed_conv:
        print("BADCONV %s:%d  %s\n         %r carries a [CONV: ...] that is "
              "neither M=TABLE nor M=NONPURECALL" % (TAGS, lineno, sym, field))
    for lineno, sym, field in undeclared:
        print("UNDECLARED %s:%d  %s\n         %r has a numeric denominator, is "
              "not in the freeze, and declares no convention.\n"
              "         Add [CONV: M=TABLE] or [CONV: M=NONPURECALL] to the "
              "slots field (WAVE20_CLOSED §6.1)." % (TAGS, lineno, sym, field))

    print("==== CHECK 1: %d of %d scoreable tags fit, %d STALE ===="
          % (scoreable - 1 - len(bad), scoreable - 1, len(bad)))
    print("==== CHECK 2: %d numeric tags -- %d declared, %d grandfathered, "
          "%d UNDECLARED, %d malformed ===="
          % (numeric - 3, declared - 1, len(grandfathered_hit), len(undeclared),
             len(malformed_conv)))

    for lineno, sym, expiry in exp_badscope:
        print("BADSCOPE %s:%d  %s\n         %r carries a [SCOPE: ...] that is "
              "none of PERMANENT / DLL-PRESENT / CONDITIONAL"
              % (TAGS, lineno, sym, expiry))
    for lineno, sym, expiry, no_sc, no_ad in exp_missing:
        want = " and ".join([w for w, m in (("[SCOPE: ...]", no_sc),
                                            ("[ADDRESSEE: ...]", no_ad)) if m])
        print("NOSCOPE %s:%d  %s\n         expiry %r is missing %s"
              % (TAGS, lineno, sym, expiry, want))
    print("==== CHECK 3: %d tags with an expiry -- %d scoped, %d addressed, "
          "%d grandfathered, %d MISSING, %d malformed, %d with no expiry field ===="
          % (len(expiries), exp_scoped - 1, exp_addr - 1, exp_grandfathered,
             len(exp_missing), len(exp_badscope), exp_noexpiry))
    if exp_grandfathered:
        print("     NOTE: all %d grandfathered expiries declare NEITHER a route "
              "scope NOR an" % exp_grandfathered)
        print("     addressee.  They do not FAIL this check (WAVE20_CLOSED §6.1 "
              "forbids rewriting")
        print("     another lane's verdict), but that is a backlog, not a clean "
              "bill.  Wave 21 lane")
        print("     SHOT proved ?_singleton@FileSystem@IO@@'s 'NONE, EVER' is "
              "really DLL-PRESENT.")
    return 1 if (bad or undeclared or malformed_conv
                 or exp_missing or exp_badscope) else 0


if __name__ == "__main__":
    sys.exit(main())
