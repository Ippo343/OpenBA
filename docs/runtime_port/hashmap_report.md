# Runtime profile ↔ retail function map — Wave 0 tooling

Produced 2026-09-06 against the live Ghidra project (`Runtime-x64-profile.dll`, ToolKit build,
and `Runtime-x64.dll`, retail). Read-only: no rename/set/create/delete calls were made against
either program.

## Tooling note (bridge bug, worked around)

`bulk_fuzzy_match` and `find_similar_functions_fuzzy` are declared to return `str` but the
bridge (`tools/ghidra_mcp/bridge_mcp_ghidra.py`) implements them with `safe_get()` (which returns
a `list[str]`) instead of `safe_get_json()` (which returns `str`). Every call through the MCP
tool interface fails Pydantic's output validation (`Input should be a valid string`), regardless
of parameters — confirmed with several address/offset/limit combinations. The underlying Ghidra
HTTP endpoint (`http://127.0.0.1:8089/bulk_fuzzy_match?...`) is unaffected and returns clean JSON;
all bulk fuzzy matching below was done with direct HTTP calls (Python `urllib`) to that endpoint,
paging with `limit=500`. This is a one-line bug in our own bridge script, not a Ghidra data
problem; flagging it here rather than fixing it since the task was read-only/data-only.

## Deliverables

- `docs/runtime_port/runtime_hash_index.json` (9.4 MB) — normalized-opcode-hash index over both
  Runtime programs, `filter="all"`. Tool summary: `programs_scanned=2, functions_indexed=30861,
  unique_hashes=8357, duplicates_found=2626`.
- `docs/runtime_port/profile_to_retail.csv` — 9,921 rows, columns
  `profile_addr,profile_name,retail_addr,retail_name,score,method`.
- This report.

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
