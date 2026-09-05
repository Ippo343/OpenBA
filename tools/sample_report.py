#!/usr/bin/env python3
"""Aggregate standalone_samples.txt (BANISHED_SAMPLE sampling profiler) into a report.

usage: sample_report.py <samples.txt> [--exe standalone/build-perf/reimpl-x64.exe]
                        [--tag N[,N..]] [--top 40] [--callers SYMBOL]

Each line: "<tag> <n> <frame0> <frame1> ...", frame0 = leaf. Frames are
E:0x140... (exe, symbolized with addr2line), R:<export>+off (Runtime), V:..., ?:mod+off.
Tags: 0 outside Update, 1 Update (no phase), 2 PairCache, 3 Animation, 4 EntityList,
5 Weather, 6 CitizenList, 7 PostSim.
"""
import argparse, collections, os, re, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe_exports  # noqa: E402

RUNTIME_DLL = r"C:/GOG Games/Banished/Runtime-x64.dll"

TAGS = {0: "outside-Update", 1: "Update/other", 2: "PairCache", 3: "Animation",
        4: "EntityList", 5: "Weather", 6: "CitizenList", 7: "PostSim", 8: "EntityList/buckets"}


def symbolize_exe(addrs, exe):
    out = {}
    addrs = sorted(addrs)
    if not addrs:
        return out
    CH = 2000
    for i in range(0, len(addrs), CH):
        chunk = addrs[i:i + CH]
        p = subprocess.run(["addr2line", "-e", exe, "-f", "-C", "-s"] + [hex(a) for a in chunk],
                           capture_output=True, text=True)
        lines = p.stdout.splitlines()
        for j, a in enumerate(chunk):
            fn = lines[2 * j].strip() if 2 * j < len(lines) else "??"
            loc = lines[2 * j + 1].strip() if 2 * j + 1 < len(lines) else "??"
            if fn == "??":
                fn = "exe+%x" % (a - 0x140000000)
            out[a] = (fn, loc)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--exe", default="standalone/build-perf/reimpl-x64.exe")
    ap.add_argument("--tag", default=None)
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument("--callers", default=None, help="show caller breakdown for this symbol")
    ap.add_argument("--callees", default=None, help="show what this symbol spends time in")
    a = ap.parse_args()
    want = None if a.tag is None else set(int(x) for x in a.tag.split(","))

    samples = []
    exe_addrs = set()
    tag_counts = collections.Counter()
    with open(a.path, encoding="utf-8", errors="replace") as f:
        for line in f:
            parts = line.split()
            if len(parts) < 3:
                continue
            tag = int(parts[0])
            tag_counts[tag] += 1
            if want is not None and tag not in want:
                continue
            frames = parts[2:]
            samples.append((tag, frames))
            for fr in frames:
                if fr.startswith("E:"):
                    exe_addrs.add(int(fr[2:], 16))
    total_all = sum(tag_counts.values())
    print("samples by tag (all):")
    for t in sorted(tag_counts):
        print("  %-16s %7d  %5.1f%%" % (TAGS.get(t, str(t)), tag_counts[t], 100.0 * tag_counts[t] / max(1, total_all)))
    sym = symbolize_exe(exe_addrs, a.exe)
    rt = pe_exports.Resolver(RUNTIME_DLL) if os.path.exists(RUNTIME_DLL) else None
    name_cache = {}

    def name(fr):
        r = name_cache.get(fr)
        if r is not None:
            return r
        if fr.startswith("E:"):
            r = sym.get(int(fr[2:], 16), ("?", "?"))[0]
        elif fr.startswith("?:Runtime-x64.dll+") and rt:
            nm, _ = rt.nearest(int(fr.split("+0x")[1], 16))
            r = "R:" + (nm or "?")
        elif fr.startswith("R:") or fr.startswith("V:"):
            r = fr[:2] + re.sub(r"\+0x[0-9a-f]+$", "", fr[2:])
        else:
            r = re.sub(r"\+0x[0-9a-f]+$", "", fr)
        name_cache[fr] = r
        return r

    n = len(samples)
    print("\nselected samples: %d" % n)
    if not n:
        return
    self_c = collections.Counter()
    incl_c = collections.Counter()
    for tag, frames in samples:
        names = [name(fr) for fr in frames]
        self_c[names[0]] += 1
        for nm in set(names):
            incl_c[nm] += 1

    print("\nSELF (leaf) time, top %d:" % a.top)
    for nm, c in self_c.most_common(a.top):
        print("  %6.2f%%  %6d  %s" % (100.0 * c / n, c, nm))
    print("\nINCLUSIVE (on stack) time, top %d:" % a.top)
    for nm, c in incl_c.most_common(a.top):
        print("  %6.2f%%  %6d  %s" % (100.0 * c / n, c, nm))

    if a.callers:
        cc = collections.Counter()
        for tag, frames in samples:
            names = [name(fr) for fr in frames]
            for i, nm in enumerate(names):
                if nm == a.callers and i + 1 < len(names):
                    cc[names[i + 1]] += 1
                    break
        print("\nCALLERS of %s:" % a.callers)
        for nm, c in cc.most_common(a.top):
            print("  %6.2f%%  %6d  %s" % (100.0 * c / n, c, nm))
    if a.callees:
        cc = collections.Counter()
        for tag, frames in samples:
            names = [name(fr) for fr in frames]
            for i, nm in enumerate(names):
                if nm == a.callees:
                    cc[names[i - 1] if i > 0 else "<self>"] += 1
                    break
        print("\nCALLEES of %s (direct):" % a.callees)
        for nm, c in cc.most_common(a.top):
            print("  %6.2f%%  %6d  %s" % (100.0 * c / n, c, nm))


if __name__ == "__main__":
    main()
