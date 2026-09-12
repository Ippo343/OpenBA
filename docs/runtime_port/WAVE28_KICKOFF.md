# WAVE 28 KICKOFF — 2026-09-12

Base: `wave28/base`, cut from `wave27/base` at `f5217d3a`.
**Seven lanes, every one a DECOMPILATION lane.** No lane in this wave is scoped
to build, fix or extend a tool. Second consecutive wave under that rule; it held
in all seven lanes of Wave 27 and it holds here.

## 1. BOARD, RE-DERIVED ON THIS TIP

`tools/runtime_census.py` run by the integrator on `f5217d3a`. Figures agree with
WAVE27_CLOSED.md but are NOT quoted from it.

```
TOTAL   1430 called / 1277 ported / 89.3%      gap 153
gap 153 = 26 BODY + 1 DTOR + 1 OPNEWDEL + 38 VTABLE + 49 DATA + 38 UNKNOWN
```

**Do not scope anything on "153 rows".** The honest decomposition:

- **7 of the 26 BODY rows are the allocator** — WRITTEN, held behind
  `BANISHED_RT_ALLOC`. Not this wave's work, not any lane's scope.
- **3 more IO BODY rows are instrument-only** (`TextFileStream::GetObjectName/
  Type/Version`, reached only by `file_stream_probe.cpp`). The census itself says
  "named here so no lane is sent after one as body work". Lane SLOTS owns the
  decision on whether the real vtable slots behind them are in scope; nobody
  ports them *for the probe*.
- **20 of the 38 VTABLE rows are tagged UNPROMOTABLE.** Real vtable work is 18.
- **49 DATA rows have no body to write.** Not decompilation. Not in scope.

So the real surface this wave is scoped against:

```
   19 named unwritten bodies (Graphics 12, IO 1, System 1, Video 1, Audio 1, UI 2)
+ ~23 unported SLOTS of incomplete promotion-backlog tables
+ 202 factory bodies across 101 classes   <-- THE CUTOVER WALL
```

## 2. THE FACTORY FAMILY, AND WHY THREE LANES ARE ON IT

Cutover attempt 30 died on `?CreateNewObject@SpriteSheet@UI@@` — the mint's own
per-slot FATAL. That is **one member of a 206-slot family** measured by Wave 26
lane FACTORY: 104 minted `?_typeInfo@...@` rows, each carrying `factoryFn` (+0x20)
and `inPlaceFactoryFn` (+0x28).

Re-derived on THIS tip, as FACTORY's residual #1 demands (do not quote its
numbers, re-run the command):

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_factory_census.py \
    --self-test --expect-unported "?CreateNewObject@SpriteSheet@UI@@SAPEAVBase@Object@@XZ"

  minted BaseTypeInfo rows            104
  slot-0 factory PRESENT in tree        2   (Dialog@UI, StringTable@UI)
  slot-0 factory ABSENT               101
  no factory in retail either           1   (?_typeInfo@Base@Object@@)
  BIAS CHECK  positive-present PASS, positive-absent PASS
```

**At one name per cutover boot, enumerating this family costs ~200 attempts.**
Four consecutive waves have been gated one symbol at a time.

### 2.1 The prescription this wave is deliberately NOT following

WAVE26_LANE_FACTORY.md §4.4 says the right answer is **a generator, not a
porting lane**, and it argues that well. This wave overrides it on an explicit
instruction that the wave carry no tooling. **That override is recorded here so
it is not mistaken for ignorance of the prescription.** If the three factory
lanes underdeliver, the generator is Wave 29's answer and FACTORY §4 is still
the brief for it.

What makes the hand-port viable without a tool:

- The shape is uniform. Every `CreateNewObject` read so far: locked
  `MemAlloc(N, 8)`, retail's null check, an inlined ctor, return. Every
  `CreateInPlaceObject` is a four-line shape. The variable parts are the
  allocation size, the vptr sequence the inlined ctor stamps, and the zeroed
  field extent — **all three readable mechanically from the disassembly.**
- `BANISHED_RT_MINT_FACTORYLIST=1` **already exists** and dumps the whole
  remaining roster in one log. No tool to write to enumerate progress.

### 2.2 The three caveats that decide whether these lanes are worth anything

1. **REGISTER BY DEFAULT.** `Dialog@UI` has been ported since Wave 11 and
   contributed NOTHING to the live count because `BANISHED_RT_DLGFACTORY` is
   opt-in. A lane that ports 34 factories behind opt-in switches moves the static
   census and the cutover wall **not at all**. The gate is the REGISTRY, not the
   tree. Follow `stringtable_factory.cpp`'s pattern: kill switch, **default ON**.
2. **Registration must precede `RtDataMintInit()`.** `RtUiPortInit()`
   (`standalone/resolve_standalone.cpp:333`) runs before `RtDataMintInit()`
   (`:406`). That ordering is what makes a registration clear a trap. Read it;
   do not assume it for a new file.
3. **Every one of these rows will register with attendance 0 in an ordinary
   run, and that is EXPECTED, not a defect.** Wave 11 lane OBJINT measured that
   the DLL reaches its own factories through an in-module pointer stamped by a
   static initialiser — not a door. The value of these ports is **entirely at
   cutover**. A lane that ports thirty of these and reports thirty green
   attendance zeros **has measured nothing**, and must say so in those words.

## 3. LANES

| Lane | Branch | Scope |
|---|---|---|
| FACTUI1  | `wave28/factui1`  | Factory pairs for UI classes sorting BEFORE `ImageDescription` (~31 classes) |
| FACTUI2  | `wave28/factui2`  | Factory pairs for UI classes from `ImageDescription` onward (~36), **`SpriteSheet` FIRST** |
| FACTREST | `wave28/factrest` | Factory pairs for all 34 non-UI classes (Graphics 11, Video 7, Object 6, Achievement 4, Collision2D 2, Audio 2, IO 1, Pathfinding 1, Picking 1) |
| GFXBODY  | `wave28/gfxbody`  | The 10 remaining named Graphics BODY rows (AnimationState x3, MaterialInstance x2, Material x2, Scene, Renderer, Animation) |
| PARTSYS  | `wave28/partsys`  | `?Update@ParticleSystemInstance@` (`0x1800a5600`, 0x1479 bytes), PSI slot 10, `SunLight` 6/11, `ParticleInstance::RemoveState` |
| SLOTS    | `wave28/slots`    | `0x180157c60` 0/7, `MeshInstance` 5/9, `EdgeMeshInstance` 7/9, `TextFileStream` 8/10, `Stream@Audio` 1/2, `0x180155860` 1/2 |
| MISC     | `wave28/misc`     | `Audio::Scene::Update`, `Modification::SetupFileSystem`, `Random::NextGroup`, `Context::SetTransformScale`, `StringTable` dtor + op delete + `Serialize` |

Every factory class, every promotable vtable and every named body row is owned by
**exactly one** lane. No lane may touch another lane's class or table.

### 3.1 UNPROMOTABLE filter, applied BEFORE these briefs were written

```
grep -o 'RT_UNPROMOTABLE: [^ |]*' standalone/reg_mint_promoted.cpp | sort -u
```

No lane below is handed a table tagged UNPROMOTABLE. Video's entire 12-row vtable
gap is UNPROMOTABLE, as are `??_7Base/CompiledExternal/External/Internal@Object@@`,
`??_7Dialog/DialogController@UI@@` and `??_7ReaderWriter/Stream@IO@@`.

**Note the asymmetry, and it bit Wave 27:** a class can be UNPROMOTABLE as a
*vtable* and still need its *factory* ported, because the factory is reached
through the minted `BaseTypeInfo`, not through the vtable. `DialogController@UI`
and `CompiledExternal@Object` are exactly this. Their factories ARE in scope for
FACTUI1/FACTREST; their vtables are not in scope for anyone.

## 4. WHAT IS EXPLICITLY NOT IN THIS WAVE

- **No verification lane.** Wave 27 shipped ~70 bodies that are registered and
  never executed, and named that its real debt. This wave does not pay it. Each
  lane verifies its own bodies where a live route already exists and records
  **UNVERIFIED** in those words otherwise. The debt rolls to Wave 29 and it grows
  this wave. **Nobody may report "ported + registered" as "verified".**
- **No tool built, fixed or extended.** Twelve Wave 27 instrument defects are in
  `WAVE27_INSTRUMENT_LEDGER.md`, unfixed, and this wave will add more.
- **The allocator.** Arming `BANISHED_RT_ALLOC` is a measurement and a risk
  decision, not a porting lane.

## 5. STANDING WARNINGS FOR EVERY LANE

1. **NO TOOLING.** If you find an instrument defect, **write it down and keep
   porting.** Record it in your lane doc under a heading `INSTRUMENT DEFECT
   OBSERVED, NOT FIXED`, name the file and line, move on.
2. **Ghidra for names, comments, types, navigation. `objdump` and `pefile`
   against the retail file on disk for any BYTE, ADDRESS, EXTENT or BODY a port
   depends on.** Wave 27 hit five live GhidraMCP defects and all five were one
   shape: the tool answers about a subject it has not verified is the one you
   named. **Two Ghidra calls are one source.**
3. **Count the STORES, not the fields.** `MeshInstance::GetInstanceTransform`
   performs 16 `MOVSS` stores and only 12 come from `AsMatrix` — the w column is
   `_color[0..3]`. The obvious port passes every geometric check and renders
   everything untinted. A mismatch between the writes a body performs and the
   writes its named helper accounts for is the signal, and it is **visible in
   disassembly and invisible in decompiled C**.
4. **Lift every magic constant; never retype a recognised algorithm.**
   `ParticleInstance::SetParticle` inlines a Mersenne Twister with masks
   `0xff3a58ad` / `0xffffdf8c`, not the textbook ones. RNG output is supposed to
   look arbitrary, so nothing catches a wrong constant. Recognising the algorithm
   is exactly what licenses you to stop reading the bytes — do not.
5. **A registration that does not move the census is a PHANTOM NAME.**
   `ResolveExport` returns NULL silently for a name the DLL lacks and the
   `ResolveAddr` fallback hides it. `UEBA` vs `QEBA` by analogy with neighbouring
   slots cost Wave 27 a dark port. **Attribute your census delta row by row. An
   unexplained board movement is a finding, not noise** — "about the right
   amount" is no check.
6. **OWNERSHIP, not element type, decides teardown.** `Array<ExternalPtr<Goal>>`
   looks unmistakably refcounted and is not: `GoalList::Initialize` MOVES each
   `Goal*` out, so the array owns nothing by teardown and the refcounted recipe
   would OVER-RELEASE every Goal. **The directions are not symmetric** —
   under-release leaks, over-release frees a live object. The safe-looking reflex
   is the more dangerous error. Establish ownership by reading the WRITER.
7. **Do not "fix" a faithful crash with a null check.** Something upstream handed
   you a null. Find it. A guard does not restore the behaviour, it skips it.
8. **Grep for the port before writing it.** A body PORTED but never REGISTERED is
   indistinguishable from an ABSENT one to a callee census. Grep by NAME **and**
   by ADDRESS, scoped to `code/ engine/ runtime/ standalone/ dx11/`.
9. **`AEAA` means PRIVATE, not unexported.** This DLL exports its private
   members. An address door justified by the word "unexported" in a comment is
   usually a plain name door.
10. **`deleting_dtor.cpp`'s table is POSITIONAL and it cost Wave 27 a merge
    hazard in full.** `DeletingDtorCore(self, flags, N)` indexes `kSpecs[N]`.
    Three lanes each appended starting at index 44 on their own branch; a
    keep-both merge silently dispatches the wrong destructor with no diagnostic,
    and the `static_assert` checks the COUNT ONLY. The count is now 50. **If you
    append, read `deleting_dtor_internal.h` beside the constant first, and say in
    your lane doc exactly which indices you claimed.**
11. **Report what you MEASURED separately from what you INFER.** If you did not
    run it, say so. Every closed wave in this project has a defect of the form
    "an asserted number sat beside a measured one and inherited its credibility".
12. **A residual needs an ADDRESSEE, not just a condition.** "The next lane that
    touches this" satisfies the checklist and waits forever. Name a lane, a wave,
    or a person. Four well-formed-and-wrong expiry clauses have been found.
13. **Repair the LICENCE, not the sentence.** Wave 27's ACHIEVE and LOOPS drew
    the SAME unlicensed inference from the same evidence; LOOPS' conclusion was
    false and ACHIEVE's was true. Correcting only the false one leaves the
    mechanism intact. If you catch yourself fixing a conclusion, fix the
    reasoning step that produced it.
14. **Never `find /`.** Grep/Glob scoped to your worktree.
15. **Put a correction on the surface the next person will REACH** — beside the
    row, in the file they will run, in a tool's printed output. A commit message
    is a permanently uncorrectable artefact.

## 6. DELIVERABLE

Each lane writes `docs/runtime_port/WAVE28_LANE_<NAME>.md` and commits on its own
branch in its own private worktree. **Write the doc as you go, not at the end.**

Close your lane with:

- the census delta, **attributed row by row** (warning 5),
- what you MEASURED vs what you INFER, separated,
- what you did NOT verify, in the word UNVERIFIED,
- instrument defects observed and not fixed,
- residuals, each with an addressee.

The integrator merges, re-derives the board, and runs cutover attempt 31.
