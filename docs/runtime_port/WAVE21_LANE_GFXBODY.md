# WAVE 21 — LANE GFXBODY

Branch `wave21/gfxbody`, cut from `wave21/base` at `018426f2`.
Subject: the **13 CODE rows** of the 29-row Graphics census gap (kickoff §1).
The 8 VTABLE rows are lane GFXVT's and the 8 DATA rows are lane GFXDATA's;
nothing here touches them.

---

## 1. RE-DERIVATION (kickoff rule 1)

```
tools/runtime_census.py --csv census21_gfxbody.csv      # run 2026-09-11, in this worktree
# -> 1311 rows; awk '$3=="Graphics" && $4=="0"'  -> 29 rows
```

**The kickoff re-derives EXACTLY.** 29 unported Graphics rows, and the 13 named
in this lane's brief are all present and all `ported=0`. The §1 classification
(13 code / 8 vtable / 8 data) reproduces symbol for symbol.

**None of the 13 is already ported.** Checked two ways, not one:

* the census `ported` column (denominator: 29 unported Graphics rows);
* a literal `grep -F` of each of the 13 **mangled names** across
  `code/ engine/ runtime/ standalone/ dx11/` (denominator: 13 of 13 names).
  Every hit is a *consumer* (`ResolveExport` / `RT_EXPORT_ONCE` door, an
  `RT_MINT_NAMED` table row, or a prose comment). **Zero hits are a body.**
  This is the check `reference_half_registered_port_reads_as_absent.md` asks
  for — a ported-but-unregistered body would be invisible to the census, and
  there is none here.

All 13 were also confirmed **present in the shipped export directory** of
`C:\GOG Games\Banished\Runtime-x64.dll` with `pefile`
(`reference_resolveexport_phantom_names.md`); the addresses in §2 are that
query's output, not Ghidra's.

### 1.1 Ghidra was down for the triage — and the substitute turned out to be checkable

The Ghidra MCP bridge refused connection (`127.0.0.1:8089`, WinError 10061) for
the whole of Phase 1 and 2. Sizes therefore come from **`.pdata` + `objdump`,
not from `get_function_metrics`** — which, per
`reference_ghidra_mcp_address_param_silently_ignored.md`, is the weaker tool
anyway.

**Ghidra came up during Phase 3 and every corrected extent was confirmed
exactly** (`get_function_by_address`, program `Runtime-x64.dll`, confirmed
current on the call):

| symbol | this lane's `.pdata` chain | Ghidra `Body:` | agree |
|---|---|---|---|
| `?Setup@MaterialInstance@` | `0x180102c90-0x180102ddc` (332) | `180102c90 - 180102ddb` | yes |
| `?Update@AnimationManager@` | `0x1800fc240-0x1800fc3ee` (430) | `1800fc240 - 1800fc3ed` | yes |
| `?Draw@Renderer@` | `0x180104bb0-0x180104f49` (921) | `180104bb0 - 180104f48` | yes |
| `?AddDrawItems@MeshInstance@` | `0x180100f20-0x1801011f0` (720) | `180100f20 - 1801011ef` | yes |

So the unwind-chain grouping is the right method and the **uncorrected**
single-record reading was the wrong one — confirmed from outside, not argued.

**A measurement error inside this lane, caught and corrected — record it:** my
first pass read the extent of each function from the **single `.pdata` entry
containing its entry address**, and that is a **LOWER bound, not the extent**.
MSVC splits functions across several `RUNTIME_FUNCTION` records, the extra ones
carrying `UNW_FLAG_CHAININFO` back to the primary. On that first pass
`?Update@AnimationManager@` measured **50 bytes / 13 instructions**; its real
extent is **430 bytes / 104 instructions**, and the giveaway was a `jle` in the
"last" instruction jumping past the "end". `?Draw@Renderer@` read 408 bytes and
is 921. **Three of the 13 were understated, by up to 8.6x.** The fix is to
group `.pdata` rows by the unwind chain and sum the fragments; §2 is the
corrected table. This is the mirror image of
`reference_adjacent_export_is_an_upper_bound.md`: same class of error, opposite
sign, and equally invisible without a cross-check.

---

## 2. TRIAGE TABLE

Sizes are the summed extent over all `.pdata` fragments of the unwind chain.
`calls` counts `call` instructions in that extent; `ind` is how many of those
are indirect (register / vtable / IAT).

| # | symbol | retail | bytes | instrs | calls (ind) | verdict |
|---|---|---|---|---|---|---|
| 1 | `??2FrameCopyImageBuffer@Graphics@@SAPEAX_K@Z` | `0x18001efc0` | 99 | 24 | 3 (2) | **PORT-NOW** |
| 2 | `??2MaterialInstance@Graphics@@SAPEAX_K@Z` | `0x180002820` | 99 | 24 | 3 (2) | **PORT-NOW** |
| 3 | `?Setup@MaterialInstance@Graphics@@QEBAX…` | `0x180102c90` | 332 | 83 | 1 (0) | **PORT-NOW** |
| 4 | `?Generate@FrameCopyImageBuffer@Graphics@@…` | `0x1800a3b20` | 60 | 18 | 1 (1) | BLOCKED-ON-FCIB-BLOCK |
| 5 | `??1FrameCopyImageBuffer@Graphics@@UEAA@XZ` | `0x1800a3c80` | 138 | 37 | 5 (1) | BLOCKED-ON-FCIB-BLOCK |
| 6 | `?Create@FrameCopyImageBuffer@Graphics@@…` | `0x1800a3b70` | 197 | 54 | 4 (1) | BLOCKED-ON-FCIB-BLOCK |
| 7 | `??0FrameCopyImageBuffer@Graphics@@QEAA@XZ` | `0x1800a3d10` | 304 | 80 | 11 (5) | BLOCKED-ON-FCIB-BLOCK |
| 8 | `?Serialize@MeshInstance@Graphics@@…` | `0x1800ff680` | 114 | 28 | 4 (1) | BLOCKED-ON-STREAM-ORACLE |
| 9 | `?Serialize@AnimationManager@Graphics@@…` | `0x1800fc190` | 165 | 46 | 6 (4) | BLOCKED-ON-STREAM-ORACLE |
| 10 | `?Update@AnimationManager@Graphics@@…` | `0x1800fc240` | 430 | 104 | 1 (0) | BLOCKED-ON-ANIMSTATE-LAYOUT |
| 11 | `?AddDrawItems@MeshInstance@Graphics@@…` | `0x180100f20` | 720 | 179 | 1 (0) | **UNPROMOTABLE** (NO-OP by name, HARMFUL by address) |
| 12 | `?Draw@Renderer@Graphics@@…` | `0x180104bb0` | 921 | 237 | 9 (4) | NEEDS-ITS-OWN-LANE |
| 13 | `?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ` | `0x1801033e0` | 1422 | 383 | 23 (12) | NEEDS-ITS-OWN-LANE |

Totals over the lane's own denominator of **13**: 3 PORT-NOW, 7 blocked in three
distinct clusters, 1 unpromotable, 2 needing their own lane.

### 2.1 The kickoff's one wrong guess about this lane

> "`?Draw@Renderer@` in particular may be large enough to be a lane on its own."

Half right, and it named the wrong function. `Draw` is 921 bytes / 237
instructions — big, but **`?BuildParameters@MaterialInstance@` is 1422 bytes /
383 instructions with 12 indirect calls, 55% larger and the largest row in the
Graphics gap by every measure.** A lane scoped as "Draw, plus the small stuff"
would have left the actual monster in the "small stuff".

### 2.2 Callees, measured (this is where the blockers live or die)

Every direct callee of the 13, reverse-looked-up in the shipped export table:

| caller | callee | exported? | ported in our tree? |
|---|---|---|---|
| `AnimationManager::Update` | `?Update@AnimationState@Graphics@@QEAA_NAEBVTime@System@@AEBV?$Array@UNode@GraphicsMesh@Graphics@@@Collection@@AEBVTransform@Math@@PEAV?$InterpolatedState@VTransform@Math@@@2@PEAV78@AEAVVector3@8@_N6@Z` | **YES** | no (not in census `called` at all) |
| `MaterialInstance::Setup` | `?SetupShaders@VideoProgram@Video@@QEBAXAEAVContext@2@@Z` | YES | no (not `called`) |
| `MeshInstance::AddDrawItems` | `?AddDrawItem@Renderer@Graphics@@…` | YES | **yes** (Wave 9) |
| `MeshInstance::Serialize` | `?Serialize@AnimationState@Graphics@@…` + 3 unexported (`0x18005cc60`, `0x18005d4a0`) + vtbl `+0xa0` | partly | no |
| `AnimationManager::Serialize` | 2 unexported (`0x18005c660`, `0x18005c3d0`) + vtbl `+0x00/+0x18/+0x20/+0xa0` | no | no |
| `Renderer::Draw` | `Reset`, `AddScene`, `BeginDrawGroup`, `AddCulledDrawItems` (all exported), 1 unexported `0x180062d60`, 2 `Video::Context` virtuals `+0x48/+0x50` | mostly | `Reset` **yes**, `AddScene` **yes**, other two no |
| `FrameCopyImageBuffer` ctor/dtor/Create | `??2RenderBuffer`, `?Create@RenderBuffer`, `?PlatformConstruct@RenderBuffer`, `?PlatformDestruct@RenderBuffer`, `??3RenderBuffer` | YES | **ALL FIVE YES** |
| both `operator new` | `?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z` + `EnterCriticalSection`/`LeaveCriticalSection` | YES / KERNEL32 | **yes** |

**Note the cost this lane would incur, and that nothing in the census prints
it:** `AnimationState::Update`, `VideoProgram::SetupShaders`,
`Renderer::BeginDrawGroup` and `Scene::AddCulledDrawItems` are **not in the
`called` column today**. Porting the bodies above them would ADD them to the
cutover surface — `reference_porting_can_grow_the_cutover_surface.md`.

---

## 3. THE BLOCKERS, RE-CHECKED RATHER THAN INHERITED

`reference_holdback_blockers_are_stale_check_them_first.md` says to re-check a
stated blocker before porting. All four clusters below carry a **written**
holdback from Wave 6, re-checked once at Wave 8.5. Here is the 2026-09-11 state.

### 3.1 FrameCopyImageBuffer (rows 4–7) — HALF THE BLOCKER IS NOW STALE

`runtime/src/graphics/animation.cpp:39-68` holds all four back as one block for
two reasons:

* **(a) entanglement** — "`Video::RenderBuffer`'s ctor/dtor and `Video::Context`'s
  ctor are still individually unported", so a wrong `RenderBuffer` field would
  corrupt an object three lanes were touching.
  **THIS IS NOW FALSE for every RenderBuffer symbol FrameCopyImageBuffer
  actually calls.** All five — `??2RenderBuffer`, `??3RenderBuffer`,
  `?Create@RenderBuffer`, `?PlatformConstruct@RenderBuffer`,
  `?PlatformDestruct@RenderBuffer` — read `ported=1` in today's census, and
  `runtime/types/video/` now owns the layout. The entanglement argument as
  written no longer describes the tree.
* **(b) no GPU oracle** — "there is no memcmp oracle for *did the GPU texture
  get copied correctly*; only a screenshot pixel comparison would tell".
  **THIS STILL STANDS**, and Wave 8.5 already named it the load-bearing half.
  It bites `?Generate@` hardest (18 instructions, whose entire effect is one
  virtual call into the DX11 backend) and the ctor/dtor least.

**Verdict: re-classify from "held as one block" to a sized, oracle-shaped
question.** The ctor and dtor are CPU-side object construction over a layout we
now own and are answerable with a post-state compare; `Create` and `Generate`
are the GPU half and are not. This lane did not have the budget to pin the
`FrameCopyImageBuffer` layout (0x88 bytes, per
`runtime/src/save/saveload_draw.cpp:451`) and split the block, so it is recorded
as a re-scoped input rather than done. **It is also item 5 of the kickoff's §3
("what 'close the screenshot hole' MEANS") in disguise** — lane SHOT and any
future FCIB lane are the same lane, and neither doc says so.

### 3.2 AnimationManager::Update (row 10) — THE STATED BLOCKER IS WRONG AS WRITTEN

`animation.cpp:70-79` holds it back because of "a wrong argument-slot mapping in
the **9-argument `AnimationState::Update` call** this function makes", through a
"nested, **unexported** `AnimationState` record … this lane did not have budget
to fully pin".

Measured: the body is 430 bytes / 104 instructions and makes **exactly one
call**, to `0x1800fdb20`, which **IS EXPORTED** — and its mangled name spells
out all nine arguments and their types in full
(`_N` return, `Time const&`, `Array<GraphicsMesh::Node> const&`,
`Transform const&`, two `InterpolatedState<Transform>*`, `Vector3&`, `_N`, `_N`).

**So the specific risk the holdback names — mis-assigning the argument slots —
is answerable from the export table in one command, with no reversing at all.**
The residual risk is real but different: it is the **iteration layout** (the
0x30-byte group records the manager walks), not the call. A blocker that names
the wrong hazard is not a blocker anyone can discharge.

The second half of that holdback — "it runs on every character's skeleton every
frame, behind a default-on non-retail batching path
(`standalone/perf_experimental.cpp`, `ANIM_BATCH_ALL`)" — was **not re-checked
by this lane** and remains as stated. See §6.

### 3.3 The two Serialize rows (8, 9) — BLOCKER NOT DISCHARGED, BUT ITS PREMISE IS CONTESTED

Both are held on "a differential run would consume or emit the record TWICE and
a stream desync surfaces later on an innocent record", plus (for
`MeshInstance::Serialize`) the genuinely nasty
`reference_encoding_vtable_offsets_differ_between_builds.md` — the Encoding
vtable slot is `+0xa0` in retail and `+0xb8` in the profile build, so a port
written from the profile decompile desyncs every save while looking right.

**The offset hazard is real and I confirmed the slot: both bodies call
`*(rax+0xa0)`, the retail spelling.** But the "no oracle" half is contested by
`reference_write_path_serialize_is_replayable.md` — "*'no oracle possible' was
WRONG for 4 Serialize ports across 3 waves; on the WRITE path a Serialize is
replayable*". Neither `animation.cpp` nor `mesh.cpp` mentions that memory; both
predate it. **This is a blocker that should be re-argued against the write-path
oracle before anyone repeats it a fourth time**, and this lane is not the one to
do it — it is IO/Serializer expertise, not Graphics.

### 3.4 AddDrawItems (row 11) — UNPROMOTABLE, AND THE REASON HOLDS EXACTLY

`runtime/src/graphics/mesh.cpp:1823-1845` gives two reasons and **I verified the
sharper one against live source, not against the comment**:

`standalone/scene_watch.cpp:400` still reads

```c++
if (s_addDrawItems == 0) s_addDrawItems = ResolveAddr(Module::Runtime, 0x180100f20);
...
return reinterpret_cast<uintptr_t>(vt[3]) == s_addDrawItems;
```

— the address is an **IDENTITY TOKEN**, compared against vtable slot 3 of a live
object. `ResolveAddr(Module::Runtime, …)` consults the port registry FIRST
(`reference_resolveaddr_consults_port_registry_first.md`), so a
`RegisterRuntimeAddr` on `0x180100f20` makes that compare fail for **every**
object and `BANISHED_LOD_CENSUS` / `BANISHED_FORCE_LOD0` silently see an empty
scene. Registering by NAME instead is not harmful but is a **no-op**: the only
caller is the DLL's own `Renderer`, dispatching through the DLL's own
`??_7MeshInstance@` — we intercept only at doors, and an in-DLL vtable dispatch
is not one (`reference_a_ported_row_can_be_permanently_unpromotable.md`).

**This row should be tagged `RT_UNPROMOTABLE` with reason class HARMFUL** (the
distinction `wave21/base` commit `1afa558e` just introduced) so the Graphics
ceiling stops counting it as outstanding work. That is kickoff item 11's
machinery and lane CLASS's tool; this lane supplies the row and the reason.

### 3.5 Draw and BuildParameters (rows 12, 13) — NOT BLOCKED, JUST BIG

Neither carries an oracle-shaped blocker in the tree; `renderer.cpp:46-56` holds
them on lane-concurrency ("layouts this lane did not pin", "other lanes running
concurrently") — a **Wave 6 scheduling fact, not a property of the functions**,
and four of `Draw`'s six named dependencies have since been ported. They are
sized out of this lane, not argued out of it: 921 + 1422 bytes with 16 calls
between them is a lane.

---

## 4. WHAT THIS LANE PORTED

**3 of the 13.** All in `runtime/src/graphics/wave21_lane_gfxbody.cpp`,
registered by `RtGraphicsWave21GfxbodyRegister()` behind its own kill switch
`BANISHED_RT_GFXBODY` (wired into `rt_graphics_port.cpp`; unset = ON, per
PLAN §4.1 decision 3).

| symbol | how | verification tier |
|---|---|---|
| `??2FrameCopyImageBuffer@Graphics@@SAPEAX_K@Z` | `MemAlloc(bytes, 8)` | **STRUCTURAL** |
| `??2MaterialInstance@Graphics@@SAPEAX_K@Z` | `MemAlloc(bytes, 8)` | **STRUCTURAL** |
| `?Setup@MaterialInstance@Graphics@@QEBAX…` | full transcription | **STRUCTURAL** |

**Registered by NAME only, deliberately.** All three addresses were checked
first: `0x18001efc0` and `0x180002820` have **zero** occurrences anywhere in
`code/ engine/ runtime/ standalone/ dx11/`, and `0x180102c90` occurs only in
three comments (`runtime/src/ui/spritecontext_flush.cpp` :95, :138, :260) —
both live consumers of `Setup`
(`code/Game/Player/tool_base_tilerender.cpp:100` and
`runtime/src/ui/spritecontext_flush.cpp:267`) resolve it **by name**. That check
exists because this lane's own row 11 is a case where a `RegisterRuntimeAddr`
would be active harm; the rule is check every address, not register none.

### 4.1 Why STRUCTURAL and not EXERCISED — say it plainly

**No run happened.** `tools/game_dir_lock.sh acquire GFXBODY 600` **TIMED OUT
after 600s** with lane GFXDATA still holding the directory. It was not piped
into anything, so the refusal was read as a refusal
(`reference_a_piped_acquire_discards_the_refusal.md`), and nothing was deployed.
The lock was never held by this lane.

So the tier is STRUCTURAL for all three — argued from the disassembly, never
run. It is, however, **structural against two independent recoveries**, which is
stronger than the usual single-source structural claim:

* each body was transcribed by this lane from `objdump -d` on the shipped DLL,
  **before** Ghidra was available;
* each was then re-read from Ghidra's decompiler **after**, and compared.

The two agree on every point, including the four places a plausible rewrite
diverges — the masked shift (`1 << (slot & 0x1f)`), the unordered-compare
semantics, the integer (not float) copy of the four constants, and both
`operator new` alignments being 8 and not 4. The comparison is recorded in the
source file's header comment.

Ghidra's decompiler drops dereferences and whole arguments, so it is **not** an
oracle on its own; here it is used only as a second, differently-biased reading
of a body this lane had already transcribed from the bytes.

### 4.2 The measured cost of this port, stated as the census sees it

```
                 before      after
Graphics ported     70    ->    73        (+3, of a 13-row lane denominator)
TOTAL called      1311    ->  1312        (+1)
TOTAL ported      1124    ->  1127        (+3)
```

**The `called` column moved, and this lane moved it.** `?Setup@MaterialInstance@`
calls `?SetupShaders@VideoProgram@Video@@QEBAXAEAVContext@2@@Z`, which was not
in `called` before — porting a body adds its callees to the cutover surface
(`reference_porting_can_grow_the_cutover_surface.md`). One new row, in Video,
unported. It is named in the source at the call site so the next census reader
can attribute it.

Build: `mingw32-make standalone-debug` clean in this worktree; all three symbols
present in `standalone/build/reimpl-x64.exe` (`nm`). Nothing was copied into the
game directory.

### 4.3 A sized follow-on this lane opened but did not take

Ghidra's decompile of `?Update@AnimationManager@` (row 10) enumerates the whole
of the "unpinned `AnimationState` record" the Wave 6 holdback could not afford
to pin. It is **two records and eight offsets**: the group is stride `0x30`
(`+0x00` ptr, `+0x10` `Array<GraphicsMesh::Node>`, `+0x20` state array `_data`,
`+0x28` count) and the state is stride `0x48` (`+0x18` `Vector3`, `+0x1c`,
`+0x20`, `+0x28` `Transform*`, `+0x38` `InterpolatedState<Transform>*`). With
the callee's nine arguments already spelled out by its exported mangled name
(§3.2), **the stated blocker is an afternoon, not a wave.** That enumeration is
decompiler output and has NOT been checked against the disassembly, which is
exactly the step the coordinator's hazard 3 says not to skip — so it is a
scoped input for a follow-on lane, not a finding.

---

## 5. WHAT I DID NOT CHECK (kickoff rule 7)

* **Ghidra.** The MCP bridge was down for this lane's entire run. No decompiler
  output was consulted for any of the 13; every claim in §2 comes from `objdump`
  on the shipped DLL and from `.pdata`. The profile DLL was never opened, so no
  name was recovered from it — the callee names in §2.2 are all from the
  **retail** export table.
* **The `ANIM_BATCH_ALL` path** in `standalone/perf_experimental.cpp` — the
  second half of the `AnimationManager::Update` holdback. Not read.
* **Whether `??_7MeshInstance@`, `??_7EdgeMesh@` and `??_7EdgeMeshInstance@`
  really are in the promotion backlog but not the unported list** — that is lane
  GFXVT's first question and I deliberately did not answer it.
* **Any of the 8 VTABLE rows or 8 DATA rows.** Out of scope by assignment.
* **The claim that `0x180062d60`, `0x18005c660`, `0x18005c3d0`, `0x18005cc60`
  and `0x18005d4a0` are genuinely unexported internals** rather than exports I
  failed to match — they are absent from the retail export table by address,
  which `reference_grep_cannot_tell_internal_from_unported_export.md` warns is
  not the same question. Not chased further.
* **Any runtime behaviour.** No game was launched by this lane. The lock was
  requested and **refused on timeout** (§4.1), so all three ports are
  STRUCTURAL. In particular nobody has seen `MaterialInstance::Setup` bind a
  texture through our body: it is on the sprite-flush path
  (`runtime/src/ui/spritecontext_flush.cpp`) and the tile-overlay path
  (`code/Game/Player/tool_base_tilerender.cpp`), both of which run every frame,
  so **a defect in it should be visible rather than silent** — but "should be
  visible" is a prediction, not a check. `BANISHED_RT_GFXBODY=0` is the A/B.
* **The `.pdata` extents of the ten rows I did NOT port** were confirmed against
  Ghidra for only **4 of 13** (§1.1). The other nine sizes in §2 rest on the
  unwind-chain method alone. The method is now corroborated; those nine
  individual numbers are not.
* **Whether `?SetupShaders@VideoProgram@Video@@` is safe to leave on the DLL.**
  It is, today, because that is where it already was — but this lane made our
  tree depend on it by name for the first time and did not read its body.
