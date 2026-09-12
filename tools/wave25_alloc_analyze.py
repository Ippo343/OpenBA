"""WAVE 25 lane ALLOC -- the A/B analysis, with its attestation control in the same pass.

WHY THE ATTESTATION IS NOT OPTIONAL
The perf exe compiles every printf to ((void)0), so the ONE binary an A/B is measured on
was, until Wave 22, precisely the binary that could not say whether it had armed. A
silent refusal (missing platform seam, missing detour installer, foreign prologue patch
-- RtSystemAllocatorRegister has refusal paths for all of these) would turn the whole
series into an A/A that reads as "no regression". Both arms now attest on stderr, which
survives the strip, and this script REFUSES to report a pair whose runs did not each
attest the arm their label claims.

The arm is read with a WORD-BOUNDARY test, not a substring test: "ARMED" in "UNARMED" is
true, and Wave 22's own summariser labelled every control run armed because of it.
"""
import csv
import os
import re
import statistics
import sys

RESULTS = "perf/results"
PAIRS = range(1, 9)


def read_attest(d):
    """Return 'on' / 'off' / None, read from the run's OWN artefacts."""
    p = os.path.join(d, "standalone_error.log")
    if not os.path.exists(p):
        return None
    armed = unarmed = False
    exports = None
    with open(p, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if "[RtAlloc] ATTEST:" not in line:
                continue
            # word boundary -- "UNARMED" must not match "ARMED"
            if re.search(r"ATTEST:\s+ARMED\b", line):
                armed = True
                m = re.search(r"exports=(\d+)", line)
                if m:
                    exports = int(m.group(1))
            elif re.search(r"ATTEST:\s+UNARMED\b", line):
                unarmed = True
    if armed and not unarmed:
        return ("on", exports)
    if unarmed and not armed:
        return ("off", exports)
    return None


def sim_median(d):
    """Median sim ms/iteration over the reported windows, dropping the first two
    (warm-up), per perf/README.md."""
    p = os.path.join(d, "standalone_stress.csv")
    if not os.path.exists(p):
        return None, 0
    vals = []
    with open(p, encoding="utf-8", errors="replace") as fh:
        for row in csv.DictReader(r for r in fh if not r.startswith("#")):
            raw = row.get("update_ms_per_iter")
            if raw in (None, ""):
                continue
            try:
                vals.append(float(raw))
            except ValueError:
                continue
    vals = vals[2:]
    if not vals:
        return None, 0
    return statistics.median(vals), len(vals)


def load_summary(d):
    p = os.path.join(d, "machine_load.txt")
    if not os.path.exists(p):
        return "(no load record)"
    busy = []
    for line in open(p, encoding="utf-8", errors="replace"):
        m = re.search(r"busy_cores_during_window=([\d.]+)", line)
        if m:
            busy.append(float(m.group(1)))
    if not busy:
        return "(load record present but unparsed)"
    return "busy_cores %s" % "/".join("%.2f" % b for b in busy)


def find(label):
    hits = [d for d in os.listdir(RESULTS) if d.endswith("_" + label)]
    if not hits:
        return None
    return os.path.join(RESULTS, sorted(hits)[-1])


def main():
    print("PAIR  ORDER     ON med    OFF med    on-off     on%      attest      load(on) / load(off)")
    diffs = []
    ons, offs = [], []
    bad = 0
    for i in PAIRS:
        don, doff = find("w25ab_p%d_on" % i), find("w25ab_p%d_off" % i)
        if not don or not doff:
            print("%-5d  INCOMPLETE -- discarded (an unpaired arm cannot enter a paired analysis)" % i)
            continue
        aon, aoff = read_attest(don), read_attest(doff)
        ok = aon and aoff and aon[0] == "on" and aoff[0] == "off"
        if not ok:
            print("%-5d  ATTESTATION FAILED on=%s off=%s -- DISCARDED" % (i, aon, aoff))
            bad += 1
            continue
        mon, non = sim_median(don)
        moff, noff = sim_median(doff)
        if mon is None or moff is None:
            print("%-5d  NO CSV -- discarded" % i)
            continue
        order = "on,off" if int(os.path.basename(don)[9:15]) < int(os.path.basename(doff)[9:15]) else "off,on"
        d = mon - moff
        diffs.append(d)
        ons.append(mon)
        offs.append(moff)
        print("%-5d %-9s %7.4f   %7.4f   %+7.4f  %+6.1f%%   exports=%-4s %s / %s"
              % (i, order, mon, moff, d, 100.0 * d / moff, aon[1], load_summary(don), load_summary(doff)))

    if not diffs:
        print("\nno usable pairs")
        return 1
    n = len(diffs)
    mean = statistics.mean(diffs)
    sd = statistics.stdev(diffs) if n > 1 else 0.0
    sem = sd / (n ** 0.5) if n > 1 else 0.0
    base = statistics.mean(offs)
    signs = sum(1 for d in diffs if d > 0)
    print("\nPAIRS USED           n=%d   (attestation-rejected: %d)" % (n, bad))
    print("ARMED   mean of medians %.4f  min %.4f  max %.4f  spread %.4f"
          % (statistics.mean(ons), min(ons), max(ons), max(ons) - min(ons)))
    print("unarmed mean of medians %.4f  min %.4f  max %.4f  spread %.4f"
          % (base, min(offs), max(offs), max(offs) - min(offs)))
    print("PAIRED DIFF (on-off) mean %+.4f ms/iter  = %+.2f%% of the unarmed mean" % (mean, 100.0 * mean / base))
    print("  sd %.4f  sem %.4f   sign: %d of %d pairs positive" % (sd, sem, signs, n))
    # 95% CI on the paired mean, t~2.14 at 14 df / 2.36 at 7 df -- stated, not hidden.
    t = {7: 2.365, 6: 2.447, 5: 2.571, 4: 2.776, 3: 3.182}.get(n - 1, 2.0)
    lo, hi = mean - t * sem, mean + t * sem
    print("  95%% CI on the paired mean: %+.4f .. %+.4f ms/iter  (%+.2f%% .. %+.2f%%)"
          % (lo, hi, 100.0 * lo / base, 100.0 * hi / base))
    print("  => the series BOUNDS the treatment at +-%.2f%% of sim time; it does not measure one."
          % max(abs(100.0 * lo / base), abs(100.0 * hi / base)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
