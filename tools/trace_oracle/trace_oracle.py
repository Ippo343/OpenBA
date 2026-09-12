#!/usr/bin/env python3
"""trace_oracle — a draw-call / pipeline-state verdict engine.

WHY THIS EXISTS
---------------
Wave 23 measured that `BANISHED_SEED` + `BANISHED_CAMERA_PIN` do NOT give a
reproducible frame: five runs at identical seed, pin and command line split into
two clusters ~98% apart, and the split was independent of the flag under test.
**A pixel diff between two runs of this game measures nothing.**

This tool compares the *command stream* instead: what pipeline configurations the
renderer built, which shaders and states and views it bound, per draw. It needs no
frame determinism, because every screen it runs is either set-based (order- and
count-free) or explicitly floored against a same-build control pair.

It consumes the JSONL produced by `tools/dx11_state_diff/dump_state.py`, which
already canonicalises every resource reference to a content-derived key (shaders
by sha1 of DXBC, textures by desc + mip-0 hash, buffers by length) so records are
comparable across two processes.

WHAT IT EXAMINES — stated, not implied
--------------------------------------
Every run prints an EXAMINED census before any verdict: how many draws, how many
fields, and for each field how many records carried a non-empty value. A field
that is empty in 100% of records is reported NOT-EXAMINED rather than silently
counted as "agreeing" — a screen looking for an absence must be handed a positive
shape, or "0 differences" is indistinguishable from a dead parser.

SIX SCREENS, SCORED SEPARATELY
-------------------------------
  S1 configSet    set of full pipeline configurations (alignment-free)
  S2 sigSet       ditto with bound-resource identity removed
  S3 fieldDiff    aligned per-field disagreement rate, floored by the control
  S4 invariants   texture-derived quantities checked against the bound texture
  S5 srvPool      the set of textures ever bound as an SRV
  S6 strideSet    the set of vertex/index buffer strides (offsets excluded)

S6 exists because this tool's own control harness found the hole. The positive
control `vb.stride` (+4 on one vertex buffer) was caught by S3 ALONE, and S3 is
suppressed without a control pair -- so without S6, a lane with one capture per
arm could not see a vertex stride difference at all. `vb`/`ib` are CONTENT fields
in compare_state (a dynamic buffer's identity is allocation order, not
configuration), but the STRIDE and OFFSET inside them are configuration. S6 takes
exactly those, as a set, so it needs no alignment and no control pair.

They are never merged into one number. A union score hides a dead screen: a
control that is caught by S1 says nothing about whether S3 works.

THE CONTROL PAIR IS NOT OPTIONAL FOR S3 -- S3 IS SUPPRESSED WITHOUT ONE
-----------------------------------------------------------------------
Two runs draw different numbers of things by themselves. Pass `--control A A2`
(two traces from the SAME build) and S3 reports only fields the control proves
stable.

Without `--control`, S3 emits NO findings at all. That is not caution: this
tool's own control harness measured it. `trace_selftest`'s negative control
`fewer draws` -- a trace that drew fewer of the SAME configurations, which is
precisely the non-determinism Wave 23 measured -- moved 6159 field values, and
unfloored S3 flagged it while S1/S2/S5 stayed silent. Its rates are still
printed, tagged `suppressed`, because they are useful to look at and are not
allowed to be evidence.

MEASURED NOISE FLOOR -- READ THIS BEFORE QUOTING ANY SCREEN
-----------------------------------------------------------
Three captures, same exe (md5 5a909b6df845cf00db7b3237af54acc6), same seed
12345, same pin 0,0,60, same command line, same frame 150. They drew 566 / 557 /
509 draws. Between two IDENTICAL runs, each screen reported:

  screen          unfloored            floored by one control pair
  S1 configSet    41-74 findings       51          <- NOISE
  S2 sigSet        3-6  findings        3          <- NOISE
  S3 fieldDiff    (suppressed)          0 (after the NON_VERDICT calibration)
  S4 invariants     0                   0          <- verdict-bearing
  S5 srvPool      21-43 findings       29          <- NOISE
  S6 strideSet      0                   0          <- verdict-bearing

**S4 and S6 are the only screens with a measured zero floor.** S1, S2 and S5 are
descriptive: which materials and textures drew this frame follows what was on
screen, and a single control pair does not floor them, because the SET of
run-dependent keys is itself run-dependent (flooring S1 by one control pair took
74 to 51, not to 0). Their counts are still printed and are still worth reading
-- a port that deletes a whole render pass will move them far outside this band
-- but a difference of a few dozen keys is the game, not the port.

Also measured: compare_state.explain_signature_gap()'s content classifier -- "the
shader pair never drew on the other side" -- explained **0 of the 3** one-sided
S2 signatures between identical runs. That classifier is not a floor here.

Usage:
    trace_oracle.py --test ours.jsonl ref.jsonl [--control a.jsonl a2.jsonl]
                    [--label-test-a ours --label-test-b shipped]
                    [--json out.json] [--quiet]
"""

from __future__ import annotations

import argparse
import difflib
import importlib.util
import json
import os
import sys
from collections import Counter, defaultdict

_HERE = os.path.dirname(os.path.abspath(__file__))
_CS_PATH = os.path.join(_HERE, os.pardir, "dx11_state_diff", "compare_state.py")


def _load_compare_state():
    """Single source of truth for the record schema lives in compare_state.py."""
    spec = importlib.util.spec_from_file_location("_cs", os.path.abspath(_CS_PATH))
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot load {_CS_PATH}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


CS = _load_compare_state()

# Fields the oracle compares in S3. Derived from compare_state.all_fields(), minus
# the ones that are per-draw payload of a non-deterministic simulation and cannot
# carry a verdict. They are still RECORDED and still printed, just never promoted
# to a finding: see NON_VERDICT below.
NON_VERDICT = {
    "cbContents",    # per-object transforms genuinely differ between runs
    "draw",          # index/instance counts follow what is on screen
    "name",          # RenderDoc's action label carries the EID
    "flags",
    # The two below were MEASURED unstable, not assumed. Three same-build captures
    # (ctrlA/B/C: identical seed 12345, pin 0,0,60, command line, exe md5
    # 5a909b6df845cf00db7b3237af54acc6) gave, on the aligned per-field table:
    "cbBufferSizes",  # 30.9-37.4% between IDENTICAL runs. This is a dynamic
                      # constant-buffer ring: the byte length of the allocation a
                      # draw landed in is not a renderer decision. It settles
                      # trace_selftest's `cb buffer size` OPEN control as NEGATIVE.
    "vbStrides",      # 11.1-15.2% between IDENTICAL runs -- because
                      # compare_state.strides_only() bundles the byte OFFSET in
                      # with the stride, and the offset is ring allocation. The
                      # stride ALONE is still verdict-bearing: that is S6, whose
                      # measured floor on all three pairs is exactly 0.
}


# ── loading ──────────────────────────────────────────────────────────────────

def load(path):
    meta, recs, errs = {}, [], []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            d = json.loads(line)
            if d.get("_meta"):
                meta = d
            elif "error" in d:
                errs.append(d)
            else:
                recs.append(d)
    return meta, recs, errs


def is_empty(v) -> bool:
    return v is None or v == "" or v == "-" or v == [] or v == {}


# ── EXAMINED census — the anti-dead-parser screen ────────────────────────────

def examined_census(recs, label):
    """Per-field: how many records carried a non-empty value.

    A field at 0/N is NOT EXAMINED. Reporting it as "0 differences" would be a
    dead parser reading as a clean verdict.
    """
    fields = Counter()
    nonempty = Counter()
    for r in recs:
        fa = CS.all_fields(r)
        for k, v in fa.items():
            fields[k] += 1
            if not is_empty(v):
                nonempty[k] += 1
    return {
        "label": label,
        "draws": len(recs),
        "fields": sorted(fields),
        "populated": {k: nonempty[k] for k in fields},
        "total": {k: fields[k] for k in fields},
    }


def dead_fields(cen) -> list:
    return sorted(k for k in cen["fields"] if cen["populated"].get(k, 0) == 0)


# ── S1/S2/S5 set screens ─────────────────────────────────────────────────────

def set_screen(recs_a, recs_b, key_fn):
    ca = Counter(key_fn(r) for r in recs_a)
    cb = Counter(key_fn(r) for r in recs_b)
    return {
        "distinctA": len(ca), "distinctB": len(cb),
        "shared": len(set(ca) & set(cb)),
        "onlyA": sorted(set(ca) - set(cb)),
        "onlyB": sorted(set(cb) - set(ca)),
        "countsA": ca, "countsB": cb,
    }


def srv_pool(recs):
    pool = Counter()
    for r in recs:
        for e in (r.get("srv") or []):
            if len(e) >= 4:
                pool[e[3]] += 1
    return pool


def stride_key(rec) -> str:
    """Vertex/index buffer STRIDES, with identity AND byte offsets removed.

    A stride is the vertex format the renderer chose: configuration. A byte
    OFFSET is where in a dynamic ring the draw's data happened to land, which
    depends on how much was drawn before it: content. compare_state's
    strides_only() keeps both, which is right for its floored per-field table and
    wrong for an unfloored set screen -- so this key drops the offsets.

    That split was not assumed. trace_selftest carries `vb.stride` (positive,
    must be caught) beside `buffer byte offsets` (negative, must not be flagged),
    and S6 is scored on both.
    """
    ib = rec.get("ib") or ["-", 0, 0]
    vb = rec.get("vb") or []
    return CS.canon({
        "ibStride": ib[1] if len(ib) > 1 else 0,
        "vbStrides": sorted([[e[0], e[2]] for e in vb if len(e) >= 3]),
    })


def srv_screen(recs_a, recs_b):
    pa, pb = srv_pool(recs_a), srv_pool(recs_b)
    return {
        "distinctA": len(pa), "distinctB": len(pb),
        "shared": len(set(pa) & set(pb)),
        "onlyA": sorted(set(pa) - set(pb)),
        "onlyB": sorted(set(pb) - set(pa)),
    }


# ── S3 aligned per-field diff ────────────────────────────────────────────────

def field_diff(recs_a, recs_b):
    """Align on config hash, then diff every field of every matched pair.

    Returns (rates, pairs, changedFields) where `changedFields` is the
    EFFECTIVENESS COUNTER: how many (pair, field) comparisons actually differed.
    A screen whose effectiveness counter is 0 on a control that should differ is
    dead, not clean.
    """
    ha = [CS.config_hash(r) for r in recs_a]
    hb = [CS.config_hash(r) for r in recs_b]
    sm = difflib.SequenceMatcher(a=ha, b=hb, autojunk=False)
    pairs = []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            for k in range(i2 - i1):
                pairs.append((recs_a[i1 + k], recs_b[j1 + k]))

    diffs = Counter()
    compared = Counter()
    examples = defaultdict(list)
    for ra, rb in pairs:
        fa, fb = CS.all_fields(ra), CS.all_fields(rb)
        for k in sorted(set(fa) | set(fb)):
            va, vb = CS.canon(fa.get(k)), CS.canon(fb.get(k))
            compared[k] += 1
            if va != vb:
                diffs[k] += 1
                if len(examples[k]) < 2:
                    examples[k].append((ra.get("eid"), rb.get("eid"), va[:240], vb[:240]))
    return {
        "pairs": len(pairs),
        "unalignedA": len(recs_a) - len(pairs),
        "unalignedB": len(recs_b) - len(pairs),
        "diffs": diffs,
        "compared": compared,
        "examples": examples,
        "changedFields": sum(diffs.values()),
        "comparisons": sum(compared.values()),
    }


def rate(d, k):
    c = d["compared"].get(k, 0)
    return (100.0 * d["diffs"].get(k, 0) / c) if c else None


# ── S4 texture-derived invariants ────────────────────────────────────────────

def invariants(recs):
    """maxLOD == mips-1, srv.numMips == mips, srv.numSlices == arraySize.

    These are the three quantities S2's signature deliberately excludes because
    D3D11 derives them from the bound texture. Checking them here is a STRONGER
    claim than leaving them in the signature would be: not "both sides used 7"
    but "both sides derived it from the texture by the same rule, every time".
    """
    out = {"maxLOD": [0, 0], "numMips": [0, 0], "numSlices": [0, 0]}
    for r in recs:
        srv_by_slot = {(e[0], e[1]): e[3] for e in (r.get("srv") or []) if len(e) >= 4}
        for e in (r.get("srv") or []):
            tex = e[3] or ""
            if len(e) < 10 or ":m" not in tex or ":a" not in tex:
                continue
            try:
                mips = int(tex.split(":m")[1].split(":")[0])
                slices = int(tex.split(":a")[1].split(":")[0])
            except (ValueError, IndexError):
                continue
            out["numMips"][0] += 1
            if e[6] != mips:
                out["numMips"][1] += 1
            out["numSlices"][0] += 1
            if e[8] != slices:
                out["numSlices"][1] += 1
        for s in (r.get("sampler") or []):
            if len(s) < 12:
                continue
            tex = srv_by_slot.get((s[0], s[1]))
            if not tex or ":m" not in tex:
                continue
            try:
                mips = int(tex.split(":m")[1].split(":")[0])
            except (ValueError, IndexError):
                continue
            out["maxLOD"][0] += 1
            try:
                if abs(float(s[11]) - (mips - 1)) > 1e-3:
                    out["maxLOD"][1] += 1
            except (TypeError, ValueError):
                out["maxLOD"][1] += 1
    return out


# ── the verdict ──────────────────────────────────────────────────────────────

def run_screens(recs_a, recs_b):
    return {
        "S1_configSet": set_screen(recs_a, recs_b, CS.config_hash),
        "S2_sigSet": set_screen(recs_a, recs_b, CS.pipeline_sig),
        "S3_fieldDiff": field_diff(recs_a, recs_b),
        "S4_invariants": {"A": invariants(recs_a), "B": invariants(recs_b)},
        "S5_srvPool": srv_screen(recs_a, recs_b),
        "S6_strideSet": set_screen(recs_a, recs_b, stride_key),
    }


SET_SCREENS = ("S1_configSet", "S2_sigSet", "S5_srvPool", "S6_strideSet")


def set_floor(control, name):
    """Keys that appeared on ONE side only between two runs of the SAME build.

    A key that is run-dependent in the control cannot carry a verdict in the
    test. This is NOT a theoretical precaution: measured on three same-build
    captures (ctrlA/B/C, identical seed, pin and command line, same exe md5),
    the one-sided counts between IDENTICAL runs were

        S1 configSet  25-39 per side      S5 srvPool  10-22 per side
        S2 sigSet      0-3  per side      S6 strideSet     0 per side

    and compare_state's existing "the shader pair never drew on the other side"
    content classifier explained ZERO of the S2 residue. So S1, S2 and S5 have a
    real noise floor and are not verdict-bearing unfloored. S4 and S6 measured a
    floor of exactly zero on all three pairs.
    """
    if control is None:
        return None
    sc = control[name]
    return set(sc["onlyA"]) | set(sc["onlyB"])


def screen_findings(sc, control=None, margin=0.0, s3_unfloored=False):
    """Per-screen findings. Never unioned — each screen keeps its own verdict."""
    f = {}

    for name in SET_SCREENS:
        floor = set_floor(control, name)
        one_sided = list(sc[name]["onlyA"]) + list(sc[name]["onlyB"])
        if floor is not None:
            one_sided = [k for k in one_sided if k not in floor]
        f[name] = len(one_sided)
        sc[name]["residue"] = one_sided
        sc[name]["floored"] = floor is not None

    inv = 0
    for side in ("A", "B"):
        for _, (tot, bad) in sc["S4_invariants"][side].items():
            inv += bad
    f["S4_invariants"] = inv

    d = sc["S3_fieldDiff"]
    flagged = []
    # S3 IS SUPPRESSED WITHOUT A CONTROL PAIR, and that is a MEASURED decision.
    # trace_selftest's negative control `fewer draws` -- a trace that merely drew
    # fewer of the SAME configurations, which is exactly the non-determinism
    # Wave 23 measured -- moved 6159 field values, and unfloored S3 flagged it
    # while the set screens S1/S2/S5 stayed silent. An unfloored S3 is therefore a
    # false-positive generator on the one input shape this project is guaranteed
    # to hand it. `s3_unfloored` is for trace_selftest alone, whose floor is an
    # exact copy of the trace and is provably zero.
    if control is None and not s3_unfloored:
        f["S3_fieldDiff"] = None
        return f, []
    for k in sorted(d["compared"]):
        if k in NON_VERDICT:
            continue
        rt = rate(d, k)
        if rt is None:
            continue
        floor = 0.0
        if control is not None:
            rc = rate(control["S3_fieldDiff"], k)
            floor = rc if rc is not None else 0.0
        if rt > floor + margin:
            flagged.append((k, rt, floor, d["diffs"][k], d["compared"][k]))
    f["S3_fieldDiff"] = len(flagged)
    return f, flagged


def report(sc, la, lb, cen_a, cen_b, control, margin, quiet=False):
    p = (lambda *a: None) if quiet else print

    p(f"=== trace_oracle:  {la}  vs  {lb} ===")

    # ── EXAMINED (rule: state what the instrument examined) ──────────────────
    p("\n-- EXAMINED --")
    for cen in (cen_a, cen_b):
        dead = dead_fields(cen)
        p(f"  {cen['label']}: {cen['draws']} draws, {len(cen['fields'])} fields"
          + (f"   NOT-EXAMINED (0/{cen['draws']} populated): {', '.join(dead)}" if dead else
             "   every field populated in >=1 draw"))
    if cen_a["draws"] == 0 or cen_b["draws"] == 0:
        p("  !! A side has ZERO draws. No verdict is possible; this is a dead trace,")
        p("     not a clean one. Aborting before any screen reports.")
        return None, None, True

    d = sc["S3_fieldDiff"]
    p(f"  aligned pairs: {d['pairs']}  (unaligned {d['unalignedA']} in {la}, "
      f"{d['unalignedB']} in {lb})")
    p(f"  EFFECTIVENESS: {d['comparisons']} field comparisons made, "
      f"{d['changedFields']} differed")
    if d["pairs"] == 0:
        p("  !! ZERO aligned pairs: S3 examined nothing. Its silence is not evidence.")

    # ── screens ─────────────────────────────────────────────────────────────
    p("\n-- S1 configSet (full pipeline configuration, alignment-free) --")
    s = sc["S1_configSet"]
    p(f"  distinct {s['distinctA']} vs {s['distinctB']}, shared {s['shared']}, "
      f"only-{la} {len(s['onlyA'])}, only-{lb} {len(s['onlyB'])}")

    p("\n-- S2 sigSet (bound-resource identity removed) --")
    s = sc["S2_sigSet"]
    p(f"  distinct {s['distinctA']} vs {s['distinctB']}, shared {s['shared']}, "
      f"only-{la} {len(s['onlyA'])}, only-{lb} {len(s['onlyB'])}")

    p("\n-- S4 texture-derived invariants --")
    for side, lab in (("A", la), ("B", lb)):
        parts = []
        for name, (tot, bad) in sc["S4_invariants"][side].items():
            parts.append(f"{name} {tot - bad}/{tot}" + (f" ({bad} EXCEPTIONS)" if bad else ""))
        p(f"  {lab}: " + ", ".join(parts))

    p("\n-- S5 srvPool (textures ever bound as an SRV) --")
    s = sc["S5_srvPool"]
    p(f"  distinct {s['distinctA']} vs {s['distinctB']}, shared {s['shared']}, "
      f"only-{la} {len(s['onlyA'])}, only-{lb} {len(s['onlyB'])}")

    p("\n-- S6 strideSet (vb/ib strides; identity AND byte offsets removed) --")
    s = sc["S6_strideSet"]
    p(f"  distinct {s['distinctA']} vs {s['distinctB']}, shared {s['shared']}, "
      f"only-{la} {len(s['onlyA'])}, only-{lb} {len(s['onlyB'])}")

    findings, flagged = screen_findings(sc, control, margin)

    p("\n-- S3 fieldDiff " + ("(FLOORED by control)" if control else "(SUPPRESSED: no --control pair; rates below are "
      "information only and CANNOT become findings)") + " --")
    p(f"  {'field':<16}{'test':>10}{'floor':>10}   verdict")
    for k in sorted(d["compared"]):
        rt = rate(d, k)
        if rt is None:
            continue
        rc = rate(control["S3_fieldDiff"], k) if control else None
        tag = "non-verdict" if k in NON_VERDICT else ""
        if not tag:
            if control is None:
                tag = "suppressed" if rt > 0 else "ok"
            else:
                floor = rc if rc is not None else 0.0
                tag = "FINDING" if rt > floor + margin else "ok"
        p(f"  {k:<16}{rt:>9.1f}%" + (f"{rc:>9.1f}%" if rc is not None else f"{'-':>10}")
          + f"   {tag}")

    p("\n-- PER-SCREEN VERDICT (never unioned) --")
    if control is None:
        p("  UNFLOORED. Measured on three same-build captures, the one-sided counts")
        p("  between IDENTICAL runs were S1 25-39, S2 0-3, S5 10-22, S6 0 per side.")
        p("  S1, S2 and S5 numbers below are therefore NOT findings without --control.")
    for k in ("S1_configSet", "S2_sigSet", "S3_fieldDiff", "S4_invariants",
              "S5_srvPool", "S6_strideSet"):
        v = findings[k]
        p(f"  {k:<16} findings=" + ("SUPPRESSED (needs --control)" if v is None else str(v)))
    if flagged:
        p("\n  S3 findings:")
        for k, rt, floor, nd, nc in flagged:
            p(f"    {k}: {nd}/{nc} = {rt:.1f}% (floor {floor:.1f}%)")
            for ea, eb, va, vb in d["examples"].get(k, [])[:1]:
                p(f"      eid {ea} vs {eb}")
                p(f"        {la}: {va}")
                p(f"        {lb}: {vb}")
    return findings, flagged, False


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--test", nargs=2, required=True, metavar=("A", "B"))
    ap.add_argument("--control", nargs=2, metavar=("A", "A2"),
                    help="two traces from the SAME build: the noise floor for S3")
    ap.add_argument("--label-test-a", default=None)
    ap.add_argument("--label-test-b", default=None)
    ap.add_argument("--margin", type=float, default=0.0,
                    help="a test field must beat the control floor by this many "
                         "percentage points to be a finding")
    ap.add_argument("--json", dest="json_out", default=None)
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    _, ra, ea = load(args.test[0])
    _, rb, eb = load(args.test[1])
    la = args.label_test_a or os.path.basename(args.test[0])
    lb = args.label_test_b or os.path.basename(args.test[1])
    if ea or eb:
        print(f"note: {len(ea)}/{len(eb)} records were extraction errors and are excluded",
              file=sys.stderr)

    control = None
    if args.control:
        _, ca, _ = load(args.control[0])
        _, cbr, _ = load(args.control[1])
        control = run_screens(ca, cbr)

    sc = run_screens(ra, rb)
    findings, flagged, dead = report(sc, la, lb,
                                     examined_census(ra, la), examined_census(rb, lb),
                                     control, args.margin, args.quiet)

    if args.json_out:
        out = {
            "testA": la, "testB": lb,
            "draws": [len(ra), len(rb)],
            "dead": bool(dead),
            "floored": control is not None,
            "findings": findings,
            "s3": {
                "pairs": sc["S3_fieldDiff"]["pairs"],
                "comparisons": sc["S3_fieldDiff"]["comparisons"],
                "changedFields": sc["S3_fieldDiff"]["changedFields"],
                "rates": {k: rate(sc["S3_fieldDiff"], k)
                          for k in sc["S3_fieldDiff"]["compared"]},
            },
            "flagged": [list(f) for f in (flagged or [])],
        }
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump(out, fh, indent=1)
        if not args.quiet:
            print(f"\nwrote {args.json_out}")

    if dead:
        return 3
    return 1 if findings and any(v for v in findings.values() if v) else 0


if __name__ == "__main__":
    sys.exit(main())
