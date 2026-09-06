# Wave 0 — Runtime port tooling

Landed 2026-09-06 on `wave/upgrade-path-complete`. Wave 0 delivers mechanisms, not ported
functions: the seam every later wave registers into, the detour that reaches the DLL's own
internal callers, the oracle that says whether a port is right, and the census that says how far
along we are. Scope and rationale: [PLAN.md](PLAN.md) §4.1–§4.2.

## What landed

| Piece | Files | What it does |
|---|---|---|
| **Registry seam** | `standalone/resolve.h`, `standalone/resolve_standalone.cpp` | `RegisterRuntimeExport(name, ours)` / `RegisterRuntimeAddr(addr, ours)` / `RegisterRuntimeOrdinal(n, ours)`. `ResolveExport` / `ResolveOrdinal` / `ResolveAddr(Module::Runtime, …)` consult these first, so all ~1,180 of our call sites flip the moment a port registers. `ResolveExportRaw` / `ResolveOrdinalRaw` / `ResolveRuntimeRaw` always return the shipped DLL code. |
| **Detour installer** | `standalone/runtime_detour.{h,cpp}`, `standalone/hde/` | `InstallRuntimeDetour(…)` patches the DLL function itself so its *internal* callers come to us too. HDE64-based: it decodes the prologue and relocates PC-relative instructions, so an arbitrary function can be detoured without hand-counting bytes. Revived from `fcd3fb6^:patches/src/hooks.cpp`. |
| **Differential oracle** | `runtime/funcs/rt_diff.{h,cpp}` | `BANISHED_RT_DIFF=1` runs ours *and* the DLL's on the same inputs and reports the first divergence per site with a per-site summary at exit. Zero cost when unset. |
| **Census** | `tools/runtime_census.py` | total (in the DLL) vs called (our tree resolves it) vs ported vs detoured, per namespace. |
| **Source skeleton** | `runtime/src/<dir>/README.md` ×20, `tools/runtime_src_skeleton.py` | One directory per original source directory, each README listing the original file names to mirror. Regenerate with the script; it re-lists what is ported now. |
| **Build** | `Makefile` | One compile rule generated per `runtime/src/` subdirectory (adding a directory no longer means editing the Makefile), plus a C rule for hde64. |
| **Acceptance test** | `standalone/rt_selftest.cpp` | `BANISHED_RT_SELFTEST=1` proves all three mechanisms on a live process. See below. |
| **Ghidra map** | `docs/runtime_port/profile_to_retail.csv`, `runtime_hash_index.json`, `hashmap_report.md` | 9,921 profile↔retail function mappings, 1,181 of them byte-provable hash matches; 8/8 spot-checked pairs genuine. |

## Environment variables

| Variable | Effect |
|---|---|
| `BANISHED_RT_PORT=0` | Master kill-switch: every registered port is bypassed, all calls go to the DLL. The A/B control for any runtime-port run. |
| `BANISHED_RT_DIFF=1` | Differential oracle on. Slower by design — never benchmark with it. |
| `BANISHED_RT_CENSUS=1` | Dump the runtime registry (every ported name/address) at init and at exit. |
| `BANISHED_RT_SELFTEST=1` | Run the Wave 0 acceptance test, and leave a null port registered for the rest of the run. |
| `BANISHED_RT_<CLASS>=0` | Per-port kill-switch, added by each port as it lands (the `kDefaultOnFlags` convention from the exe port). |

## The acceptance test, and what it proved

```
BANISHED_RT_SELFTEST=1 BANISHED_RT_DIFF=1 BANISHED_RT_CENSUS=1 make sa-run
grep RtSelfTest "C:/GOG Games/Banished/standalone_error.log"
```

Subject is `System::Random` — exported, pure with respect to its own 0x9C4-byte state, and
replayable by memcpy, so the sequence is deterministic without knowing how it is seeded.

Result of the 2026-09-06 run:

```
[RtSelfTest] pass: subject is deterministic under snapshot replay
[RtSelfTest] pass: registry precedence: ResolveExport sees the port, Raw does not
[RtDetour] selftest:System::Random::Rand: 00007ffca2e88040 -> 00007ff6d8b3e5bb
           (trampoline 0000017c1af60000, 12 bytes displaced)
[RtSelfTest] pass: detour: wrapper entered, trampoline reproduces the original exactly
[RtSelfTest] pass: detour removal restores the DLL byte for byte
[RtDiff] DIVERGENCE selftest::diverge (EXPECTED)  call #1  ours=980 (0x3d4)  theirs=811 (0x32b)
[RtSelfTest] pass: diff harness exercised
[RtSelfTest] pass: null port registered for System::Random::Rand
```

Test 4 leaves a **null port** live — `Random::Rand` routed through a pass-through thunk of ours
for the rest of the run. It reimplements nothing, so a run with the self-test on should be
indistinguishable from one without it; if it ever is not, the seam is at fault and we learn that
before a real port depends on it.

## The FUN_ pool: measured, and the map does not solve it

PLAN.md §4.3 risk 4 is the 2,467 `FUN_`-named retail internals holding 407 KB — a third of all
code. The profile↔retail map was expected to be the lever. Measured against the finished CSV, it
is not:

> Rows where retail is `FUN_`, the profile side has a real name, and the retail address is
> unique in the map: **9**. None of them are hash matches.

The reason is structural, not a tooling failure: the profile build's internals are `FUN_` in
Ghidra too, because Ghidra names from the export table and these functions are not exported.
Matching an unnamed function to an unnamed function yields nothing. The remaining lever was the
profile build's **strings** — `code\Runtime\<Dir>\<File>.cpp` paths and assert text — applied
profile-side first and only then carried across by the map (`profile_string_anchors.csv`,
`fun_pool_candidates.csv`). It was tried, and it does not work either.

**The string-anchor pass is now measured too, and it does not close the gap either.**
`batch_string_anchor_report` over six patterns touches 1,208 profile `FUN_`s, but only **66** are
real attributions: 35 high (a short `Dir\File.cpp` compile-unit literal — the `__FILE__` argument
to an assert, which genuinely means "defined in this .cpp") and 31 medium (single-class header
paths). The other 1,140 are the *same* collision problem as the hash map, arriving through a
different door: `Collection/Array.h` alone accounts for 879, because header-only templates are
instantiated into hundreds of unrelated classes and a string hit on one says "touches a
container", not which class.

Bridging those 66 to retail is where it dies: **0 hash matches, 6 weak fuzzy candidates
(0.52–0.58), and the one that was decompiled to check was a confirmed FALSE match** (an STB
pixel-expansion routine paired with `Audio::SoundBuffer::PlatformSerialize`). Net: at most 4 of
2,467 retail `FUN_`s get even a weak unconfirmed candidate; zero confirmed. Sharper still:
**62 of the 66 have no retail candidate at all** — the map cannot even propose a partner for
94% of them, so there is nothing to spot-check, let alone rename. The reason is
structural and worth remembering — the functions worth string-anchoring are the substantive ones
(codecs, parsers, thread bodies), which are exactly the ones whose codegen differs most between
the two builds, so they are the *least* likely to match cleanly. The two levers are
anti-correlated.

Directories reached with real attribution: 9 of 18 — Video 20, Utility 16, IO 10, System 10,
Graphics 4, Input 3, Engine 1, Save 1, Picking 1. **Math and Debug are confirmed dark**: zero
source paths in the recovered tree, zero anchors of any kind.

The PDB is not a way out. `Runtime-x64-profile.dll`'s debug record names
`C:\Projects\Buildy\bin\x64.Profile\Runtime-x64-profile.pdb`, but no Banished PDB exists on
this machine and none ships with the game or the ToolKit.

### Third lever: call-graph propagation — works, and still does not close the pool

The one idea with a mechanism the first two lacked: use structure instead of bytes. Seed with the
1,181 byte-provable hash pairs, compare callee and caller sets across each mapped pair, and where
filtering out already-anchored nodes leaves exactly one unresolved `FUN_` on each side, those two
are the same function by elimination. Iterate, feeding each new pair back in as an anchor.

It works, and it is clean: **46 pairs, every one at arity 1/1 (no guessing between candidates),
zero target conflicts, and 8 of 8 spot-checked pairs confirmed by decompiling both sides — a 0%
false-positive rate.** Two of the eight were wrapper shells whose whole body calls into another
pair this same pass resolved, which is the pass cross-confirming itself. Yield by round:
33 / 8 / 4 / 1 / 0 — it compounds for three rounds, then dies.

**But it transfers zero names.** All 46 pairs are `FUN_`↔`FUN_`: what they establish is
cross-build address equivalence, not a name, because the profile side of every pair is itself
unnamed. Nine inherit a *file* attribution from the string-anchor pass (8 `Utility/STBImage.cpp`
codec siblings, 1 `Engine_win.cpp`); 17 more are `Array<T>`/`Allocator<T>` template siblings, the
same low-value noise category Parts 1–2 already flagged, reached by a third route; the remaining
20 carry nothing but the equivalence itself.

The reason it decays is structural: the rule only fires where a `FUN_` sits graph-adjacent to an
already-anchored node, and the anchor set clusters in a few container/template/codec
neighbourhoods. Once those are exhausted there is no frontier left to expand into.

### The tally, across all three levers

| Lever | Retail `FUN_`s touched | Actual NAMES transferred | Verified |
|---|---|---|---|
| Hash / fuzzy map | 9 | 9 (unverified) | no |
| String anchors | ≤4 candidates | 0 | 1 checked → FALSE |
| Call-graph propagation | 46 | 0 (9 file attributions) | 8/8 correct |
| **Total** | **~50 of 2,467 (2%)** | **9, none verified** | |

Three structural levers, run to their own fixed points, on a 2,467-function pool: **~2% touched
and effectively zero functions actually named.** The PDB is not a way out either — the profile
DLL's debug record names `C:\Projects\Buildy\bin\x64.Profile\Runtime-x64-profile.pdb`, but no Banished
PDB exists on this machine and none ships with the game or the ToolKit. A debug record naming a
PDB is a build-time breadcrumb, not evidence the file was distributed.

**Conclusion: the `FUN_` pool will not close by a bulk automated pass.** It closes the same way
the exe port closed — ordinary decompile-and-read work on the functions that actually surface as
each wave needs them. That is a real cost to carry into the wave plan, and it is bounded: a wave
only needs the internals its own classes call, not all 2,467. What must not happen is a wave
planned as though the naming were solved.

Until something lands, **budget `FUN_` naming per wave as PLAN.md §4.3 already says, and treat
the pool as unnamed**. That is a cost the wave plan can absorb; what it cannot absorb is a wave
planned on the assumption that the naming was solved.

The map's real value is what it *is* good at: 1,181 byte-provable profile↔retail pairs and 9,730
fuzzy pairs among already-named functions, which is the address translation the per-function
recipe (PLAN.md §6 step 2) needs. Caveats that matter before trusting a row — the many-to-one
`Unwind@` thunk collisions, and the fact that a correct match can score as low as 0.72 — are in
`hashmap_report.md`.

## Also fixed here

`tools/ghidra_mcp/bridge_mcp_ghidra.py`: `bulk_fuzzy_match` and `find_similar_functions_fuzzy`
are declared `-> str` but returned `safe_get()`'s `list[str]`, so every call through the MCP tool
interface failed output validation before the caller saw any data. Both now use
`safe_get_json()`. The Ghidra endpoint itself was always fine.

## Definition of done

- [x] Registry seam, with Raw escape hatches for anything that patches or diffs against the DLL.
- [x] Detour installer with prologue relocation, plus removal, verified byte-for-byte.
- [x] Differential oracle with per-site summary.
- [x] `tools/runtime_census.py` reporting registered / called / total per namespace.
- [x] `runtime/src/<dir>/` skeleton mirroring the recovered source tree.
- [x] Profile↔retail hash index and map for both Runtime programs.
- [x] String-anchor naming pass over the `FUN_` pool — DONE and MEASURED: 66 real anchors,
      0 confirmed bridges to retail. Neither lever works; call-graph propagation is the next one.
