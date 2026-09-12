# WAVE 28 LANE FACTREST

Branch `wave28/factrest` off `wave28/base` (`5d2594de`), private worktree.

Scope: the `?CreateNewObject@` / `?CreateInPlaceObject@` pair for every **non-UI**
class in the absent-factory roster.

---

## 0. THE HEADLINE, AND WHAT IT IS NOT

**35 classes, 70 slots, 68 distinct functions ported and registered by default.**

**It is not 35 verified bodies.** Every row registers with **attendance 0** in an
ordinary run and nothing in this lane executed any of these bodies through a
door. That is the EXPECTED reading with the DLL present (Wave 11 lane OBJINT:
the DLL reaches its own factories through an in-module pointer stamped by a
static initialiser, which is not a door), and it means this lane **has measured
nothing about the correctness of any body**. The value is entirely at cutover.
See §4 for what is UNVERIFIED, in that word.

**The roster is 35 classes, not the 34 the kickoff says.** §1.

**Ten of the 35 classes are not constructible at all** — both factories are
`xor eax,eax ; ret` in retail. §2.1. That is a third of the roster that is two
instructions each, and it was invisible until the bodies were read.

---

## 1. THE ROSTER, RE-DERIVED (MEASURED)

Per the lane brief, derived here rather than taken on faith:

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_factory_census.py
  minted BaseTypeInfo rows            104
  slot-0 factory PRESENT in tree        2   (Dialog@UI, StringTable@UI)
  slot-0 factory ABSENT               101
  no factory in retail either           1   (?_typeInfo@Base@Object@@)
```

Filtering the 101 ABSENT rows to non-UI classes gives **35**, not 34:

| namespace | n | classes |
|---|---|---|
| Graphics | 11 | AnimationEvent, AnimationGroup, EdgeMesh, FrameCopyImageBuffer, GeneratedImageBuffer, GraphicsMesh, Material, MaterialInstance, Particle, ParticleSystem, ReflectionImageBuffer |
| Video | 7 | ImageBuffer, PixelProgram, RenderState, ShaderProgram, VertexLayout, VertexProgram, VideoProgram |
| Object | 6 | CompiledExternal, ExtensionCompiler, External, ExternalCompiler, ExternalList, Internal |
| Achievement | 4 | Achievement, AchievementInstance, Goal, GoalInstance |
| Collision2D | 2 | CollisionMesh, Surface |
| Audio | 2 | SoundBuffer, SoundEffect |
| IO | 1 | PackageFile |
| Pathfinding | 1 | PointList |
| Picking | 1 | PickingMesh |
| **total** | **35** | |

**The kickoff's own itemised lists sum to 35; its headline number says 34.**
11+7+6+4+2+2+1+1+1 = 35. Nothing was added or removed by this lane — the
arithmetic in `WAVE28_KICKOFF.md` §3 (and in the FACTREST brief) is simply one
short. No class is lost by it: every name in the kickoff's lists is ported here.

### 1.1 Absence established before a line was written (MEASURED)

`reference_grep_for_the_port_before_writing_it.md` and
`reference_half_registered_port_reads_as_absent.md`. All 70 mangled names AND
all 70 addresses, greped over 1,555 source files under `runtime/ code/
standalone/ engine/ dx11/`, excluding `standalone/rt_data_mint_tables.inc` (the
minted row itself — including it turns every row into a false hit):

```
HITS: none, in either direction, for any of the 70.
```

So none of the classes with already-ported *constructors or members* from
earlier waves (`MaterialInstance` wave24 GFXMAT, `EdgeMesh`/`Particle` Wave 27,
`PackageFile` Wave 27 IOTEXT, the Achievement family Wave 27 ACHIEVE) had its
FACTORY ported. The brief's warning was right to be issued and turned out not to
bite.

### 1.2 A shared slot that crosses lane boundaries (MEASURED — addressee below)

`?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z` (`0x1800070c0`) is
the slot-1 factory of **five** minted rows, not one:

```
AchievementInstance   (FACTREST)
GoalInstance          (FACTREST)
Internal              (FACTREST)
DialogInstance        (FACTUI1/FACTUI2)
Element               (FACTUI1/FACTUI2)
```

This lane ports and registers it (it is `xor eax,eax ; ret`). **Addressee:
lanes FACTUI1 and FACTUI2, and the Wave 28 integrator at merge.** If a UI lane
registers the same name to its own function, the later registration wins and one
of the two is dead code; the bodies are identical so nothing breaks, but the
merge should keep exactly one. Both were messaged during the wave.

---

## 2. THE BODIES (MEASURED)

Read from `objdump -d` over `C:\GOG Games\Banished\Runtime-x64.dll` **on disk**,
with the export directory parsed by `pefile` for names and RVAs. Ghidra was used
for nothing a port depends on (kickoff §5.2). Function extents came from an
export-boundary scan terminated at the first `int3` — **not** from `.pdata`,
which is a lower bound (`reference_pdata_is_a_lower_bound_on_function_extent.md`).

### 2.1 Three shapes, and ten classes that are not constructible

| shape | slots | what it is |
|---|---|---|
| (a) `xor eax,eax ; ret` | **20** | the class has no factory body in retail |
| (b) alloc + INLINED ctor | **26** | a straight run of stores, no control flow |
| (c) alloc + out-of-line `??0C@NS@@` call | **24** | the in-place half is `test rcx,rcx / jne <ctor>` |

Shape (a) covers **both** slots of ten classes: `AnimationEvent`,
`CompiledExternal`, `ExtensionCompiler`, `External`, `ExternalCompiler`,
`GeneratedImageBuffer`, `Goal`, `Internal` (new), `Particle`, `ShaderProgram` —
plus `Internal`'s in-place, shared by five rows (§1.2).

**This is a fact about the object model, not about the port:** these ten types
exist in the reflection table and are reachable by name, but the object model
cannot instantiate them — they are abstract bases (`External`, `Internal`,
`CompiledExternal`) or types only ever constructed by their owner
(`AnimationEvent`, `Particle`, `GeneratedImageBuffer`, `ShaderProgram`). A
cutover that hits one of these twenty slots and gets `nullptr` is getting
**retail's answer**, and any caller that then faults is a caller that would have
faulted against the shipped DLL too.

### 2.2 The allocation door, verified for this lane rather than inherited

Every shape-(b)/(c) `CreateNewObject` inlines the same lock / `?Allocate@` /
unlock triple. `?MemAlloc@System@@YAPEAX_KH@Z` (`0x1800c7a50`) is that triple out
of line. This lane disassembled `0x1800c7a50` and compared it instruction by
instruction against the inlined copy at `0x180001144-0x18000117c`
(`reference_a_written_recipe_can_be_wrong.md`): same singleton slot
(`?_singleton@MemoryAllocator@System@@`), same `+0x640` lock object, the same two
IAT slots `0x18014b390` / `0x18014b388` in the same order, same callee
`0x1800c6e60`. Wave 26 lane FACTORY's conclusion holds here — **checked, not
assumed.**

The allocation SIZE is read off the allocation, never inferred from the stores.
Two encodings appear and the transcription handles both: `mov $N,%edx ; lea
-K(%rdx),%r8d` for small N, and `mov $N,%edx ; mov $8,%r8d` for N ≥ 0x100. All
25 allocating classes are align 8.

### 2.3 The corroboration check (MEASURED)

For each shape-(b) class the transcribed stores were replayed over a byte map
and the written extent compared with the allocation size. **All 25 allocating
classes: extent ≤ allocSize, with no hole except field padding**, and the four
classes whose extent is short of the size are short by tail padding only. The
three larger holes are each exactly the range the out-of-line ctor writes
(`AchievementInstance` +0x8..+0xf ← `??0Internal@Object@@`, which writes +0x8 and
+0xc; `ReflectionImageBuffer` +0x8..+0x67 ← `??0ImageBuffer@Video@@`).

This corroborates the size; it is not its source.

### 2.4 Every symbol verified to EXIST (MEASURED — standing warning 5)

All **98** distinct mangled symbols the tables name (70 factories, 12 out-of-line
constructors, 16 `??_7` vtables) were looked up in the export directory by RVA
with `pefile` before being written into a registration or a resolve.
**98 present, 0 missing.** No name in this lane is a phantom
(`reference_resolveexport_phantom_names.md`), and none is justified by the word
"unexported" in a comment (`reference_aeaa_is_private_not_unexported.md`).

---

## 3. HOW IT IS BUILT

- `runtime/src/object/rt_factories.h` — op vocabulary and the class descriptor.
- `runtime/src/object/rt_factories_core.cpp` — the two entry shapes, the step
  interpreter, the locked allocator door, registration, `RtFactoriesPortInit()`.
- `runtime/src/<ns>/rt_factories_<ns>.inc` — the transcribed step tables. **Every
  step carries the retail instruction address it came from**, so any single row
  can be checked against `objdump -d` without reading the interpreter.
- `runtime/src/<ns>/rt_factories_<ns>.cpp` — one registrar each.

The step tables were produced by a **scratchpad transcription script** reading
`objdump` output, not typed by hand. The script is **not delivered**: this wave
carries no tooling (kickoff §5.1). What is delivered is its output plus the two
derived checks in §2.3 and §2.4, both of which are re-derivable from the
committed tables and the retail DLL by anyone.

### 3.1 Registration is DEFAULT ON, before the mint

`RtFactoriesPortInit()` is called from `standalone/resolve_standalone.cpp`
immediately after `RtUtilityPortInit()` and therefore **before**
`RtDataMintInit()`. That ordering was READ in the file, not assumed
(`resolve_standalone.cpp`: the port inits, then the mint). Kill switch
`BANISHED_RT_FACTREST`, **default ON** — `dialog_factory.cpp` has been dark since
Wave 11 because its switch is opt-in, and a registration that does not happen
cannot clear a trap.

Both doors are registered per slot: `RegisterRuntimeExport` (name) and
`RegisterRuntimeAddr` (address). Name-only leaves by-address consumers on the
DLL; address-only is invisible to `tools/runtime_census.py`.

---

## 4. WHAT IS UNVERIFIED

**UNVERIFIED: every one of the 70 slots.** No body in this lane was executed
through a door in any run. There is no live route: the DLL reaches its own
factories through an in-module pointer, so with the DLL present the ported rows
are registered and dead by construction. The attendance counters
(`RtFactoriesSummary()`) exist so a later DLL-ABSENT cutover can say whether a
body was entered, and so that a zero cannot be quietly read as "verified".

**UNVERIFIED: the 7 still-unported out-of-line constructors.** For the 14
shape-(c) rows this
lane ports the FACTORY and leaves the CONSTRUCTOR to whoever ports it. At
cutover a `Call` step whose name does not resolve prints a named FATAL in the
mint's own shape and returns null, rather than executing an EXEC-of-0. This is
**not** a guard over a faithful crash (project doctrine): retail calls these
unconditionally and always reaches a real body — the diagnostic says the
constructor behind a ported factory is itself unported. See §5.

**A VERIFICATION ROUTE EXISTS AND THIS LANE DID NOT RUN IT.** Lane FACTUI1
built an OFFLINE byte oracle for exactly this family and reports 26/26
in-place bodies and 14/14 out-of-line constructors byte-identical to the
shipped DLL's, with a negative control at 0/26 and 0/14: a short-lived
process `LoadLibrary`s the retail DLL, stands up `MemoryAllocator` and
`InternalFactory` through their own exported ctor+`Startup`, runs ours and
theirs on two DIFFERENTLY-poisoned buffers and compares only bytes either
side actually wrote. **That method applies unchanged to all 70 slots here and
none of them has been through it** — it lives on FACTUI1's branch and could
not run against this branch's tables before the merge. So "no oracle" was
never true for this family (`reference_no_route_is_not_no_oracle.md`); what
is true is that this lane did not have one in reach and says so.
**Addressee: the Wave 28 integrator on the merged tip, or Wave 29** — point
FACTUI1's harness at `runtime/src/*/rt_factories_*.inc`.

**MEASURED, not predicted: the census delta.** §6.

**NOT MEASURED AT ALL:** any statement about whether the cutover wall moves. This
lane clears 70 of the ~206 slots in the family. Attempt 31 is the measurement.

---

## 5. RESIDUALS, EACH WITH AN ADDRESSEE

1. **SEVEN out-of-line constructors are unported** — not twelve, and this lane
   nearly shipped the wrong number. A shape-(c) factory is only as good as its
   constructor at cutover, so the fourteen delegating rows were checked one at a
   time for an existing port, by NAME and by the registration call, after the
   first draft of this section had already asserted all twelve were missing
   (`reference_holdback_blockers_are_stale_check_them_first.md`, and lane
   FACTUI1's message is what prompted the re-check). MEASURED:

   | constructor | state |
   |---|---|
   | `??0EdgeMesh@Graphics@@` `0x1800a2830` | **PORTED**, `runtime/src/graphics/mesh.cpp` |
   | `??0FrameCopyImageBuffer@Graphics@@` `0x1800a3d10` | **PORTED**, `runtime/src/graphics/wave23_lane_gfxbody.cpp` |
   | `??0ImageBuffer@Video@@` `0x1800f2580` | **PORTED**, `runtime/src/video/buffers.cpp` |
   | `??0PackageFile@IO@@` `0x1800b4440` | **PORTED**, `runtime/src/io/package_file.cpp` |
   | `??0Internal@Object@@` `0x1800bfd10` | **PORTED**, `runtime/src/object/internal.cpp` (Wave 11 tier 2 — written, registered only under its opt-in `table` gate, which that file's own banner calls MANDATORY at cutover) |
   | `??0GraphicsMesh@Graphics@@` `0x1800ff170` | unported — **call-free store run** |
   | `??0ParticleSystem@Graphics@@` `0x1800a5320` | unported — **call-free store run** |
   | `??0PixelProgram@Video@@` `0x1800f3950` | unported — ends in a virtual call on the video Device |
   | `??0RenderState@Video@@` `0x18004b660`→`0x1800f4b60` | unported — same |
   | `??0VertexLayout@Video@@` `0x1800f5d50` | unported — same |
   | `??0VertexProgram@Video@@` `0x1800f6100` | unported — same |
   | `??0VideoProgram@Video@@` `0x1800f45b0` | unported — same |

   So **seven of the fourteen delegating rows are already whole** end to end
   (`AchievementInstance`, `GoalInstance`, `EdgeMesh`, `FrameCopyImageBuffer`,
   `ImageBuffer`, `ReflectionImageBuffer`, `PackageFile`), and seven are not.
   Two of the seven missing are call-free straight-line store runs and are the
   cheapest work left in this family; five are Video and each ends in a
   `PlatformConstruct` virtual call on the Device, which is Video-backend work,
   not factory work.
   **Addressee: WAVE 29, as a named lane, not as an opportunistic pickup** — and
   a cheap-wins list is the stalest section of any handover
   (`reference_cheap_wins_list_is_the_stalest_section.md`), so re-run the check
   above (`grep` for the `RegisterRuntime*` call, not for the name) before
   scoping it. The FATAL in §4 names the exact symbol at the exact moment it is
   needed, so a cutover boot identifies which of the seven actually blocks it.
2. **`?CreateInPlaceObject@Internal@Object@@` is registered by this lane and is
   also the slot-1 factory of two UI rows.** **Addressee: the Wave 28
   integrator** — keep exactly one registration at merge; the bodies are
   identical (`xor eax,eax ; ret`), so either is correct.
3. **The kickoff's "34 non-UI classes" is 35.** **Addressee: the Wave 28
   integrator, when re-deriving the board at close** — the correction is written
   here and in §1 rather than only in a commit message
   (`reference_a_correction_belongs_on_the_surface_the_next_person_reaches`).

---


## 6. CENSUS DELTA, ATTRIBUTED ROW BY ROW (MEASURED)

### 6.0 The board will NOT move, and that is not underdelivery

`tools/runtime_census.py` **cannot see this family at all.** Its `called`
population is "functions our tree calls today", and the only consumer of a
`?Create*Object@` is the minted `BaseTypeInfo` row — so no factory is in its
denominator. Lane FACTUI2 measured this with a two-sided control on its own
branch (its file present vs moved aside: UI 215/206 and TOTAL 1430/1277,
**identical both ways**, for 102 registrations), and confirmed `--csv` emits zero
rows whose key contains `CreateNewObject` — not this wave's, not Wave 26's
`StringTable@UI`, not Wave 11's `Dialog@UI`.

**So the correct board figure for this lane is +0, and the number to quote is
`tools/mint_factory_census.py` plus the live mint count below.** Stated here in
those words so a flat board at close is not read as three factory lanes
underdelivering. (MEASURED by FACTUI2 on its branch; INFERRED for this branch by
the same mechanism — this lane did not re-run that control.)

### 6.1 Static census

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_factory_census.py
                             wave28/base        this branch
  slot-0 factory PRESENT           2                 37
  slot-0 factory ABSENT          101                 66
```

**+35, and 35 is exactly this lane's class list** (§1). Not "about the right
amount" — one row per class, no unexplained movement.

### 6.2 LIVE census, with a two-run control (MEASURED)

The static screen counts SOURCE TEXT. The number that matters is the REGISTRY,
which is what `RtDataMintInit()` reads, and Wave 26 §0.1 is the standing reason
to distrust the static one on its own. Both runs are the same exe
(`standalone/build/reimpl-x64.exe`, deployed under `tools/game_dir_lock.sh`;
`assert-held` verified the deployed md5 was this build), `BANISHED_AUTOQUIT=1`:

```
run A   BANISHED_RT_FACTREST=0   [RtMint] 208 slots;   2 PORTED, 204 ABORT-TRAP
run B   default (switch ON)      [RtMint] 208 slots;  74 PORTED, 132 ABORT-TRAP
```

**+72 slots, and every one is accounted for:**

| slots | attribution |
|---|---|
| 70 | this lane's 35 classes x 2 |
| **+2** | `DialogInstance@UI` and `Element@UI` **slot-1**, UI rows owned by FACTUI1, whose slot-1 factory IS `?CreateInPlaceObject@Internal@Object@@` (§1.2). Registering Internal's in-place cleared them as a side effect. |
| 2 | unchanged in both runs: `StringTable@UI`, Wave 26. |

Verified directly in the roster the mint prints under
`BANISHED_RT_MINT_FACTORYLIST=1`: after this lane, rows `[27] DialogInstance` and
`[33] Element` list **slot0 only** as unported. The 2 extra slots are a measured
side effect with a named mechanism, not noise.

`Dialog@UI` is PRESENT in the static screen and contributes nothing to either
live run — it is still behind the opt-in `BANISHED_RT_DLGFACTORY`. That is the
exact defect §3.1 avoids, still visible in the same measurement.

### 6.3 Attendance (MEASURED, and it measures nothing about correctness)

```
[RtFactory] FACTREST registered 35 non-UI factory classes, 68 distinct slots,
            both doors each (2 slot-1 symbols were already bound by an earlier
            row and were NOT re-bound)
[RtFactory] FACTREST ATTENDANCE: classes=35 CreateNewObject=0
            CreateInPlaceObject=0 allocFail=0 ctorMissing=0
[RtFactory] FACTREST VERDICT: nothing in this run reached any of these bodies
            through a door. EXPECTED with the DLL present (Wave 11 lane OBJINT)
            and NOT evidence any body is correct.
```

**Thirty-five green attendance zeros. This lane has measured nothing about the
correctness of any of the seventy bodies**, and says so in those words, as the
brief requires. `standalone_crash.log` is empty on both runs; the boot is
otherwise unchanged.

### 6.4 Build gate

`mingw32-make standalone-debug` exits **0**. The cutover-manifest gate reports
**OK (60 rows; row set and every disposition match a fresh derivation)** with a
COSMETIC-only staleness warning — which is precisely what adding 136 by-name
doors does. See §7.2.

---

## 7. INSTRUMENT DEFECT OBSERVED, NOT FIXED

1. **`standalone/cutover_manifest.inc` goes stale on any lane that adds doors,
   and the gate says so on every build.** Emitted by `tools/cutover_manifest.py`
   from the `cutover manifest gate` step in `Makefile`. This lane adds 136
   by-name doors and trips it. **NOT FIXED and deliberately NOT refreshed:**
   three Wave 28 lanes are adding doors concurrently and each refreshing a
   generated file on its own branch produces a guaranteed merge conflict. Wave 26
   defect 4 already prescribes regenerating it **on the merged tip**.
   **Addressee: the Wave 28 integrator, after the merge** —
   `python tools/cutover_manifest.py --inc`.
2. **`tools/mint_factory_census.py`'s PRESENT screen is a whole-tree text grep,
   so a COMMENT counts as a hit** (`tools/mint_factory_census.py`, the PRESENT
   scan; the tool labels it "upper bound on ported"). It is therefore not a
   REGISTRY screen: a lane porting behind an opt-in switch scores on it exactly
   like one that registers by default — `Dialog@UI` does this today (§6.2).
   **NOT FIXED** (no tooling this wave). The live two-run control in §6.2 is what
   this lane used instead. **Addressee: Wave 29's factory lane(s), and the
   generator prescribed in WAVE26_LANE_FACTORY §4.4.**
3. **`tools/runtime_census.py` silently excludes the entire 206-slot factory
   family from its population** (§6.0), so it reports a flat board for a lane
   that cleared 72 abort traps, with no indication that it was not looking.
   **NOT FIXED.** **Addressee: the Wave 28 integrator when re-deriving the board,
   and Wave 29 if anyone proposes scoring factory work on the census.**
   (Measured by FACTUI2 with a two-sided control; this lane relies on that
   measurement rather than repeating it.)
4. **NOT an instrument defect, recorded so it is not mistaken for one:** the
   kickoff's "34 non-UI classes" is 35 (§1). The census is right; the prose is
   one short.

---

## 8. WHAT A FOLLOW-ON LANE SHOULD READ FIRST

- **The marginal cost in this family is the CONSTRUCTOR, and the brief's "the
  shape is uniform" understates it.** Measured from both ends independently:
  20 of 70 non-UI slots are `xor eax,eax ; ret`, 26 are fully inlined stores, and
  24 delegate to a separately-exported `??0` — and FACTUI2 measured the same
  three-way split on the UI half (6 inlined, 8 mixed, 21 pure delegation of 35).
  For the delegating shape the "three variable parts readable mechanically" is
  really only the allocation size.
- **Seven** unported constructors (§5.1 — NOT twelve; five were already ported
  and this lane's first draft had that wrong) are the only thing between a
  shape-(c) row and a working cutover, and two of them
  (`??0GraphicsMesh@Graphics@@` `0x1800ff170`, `??0ParticleSystem@Graphics@@`
  `0x1800a5320`) are call-free store runs. The other five are Video and each
  ends in a `PlatformConstruct` virtual call on the Device.
- **Do not read "74 PORTED" as "74 working factories."** It means 74 slots now
  resolve to one of our functions. Whether those functions do the right thing is
  untested, by construction, until a DLL-absent boot runs one.
