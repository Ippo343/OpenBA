# WAVE 24 LANE TRACE — replacing the dead pixel oracle with a command-stream oracle

Branch `wave24/trace`, cut from `wave24/base` at `f5d70de2`.
Private worktree. Game-directory lock held once, ~13 minutes, released.

---

## 0. HEADLINE

**The instrument this lane was asked to build already half-existed, and nobody
remembered.** `tools/dx11_state_diff/` — a per-draw pipeline-state extractor and
comparator written for the VideoDX11 port — has been in the tree the whole time,
and its README already states the non-determinism fact Wave 23 rediscovered as
new ("Banished's simulation is **not** frame-deterministic even with the world
seed pinned"). It still works today; I re-verified it rather than quoting it.

What it lacked is what this lane built: **a verdict engine with named subjects, a
control harness, and a MEASURED noise floor.** Its README left the verdict to a
human reading two tables.

The second headline is the measurement, and it is a partial negative result:

> **Only three of the six screens are verdict-bearing on this game.** Between two
> captures of the *identical* binary, seed, pin, command line and frame, the
> set-based screens S1/S2/S5 report 51 / 3 / 29 findings **even after flooring
> against a control pair**. S3 (floored), S4 and S6 report **0**.

My own first draft of the README claimed S1/S2/S5 "need no control pair". That
claim was wrong and the measurement below is what refuted it.

---

## 1. WHAT I MEASURED, AND THE COMMANDS THAT RE-DERIVE IT

### 1.1 The extractor still works on this tip

```
PY="<main checkout>/tools/renderdoc_mcp/.venv/Scripts/python.exe"
$PY tools/dx11_state_diff/dump_state.py \
    "C:/GOG Games/Banished/reimpl_capture_capture.rdc" -o /tmp/smoke.jsonl --limit 25
```
`25 records`, `actions=586 textures=194 buffers=289 resources=915`. Not a dead
parser: it was handed a real capture and returned a real positive shape.

### 1.2 Three same-build captures — the control pair the project never had

Lock: `tools/game_dir_lock.sh acquire lane-TRACE 1500` — **bare, never piped**.
Queued ~45s behind `lane-DIFF`, `ACQUIRED by lane-TRACE` / `GRANT lane-TRACE`.
Released after the third capture.

```
bash tools/trace_oracle/capture_arm.sh ctrl{A,B,C} --frame 150 \
     --expect-log '\[AutoStart\]' -- BANISHED_SEED=12345 BANISHED_CAMERA_PIN=0,0,60
```

Identical in every respect. Deployed exe md5 `5a909b6df845cf00db7b3237af54acc6`
for all three (recorded automatically in `<tag>.arm.txt` beside each capture).
**I did not build this exe** — it was `lane-DIFF`'s deploy, and that is stated
rather than glossed: this lane's claim is about the instrument's run-to-run
variance, not about any port, so any working binary is a valid subject provided
all three arms used the *same* one. They did.

Extracted draw counts: **566 / 557 / 509**. The non-determinism is plainly
visible before any comparison runs.

### 1.3 THE NOISE FLOOR — the number this lane exists to produce

```
for p in "A C" "B C"; do
  $PY tools/trace_oracle/trace_oracle.py --test ctrl$a.jsonl ctrl$b.jsonl \
      --control ctrlA.jsonl ctrlB.jsonl
done
```

| screen | subject | unfloored | floored by one control pair |
|---|---|---|---|
| S1 configSet | set of full pipeline configurations | 41–74 | **51** — NOISE |
| S2 sigSet | S1 with bound-resource identity removed | 3–6 | **3** — NOISE |
| S3 fieldDiff | aligned per-field rates | suppressed | **0** |
| S4 invariants | texture-derived quantities vs the bound texture | **0** | **0** |
| S5 srvPool | textures ever bound as an SRV | 21–43 | **29** — NOISE |
| S6 strideSet | vb/ib strides, offsets excluded | **0** | **0** |

The key-identity floor helps and does not close: S1 went 74 -> 51, S5 43 -> 29.
**The set of run-dependent keys is itself run-dependent**, which is why a single
control pair cannot floor a set screen.

### 1.4 An existing project instrument reports 3 false findings here

`compare_state.explain_signature_gap()` classifies a one-sided signature as
content if "that shader pair never drew on the other side". On two **identical**
runs it reported `gap=3 content=0 UNEXPLAINED=3`, three times over. Its
`UNEXPLAINED` line is not a finding on this workload. Re-derive:

```
$PY <scratch>/s2probe.py tools <trace dir> ctrlA ctrlB ctrlC
```
(the probe is 40 lines and is reproduced in `trace_oracle.py`'s `set_floor`
docstring; it is not committed as a tool because its whole content is this one
measurement.)

### 1.5 Two fields measured unstable, and one guess measured wrong

Between identical runs, on the aligned per-field table:

* `cbContents` **100%** (already known)
* `cbBufferSizes` **30.9–37.4%**
* `vbStrides` **11.1–15.2%** — because `compare_state.strides_only()` bundles the
  byte OFFSET in with the stride
* `cbBindings` **0.0%**

`cbBufferSizes` **settles an open item against my own argument.** I had reasoned
that a D3D11 constant buffer's byte length is fixed at creation, so two agreeing
renderers must allocate the same size — which would have made it a POSITIVE
control. It is a dynamic ring allocation and it is not stable. The control is
NEGATIVE and the field is now `non-verdict`.

---

## 2. MY PROBE SHAPE (kickoff §1c)

**Synthesised oracle**, with a **mutation control harness**. Not the other four:

* *input sweep against the shipped export* — there is no single pure function
  under test; the subject is a whole frame's command stream.
* *shadow/diff site* — a shadow site takes the port's own verdict and never
  consults anything external (`reference_shadow_marker_is_not_an_oracle.md`). A
  renderer's output is not a return value it could compare.
* *write-path replay* — nothing here serialises.
* *two-sided disassembly check* — STRUCTURAL, and this lane is about runtime
  behaviour, not about one body's bytes.

### The control, and its effectiveness counter

`tools/trace_oracle/trace_selftest.py` corrupts a **real** trace in 28 known ways
and scores per screen. Final run, on `ctrlA.jsonl` (566 draws):

```
identity baseline (trace vs an exact copy): all 6 screens silent
positive controls: 19/19 CAUGHT
negative controls:  9/9  clean
open controls:      0
EFFECTIVENESS: every scored mutation changed >=1 field value; 0 INERT
total field values altered across all controls: 11,695

PER-SCREEN RECALL (never printed as a union)
  S1_configSet   17/19     S4_invariants  2/19
  S2_sigSet      14/19     S5_srvPool     1/19
  S3_fieldDiff    1/19     S6_strideSet   1/19
CAUGHT WITHOUT S3: 18/19   (S3 ALONE: cb.bindSize)
```

**The per-screen row is the point.** A union score reads 19/19 and hides that
four of the six screens each carry exactly one or two controls — which is the
`reference_union_control_hides_a_blind_screen.md` failure, and it is the row that
found three of the four defects below.

**Both control directions are exercised.** Positives corrupt a subject the oracle
compares (cull, depth writes, blend enable and source factor, sampler address and
filter, pixel shader, SRV format, depth target, viewport, topology, input-layout
offset, vertex stride, constant-buffer bind size, numMips, maxLOD, bound texture,
RT format, scissor). Negatives corrupt subjects it must **not** flag (event ids,
record indices, action names, constant-buffer contents, draw counts, dynamic
buffer identity, dynamic buffer byte offsets, constant-buffer allocation size,
and fewer draws of the same configurations). The last of those is the measured
non-determinism, reproduced synthetically.

**A mutation that changes nothing is `INERT`, not a pass.** Zero were inert here,
which is itself the check that the mutations reached their subjects.

### Four defects the harness found in my own instrument

1. `vb.stride` was caught by **S3 alone**, and S3 is suppressed in normal use —
   so a vertex-stride difference was invisible in the configuration a lane
   actually has. **S6 was added.**
2. The first S6 keyed on strides *and* offsets, so the `buffer byte offsets`
   negative control tripped it. **S6 now drops offsets** — and §1.5 then measured
   that offsets really are 11–15% noise, confirming the split by measurement
   rather than by argument.
3. `cb buffer size` was `open` on an argument that turned out to be wrong (§1.5).
4. Moving `cbBufferSizes` and `vbStrides` to `non-verdict` left S3 with **no
   positive control at all**, which the harness printed as
   `S3_fieldDiff caught 0/18 <- CAUGHT NOTHING: this screen is unvalidated by
   this run`. **`cb.bindSize` was added**; every screen now has at least one.

### The hand-check, made while the tool was agreeing with me (kickoff §3.1)

Two rows checked by inspection rather than by the harness's own verdict:

* `raster.cull` — record idx 0, eid 23, `Front -> Back`; config hash
  `e493ccb77eed -> 0970042dcf80`; **0 other records altered**; S1 `onlyA` names
  exactly the original hash and `onlyB` exactly the mutated one; the original
  config occurred once in base and zero times in mutated. AGREES.
* `vb.stride` — the row whose result I did *not* expect. `[0,'buf:2424',24,0] ->
  [...,28,0]`; `config_hash` unchanged, `pipeline_sig` unchanged. Confirms the
  blind spot was real and not a scoring artefact.

---

## 3. WHAT I DID NOT ESTABLISH

* **Nothing here is a comparison against retail.** All three captures are of
  *our* exe. The oracle answers "did arming this port change what the renderer
  does, beyond the noise?" It does **not** answer "does our renderer issue the
  same commands as the shipped VideoDX11 DLL". `tools/dx11_state_diff/capture.sh`
  has a `shipped` arm for that; I did not run it, and the A/B seam it depends on
  is not re-verified by this lane.
* **No port was verified.** This lane ported nothing and promoted nothing. The
  three captures were taken to calibrate the instrument, not to test a flag.
* **The `--expect-log` seam is only weakly tested.** I exercised it with a
  pattern that always matches (`\[AutoStart\]`), so I confirmed the *match* path
  and **never the reject path**. A lane using it for a real arm gate should
  deliberately pass a pattern it knows is absent once, and confirm the script
  exits 3 with "this arm did not take effect".
* **The noise floor is three captures on one machine, one build, one frame
  number, one map.** It is not a distribution. A lane whose S1/S5 residue lands
  near 51/29 should not read that as "within the floor" with any confidence; the
  actionable claim is the *order of magnitude*, and the zero on S3-floored, S4
  and S6.
* **I did not diagnose the non-determinism** (secondary, per the kickoff brief).
  What I saw cheaply and nothing more: draw counts 566/557/509 at pinned seed and
  pinned camera; the one-sided S2 signatures are whole materials (a 512x320
  render target, a 2046x2046 depth-only pass, a 1024x640 MSAA pass) rather than
  scattered field noise, i.e. **entire passes or material variants appear and
  disappear between runs**; and `cull=NoCull` variants of a shader pair appear in
  one run and not another. Consistent with Wave 23's day/night-or-weather
  hypothesis. **Not investigated further, and not claimed.**
* **I did not use the RenderDoc MCP tools.** The brief named them; the Python
  `renderdoc` module that the MCP server itself wraps turned out to be the right
  layer, because a committed script a future lane can run beats a sequence of MCP
  calls a future lane has to reproduce from prose. The MCP tools remain available
  for interactive inspection of a capture the oracle flags.
* **`tools/renderdoc_mcp/` is gitignored and absent from every worktree.** The
  scripts depend on it by absolute path into the main checkout. That is a
  documented seam, not a fixed one.

---

## 4. WHAT LANDED

```
tools/trace_oracle/trace_oracle.py     verdict engine, six screens, EXAMINED census
tools/trace_oracle/trace_selftest.py   28-mutation control harness, per-screen scoring
tools/trace_oracle/capture_arm.sh      arbitrary-arm capture with --expect-log gating
tools/trace_oracle/README.md           usage + the measured noise floor
docs/runtime_port/WAVE24_LANE_TRACE.md this file
```

Nothing under `runtime/`, `code/`, `standalone/` or `engine/` was touched. No
`RT_UNPROMOTABLE` tag was touched, so none needs a SCOPE or ADDRESSEE token.
The census is unchanged by this lane and I publish no board figure.

---

## 5. FOR THE NEXT LANE THAT WANTS TO VERIFY A GRAPHICS OR VIDEO PORT

1. Take **three** captures under one lock, not two: `armed`, `armed2`, `unarmed`.
   The third is the control pair, and S3 — the screen with a measured zero floor
   and the finest resolution — does not exist without it.
2. Always pass `--expect-log`. A run that silently failed to arm produces a
   perfectly clean comparison, and nothing else in the pipeline can tell you.
3. Run `trace_selftest.py` **on your own trace** before believing the oracle's
   silence. It takes seconds and it is the difference between "no findings" and
   "no parser".
4. Read S3/S4/S6 as verdicts. Read S1/S2/S5 as description.
5. If you change either script, the selftest must still report 19/19, 9/9, and
   **no screen at 0/N**.
