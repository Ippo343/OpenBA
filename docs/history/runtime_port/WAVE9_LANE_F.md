# Wave 9 — Lane F: retire the fallback idiom, and triage the §2.2 patches

Produced 2026-09-07 on `wave/upgrade-path-complete`. Nothing was built and the game
was not run (lane R owns the exe and the game directory); every claim below is from a
static tool, a `-fsyntax-only` pass, or a read of the shipped DLL's export directory.

---

## Half 1 — the 163 guarded fallbacks are gone

`tools/resolve_fallback_audit.py` now reports

```
0 guarded ResolveExport->ResolveAddr fallbacks; 0 do NOT point at their own name
... exit 0
```

**0 sites, which is the gate. 0 defects was never the gate**, for the reason
[WAVE9_LANE_D.md](WAVE9_LANE_D.md)'s addendum states: the audit answers "is this
address the entry point of its own name" by reading the shipped DLL's export
directory, and lane G deletes that DLL. On that day the checker stops being able to
answer its question at exactly the moment the fallbacks stop being dead — with no DLL
no `ResolveExport` can succeed, so all 163 become the live path **simultaneously** and
jump at addresses in a module that is not mapped. The replacement for the checker is
leaving nothing to check.

### The batches, and the count after each

| Batch | Mechanism | Sites removed | Audit after |
|---|---|---|---|
| 0 | (start) | — | **163** sites, 0 defects |
| 1 | The primary names a **registered port** — `RegisterRuntimeExport` already puts our body in front of the DLL, so `ResolveExport` cannot return null and the branch is dead twice over. This is how lane R retired two of lane D's five defects. | **137** | **26** sites, 0 defects |
| 2 | The primary is still **DLL-only**. Same deletion, but this is the set where a null after the deletion is a real event, so it is what the policy below is for. | **26** | **0** sites, 0 defects |

The count fell by exactly the number removed at each step, and the defect count stayed
0. That check is not ceremony: **a site *vanishing* from the audit's view is as bad as a
defect, and this project has already had that happen** — a 12-line explanatory comment
pushed a guard outside the checker's fixed line window and the site silently disappeared,
166 → 162, reading as a clean pass. My own first deletion pass hit the same shape from
the other side: a 14-line lookback window found 162 of 163 blocks, and the one it missed
was `RadioElement::FindCheckBoxes` — missed **because of the 14-line comment lane R wrote
while fixing it**. Documenting a fallback is what hides it. The window was widened to 30
lines and the block count then matched the audit exactly, 163 for 163.

### What was in the 163

* **149 of 163 are in one file**, `engine/funcs/engine_funcs.cpp` — the typed `fn_*`
  pointer table. The remaining 14 are scattered across `code/Game/` (7), `standalone/`
  (3), `dx11/` (1) and three others.
* Every deleted block contained **only** the guard, the assignment, and comments —
  verified mechanically (no line inside any of the 163 blocks carries a `;` other than
  the `ResolveAddr` statement itself), so no deletion dropped an unrelated statement.
* No block was guarded by an `else` arm, so no deletion left a dangling `if`.
* The 26 in batch 2 are worth naming, because they are disproportionately the wave's
  known holdbacks: `ExternalFactory::FindOrLoad`, `ExternalFactory::Clean`,
  `External::SaveText`, `KeyMap::Load`, `Audio::Scene::Update`, `Input::Device::Update`,
  `UpdateLoop::LoadResources`, `~SaveLoad`, `~DialogInstance`, `~Context`. Those are
  exactly WAVE9.md's rows 2-4, 11, 16, 18, 21-22, 26-28.

### Comments that the deletions made false

Seven comment blocks in `engine_funcs.cpp` said some variant of *"only the ResolveAddr
fallback below kept this non-null"*. All seven were **historically true and are now
about something that no longer exists** — each records a Wave 7/8 phantom-mangling fix
where the primary really did resolve null for a while. They were rewritten in place to
past tense naming this lane, rather than deleted: the phantom-name history is the reason
the sites are interesting.

Three deletions removed a block whose comment was the **only surviving record of a real
defect**. Those were re-added, condensed, above the surviving `ResolveExport`:

| Site | What the record is |
|---|---|
| `?FindCheckBoxes@RadioElement@UI@@QEAAXXZ` | The fallback named the **wrong member of an overload set** — `0x1800d5750` is the private 1-arg sibling, a genuine entry point that would not have faulted, just run radio-group tree surgery with whatever the caller left in RDX. This is why "is the address an entry point" was never a sufficient check. |
| `?GetPath@External@Object@@…` | `0x18006a3e0` is mid-body of `FUN_18006a390`. |
| `?OnVideoError@MainLoop@Engine@@…` | `0x1800dcc00` is an address **inside OnVideoError's own body** (`0x1800dc5e0..0x1800dcd12`). |

### One self-inflicted finding, recorded because it is the recurring shape

Writing the policy comment below **broke `tools/export_name_audit.py`**: it harvests every
`ResolveExport` call with a string-literal argument, and a worked example in a comment read
to it as a 663rd symbol the DLL does not export — 662 names / **1 phantom**, exit 0 but with
a fake row. Fixed by spelling the example without a string literal, and the audit is back to
**661 distinct names / 0 phantoms**. It is the same shape as the comment that deleted a live
site from `resolve_fallback_audit.py`'s window: *the documentation of a checker is inside the
corpus that checker reads.* Both directions are now noted in the respective files.

---

## The null-resolve policy

Deleting the fallbacks leaves the question they pretended to answer. `ResolveExport`
returned null **silently** — that is `reference_resolveexport_phantom_names.md`, six found
by Wave 2, two re-diagnosed by Wave 5 and fixed again by Wave 8. Until now the fallback was
what stood between a null and a crash, badly.

**The policy: name it loudly, at resolve time, and keep going.**
Implemented in `standalone/resolve_standalone.cpp` (`RecordResolveMiss`,
`DumpUnresolvedSymbols`), declared and argued in `standalone/resolve.h`, dumped from
`standalone/main.cpp` beside `DumpRuntimeResidue`.

* **Loudly** — one `[RtUnresolved] <symbol> (asked by <caller>)` line on stderr per
  *distinct* symbol, at the miss. The alternative is a null function pointer called much
  later from a body with no idea what it wanted: this project's `EXEC-of-0` crash logs,
  which name an address and nothing else. The line lands in the log *above* the crash.
* **At resolve time, not at first call** — most of these are filled by `EngineFuncsInit`
  at boot and never re-checked.
* **And keep going, not abort.** This is the part that decides the policy, and it is a
  cutover argument: **on cutover day every unported symbol misses.** Aborting on the first
  one prints one name and teaches nothing; running to the end prints the complete missing-
  symbol list, which is precisely the work list lane G needs. `BANISHED_RT_FATAL_RESOLVE=1`
  opts into fail-fast for the narrower "this run should have resolved everything" case.
* **Always on, not gated on `BANISHED_RT_STRICT`.** Strict mode measures requests that
  *reach* the DLL, a different question; a symbol that resolved to nothing is a defect in
  any run whether or not anyone is measuring. The cost is one map insert on a path that in
  a healthy pre-cutover run is never taken, and `DumpUnresolvedSymbols` prints nothing at
  all when the map is empty — so it cannot pollute a residue measurement.

It does **not** replace `tools/export_name_audit.py`. That catches a phantom mangling from
source before the process runs; this is the runtime half, and it is the half that keeps
working after cutover, when the export table the static audit reads is gone.

**Applied consistently:** both `ResolveExport` and `ResolveOrdinal` record. The three
`*Raw*` doors deliberately do **not** — they already have their own `[RtRawResidue]`
population with lane V's per-site verdicts, and a raw miss after cutover is expected by
construction (the oracle doors die with the DLL), so recording them here would put ~100
known-and-verdicted rows into a list whose whole value is that it is empty.

---

## Half 2 — the §2.2 coupling hazards

**Nothing in this half was deleted.** Every item is either load-bearing today or
self-retiring at lane G, and one of them is mis-described in PLAN.md in a way that would
have caused real harm if the list had been executed literally. A written carry is the
result here.

| # | Hazard | Verdict | Reason |
|---|---|---|---|
| **A1** | `standalone/allocator_lock.cpp` — 13/12-byte detours on `MemoryAllocator::Allocate` / `Free` | **CARRIED — LANE E OWNS IT, DO NOT TOUCH** | Default-on, load-bearing, and lane E moved its install *earlier* in boot this wave; the allocator shadow driver requires the lock. It retires when lane E takes the heap (WAVE9.md §3: the heap cannot be half ours), not before, and not by this lane. |
| **A2** | `standalone/filename_runtime_hook.cpp` | **CARRIED — AND PLAN §2.2 IS WRONG ABOUT IT** | PLAN calls this file "diagnostic detours". Two of its four installers are diagnostics (`InstallFilenameRuntimeHook` gated `BANISHED_FNHOOK`, `InstallReadGuard` gated `BANISHED_READ_GUARD`, both default OFF). **`InstallFindOrLoadWatch()` is DEFAULT ON and is labelled "THE MENU CRASH FIX"** (`standalone/main.cpp:1735-1747`): one global recursive CS taken at both `ExternalFactory::FindOrLoad` and `External::Load`, because the shipped pair is not thread-safe and the Runtime background loader races the main thread — 0 crashes / 24 with it, ~33% without. **Deleting this file as "diagnostics" would reintroduce an intermittent menu heap crash.** At cutover the *lock must move into the port*, and note that `FindOrLoad` and `Clean` are WAVE9.md rows 27-28, still held back for having no possible oracle. See "must not be deleted" below. |
| **A3** | `standalone/video_interface.cpp` — detours `Video::Device::InitializeInterfaces`, writes the Device interface slots | **CARRIED TO LANE G** | This is what makes `dx11/` a replacement rather than a patch: it fills interface slot 1 with our two exports before the DLL's own loader gets there, so `VideoDX11-x64.dll` is never loaded. It is not a diagnostic and cannot go early. It retires exactly when `Video::Device::InitializeInterfaces` (`0x1800f79f0`) and `CreateVideoInterface` (`0x1800f7930`) are ours — the port must then populate the slots itself. Deleting the detour without doing that leaves the renderer unbound. |
| **A4** | `MessageBoxW` IAT swap in `standalone/main.cpp:706-760` (and `HookRuntimeUser32Import` at `:811`, used for `SetWindowPos` / `ShowWindow`) | **CARRIED TO LANE G — then DELETE, and the purpose does not need a replacement** | Every one of these begins `GetModuleHandleA("Runtime-x64.dll")` and returns false/null when that is gone, so they self-neutralise rather than misfire. They should still be deleted at lane G rather than left as dead code. The `MessageBoxW` swap exists to catch the **MSVC CRT** `_purecall` R6025 modal raised from inside the DLL; with the DLL gone there is no MSVC CRT in the process, MinGW's `_purecall` does not raise a modal, and a null vtable slot 0 faults as `EXEC-of-0`, which the VEH handler already reports. The mechanism becomes obsolete, not unimplemented. |
| **B** | Direct reads of Runtime-internal layout | **CARRIED TO LANE C2 / LANE G — and all but one already have a destination** | `BaseTypeInfo` walk: verified clean this wave, field-for-field; the prospective risk is the `next` chain at mint time, which is lane C2's. The other six are **already covered by lane C2's mint**, which I checked name by name against `standalone/rt_data_mint_tables.inc`: `?_errorString@Error@System@@0PADA`, `?_frameIndex@ParticleInstance@Graphics@@1HA`, `?_singleton@MemoryAllocator@System@@…`, `?_singleton@FileSystem@IO@@…`, `?_singleton@Modification@IO@@…`, `?_firstTypeInfo@BaseTypeInfo@Object@@…` are all minted symbols. The lane-G action is therefore mechanical: **switch these consumers from `ResolveAddr(Module::Runtime, 0x1801f…)` to `ResolveExport("?…")`,** which is a no-op today (both reach the same DLL address) and picks up the mint at cutover. **I did not make that change**, deliberately: it moves rows in the `[RtResidue]` census from `addr:0x…` keys to name keys mid-wave, which would corrupt lane 0's matrix and lane V's verdict list while both are open — the same reason lane D deferred its five fixes one round. **The one gap: `0x1801fc6d0`, the Modification `HMODULE` that `code/Game/Game/game_main.cpp:355-357` calls `FreeLibrary` on, is UNEXPORTED and is NOT in the mint** (the mint has `?_singleton@Modification@IO@@` at `0x1801fc6b0`, which is a different slot). Lane D's correction 2 says "mint 8 bytes"; that has not happened. **This is an open item for lane C2.** |
| **C** | Crash handler's Runtime instruction offsets, `standalone/main.cpp:541` (`+0xC0390…A0`) and `:559` (`+0xBC73E`) | **CARRIED TO LANE G — the lead's reasoning checks out against the code** | PLAN §2.2 C is the worked example of a "retire this" that was wrong: Wave 3 corrected it to KEEP-narrowed because our registrations move only *our* callers and the DLL's own internal callers still execute those addresses. **At cutover that reason evaporates**, and the code confirms it mechanically rather than by argument: both blocks are guarded on `uintptr_t runtimeBase = GetModuleHandleA("Runtime-x64.dll")`, which returns null once the DLL is gone, so neither block can fire and neither offset has a base to be added to. They become unreachable, and lane G should delete them rather than leave them. **One repair made here:** only the `+0xC0390` block carried the scope note; the `+0xBC73E` block had none, so a lane G that grepped for the note would have retired one and left the other. The note is now on both. |
| **D** | Vtables copied out of live Runtime objects — `thumb_vtables.cpp:66-82`, `statusicon_geometry.cpp:328`, `saveloaddialog.cpp:854`, `frustumoutlinedescription.cpp:89` | **CARRIED TO LANE G — all four CAN now source from the mint** | Checked per site against `rt_data_mint_tables.inc`. Every base class these four stamp from is a minted `??_7`: `??_7MaterialInstance@Graphics@@6B@` and `??_7SpriteSheet@UI@@6B@` (thumb_vtables), `??_7Geometry@Graphics@@6B@` (statusicon), `??_7DialogLoaderLoopT@Engine@@6B@` (saveloaddialog), `??_7Element@UI@@6B@` (frustumoutline). So the replacement exists: read the minted table by name instead of copying slots off a constructed object. **They are also, usefully, self-healing** — `thumb_vtables.cpp`'s `Arm()` copies slots 1..8 from whatever vtable the ctor installed, and at cutover the ctor is ours installing the minted table, so the copy still yields the right slots. That makes this the *low*-risk item in §2.2, not the high one PLAN implies. Sourcing by name is still better, because a copy-from-object cannot fail loudly and a missing minted symbol can. |
| **E** | Dead-code claims | **CONFIRMED, still true** | `runtime/src/ui/spritesheet_debug.cpp` is gone (file absent). `sprite_debug.cpp` and `element_sprite_debug.cpp` are **LIVE** — reached through bare `extern` declarations from `buildui.cpp:23-24`, `buildrequirementui.cpp:29-30` and `toolbardialog.cpp`, which a filename grep misses; `Debug_Sprite_Ctor` / `Debug_Sprite_DefaultCtor` have real call sites. `runtime/funcs/runtime_funcs.cpp` still resolves nothing (12 lines, zero `ResolveExport`/`ResolveAddr`). |

### What must NOT be deleted

Three things on the "retire the §2.2 patches" list would cause real harm if executed
literally, and each is on the list for a reason that is no longer (or was never) accurate:

1. **`InstallFindOrLoadWatch()` in `filename_runtime_hook.cpp`.** PLAN files the whole file
   under "diagnostic detours". It is not: this installer is default-on and is the fix for an
   intermittent menu heap crash caused by a genuine data race between the main thread and the
   Runtime loader thread. The lock has to survive into the port of `FindOrLoad` / `External::Load`,
   and those are still held back with no oracle.
2. **`allocator_lock.cpp`.** Lane E's, load-bearing, and its shadow driver depends on it. Reported,
   not touched.
3. **`video_interface.cpp`.** Not a diagnostic; it is the mechanism that keeps `VideoDX11-x64.dll`
   unloaded. Its job has to move into the `InitializeInterfaces` port before the detour goes.

---

## Tool state at the end of this lane

| Tool | Result |
|---|---|
| `tools/resolve_fallback_audit.py` | **0 sites, 0 defects, exit 0** — the lane's gate, met |
| `tools/export_name_audit.py` | 661 distinct names, **0 phantoms**, exit 0 (was 662/1 because of this lane's own comment; fixed) |
| `tools/port_bypass_audit.py` | 0 on both doors, exit 0. The address door drops 352 → **189** sites and the `GUARDED-FALLBACK=163` bucket is gone, which is this lane's deletions seen from the other tool |
| `tools/runtime_census.py` | 4632 total / 843 called / 586 ported / **69.5%**, exit 0 (moved by other lanes, not by this one — deleting a dead branch changes no registration) |
| `tools/rawdoor_audit.py` | 29 UNGUARDED-CALL when I ran it — **lane R's number, moving under me; not claimed here** |
| `g++ -fsyntax-only` | 0 errors on all 11 files this lane edited. **This is not a build** — no artifacts, exe and game directory untouched, per the lane constraint. The integrator still builds. |

## For the integrator

* The deletions in `engine/funcs/engine_funcs.cpp` and the ten other source files, plus the
  first version of the `resolve_standalone.cpp` policy code, were swept into commit `6d0f6d5`
  by the integrator while in flight. The tree is coherent — the audit reads 0 and all files
  pass `-fsyntax-only` — but the commit message does not describe them.
* One behavioural change ships in this lane: `[RtUnresolved]` lines on stderr. In a healthy
  pre-cutover run there should be **none**, and `DumpUnresolvedSymbols` prints nothing at all
  when the list is empty, so an existing residue or diff measurement is unaffected. If a run
  *does* print one, that is a real missing symbol and predates this lane — the fallback was
  hiding it.
