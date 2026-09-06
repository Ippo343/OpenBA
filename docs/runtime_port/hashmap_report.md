# Runtime profile ↔ retail function map — Wave 0 tooling

Produced 2026-09-06 against the live Ghidra project (`Runtime-x64-profile.dll`, ToolKit build,
and `Runtime-x64.dll`, retail). Read-only: no rename/set/create/delete calls were made against
either program.

## Tooling note (bridge bug, now fixed)

`bulk_fuzzy_match` and `find_similar_functions_fuzzy` were declared to return `str` but the
bridge (`tools/ghidra_mcp/bridge_mcp_ghidra.py`) implemented them with `safe_get()` (which returns
a `list[str]`) instead of `safe_get_json()` (which returns `str`). Every call through the MCP
tool interface failed Pydantic's output validation (`Input should be a valid string`), regardless
of parameters. The underlying Ghidra HTTP endpoint (`http://127.0.0.1:8089/bulk_fuzzy_match?...`)
was unaffected and returned clean JSON; the bulk fuzzy matching in Part 1 (below) was done with
direct HTTP calls (Python `urllib`) to that endpoint, paging with `limit=500`, before the fix
landed. The team lead has since fixed and committed the one-line bridge bug as part of Wave 0;
Part 2 (string-anchor pass, below) still uses the direct-HTTP path for the per-function
`find_similar_functions_fuzzy` sweep and `batch_string_anchor_report` (which was never affected —
it correctly used `safe_get_json` throughout) purely because that code was already written before
the fix landed, not because it was still necessary.

## Deliverables

- `docs/runtime_port/runtime_hash_index.json` (9.4 MB) — normalized-opcode-hash index over both
  Runtime programs, `filter="all"`. Tool summary: `programs_scanned=2, functions_indexed=30861,
  unique_hashes=8357, duplicates_found=2626`.
- `docs/runtime_port/profile_to_retail.csv` — 9,921 rows, columns
  `profile_addr,profile_name,retail_addr,retail_name,score,method`.
- `docs/runtime_port/profile_string_anchors.csv` (Part 2) — 1,208 rows, columns
  `profile_addr,current_name,anchor_string,anchor_kind,inferred_dir,inferred_file,confidence,evidence`.
- `docs/runtime_port/fun_pool_candidates.csv` (Part 2) — 66 rows, the high/medium-confidence
  anchored functions re-matched against retail.
- This report.

# Part 1 — profile↔retail address map (hash + fuzzy)

## Coverage

| Metric | Count |
|---|---|
| Profile functions in hash index | 15,752 (16,002 raw Ghidra count; ~250 are pure import thunks the indexer skips) |
| — of which named (non-`FUN_`) | 11,839 |
| Retail functions in hash index | 15,109 (15,290 raw) |
| — of which named (non-`FUN_`) | 12,642 |
| — of which `FUN_`-named internals | 2,467 (matches PLAN.md §3.1 exactly) |
| Retail distinct export addresses | 4,927 (measured directly via `list_exports`; PLAN.md's "4,633 export entry points" undercounts because several export names alias one address — ordinal + mangled name + underscore-prefixed variant) |
| **Exact 1:1 hash matches** (unique profile hash ⇄ unique retail hash) | **1,181** (990 named profile side, 191 unnamed) |
| Hash groups too ambiguous to use as 1:1 (shared by ≥2 profile and/or ≥2 retail functions) | 588 groups, 9,160 profile functions involved — almost entirely `Unwind@…` SEH thunks and one-line trivial bodies (`Type`, `operator_new`, empty destructors) that are byte-identical after normalization and cannot be disambiguated by hash alone |
| Fuzzy matches (`bulk_fuzzy_match`, `filter="named"`, `threshold=0.7`) | 9,730 of 11,711 named source functions matched ≥0.7 (1,981 named functions had no match ≥0.7) |
| **Final combined map** (`profile_to_retail.csv`) | **9,921 rows** — 1,181 by hash (score 1.0, method=hash), 8,740 net-new by fuzzy (method=fuzzy; the other 990 fuzzy hits duplicate the hash rows and were dropped in favor of the hash row) |
| Profile-side coverage | 9,921 / 15,752 = **63.0%** of all profile functions; 9,730 / 11,839 = **82.2%** of named profile functions |
| Retail exports covered | 1,487 / 4,927 = **30.2%** |
| Retail `FUN_`-named internals covered | 203 / 2,467 = **8.2%** — expected: the `FUN_` pool is internal engine code with no profile-side name to anchor a search, so it's reached almost entirely by luck (fuzzy match landing on an unnamed target) rather than intentionally |

### Score distribution (9,921 rows)

| Bucket | Rows |
|---|---|
| 1.00 (hash exact, or fuzzy landed on identical body) | 7,026 |
| 0.90–0.99 | 72 |
| 0.80–0.89 | 683 |
| 0.70–0.79 | 2,140 |
| below 0.70 | 0 (threshold floor) |

### Important caveat: many-to-one collisions

The map is **profile→retail best-match**, not a bijection. 689 retail addresses receive more
than one profile-side match (only 1,533 of the 2,222 distinct retail addresses hit are unique
1:1 targets). The worst offenders are the same structural collision the hash pass found:
per-function SEH unwind helper thunks (`Unwind@<addr>`, hundreds of near-identical bodies — one
example, retail `0x18012c1f8`, is claimed as the "best fuzzy match" by 323 different profile
`Unwind@` thunks, all scoring 1.0) and trivial one-liners (`Type`, `operator_new`, generic
one-field destructors like `~CriticalSection`/`~String`). These rows are **not actionable for
renaming** — treat any row whose `retail_name` starts with `Unwind@` as noise, and treat any
row hit by >1 distinct profile source as needing manual disambiguation before a Ghidra rename.
The clean, high-confidence subset is: `method=hash` (1,181 rows, byte-provably the same function)
plus `method=fuzzy` rows whose `retail_addr` is unique in the CSV and whose name is not one of
the generic collision patterns above.

## Spot-check (8 mappings picked pseudo-randomly across the score range, excluding the known-noisy
`Unwind@`/generic bucket so the check is informative rather than a coin flip)

Both sides decompiled with `decompile_function(program=...)`.

| # | Profile addr/name | Retail addr/name | Method/score | Verdict |
|---|---|---|---|---|
| 1 | `0x1800fec00 CreateConstantBufferData` | `0x1800f85f0 CreateConstantBufferData` | hash / 1.0 | **Match.** Byte-identical bodies (`Video::Device::CreateConstantBufferData`), only the vtable-slot immediate constant differs across builds — it doesn't, both read `+0x58`. |
| 2 | `0x18003a800 operator==` | `0x1800150a0 operator==` | hash / 1.0 | **Match.** `Math::Vector3::operator==`, identical 3-float compare, confirmed by the embedded mangled-name comment in both decompilations. |
| 3 | `0x180028100 TypeName` | `0x180002810 TypeName` | fuzzy / 1.0 | **Match.** `Graphics::MaterialInstance::TypeName` on both sides (confirmed by the mangled name in the plate comment, not just the generic `TypeName` label) — returns the class's own string pointer. |
| 4 | `0x1800d8b60 TypeName` | `0x18004e8e0 TypeName` | fuzzy / 1.0 | **Match.** `Video::PixelProgram::TypeName` on both sides — same mangled name, different literal-string address as expected across builds. Shows the generic `TypeName` label alone is not proof (there are hundreds of these); the mangled-name comment is what confirms it. |
| 5 | `0x18009b630 CommitToPage` | `0x1800c7bd0 CommitToPage` | fuzzy / 0.95 | **Match.** `System::MemoryAllocator::CommitToPage`, identical logic; only the hardcoded page-size field offset differs (`+0x690` profile vs `+0x668` retail — expected struct-layout drift between builds). |
| 6 | `0x180050b40 IsShadowMap` | `0x18001eb20 IsShadowMap` | fuzzy / 0.85 | **Match.** `Video::ImageBuffer::IsShadowMap`, identical bit-test; only the flags-field offset differs (`+0x3c` vs `+0x34`). |
| 7 | `0x1800b3930 LabelElement ctor` | `0x1800d2620 LabelElement ctor` | fuzzy / 0.775 | **Match.** `UI::LabelElement::LabelElement()`. The profile side inlines `Sprite::Sprite()` construction where retail's is a zero-init of the same fields — real compiler-generation difference, same function. |
| 8 | `0x180098080 GetPathFilename` | `0x1800c6740 GetPathFilename` | fuzzy / 0.724 | **Match.** `System::Filename::GetPathFilename`. Lower score is explained by a real ABI difference: retail takes an explicit hidden-return `Filename*` parameter and does its own refcounting/`MemFree` inline, where the profile build's `String::SubString` callee does more of that work — same algorithm, different amount of inlining. This is the kind of case the recipe in PLAN.md §6 anticipates (byte-faithful first; confirm via decompile, not score alone). |

**8/8 spot-checked pairs are genuine matches.** All eight required actually reading the mangled
name / field-offset comment in the decompilation, not just the score or the display name — #3
and #4 in particular would be indistinguishable from a false match by name alone (many classes
share the trivial `TypeName` body shape), and #8 shows a correct match can legitimately score as
low as 0.72. Conversely, the many-to-one collision analysis above is direct evidence that the
same low-0.7-to-0.85 band also contains real false matches whenever the target is a generic
thunk — the fix is not a higher threshold (0.72 was a true positive) but excluding the known
collision patterns (`Unwind@`, single-field ctors/dtors, `operator_new`/`operator_delete`,
bare `Type`) before trusting a row for renaming.

## Recommendation for the renaming pass (not done here)

1. Rename retail functions from the 1,181 `method=hash` rows first — provably identical, zero
   ambiguity.
2. For `method=fuzzy` rows, restrict to the 1,533 rows whose `retail_addr` appears exactly once
   in `profile_to_retail.csv` (excludes the `Unwind@`/generic collision set almost entirely) and
   spot-check score ≥0.8 in bulk, ≥0.7 individually — as done above, the mangled-name comment in
   the decompilation is the actual proof, not the score.
3. Re-run `bulk_fuzzy_match` at a lower threshold (e.g. 0.5) only after excluding known collision
   targets from the candidate pool, to recover more of the 1,981 unmatched named functions and
   the `FUN_` pool without re-adding the thunk noise.

# Part 2 — string-anchor naming pass (profile side, source-path/log-string evidence)

Cross-check confirmed Part 1's map barely touches the `FUN_` pool: named-profile↔named-retail
matching cannot resolve an unnamed↔unnamed pair. This part answers "how far can source-string
evidence alone get us on the *profile* side, and does that then bridge to retail?" The pass is
strictly read-only: profile-side "naming" below means recording an inferred dir/file/confidence
in a CSV row, never a Ghidra rename.

## Method

`batch_string_anchor_report(pattern, program="Runtime-x64-profile.dll")` scans every string
matching `pattern` and returns, per matching string, the functions that reference it (split into
already-documented vs `FUN_`-named). Six passes were run:

| Pattern | Anchors found | Undocumented (`FUN_`) functions reached |
|---|---|---|
| `.cpp` (short `Dir\File.cpp` compile-unit literals) | 117 | 35 |
| `code\Runtime` / `.h` / `Buildy` (all three converge on the same 92 full absolute-path strings) | 92 | 1,245 |
| `Assert` | 3 | 0 |
| `ERROR` | 49 | 12 |
| `Failed` | 30 | 1 |
| `Verify`, `WARN` | 0–1 | 0 |

`docs/runtime_port/original_source_tree.txt` lists 18 top-level directories with recovered
source paths (Achievement, Application, Audio, Collection, Collision2D, Engine, Graphics, Input,
IO, Object, Pathfinding, Picking, Save, System, UI, Utility, Video, Volume). **Math and Debug
have zero entries in that file and zero string anchors of any kind** — confirmed dark, exactly as
expected going in (PLAN.md §3.1: Math is 228 pure functions with no debug strings; Debug is 8
functions, presumably the assert/logging primitives themselves, which don't get their own
`__FILE__` literal). All 18 recovered directories got at least one string anchor (documented or
not), so directory-level coverage of the *recovered source tree* is complete; the question that
matters is how much of that reaches an actual `FUN_`.

### The `.h`/full-path pass is dominated by template-instantiation noise, same failure mode as Part 1

Sorting the 92 full-path anchors by total hit count reproduces the Collection-template problem
from Part 1, just via strings instead of hashes:

| Header | Total refs | Undocumented | Why it's noise |
|---|---|---|---|
| `Collection/Array.h` | 1,199 | 879 | Header-only template (`Array<T>`), instantiated into hundreds of unrelated consumer classes — matches PLAN.md §3.3/§4.3 risk 1 exactly |
| `Object/InternalPtr.h` | 142 | 105 | Header-only smart-pointer template, same story |
| `Object/ExternalFactory.h` | 88 | 76 | Has its own `.cpp`, but this hit is from a generic templated helper method inside the header, reused broadly |
| `Collection/Allocator.h` | 81 | 74 | Header-only allocator template |
| `Utility/HashGrid.h` | 45 | 45 | Header-only spatial-hash template (no `.cpp` counterpart in the source tree — same signature as the four above) |
| `Utility/LooseQuadTree.h` | 7 | 7 | Header-only, same signature |

These six headers account for **1,186 of the 1,245 `FUN_` hits from the full-path pass (95%)**.
A function that merely embeds one of these strings tells you "this touches a Collection/Object
container," not which class it belongs to — it is not a usable per-file attribution and
`profile_string_anchors.csv` marks all of it `confidence=none`.

### What's actually usable: 66 functions with real dir+file attribution

Excluding the six noise headers leaves two solid categories, both in `profile_string_anchors.csv`
with `confidence=high` or `medium`:

- **35 `high`-confidence rows** — the `.cpp` compile-unit pattern. The string is a short
  `Dir\File.cpp` literal passed as the `MemoryAllocator::MemAlloc` allocation tag or as an
  `Assert`/`Warn` file argument; this is compiled from that exact translation unit, so the
  function is defined in that file. Spans Engine, Graphics, IO, Save, System, Utility, Video,
  Picking (see table below).
- **31 `medium`-confidence rows** — single-class header paths whose total reference count is
  small (≤40) and whose header is not one of the six noise templates: `Video/VideoConfiguration.h`
  (20), `Input/InputConfiguration.h`, `System/Memory.h` (10), `Graphics/AnimationManager.h`,
  `Graphics/GraphicsScene.h`. These are inline snippets or small classes where a header path
  narrowly identifies one class.

Directories reached by the 66 real (`high`+`medium`) rows — **9 of the 18 recovered
directories**: Video (20), Utility (16), IO (10), System (10), Graphics (4), Input (3), Engine
(1), Save (1), Picking (1). The other 9 recovered directories (Achievement, Application, Audio,
Collection, Collision2D, Object, Pathfinding, UI, Volume) have source-path evidence for their
*documented* functions only — no `FUN_` in those directories happens to sit next to a
compile-unit or single-class-header string.

### Spot-check: 6 anchored functions, decompiled and checked against the inferred file

| Profile addr | Anchor (dir/file, confidence) | What the code does | Plausible? |
|---|---|---|---|
| `0x18008e320` | Save/SaveLoad.cpp, high | `MemAlloc(0x70,8,"Save\SaveLoad.cpp",0x27)` then builds a `Video::ImageBuffer` and calls `CopyToTexture` under the video device lock | **Yes.** A save-thumbnail/screenshot capture helper — exactly what `SaveLoad.cpp` should contain. |
| `0x18006d190` | IO/Persistence.cpp, high | Loops over save chunks matching a chunk-id array; hits `Assert("_saveInfo[j]._length == chunks[i]._dataLength", ..., "IO\Persistence.cpp")` and `Warn("Chunk not found!", "IO\Persistence.cpp")` inline, then calls `System::Thread::Start` | **Yes.** A background chunk-loader thread body for `Persistence` — matches the class exactly (also independently corroborated by two inline literals, not just the anchor). |
| `0x180059b50` | IO/BinaryEncoding.cpp, high | `MemAlloc(0x30,4,"IO\BinaryEncoding.cpp",0x19)` then `IO::BinaryReader::BinaryReader(...)` | **Yes.** A `BinaryReader` factory — `BinaryEncoding.cpp` is the encoding class that would own this. |
| `0x18009a2a0` | System/Memory.h, medium | Bounds-checks an index against `_tableSize` (assert literal is `System/Memory.h`) and returns `index*0x108 + base` | **Yes.** A page/bucket-table indexer — genuinely `MemoryAllocator` internals. |
| `0x180034400` | Video/VideoConfiguration.h, medium | Touches **both** `Video::Device::_singleton` and `Video::Configuration::_singleton` (two different assert literals, one per header), writes a `Configuration` field, calls `Video::Device::ValidateConfiguration` | **Plausible but imprecise** — it's genuinely Video-namespace configuration-apply code, but the function straddles two sibling headers (`VideoDevice.h` and `VideoConfiguration.h`); "medium" is the right confidence, not "high". |
| `0x1800ecdb0` | Utility/STBImage.cpp, high | `MemAlloc(...,"Utility\STBImage.cpp",0xc1)` then a pixel-format expansion loop (palette index → RGB/RGBA bytes) | **Yes.** Textbook STB image-decoder channel-expansion routine. |

**6/6 dir/file inferences are plausible from the code**, including one (`0x18006d190`) that was
independently corroborated by two more inline string literals beyond the anchor itself. The one
imprecise case (`0x180034400`) is a real ambiguity between two sibling headers of the same class
family, not a wrong inference.

### Does any of this bridge to retail? Essentially no.

For the 66 `high`/`medium` functions:

- **Hash match against `profile_to_retail.csv`: 0/66.** None of the anchored addresses appear in
  the 191-address unnamed-hash-match set from Part 1. This makes sense in hindsight: functions
  worth string-anchoring are the non-trivial ones (codecs, parsers, thread bodies) — precisely
  the functions whose codegen is *most* sensitive to compiler/optimization differences between
  the profile and retail builds, so they're the least likely to hash-match byte-for-byte.
- **Per-function fuzzy match (`find_similar_functions_fuzzy`, threshold 0.5) against all of
  `Runtime-x64.dll`: 6/66**, and all six are weak (score 0.52–0.58). One of the six
  (`0x1800ecdb0`, the STB pixel-expansion routine above) matched retail `0x1800e05c0`
  = `Audio::SoundBuffer::PlatformSerialize` at score 0.52 — **confirmed a false match** by
  decompiling both sides: `PlatformSerialize` reads/writes sound-buffer byte arrays through a
  `Serializer`, nothing like the STB pixel loop. The other five scores are in the same weak band
  and should be treated the same way (unconfirmed, likely spurious) until individually
  decompiled — `fun_pool_candidates.csv` marks all six `WEAK, needs manual decompile
  confirmation before any rename`.
- Net: **at most 4 of retail's 2,467 `FUN_`-named internals (0.16%) get even a weak candidate
  name from this pass**, and zero get a confirmed one.

### Answering the question directly

A string-anchor pass on the profile build gets you real, spot-checked-plausible **file-level**
attribution for 66 profile-side functions (plus low-value template-noise "touches" for another
1,140+), covering 9 of 18 recovered directories. It **does not** meaningfully close PLAN.md
§4.3 risk 4 (the 2,467-function, 407 KB retail `FUN_` pool) — both hash and fuzzy matching fail
on almost this whole set, because the functions worth anchoring by string are exactly the ones
whose machine code differs most between the two builds. The lever that could still work:

1. **Call-graph propagation from named callers.** Every anchored profile function has a known
   caller set; if a caller is itself matched to retail (via Part 1's map) and that caller's
   retail call site still targets a `FUN_`, the callee at that call target is a strong same-slot
   candidate — this sidesteps hash/fuzzy entirely and uses structure instead of bytes. Not
   attempted here (out of scope for a read-only data pass — it needs `get_function_callers`/
   `get_function_callees` on both programs plus the existing map, cross-referenced); worth a
   dedicated follow-up.
2. **The profile PDB** (`Runtime-x64-profile.pdb`, referenced by the profile DLL's CodeView
   record per PLAN.md §3.4) may carry full symbol names Ghidra's analysis didn't recover as
   plain strings — worth checking whether it's on disk and importable, before spending more
   effort on string/hash/fuzzy heuristics.
