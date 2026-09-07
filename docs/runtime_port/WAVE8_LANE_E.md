# Wave 8 — Lane E — `Application` + `Save::SaveLoad` remainder + `Utility::PerlinNoise`

Branch `wave/upgrade-path-complete`. Files owned by this lane:
`runtime/src/application/application.cpp`, `runtime/src/application/application_win.cpp`,
`runtime/src/application/rt_application_port.cpp`, `runtime/types/application/Application.h`,
`runtime/src/utility/perlin.cpp`, `runtime/src/utility/rt_utility_port.cpp`,
`runtime/types/utility/PerlinNoise.h`, this document, plus two small additions to
`runtime/src/save/saveload.cpp` (Wave 5 lane C's file — address-registering two functions
it already ported) and defect fixes in `engine/funcs/engine_funcs.cpp`/`.h` (PLAN §6 step 8).

**Result: 6 of 13 named functions ported and newly registered this wave** (5 `Application`
members + `PerlinNoise::Generate`), **2 more (`Save::SaveLoad::CheckMod`/`SetSaveName`)
already ported by Wave 5 and now closed out** (real names fixed at their only call site,
double-registered by address), **4 correctly held back with written reasons** (`~SaveLoad`,
`Draw`, both inherited unchanged from Wave 5), and **1 data export** (`Application::_singleton`,
already wired correctly, one stale comment fixed). The `PerlinNoise` constructor is not in
this lane's function list and is not ported (see §3). `tools/export_name_audit.py`: **3 → 1
phantom names** (the remaining one, Audio's `PlayEffect`, is lane C's). `tools/abi_retslot_audit.py`:
**0 suspicious of 48**. All five new/edited files compile warning-clean at `-std=c++17 -Wall
-Wextra` at both `-O0` and `-O2` with the project's exact include set.

Not run by this lane, by instruction: the game, `make`, `tools/rt_run.sh`, `sa-run`,
`git commit`. Every runtime number below is a *prediction*, not a measurement.

---

## 1. The function table

| Export | Retail | Ord | Ported | Oracle | Diff site |
|---|---|---|---|---|---|
| `?IsPaused@Application@1@QEAA_NXZ` | 0x18009f4e0 | 3150 | **yes** | full replay | `Application::Application::IsPaused` |
| `?PlatformUpdate@Application@1@AEAA_NXZ` (private) | 0x18009fdf0 | 3410 | **yes** | none (pumps the live queue) | `…PlatformUpdate [no oracle…]` |
| `?Quit@Application@1@QEAAXXZ` | 0x18009f4d0 | 3437 | **yes** | scratch-copy diff (0x29-byte prefix) | `Application::Application::Quit` |
| `?SleepThread@Application@1@QEAAXH@Z` | 0x18009f850 | 4069 | **yes** | none (blocks the thread) | `…SleepThread [no oracle…]` |
| `?YieldToThreads@Application@1@QEAAXXZ` | 0x18009f920 | 4568 | **yes** | none (yields the thread) | `…YieldToThreads [no oracle…]` |
| `?_singleton@Application@1@0PEAV11@EA` (DATA) | 0x1801fc678 | 4800 | n/a | — | already wired via dispatch slot; stale comment fixed |
| `??1SaveLoad@Save@@QEAA@XZ` | 0x1800c3c90 | 666 | **no** (Wave 5 hold-back, unchanged) | — | — |
| `?CheckMod@SaveLoad@Save@@QEBAHHH@Z` | 0x1800c1090 | 1691 | **yes** (Wave 5), name+addr fixed this wave | full diff | `Save::SaveLoad::CheckMod` |
| `?Draw@SaveLoad@Save@@…` | 0x1800c2eb0 | 2260 | **no** (Wave 5 hold-back, unchanged) | — | — |
| `?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z` | 0x1800c3bb0 | 3953 | **yes** (Wave 5), name+addr fixed this wave | scratch-copy diff w/ refcount compensation | `Save::SaveLoad::SetSaveName` |
| `?_singleton@SaveLoad@Save@@0PEAV12@EA` (DATA) | 0x1801fc6f0 | 4817 | n/a | — | Wave 5, unchanged |
| `??0PerlinNoise@Utility@@QEAA@MMHH@Z` | 0x180108c70 | 305 | **not in this lane's scope** (see §3) | — | — |
| `?Generate@PerlinNoise@Utility@@QEBAMMM@Z` | 0x1801089b0 | 2396 | **yes** | full replay, tolerance (libm) | `Utility::PerlinNoise::Generate` |

Every ported export is registered **both by name and by address**
(`RegisterRuntimeAddr` alongside `RegisterRuntimeExport`), per WAVE8.md §4/WAVE7.md §3.

---

## 2. The vtable question

Neither `Application::Application` nor `Utility::PerlinNoise` exports a `??_7…6B@` symbol
(WAVE8.md §2's pre-wave measurement, 147 total, none for either class). Both were also
**verified from the ctor's first store**, not assumed from the absent export:

* **`Application::Application`** — the ctor is not in this lane's function list (no mangled
  export of its own; found by following the xrefs to `_singleton`, at 0x18009f590). Its first
  store is `MOV qword ptr [RCX],R8` with `R8` zeroed just before — a plain zeroed qword, never
  a `LEA` of a `??_7…` symbol. Not polymorphic.
* **`Utility::PerlinNoise`** — `??0PerlinNoise@Utility@@QEAA@MMHH@Z` (0x180108c70) opens with
  `MOVSS dword ptr [RCX],XMM1`, a float store, never a vtable `LEA`. Not polymorphic.

`Save::SaveLoad` was already settled by Wave 5 (WAVE5_LANE_C.md §4): the ctor writes an int at
`+0x00`, not a vtable pointer, and the DLL exports no `??_7SaveLoad@Save@@6B@`. Not re-derived
here, only cited.

**No detour was installed for any class in this lane.** Nothing here caches derived state that
the DLL's own internal callers and this port's callers could disagree about — `Application`'s
members are single-byte reads/writes and two are pure OS calls; `PerlinNoise::Generate` is a
pure function of its arguments. WAVE8.md §1's "every lane must state which of the two reasons
applies" is answered: no `??_7` export for either class, confirmed from the ctor's first store
in both cases, and no detour needed because nothing here is process-lifetime shared state a
second implementation could desync.

---

## 3. `PerlinNoise::PerlinNoise` — explicitly NOT ported, and why that's not a gap

The ctor is fully reversed (see `PerlinNoise.h`'s banner) and its body is reproduced, unregistered,
as `PerlinNoise_Ctor` in `perlin.cpp` purely so the derivation is not lost. It is not registered
because:

1. It is **not in `docs/runtime_port/wave8_lanes.txt`'s Utility list** — only `Generate` and the
   ctor's own line are there, and the ctor's line (0x180108c70) was assigned to this lane, but
   nothing in our tree constructs a NEW `PerlinNoise` — the class's only live use this project's
   own callers make is through an already-constructed instance's `Generate`. A `RegisterRuntimeExport`
   with zero callers is the exact `reference_uncalled_port_is_not_verified.md` trap: it would
   report `calls=0 divergences=0` forever, which reads as clean and proves nothing.
2. There is no meaningful oracle for a four-store ctor with no singleton to order around (unlike
   `Save::SaveLoad`'s ctor, which has `_singleton` publication to order against) — a scratch-copy
   diff would work, but building one to verify a function nothing calls is effort spent proving
   a fact nobody needs yet.

If a future lane needs to construct a `PerlinNoise` (e.g. porting the map-generation caller),
`PerlinNoise_Ctor` in `perlin.cpp` is already correct and just needs a `RegisterRuntimeExport` line.

---

## 4. Oracle discipline, argued per function

**`IsPaused`** — const, one byte read, no side effects. **Full replay.**

**`Quit`** — one byte store (`this[0x28] = 1`). **Scratch-copy diff, but restricted to the first
`sizeof(Application)` (0x29) bytes of the live object, not the whole object.** This lane does not
port the constructor and therefore does not know the class's true extent — only that it is *at
least* 0x29 bytes, because Quit's own target byte (`+0x28`) is inside a live object every time
this runs. Copying the prefix this lane's own struct describes is provably in-bounds; copying
more would not be. The prefix is compared byte-for-byte via `RT_DIFF_BYTES`, exactly like Wave
5's `SetOperation`/`SetMinOperationTime`.

**`PlatformUpdate`, `SleepThread`, `YieldToThreads`** — **no oracle, and WAVE8.md §5 says so
before any lane started work**: PlatformUpdate drains the live Win32 message queue with
`PeekMessageW(…, PM_REMOVE)`, so a second call would consume the *next* message, not the same
one; SleepThread and YieldToThreads block/yield the calling thread for real, so calling the DLL's
copy afterward sleeps or yields a second time. All three are registered anyway — each is short,
unambiguous, and every Win32 call is made in retail's exact sequence — and marked with
`RT_DIFF_SHADOW(true, …)` so they are counted (`calls=`) but never compared, the identical
pattern `??1SavedState@Save@@…` uses in Wave 5's `saveload.cpp`.

**`CheckMod`, `SetSaveName`** — unchanged from Wave 5 (full diff; scratch-copy diff with
refcount compensation, respectively). This lane's only change to their bodies is none; the
change is the two callers now reaching them (§5) and the address door added (§1).

**`PerlinNoise::Generate`** — the one function in this whole wave with **zero side effects and
zero shared state**: it reads only `*this` and its two float arguments and calls two `const`
helpers on the same object plus two transcendentals. **Full replay, `RT_DIFF_FLOAT` with a
non-zero tolerance**, and the tolerance is justified rather than silently widened:

* `Generate`'s two transcendental calls (0x180127560 = `powf`, 0x180126d10 = `cosf`) are
  Runtime-x64.dll's own statically-linked MSVC CRT — `runtime/funcs/rt_libm.h` already documents
  exactly this family for `sqrtf`/`sinf`/`cosf`/`atan2f` and its policy is unambiguous: **the
  default is OUR OWN libm** (`std::pow`/`std::cos`), and the DLL's copies are available only as a
  diagnostic (`BANISHED_RT_LIBM=dll`) that "does not survive cutover" — PLAN.md §4.1 decision 1
  requires the port to run with the DLL absent. A port that depended on the DLL's transcendentals
  to pass its own oracle would violate that on the first day the DLL is removed.
* `0x180126d10` was **already** named `kDllCosf` in `rt_libm.cpp` by an earlier wave (found from
  `Transform2`'s ctor and `Vector3::Heading`). This lane's own disassembly of `Generate` reaches
  the identical address independently — a cross-check, not a new discovery.
* `0x180127560` was **not** in `rt_libm` before this lane. Decompiling it shows the classic
  fdlibm-shaped branchy `ieee754_pow` (exponent/mantissa special-case ladder, `FUN_18012a940` for
  the NaN/Inf/domain-error tail) — confirmed as `powf`, added as `RtLibm::Pow` /
  `kDllPowf = 0x180127560`, same pattern as the other three.
* **The tolerance is therefore attributable to libm, not to this port**, and is checkable: running
  the same session with `BANISHED_RT_LIBM=dll` routes `RtLibm::Cos`/`RtLibm::Pow` through the DLL's
  own copies; if the divergence disappears under that flag, libm was the whole story. This is the
  same "1-ULP residual" family WAVE8.md's own pre-wave control-run note already expects to reappear
  once a long enough run reaches it. Tolerance used: `|ours| * 5e-7 + 5e-7` (≈4 ULP at float
  precision) — the guard is generous on purpose so a *real* offset or formula defect (which would
  produce a divergence orders of magnitude larger, since it changes which lattice cell is sampled)
  still trips it.

`SmoothNoise` (0x1801085c0, `?SmoothNoise@PerlinNoise@Utility@@AEBAMHH@Z`) is `private`/`AEBA`,
not called by our tree directly (only by `Generate`), and is not in `wave8_lanes.txt`'s Utility
list — reproduced in `perlin.cpp` as an internal helper, unregistered, the same way Wave 5's
`CheckMod` needed `IO::Modification`'s layout without owning that class.

---

## 5. Defects found and fixed at their source

**Two phantom mangled names in `engine/funcs/engine_funcs.cpp`, confirmed already diagnosed by
Wave 5 (WAVE5_LANE_C.md §1) but never fixed.** `?CheckMod@SaveLoad@Save@@QEAAHHI@Z` and
`?SetSaveName@SaveLoad@Save@@QEAAXV?$String@D@System@@H@Z` do not exist in the shipped DLL
(confirmed again with `tools/export_name_audit.py`, which now reports the whole tree at 1
phantom instead of 3). Both `ResolveExport` calls had been dead code since they were written —
`if (!fn_…)` always took the exe-dispatch-slot fallback, which happened to be registered under
the *correct* name by `standalone/address_registry.cpp`, so nothing was ever functionally broken,
only fragile: deleting either `RegisterDispatchSlot` line (as the unregistered-slots wave was
routinely doing) would have silently nulled both pointers with no diagnostic. **Fixed at the
source**: both `ResolveExport` string literals now spell the DLL's real mangled names
(`QEBAHHH` / `AEBVString@System@@`); the exe-slot fallback is left in place as a genuine second
door rather than the only one. `runtime/src/save/saveload.cpp` now also registers both by
address (0x1800c1090, 0x1800c3bb0) — three doors onto the same function.

**A stale comment, not a live defect**: `engine_funcs.cpp:938` and `engine_funcs.h:383` both
claimed `Application::Application::_singleton` lives at `0x1801feb80`. It does not — the ctor's
own disassembly (`MOV qword ptr [0x1801fc678],RCX`) and the pefile export directory both say
`0x1801fc678`, which is what `standalone/address_registry.cpp:1530`'s dispatch-slot registration
actually resolves. The wrong number was never read by any code path (the real address was used
correctly at the one call site that matters); fixed as a comment correction only, no behavior
change.

**Nothing else in PLAN.md, CLAUDE.md or the memory notes was disproved by this lane.**

---

## 6. Verification run

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/export_name_audit.py
  643 distinct ResolveExport names; 1 are NOT in Runtime-x64.dll  (was 3 pre-wave)

tools/ghidra_mcp/.venv/Scripts/python.exe tools/abi_retslot_audit.py
  0 suspicious of 48 by-value-returning exports

tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py
  Application   26 total   6 called   5 ported   83.3%
  Save          37 total  18 called  15 ported   83.3%   (called dropped 20->18: the
                two phantom-name string literals no longer appear verbatim)
  Utility       16 total   2 called   1 ported   50.0%
```

`C:\msys64\ucrt64\bin\g++ -std=c++17 -fno-strict-aliasing -Wall -Wextra -Wno-cast-function-type
-include standalone/env_cache.h` with the project's exact `SA_INCLUDES`, at both `-O0` and
`-O2`, on: `application.cpp`, `application_win.cpp`, `rt_application_port.cpp`, `perlin.cpp`,
`rt_utility_port.cpp`, plus the edited `saveload.cpp` and `rt_libm.cpp` — **all warning-clean**.

---

## 7. What the integrator must wire

* `void RtApplicationPortInit();` and `void RtUtilityPortInit();` declarations into
  `runtime/funcs/rt_port.h`'s wave-init list, and both calls added to
  `standalone/resolve_standalone.cpp`'s `RuntimePortInit()` — this lane did not touch either
  file, per instruction.
* `runtime/src/application/application_win.cpp` needs `-luser32` on the link line (already
  present in `SA_LIBS` for other `_win` files) — no new library required.
* Kill switches: `BANISHED_RT_APP=0` (Application namespace), `BANISHED_RT_UTILITY=0`
  (Utility namespace). `BANISHED_RT_SAVE`/`BANISHED_RT_SAVELOAD` are unchanged from Wave 5.
* `BANISHED_RT_LIBM=dll` is a pre-existing diagnostic knob (not new to this lane) — worth
  running once during integration specifically on `Utility::PerlinNoise::Generate`'s diff site
  to confirm any observed residual is libm and not a formula defect, per §4.
