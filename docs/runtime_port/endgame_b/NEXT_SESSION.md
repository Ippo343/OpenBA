# Next session plan (written 2026-09-19, at the B.3.4 close)

State and counts live in `../STATUS.md`; this file is only the plan. Delete it (or replace
it) when the session it describes has started.

## 0. Sync (5 minutes)

- `git log --oneline -8`, `git worktree list`, `git branch --no-merged main` (expect none).
- Prune the merged agent worktrees under `.claude/worktrees/` (B.3.4 left ~30; check each is
  clean first; `git worktree remove` without `--force`, `git branch -d`) — they slow every
  unscoped grep and a stale one is where an agent's "zero hits" comes from.
- Greps over `standalone/` or `tests/` exclude `standalone/build*/` and `tests/unit/build/`.
- Open Ghidra (all four programs) BEFORE launching an agent that needs it. `switch_program`
  is GLOBAL state: every agent asserts `get_current_program_info` before each read batch
  (retail `Runtime-x64.dll` 15290 functions; the profile DLL, 16002, shares the image base and
  is never a source of offsets; `Application-x64.exe` 11473) and switches BACK.

## 1. Carry-overs (each its own gated change; none blocks step 5)

From B.3.3, still open (unchanged by step 4):
1. The IAT-cached `fn_InternalBasePtr_Ctor` / `_DeletePtr` callers in ten files
   (`placementtool`, `edittool`, `removenaturalresource`, `performwork`, `dropoffrawmaterial`,
   `work_act_perform`, `work_act_path`, `work_act_clear`, `pickup_gate`, `work_base_reimpl`).
2. One `?_typeInfo@` record per name (`Base`, `External`, `ExternalList`, `CompiledExternal`,
   `Internal`): owner task with the coordinator in the loop, needs `rt_data_mint.cpp` +
   `rt_mint_gen.py` and a live type-table comparison.
3. `ExternalList::Find`'s second parameter is `Object::Type` by value, not `uint32_t`.
4. A menu-driver route that opens Options (the 6C `GetTypeIdFromName` conversions have build
   coverage only).

New from B.3.4 (details and anchors in `STATUS.md`'s B.3.4 block and `B34B_RULES.md`):
5. **`System::MemoryAllocator` — deferred past step 4 on purpose** (~1,146 sites, three caller
   surfaces, member vs static bodies lock differently; B.4 already lists the allocator among
   what legitimately stays cast-heavy). Assessment: `B34B_RULES.md` §6.
6. **Three open defects, none guarded, all recorded with stacks:**
   (a) the allocator `LockEnter`-on-near-null crash (`Get()` returned null → `EnterCriticalSection(0x640)`;
   W2 teardown vs W2′ unlocked free-list wild write; discriminator = print `_singleton` in the
   crash dump — a small `standalone/` change, do it first);
   (b) `SaveLoadDialog_PopulateFileList` null deref on the load-a-save route (seen 2 in 7 under
   load, never since);
   (c) the AUDIO STREAM thread faulting in `IO::FileSystem::ReadFile` (`_packfiles[packIndex]`)
   while `Engine::Loader::Load` runs `Modification::SetupFileSystem` (package remount) on the
   caller thread — seen once in ~60 resave runs. Question for the Audio step: does retail park
   the music stream before a load, or skip the remount when the mod list is clean?
7. The eleven `CUTOVER_REQUIRED` System statics: `MintStorageCell` redirect owed at the
   class-static conversion (end of B.3), tabled per row in `B34B_RULES.md` §4.1; Timer's five
   move together or not at all.
8. `System::Configuration::CurrentBuildId` (0x1800c4c60) stays a free door until someone reads
   its prologue (it calls `String::Set` on an un-zeroed hidden return slot).
9. The 125 call sites through the TWELVE `ExternalPtr` exe-side doors (template instantiations
   ported as game-code bodies with their own guard set — NOT Runtime members): exe-phase,
   receiver-side work. `fn_ExternalPtr_Serialize3` is the last reader of exe 0x140007140.
10. Small, retail-read-shaped (`moddetailsdialog.cpp` Find-by-address is DONE, `94258ffc`): eight direction reads with an unproved
    receiver (listed in `d6ee7bfc`); `dx11/src/buffers.cpp:271` reads a stream direction but
    dx11 has no IO dependency; `standalone/res_load_lock.{h,cpp}`'s header rule is followed by
    exactly one body — decide live or dead.
11. System leftovers that are NOT oversights: about 150 `System::Time` ctor call sites (exe slot
    0x140152b00) and the `Random` ctor sites stay doors by design (B4b-2); eight by-value `String`
    returns write into an already-constructed `String`; `fn_Random_RandFloatTol` is an `extern "C"`
    SHIM FUNCTION (`code/Game/Game/worldgen_f390_shims.cpp`), not a pointer — its ~29 call sites are
    plain function calls and its body is the member call. The System guard-and-cache sweep
    and the dead System globals were done inside step 4 (`b34_session/SYSTEM_GUARD_BRIEF.md`).
    Still owed: `Time::AsSeconds` / the `Time` ctor reach `System::Timer` through
    `RT_EXPORT_ONCE` by name although `Timer::TicksToSeconds` / `SecondsToTicks` are now static
    members (a routing change, so it is its own gated commit).
12. DONE (`59ca40dd`): the `?_singleton@Configuration@System@@` row now names only `?Get@`
    as the blocker; disposition unchanged. The 2026-09-19 defect pass's owed routes (mods,
    tutorial, perf build, (d) with a conflict) were run the same day — results in `../STATUS.md`.

## 2. B.3 step 5 — `Video` and `Graphics` (dx11/ stays the plugin, implemented in-tree)

The session kit from B.3.4 — briefs, the guard-sweep spec, the binding-resolving sweep scripts,
the per-file warning A/B and the coordinator's gate script — is in `b34_session/` (README there).
Same shape that has now worked six times: read-only inventory -> committed rule sheet
`B35_RULES.md` -> ONE owner for the question pass and the core -> coordinator gates each
merge -> parallel blocks by DISJOINT file set -> caller batches by directory -> a closing
pass that is specced by its predecessor and fanned out.

### 2a. What is already measured (`VIDEO_GRAPHICS_INVENTORY.md` — a MAP, NOT EVIDENCE)
- Video: 7 type headers / 31 sources / 19 minted vtables. Graphics: 10 / 26 / 20. 18 of the 39
  classes have a `?_typeInfo@` row (so they sit over `Object::External`, converted in B.3.3);
  the rest (`Context`, `Device`, the `*Data` family, the Light siblings) have UNCONFIRMED bases.
- ~1,012 registrations (about double IO). Cutover manifest: Video 18 / Graphics 4 REQUIRED
  storage rows — not the same tally as the 39 vtable rows; do not conflate them.
- Callers: only 61 CALLs through `engine_funcs.h` doors, against 178 raw
  `ResolveExport("…@Video@@/@Graphics@@…")` sites and 28 files caching into local statics. An
  `engine_funcs.h`-anchored sweep would undercount by 4x. Hot spots: `minimap_paint.cpp`,
  `gameupdateloop*.cpp`, `Scene::AddGlobalObject/AddObject`, `MeshInstance`, `Camera`.
- Nine hand-inlined int64 slot calls on a raw `ReaderWriter*` live in the Graphics/Audio ports
  (`graphics/animationstate.cpp:329-331`, `graphics/vtbatch7_animgroup_particlesystem.cpp:324/616/638/639`,
  two in audio): receiver-side work; `IO::Serializer::Serialize(int64_t&)` now exists.

### 2b. Block 0 — the question pass (ONE opus owner, no ported code touched, into `B35_RULES.md`)
1. Read `dx11/src/dx11_backend_access.h` in full: the inventory found only two real includes
   of runtime type headers in dx11/ and suspects it DUPLICATES offsets locally — the
   duplicate-POD hazard B34_RULES §1.5.2 found for `IO::Stream`. Decide the boundary: which
   vtables are dx11's, which are Runtime's, who stamps what, and whether dx11 may include
   `runtime/types/video/*.h` (it already compiles with `-Iruntime/types`).
2. Per class, from the RETAIL DLL byte-wise (not the profile DLL, not decompiled C): slot list
   and order, base class, size, who constructs it and where its vtable is stamped (by name or
   BY ADDRESS), whether anything holds it BY VALUE. Known: `Video::Context` holds `Viewport`,
   `ConstantBufferOpaque`, `MatrixRaw`, `Vector4Raw` by value — B34's by-value rule applies.
3. File ownership: `videoprogram.cpp` vs `video_program.cpp`, `constant_buffer.cpp` vs
   `buffers.cpp`, and the eleven `waveNN_lane_gfx*.cpp` files that cut across classes.
   Blocks launch only once their file sets are PROVEN disjoint.
4. Threading and device-lost handling in `runtime/src/video` + `dx11/src` (not yet searched).
5. Re-read the three CLOSED Video/Graphics bugs in STATUS.md (`Device::InitializeInterfaces`
   LoadLibrary; `Graphics::Geometry` copy ctor through a NULL raw door; `GraphicsMesh::
   PlatformSerialize` registration stripped from the PERF build; a `Math::Matrix` returned BY
   VALUE from a vtable slot) — the same files will be converted, and the same three shapes
   (raw-door fallback, printf-stripped registration, by-value return through a slot) recur.

### 2c. Proposed blocks after block 0 (inventory §6 — confirm disjointness first)
1. Video small `*Data` family (proves the dx11 boundary on 3–7-slot classes).
2. Video program/shader family (8–9 slots, all `External` subclasses — mechanical after one).
3. Video `Context` / `Device` / `RenderState` / `ImageBuffer` (the big PODs; needs 2b.2).
4. Graphics mesh / geometry / scene / camera (the highest caller traffic — after the pattern
   is proven, not first).
5. Graphics material / animation / particle / light (absorbs the wave-lane files).
6. Renderer + `rt_factories_*` / `rt_*_port.cpp` stay with the owner.
7. Caller batches by directory, `code/Game/Game` first.

### 2d. Standing rules every prompt carries (B.3.3's list still holds; these are B.3.4's)
Class shape
- A POD that already OCCUPIES the retail name (it sat inside `namespace System`) moves to the
  `Rt*` namespace in the same commit that declares the class (B4b-1). POD namespace spelling
  is NOT uniform (`RtIO`, `RtIo`, `rt::io`, `RtSystem`): check the file.
- Abstract (slot 0 pure) unless a REAL class holds it by value; then concrete + protected
  `= default` ctor + `friend` the holder. A POD mirror holding the POD is not that case (B4b-3).
- No real constructor on a polymorphic class, nor where a by-value holder is seeded through a
  ctor DOOR (the `Random`/`InternalFactory` double-seed): assert it (B4b-2). Note
  `is_trivially_default_constructible` stops working as that assert once a real dtor exists.
- A non-polymorphic class may take its real ctor/dtor when every one has a ported, registered
  body and no by-value holder would double-release (WideString, ~FileSystem, ~Persistence).
- Assert `alignof` as well as `sizeof` (`uint64_t` for a FILETIME keeps 8 and changes 4->8).
  All-static classes carry no layout asserts and SAY SO in the banner.
- Doors delegate with a QUALIFIED non-virtual call on polymorphic classes (first executable
  proof: `tests/unit/system_abi_test.cpp`). A retail null-`this` test lives in the door.
- By-value `String`/`Filename`/`Matrix` returns are hidden-retslot doors: the door
  placement-news the member's prvalue into the caller's slot; a caller converts only as "the
  door body moved to the call site".
Name lookup traps (each compiled CLEAN)
- A BARE class name inside `namespace NS {` binds the class, not the POD (fired 3x in IO).
  Define member bodies at GLOBAL scope with qualified names when class and POD are
  field-identical. A global `using RtSystem::String;` still loses to `namespace System` inside
  a member body.
- Sweep FORWARD DECLARATIONS: GCC accepts `struct X;` for a class silently (B4b-5).
Finding callers — by BINDING, never by name or comment. Eight spellings, each cost a miss:
  1 `ResolveAddr(Module::Exe, slot)`; 2 helpers (`RSlot`, `ResolveSlotPtr<T>`, a file-local
  `ResolveSlot(addr, cache, tried, tag)`, `loadExref` lambdas, `ResolveFn<T>`); 3
  `ResolveExport` by mangled name — string literals INTACT, and wrapped over two lines; 4 a
  two-statement `uintptr_t` scratch local, resolved POSITIONALLY (`moveto.cpp` rebinds one ten
  times; a whole-file closure reported 136 calls where there were 5); 5 plain aliases and
  `uintptr_t` round-trips; 6 globals bound in `engine_funcs.cpp` and called elsewhere; 7
  handle-struct FIELDS (`g_h.x`); 8 inline `RT_EXPORT_ONCE(T, "…")(args)`.
- THE DOOR DECIDES THE OVERLOAD, never the argument's type: arguments are untyped `s + 0xNN`.
- `serializer + 8` is the ReaderWriter; nearly every hand-inlined site misnames it. A local's
  name proves nothing about what object it is (`work_base_walkpos.cpp:86` is a COUNT).
- An arity mismatch between a cache's typedef and its door is a finding, not a conversion
  (the Harvest/Plant bool-array alias could never have executed — settle from retail).
Process
- A queued message RESUMES a finished agent: send the predecessor an explicit STOP before
  launching its successor. Forbid `git stash` (shared across worktrees). Killing a queued
  lock `acquire` can orphan the lock: agents `release` after killing waiters.
- The game dir is ONE resource. With more than three parties it starves: agents run static
  gates only and the coordinator gates the merged tree. After `TaskStop` of a gate, release
  the lock by hand.
- File reservations for one agent are invisible to a directory sweep: list them in the sweep's
  final pass (two reserved files carried the last four guards into pass 2d).
- The warning A/B must run with the PERF flag set too (printf stripping is where new
  `unused variable` warnings come from), must materialise the base copy BESIDE the real file
  (relative includes), and must not run during a build (its temp file joins the source glob).
  The coordinator's warning diff only sees RECOMPILED files: a clean rebuild at the close.
- `standalone` (release) and `standalone-debug` SHARE `standalone/build/obj`. A true release
  compile (empty the obj dir first, or it compiles nothing) must be FOLLOWED by `make clean`:
  otherwise the next `standalone-debug` links a "debug" exe out of log-stripped -O3 objects
  (10 MB instead of ~60 MB), stdout stops after ~120 lines and every log-based gate check
  fails on a healthy run. Cost this session one false gate failure and a 14-file bisect.
- A closing pass is bigger than its brief: B.3.4's was 45 globals + 369 file-local caches +
  ~650 guards. Have the closing owner write a measured spec and fan it out by directory;
  the globals' own declarations die LAST, in one pass, so the build proves the invariant.
- A resave DIFF is classified by WHICH BYTES differ, never by size: only the count field at
  +0x18 (either direction) or one/two whole resident rows incl. a DUPLICATE row is the old
  loader race (`reftable_diff.py`; keep the DIFF image). After the single-loader fix
  (`96c17f47`) neither signature should appear at all: one appearing is now a finding.
  `[GUL::Load] redundant dispatch` reappearing means a second dispatcher is back.

### 2e. The gate for every merge (coordinator only)
debug + release + perf build exit 0 (`standalone` shares an obj dir with `standalone-debug`;
perf is the real NDEBUG build) · new-warning set diff vs a clean-rebuild baseline · `make
unit` · new game + pause-menu quit · tutorial · Load Game dialog · resave gate PASS x3
consecutive (x5 for anything on a serialize or load path) · pointer-free type table vs
baseline · `[RtResolve] 0 misses`, no `[RtMint] TRAP`, 0 GATE WARN, new-game `calls=5686` ·
`dispatch_slot_map.py --check` 0 · cutover manifest gate · `rt_mint_gen --check` · `git diff
--stat` against the agent's file set before merging · at a step close also the Mods route with
`.pkm` mounted (move them back), one functional `make sa-stress` (record, never chase), one
true release compile from a clean obj dir, and a clean-rebuild warning diff. For step 5 add a
rendering check that is NOT a pixel diff: `tools/trace_oracle/` (skill `camera-pin`).
