#!/usr/bin/env python3
"""Fail if a STANDING NOTE has been deleted from the tree.

WHY THIS EXISTS
---------------
Some notes in this project are not commentary: they are the record of a
question that was ASKED, ANSWERED and must not be re-opened on the same guess.
They survive only as prose, and prose is what a cleanup pass deletes first.
Wave 25's kickoff asked for exactly one of them to be made undeletable --
`Graphics::AnimationManager::Serialize` must STAY UNVERIFIED against
`Animation.h +0x10` -- and a rule that lives only in a wave document is a rule
that expires with the wave.

SUBJECT (state what you examined -- instrument rule 5):
    the files named in NOTES below, read from disk, in full, as raw text.
    Nothing else in the tree is examined, and this checker makes no claim about
    any note it does not list.

WHAT IT CANNOT DO: it checks that the TEXT is present, not that the text is
still TRUE. A note can be preserved word-for-word and be stale -- which is the
defect class this project has paid for most often. Every entry therefore names
the measurement that re-derives its claim, printed with --why.

USAGE
    python tools/check_standing_notes.py            # verdict, exit 1 on loss
    python tools/check_standing_notes.py --why      # + the re-derivation of each
    python tools/check_standing_notes.py --selftest # controls, both directions
"""
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Each entry: (id, path, required substrings (ALL must be present), why, re-derive)
NOTES = [
    (
        "anim-f10-unverified",
        "runtime/types/graphics/Animation.h",
        [
            "UNVERIFIED",
            "WAVE 24 lane GFXMAT ported",
            "ANSWERED R4",
        ],
        "AnimationManager +0x10 must stay UNVERIFIED. Wave 23 GFXBODY's R4 named "
        "?Serialize@AnimationManager@ as the body that would settle it; Wave 24 "
        "GFXMAT ported that body and the answer was NO -- it reads only +0x00 and "
        "+0x08. Deleting the note loses the answer and the next lane re-opens it.",
        "read RtGraphics_AnimationManager_Serialize in "
        "runtime/src/graphics/wave24_lane_gfxmat.cpp: the only self-relative reads "
        "are *(void**)self (+0x00) and *(int32*)(self+8).",
    ),
    (
        "anim-f10-field-marker",
        "runtime/types/graphics/Animation.h",
        ["_f10;             // +0x10  UNVERIFIED"],
        "The struct field itself carries the marker; a reader who never scrolls to "
        "the header comment still sees it.",
        "grep -n '_f10' runtime/types/graphics/Animation.h",
    ),
]


def check(notes, read=lambda p: open(os.path.join(REPO, p), encoding="utf-8",
                                     errors="replace").read()):
    """Returns (missing, examined_files, examined_bytes, rules_evaluated)."""
    missing = []
    files = {}
    rules = 0
    for (nid, path, needles, why, rederive) in notes:
        try:
            text = files[path]
        except KeyError:
            try:
                text = files[path] = read(path)
            except OSError as e:
                missing.append((nid, path, "FILE MISSING: %s" % e))
                continue
        for needle in needles:
            rules += 1
            if needle not in text:
                missing.append((nid, path, "lost: %r" % needle))
    return missing, len(files), sum(len(t) for t in files.values()), rules


def selftest():
    """Controls, BOTH directions, on a subject the checker actually compares.

    POSITIVE: the real files, unmodified -- every rule must pass. This is the
      "handed a positive shape" half: a screen looking for an ABSENCE has no
      natural positive case, so 0 findings is otherwise indistinguishable from
      a dead reader (an exception swallowed, a path typo, an empty string).
    NEGATIVE: one needle deleted from the text the checker reads, one rule at a
      time -- each deletion MUST produce exactly one finding. A control that
      corrupts something the screen does not examine would prove nothing.
    """
    ok = True
    miss, nfiles, nbytes, nrules = check(NOTES)
    print("POSITIVE control: %d files, %d bytes, %d rules -> %d findings (want 0)"
          % (nfiles, nbytes, nrules, len(miss)))
    if miss:
        ok = False
        for m in miss:
            print("   unexpected finding:", m)

    caught = 0
    total = 0
    for i, (nid, path, needles, why, rederive) in enumerate(NOTES):
        for needle in needles:
            total += 1
            def read(p, _path=path, _needle=needle):
                t = open(os.path.join(REPO, p), encoding="utf-8", errors="replace").read()
                return t.replace(_needle, "") if p == _path else t
            m, _, _, _ = check(NOTES, read=read)
            # The deletion may trip sibling rules that share the needle's text;
            # what matters is that THIS rule fired.
            fired = any(nid == f[0] and needle in f[2] for f in m)
            caught += 1 if fired else 0
            if not fired:
                ok = False
                print("   NOT CAUGHT: deleting %r from %s produced no finding"
                      % (needle, path))
    print("NEGATIVE controls: %d/%d deletions caught (effectiveness counter)"
          % (caught, total))
    print("SELFTEST:", "ALIVE" if ok else "*** BROKEN -- its silence means nothing ***")
    return 0 if ok else 1


def main():
    if "--selftest" in sys.argv:
        return selftest()
    miss, nfiles, nbytes, nrules = check(NOTES)
    print("check_standing_notes: examined %d files (%d bytes), evaluated %d rules "
          "over %d notes" % (nfiles, nbytes, nrules, len(NOTES)))
    if "--why" in sys.argv:
        for (nid, path, needles, why, rederive) in NOTES:
            print("\n[%s] %s" % (nid, path))
            print("   WHY:      %s" % why)
            print("   REDERIVE: %s" % rederive)
    if miss:
        print("\n*** %d STANDING NOTE(S) LOST ***" % len(miss))
        for (nid, path, what) in miss:
            print("   %-28s %s  %s" % (nid, path, what))
        print("\nRestore the text, or -- if the note is genuinely obsolete -- delete its "
              "entry HERE in the same commit, with the measurement that retired it.")
        return 1
    print("all standing notes present")
    return 0


if __name__ == "__main__":
    sys.exit(main())
