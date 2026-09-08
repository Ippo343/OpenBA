# Wave 9 — Lane W: the 25 rows that are still cutover work, one disposition each

**Input:** [WAVE9_LANE_V.md](WAVE9_LANE_V.md) §2 — the 11 rows still on a raw door and the 14
that migrated to the port-registry door without being satisfied.

**Finding this lane must not undo, and did not:** `ResolveExport` misses the port registry
for an unported symbol and falls through to `ResolveExportRawImpl`
(`standalone/resolve_standalone.cpp:692`), which returns null with no module handle. Converting
`RT_ORIGINAL_ONCE` → `RT_EXPORT_ONCE` on a symbol with **no port and no mint entry** changes
only which residue list prints it. **The sufficient condition is a PORT or a MINT ENTRY**, and
every row below is dispositioned against that condition, not against which door it uses.

| Disposition | Rows |
|---|---|
| **CLOSED — ported this lane** | **7** |
| **CLOSED — the port already existed; the door was the bug** | **1** |
| OWNED BY A NAMED LANE, with the evidence re-checked | 4 |
| BLOCKED on a stated fact about the binary | 2 |
| PORT — specified, not done, with the blocker or dependency named | 11 |

**Nothing is defaulted.** Where a row is not closed, the reason is a fact that was checked in
this session, not an estimate of effort.

**EXECUTED 2026-09-08 — §9.** Four cells, **0 divergences in 0 sites** in every one, no crash.
**8 of the 9 ports ran**; the ninth pair has no caller anywhere and is named as unverified rather
than reported clean. Two findings the runs produced that source alone could not: the list branch
**cutover will take first is the only one never executed**, and the `IO::Encoding` blocker of §4
is **measured live on both the save and the load critical path**.

---

## 0. The finding this lane adds: 25 is a floor, and the mechanism is structural

Lane V's population is the `[RtRawResidue]` union — **the requests nine harness cells actually
made**. A symbol on a live path that no cell exercised cannot appear in it, and therefore cannot
appear in the verdict table. That is not an oversight in lane V; it is what residue measures.

The demonstration is one `switch` statement. `runtime/src/save/saveload.cpp:684-700` dispatches
`SaveLoad::Update` to five arms, all five through `RT_EXPORT_ONCE`, all five unported and
unminted:

```
kOpList          -> ?UpdateList@SaveLoad@Save@@QEAAXXZ           <- lane V row
kOpLoad          -> ?UpdateLoad@SaveLoad@Save@@QEAAXXZ           <- lane V row
kOpLoadFromFile  -> ?UpdateLoadFromFile@SaveLoad@Save@@QEAAXXZ   <- lane V row
kOpSave          -> ?UpdateSave@SaveLoad@Save@@QEAAXXZ           <- NOT in the CSV
kOpDelete        -> ?UpdateDelete@SaveLoad@Save@@QEAAXXZ         <- NOT in the CSV
```

`?UpdateSave` and `?UpdateDelete` appear in **zero rows** of
`docs/runtime_port/wave9_rawresidue_verdicts.csv` (`grep -c` = 0). Same file, same switch, same
door, same disposition — and invisible, because no cell in the matrix saved a game or deleted a
save.

So this lane wrote the source-side complement:
**`tools/unsatisfied_export_scan.py`** — every port-aware resolve of a symbol with neither a
port nor a mint entry, found by reading the tree rather than by running it. Output kept as
`docs/runtime_port/wave9_laneW_unsatisfied.txt`.

> **719 distinct names are resolved through a door. 141 have neither a port nor a mint entry.
> 138 of those have at least one site OUTSIDE a diff guard.**

Read that number carefully, because it is easy to over- and under-claim:

* It is **not** 138 crashes. "Outside a diff guard" is a *structural* property. Reachability is
  established for none of them — the same caveat WAVE9.md already carries for lane V2's 36
  conversions, and it is repeated here rather than quietly dropped.
* It **is** the right denominator. Residue answers *"what did this run ask for"*; this answers
  *"what would any run ask for"*. Neither subsumes the other, which is the same pairing
  `reference_count_the_doors_not_the_calls.md` records for the census — you need both doors
  counted and both questions asked.
* The scan states its own blind spot with a number rather than leaving it implied: **2** resolves
  go through a wrapper taking a non-literal argument and are invisible to it. That is the shape
  lane C2 found in `RtVtable()` and the census's `RE_EXPORT` limitation, and a scan that could
  not see it would have read as cleaner than it is.
* Its **effectiveness control** is this lane's own work: of the nine functions ported below, all
  nine dropped out of the list between the first and last run. A scan that could not respond to a
  port would have reported the same 141 either way.

---

## 1. A defect this lane found: `Geometry::SetBounds` was ALREADY PORTED

`?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z` is lane V row 9, verdict *REAL
DEPENDENCY*, remedy *"port or stub Graphics::Geometry::SetBounds"*.

It has been ported since **Wave 6 lane D**. `runtime/src/graphics/scene.cpp:1750` registers
`RtGraphics_Geometry_SetBounds` under that exact mangled name, with a full replay oracle at
`scene.cpp:1018`. The raw-door site lane V named (`mesh.cpp:233` at HEAD; `:250` after this edit) was therefore not an unported
dependency at all — it was a **live-path port bypass**: `RtGraphics_EdgeMeshInstance_SetMesh`
built every edge-mesh instance's bounds through the DLL while our registered, diffed body sat
unreached. The row closes with a **three-line door conversion** and no new code.

**And `port_bypass_audit.py --by-name` reports 0 while missing it.** Its verdict for that site is
`OK-RESOLVE-ONLY — "pointer stored; no unguarded use of `it` in RtGeometrySetBounds"`. That is
accurate about the accessor and wrong about the program: the accessor **returns** the pointer and
the call is in the caller, one frame out. WAVE9.md records that gate's blind spot as CLOSED —
the fix was to follow the assignment target through the brace-matched enclosing function — and a
resolve-and-return escapes exactly that. **This is a third face of the same hole**, after the
line-window version and the continuation-line version. The gate's 0 is real for the shapes it
sees; it does not cover the accessor family, and lane V had to read all eleven of its raw-door
rows by hand for precisely this reason.

Nine sites in the tree still carry `OK-RESOLVE-ONLY` (three in `audio.cpp`, six in
`buffers.cpp`). This lane read all six of the `buffers.cpp` ones by hand: every use is inside
`if (rt_diff_on__)`, so the **verdict is right and the stated reason is not**. The three
`audio.cpp` ones are pre-existing and were not re-read here.

**The follow-on this exposed, and it cost nothing to catch.** Porting the six
`Platform{Construct,Destruct}` bodies turned `mesh.cpp:271/274`
(`RtVertexBufferPlatformCtor` / `RtIndexBufferPlatformCtor`, called unguarded from `EdgeMesh`'s
ctor at `mesh.cpp:1442-1443`) into port bypasses **with nothing in `mesh.cpp` changing** — the
hazard WAVE9.md's bypass row states in as many words ("re-run after every LANDING, not after
every edit"). It was found by the audit's *denominator* moving, not by its verdict: ported names
573 → 579 and `OK-RESOLVE-ONLY` 4 → 11. Both converted.

---

## 2. The 25 rows

### Still on the raw door — 11

| # | Symbol | Disposition | Evidence |
|---|---|---|---|
| 1 | `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` | **LANE E** | It **is** ported and registered — `runtime/src/system/allocator.cpp:1088` — but every registration in that function sits behind an arming gate that refuses on three separate conditions (`allocator.cpp:1046-1085`), and WAVE9.md records the allocator as **GO, meaning READY TO ARM, NOT ARMED**. `port_bypass_audit` reports its five ADDRESS-door sites as `UNPORTED`, which is the correct reading of a gated registration and the mirror of `reference_census_counted_gated_and_missed_dx11.md`. Not touched, per this lane's brief. |
| 2 | `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` | **LANE E** | Same gate, same line block (`allocator.cpp:1090`). |
| 3 | `?PlatformConstruct@ImageBuffer@Video@@AEAAXXZ` | **CLOSED — PORTED** | `runtime/src/video/buffers.cpp`, `Video_ImageBuffer_PlatformConstruct`. See §3. |
| 4 | `?PlatformDestruct@ImageBuffer@Video@@AEAAXXZ` | **CLOSED — PORTED** | ditto. |
| 5 | `?PlatformConstruct@IndexBuffer@Video@@QEAAXXZ` | **CLOSED — PORTED** | ditto. |
| 6 | `?PlatformConstruct@VertexBuffer@Video@@QEAAXXZ` | **CLOSED — PORTED** | ditto. |
| 7 | `?ResetAnimationState@AnimationState@Graphics@@QEAAXXZ` | **PORT — specified, not done** | 0x1800fd930, 0xDE bytes. Walks the state's own 0x38-byte entry array (`+0x08` data, `+0x10` count); for each entry with `[e+0x0c] >= 0` it decodes `(v>>8) & 0xff` into a 0x30-byte record in the table at `[[?_singleton@AnimationManager@Graphics@@]]`, decrements a refcount at `+0x0c`, and on zero clears the record and calls two **unexported** internals (0x18005aa30, 0x18005b1e0). Then frees the array through the allocator (`[?_singleton@MemoryAllocator@System@@]+0x640` lock, `0x1800c6c00`) and nulls `+0x08`/`+0x10`. **Dependencies: the two unexported dtors (callable by address, as `ClearAll`/`RegisterInput` already do) and lane E's allocator.** Not a blocker, a lane slot. |
| 8 | `?PlayAnimation@AnimationState@Graphics@@QEAAX…` | **PORT — specified, not done** | 0x1800fdf50, 0x2C1 bytes, the largest of the Graphics rows. Grows the same entry array by one, so it shares row 7's allocator dependency and must be ported **with** it — the two share the array's growth policy and a port of one without the other is `PLAN §4.1 decision 5`'s half-port. |
| 9 | `?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z` | **CLOSED — the port already existed** | See §1. Closed by converting `mesh.cpp:250` to `RT_EXPORT_ONCE`; `scene.cpp:1039` is correctly ORACLE-ONLY (inside the port's own replay guard) and is left alone. Lane V's row says "graphics/scene.cpp:1039 is a second raw use" — it is the oracle, not a use. |
| 10 | `?PersistObject@InternalFactory@Object@@QEAAPEAVInternal@2@AEAVStream@IO@@IHI@Z` | **PORT — specified, not done** | 0x1800bff10. Read path of `InternalBasePtr::Serialize`. Walks the `?_singleton@Factory@Object@@` BST by type id, calls the descriptor's vtable `+0x20` factory, then `Base::IsDerivedFrom`, then the new object's `Serialize` at `+0x18`. Contains a **faithful unguarded deref** (`*(uint*)(obj+0x0c) = gen` runs even when `IsDerivedFrom` returned false and the local was set to null) that a port must reproduce, not guard. **A ready-made observation mechanism already exists**: `standalone/persistobject_probe.cpp` detours it under `BANISHED_PERSIST_TRACE=1` and logs every argument, so this is one of the better-instrumented rows in the table. |
| 11 | `addr:0x18007fbfc` | **LANE D** | The DLL's own statically-linked `_beginthreadex`, reached by address from `runtime/src/system/thread_win.cpp:152`. Not an export, so neither a port registration by name nor a mint row can serve it; the disposition is lane D's stated one — replace with UCRT's `_beginthreadex` and answer the thread-local CRT state question. Unchanged by this lane. |

### Migrated to the port-registry door, still unsatisfied — 14

| # | Symbol | Disposition | Evidence |
|---|---|---|---|
| 12 | `??0CompressedStream@IO@@QEAA@XZ` | **BLOCKED — named fact about the binary** | See §4. The shared `IO::Encoding` vtable at **0x1801554a8 is not an exported symbol**, and every concrete Stream ctor in this family stores it. |
| 13 | `??1CompressedStream@IO@@UEAA@XZ` | **PORT — specified, not done** | 0x1800aa650, 0x66 bytes and deceptively small: it calls `?Release@CompressedStream` (0x1800aa100, itself unported) and three **unexported** `Collection::Array` destructors (0x180024aa0 / 0x1800249c0 / 0x1800248e0) on the members at `+0x50`, `+0x40`, `+0x30`. Callable by address; the real cost is `Release`. |
| 14 | `??0MemoryStream@IO@@QEAA@XZ` | **BLOCKED — same named fact** | See §4. |
| 15 | `??1MemoryStream@IO@@UEAA@XZ` | **CLOSED — PORTED** | `runtime/src/io/compressedstream.cpp`, `RtIo_MemoryStream_Dtor`. See §5. |
| 16 | `?Open@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z` | **PORT — specified, not done** | 0x1800aa560, 0xE3 bytes. Needs `?SetWindowCount` (0x1800aa020, unported), the unexported array-resize 0x180024d70, and two dispatches through the source stream's own vtable (`+0x18` Read, `+0x30` GetSize). No Encoding dependency — this one is **not** blocked by §4, only unscheduled. |
| 17 | `?Open@MemoryStream@IO@@QEAA_NH@Z` | **CLOSED — PORTED** | `RtIo_MemoryStream_Open`. See §5. |
| 18 | `?CacheText@TextElement@UI@@AEAAXXZ` | **PORT — specified, not done, and ITS STATED BLOCKER IS STALE** | 0x18011ec10, 0x189 bytes. `runtime/src/ui/widgets_layout.cpp:95` says it is not reimplemented because it goes "through `String::FormatParamList`, which is out of this lane's scope (Sprite/String primitives are lane F's)". **`?FormatParamList@String@System@@SA?AV12@AEBV12@PEBV12@@Z` was PORTED by Wave 9 lane B3** and is registered at `runtime/src/system/string.cpp:1116`, alongside `FormatParamCount`, `Release`, `Format` and `operator=` — so the named blocker no longer exists. Reading the body, what is actually left is: `?SetParameterCount@TextElement@UI@@AEAAXXZ` (0x18011e8e0, exported, unported), one `String` conversion operator on `this+0xd8`, and four inlined copies of the refcount-release idiom (`--*(int*)(p-4)`, free at `p-8`) that `String::Release`'s port already implements. **Fourth stale blocker checked in this session**, and it is the shape `reference_holdback_blockers_are_stale_check_them_first.md` names: the lane was right on its evidence, the failure is transport. |
| 19 | `?DateAsString@DateTime@System@@QEBA?AVString@2@XZ` | **PORT — specified, not done, and porting it ALONE MAKES THINGS WORSE** | 0x1800c56a0. Fully read: `FileTimeToLocalFileTime` → `FileTimeToSystemTime` → `GetDateFormatW(0x400, DATE_SHORTDATE, …, 0x100)` → `WideString::Set` → `WideString::AsUTF8` → release the temporary. **Hidden return slot confirmed in the disassembly** (`MOV [RSP+0x10],RDX; MOV RBX,RDX; … MOV RAX,RBX`) — Ghidra's C shows only `this`, which is rule 3's trap. The reason not to port it now is arithmetic, not effort: its two callees `?Set@WideString@System@@QEAAXPEB_W@Z` and `?AsUTF8@WideString@System@@QEBA?AVString@2@XZ` are **both unported and unminted**, and so is every other `WideString` member. Porting this trades two unsatisfied symbols for two new ones. **The row's real content is "port `System::WideString`"**, which is a lane, and it is named here rather than hidden behind a green tick. |
| 20 | `?TimeAsString@DateTime@System@@QEBA?AVString@2@XZ` | **PORT — specified, not done** | 0x1800c55b0, byte-identical to row 19 except `GetTimeFormatW(0x400, 2, …)`. Same blocker, same lane. |
| 21 | `?FindFree3DEffect@Scene@Audio@@AEBAH…` | **CLOSED — PORTED** | `runtime/src/audio/audio.cpp`, `RtAudio_Scene_FindFree3DEffect`. See §6 — its stated blocker did not survive reading the body. |
| 22 | `?Stop@Device@Audio@@QEAAXPEAUAudioChannel@12@@Z` | **AUDIO CALLBACK LANE** (WAVE9.md rows 2-7) — and the blocker RE-CHECKED and CONFIRMED, not assumed | 0x1800de200, 0x74 bytes. Read in full this session: it calls `StopPlatform` and `FreeChannelPlatform` (both themselves holdbacks), clears bits 0-1 of the channel record, zeroes `+0x08`, and then **ends in `CALL RAX` on the completion callback stored at `channel+0x20`**, passing `+0x10` and `+0x18`. That is exactly the shared root cause `reference_audio_holdbacks_share_a_callback_root_cause.md` names, seen for a third time. A callback that has already run into game state cannot be un-run, so this row genuinely has no replay oracle and belongs with 2-7 rather than being ported alone. **`reference_holdback_blockers_are_stale_check_them_first.md` cuts both ways: three blockers in one session were stale, and this one is not.** |
| 23 | `?UpdateList@SaveLoad@Save@@QEAAXXZ` | **PORT — specified, not done** | 0x1800c1ea0, **0x6AA = 1706 bytes**, the largest single body in the table. The save-file enumerator. Save-path desync is this project's most-cited failure mode and this is a lane, not a slot. |
| 24 | `?UpdateLoad@SaveLoad@Save@@QEAAXXZ` | **PORT — specified, not done** | 0x1800c2690, 0x3DB = 987 bytes. Same switch, same lane as 23/25. |
| 25 | `?UpdateLoadFromFile@SaveLoad@Save@@QEAAXXZ` | **PORT — specified, not done** | 0x1800c2560, 0x123 = 291 bytes — **the tractable one of the three**, and the natural first slot for that lane. |

**And see §0**: rows 23-25 have two siblings on the identical door in the identical switch,
`?UpdateSave` and `?UpdateDelete`, which are in no verdict CSV because no harness cell saved or
deleted. Whoever takes the SaveLoad lane takes five arms, not three.

---

## 3. What was ported: the six `Platform{Construct,Destruct}` bodies

`runtime/src/video/buffers.cpp` + `runtime/src/video/buffers_win.cpp` (new).

All six are one function with three parameters — the companion-factory slot on
`Video::Device`'s `DeviceData` vtable (0x50 Image / 0x60 Index / 0x68 Vertex), the two link
offsets (`+0x50/+0x58` for ImageBuffer, `+0x10/+0x18` for the buffers), and which class-static
list head. Construct makes the dx11 companion, stores it, takes Device's lock, splices `this`
into the class's circular intrusive list, drops the lock. Destruct releases the companion,
takes the lock, unsplices, drops the lock.

**Two of the six are not lane V rows**, and are ported anyway: `?PlatformDestruct@IndexBuffer`
and `?PlatformDestruct@VertexBuffer` are absent from the 25 for the single reason that their
resolvers were **defined and never called** (two dead helpers at the top of `buffers.cpp`). They
become rows the moment `~IndexBuffer` or `~VertexBuffer` is ported, which is `reference_
porting_can_grow_the_cutover_surface.md` seen a step ahead.

**Three things this port needed that are worth recording.**

* **The three class-static list heads had to be minted.** `?_firstSurface@ImageBuffer@Video@@`,
  `?_firstBuffer@IndexBuffer@Video@@` and `?_firstBuffer@VertexBuffer@Video@@` are exported DATA
  and were not in the mint. They are reached through `ResolveExport`, so while the DLL is loaded
  and the mint disarmed our linkage and the DLL's still-unported `ImageBuffer::RecreateDynamic` /
  `ReleaseDynamic` agree on **one** list; at cutover the mint owns the bytes. `rt_mint_gen.py`
  **refused to generate** until each was given a size in `EXTRA_SIZES` — failing closed rather
  than guessing 8 for a possible object — which is the behaviour that makes the regeneration
  safe. Storage 28 → 31 symbols, `kStorageBytes` 2992 → 3040.
* **The Device lock is recursive by hand and both halves matter.** Retail keeps an owner thread
  id at `Device+0x88` and a depth at `+0x8C` beside the CRITICAL_SECTION at `+0x60`, and
  `?VerifyLock@Device@Video@@` reads them; dx11 calls `Lock`/`Unlock` by name. Writing the depth
  but not the owner would give a lock that works and a `VerifyLock` that lies. The unlock's
  **re-read** of `Device::_singleton` (rather than the pointer it already loaded) is reproduced
  deliberately — it is observable if anything swaps the singleton while the lock is held, which
  the device-reset path does.
* **`<windows.h>` stays behind the `_win` seam** (PLAN §4.1 decision 1 — only four files in the
  tree include it). The lock moved to `buffers_win.cpp`, which hardcodes the three offsets; the
  three `static_assert`s that bind them to `Device.h` live in the portable half, so a layout
  change breaks the build rather than silently corrupting the lock.

### The oracle: restore-and-replay, on the linkage

A plain call-both is wrong twice here — theirs would splice the same object into the list a
second time (a self-loop, and a corrupt list for every later walker) and would create a second
companion nothing frees. So the diff path captures the five words a link/unlink can write **at
their entry-time addresses**, runs OURS in full, reads them back, rewinds (giving our companion
back to dx11 through its own slot-0 deleting destructor, restoring the companion field and the
five words), runs THEIRS in full, and compares. The final state is retail's by construction — if
our linkage is wrong the run continues on the DLL's list and the divergence is a log line rather
than a crash three frames later.

**What it does not cover, stated rather than buried.** The companion create/release itself is not
differenced: in Construct ours' object is destroyed before theirs runs, and in Destruct the
companion field is nulled before theirs runs so retail's own `if (pc != nullptr)` skips a second
release of memory we already freed. Both are a single virtual dispatch into code this project
owns (`dx11::DeviceData`'s factory slot; the companion's slot-0 vector deleting destructor), and
both execute for real on the ordinary non-diff path. The **linkage** — the part that is retail
logic transcribed, including the two branch oddities — is what is compared, on every call.

**A near-miss worth recording.** The first mechanical pass that pointed the four live call sites
at our new bodies also rewrote the four calls **inside the oracle blocks**, so
`Video_IndexBuffer_PlatformConstruct`'s "theirs" was itself. That is
`widgets_layout.cpp`'s `RemoveGraphs` again — a function resolving to itself and recursing to
stack death — reached by a different route (a `str.replace` whose 4-space prefix matched the tail
of an 8-space line). Caught by re-reading every changed line rather than by the compiler, which
was perfectly happy with it.

---

## 4. The blocker on `??0MemoryStream` / `??0CompressedStream`, stated as a fact

Every concrete `IO::Stream` constructor in this family allocates an 8-byte `IO::Encoding` and
stores exactly one pointer into it: the shared encoding vtable at **0x1801554a8**.
`RtIo_CompressedMemoryStream_Ctor`'s oracle already special-cases that address by value.

**0x1801554a8 is not an exported symbol.** Checked against the DLL's export directory **by
address**, not by name pattern: 0x1801555b0 (`??_7MemoryStream@IO@@6B@`) is exported and
0x1801554a8 has no export at all. It is also not merely the interior of a larger exported table
— 0x180155488..0x1801554a0 hold four `_purecall` slots belonging to the abstract base, so
0x1801554a8 is where the **concrete** encoding's table begins.

Consequence: a ported ctor must obtain that pointer, and at cutover there is no name for
`ResolveExport` to take and no row the mint can carry, because **the mint is keyed by exported
symbol**. These two rows are therefore blocked on *extending the mint to unexported vtables*, or
on identifying the concrete `IO::Encoding` class and rebuilding its table slot by slot from the
DLL — lane C2's shape of work, not a port. Handed over named.

Neither `?Open@MemoryStream` nor `??1MemoryStream` touches the encoding's **contents**, only its
address, so neither inherits the blocker; that asymmetry is why the row splits 2 closed / 2
blocked rather than 4 of either.

---

## 5. `IO::MemoryStream::Open` and `~MemoryStream`

`Open` (0x1800ae670) is five stores and returns `true` **unconditionally, including when the
allocation failed** — reproduced, because a caller that trusts the `true` and writes into a null
buffer is retail's behaviour and a guard here is the one CLAUDE.md warns against. The
lock/Allocate/unlock trio the decompile shows inlined is instruction-for-instruction
`?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50) — compared, not assumed — so
calling `MemAlloc` is both faithful and the spelling that follows lane E's allocator when it
arms. Its oracle is a **full replay into a scratch object**: theirs' `Open` runs on a
DLL-constructed scratch with the same size and all four non-pointer stores plus the return are
compared; `_data` is excluded because two independent allocations cannot share an address.

`~MemoryStream` (0x1800ae890) writes exactly one field — the vptr, twice — and frees at most two
blocks, leaving `_data` and `_encoding` holding freed pointers (retail does not null them;
neither do we). Its oracle is an **A/B on two scratch objects**, and the reason it is not a
replay is the point: a destructor cannot be replayed (theirs after ours double-frees), and
suppressing the frees would leave nothing to compare because the only field written is written
unconditionally. Both scratches are **opened** before being destroyed so `_ownsData` is true and
the branch that matters runs on both sides — the effectiveness control, without which the
comparison would be green over an untaken branch. What it does **not** prove is that both bodies
freed the same blocks; two frees per side are not observable from here.

`FUN_180006a10`, which retail calls here, was read: it is a **second un-exported copy** of
`?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (0x1800c7ad0), same three instructions. Calling the
exported name is the same code and is the one lane E replaces.

---

## 6. `Audio::Scene::FindFree3DEffect` — a stated blocker that did not survive the body

`runtime/src/audio/audio.cpp`'s PlayEffect3D comment said the helper "dereferences `Audio::SoundEffect`
internals (a class this lane does not own)". Reading it, the SoundEffect internals it touches are
**exactly two scalars**: a float cutoff radius at `+0x20` and an int64 reuse window at `+0x28`.
Two field offsets are not a class layout. There is no vtable call, no allocation, and **no store
of any kind** — it is a pure const reader over the Scene's own 4096-entry effect table, which is
why it gets a **full call-both oracle** rather than a shadow. Same shape as
`reference_const_reader_needs_a_caller_not_a_human.md`.

Transcribed from the **disassembly**, for three reasons the decompiler's C got wrong or would
have (`reference_ghidra_c_output_vs_disassembly.md`):

* Ghidra prints the free-slot test as `pfVar2[9] == 0.0`, a float compare. The instruction is
  `CMP dword ptr [RCX+0x24],0` — an **integer** compare of `EffectSlot::idTag`. A float compare
  there reads 0x80000000, a valid tag, as `-0.0f` and equal to zero.
* The distance sum's **association** is observable in float: retail computes
  `(dy*dy + dx*dx) + dz*dz`, in that order. Another grouping can differ in the last bit and flip
  the radius test at the boundary.
* Both `COMISS` tests are spelled to match the FLAGS rather than C's default. `JNC` after
  `COMISS d2,rr` takes the branch when CF==1, i.e. `d2 < rr` **or unordered** — hence
  `!(d2 >= rr)`, not `d2 < rr`. The earlier `JBE` is ordered-and-strictly-greater, which is plain
  `>`.

The 20.0f divisor is 0x41A00000 lifted from 0x180161700 rather than a decimal retyped
(`reference_crt_coefficients_must_be_lifted_not_retyped.md`); it is exactly representable, so
both spellings agree here, and the bits are recorded so a later edit cannot quietly change that.

---

## 7. Verification

Every number below is a **delta against HEAD `de0207c` measured by reverting this lane's files
and re-running**, because a site vanishing from an audit reads exactly like a clean pass.

| Check | HEAD `de0207c` | With lane W | Accounting |
|---|---|---|---|
| `rawdoor_audit.py` | 468 sites, **0 UNGUARDED-CALL**, 36 DEFERRED | 472 sites, **0 UNGUARDED-CALL**, 36 DEFERRED | **+4 = +7 new oracle sites − 3 conversions.** The 7: 1 in `audio.cpp` (FindFree3DEffect's "theirs"), 6 in `compressedstream.cpp` (two scratch ctors, two scratch Opens, two scratch dtors). All inside `if (rt_diff_on__)`. The 3: `mesh.cpp` SetBounds, VertexBufferPlatformCtor, IndexBufferPlatformCtor. |
| `port_bypass_audit.py --by-name`, ADDRESS door | 189 sites, **0** | 189 sites, **0** | unchanged |
| … NAME door | 388 sites, **0 bypasses** | 398 sites, **0 bypasses** | **+10 = +6 (the `buffers.cpp` accessors, which only acquired a verdict because their names became ported) + 5 new oracle sites naming a ported symbol − 1 (SetBounds converted).** `mesh.cpp:271/274` net to zero: created by the port, removed by the conversion. |
| … ported names | 573 | 582 | **+9**, exactly the nine registrations |
| … statements naming a ported symbol | 377 | 387 | +10, matching the NAME-door delta |
| … parser did not see | 0 | 0 | |
| `rt_data_door_audit.py` | PASS, exit 0 | PASS, exit 0 | 3 `RT_RAW_OK` exemptions unchanged |
| `g++ -fsyntax-only` | — | clean on all 6 touched TUs | `buffers.cpp`, `buffers_win.cpp`, `mesh.cpp`, `audio.cpp`, `compressedstream.cpp`, `rt_data_mint.cpp` |
| `unsatisfied_export_scan.py` (new) | — | 141 unsatisfied, 138 with a live site | all nine ported names dropped out — the tool's own effectiveness control |

`rt_data_mint_tables.inc` regenerated: **vtables 46 → 47** (`??_7MemoryStream@IO@@6B@`, 8 slots,
count proved by adjacency), **storage 28 → 31**, traps 429 → 437 (+8 = the new table's slots),
`kStorageBytes` 2992 → 3040. Tracked diff is 32 insertions / 29 deletions — the storage table and
one header count, nothing else.

### ⚠️ This lane invalidates a recorded gate expectation

WAVE9.md's **"Minted DATA vs the DLL"** row is already marked SUPERSEDED / MUST BE RE-RUN, and
lists the numbers the re-run must produce: *"ground-truth 315 → 429, BaseTypeInfo fields 70 →
165, vtables 39 → 46"*. **Those are now stale by this lane's four additions.** Expect
**vtables 47** and **429 + 8 = 437 minted slots**, with `BaseTypeInfo` unchanged at 33 / 165 and
`suspects / absent / MISMATCH` still all 0. A recorded gate result is only valid for the artifact
it measured, and so is a recorded gate *prediction*.

### What needed a run — **DONE, see §9**

*Written before the directory was available; kept because the predictions in it are what §9
checks against.* At the time: nine ports and three door conversions had never executed. The
cells named here were then run, and §9 records the result — including two predictions that
held (`FindFree3DEffect` needed a positional sound; the MemoryStream pair is save-path only)
and one that did not (autostart reached `FindFree3DEffect` on its own). The cells:

* `BANISHED_RT_DIFF=1` on **`sa-run` (autostart)** — covers the six `Platform{Construct,Destruct}`
  bodies (every buffer and surface the renderer builds) and `Geometry::SetBounds` through
  `EdgeMeshInstance::SetMesh`. Expect non-zero `calls=` on all seven; a `calls=0` on the
  ImageBuffer pair would mean the port is not reached, not that it agrees.
* `BANISHED_RT_DIFF=1` on a cell that **plays a positional sound** — `Audio::Scene::FindFree3DEffect`.
  Lane V measured it in 2 of 9 cells, so plain autostart may not reach it; read `calls=`, not
  `divergences=`.
* `BANISHED_RT_DIFF=1` on an **autoload / save** cell — `IO::MemoryStream::Open` and
  `~MemoryStream`. These are save-path only; `compressedstream.cpp`'s own registration banner
  already says a plain autostart session never reaches that family.

Two specific things to watch that a clean summary would hide:

1. **The `~MemoryStream` oracle allocates.** Under `BANISHED_RT_DIFF=1` each call builds and
   destroys two 64-byte scratch streams. If lane E's allocator is armed in the same run, that is
   allocator traffic the shadow will see; it is diagnostic-mode only and absent from every
   shipping run, but it is not nothing.
2. **The `Platform*` oracle creates and destroys an extra dx11 companion per diffed call.** Also
   diagnostic-mode only. If dx11's companion factory is not idempotent under create/destroy/create
   the oracle will say so — which is itself worth knowing.

---

## 8. What this lane did not resolve, named

* **Rows 7, 8, 10, 13, 16, 18, 19, 20, 23, 24, 25** — eleven ports specified with their blocker
  or dependency named in §2, none started. The two largest (`?UpdateList` at 1706 bytes and
  `?UpdateLoad` at 987) are save-path and want their own lane.
* **Rows 12 and 14** — blocked on §4's unexported vtable. This needs a *decision*, not effort:
  either the mint grows a mechanism for unexported tables, or the concrete `IO::Encoding` class
  is identified and its table rebuilt. **PROMOTED TO THE LANE'S TOP ITEM BY §9 FINDING 3**: the
  runs measure the enclosing ported constructor at `calls=1` on BOTH the save and the load path,
  so at cutover this is a null call before the first save and the first load can do any work. It
  is no longer "two rows handed over"; it is a save/load-blocking gap with a known cause and no
  current mechanism.
* **The two branches cutover takes FIRST** (§9 finding 2) — `ListLink[first-in-list]` and
  `ListUnlink[list-emptied]`, `calls=0` in all four cells for all three classes, because the DLL's
  own constructors never reach our ports and its lists are never empty by the time ours run. At
  cutover they start empty and that branch runs immediately. The probe that would close it is
  sketched in §9 and deliberately not built.
* **`Video::{Index,Vertex}Buffer::PlatformDestruct`** (§9 finding 1) — registered, never entered
  in any cell, no caller in our tree or reachable in the DLL. Verified only through their shared
  body and their construct-side siblings; the composition is unproved.
* **Row 19/20's real content is "port `System::WideString`"** — six exported members, none
  ported, none minted. Filed as a lane, not as two rows.
* **`?UpdateSave` and `?UpdateDelete`** — two more arms of rows 23-25's switch, in no verdict
  table because no harness cell exercised them (§0).
* **The 138** — `wave9_laneW_unsatisfied.txt`. Reachability is established for none of them, and
  establishing it is a separate piece of work. What is established is that **25 is a floor**.
* **`port_bypass_audit.py`'s accessor blind spot** (§1) is *measured*, not *fixed*. Nine
  `OK-RESOLVE-ONLY` sites remain; six were read by hand here and are genuinely oracle-only, three
  in `audio.cpp` were not re-read. The fix is to follow a returned pointer to its callers, which
  is a change to the tool and not to any port.

---

## 9. EXECUTED — four cells, 2026-09-08

Section 7 said "nothing here has been executed". That is no longer true, and this section
replaces it. The directory was released to this lane after lane N finished; four runs, one
build per source change, each with the exe stamp and the source-mtime check quoted below.

**Build provenance.** Fresh object dir (the worktree had none), `standalone-debug`, exe stamped
**2026-09-08 01:12:38**, deployed 01:12; a `find` over `runtime/ code/ standalone/ engine/ dx11/`
for anything newer than the exe returned **nothing** on every build. Stronger than the stamp, the
binary is **self-evidencing** — three registration banners can only be emitted by code that
contains this lane's work:

```
[RtPort]   IO::CompressedMemoryStream: 8 exports registered    (was 6 — +Open@MemoryStream, +~MemoryStream)
[RtPort] Video::ImageBuffer/IndexBuffer/VertexBuffer (lane C): 27 exports registered   (was 21 — +6)
```

### Results

**Every cell: 0 divergences in 0 sites. No crash in any of the four** (`standalone_crash.log`
0 bytes each time; the intermittent WorkComponent teardown fault did not fire).

| Cell | Sites | Calls | Divergences |
|---|---|---|---|
| autostart → pause-menu Quit | 319 | 31,882,125 | **0 in 0 sites** |
| autostart, with branch counters | 324 | 32,025,561 | **0 in 0 sites** |
| autostart + `BANISHED_SAVE_TEST=900` (SAVE) | 353 | 37,436,400 | **0 in 0 sites** |
| `BANISHED_AUTOLOAD=Auto` (LOAD) | 344 | 16,143,510 | **0 in 0 sites** |

The save cell wrote a fresh 7.26 MB `Auto.sav` at 01:13, and the load cell loaded **that** save —
deliberately, so the mods-off/mods-on load hazard
(`reference_mods_off_breaks_save_loading.md`) could not be confused with a port defect. The
pre-existing `Auto.sav` is preserved as `Auto.sav.pre_laneW`.

### Per-site `calls=`, with the branch each call took

| Port | autostart | save | load | branch coverage |
|---|---|---|---|---|
| `Video::ImageBuffer::PlatformConstruct` | 6 | 6 | 5 | appended 6/6/5 · **first-in-list 0/0/0** |
| `Video::IndexBuffer::PlatformConstruct` | 4 | 4 | 6 | appended 4/4/6 · **first-in-list 0/0/0** |
| `Video::VertexBuffer::PlatformConstruct` | 4 | 4 | 6 | appended 4/4/6 · **first-in-list 0/0/0** |
| `Video::ImageBuffer::PlatformDestruct` | 4 | 4 | 2 | unlinked 4/4/2 · early-out 0 · **list-emptied 0/0/0** |
| `Video::IndexBuffer::PlatformDestruct` | — | — | — | **NEVER ENTERED — see below** |
| `Video::VertexBuffer::PlatformDestruct` | — | — | — | **NEVER ENTERED — see below** |
| `Audio::Scene::FindFree3DEffect` | 4 | 4 | — | free-slot 4/4 · **reuse-hit 0** · table-full 0 |
| `IO::MemoryStream::Open` | — | — | **1** | no branches |
| `IO::MemoryStream::~MemoryStream` | — | 1 | 1 | owned-buffer freed 1/1 · encoding freed 1/1 · no-buffer 0 |
| `Graphics::Geometry::SetBounds` (door conversion) | 30 | 32 | 1 | — |

`Geometry::SetBounds` at 30/32/1 is the **conversion proved live**: before it, every one of those
calls went to the DLL and our registered, diffed Wave 6 body was unreached.

`FindFree3DEffect` was reached in the plain autostart cell, which lane V's 2-of-9 measurement did
not predict — so it is verified without needing a bespoke positional-sound cell.

### ⚠️ FINDING 1 — two ports have ZERO callers anywhere, in any cell

`Video::IndexBuffer::PlatformDestruct` and `Video::VertexBuffer::PlatformDestruct` **never
appear in any of the four `[RtDiff]` site lists at all** — not `calls=0`, but never registered,
i.e. never entered. This is stated as a finding, not a footnote.

It is **expected and was predicted** (§3): those two were ported for symmetry precisely because
lane V had excluded them for having a defined-but-never-called resolver. Their only callers would
be `~IndexBuffer` / `~VertexBuffer`, which are unported, and no detour is installed, so the DLL's
own internal destructors do not reach our bodies either.

What *is* verified about them, and it is not nothing: they share `PlatformDestructBody` with
`ImageBuffer::PlatformDestruct` (10 verified calls, `unlinked` branch taken all 10 times), and
their class-specific parameters are verified from the other side — `kBufferLinks`
(`+0x10`/`+0x18`) is exercised 10 and 10 times by their own `PlatformConstruct` siblings. The
composition is what is unverified: the `nullCompanionAfterRelease=true` flag, which is one store,
and is the single line where the Index/Vertex pair differs from ImageBuffer in the shipped bytes.

**Do not read their absence as agreement.** Per
`reference_uncalled_port_is_not_verified.md` they are registered, counted, and unproved.

### ⚠️ FINDING 2 — the branch cutover will take FIRST is the one never executed

`ListLink`'s **first-in-list** branch is `calls=0` in all four cells, across all three classes,
and `ListUnlink`'s **list-emptied** branch likewise. Every one of the 31 verified construct calls
took `appended`; every one of the 10 verified destruct calls took `unlinked`.

The cause is structural and it matters: **the DLL's own internal constructors never reach our
ports** — no detour is installed on these six — so by the time our first call runs, the shipped
`Video::Device` startup has already put surfaces and buffers on those lists and `*first` is never
null. Our 31 calls are only the ones that came from *our* code.

**At cutover that inverts completely.** The mint owns `?_firstSurface@ImageBuffer@Video@@` and the
two `?_firstBuffer@` heads, they start at null, and every construct in the process is ours — so
**the very first buffer the renderer builds takes the branch that has never been executed and
never been compared against retail.** It is three stores and it is transcribed from the same
decompile as the branch that is verified 31 times, but that is a structural argument, not a
measurement, and this row must not be reported as fully covered.

The oracle that would close it exists in principle — save the real list head, null it, run ours
and theirs against a scratch node under the Device lock, restore — but it mutates the live global
list that the DLL's own `ImageBuffer::RecreateDynamic`/`ReleaseDynamic` walk from the device-reset
path, which is exactly the structure under test. **Deliberately not built while holding the
directory with another lane queued behind**; it wants its own review.

### ⚠️ FINDING 3 — the `IO::Encoding` blocker is on the SAVE **and** LOAD critical path, measured

§4 handed `??0MemoryStream` and `??0CompressedStream` over as blocked on the unexported encoding
vtable at 0x1801554a8. The runs upgrade that from a tidiness item to a **cutover blocker with
measured reachability**:

* SAVE cell: `IO::CompressedMemoryStream::CompressedMemoryStream calls=1`.
* LOAD cell: same, `calls=1`, plus `CompressedMemoryStream::Open`, `MemoryStream::Open`,
  `Close`, `GetMemoryStream` and both destructors, all `calls=1`.

That ported constructor's first two statements are
`RT_EXPORT_ONCE(CtorFn, "??0CompressedStream@IO@@QEAA@XZ")` and
`RT_EXPORT_ONCE(CtorFn, "??0MemoryStream@IO@@QEAA@XZ")` (`compressedstream.cpp:205,207`). Both
names are unported and unminted, so at cutover `ResolveExport` misses and returns null.

> **Measured consequence: with the DLL removed, the first save and the first load each call
> through null inside `RtIo_CompressedMemoryStream_Ctor`, before either can do any work.** Not
> inferred from a source sweep — observed as `calls=1` on the enclosing port in two different
> cells.

And it cannot be closed by porting harder: the encoding vtable has no exported name for
`ResolveExport` to take and no row the mint can carry. **This is the highest-priority item this
lane hands over**, and it is a mint-mechanism decision, not a porting slot.

### A defect found in this lane's own probe, kept on the record

The first branch-counting pass reported the exact opposite of the truth. `CountBranch` was
written as `if (taken) RtDiffReport(...); else RtDiffRegister(...)` on the assumption that
reporting registers the site. **`RtDiffReport` does not register** (`rt_diff.cpp:82` — it only
bumps `calls`), so an *always-taken* branch never entered the site list and never printed, while a
*never-taken* one printed `calls=0`. Run 2 therefore showed `[branch:first-in-list] calls=0` and
no `[branch:appended]` line at all, which reads like "neither branch ran".

The data was still recoverable by arithmetic — a missing site whose sibling reads 0 must have
taken every one of the port's calls — and run 3 with the fix confirmed it exactly
(`appended` = 4/4/6, matching each port's own `calls=`). Recorded rather than quietly fixed
because it is the shape `reference_probe_needs_an_effectiveness_control.md` names: **a coverage
instrument that is itself wrong reads as coverage.**

### Housekeeping

`rt_audio_port.cpp`'s banner said `Audio: %d/22 exports registered (3 held back, 1 phantom name
fixed, 1 data export)`. Adding `FindFree3DEffect` made `20/22` arithmetic that no longer resolves
against that breakdown. The hardcoded denominator is dropped rather than bumped — a denominator
that must be re-derived every time a lane lands is a comment on its way to becoming a lie, which
is what `e9d81d9` was cleaning up.

Audits re-run after every source change and unmoved from §7: **rawdoor 472 sites / 0
UNGUARDED-CALL · port_bypass ADDRESS 189/0, NAME 398/0, 582 ported names, 0 unseen ·
rt_data_door exit 0**. Full autoload-cell log kept as
`docs/runtime_port/wave9_laneW_run_autoload.txt`.
