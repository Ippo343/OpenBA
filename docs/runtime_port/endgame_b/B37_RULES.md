# B.3 step 7 — `Engine`, `Save`, `Achievement`, `Audio`, `Input`, `Pathfinding`, `Picking`, `Application` become real C++ classes: the rule sheet

Written 2026-09-26 by block 0 (the question pass) on main `96b03474`. This is the sheet the conversion
agents are handed. It answers `NEXT_SESSION.md` §2b's eight questions from the RETAIL `Runtime-x64.dll`
(15290 functions, image base 0x180000000), from `Application-x64.exe` (11473 functions) where the
question is about the exe, and from the tree. It supersedes `STEP7_INVENTORY.md` wherever the two
disagree — the inventory is a map, this is the evidence. §13 lists what is still NOT VERIFIED; nothing
outside §13 was taken on trust.

Standing rules from `B34B_RULES.md` §2d, `B35_RULES.md` §8 (B5-1..17, B5-12's coordinator amendment),
`B36_RULES.md` §8, §10 and §14, and `NEXT_SESSION.md` §2d apply unchanged and are not repeated except
where this batch instantiates them. `ENDGAME.md` §B.1/§B.2 decisions apply unchanged: retail layout
kept, slots in retail order, **no virtual destructor** (slot 0 is `DeletingDtor(unsigned)` where slot
0 IS a deleting dtor — see B7-1 for the tables where it is not), no real ctor on a polymorphic class,
doors delegate with a QUALIFIED non-virtual call.

## 0. How this sheet was measured

| fact | instrument |
|---|---|
| vtable addresses, slot count, each slot's target + retail name | `b37_session/ghidra/Step7Appendix.java` (parameterised from `b36_session/ghidra/UiAppendix.java`): qwords over `.rdata`, terminating on the next `??_7`/`??_R` symbol, a zero qword or a non-code target. NOT decompiled C. |
| `sizeof` of every polymorphic class | the `MOV EDX,imm` in its slot-0 vector deleting destructor, read as DISASSEMBLY; where slot 0 is not a deleting dtor (Loader, Stream, IPathInfo, Thread) from the ctor's last store and the holder (`B37_SLOTS.md` §1a) |
| `sizeof` of the non-polymorphic classes | the allocation immediately before the ctor call: `Engine::Startup` (0x1800db8a0) in the DLL, `GameMain`/`GameUpdateLoop::Load` in the exe (`MemAlloc(size, align)`), plus each ctor's last store |
| polymorphism of a class with no exported `??_7` | the head of EVERY `??0`/`??1` export of every class in the set, with every LEA into data and its symbol (`B37_SLOTS.md` §7) |
| base class | `?_ParentTypeName@X@NS@@CAPEBDXZ`'s returned literal (only 8 classes carry one), the base ctor each ctor CALLs, and the stamp chain |
| by-value returns | every export whose mangled return is `?AV`/`?AU`; the prologue of each one read |
| by-value ARGUMENTS | Ghidra's MicrosoftDemangler signature for every export; every parameter that is a class not behind `*`/`&` got its prologue read as disassembly (`B37_SLOTS.md` §8) |
| exe subclasses | the exe mode of the same script: the five named loop tables, every exe call site of a step-7 ctor/dtor import with the LEA'd `.rdata` table after it, the undefined `FF 25` JMP stubs decoded to their import names |
| threading | `Loader::Run` 0x1800dbfc0, `Loader::Load` 0x1800dc050, `Stream::Run` 0x1800a0f60 and the seven voice-callback bodies read as disassembly |
| exe import table | `pefile` over `Application-x64.exe` and `VideoDX11-x64.dll` |
| our tree | `Grep`/`Glob` scoped to the worktree, build dirs excluded; the hex-literal sweep of `B37_SLOTS.md` §T; the three read-only sibling reports (`B37_FILEMAP.md` by b37-0F, `b37_session/EXE_SUBCLASSES.md` and `b37_session/CALLER_SURFACE.md` by b37-0X), summarised with attribution in §3, §6.3 and §7 |
| script versions | run 1 (DLL) was made before the exe mode was added to `Step7Appendix.java`; the DLL code path is unchanged in the checked-in file, which also carries the exe mode used for run 2 |

**The raw data is `B37_SLOTS.md`** (and `b37_session/step7_appendix_raw.txt`, the unabridged script
output of both runs). Copy from it rather than from the tables here.

---

## 1. Q1 — THE CLASS LIST, SETTLED

**30 classes carry exports in the eight namespaces (618 exports; `B37_SLOTS.md` §6).** By shape:

- **8 `Object::Base` classes** (a `??_7`, a `?_typeInfo@`, a `_ParentTypeName`): `Achievement::Achievement`
  (External), `Achievement::AchievementInstance` (Internal), `Achievement::Goal` (External, ABSTRACT),
  `Achievement::GoalInstance` (Internal), `Audio::SoundBuffer` (**CompiledExternal**), `Audio::SoundEffect`
  (External), `Pathfinding::PointList` (External), `Picking::PickingMesh` (External).
- **4 Engine loop classes — their OWN root, NOT `Object::Base`:** `Engine::UpdateLoop` (ABSTRACT),
  `Engine::LoaderLoop` (CONCRETE), `Engine::DialogLoopT` (ABSTRACT), `Engine::DialogLoaderLoopT` (ABSTRACT).
  No `_typeInfo`, no type-name literal; slot 1 is `Startup`, not `TypeName`.
- **2 `System::Thread` subclasses:** `Engine::Loader`, `Audio::Stream` (slot 0 `Run`, slot 1 inherited
  `?OnCompleted@Thread@System@@`; no deleting dtor in the table; non-virtual `QEAA` destructors).
- **1 pure interface:** `Pathfinding::IPathInfo` — 2 slots, both `_purecall`, no dtor slot, 8 bytes.
- **1 unexported polymorphic class:** the `IXAudio2VoiceCallback` implementation whose table is at
  0x180157c60 (7 slots), 64 records embedded in `Audio::Device` at +0xa98. No name in the binary.
- **15 non-polymorphic classes:** `Engine::Engine`, `Engine::MainLoop`, `Application::Application`,
  `Achievement::AchievementManager`, `Achievement::GoalList`, `Audio::Configuration`, `Audio::Device`,
  `Audio::Scene`, `Input::Configuration`, `Input::Device`, `Input::KeyMap`, `Pathfinding::Scene`,
  `Picking::Scene`, `Save::SaveLoad`, `Save::SavedState`.
- Nested value types with NO exports of their own, layout-only, belong to their holder's block:
  `Save::SaveLoad::SaveInfo` / `ModInfo` (retail NESTS them in `SaveLoad`), `Audio::Device::AudioChannel` /
  `PlatformChannel`, `Audio::Scene::AttenutedEffect` (retail's spelling) / `SoundType`, `Input::Device::State` /
  `InputDevice`, `Input::KeyMap`'s `Binding`/`BindingNode`, `Picking::PickingMesh::SubMesh` / `Triangle`,
  `Picking::Scene::ClipInfo` (+ the tree's `Cell`/`Link`/`Entry`), `Pathfinding::PointList::Point`,
  `Pathfinding::Scene::Node` / `Link`, `Achievement::Goal::GoalState`.

**Per namespace (exports):** Achievement 127, Audio 144, Engine 116, Input 67, Picking 51, Pathfinding 48,
Save 38, Application 27.

**Contradictions of `STEP7_INVENTORY.md` (each read from bytes):**
1. **`Achievement::Achievement` IS polymorphic** — `??0Achievement@0@QEAA@XZ` (0x18009e240) stamps
   `??_7Base@Object` → `??_7External@Object` → `??_7Achievement@0@6B@` (0x180154740, 9 slots). The mint
   already carries it (`rt_data_mint_tables.inc:3889`); the inventory's grep `@Achievement@@6B@` missed
   the MSVC back-reference spelling `@0@`. **The mint has 16 rows for the set, not 14** (+ this one, +
   the unnamed 0x180157c60 row at `:3888`), and **8 `_typeInfo` rows, not 7** (`?_typeInfo@Achievement@1@`).
2. **`Achievement::GoalList` has NO `RT_DELDTOR` row** (and no vtable): the four Achievement rows at
   `deleting_dtor.cpp:288-291` are `Achievement`, `AchievementInstance`, `Goal`, `GoalInstance`.
   `GoalList` is a 0x10 value type (one `Array<InternalPtr<GoalInstance>>`) with a non-virtual `QEAA`
   dtor (0x18009f420) that deletes each element through its slot 0.
3. **`Application` has 27 named exports**, all spelled `@Application@1@` / `??0Application@0@` (§6).
4. **`Pathfinding` is 48 exports in THREE classes**, not "`PointList` + `IPathInfo`": `Pathfinding::Scene`
   (19 exports) exists. `PointList` is fully ported already (`pointlist.cpp`: 9 slots + dtor) and LIVE (§6.2).
5. **`Engine::MainLoop` is NOT polymorphic** — confirmed: ctor 0x1800dd6b0 `XOR EAX,EAX / MOV [RCX],RAX`
   (the Array data pointer), no `??_7` stamped anywhere in it; dtor `??1MainLoop@Engine@@QEAA@XZ` is `QEAA`.
   `runtime/types/engine/MainLoop.h:41-51` is RIGHT.
6. **Engine has 0 `_typeInfo` rows because no Engine class is an `Object::Base`** — the loops are their
   own root (§2.1), `Engine`/`MainLoop` are non-polymorphic.

## 2. Q1/Q6 — PER CLASS, FROM RETAIL BYTES

### 2.1 The hierarchy (every edge is a read literal, a base-ctor CALL, or a stamp chain)

```
Object::Base (3 slots)
├─ Object::External (8)
│  ├─ Achievement::Achievement 0x58 (9: 3 GetFileVersion, 4 Compile=External's, 5 Serialize,
│  │                                 6 Reconstruct, 7 Destruct, 8 Construct)    literal "Object::External"
│  │     └─ [exe] Scenario (9-slot table 0x14015f020)                              §3
│  ├─ Achievement::Goal 0x30 (10)  ABSTRACT: 6,7,8 _purecall; 9 GetInstanceType RETSLOT   "Object::External"
│  │     └─ [exe] BuildingGoal, PopulationGoal, StatGoal, TimeGoal, FinishTutorialGoal (11-slot tables) §3
│  ├─ Audio::SoundEffect 0x40 (9: overrides 4 Compile)                             "Object::External"
│  ├─ Pathfinding::PointList 0x50 (9: overrides 4 Compile)                         "Object::External"
│  ├─ Picking::PickingMesh 0x48 (9: overrides 4 Compile)                           "Object::External"
│  └─ Object::CompiledExternal (8)
│        └─ Audio::SoundBuffer 0x80 (9: 4 Compile=External's)                      "Object::CompiledExternal"
└─ Object::Internal (4)
   ├─ Achievement::AchievementInstance 0x28 (4)                                    "Object::Internal"
   │     └─ [exe] 4-slot table 0x14015e350 (built in GameUpdateLoop's slot-6 Load)  §3
   └─ Achievement::GoalInstance 0x20 (7: 4 Startup, 5 Update(const Time&, const GoalList&),
         6 CheckGoalState → enum)                                                  "Object::Internal"
         └─ [exe] FinishTutorialInstance, TimeGoalInstance (7-slot tables)          §3

Engine::UpdateLoop 0x28 (12)  ROOT, ABSTRACT: 1 Startup, 2 Shutdown, 3 Update, 4 Draw, 6 Load _purecall;
│   0 DeletingDtor, 5 DrawFailed, 7 SaveRestore(bool, Save::SavedState*), 8 OnVideoError,
│   9 OnVideoRestart, 10 GetResourceFilename, 11 AllowsModChanges — real bodies
├─ Engine::LoaderLoop 0x48 (12)  CONCRETE: overrides 0,1,2,3,4,6,8
│  └─ Engine::DialogLoaderLoopT 0x188 (13)  ABSTRACT: overrides 0,1,2,3,4,6; slot 12 NEW, _purecall
└─ Engine::DialogLoopT 0x168 (13)  ABSTRACT: overrides 0,1,2,3,4,6; slot 12 NEW, _purecall
   (exe subclasses of all four: §3)

System::Thread 0x30 (2)  ABSTRACT: 0 Run _purecall, 1 OnCompleted   [a REAL class since B.3.4b, Thread.h:221]
├─ Engine::Loader 0x38 (2: 0 Run 0x1800dbfc0)   the only instance: static LoaderLoop::_loader (0x1801fe7b8)
└─ Audio::Stream 0x88 (2: 0 Run 0x1800a0f60)    held BY VALUE in Audio::Device at +0xa00

Pathfinding::IPathInfo 8 (2)  pure interface: slots 0 and 1 _purecall, no dtor slot

(unexported) VoiceCallback record 0x20 (7)  IXAudio2VoiceCallback: 2 OnStreamEnd, 4 OnBufferEnd real, 5 x RET 0

NON-POLYMORPHIC ROOTS: Engine::Engine ≥0x6f2 (0x6f8 inferred), Engine::MainLoop 0xa0,
  Application::Application 0x30, Achievement::AchievementManager 0x48, Achievement::GoalList 0x10,
  Audio::Configuration 0x10, Audio::Device 0x1298, Audio::Scene 0x38, Input::Configuration 0x10,
  Input::Device 0x1200, Input::KeyMap 0x18, Pathfinding::Scene ≥0x90, Picking::Scene 0x60,
  Save::SaveLoad 0x90, Save::SavedState 0x10
```

**Multiple inheritance — NONE.** Every `??_7` store in every ctor/dtor of the set is `MOV qword [this],RAX`
at offset 0 (`B37_SLOTS.md` §7 and the raw SECTION 7). `Audio::Device`'s 64 stamps of 0x180157c60 are
into MEMBER records at +0xa98 stride 0x20, not a second vptr of `Device`.

### 2.2 Slot lists — the shapes, and the traps read from bytes

All slot lists are in `B37_SLOTS.md` §1.

**RULE B7-1 (slot-order traps).**
1. **The four loops are NOT `Object::Base` classes.** Slot 1 is `Startup`, slot 2 `Shutdown`, slot 3
   `Update(const System::Time&)`, slot 4 `Draw(float)`, slot 6 `Load`. A loop class that inherits
   `Object::Base`, or declares `TypeName`/`Type`/`Persist`, is wrong. No `DEFINE_TYPE`, no literal.
2. **`UpdateLoop` is ABSTRACT with FIVE pure slots (1, 2, 3, 4, 6)**; `LoaderLoop` overrides all five and
   is CONCRETE; `DialogLoopT` and `DialogLoaderLoopT` introduce slot 12 as `_purecall` and are ABSTRACT.
   Slot 12 has no attested name (the exe fills it with a dialog-creating body — `MainMenuDialog_Ctor`,
   `LoaderDialog_Ctor3` in Ghidra's auto-names); `runtime/types/engine/UpdateLoop.h`'s `kSlot_CreateDialog`
   is descriptive, keep it as a comment-level name.
3. **`DialogLoaderLoopT` inherits `LoaderLoop::OnVideoError` (slot 8, 0x1800dc360); `DialogLoopT` inherits
   `UpdateLoop::OnVideoError` (0x180031760).** Neither Dialog loop overrides slots 5, 7, 9, 10, 11.
   `virtual`, not `override`, on `UpdateLoop`'s twelve and on each Dialog loop's slot 12.
4. **The two `System::Thread` subclasses have NO deleting dtor slot.** Slot 0 is `Run`. They cannot take
   `DeletingDtor(unsigned) = 0`; their destructors are non-virtual `QEAA` exports that re-stamp
   `??_7Thread@System@@` before joining the thread (`??1Loader` 0x18004d330, `??1Stream` 0x1800a0d90).
5. **`IPathInfo` has no dtor slot either**: two pure virtuals whose names are not attested (the mint row
   `kVSlots70` is two `RT_MINT_PURECALL`s). Declare `virtual … Slot0() = 0; virtual … Slot1() = 0;`
   only if something needs the class at all (§6.2: nothing does).
6. **`Goal`'s pure slots are 6 `Reconstruct`, 7 `Destruct`, 8 `Construct`** (inherited-from External for 6
   and 7) and slot 9 `GetInstanceType` is Goal's own, a RETSLOT virtual (B7-4). `Construct` sits at slot
   8 on all six External leaves here (`Achievement`, `SoundBuffer`, `SoundEffect`, `PointList`,
   `PickingMesh`) — the B5-16 resource shape, no `CreateInstance` in front of it.
7. **`Compile` (slot 4) is overridden by exactly three:** `SoundEffect` 0x1800e09a0, `PointList` 0x18010f6d0,
   `PickingMesh` 0x180122950. `Achievement`, `Goal`, `SoundBuffer` inherit `?Compile@External@Object@@`
   (0x1800066e0). A block that declares `Compile` on the other three is wrong.
8. **`GoalInstance` numbers its virtuals 4 `Startup`, 5 `Update(const Time&, const GoalList&)`, 6
   `CheckGoalState() const → Goal::GoalState` (an enum in EAX, not a retslot).**
   `runtime/types/achievement/Goal.h:191-203` agrees.

### 2.3 `sizeof` — every class read, against every `static_assert(sizeof)` in `runtime/types/<ns>/`

| class | retail | instrument | tree | verdict |
|---|---|---|---|---|
| `Achievement::Achievement` | 0x58 | slot 0 `MOV EDX,0x58` | `AchievementManager.h:112` 0x58 | agree |
| `Achievement::AchievementInstance` | 0x28 | `MOV EDX,0x28` | `AchievementManager.h:139` 0x28 | agree |
| `Achievement::AchievementManager` | 0x48 | `Engine::Startup` `MemAlloc(0x48)` 0x1800dbe42 | `AchievementManager.h:212` 0x48 | agree |
| `Achievement::Goal` | 0x30 | `MOV EDX,0x30` | `Goal.h:80` 0x30 | agree |
| `Achievement::GoalInstance` | 0x20 | `MOV EDX,0x20` | `Goal.h:138` 0x20 | agree |
| `Achievement::GoalList` | 0x10 | ctor stores +0/+8; embedded at AchievementInstance+0x10..+0x1f | `Goal.h:178` 0x10 **and** `AchievementManager.h:55` `GoalListView` 0x10 | agree — but TWO definitions (§4) |
| `Application::Application` | 0x30 | `MemAlloc(0x30)` 0x1800dbbb3 | `Application.h:155` 0x30 | agree |
| `Audio::Configuration` | 0x10 | `MemAlloc(0x10)` 0x1800dbd00 | `Audio.h:198` 0x10 | agree |
| `Audio::Device` | 0x1298 | `MemAlloc(0x1298)` 0x1800dbdd4 | none (no struct; `audio_device.cpp` offsets) | undocumented |
| `Audio::Scene` | 0x38 | exe `MemAlloc(0x38,8)` 0x14009706a | `Audio.h:178` 0x38 | agree |
| `Audio::SoundBuffer` | 0x80 | `MOV EDX,0x80` | none (defined in `vtbatch7_soundbuffer_soundeffect.cpp`) | undocumented |
| `Audio::SoundEffect` | 0x40 | `MOV EDX,0x40` | none (same file) | undocumented |
| `Audio::Stream` | 0x88 | `Device+0xa00` → next member `+0xa88` | none (`audio_stream_win.cpp`) | undocumented |
| `Engine::Engine` | ≥0x6f2 | ctor's last store `MOV word [RDI+0x6f0]`; exe holds it on the stack | `Engine.h:204` 0x6f8 "inferred" | consistent; still inferred |
| `Engine::MainLoop` | 0xa0 | exe `MemAlloc(0xa0,8)` 0x140041f77 | `MainLoop.h:270` 0xa0 | agree |
| `Engine::UpdateLoop` | 0x28 | `MOV EDX,0x28` | `UpdateLoop.h:91` 0x28 | agree |
| `Engine::Loader` | 0x38 | ctor last store +0x30; static init writes `_loader+0x30` | `UpdateLoop.h:150` 0x38 | agree |
| `Engine::LoaderLoop` | 0x48 | `MOV EDX,0x48` | `UpdateLoop.h:184` 0x48 | agree |
| `Engine::DialogLoopT` | 0x168 | `MOV EDX,0x168` | `UpdateLoop.h:209` 0x168 | agree |
| `Engine::DialogLoaderLoopT` | 0x188 | `MOV EDX,0x188` | `UpdateLoop.h:232` 0x188 | agree |
| `Input::Configuration` | 0x10 | `MemAlloc(0x10)` 0x1800dbd51 | fields to +0x0a, no size assert (`Input.h:275`) | add the assert |
| `Input::Device` | 0x1200 | `MemAlloc(0x1200)` 0x1800dbe0b | `Input.h:252` `kDeviceMappedSize = 0x1200` (a constant, no struct) | agree |
| `Input::KeyMap` | 0x18 | `MemAlloc(0x18,4)` in `Configuration::Startup` 0x1800e0d76 | fields to +0x14, no size assert (`Input.h:49`) | add the assert |
| `Pathfinding::IPathInfo` | 8 | ctor stores only the vptr | none | undocumented (dead, §6.2) |
| `Pathfinding::PointList` | 0x50 | `MOV EDX,0x50` | `pointlist.cpp` banner 0x50, no header | agree (prose only) |
| `Pathfinding::Scene` | ≥0x90 | ctor last store float +0x8c | none | undocumented (dead, §6.2) |
| `Picking::PickingMesh` | 0x48 | `MOV EDX,0x48` | `Scene.h:158`, offsets to +0x40, no size assert | add the assert |
| `Picking::Scene` | 0x60 | exe `MemAlloc(0x60,8)` 0x140097093 | `Scene.h:319` 0x60 | agree |
| `Save::SaveLoad` | 0x90 | exe `MemAlloc(0x90,4)` 0x140097236 | `SaveLoad.h:349` 0x90 | agree |
| `Save::SaveLoad::SaveInfo` | 0x58 | `GetSaveInfo` `IMUL RAX,RAX,0x58` 0x1800c3b63 | `SaveLoad.h:238` 0x58 | agree |
| `Save::SaveLoad::ModInfo` | — | not read (§13) | `SaveLoad.h:116` 0x20 | NOT VERIFIED |
| `Save::SavedState` | 0x10 | exe `MemAlloc(0x10,4)` | `SaveLoad.h:263` 0x10 | agree |
| `System::Thread` | 0x30 | B34B §2 | `Thread.h:291` 0x30 | agree |

**No size disagrees.** Five classes have no header at all (`Audio::Device`, `SoundBuffer`, `SoundEffect`,
`Stream`, `Pathfinding::PointList` — their layouts live in their `.cpp` files) and three headers lack a
`sizeof` assert on a pinned size (`KeyMap` 0x18, `Input::Configuration` 0x10, `PickingMesh` 0x48). RULE
B6-8 applies to all eight: MOVE the ported bodies' offsets under the retail name, assert the retail
`sizeof`, name only bytes a body reads.

### 2.4 The NON-polymorphic classes — real ctor or not

B6-10 decides: a value type gets real ctors only if something holds it BY VALUE; a class allocated and
built inline by a retail function that the tree ports as a door keeps the door.

| class | held by value? | copy ctor (retail) | trivially copyable | real ctor/dtor? |
|---|---|---|---|---|
| `Achievement::GoalList` | YES — `AchievementInstance+0x10` | deep: `??0GoalList(const&)` 0x180001730 copies the Array via 0x180003a30 | NO (dtor deletes elements) | **YES** (value type; public ctor/dtor/copy), `AchievementInstance` holds it |
| `Save::SaveLoad::SaveInfo` | YES — `Array<SaveInfo>` at `SaveLoad+0x50` | not exported; element holds `System::String`s and a `UI::Sprite` (`SaveLoad.h`) | NO | **YES** — needed by the `Collection::Array<T>` template (ENDGAME B.1.3) |
| `Save::SaveLoad::ModInfo` | YES — `Array<ModInfo>&` is a `LoadMetaData` parameter | not exported | not read | **YES** if the Save block ports `Array<ModInfo>` as the template |
| `Audio::Scene` | YES (exe) — `MenuDialogLoopT+0xb30` | deep (0x180050a50: two Array copies) | NO | ctor/dtor are live exe imports (`??0`/`??1Scene@Audio@@` ×2/×4): real class members, the doors stay for the exe callers |
| `Picking::Scene` | YES (exe) — `+0x8` of the object 0x14009ee70 builds | deep (0x1800722f0 → 0x180072340) | NO | same as `Audio::Scene` |
| `Audio::Stream` (polymorphic) | YES — `Audio::Device+0xa00` | none exported | NO | NO real ctor (B4b-2: a polymorphic Thread subclass; `Ctor_AudioDevice` builds it through the door) |
| `UI::UserInterface` | YES — `DialogLoopT+0x40`, `DialogLoaderLoopT+0x60` | step 6 (B36_RULES §10: NOT trivially copyable) | NO | settled in step 6 |
| `System::MemoryAllocator` | YES — `Engine+0x000` (`??0Engine` CALLs `??0MemoryAllocator` on `this`) | — | — | NOT this step (carry-over 4) |
| `Engine::Loader` (polymorphic) | YES — the static `LoaderLoop::_loader` | none exported | — | NO (B4b-2; `rt_data_mint.cpp` `ConstructLoader` builds it) |
| `Engine::Engine` | YES (exe) — `GameMain`'s stack `[RSP+0x60]` | only `??4` exported | — | the exe ctor import `??0Engine@0@` stays a door (§3) |
| `Engine::MainLoop`, `Save::SaveLoad`, `Save::SavedState` | NO — heap (`MemAlloc` + imported ctor) | deep (MainLoop 0x180031580 Array copy; SaveLoad 0x180030d50 String refcount + Arrays) | NO | ctor/dtor doors stay (exe imports them); members convert |
| `Application`, `AchievementManager`, `Audio::Configuration`, `Audio::Device`, `Input::Configuration`, `Input::Device`, `Input::KeyMap` | NO — heap, ctor INLINED in `Engine::Startup` / `Configuration::Startup` | shallow or none | — | ctors are the `singleton_ctors.cpp` stampers today (`Ctor_Application`, `Ctor_AudioDevice`, `Ctor_InputDevice`, `Ctor_InputConfiguration`, `Ctor_AudioConfiguration`); keep them as doors this step |

**The by-value EXE holders (b37-0X: MenuDLT+0xb30 `Audio::Scene`, exe Game+0x8 `Picking::Scene`,
`GameMain`'s stack `Engine`) and B5-4.** All three held classes are non-polymorphic, so they are concrete
by construction; no abstract-class problem arises. The holders are unconverted exe PODs that construct in
place through the ctor DOORS (`engine_funcs.h:2610`, `gameupdateloop_load.cpp:639-641`,
`game_main.cpp:446`). By the B4b-3 ruling, a POD holding a POD does not trigger `= default` + `friend`.
So in step 7: the ctor/dtor doors stay callable, and the classes get public members. A C++ holder, and
with it the protected ctor and `friend` shape, arrives with the exe subclasses in step 8.

### 2.5 Hidden return slots — 31 exports, ONE through a vtable slot

`B37_SLOTS.md` §4 lists 25; the script's `@@` anchor skipped the six `Achievement::Achievement` retslots,
whose names end `@Achievement@1@` (B7-11): `GetStringTable`, `GetSpriteSheet` (`ExternalPtr`),
`GetSpriteName`, `GetTitle`, `GetDescription` (`System::String`), `CreateInstance`
(`InternalPtr<AchievementInstance>`) — all in §6 of the slots file. The inventory's "26" missed them too.
The five `?AW4` exports return an enum in EAX (`GoalInstance::CheckGoalState` slot 6,
`SaveLoad::GetOperation`, the three `Compile` overrides) and are NOT retslots.

**RULE B7-4 (the one retslot virtual).** `?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ`
(0x18009eeb0) is slot 9 of `??_7Goal@Achievement@@6B@` (0x1801546f8). Its body is
`MOV EAX,[GoalInstance::_typeInfo]; MOV [RDX],EAX; MOV RAX,RDX; RET` — MSVC `this` in RCX, return slot in
RDX. Per B5-12's amendment it is declared
`virtual Object::Type* GetInstanceType(Object::Type* ret) const;` on `Goal` AND on every class that
overrides it. **Two EXE subclasses override slot 9 with their own bodies** — `FinishTutorialGoal`
(0x1400261f0) and `TimeGoal` (0x1400267f0) (`B37_SLOTS.md` §E3); `BuildingGoal`, `PopulationGoal`,
`StatGoal` inherit Goal's through the import stub 0x14003ab5e. Their tree bodies must keep the
retslot-first shape; `Goal.h:198-203` (`kGetInstanceType = 0x48`, "GoalList::Initialize does
`CALL [..+0x48]`") names the live caller. A unit test pins the shape (the `graphics_geometry_abi_test.cpp`
PART C model).

The 30 non-virtual retslot exports (doors keep `(this, ret, …)` and placement-new the prvalue, B5-12's
general sentence): `System::Time` ×6 (`MainLoop::GetDeltaTime/GetFrameTime/GetFrameStartTime/
GetFrameUpdateTime`, `SoundEffect::GetMaxLength`, `Input::Device::PressedTime`), `System::String` ×7
(`Goal::GetText`, `PointList::GetMeshName`, `SaveLoad::GenerateSaveName/GetAutoSaveName`,
`Achievement::GetSpriteName/GetTitle/GetDescription`), `ExternalPtr<T>` ×8 (`Achievement::GetStringTable/
GetSpriteSheet`, `AchievementInstance::GetAchievement`, `AchievementManager::GetAchievement`,
`Goal::GetStringTable`, `GoalInstance::GetGoal`, `Picking::Scene::GetMesh`, `SoundEffect::GetSoundBuffer`),
`InternalPtr<T>` ×3 (`GoalList::GetGoal`, `AchievementManager::GetAchievementInstance`,
`Achievement::CreateInstance`), `Volume::Box` ×3 (`PickingMesh::AABB`, `Picking::Scene::GetAABB/
GetLocalAABB`), `Math::Transform` (`Picking::Scene::GetTransform`), `IO::Persistence::SaveBlock` ×2
(`SavedState::SetupLoad/GetSaveBlock`). Prologues read: `GetDeltaTime` 0x1800314f0 (`MOV [RDX],RAX`),
`GoalList::GetGoal` 0x180001710 (`MOV [RDX],RCX`, index in R8).

### 2.6 By-value ARGUMENTS — the prologue per member (B6-11, extended)

`B37_SLOTS.md` §8, sixteen members read:

**RULE B7-5 (argument passing, per TYPE, measured here).**
- `Object::ExternalPtr<T>` by value → **bare register** (ten members: `SetAchievement`, `SetAchievements`,
  `SetGoal`, `Device::Play`, `PlayPlatform`, `FindFree3DEffect`, `FindEffect3D`, `PlayEffect3D`,
  `PlayEffect`, `Picking::Scene::AddMesh`). `SaveLoad::Draw`'s `ExternalPtr<Graphics::Material>` is NOT
  VERIFIED (§13).
- `Object::InternalPtr<T>` by value → **hidden pointer** (`GoalList::AreGoalsMetExcept`: `MOV RDX,[RDX]`).
- `Collection::Array<T>` by value → **hidden pointer** (`GoalList::Initialize`: `MOV EDX,[RDX+0x8]`).
- `UI::Sprite` by value → **hidden pointer** (`SaveLoad::SetSaveSprite`: `MOV RAX,[RDX]`).
- **`System::String` by value → HIDDEN POINTER even though it is 8 bytes** (`SaveLoad::SaveMetaData`'s
  stack argument is loaded as a pointer into R15 and the callee frees the string through it at
  0x1800c1ab7..0x1800c1b0b). String has a non-trivial copy/dtor in MSVC's eyes, like `InternalPtr`.
- **`System::Time` by value → REGISTER** (`Audio::Scene::Update(Time, Time, …)`: the second Time's ticks,
  R8, are stored raw at `this+0`). 8 bytes, trivially copyable in MSVC's eyes, like `ExternalPtr`.
Declare every hidden-pointer parameter as `T* p` in retail order and pin one raw retail-order call per
type in the block's unit test. A by-value `Time` stays by value.

---

## 3. Q3 — THE EXE-SIDE SUBCLASSES (the constraint on the bases)

Report: `b37_session/EXE_SUBCLASSES.md` (agent b37-0X, read-only enumeration, file:line per row). Its
summary is §3.0 below; the report is authoritative for the per-class rows and the per-site lists.
Block 0's own retail reads (exe mode of the script, `B37_SLOTS.md` §E1–§E3) are §3.1–§3.2; the two
sources were produced independently and agree on every loop slot.

### 3.0 What the exe builds on top of these bases (b37-0X, `EXE_SUBCLASSES.md` §2)

| Runtime base | exe subclasses | hand-built tables | how the base is constructed | base dtor run by slot 0? |
|---|---|---|---|---|
| `Engine::UpdateLoop` | 1 (`GameUpdateLoop`) | 1 (12 slots, `standalone/gameupdateloop_stubs.cpp:149`) | `fn_UpdateLoop_Ctor` door, vptr overwritten (4 sites) | yes (file-local door, `gameupdateloop.cpp:2822`) |
| `Engine::DialogLoopT` | 1 (`MenuDialogLoopT`) | 1 (13) | `fn_DialogLoopT_Ctor` inside the inline `MenuDialogLoopT_Ctor` (`engine_funcs.h:2601-2615`, which also builds `Audio::Scene` by value at +0xb30 and `System::Random` at +0x168) | **no** |
| `Engine::DialogLoaderLoopT` | 1 (splash) | 1 (13) | `fn_DialogLoaderLoopT_Ctor` (7 sites) | **no** |
| `Engine::LoaderLoop` | 2 (inner, outer) | 2 (12 each) | `fn_LoaderLoop_Ctor` (7 sites) | **no** |
| `Achievement::Goal` | 5 | 5 | `fn_Goal_BaseCtor` or a file-local `??0Goal` | yes |
| `Achievement::GoalInstance` | 2 | 2 (7) | file-local `??0GoalInstance` | yes |
| `Achievement::Achievement` | 1 (`Scenario`) | 1 | Runtime ctor BY ADDRESS 0x18009e240 (`scenario.cpp:74-75`) | yes |
| `Achievement::AchievementInstance` | 1 in retail | **0** — 0x14015e350 is resolved (`gameupdateloop_load.cpp:1633`) but never registered; the object keeps the Runtime table | file-local `??0AchievementInstance` | Runtime's |
| `Audio::*`, `Picking::*`, `Input::*`, `Save::*`, `Pathfinding::*` | 0 | 0 | by-value holders only (MenuDLT+0xb30 `Audio::Scene`, exe Game+0x8 `Picking::Scene`, `EngineStorage` on `GameMain`'s stack) | — |

Twelve exe classes, fourteen tables. **25 QUALIFIED-call sites** (`EXE_SUBCLASSES.md` §3A): loops 7
(`splash_loop_stubs.cpp:173, :199, :549` — `:549` recurses forever if left unqualified;
`menudialogloopt_stubs.cpp:318, :551, :669`; `gameupdateloop.cpp:2822` `~UpdateLoop`), the Goal family 15
(7 base-dtor calls, 5 `Goal::Serialize`, 2 `GoalInstance::Persist`, 1 `GoalInstance::Update`), Scenario 3
(`scenario.cpp:112` dtor, `:134` `GetFileVersion`, `:153` `Serialize`). Only 5 of the 25 are reachable by a
name-anchored sweep: the Goal-family and Scenario doors are exe dispatch slots or by-address Runtime reads.
**Owed retail base calls the standalone stubs drop** (§3B there): MenuDLT's `Shutdown` (carry-over 11) and
the base dtors skipped by MenuDLT, splash and both LoaderLoop slot 0s. **No loop table holds a Runtime
pointer today**: every inherited slot is a standalone stub that re-implements the base body (35 stub
slots); making them inherited after conversion is a behaviour decision for block 1, not a qualification.
Goal tables: slot 1 (`TypeName`) is `nullptr` in all five standalone tables; `FinishTutorialGoal` never
calls `Goal_FillInheritedVtableSlots`, so its slots 1, 4 and 7 stay null (a `Compile` dispatch would jump
to 0). `Goal_FillInheritedVtableSlots` (`goal_exe_helpers.cpp:210-235`) installs `External::Compile` and
`Goal::GetInstanceType` BY NAME; it survives while both names resolve. `TutorialGoals.h`'s family derives
from `Object::External`, not `Goal` — out of this step.

**Two disagreements between the standalone tables and retail (block 0's reads, `B37_SLOTS.md` §E3):**
(1) every retail Goal subclass table has **11** slots — `0x140160420 + 11×8 = 0x140160478` is exactly
`FinishTutorialInstance`'s table, and likewise `0x140160540 + 0x58 = 0x140160598` — so the "12th slot" of
the standalone `TimeGoal`/`FinishTutorialGoal` arrays is the adjacent GoalInstance table's slot 0, laid out
as one array; (2) retail `Scenario`'s table (0x14015f020) has **9** slots followed by a zero qword, so the
standalone array's null slots 9–12 have no retail counterpart.

### 3.1 The five loop tables named in the brief

| exe table | base | slots | exe bodies | inherited from the Runtime base (import stub) |
|---|---|---|---|---|
| `GameUpdateLoop` 0x14015e2f0 | `UpdateLoop` | **12, not 22** (the brief's count ran into the next table, 0x14015e350) | 0–10 (5 `DrawFailed` is a bare `RET 0` at 0x140096000) | 11 `AllowsModChanges` |
| `MenuDialogLoopT` 0x14015ed00 | `DialogLoopT` | 13 | 0,1,2,3,4,6,8,9,10,11,12 | 5 `DrawFailed`, 7 `SaveRestore` (both `UpdateLoop`'s) |
| splash 0x140158800 | `DialogLoaderLoopT` | 13 | 0,1,3,12; 2 `Shutdown` is a bare `RET 0` | 4 `Draw`, 6 `Load` (DialogLoaderLoopT's), 5,7,9,10,11 (UpdateLoop's), 8 `OnVideoError` (LoaderLoop's) |
| LoaderLoop "inner" 0x140158870 | `LoaderLoop` | 12 | 0, 6 `Load`, 10 `GetResourceFilename` | 1,2,3,4,8 (LoaderLoop's), 5,7,9,11 (UpdateLoop's) |
| LoaderLoop "outer" 0x1401588d0 | `LoaderLoop` | 12 | 0 only | 1–11 |

So the Runtime base bodies that exe tables REACH directly are: `UpdateLoop::{DrawFailed, SaveRestore,
OnVideoRestart, GetResourceFilename, AllowsModChanges}`, `LoaderLoop::{Startup, Shutdown, Update, Draw,
Load, OnVideoError}`, `DialogLoaderLoopT::{Draw, Load}`. Those members must keep their exported
identity (the minted tables' slots and the exe's import slots both point at them; B36_RULES §3.3).

### 3.2 Exe subclasses the brief did not list (read from the stamp after each base-ctor import call)

| exe table | base | slots | where built |
|---|---|---|---|
| 0x14015f020 ("Scenario") | **`Achievement::Achievement`** | 9: overrides 0,1,2,3,5,6,7,8; 4 `Compile` via stub | `Scenario_Factory`; tree `code/Game/Game/scenario.cpp` calls `??0Achievement` (0x18009e240), `??1Achievement` (0x18009e180), **`GetFileVersion` (slot 3, 0x18009e170) and `Serialize` (slot 5, 0x18009e070) BY ADDRESS** (`scenario.cpp:75-81`) — both VIRTUAL on the base: QUALIFIED calls after conversion (B6-3) |
| 0x14015e350 | `Achievement::AchievementInstance` | 4: 0 exe, 1–3 inherited | GameUpdateLoop's slot-6 `Load` body (0x140096b00) — constructed on the LOADER thread |
| 0x140160300 `BuildingGoal`, 0x140160370 `PopulationGoal`, 0x1401604c0 `StatGoal`, 0x140160540 `TimeGoal`, 0x140160420 `FinishTutorialGoal` | `Achievement::Goal` | **11** — slot 10 is an EXE-introduced virtual (Goal has 10) | factories in `code/Game/Goal/`; 4 `Compile` via stub; 9 via stub on Building/Population/Stat, exe body on Time/FinishTutorial (B7-4) |
| 0x140160478 `FinishTutorialInstance`, 0x140160598 `TimeGoalInstance` | `Achievement::GoalInstance` | 7, all exe bodies | factories |

Exe ctor/dtor import call counts (`B37_SLOTS.md` §E2): `??0Goal` 17, `??1Goal` 25, `??0GoalInstance` 4,
`??1GoalInstance` 10, `??0Achievement@0@` 4, `??1Achievement@0@` 6, `??0/??1DialogLoaderLoopT` 8/10,
`??0/??1LoaderLoop` 8/4, `??0/??1DialogLoopT` 1/3, `??0/??1UpdateLoop` 1/5, `??0/??1MainLoop` 1/1,
`??0/??1Engine` 1/2, `??0/??1SaveLoad` 2/7, `??0/??1Scene@Audio` 2/4, `??0/??1Scene@Picking` 2/4.

**RULE B7-13 (the bases the exe builds on).** `UpdateLoop`, `LoaderLoop`, `DialogLoopT`,
`DialogLoaderLoopT`, `Achievement::Achievement`, `AchievementInstance`, `Goal`, `GoalInstance` all have
exe subclasses with hand-built tables: each keeps its ctor/dtor door, slot 0 `DeletingDtor(unsigned) = 0`
with its `deleting_dtor.cpp` row, and every call from an exe slot body to a VIRTUAL of the base is
QUALIFIED (B6-3). `LoaderLoop` is concrete in retail but is still never constructed as a C++ object this
step (its only constructions are the two exe subclasses and the splash subclass's base, all through the
ctor door).

---

## 4. Q2 — THE `engine/types` MIRRORS vs `runtime/types/`

`Glob engine/types/*` for the eight namespaces finds EIGHT headers, not the inventory's two
(`DialogLoopT.h` was deleted in step 6):

| header | what it is | includers (by basename) | verdict |
|---|---|---|---|
| `engine/types/UpdateLoop.h` (66 l.) | global `::UpdateLoop` POD 0x28 + `UpdateLoopVtable` (12 slots) | `game_main.cpp`, the three `standalone/*_stubs.cpp`, and three engine headers | **alias-of-RtNs-POD**: `using UpdateLoop = RtEngine::UpdateLoop;` — `Engine::UpdateLoop` is ABSTRACT, so it cannot be the alias target where a POD is expected (B6-4 correction). Field names differ (`m_resourceList` vs `_resourceList` …): the `m_` names are used at **107 sites in 15 files** (`code/Game/{Dialog,Game,Goal,Tutorial}`, `standalone/{autostart,loader_loop_stubs,menudialogloopt_stubs,splash_loop_stubs}.cpp`, `engine/funcs/engine_funcs.h`) — the rename is the aliasing commit's work. `UpdateLoopVtable` stays until the five hand-built tables go (step 8) |
| `engine/types/LoaderLoop.h` (49 l.) | `::LoaderLoop` POD 0x48, base fields spelled inline | `DialogLoaderLoopT.h`, `standalone/loader_loop_stubs.cpp` | alias-of-RtNs-POD (`RtEngine::LoaderLoop`), same rename set |
| `engine/types/DialogLoaderLoopT.h` (57 l.) | `::DialogLoaderLoopT` POD 0x188, `UserInterface` as `uint8_t[0x128]` | `code/Game/Game/dialogloaderloop.h`, `standalone/splash_loop_stubs.cpp` | alias-of-RtNs-POD (`RtEngine::DialogLoaderLoopT`, which holds `UI::UserInterface` by value) |
| `engine/types/MainLoop.h` (148 l.) | `::MainLoop` POD 0xA0 + `MainLoopAddr` constants | `code/Game/Game/dialogloaderloop.cpp`, `standalone/splash_loop_stubs.cpp` | **alias-of-class** (`using MainLoop = Engine::MainLoop;`): MainLoop is NOT abstract, NOT polymorphic, and nothing embeds it. Its `m_` field names are used in about 12 files (the `m_nextLoop` spelling is shared with LoaderLoop; count by receiver). The `MainLoopAddr` constants (`engine/types/MainLoop.h:42-74`) are most of the Engine "named constant" hex hits of `B37_SLOTS.md` §T and die with it |
| `engine/types/GameUpdateLoop.h` (166 l.) | exe `GameUpdateLoop` 0x160 | `gameupdateloop.cpp`, `gameupdateloop_load.cpp` | STAYS (exe subclass, step 8); cross-assert its base prefix against `RtEngine::UpdateLoop` |
| `engine/types/AchievementGoals.h` (165 l.) | seven exe `Goal`/`GoalInstance` subclasses; Goal base as `uint8_t _goalBase[0x28]` | six `code/Game/{Goal,Tutorial}/*_funcs.h` | STAYS (exe subclasses, step 8) — the blob keeps its layout; no Runtime POD is embedded |
| `engine/types/TutorialGoals.h` (109 l.) | exe tutorial-goal classes | four `code/Game/Tutorial/*_funcs.h` | STAYS (exe, step 8); not a Runtime mirror |
| `engine/types/PickingComponent.h` (39 l.) | exe `PickingComponent` (a Component, vtable 0x140164040) | `code/Game/Component/pickingcomponent_funcs.h` | STAYS — NOT a `Picking::*` mirror at all (the inventory's "mirror POD" reading is wrong) |

**Inside `runtime/types/`: two definitions of `Achievement::GoalList`** — `AchievementManager.h:52`
`GoalListView` (its own banner says it waits for "a real GoalList") and `Goal.h:175` `GoalList`. RULE B6-4
applies: the Achievement block makes `GoalList` the real value class in `Goal.h` and turns `GoalListView`
into `using GoalListView = Achievement::GoalList;` (or deletes it with its includers renamed).

**RULE B7-6 (one definition per layout, this batch).** The four loop PODs keep living as `RtEngine::`
PODs (the B4b-3 "POD mirror holding the POD" ruling: `RtEngine::LoaderLoop` holds `RtEngine::UpdateLoop
_base;`, `RtEngine::Loader` holds `RtSystem::Thread _thread;`) and are cross-asserted field for field
against the four classes; the engine-side loop headers become aliases of the `RtEngine::` PODs, NOT of
the abstract classes. `MainLoop` (concrete, non-polymorphic) aliases the class. Every other engine-side
header above is exe-phase and stays.

---

## 5. Q5 — THREADING: FOUR execution contexts, not two

| context | entry | what it touches (read from the bytes) |
|---|---|---|
| **main** | `MainLoop::Run`/`Frame` | everything |
| **loader thread** (`Engine::Loader`, a `System::Thread`, stack 0x40000) | `Loader::Run` 0x1800dbfc0 | `Engine::_singleton` → `Engine+0x6c0` (the `Video::Device`) → its `+0x100` object's slot 4 with `DL=1` before and `DL=0` after (the device's loader-thread acquire — the `DEVICE_LOST_LOADER_RACE.md` window); `ExternalFactory::_singleton` `+0x41` = 1 / 0 (the "loading" flag) and `+0x0` = the loop's `_loadBit`; `UpdateLoop::LoadResources(loop)`; **the loop's slot 6 `Load` — for the exe loops that is `GameUpdateLoop::Load` (0x140096b00), which constructs `SaveLoad` (`MemAlloc(0x90)`), `Audio::Scene`, `Picking::Scene`, an `AchievementInstance` subclass and `UI::UserInterface`** (`B37_SLOTS.md` §E2 — the `Thread` ctor argument is 0x40000 for Loader, 0x10000 for Stream), and for the Runtime Dialog loops `DialogLoopT::Load`/`DialogLoaderLoopT::Load` (resource `FindOrLoad` of the `.rsc` dialog). `Loader::Load` (main thread, 0x1800dc050) stores the loop at `+0x30`, applies mod changes through `IO::Modification` under `ExternalFactory+0x41`, then `SetEvent`s the start handle |
| **audio stream worker** (`Audio::Stream`, a `System::Thread`, stack 0x10000) | `Stream::Run` 0x1800a0f60 | `Audio::Device::_singleton`'s `AudioChannel[this+0x7c]` (stride 0x28; reads `+0x8`, the playing `SoundBuffer*`) and `PlatformChannel[this+0x7c]` (`Device+0xa98`, stride 0x20; writes `+0x18` = its own `+0x48` event); the `SoundBuffer`'s `+0x20` loop flag, `+0x30` stream path, `+0x48/+0x50` WMA seek table; an `IO::FileStream` it constructs itself; the XAudio2 source voice (`GetState` `[vt+0xc8]`, `SubmitSourceBuffer` `[vt+0xa8]`) ALWAYS under its own `CRITICAL_SECTION` at `this+0x50`; `WaitForMultipleObjects` on `+0x48`/`+0x40`; the stop byte `+0x80`; `MemFree` through the `MemoryAllocator` lock (`+0x640`) |
| **XAudio2 callback thread** (not ours) | the unexported 0x180157c60 table | slot 2 `OnStreamEnd`: `OR dword [channel->flags],2` — **no lock**; slot 4 `OnBufferEnd`: `SetEvent(record+0x18)` (the Stream's `+0x48`) |

**Classes touched from two or more contexts:** `Audio::Device` (main: `Play`/`Stop`/`Update`/reap;
worker: channel arrays; XAudio2: `AudioChannel.flags`), `Audio::Stream` (main: `Play`/`Stop`/`Lock`;
worker: everything), `Audio::SoundBuffer` (main: owner; worker: reads), `Video::Device` (loader-thread
acquire), `Object::ExternalFactory` (both), `UI::UserInterface` and every `External` the loop's `Load`
constructs (loader thread constructs, main thread draws the previous loop), `Save::SaveLoad`,
`Audio::Scene`, `Picking::Scene`, the `AchievementInstance` subclass (constructed on the loader thread,
used on main).

**RULE B7-7 (threading).**
1. B6-5 applies to every member that runs on the loader or worker thread. Today's offenders, by line:
   `runtime/src/engine/loops.cpp:188` (`FindOrLoad()`) and `:193` (`IsDerivedFrom()`) — guarded
   `static Fn fn = ResolveExport(...)` caches reached from `RtEngine_DialogLoopT_Load` (`:742`) and
   `RtEngine_DialogLoaderLoopT_Load` (`:832`), both slot-6 bodies run by `Loader::Run`; `:1334`
   (`loadResources`) inside `RtEngine_Loader_Run` itself; and `:1023-1027` (`LoaderLoopLoader()`,
   `B37_FILEMAP.md` §3.1). When these become members they resolve at
   namespace scope with the `RtResolveCached` shape (B36_RULES 14.4), never a function-local static.
2. `RT_EXPORT_ONCE` (`runtime/funcs/rt_port.h:127`: a constant-initialised `static void*` filled only by
   `RtResolveCached`) is ALREADY that shape; `Stream::Run`'s six uses in
   `vtbatch7b_stream_run_win.cpp` (`:335..:538`) are acceptable as they stand.
3. **No lock is added anywhere.** The XAudio2 thread's unlocked `OR [flags],2` and the worker's reads of
   `AudioChannel` are retail's design; `Stream`'s `CRITICAL_SECTION` (+0x50, raw
   `InitializeCriticalSection` + spin 5000) and the `MemoryAllocator` lock are the only synchronisation.
   B5-6 applies to `Stream`'s lock: an opaque `uint8_t[0x28]`, no `std::mutex`, no `<windows.h>` in the
   header (B4b's `System::CriticalSection` ruling: `Stream` never reaches the door).
4. The device-lost / loader race (`DEVICE_LOST_LOADER_RACE.md`) is the `Loader::Run` slot-4 pair above;
   it stays the user's decision (carry-over 9). Converting `Loader` must not move or add a call there.

---

## 6. Q4 — `Application`, AND Q8 — `Pathfinding`

### 6.1 `Application::Application`

- **Not polymorphic:** `??0Application@0@QEAA@PEAX@Z` (0x18009f590) stores `0` at +0, the `HINSTANCE`
  argument at +0x8, `word 0x100` at +0x10, an empty `Array` at +0x18, a byte at +0x28, and publishes
  `_singleton` (0x1801fc678). No `??_7`. 0x30 bytes (`Engine::Startup` `MemAlloc(0x30)`).
- **27 named exports**, all under the back-reference mangling `@Application@1@` (`??0`/`??1`/`??4` as
  `@Application@0@`): `Get`, `Startup`, `Shutdown`, `Update`, `Quit`, `IsPaused`, `LaunchProcess`,
  `EnableCursor`, `IsCursorEnabled`, `SleepThread`, `YieldToThreads`, `Register/UnregisterMessageCallback`,
  `GetInstance`, `GetWindow`, `WindowProc` (private static), six `Platform*`, the three special members
  and `_singleton`. The inventory's "zero named exports" came from a parser that required
  `@Application@@`. **`runtime/types/application/Application.h:13-15` ("it has no mangled export of its
  own") is WRONG** for the same reason.
- **The four dx11 rows are VideoDX11-x64.dll's OWN IAT slots**, not Runtime addresses: 0x180013290
  `?RegisterMessageCallback@Application@1@…`, 0x180013298 `?UnregisterMessageCallback@Application@1@QEAAXI@Z`,
  0x1800132a8 `?GetInstance@Application@1@QEBAPEAUHINSTANCE__@@XZ`, 0x180013310 `?_singleton@Application@1@`
  (pefile over `VideoDX11-x64.dll`). They reconcile with the export list exactly.
- **What `Engine::Startup` needs from it (0x1800db8a0, read as disassembly):** Application is built AFTER
  the MemoryAllocator/FileSystem/Persistence/Modification/Configuration and the three Object factories
  (ExternalFactory `MemAlloc(0x48)` → `Engine+0x6b0`), and BEFORE `Video::Configuration` (0x44 →
  `+0x6b8`) and `Video::Device` (0x108 → `+0x6c0`): `MemAlloc(0x30)` → ctor inlined with
  `Engine+0x698` (the `HINSTANCE`) → `Engine+0x690` → `Application::PlatformStartup` (0x18009fc10, the
  window) → an import at `[0x18014b618]` → Video. `Engine::PlatformConfigure` (the launcher dialog)
  and the Video device's platform startup (0x1800f8260) and `Input::Device::PlatformStartup` (0x1800e1880)
  come later and all read `Application::_singleton`: Input registers three message callbacks
  (0x1800e1a88/aa0/ab8), the Video device two (0x1800f83b8/3d0). The exe imports only `Quit`, `IsPaused`,
  `YieldToThreads` and `_singleton`.
- **Recommendation: YES, block 1 converts `Application` to a real non-polymorphic class** (members for
  all 27 exports; `_singleton` stays `CUTOVER_REQUIRED` storage read through its `RT_DATA_SLOT` until
  the end of B.3, carry-over 5), and dx11's `devicedata.cpp` drops the three function rows for member
  calls in the same block (the `appSingleton` row goes with the class static at the end of B.3). The
  ctor stays `Ctor_Application` in `singleton_ctors.cpp` (Engine-owned file): Startup's ORDER must not
  move — Application before Video, PlatformStartup before any `RegisterMessageCallback`.

### 6.2 `Pathfinding` — PointList is LIVE; Scene and IPathInfo are dead in retail

- **Retail side.** The exe imports exactly ONE Pathfinding symbol: the DATA export
  `?_typeInfo@PointList@Pathfinding@@` (IAT 0x140151448). It is read twice by exe 0x14000ab20, the
  `ExternalPtr<Pathfinding::PointList>` serializer: `ExternalFactory::FindOrLoad(path)` then
  `Object::Base::IsDerivedFrom(PointList::TypeId)` (imports 0x140151230 / 0x140152a98). That serializer is
  called from `InteractDescription::Serialize` (0x140123500) and `ParticleDescription::Serialize`
  (0x140053460), both on field `+0x20`. So every InteractDescription / ParticleDescription resource the
  game loads loads a `PointList` through the factory (its `Construct`/`Serialize`/`Destruct` slots).
  `Pathfinding::Scene` (ctor 0x18010f660, `GeneratePath` 0x18010dcf0) and `IPathInfo` (ctor 0x18006c970)
  have NO code reference in the DLL (only export / `.pdata` / unwind data) and are not imported by the
  exe: dead in retail. (The exe's pathfinder is its own `System::Thread` subclass, `pathfinder_seed.cpp`.)
- **Tree side.** `@Pathfinding@@` names appear only in `runtime/src/pathfinding/`, `standalone/
  rt_export_addrs.inc`/`rt_data_mint_tables.inc` and `resolve_standalone.cpp:291-295` (registration).
  The hex-literal sweep finds ONE Pathfinding literal outside the tables (the `RT_DELDTOR` row). But
  `code/Game/Component/interactdescription.cpp` walks the resolved PointList's `+0x30`/`+0x38` array
  (`Points()` is `LEA RAX,[RCX+0x30]`, 0x180071190) by RAW OFFSET — a live layout reader with no binding
  any sweep sees. Its banner calls `+0x20` "`_descRef` ExternalPtr (Description)" and
  `particledescription.cpp:9` calls it "`materialRef`": **both are `ExternalPtr<Pathfinding::PointList>`**
  (the type-id check above).
- **RULE B7-9.** `Pathfinding::PointList` gets the full block treatment (real `Object::External` class,
  9 slots, 0x50, `Compile` overridden, literal `"Object::External"`, `DeletingDtor = 0` + its
  `deleting_dtor.cpp:927` row) — it is small and its bodies already exist. `Pathfinding::Scene` and
  `IPathInfo` are a header + declarations only (`// not ported`, defined nowhere), no bodies ported.
  The two exe description files name the field correctly in the caller round.

---

## 6.3 Callers — BY BINDING (b37-0X's `b37_session/CALLER_SURFACE.md`), cross-checked by block 0's hex sweep

**The caller population (`CALLER_SURFACE.md` §1, units of `b36_session/CLOSE_NUMBERS.md` §3):**

| namespace | `engine_funcs` door CALLs | file-local-pointer CALLs (L) | `RT_EXPORT_ONCE` | **CALL sites** | data reads of `engine_funcs` globals |
|---|---:|---:|---:|---:|---:|
| Engine | 37 | 15 | 6 | **58** | 60 |
| Save | 53 | 22 | 15 | **90** | 25 |
| Achievement | 31 | 35 | 5 | **71** | 22 |
| Audio | 41 | 32 | 13 | **86** | 10 |
| Input | 69 | 23 | 12 | **104** | 62 |
| Pathfinding | 0 | 0 | 0 | **0** | 0 |
| Picking | 0 | 22 | 0 | **22** | 0 |
| Application | 2 | 12 | 6 | **20** | 13 |
| **total** | **233** | **161** | **57** | **451** | **192** |

The `engine_funcs` population by INITIALISER is **82 door pointers + 10 data globals** (not the inventory's
63): exe-slot-only bindings (`fn_IsRunning`, `fn_Goal_BaseSerialize`, `fn_KeyMap_Released`) and the
back-referenced names are the difference. Five doors have zero calls (`fn_UpdateLoop_LoadResources`,
`fn_AudioDevice_FreeChannel`, `fn_InputDevice_Update`, `fn_App_PlatformUpdate`, `fn_App_SleepThread`).
The per-directory cut for the caller batches is `CALLER_SURFACE.md` §1.1. Hand-inlined vtable calls onto
these classes: two, both proved `UpdateLoop` slot 7 `SaveRestore` (`moddialog.cpp:733`,
`mod_dialog_vtables.cpp:440`) — ordinary virtual calls, unqualified after conversion.

**Block 0's independent hex-literal sweep (`B37_SLOTS.md` §T)** finds 66 by-address resolve LINES for the
eight namespaces across `ResolveAddr(Runtime|Exe)`, `RtAddr`, `resolveIAT`, `RSlot`, `ResolveSlot`
(including `engine_funcs.cpp`'s own 22), against the inventory's 8 — consistent with b37-0X's (e)+(g) = 47
resolution sites outside `engine_funcs.cpp`. Registration rows and header address constants (`MainLoopAddr`,
`EngineAddr`) are counted separately there and are not callers.

**Binding defects b37-0X found (NOT fixed; `CALLER_SURFACE.md` §6):** (1) `fn_KeyMap_Released` is always
null — its only initialiser is the unregistered exe slot 0x1401528f0, so `dragtool.cpp:255, :401` never see a
key release; (2) `finishtutorialinstance.cpp:72` resolves slot 0x140151c08 as "IsDerivedFrom", but the PE
import table says `?Save@Configuration@Audio@@QEAAXXZ` (unregistered, so the type check is skipped);
(3) `g_pEngineSingleton` and `g_pMainLoopSingleton` are two globals for ONE symbol, `MainLoop::_singleton`,
and `g_pKeyMapObjectExref` is `Input::Configuration::_singleton`; (4) the exe AchievementInstance table
0x14015e350 is never registered. Each is a candidate carry-over for the coordinator.

**RULE B7-11 (name sweeps must match back-references).** Three classes in this batch share their
namespace's name and are MANGLED WITH A BACK-REFERENCE: `Achievement::Achievement` (`@Achievement@1@`,
`??0Achievement@0@`, `??_7Achievement@0@6B@`, `?_typeInfo@Achievement@1@`), `Application::Application`
(`@Application@1@`, `@Application@0@`), `Engine::Engine` (`@Engine@1@`, `@Engine@0@`). A grep for
`@<NS>@@` misses every one of them — that is how the inventory lost a mint row, a type-info row and 27
exports. Sweep with `@<NS>@[01]?@` or by address.

---

## 7. Deliberately NOT converted in step 7

| left alone | why |
|---|---|
| the 16 mint rows of the set (15 `??_7` + the unnamed 0x180157c60) and the 8 `_typeInfo` rows; the 12 `RT_DELDTOR` rows (4 Achievement, 4 Engine loops, `PointList`, `PickingMesh`, `SoundBuffer`, `SoundEffect`) | retire at the END of B.3 (B35_RULES §11); no `DEFINE_TYPE` this step — the 8 literals are RECORDED (`B37_SLOTS.md` §3, B5-15) |
| ctor/dtor DOORS of every polymorphic class (`??0/??1` of the loops, `Loader`, `Stream`, the Achievement four, `SoundBuffer`, `SoundEffect`, `PointList`, `PickingMesh`) | B4b-2 / B6-3: the exe subclasses and `singleton_ctors.cpp` construct through them |
| `singleton_ctors.cpp`'s `Ctor_Application`, `Ctor_AudioDevice`, `Ctor_AudioConfiguration`, `Ctor_InputDevice`, `Ctor_InputConfiguration` (and the non-step-7 ones) | owner-retained; ported ctors of classes whose retail ctor is INLINED in `Engine::Startup` |
| the ten singleton storage rows: nine `CUTOVER_REQUIRED` manifest rows (`AchievementManager`, `Application`, `Audio::Configuration`/`Device`, `LoaderLoop::_loader`, `Engine`, `MainLoop`, `Input::Configuration`/`Device`) **plus `?_singleton@SaveLoad@Save@@` (0x1801fc6f0), which is read through `RT_DATA_SLOT` at two sites and has NO manifest row** | carry-over 5 (`MintStorageCell` redirect at the class-static conversion, END of B.3); the SaveLoad row is new to the list |
| `System::MemoryAllocator` (held by value at `Engine+0`) and its doors | carry-over 4 |
| the exe-phase subclasses (§3: five loops, Scenario, the AchievementInstance subclass, five Goals, two GoalInstances, the tutorial goals) and `engine/types/{GameUpdateLoop,AchievementGoals,TutorialGoals,PickingComponent}.h` | step 8; step 7 gives them real bases |
| the XAudio2 `VoiceCallback` class (0x180157c60) | unnamed in retail; its bodies stay in `audio_voice_callback_win.cpp` as they are |
| `Pathfinding::Scene`, `IPathInfo` bodies | dead in retail (§6.2) |

**File ownership (Q7) is agent b37-0F's `B37_FILEMAP.md`.** Summary: 116 files in six blocks, **disjointness
PASS** (`blocks.py`); five body straddlers, all resolved without a line split — `singleton_ctors.cpp`
(B1+B4+B5: owner-retained by B1, the B4/B5 bodies are ctor doors that stay, and B4/B5 keep the
`RtAudio::`/`RtInput::` POD spellings until they rebase on B1), `engine_lifecycle.cpp` (B1+B3: B1 keeps the
`AchievementManager` ctor door), `platform_startup_win.cpp` (B1+B3: ONE fragment body,
`AchievementManager::PlatformStartup`, moved after both merge), `deleting_dtor_leaves.cpp` and
`reg_mint_promoted.cpp` (coordinator's). The three `closure_batch2b.cpp` are distinct files; the Engine one
holds `Application`'s unexported scalar deleting dtor (0x18004da00). Two more one-definition items the file
map found: `pickingmesh.cpp:87`'s anonymous-namespace `PickingMesh` (full Array header, capacity at +0x44)
duplicates `RtPicking::PickingMesh`, and `loops.cpp:1023-1027` (`LoaderLoopLoader()`, a function-local
`static Loader* p = ResolveExport(...)` read on the loader thread) is a further B6-5 site (B7-7). Coordinator edits at merges (`B37_FILEMAP.md` §7.1):
`runtime/types/ui/UserInterface.h` carries the `friend` declarations of the `RtEngine` loop PODs (B1) and
the tree's only `namespace Audio` forward declaration (B4); `engine_funcs.h`'s `MenuDialogLoopT_Ctor`
(`:2601-2615`, B1 + B4) is the closing sweep's; `input_platform_shutdown_win.cpp` (B5) includes B1's
`Application.h`, so B5 runs after B1. The file map's "15 mint rows" is the 15 EXPORTED `??_7` rows; with the
unnamed 0x180157c60 row the mint holds 16.

---

## 8. The rules, collected

| # | rule | § |
|---|---|---|
| B7-1 | Slot-order traps: the four loops are NOT `Object::Base` (slot 1 `Startup`); `UpdateLoop` pure at 1,2,3,4,6; `LoaderLoop` concrete; Dialog loops add pure slot 12; Loader/Stream/IPathInfo have NO deleting-dtor slot; `Goal` pure at 6,7,8; `Compile` overridden only by `SoundEffect`/`PointList`/`PickingMesh`; `Construct` at 8 on all six External leaves | 2.2 |
| B7-2 | The Object-rooted eight take their literal from `B37_SLOTS.md` §3 (`SoundBuffer` → `"Object::CompiledExternal"`); nothing else in the batch gets a `DEFINE_TYPE` or a type name | 2.1 |
| B7-3 | `Achievement::Achievement` is a polymorphic `Object::External` (9 slots, 0x58) with an exe subclass (Scenario): abstract-base treatment, ctor door, qualified calls to its `Serialize`/`GetFileVersion` from `scenario.cpp` | 1, 3.2 |
| B7-4 | `Goal::GetInstanceType` (slot 9) is `Object::Type* GetInstanceType(Object::Type* ret) const` on Goal AND on the exe overrides (`FinishTutorialGoal`, `TimeGoal`); unit-test it | 2.5 |
| B7-5 | By-value arguments per type: `ExternalPtr` and `System::Time` in a REGISTER; `InternalPtr`, `Collection::Array`, `UI::Sprite` and **`System::String` (8 bytes!)** by HIDDEN POINTER | 2.6 |
| B7-6 | One definition per layout: the loop PODs stay `RtEngine::` (POD-holds-POD), engine-side loop headers alias the PODs (107-site `m_` rename), `::MainLoop` aliases the class, `GoalListView` folds into `GoalList`, `pickingmesh.cpp:87`'s private `PickingMesh` folds into the class | 4, 7 |
| B7-7 | Four contexts (main, loader, audio worker, XAudio2 callback); B6-5 at `loops.cpp:188,193,1334`; `RT_EXPORT_ONCE` is fine; no lock is added; `Stream`'s lock is B5-6-opaque | 5 |
| B7-8 | Value types with real ctors: `GoalList`, `SaveLoad::SaveInfo` (and `ModInfo` if templated). Every copy ctor read here is DEEP (`GoalList`, `KeyMap`, `Audio::Scene`, `Picking::Scene`, `Pathfinding::Scene`, `MainLoop`, `SaveLoad`): none of those is trivially copyable — do not assert it | 2.4 |
| B7-9 | `PointList` is live and gets the full treatment; `Pathfinding::Scene`/`IPathInfo` are declarations only | 6.2 |
| B7-10 | `Application` becomes a real non-polymorphic class in block 1 (27 members); dx11 drops three rows; Startup order (Application before Video, `PlatformStartup` before any callback registration) is untouched | 6.1 |
| B7-11 | Name sweeps match back-references (`@Achievement@1@`, `@Application@1@`, `@Engine@1@`, `??_7Achievement@0@6B@`); callers are swept by hex literal from day one (66 by-address sites) | 6.3 |
| B7-12 | `Audio::Device` (non-polymorphic, 0x1298, no header) holds the polymorphic `Audio::Stream` BY VALUE at +0xa00 and 64 `VoiceCallback` records at +0xa98: the Device POD holds the Stream POD (B4b-3), `Stream` keeps its ctor door, `Device` gets no real ctor this step | 2.4, 5 |
| B7-13 | Every base the exe builds on (four loops, Achievement, AchievementInstance, Goal, GoalInstance) keeps its ctor door and slot-0 row; qualified calls inside exe slot bodies (B6-3); the Runtime base bodies the exe tables reach through import stubs keep their exported identity | 3 |

## 9 (proposed). The blocks — the coordinator finalises this from `B37_FILEMAP.md`

The file sets are `B37_FILEMAP.md` §7 verbatim (disjointness PASS, 116 files); this table adds the rules
each block carries.

| wave | block | classes | files (`B37_FILEMAP.md` §7) | rules / notes |
|---|---|---|---|---|
| 1 | **B1 Engine + Application** (serial owner) | `UpdateLoop`, `LoaderLoop`, `DialogLoopT`, `DialogLoaderLoopT`, `Loader`, `MainLoop`, `Engine`, `Application` | 37: `runtime/types/{engine/*,application/Application}.h`, `runtime/src/engine/*` (14 + `mainloop_internal.h`), `runtime/src/application/*` (8), `engine/types/{UpdateLoop,LoaderLoop,DialogLoaderLoopT,MainLoop,GameUpdateLoop}.h`, `standalone/{loader_loop,splash_loop,menudialogloopt,gameupdateloop}_stubs.cpp`, `runtime/src/video/context_ctor.cpp` | B7-1, B7-6 (loop aliases + the `m_` rename), B7-7.1 (four `loops.cpp` caches), B7-10 (dx11's three rows follow in the caller round or here), B7-13 and the seven loop QUALIFIED sites of §3.0; owns `singleton_ctors.cpp`; `Loader` derives from the real `System::Thread`; carry-over 11 (MenuDLT `Shutdown`) is decided here |
| 1 | **B2 Save** (parallel with B1) | `SaveLoad`, `SaveInfo`, `ModInfo`, `SavedState` | 11: `runtime/types/save/SaveLoad.h`, `runtime/src/save/*` | B7-5 (`SaveMetaData`'s String and `SetSaveSprite`'s Sprite by hidden pointer; read `Draw`'s ExternalPtr, §13.1), B7-8 (`SaveInfo`); resave ×5 at its gate; `SaveLoad::_singleton` stays a slot |
| 2 | **B3 Achievement** | `Achievement`, `AchievementInstance`, `AchievementManager`, `Goal`, `GoalInstance`, `GoalList` | 29: `runtime/types/achievement/*`, `runtime/src/achievement/*` (11 + `.inc`), `engine/types/AchievementGoals.h`, `code/Game/Goal/*` (14) | B7-3, B7-4 (+ unit test), B7-5, B7-8 (`GoalList`), B7-6 (`GoalListView`); the 15 Goal-family QUALIFIED sites; `code/Game/Tutorial/finishtutorial*` are NOT its files (the Scenario and FinishTutorial sites go to a caller batch) |
| 2 | **B4 Audio** (after B1) | `SoundBuffer`, `SoundEffect`, `Stream`, `Scene` (+ `Channel`, `EmitterInstance`, `EffectSlot`), `Configuration`, `Device` | 17: `runtime/types/audio/Audio.h`, `runtime/src/audio/*` (15 + `.inc`) | B7-7, B7-12; `SoundBuffer` is CompiledExternal; the exe's by-value `Audio::Scene` in MenuDLT needs its ctor door kept |
| 2 | **B5 Input** (after B1) | `KeyMap`, `Binding`, `BindingNode`, `Device`, `State`, `Configuration` | 10: `runtime/types/input/Input.h`, `runtime/src/input/*` | the two missing size asserts; `input_platform_shutdown_win.cpp` includes B1's `Application.h` |
| 2 | **B6 Pathfinding + Picking** | `PointList` (real), `IPathInfo`/`Pathfinding::Scene` (declarations), `Picking::Scene`, `PickingMesh` (+ `SubMesh`, `Entry`, `Cell`, `Link`) | 12: `runtime/types/picking/Scene.h`, `runtime/src/pathfinding/*`, `runtime/src/picking/*` | B7-9; the `PickingMesh` size assert and the `pickingmesh.cpp:87` duplicate folded (B7-6) |
| 3 | fragment | `AchievementManager::PlatformStartup` in `platform_startup_win.cpp` | after B1 and B3 | the only fragment body |
| 3–4 | callers, two rounds, by directory | per `CALLER_SURFACE.md` §1.1 | the 96 unowned files of `B37_FILEMAP.md` §7.2, `dx11/src/devicedata.cpp` included | B7-11; the Scenario and FinishTutorial QUALIFIED sites; the four binding defects of §6.3 are the coordinator's call |
| 5 | close | `engine_funcs` sweep BY BINDING including back-reference spellings and `engine_funcs.h`'s inline bodies (`MenuDialogLoopT_Ctor`); numbers | | |

## 10. Coordinator's dated corrections from the blocks (2026-09-26, recorded by `b37-close`)

Each item names the block that found it. Where an item contradicts §1–§9 above, the item wins; the
text above is kept as block 0 wrote it.

> **CORRECTION to B7-1.6 / §2.1 (block B3) — `Achievement::Goal` slot 8 is `CheckGoalState`, not
> `Construct`.** `GoalInstance::CheckGoalState` (0x18009f060) is `MOV RAX,[goal]; JMP [RAX+0x40]` -- a
> tail call into the goal's slot 8 with `this` only -- and every exe override of slot 8 returns 0/1/2
> (a `Goal::GoalState`), so the pure slot is the goal's state check. `Goal.h` declares it
> `virtual GoalState CheckGoalState() const = 0` (descriptive; the name is not attested). B7-1.6's "`Construct` sits at
> slot 8 on all six External leaves" stays true for `Achievement`, `SoundBuffer`, `SoundEffect`,
> `PointList`, `PickingMesh`; `Goal` is the exception. **`GoalList::Initialize` calls slot 9**
> (`GetInstanceType`, B7-4), confirming `Goal.h`'s `kGetInstanceType = 0x48`. **`Object::Type` did not
> exist in the tree**; block B3 added it as `runtime/types/object/Type.h` (one dword; its
> `static_assert` cites `Goal::GetInstanceType` 0x18009eeb0).

> **CLOSED §13.3 (block B1) — `sizeof(Engine::Engine)` is 0x6f8, PINNED.** `??4Engine@0@`
> (0x18000c7d0) copies with `MOV R8D,0x6f8`. `Engine.h` asserts it and cites the instruction.
> Also from B1: UpdateLoop slot 12 is declared `CreateLoopDialog` (`CreateDialog` is a `windows.h`
> macro, B7-1.2's `kSlot_CreateDialog` stays the comment-level name); `Video::DeviceData` slot 4
> takes a `bool` in DL (the loader-thread acquire/release of §5 — `loops.cpp` keeps the raw slot call
> for that reason); `runtime/types/common.h`'s global forward declarations clash with
> `video/ImageBuffer.h`; retail `MenuDialogLoopT::Shutdown` (0x14009b280) is a 12-step teardown and is
> now transcribed (carry-over 11 CLOSED; a behaviour change, §12).

> **CLOSED §13.1 and §13.2 (block B2).** `SaveLoad::Draw`'s `ExternalPtr<Graphics::Material>`
> travels in a REGISTER (R8, homed at `[RBP+0x2b0]`) — B7-5's ExternalPtr rule holds. `ModInfo` is
> 0x20, read from three bodies. Also from B2: `UpdateList`'s stack template DOES write +0x2c.

> **FROM BLOCK B4 (Audio).** The "unnamed" voice-callback record of §1/§2.1/§5 is
> **`Audio::Device::PlatformChannel`** — `GetPlatformChannel` (0x18000c720) returns
> `this + 0xa98 + i*0x20`; the 0x180157c60 table is that record's `IXAudio2VoiceCallback` vtable.
> `SoundEffect`: +0x20 `GetRadius`, +0x24 `GetInnerRadius`, +0x28 `GetMaxLength` (a `System::Time`;
> `IsLooping` is `[+0x28] == 0`). `SoundBuffer`: +0x48 `GetWMAData`, +0x50 `GetWMALength`.
> `Audio::Stream` has 2 slots (`Run`, `OnCompleted`), not the inventory's 9.

> **FROM BLOCK B5 (Input).** `KeyMap` +0x00 is `_suspendCount` (`Suspend` 0x1800a7f90).
> **`Input::Device` is exactly 0x1200** (§2.3 had it as the mapped size; it is the class size).

> **CLOSED §13.4 (block B6) — `Pathfinding::Scene` is exactly 0x90** (the copy ctor 0x18006ca70 ends
> at the +0x8c dword). `Picking::Scene`'s ctor and `InitGrid` were two copies of one body in the tree;
> B6 folded them.

> **FROM THE QUESTION PASS (0G / 0X), confirmed by the blocks.** The retail `Scenario` table
> (0x14015f020) has **9 slots followed by a zero qword** (the tree's standalone array has 13); every
> retail `Goal`-subclass table has **11 slots** (the tree's 12-slot `TimeGoal`/`FinishTutorialGoal`
> arrays run into the adjacent GoalInstance table). `Audio::Stream` 2 slots (above).

> **BINDING DEFECTS (§6.3) and their fate.**
> 1. `fn_KeyMap_Released` (bound only to the unregistered exe slot 0x1401528f0, always null): K3
>    converted both `dragtool.cpp` callers to `Input::KeyMap::Released`; the global was deleted by the
>    close (commit 1). Behaviour change: the drag tool's key-release paths are live (§12).
> 2. `finishtutorialinstance.cpp:72` resolved exe slot 0x140151c08 as "IsDerivedFrom"; the slot is
>    `?Save@Configuration@Audio@@`. K2 made the call `Object::Base::IsDerivedFrom` per retail
>    0x1400d7bf0. Behaviour change: the tutorial-instance type check is live (§12).
> 3. `g_pEngineSingleton` was a second global for `MainLoop::_singleton`: FOLDED into
>    `g_pMainLoopSingleton` by the close (commit 1). `g_pKeyMapObjectExref` is
>    `Input::Configuration::_singleton`'s exe slot: RENAMED `g_pInputConfigSingletonExref`.
> 4. The exe `AchievementInstance` table 0x14015e350 is still never registered (step 8).
> 5. `citizen_assignworkplace.cpp`'s five unregistered Audio exe slots: K4 made them live member
>    calls (behaviour change, §12).
> 6. REPORTED, NOT FIXED: `Math::Vector3::operator==` (exe slot 0x140152a70) is still unregistered,
>    which keeps the MoveEmitter loop in `citizen_assignworkplace.cpp` dead.

## 11. The blocks, the fragment and the caller rounds, as run

Every block ran on its own worktree branch under `b37_session/BLOCK_BRIEF.md` (caller batches under
`b37_session/CALLER_BATCH_BRIEF.md`); every merge was gated by the coordinator on the merged tree and
landed with **0 problems**. The coordinator's per-merge gate table is below.

| order | block(s) | what it converted |
|---|---|---|
| 0 | question pass (block 0, `951fd29b`; 0F, 0G, 0X read-only) | this sheet, `B37_SLOTS.md`, `B37_FILEMAP.md`, `b37_session/{EXE_SUBCLASSES,CALLER_SURFACE}.md` |
| 1 | B6 Pathfinding + Picking | `Pathfinding::PointList` (real `Object::External`, 9 slots, 0x50), `IPathInfo` / `Pathfinding::Scene` as declarations; `Picking::Scene` (0x60), `PickingMesh` (0x48, the `pickingmesh.cpp` duplicate folded) |
| 1 | B3 Achievement | `Achievement`, `AchievementInstance`, `Goal`, `GoalInstance` (the retslot `GetInstanceType`, unit-tested), `GoalList` (value type; `GoalListView` folded), `AchievementManager`; `Object::Type` |
| 1 | B2 Save | `SaveLoad` (0x90), `SaveInfo`, `ModInfo`, `SavedState`; resave ×5 at its gate |
| 1 | B1 Engine + Application | the four loops (abstract bases, exe tables untouched), `Loader` on the real `System::Thread`, `MainLoop`, `Engine` (0x6f8 pinned), `Application` (27 members); the four `loops.cpp` B6-5 caches; MenuDLT `Shutdown` transcribed (carry-over 11) |
| 2 | F (fragment) | `AchievementManager::PlatformStartup` in `platform_startup_win.cpp` |
| 2 | B5 Input | `KeyMap` (0x18), `Binding`, `BindingNode`, `Input::Device` (0x1200), `Input::Configuration` (0x10) |
| 2 | B4 Audio | `SoundBuffer` (CompiledExternal), `SoundEffect`, `Stream` (Thread subclass, 0x88), `Audio::Scene`, `Audio::Configuration`, `Audio::Device` (0x1298, first header) + `AudioChannel` / `PlatformChannel` |
| 3 | K1 (`code/Game/Dialog`, round 1), K2 (the rest of `code/Game`, round 1) | `engine_funcs` door calls, `ResolveExport` / exe-slot / hex / hand-inlined vtable calls onto the eight namespaces made member calls, each receiver a `// B.3.7 bridge` |
| 4 | K5 (standalone / engine / runtime / dx11, round 2), K4 (Audio callers, round 2, the emitter slots live), K3 (Input callers, round 2, dead caches) | the sites round 1 left for members defined on main only after its launch; dx11's three `Application` rows |
| 5 | close (`b37-close`, commits 1–4, 6, 7) | below |

The closing pass: (1) the `engine_funcs` zero-caller sweep BY BINDING — 74 door globals deleted
whole with their fallbacks, `%p` rows, `CHECK_FN` and readiness guard; `g_pEngineSingleton` folded
into `g_pMainLoopSingleton`; `g_pKeyMapObjectExref` renamed `g_pInputConfigSingletonExref`; the one
printf-argument door call made a member call into a local; 8 ctor/dtor doors and 9 storage globals
kept; (2) the two dead door caches (`citizenai_executor.cpp` `s_slot_pickXf` → a member trampoline
for PerfX; `gameupdateloop_serialize.cpp` `s_fn_AudioScene_Serialize`), `mainloop_run.cpp`'s unused
`Module`/`ResolveAddr` declarations, `engine/types/MainLoop.h`'s 27 dead `MainLoopAddr` constants;
(3) the whole-tree hex-literal sweep — no live call to a defined non-ctor member left (12 resolve
sites, all ctor/dtor doors or storage), 7 dead mangled-name constants removed; (4) stale banners and
READMEs, §10 and §13's status; (5) nothing — no COSMETIC manifest warning; (6)
`b37_session/CLOSE_NUMBERS.md`, the three unmarked Save bridge casts marked, the
`Audio::Configuration` dtor call in `Engine::Shutdown` made a member call; (7) this section, §12,
§14 and the READMEs' status paragraphs.

**Coordinator's gate table (2026-09-26). Every gate = debug + release + perf build, warning diff vs
a clean-rebuild baseline, unit, `dispatch_slot_map --check`, cutover manifest, `rt_mint_gen
--check`, new-game / tutorial / Load Game routes, pointer-free type table vs baseline, resave ×3
(×5 for Save): all 0 problems, typetable == baseline, `[RtResolve] 0 misses`, 0 `[RtMint] TRAP`.**

| merge | unit tests | result |
|---|---:|---|
| baseline (8cce427a) | 462 | 0 problems |
| B6 (Pathfinding+Picking) 147bb9d5 | 474 | 0 problems |
| B3 (Achievement) e5792443 | 483 | 0 problems |
| B2 (Save) 38c57c59 | 491 | 0 problems, resave 5/5 |
| B1 (Engine+Application) bea919a2 | 501 | 0 problems (2 warning rows were the pre-existing CallbackPresent unused fn with its type respelled) |
| F (fragment PlatformStartup) c24bb4ca | 501 | 0 problems |
| K1 (code/Game/Dialog callers r1) 049e03f6 | 501 | 0 problems |
| B5 (Input) a026c2fb | 504 | 0 problems |
| K2 (code/Game rest callers r1) 01dd683a | 504 | 0 problems |
| B4 (Audio) 53782e1d | 516 | 0 problems |
| K5 (standalone/engine/runtime/dx11 callers r2) ca222c0d | 516 | 0 problems |
| K4 (Audio callers r2, incl. emitter slots live) 7ecd16cd | 516 | 0 problems |
| K3 (Input callers r2 + dead caches) d5100882 | 516 | 0 problems |
| close (b37-close commits 1-7) db990145 | 516 | 0 problems |
| step close (b37_session/stepclose.sh on 6e8afd2d) | 516 | 0 problems (Mods, Quit, Options, Achievements routes; in-game save; sa-stress exit 0; true release compile; clean rebuild + full gate 0 problems) |
| close (b37-close, commits 1–4, 6, 7) | 516 | static gate per commit (below); the step close is the coordinator's |

The closing pass ran the static gate only, per commit: `standalone-debug`, `standalone-perf`, `unit`
(516 / 0 failures), `dispatch_slot_map --check` (0), `cutover_manifest --gate` (OK, 59 rows, no
COSMETIC warning at any commit), `rt_mint_gen --check` (UP TO DATE), and a warning A/B of every
changed `.cpp` under both flag sets. No game run.

## 12. Deliberately left (sizes measured at the close; `b37_session/CLOSE_NUMBERS.md`)

**Doors that remain, by design (§7, B4b-2, B7-13):** 285 mangled names of the eight namespaces are
still registered by `RegisterRuntimeExport` — 238 one-line qualified delegations to members, 24
dtors, 22 ctors, 1 `operator=`; the factory statics through the `rt_factories_*.inc` tables. In
`engine_funcs`: `fn_DialogLoaderLoopT_Ctor` (7 calls), `fn_LoaderLoop_Ctor` (7), `fn_UpdateLoop_Ctor`
(4), `fn_SaveLoad_Dtor` (4), `fn_Goal_BaseCtor` (3), `fn_AudioScene_Ctor` (2), `fn_SavedState_Ctor`
(2), `fn_DialogLoopT_Ctor` (1). In `code/Game`: 18 by-name ctor/dtor/type-info resolves (the exe's
by-value and heap holders — `Audio::Scene`, `Picking::Scene` ×3, `SaveLoad`, `SavedState`, `Engine`
×2, `MainLoop` ×2 by address, `UpdateLoop`, `AchievementInstance`, `Goal` ×2, `GoalInstance` ×2,
`Achievement` by address). The non-polymorphic dtor doors among them (`Audio::Scene`,
`Picking::Scene`, `SaveLoad`, `SavedState`) have defined dtors; they stay doors because the calls
replicate exe import calls on exe-held objects (the caller batches' reading of §2.4) — a step-8
decision with the holders.

**Retired only at the END of B.3:** 16 mint vtable rows (15 named + the `PlatformChannel` row
0x180157c60), 8 `?_typeInfo` rows, 12 `RT_DELDTOR` rows; the singleton STORAGE (carry-over 5): 9
`CUTOVER_REQUIRED` manifest rows plus `?_singleton@SaveLoad@Save@@` (0x1801fc6f0), which still has
NO manifest row, 36 `RT_DATA_SLOT` reads of that storage under `runtime/src`, 9 `engine_funcs` data
globals, 6 exe-slot exref reads.

**Declared, not defined (the honest ceiling): 105 non-ctor members** — `Pathfinding::Scene` 15
(dead in retail: declarations only, and `IPathInfo`'s 2), `Engine::MainLoop` 12 (the retslot `Time`
getters and the post-callback API, no caller), `Audio::Device` 11, `Input::Device` 10,
`Input::Configuration` 9, `Picking::Scene` 9, `Engine::Engine` 6, `AchievementManager` 5,
`Audio::Scene` 5, `Audio::Stream` 5, `GoalList` 4, `SoundBuffer` 3, `KeyMap` 3, `Audio::Configuration`
2, `SoundEffect` 2, `LoaderLoop` 2, `GoalInstance` 1, `PickingMesh` 1. Plus 8 undefined dtors that are
doors (`~Engine`, `~Application`, `Input::~Configuration`, `~KeyMap`, `Audio::~Device`, …).

**Carry-overs (not done in step 7, each with its size):**
1. **The exe-phase subclasses (step 8):** five loop tables (`GameUpdateLoop`, `MenuDialogLoopT`,
   splash, two `LoaderLoop`), the Goal family (5 `Goal` + 2 `GoalInstance` tables), `Scenario`, and
   the never-registered exe `AchievementInstance` table 0x14015e350; the retail Scenario table's 9
   slots vs the tree's 13 and the Goal tables' 11 vs 12 (§10).
2. **The `RtEngine::` loop PODs** stay because B1 did NOT alias
   `engine/types/{UpdateLoop,LoaderLoop,DialogLoaderLoopT,MainLoop}.h`: 95 `.m_`/`->m_` uses of their
   36 field names in 7 files (`gameupdateloop.cpp` 44 — an upper bound, `GameUpdateLoop`'s own
   fields share some names — `splash_loop_stubs.cpp` 31, `loader_loop_stubs.cpp` 12,
   `dialogloaderloop.cpp` 4, `menudialogloopt.cpp` 2, `worldgen.cpp` 1, `menudialogloopt_stubs.cpp`
   1). `UpdateLoopVtable` stays with the hand-built tables.
3. **`goal_exe_helpers.cpp:210-235`** installs `External::Compile` and `Goal::GetInstanceType` into
   the exe Goal tables BY NAME (two `ResolveExport`s in a function-local cache) — it goes when the
   Goal subclasses become C++ (step 8).
4. **`Stub_Destroy` slots** of MenuDLT (`menudialogloopt_stubs.cpp:242`), the splash
   (`splash_loop_stubs.cpp:132`) and the `LoaderLoop` tables skip the base dtors retail's slot 0
   runs (`EXE_SUBCLASSES.md` §3B) — a behaviour decision for step 8.
5. **`KeyMap`'s two split fields** stand in for a `Collection::Set<Binding>`; the template needs a
   byte-key compare policy first.
6. **19 `rtpickgrid::` free functions** (`runtime/src/picking/scene_grid.h`) remain the
   `Picking::Scene` grid internals, registered by address.
7. **Address-documentation constants:** `RtEngine::MainLoopAddr` (27) and `EngineAddr` (11) in
   `runtime/types/engine/` have no reader (engine.cpp reads only the two device-slot OFFSETS).
8. **`Math::Vector3::operator==`** (exe slot 0x140152a70) is still unregistered, which keeps the
   MoveEmitter loop in `citizen_assignworkplace.cpp` dead — reported, not fixed.
9. The device-lost / loader race (`DEVICE_LOST_LOADER_RACE.md`) is the user's decision; nothing in
   step 7 moved or added a call in the bracket.
10. `gameupdateloop_load.cpp`'s `BuildSkyMesh_Step28` keeps two function-local lazy resolves that run
    on the loader thread; they are Graphics/System symbols (not step 7) — a B6-5 item for their
    namespace.

**Behaviour changes in step 7 (each gated):** the drag tool's key-release paths are live (K3,
`fn_KeyMap_Released` was always null); citizen emitter teardown / volume are live (K4, five
unregistered Audio exe slots made member calls); the tutorial-instance type check is live (K2,
`Object::Base::IsDerivedFrom` per retail 0x1400d7bf0 where the tree had resolved
`Audio::Configuration::Save`'s slot); MenuDLT `Shutdown` runs retail's full 12-step teardown (B1);
`PointList` (B6) and `Audio::Scene` (B4) allocate through the `rtcoll` door.

---

## 13. NOT VERIFIED at block 0 — status at the close (2026-09-26)

CLOSED: item 1 (`Draw`'s ExternalPtr in a register, B2), 2 (`ModInfo` 0x20, B2), 3 (`Engine` 0x6f8
pinned, B1), 4 (`Pathfinding::Scene` 0x90, B6), 5 (the five header-less classes now have headers:
`audio/{Device,SoundBuffer,SoundEffect,Stream}.h`, `pathfinding/PointList.h`, B4/B6), 7 (the
`engine_funcs` population re-measured BY BINDING at the close: 82 door pointers + 10 data globals,
exactly 0X's census; `b37_session/CLOSE_NUMBERS.md` §3a). OPEN: item 6 (the exe Goal subclasses'
slot 10, step 8). Items 8–10 describe block 0 only; the blocks built, gated and ran (§11). The
original text is kept below as written at block 0.


Read from RETAIL BYTES this pass: all 16 exported vtables in the set + `System::Thread` + the five Object
parents, slot by slot; the unexported 0x180157c60 table's seven bodies (raw bytes); every `sizeof` in
§2.3 marked with an instrument; all 16 type-name literals; the stampers of every vtable; the head of every
`??0`/`??1` export of all 30 classes; the 25 retslot exports (three prologues read) and the 16 by-value-
argument members' prologues; `Loader::Run`, `Loader::Load`, `Stream::Run`, `Engine::Startup` in full;
the five exe loop tables, the eight further exe subclass tables, every exe call site of a step-7 ctor/dtor
import; the exe's and `VideoDX11-x64.dll`'s import tables. Read from the TREE: every `static_assert(sizeof)`
under the seven `runtime/types/<ns>` dirs and `engine/types`, the mint and `RT_DELDTOR` rows, the manifest
rows, the hex-literal sweep.

1. **`Save::SaveLoad::Draw`'s `ExternalPtr<Graphics::Material>` argument** — R8 is homed and its first
   use is past the 60 instructions read (§2.6). Expect bare register (B7-5); the Save block reads it.
2. **`Save::SaveLoad::ModInfo`'s 0x20** — the tree's assert; no retail stride was read.
3. **`Engine::Engine`'s size** stays inferred (≥0x6f2; the exe keeps it on `GameMain`'s stack and nothing
   bounds that slot).
4. **`Pathfinding::Scene`'s exact size** (≥0x90) — dead code, not pinned.
5. **The fields of the five header-less classes** (`Audio::Device`, `SoundBuffer`, `SoundEffect`,
   `Stream`, `PointList`) beyond the offsets quoted here — each block pins its own (B6-8).
6. **The exe Goal subclasses' slot 10** — an exe-introduced virtual; its meaning is b37-0X's / step 8's.
7. **The `engine_funcs` door census** is b37-0X's (82 doors + 10 data globals, 233 door calls, by
   initialiser), not re-derived by block 0; block 0's hex sweep agrees with its resolution-site totals
   (§6.3). The closing owner re-measures BY BINDING including the back-reference spellings (B7-11).
8. **The exe `Unwind@`/`Ghidra auto-name` labels in `B37_SLOTS.md` §E** are Ghidra's, not attested names.
9. **Tool side effect:** one `disassemble_bytes` call at 0x1800501c0 (length 128) was made while reading
   the callback bodies; it may have created instructions in the Ghidra database for undefined bytes
   (no rename, no annotation, nothing saved by this pass). The bodies above were decoded from
   `read_memory` bytes, not from that listing.
10. **I did not build, run or gate anything.** `switch_program` was returned to `Runtime-x64.dll` after
    each exe read.

## 14. Corrections from the closing pass (2026-09-26, `b37-close`)

1. **§6.3's `engine_funcs` population (82 door pointers + 10 data globals) is exact** — re-measured
   by initialiser at 765b2891 and at the start commit 951fd29b. The caller batches converted every
   call but deleted no global (the brief's invariant), so the close deleted 74 whole; 8 ctor/dtor
   doors and 9 storage globals remain, each referenced.
2. **The two misbound data globals.** `g_pEngineSingleton` and `g_pMainLoopSingleton` held the SAME
   value in standalone (exe slot 0x140151440 is registered with the `MainLoop::_singleton` export;
   `gameupdateloop.cpp`'s late alias never fired) — folded. `g_pKeyMapObjectExref` is NOT a second
   name for `g_pInputConfigSingleton`: it is one indirection further out (the exe slot, not the
   storage), and standalone registers that slot with a two-link chain that ends in the `KeyMap*`
   (`address_registry.cpp` `g_StandaloneKeyMapMid`), so it was renamed
   (`g_pInputConfigSingletonExref`), not folded.
3. **`EngineFuncsInit`'s last readiness guard** (`if (!fn_InputDevice_Update) return false;`) guarded
   a pointer nobody called; it went with the global. Input::Device::Update is a registered door and
   `MainLoop::Run` calls the member.
4. **"NOT exported" claims that were false:** `runtime/types/engine/MainLoop.h` filed `Update`,
   `Frame`, `Draw`, `CreateVideoContext`, `ReleaseVideoContext` under "Internal, NOT exported" — all
   five are exports (two private); `input_win.cpp` called the private `Input::Device::PlatformUpdate`
   (ordinal 3411) unexported in two places while its own registration block said otherwise. Four
   `runtime/src/<ns>/README.md` said "_nothing yet_" over ported directories. The rule stands: check
   `rt_export_addrs.inc` before writing "not exported" (the AEAA trap, B35 memory).
5. **A defined dtor still called through its door.** `Engine::Shutdown` resolved
   `??1Configuration@Audio@@` by name although `Audio::Configuration::~Configuration` is a defined,
   non-virtual member; the close made it a qualified member call (as K4 had done for
   `~AchievementManager`). The Input::Configuration and Application dtors stay doors — declared, not
   defined.
6. **Hex sweeps: anchor on the demangled OWNER, not on `@NS@`.** K3's `code/Game` hexmine truncates the
   name at the first `@@` and then needs `@NS@` preceded by `@`, so `??0Achievement@0@QEAA@XZ`
   (`scenario.cpp:61`, the Scenario base's ctor door) is invisible to it. The close's `hexall.py`
   demangles the owner scope (0X's `common.ns_of`) and finds it. Result: 66 → 12 live resolve lines,
   no live call to a defined non-ctor member left.
7. **Warning A/B across a header change.** `warn_ab2.sh` compiles the BASE copy of a `.cpp` against
   the WORKING tree's headers. When a commit removes a name from a header (commit 1's
   `g_pKeyMapObjectExref`), the base TU has errors, GCC suppresses later diagnostics such as
   `-Wunused-but-set-variable`, and pre-existing warnings are reported as NEW (edittool.cpp, PERF).
   Compile the base inside a worktree at the base commit (`warn_ab3.sh` in the closing scratchpad).
8. **This pass's own miscount:** commit 3's message gives the header constants as `EngineAddr` 12 and
   `RtEngine::MainLoopAddr` 26; they are 11 and 27 (39 in all with `modal_driver.cpp`'s one, which
   the message's total states correctly).
9. **Commit 5 was not needed:** no COSMETIC `stale` warning appeared at any commit, and
   `cutover_manifest.py --inc` regenerates `standalone/cutover_manifest.inc` byte-identically.
