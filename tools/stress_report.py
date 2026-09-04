#!/usr/bin/env python3
"""tools/stress_report.py -- summarise a BANISHED_STRESS metrics CSV, optionally against a baseline.

    python tools/stress_report.py standalone_stress.csv [--baseline perf/baselines/current.csv]
                                  [--compprof standalone_stress_compprof.txt] [--warmup N] [--json]

The CSV is written by standalone/stress_harness.cpp: one row per BANISHED_STRESS_INTERVAL
frames, '#' meta lines first. Every timing column is a per-frame mean over that window
(ms), except *_p95 (95th percentile of the per-frame samples in the window) and fps
(rendered frames per wall second).

What the summary means:
  update ms      GameUpdateLoop::Update wall time per sim frame (all speed iterations).
  per-iter ms    update ms / sim iterations -- the sim cost of ONE 60 Hz tick; the number
                 to compare across speeds and across builds.
  draw ms        GameUpdateLoop::Draw CPU time (command submission; the GPU is not in it).
  frame ms / fps render-frame period at the Draw entry -- what a player would see.
  phases         the SIMPROF split of update ms (PairCache, Animation, EntityList,
                 Weather, CitizenList, PostSim=UI/camera/audio).
  world          entities / citizens / employed (work248) / housed (home238) / busy AI,
                 memory working set + private bytes. Growth over the run is a leak tell.

Steady state = every row after the first --warmup rows (default 2: the town is still
settling and the staffing pass is converting laborers). Comparisons use steady state.
Exit code 0 always -- this is a reporter, not a gate; grep the 'REGRESSION' lines if you
want one.
"""
import argparse
import json
import math
import os
import sys


def load(path):
    meta, rows = [], []
    with open(path, newline="", encoding="utf-8", errors="replace") as f:
        header = None
        for line in f:
            line = line.rstrip("\r\n")
            if not line:
                continue
            if line.startswith("#"):
                meta.append(line[1:].strip())
                continue
            if header is None:
                header = line.split(",")
                continue
            vals = line.split(",")
            if len(vals) != len(header):
                continue
            row = {}
            for k, v in zip(header, vals):
                try:
                    row[k] = float(v)
                except ValueError:
                    row[k] = v
            rows.append(row)
    return meta, rows


def mean(xs):
    xs = [x for x in xs if x is not None and not (isinstance(x, float) and math.isnan(x))]
    return sum(xs) / len(xs) if xs else float("nan")


def pct(xs, p):
    xs = sorted(x for x in xs if x is not None)
    if not xs:
        return float("nan")
    k = int(round((len(xs) - 1) * p))
    return xs[k]


def summarise(rows, warmup):
    ss = rows[warmup:] if len(rows) > warmup else rows
    if not ss:
        return {}
    g = lambda k: [r.get(k, float("nan")) for r in ss]
    s = {
        "rows": len(rows),
        "steady_rows": len(ss),
        "wall_s": rows[-1]["wall_s"] if rows else 0,
        "frames": rows[-1]["frame"] if rows else 0,
        "speed": ss[-1].get("speed", 0),
        "fps_mean": mean(g("fps")),
        "fps_min": min(g("fps")),
        "frame_ms_mean": mean(g("frame_ms_mean")),
        "frame_ms_p95": pct(g("frame_ms_p95"), 0.95),
        "update_ms_mean": mean(g("update_ms_mean")),
        "update_ms_p95": pct(g("update_ms_p95"), 0.95),
        "update_ms_max_window": max(g("update_ms_mean")),
        "per_iter_ms": mean(g("update_ms_per_iter")),
        "draw_ms_mean": mean(g("draw_ms_mean")),
        "ph_paircache": mean(g("ph_paircache_ms")),
        "ph_animation": mean(g("ph_animation_ms")),
        "ph_entitylist": mean(g("ph_entitylist_ms")),
        "ph_weather": mean(g("ph_weather_ms")),
        "ph_citizenlist": mean(g("ph_citizenlist_ms")),
        "ph_postsim": mean(g("ph_postsim_ms")),
        "entities_first": rows[0]["entities"],
        "entities_last": rows[-1]["entities"],
        "citizens_first": rows[0]["citizens"],
        "citizens_last": rows[-1]["citizens"],
        "work248_last": rows[-1]["work248"],
        "home238_last": rows[-1]["home238"],
        "busy_last": rows[-1]["busy"],
        "hunger_last": rows[-1]["hunger_mean"],
        "ws_mb_first": rows[0]["ws_mb"],
        "ws_mb_last": rows[-1]["ws_mb"],
        "private_mb_first": rows[0]["private_mb"],
        "private_mb_last": rows[-1]["private_mb"],
    }
    dt_min = (rows[-1]["wall_s"] - rows[0]["wall_s"]) / 60.0 if len(rows) > 1 else 0
    s["private_mb_per_min"] = (s["private_mb_last"] - s["private_mb_first"]) / dt_min if dt_min > 0 else 0
    return s


def fmt(v, unit=""):
    if isinstance(v, float):
        if math.isnan(v):
            return "n/a"
        if abs(v) >= 100:
            return f"{v:.0f}{unit}"
        if abs(v) >= 10:
            return f"{v:.1f}{unit}"
        return f"{v:.2f}{unit}"
    return f"{v}{unit}"


def print_timeline(rows):
    print("  frame    t(s) spd    fps  frame  upd/f  /iter   draw  entities  cit  work home busy  hunger  wsMB")
    for r in rows:
        print("  {:>6} {:>6} {:>3} {:>6} {:>6} {:>6} {:>6} {:>6} {:>9} {:>4} {:>5} {:>4} {:>4} {:>7} {:>5}".format(
            int(r["frame"]), int(r["wall_s"]), int(r["speed"]), fmt(r["fps"]), fmt(r["frame_ms_mean"]),
            fmt(r["update_ms_mean"]), fmt(r["update_ms_per_iter"]), fmt(r["draw_ms_mean"]),
            int(r["entities"]), int(r["citizens"]), int(r["work248"]), int(r["home238"]), int(r["busy"]),
            fmt(r["hunger_mean"]), int(r["ws_mb"])))


def compprof_last_window(path, top=12):
    if not path or not os.path.exists(path):
        return None
    blocks, cur = [], None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if "[COMPPROF] world#" in line:
                cur = [line.strip()]
                blocks.append(cur)
            elif cur is not None and "[COMPPROF]" in line:
                cur.append(line.strip())
    if not blocks:
        return None
    return blocks[-1][: top + 1]


# Metrics compared against the baseline: (key, label, higher_is_better, regression threshold %)
COMPARE = [
    ("per_iter_ms", "sim ms / iteration", False, 10),
    ("update_ms_mean", "update ms / frame", False, 10),
    ("update_ms_p95", "update ms p95", False, 15),
    ("draw_ms_mean", "draw ms / frame", False, 10),
    ("frame_ms_mean", "frame ms", False, 10),
    ("fps_mean", "fps", True, 10),
    ("ph_entitylist", "  EntityList ms", False, 10),
    ("ph_citizenlist", "  CitizenList ms", False, 10),
    ("ph_animation", "  Animation ms", False, 10),
    ("ph_paircache", "  PairCache ms", False, 10),
    ("ph_postsim", "  PostSim ms", False, 10),
    ("private_mb_last", "private MB (end)", False, 15),
    ("private_mb_per_min", "private MB / min", False, 50),
]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv")
    ap.add_argument("--baseline")
    ap.add_argument("--compprof")
    ap.add_argument("--warmup", type=int, default=2)
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()

    meta, rows = load(a.csv)
    if not rows:
        print(f"{a.csv}: no data rows (meta: {meta})")
        return 0
    s = summarise(rows, a.warmup)

    if a.json:
        print(json.dumps({"meta": meta, "summary": s}, indent=2))
        return 0

    print(f"STRESS RUN  {a.csv}")
    for m in meta:
        print(f"  {m}")
    print(f"  {int(s['frames'])} sim frames in {s['wall_s']:.0f}s wall, {s['rows']} rows "
          f"({s['steady_rows']} steady after {a.warmup} warm-up)")
    print()
    print_timeline(rows)
    print()
    print("STEADY STATE")
    print(f"  sim       {fmt(s['per_iter_ms'])} ms per 60Hz iteration   "
          f"update {fmt(s['update_ms_mean'])} ms/frame (p95 {fmt(s['update_ms_p95'])}, worst window {fmt(s['update_ms_max_window'])}) "
          f"at speed {int(s['speed'])}")
    print(f"  phases    EntityList {fmt(s['ph_entitylist'])}  CitizenList {fmt(s['ph_citizenlist'])}  "
          f"Animation {fmt(s['ph_animation'])}  PairCache {fmt(s['ph_paircache'])}  "
          f"Weather {fmt(s['ph_weather'])}  PostSim {fmt(s['ph_postsim'])}  (ms/frame)")
    print(f"  render    draw {fmt(s['draw_ms_mean'])} ms CPU   frame {fmt(s['frame_ms_mean'])} ms "
          f"(p95 {fmt(s['frame_ms_p95'])})   {fmt(s['fps_mean'])} fps (min window {fmt(s['fps_min'])})")
    print(f"  world     entities {int(s['entities_first'])} -> {int(s['entities_last'])}   "
          f"citizens {int(s['citizens_first'])} -> {int(s['citizens_last'])}   "
          f"employed {int(s['work248_last'])}  housed {int(s['home238_last'])}  busy {int(s['busy_last'])}  "
          f"hunger {fmt(s['hunger_last'])}")
    print(f"  memory    working set {fmt(s['ws_mb_first'])} -> {fmt(s['ws_mb_last'])} MB   "
          f"private {fmt(s['private_mb_first'])} -> {fmt(s['private_mb_last'])} MB "
          f"({fmt(s['private_mb_per_min'])} MB/min)")

    cp = compprof_last_window(a.compprof)
    if cp:
        print()
        print("COMPONENT UPDATE PROFILE (last window)")
        for line in cp:
            print("  " + line.replace("[COMPPROF]", "").rstrip())

    if a.baseline and os.path.exists(a.baseline):
        bmeta, brows = load(a.baseline)
        b = summarise(brows, a.warmup)
        print()
        print(f"VS BASELINE  {a.baseline}")
        for m in bmeta:
            print(f"  {m}")
        same_shape = (int(b.get("citizens_last", -1)) and int(b.get("speed", 0)) == int(s.get("speed", 0)))
        if not same_shape:
            print("  !! baseline ran at a different speed -- per-frame numbers are not comparable, per-iteration ones are")
        print("  {:<22} {:>10} {:>10} {:>8}".format("metric", "baseline", "now", "delta"))
        regressions = 0
        for key, label, hib, thr in COMPARE:
            bv, nv = b.get(key, float("nan")), s.get(key, float("nan"))
            if isinstance(bv, float) and isinstance(nv, float) and bv and not math.isnan(bv) and not math.isnan(nv):
                d = (nv - bv) / abs(bv) * 100.0
                worse = (d < -thr) if hib else (d > thr)
                better = (d > thr) if hib else (d < -thr)
                tag = "  REGRESSION" if worse else ("  improved" if better else "")
                regressions += worse
                print("  {:<22} {:>10} {:>10} {:>+7.1f}%{}".format(label, fmt(bv), fmt(nv), d, tag))
            else:
                print("  {:<22} {:>10} {:>10} {:>8}".format(label, fmt(bv), fmt(nv), "n/a"))
        print(f"  {regressions} regression(s) over threshold")
    return 0


if __name__ == "__main__":
    sys.exit(main())
