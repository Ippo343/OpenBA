#!/usr/bin/env python3
"""Summarise a stress run's machine_load.csv into a verdict a later reader can use.

WHY THIS EXISTS
---------------
For much of Wave 25, up to seven lanes were compiling and running Python on this
machine at once. A perf number taken under unknown contention is not comparable
to one taken under different unknown contention -- and nothing recorded the
contention, so the comparison could neither be made nor refused. It was simply
unknown, and unknown reads like fine.

WHAT THIS DOES AND DOES NOT CLAIM
---------------------------------
It does NOT make two runs comparable, and it does not correct anything. It makes
the question ARGUABLE: a later reader can see that run A had four compiler
processes alive throughout and run B had none, and decide what to do about it.

Every verdict below states WHAT IT EXAMINED -- sample count, CPU-reading count,
and the run's wall duration. A screen that prints a verdict without naming its
subject is this project's most-repeated instrument defect (six in one wave), and
the inflating direction MANUFACTURES work.

VERDICTS
  UNEXAMINED   no usable samples. NOT "clean" -- the difference matters, and
               conflating them is exactly the dead-parser failure.
  CONTAMINATED another reimpl-x64.exe, or another game binary, was alive during
               the run. The number is about an unknown mixture.
  BUSY         compilers / extra Python interpreters / high CPU alongside the run.
               The number is real but was taken under load; say so when quoting it.
  CLEAN        none of the above was seen in any sample.

Usage:  tools/machine_load_report.py <machine_load.csv>
"""
import csv
import sys


def num(s):
    s = (s or "").strip()
    if not s:
        return None
    try:
        return float(s)
    except ValueError:
        return None


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    path = argv[1]
    rows = []
    try:
        with open(path, newline="") as fh:
            for r in csv.DictReader(fh):
                rows.append(r)
    except OSError as e:
        print("machine load: UNEXAMINED -- cannot read %s (%s)" % (path, e))
        return 1

    # The pre-launch sample is written with t_s = -1 and is the BASELINE: it says
    # what was already running before this run added anything. Separating it is
    # what lets "python: 3" be reported as "3 were already there" rather than as
    # something the run caused.
    base = [r for r in rows if num(r.get("t_s")) is not None and num(r["t_s"]) < 0]
    during = [r for r in rows if num(r.get("t_s")) is not None and num(r["t_s"]) >= 0]

    if not during and not base:
        print("machine load: UNEXAMINED -- %s holds no samples." % path)
        print("  (This is NOT 'clean'. Nothing was measured.)")
        return 1

    def col(rs, name):
        return [v for v in (num(r.get(name)) for r in rs) if v is not None]

    cpu = col(during, "cpu_pct")
    comp = col(during, "compiler")
    mk = col(during, "make")
    py = col(during, "python")
    re_ = col(during, "reimpl")
    og = col(during, "other_game")
    mem = col(during, "free_mb")
    ts = col(during, "t_s")

    py_base = max(col(base, "python") or [0])
    comp_base = max(col(base, "compiler") or [0])

    dur = int(max(ts)) if ts else 0

    print("machine load: examined %d samples over %ds (%d of them carry a CPU reading)"
          % (len(during), dur, len(cpu)))
    if base:
        print("  baseline (before launch): compiler=%d python=%d cpu=%s free_mb=%s"
              % (comp_base, py_base,
                 ("%d" % col(base, "cpu_pct")[0]) if col(base, "cpu_pct") else "?",
                 ("%d" % col(base, "free_mb")[0]) if col(base, "free_mb") else "?"))
    else:
        print("  baseline: NONE RECORDED -- 'how much of this was already here' is unanswerable")

    if not during:
        print("machine load: UNEXAMINED -- a baseline only; nothing was sampled during the run.")
        return 1

    def stat(vals):
        if not vals:
            return "n/a"
        s = sorted(vals)
        med = s[len(s) // 2]
        return "median %.0f, max %.0f" % (med, max(s))

    print("  cpu%%      : %s" % stat(cpu))
    print("  free_mb   : %s" % stat(mem))
    print("  compiler  : %s   (cc1plus/gcc/g++/ld/as/collect2)" % stat(comp))
    print("  make      : %s" % stat(mk))
    print("  python    : %s" % stat(py))
    print("  reimpl    : %s   (this run is 1 of these)" % stat(re_))
    print("  other_game: %s   (Application-x64 / Tools-x64)" % stat(og))

    reasons = []
    if re_ and max(re_) > 1:
        reasons.append(("CONTAMINATED", "a SECOND reimpl-x64.exe was alive (max %d)" % max(re_)))
    if og and max(og) > 0:
        reasons.append(("CONTAMINATED", "another game binary was alive (max %d)" % max(og)))
    if comp and max(comp) > 0:
        reasons.append(("BUSY", "a compiler ran during this run (max %d; baseline %d)"
                        % (max(comp), comp_base)))
    if mk and max(mk) > 0:
        reasons.append(("BUSY", "make ran during this run (max %d)" % max(mk)))
    if py and max(py) > py_base:
        reasons.append(("BUSY", "python processes rose above the baseline (%d > %d)"
                        % (max(py), py_base)))
    if cpu:
        s = sorted(cpu)
        med = s[len(s) // 2]
        if med >= 70:
            reasons.append(("BUSY", "median CPU %.0f%% across the run" % med))

    if any(v == "CONTAMINATED" for v, _ in reasons):
        verdict = "CONTAMINATED"
    elif reasons:
        verdict = "BUSY"
    else:
        verdict = "CLEAN"

    print("machine load: %s" % verdict)
    for v, why in reasons:
        print("  - [%s] %s" % (v, why))
    if verdict == "CLEAN":
        print("  - nothing on the watched list appeared in any of the %d samples."
              % len(during))
        print("    (Sampled every ~5s: something shorter than that interval can hide.)")
    elif verdict == "CONTAMINATED":
        print("  This run's numbers describe an unknown mixture. Do not baseline them.")
    else:
        print("  The numbers are real but were taken under load. Quote them WITH this line.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
