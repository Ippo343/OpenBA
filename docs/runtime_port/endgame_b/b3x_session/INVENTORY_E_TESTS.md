# INVENTORY E — the unit tests (`tests/unit/`) under the normal-virtual cutover

Block 0, NEXT_SESSION.md §3b question 6. READ-ONLY inventory: nothing was built, linked or run. Every count is
a grep / python parse over `tests/unit/*.cpp,*.h` with comment lines excluded; anything that follows from the
Itanium ABI but was not compiled is marked **NOT VERIFIED**. Per-file working notes (four groups, with every
file:line) are in the session scratchpad `b3x/inv-e/notes_{ui,gfx,io,exe}.md`; this file carries the conclusions.

## 0. Headline — partc.py undercounts the tests by an order of magnitude

partc's test rows match one helper name (`PmfSlot`) and the spellings `DeletingDtor(0)` / `(1)` only. The suite
has eleven names for the same PMF decoder, spells the flags `0u`/`1u`/`3`/`2`/`9u`, and reads slot 0 through
`_ZTV…` symbols and ~20 per-file `SlotAt` helpers that no partc key matches.

| what (non-comment lines in tests/unit) | partc §3a | actual | files |
|---|---:|---:|---:|
| PMF slot-order checks (`SlotOffset`, `PmfSlot`, `GfxSlotOffset`, `CeSlotOffset`, `ElSlotOffset`, `PlSlotOffset`, `B39SlotOffset`, `SlotOf`, `ThreadSlotOf`) | 92 | **1,192** | 33 |
| … of which take `&X::DeletingDtor` (ill-formed after the flip: no PMF to a dtor) | — | **128** | 31 |
| … on EXEMPT classes (must NOT shift) | — | **38** (+13 ReaderWriter-family, +14 `*Data`, both decision-dependent) | 7 |
| `DeletingDtor(unsigned)` declarations in test classes | 119 | 119 | 26 |
| `DeletingDtor(0)` / `(0u)` calls | 109 | **116** | 22 |
| `DeletingDtor(1)` / `(1u)` calls | 32 | **40** | 20 |
| **vector arm** `DeletingDtor(3)` / `(2)` (flags & 2) | 0 | **41** | 20 |
| `Object::Destroy(` | 17 | 17 | 3 |
| `has_virtual_destructor` asserts (almost all `!…` = "no virtual dtor") | 0 | **198** | 51 |
| `is_destructible` asserts (almost all `!…` = "dtor protected") | 0 | **97** | 22 |
| raw slot-0 dispatch through `_ZTV<class>[2]` (a slot-0 "delete" call) | 0 | **9** (rb_graphics 4, rb_ui 3, rb_video 1, rb_achievement 1) | 4 |
| per-file raw-slot helpers (`SlotAt`, `SlotAs`, `RawSlot`, `VTableOf`, `PkgSlot`) | 24 ("slot helpers", engine_loop only) | ~20 helper copies, **~400 call/index sites** | 25 |

The helper definition count: `PmfSlot` is defined **33 times** (one copy per file, plus `B39SlotOffset` in
`b39_uib_test_util.h` shared by ui_leaf_c4/c5). There is no shared header today. One representative
(`tests/unit/audio_abi_test.cpp:423-431`; every copy carries the same `static_assert`):

```cpp
template <typename PMF>
uintptr_t PmfSlot(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);
    if (words[1] != 0) return ~uintptr_t(0);
    return words[0] - 1;                 // BYTE offset; SlotOf/ThreadSlotOf return an INDEX
}
```

Per-file table (non-comment lines; `pmf(dd)` = PMF checks (of which `&X::DeletingDtor`); `hvd`/`isd` = trait lines):

| file | DD decl | DD(0) | DD(1) | DD(2/3) | pmf(dd) | hvd | isd | Destroy | disposition notes |
|---|---:|---:|---:|---:|---|---:|---:|---:|---|
| achievement_goal_abi_test | 0 | 0 | 0 | 0 | 30 (4) | 4 | 0 | 0 | script; ProbeGoal mirror reads KEEP |
| act_abi_test | 0 | 0 | 0 | 0 | 52 (2) | 2 | 0 | 0 | script |
| audio_abi_test | 3 | 3 | 1 | 1 | 25 (2) | 4 | 3 | 0 | PlatformChannel 7 PMF KEEP; Stream/Thread reads KEEP; slot-0 on stack probe :147 HAND |
| b39_eng_collision2d_test | 0 | 3 | 1 | 1 | 18 (2) | 2 | 2 | 0 | script + arms helper |
| b39_io_classes_test | 0 | 9 | 7 | 9 | 45 (11) | 12 | 12 | 0 | Encoding 5 + PersistenceWorker 2 KEEP; 13 ReaderWriter-family lines follow the RW decision |
| b39_obj_compiledexternal_test | 0 | 1 | 3 | 3 | 12 (3) | 3 | 3 | 0 | exercises the LINKED Base/External arms |
| b39_obj_externallist_test | 0 | 2 | 1 | 1 | 9 (1) | 1 | 1 | 0 | script + arms helper |
| b39_sys_thread_test | 0 | 0 | 0 | 0 | 2 (0) | 1 | 1 | 0 | **KEEP entirely** (System::Thread exempt) |
| b39_uib_test_util.h | 0 | 1 | 1 | 1 | 21 (2) | 1 | 1 | 0 | home of the UI arms/PMF templates |
| b39_vid_data_test | 0 | 1 | 1 | 1 | 14 (1) | 1 | 1 | 0 | **whole file follows §3b Q2** (dx11 `*Data` seam) |
| b39_vid_program_test | 0 | 4 | 1 | 1 | 50 (6) | 6 | 6 | 0 | script + arms helper |
| cit_abi_test | 0 | 0 | 0 | 0 | 65 (1) | 1 | 0 | 0 | script (already `S(slot)` index form) |
| dlg_abi_test | 0 | 0 | 0 | 0 | 16 (1) | 1 | 0 | 0 | script |
| engine_loop_abi_test | 0 | 0 | 0 | 0 | 23 (1) | 3 | 0 | 0 | Loader Run/OnCompleted 2 KEEP; `has_virtual_destructor<Loader>` KEEP |
| exe_class_abi_test | 0 | 0 | 0 | 0 | 43 (3) | 4 | 0 | 0 | Controller 9 PMF + 1 trait KEEP; all raw reads KEEP |
| goal_abi_test | 0 | 0 | 0 | 0 | 71 (7) | 1 | 0 | 0 | script |
| graphics_animation_abi_test | 2 | 2 | 2 | 2 | 9 (1) | 2 | 2 | 0 | constants centralised (81-89) |
| graphics_geometry_abi_test | 1 | 0 | 0 | 0 | 0 | 11 | 0 | 0 | PURE MIRROR hierarchy; 9 product trait lines invert |
| graphics_material_abi_test | 5 | 2 | 1 | 1 | 16 (2) | 2 | 2 | 0 | constants centralised (113-121) |
| graphics_particle_abi_test | 3 | 0 | 0 | 0 | 0 | 4 | 0 | 0 | PURE MIRROR |
| io_streams_abi_test | 1 | 0 | 0 | 0 | 0 | 5 | 0 | 0 | constants centralised (73-81); slot 0 on stack probe :96 HAND |
| io_vtable_abi_test | 9 | 0 | 1 | 0 | 0 | 14 | 3 | 0 | mirrors KEEP; Encoding KEEP; ReaderWriter decision; TailReaderWriter HAND |
| k1_abi_test | 0 | 0 | 0 | 0 | 23 (3) | 3 | 0 | 0 | `bad += …` arithmetic form — hand |
| loop_abi_test | 0 | 0 | 0 | 0 | 61 (9) | 10 | 0 | 0 | PathfinderThread 2 PMF + 1 trait KEEP |
| mia_abi_test | 0 | 0 | 0 | 0 | 24 (1) | 2 | 0 | 0 | TableController reads/trait KEEP |
| mib_abi_test | 0 | 0 | 0 | 0 | 48 (3) | 4 | 0 | 0 | TableController reads/trait KEEP |
| object_external_test | 0 | 0 | 0 | 0 | 0 | 4 | 0 | 2 | raw vt[1..7] SHIFT, vt[0] HAND |
| object_internal_test | 1 | 0 | 0 | 0 | 0 | 3 | 0 | 13 | TestInternal recorder HAND (§2) |
| object_typeinfo_test | 3 | 0 | 0 | 0 | 0 | 1 | 0 | 2 | stack `Destroy(&leaf)` HAND (§2) |
| object_vtable_abi_test | 3 | 1 | 0 | 0 | 0 | 1 | 0 | 0 | the rule's own test — rewritten, §3.2 |
| pathfinding_picking_abi_test | 3 | 4 | 2 | 2 | 18 (2) | 3 | 3 | 0 | constants centralised (60-68); IPathInfo KEEP; slot 0 on stack probe :97 HAND |
| pnl_abi_test | 0 | 0 | 0 | 0 | 174 (4) | 3 | 0 | 0 | script |
| rb_achievement_abi_test | 3 | 5 | 1 | 1 | 0 | 4 | 4 | 0 | `_ZTV` slot-0 call :218 HAND |
| rb_engine_abi_test | 4 | 4 | 1 | 1 | 0 | 4 | 1 | 0 | Loader slot 0 = Run :322 KEEP; qualified base DD(0) needs Q3 |
| rb_graphics_abi_test | 4 | 9 | 11 | 9 | 44 (10) | 14 | 12 | 0 | 4 `_ZTV` slot-0 calls HAND |
| rb_ui_abi_test | 5 | 4 | 0 | 2 | 0 | 5 | 0 | 0 | 3 `_ZTV` slot-0 calls HAND; `(2)` vector-no-free ×2 |
| rb_video_abi_test | 0 | 1 | 0 | 0 | 0 | 2 | 1 | 0 | 1 `_ZTV` slot-0 call HAND |
| system_abi_test | 0 | 0 | 0 | 0 | 0 | 1 | 0 | 0 | **KEEP entirely** (Thread; RawSlot 0/1 = Run/OnCompleted) |
| tool_abi_test | 0 | 0 | 0 | 0 | 44 (5) | 8 | 0 | 0 | script; RetslotProbe read KEEP |
| ui_dialog_abi_test | 8 | 3 | 1 | 1 | 9 (5) | 1 | 4 | 0 | constants centralised (DI/DC/EX 91-102) |
| ui_element_abi_test | 3 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | constants centralised (111-122) |
| ui_leaf_c1_abi_test | 12 | 9 | 1 | 1 | 42 (10) | 8 | 10 | 0 | expectation computed `off/8` — translate in helper, see §1.3 |
| ui_leaf_c2_abi_test | 12 | 10 | 1 | 1 | 21 (10) | 5 | 10 | 0 | same as c1 |
| ui_leaf_c3_abi_test | 2 | 9 | 1 | 1 | 20 (10) | 2 | 10 | 0 | macro probes record flags |
| ui_leaf_c4_abi_test | 3 | 10 | 0 | 0 | 0 (+10 template calls) | 1 | 0 | 0 | SlotAt takes an INDEX |
| ui_leaf_c5_abi_test | 16 | 14 | 0 | 0 | 0 (+14 template calls) | 1 | 0 | 0 | SlotAt takes an INDEX |
| ui_resources_abi_test | 7 | 5 | 1 | 1 | 9 (1) | 5 | 5 | 0 | Font treated as an ordinary flip class (see §7 open item) |
| video_data_abi_test | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | PURE MIRRORS of `*Data` — follow §3b Q2 |
| video_imagebuffer_abi_test | 1 | 0 | 0 | 0 | 0 | 6 | 0 | 0 | PURE MIRROR; `DeletingDtor(9u)` :296 HAND |
| video_program_abi_test | 1 | 0 | 0 | 0 | 0 | 5 | 0 | 0 | PURE MIRROR |
| vis_abi_test | 0 | 0 | 0 | 0 | 103 (2) | 2 | 0 | 0 | RootController 9 PMF + 1 trait KEEP |
| wrk_abi_test | 0 | 0 | 0 | 0 | 31 (3) | 3 | 0 | 0 | script; WrkGetTargetProbe read KEEP |
| **total (52 files)** | **119** | **116** | **40** | **41** | **1,192 (128)** | **198** | **97** | **17** | |

## 1. The PMF checks (question 1)

### 1.1 Exempt classes — these checks must NOT shift (derived from the product headers)

| class | tests | lines | evidence |
|---|---|---:|---|
| `ControllerComponent::Controller` (9 pure slots, slot 0 `FindState`, "no dtor slot in 0x14015f070") | exe_class_abi_test.cpp:121-129 | 9 PMF + 1 trait (:76) | code/Game/Component/ControllerComponent.h:48-73 |
| `StateMachineComponent::RootController` | vis_abi_test.cpp:288-296 | 9 PMF + 1 trait (:209) | code/Game/Component/StateMachineComponent.h:52-66 |
| `ControllerLive::TableController<Tag>` (`StateController`) | mia:140, mib:172 | 2 traits | code/Game/Component/controller_state_table.h:330-333 |
| `PathfinderThread` (Thread family, slot 0 `Run`) | loop_abi_test.cpp:347-348 | 2 PMF + 1 trait (:346) | code/Game/Game/PathfinderThread.h:25-40 |
| `Engine::Loader` (Thread family) | engine_loop_abi_test.cpp:142-143 | 2 PMF + 1 trait | runtime/types/engine (retail `??_7Loader` [0] Run) |
| `System::Thread` | b39_sys_thread_test.cpp:81,84; system_abi_test.cpp:199 | 2 PMF + 2 traits | runtime/types/system/Thread.h |
| `IO::PersistenceWorker` (Thread family) | b39_io_classes_test.cpp | 2 PMF | |
| `Audio::Device::PlatformChannel` (IXAudio2VoiceCallback, no dtor slot; asserts `!has_virtual_destructor` itself) | audio_abi_test.cpp:552-558 | 7 PMF + 1 trait | audio_abi_test.cpp:549-551 |
| `IO::Encoding` (5-slot policy table) | b39_io_classes_test.cpp:67-71 | 5 PMF | runtime/types/io/Serializer.h:287-295 |
| **total firm-exempt** | | **38 PMF**, ~10 trait lines | |
| decision-dependent: `IO::ReaderWriter` family (BinaryReader 7, BinaryWriter 3, Visitor 3) | b39_io_classes_test.cpp | 13 PMF | retail slot 0 IS a dtor (Serializer.h:476-483) but product code reads it BY INDEX through `rt_vtbl::rw_vtbl::k*` (~54 files, serializer_class.cpp:175 `RwSlot(rw, kDtor)(rw, 1)`). Exempt or flip is inv-A/B's call. |
| decision-dependent: the twelve `Video::*Data` (§3b Q2) | b39_vid_data_test.cpp (all 14 PMF) + video_data_abi_test.cpp mirrors | 14 PMF | these are exactly the retail offsets `dx11_seam.cpp` stamps |

Everything else (1,192 − 38 − 27 ≈ 1,127 lines) is on a class whose retail slot 0 is the deleting destructor and
flips: 128 lines name `&X::DeletingDtor` (→ cannot compile), the rest are retail slot N ≥ 1 (→ compiler N + 1).

### 1.2 Proposed rewrite: ONE shared header, the shift derived from the type

`tests/unit/slot_check.h` (new), replacing all 33 copies:

```cpp
// Itanium PMF decode: compiler vtable INDEX of a virtual member, -1 if non-virtual or this-adjusted.
template <class PMF> long CompilerSlot(PMF pmf);
// Retail slot n of class T, as the compiler numbers it. A class whose retail slot 0 is the deleting
// destructor spends TWO Itanium slots (D1, D0) on `virtual ~T()`, so its retail slots n >= 1 sit at n + 1.
// A class without a destructor slot (Thread family, Controller interface, Encoding, PlatformChannel)
// has no virtual destructor and is not shifted. No exempt list to maintain: the type says which it is.
template <class T> constexpr long RetailSlot(int n) {
    return n + (std::has_virtual_destructor<T>::value && n >= 1 ? 1 : 0);
}
#define CHECK_SLOT(T, M, n)   CHECK_EQ(CompilerSlot(&T::M), RetailSlot<T>(n))
#define CHECK_DTOR_SLOT(T)    CHECK(std::has_virtual_destructor<T>::value)  // was PmfSlot(&T::DeletingDtor) == 0
```

- The retail slot NUMBER stays written in every test (`CHECK_SLOT(Audio::SoundBuffer, Serialize, 5)`), so the
  tests keep citing retail, and the +1 is proven by the flip rather than hand-edited into 1,000 literals.
- Because `RetailSlot<T>` is a no-op while `has_virtual_destructor<T>` is false, **the conversion can land BEFORE
  the cutover as a zero-change refactor** (gate: `make unit` still 914/914), and the cutover commit then only
  changes `CHECK_DTOR_SLOT`'s body. That takes 1,064 of the 1,192 edits out of the cutover's diff. Recommended.
- Byte-literal files (`(uintptr_t)0x40`) convert by regex to index form (`0x40/8`); the five `S(slot)` files
  (cit, loop, vis, mia, mib) are already index form; `k1`'s `bad += SlotOffset(..) != 0x48` (2 templates, 23
  lines), `loop:375` and `vis:342-343` (PMF held in a variable) are the hand cases.
- Guard against a silent mistake: `RetailSlot<T>(0)` on a flipped class is meaningless; `CHECK_SLOT` should
  `CHECK(!(has_virtual_destructor<T> && n == 0))` so a stray slot-0 check fails loudly.

## 2. Test stand-in classes (question 2)

**Where.** 119 declaration lines in 26 files. Classification:

| kind | files | what the override does | after the flip |
|---|---|---|---|
| trivial `return this;` on a PRODUCT subclass (makes it concrete / gives it its own table) | rb_engine (4), rb_graphics (4), rb_achievement (3), rb_ui (5), ui_leaf_c1 (12), c2 (12), c5 (16), c4 (2), audio BareExternalProbe, pathfinding BareExternalProbe, ui_* BareExternal/BareInternal probes, graphics_material BareExternalProbe | nothing | **delete the line** (an `override` of a vanished virtual is a compile error; the classes are concrete without it since B.3.9) |
| recording `Note(this, 0[, flags])` on a PRODUCT subclass | audio SoundBuffer/SoundEffectProbe :127/130; pathfinding PointList/PickingMeshProbe :153/379; io_streams STREAM_PROBE macro :152; io_vtable OrderProbe/Stream/MemoryStream/FileStream/BinaryFileStream probes :1138-1500, PkgOrderProbe :1646; ui_dialog 6 (:125-140); ui_element 2 (:129/136, record flags); ui_resources 6; ui_leaf_c3 `C3_PROBE` macro (records flags); c4 `C4_PROBE` macro (records flags); graphics_animation 2; graphics_material 4 | records that slot 0 was reached, never destroys | `~Probe() override { Note(this, 0); }` — but see the double-destruction trap in §3.1 |
| real behaviour on a PRODUCT subclass | object_internal `TestInternal` :98 (counts, records flags, runs `~TestInternal`, frees on `&1`); object_typeinfo `TestBase/Middle/Leaf` :382-408 (count per level, record flags, never free — stack objects); io_vtable `TailReaderWriter` :930 (`CHECK_EQ(flags, 1)`, reads/writes the 4-byte tail; objects are `rw[3]` MEMBERS of HeaderEncoding) | | **SEMANTIC**, below |
| PURE MIRRORS (no product base; hand-declared `virtual X* DeletingDtor`) | object_vtable FakeBase/FakeExternal/FakeComponent; io_vtable TestReaderWriter/TestStream family; graphics_geometry Probe* (5); graphics_particle Probe* (3); video_data Mirror*Data (4, §3b Q2); video_imagebuffer MirrorImageBuffer; video_program MirrorShaderProgram; achievement_goal ProbeGoal (`Slot0`) | records flags | they still compile; they stop describing the product. Re-declare with `virtual ~Mirror()` where the mirrored class flips (geometry, particle, imagebuffer, program, object_vtable); KEEP where it doesn't (io Encoding mirrors, and `*Data`/ReaderWriter mirrors if those stay exempt) |

**Tests that ASSERT on DeletingDtor behaviour → semantic rewrite:**

| test | asserts today | proposed rewrite |
|---|---|---|
| object_internal_test.cpp:260 `destroy_reaches_slot0_with_flags_1` (+ the 12 other `Destroy` calls) | slot 0 called once, flags == 1, derived dtor ran, handle table emptied | `~TestInternal() override { ++g_derivedDtorCalls; }` + class-scope `static void operator delete(void* p) { ++g_deleteCalls; g_lastDeleted = p; InternalMemFree(p); }` on TestInternal; `delete obj;` asserts derived dtor 1, `g_deleteCalls == 1 && g_lastDeleted == obj`, table empty. `Destroy(nullptr)` → `delete (TestInternal*)nullptr` (operator delete not called — assert that). |
| object_typeinfo_test.cpp:441 `destroy_reaches_slot_zero_with_flags_one` + :464 raw slot 0 | ONLY the most-derived slot 0 runs, flags 1, returns this — on a STACK object | Meaning changes: with destructors all three levels run, leaf → middle → base. Record in the three `~T()`; heap-allocate (or give TestBase a recording no-op `operator delete`, since `delete &stackObj` would free a stack address); assert order [2,1,0] once each and operator delete received the address. The raw-slot half becomes "`vt[1]` (D0) runs the same chain and calls operator delete; `vt[0]` (D1) runs it without" — the new two-slot order pinned. |
| object_external_test.cpp:190-199 slot0(p, 0) then `Destroy(p)` | slot 0 flags 0 returns this; Destroy frees via TypeTreeFree | `p->~TestExternal()` (dispatching via `Object::Base*` too) + re-placement; `delete p` — **Object::Base has no class operator delete** (TypeInfo.h), so `delete` goes to `::operator delete` = `free`, which matches `TypeTreeAlloc`'s malloc branch in the unit build but NOT the product's `TypeTreeFree` — that is §3b Q3's problem, and this test is where it will show. |
| io_vtable_abi_test.cpp:925-963 `TailReaderWriter` | `CHECK_EQ(flags, 1)` inside the override; the dtor is a stream operation | only if ReaderWriter flips: `~TailReaderWriter() override` carries the tail read/write; a no-op recording `static void operator delete(void*)` (the objects are array members, never heap blocks); assert "operator delete called with this" in place of `flags == 1`. |
| every `CheckBothArms` / `B39ExpectBothArms` / `B39CheckDeletingDtorArms` / `CheckBothSlot0Arms` scalar arm (the 40 `DeletingDtor(1)`) | `== p` and dtor count; the free is only proven by "the heap did not fault" | `delete p;` + dtor count; to keep proving the free, one shared recording hook (a unit-only counter in the `InternalMemFree` seam, internal_class.cpp:92-93 — does not exist today) or a recording class `operator delete` on the stand-in |
| **the 41 vector-arm sites** (`DeletingDtor(3)` ×39, `(2)` ×2 at rb_ui:291/310), 20 files | hand-built MSVC cookie at `block`, elements at `block+8`, return == block, N dtors, DOWNWARD order | Itanium has no vector deleting destructor; the header arms `if (flags & 2u) {…}` vanish with `DeletingDtor`. §3b Q3 must first prove no product caller passes `& 2` (in the tests these 41 are the ONLY callers found in this inventory). Proposal: ONE shared `CheckArrayDelete<T>()` — `T* a = new T[3]; delete[] a;` asserting the three dtors ran DOWNWARD and one block was freed — instantiated for every class that has an arms test today, so each class keeps a test and the file says in one line which retail address the MSVC arm was. **NOT VERIFIED:** that every such T is default-constructible (several are placement-constructed with arguments) and GCC's cookie position; if `new T[3]` does not compile for a class, that class's vector test is retired with a comment naming the retail arm, not silently. |
| video_imagebuffer_abi_test.cpp:296 `p->MirrorImageBuffer::DeletingDtor(9u)` | a qualified base call carries an argument | re-aim at a non-pure base member of the mirror; a destructor takes no argument |

## 3. Raw vtable reads (question 3)

### 3.1 Dispositions, suite-wide

| disposition | sites | where |
|---|---:|---|
| **SHIFT** (retail index N ≥ 1 of a flipping class → N + 1) | ~400 helper calls / index reads | UI 185 `SlotAt` + ~95 `vt[]`; GFX ~190 lines (geometry/particle/program/imagebuffer are mirrors that shift only if the mirror follows); IO 33 (+27 if ReaderWriter flips); object_external vt[1..7] (7), object_internal kSlotTypeName/Type/Persist (3), object_typeinfo vt[1], vt[2] |
| **DTOR-SLOT** (slot 0 called as "delete me") | **~40** | SlotAt(raw, 0): ui_dialog 3, ui_element 2, ui_leaf_c3 1, c4 2, ui_resources 1 (×6 classes), graphics_animation 1, material 1, geometry 1, particle 2, video_* mirrors 3 (+2 Q2), audio :147, io_streams :96, io_vtable :1185/:1356/:1673, pathfinding :97; `_ZTV[2](p, 1u)`: rb_graphics 268/280/301/332, rb_ui 354/421/437, rb_video 96, rb_achievement 218; object_external :190, object_typeinfo :464, object_vtable :172/:216 |
| **KEEP** (exempt table / mirror not flipped) | ~75 | every exe_class/mia/mib controller read (vt[0] FindState, [1] FindTransition, [3] Enter, [8] HasStates — `ControllerLive::TableController`), exe_class DEFINE_EXE_VTABLE probe reads :182-184, tool :222 / wrk :339 / achievement_goal :233/:245 / engine_loop PART C retslot probes (no dtor), system_abi_test RawSlot 0/1 (Thread), b39_sys_thread leaf[0]/[1], audio :314/:324 (Stream), audio :573 (PlatformChannel), rb_engine :322 (`Loader` slot 0 = Run), pathfinding :332 (IPathInfo), io_vtable TestReaderWriter/TestEncoding/TestSerializer/TestStream mirrors (~20), io_vtable EncOrderProbe (Encoding) |
| **KEEP (address point)** | ~25 | `VptrOf(x) == &g_…VtGroup[2]` in rb_engine/rb_ui/rb_graphics/rb_achievement: offset-to-top + RTTI still precede the address point, so `_ZTV…+16` is still the vptr value |

**Two traps in the DTOR-SLOT group:**
1. **Double destruction.** audio:147, io_streams:96, io_vtable:1185/1356/1673, pathfinding:97 call slot 0 on a
   STACK probe whose override records and does not destroy. After the flip, slot 0 is D1, which really destroys;
   scope exit then destroys again (double `~External`/`~Stream`). Rewrite: placement-new the probe in a buffer,
   call D1 once, never destroy again — or drop the raw call and pin `vt[0] != vt[1]` plus a `~Probe()` Note.
2. **Silent leak / garbage return.** The `_ZTV[2](p, 1u)` calls: after the flip `[2]` is D1 (`void(T*)`, no
   free, no return). Rewrite flags-1 calls as `[3]` typed `void(*)(void*)` (D0: deleting) and drop the
   return-value compare; the flags-0 call (rb_graphics:301) as `[2]` (D1). This pins the new order directly.

**Centralised slot constants** (one edit shifts a whole file): audio 74-82, pathfinding 60-68, graphics_animation
81-89, graphics_material 113-121, geometry 397-409, particle 307-318, io_streams 73-81, io_vtable `streamorder::`
1316-1323, ui_dialog 91-102, ui_element 111-122, ui_leaf_c1 117-126, c2 137-146, c3 114-118, ui_resources 74-82,
object_internal `kSlot*` :126. **Do NOT bump the constants by 8**: ui_leaf_c1/c2 compute the expected marker as
`off/8` from the offset they read while the marker bodies write the literal retail slot, so bumped constants fail
every own-slot check. Translate inside the helper instead — `SlotAt<Fn, T>(obj, retailByteOff)` applying
`RetailSlot<T>` — which also covers the index-taking helpers (c4:84, c5:89) and leaves markers in retail numbers.
Direct `vt[i]` loops and the "minted copy" tests (c1:648/869, c2:578, c4:643, ui_dialog:314, ui_element:435,
ui_resources:172, graphics_material:304, geometry:674, particle:438, audio:338) need the loop bound / array
length +1 and the patched index shifted; distinctness loops stay meaningful (D1 ≠ D0).

### 3.2 `tests/unit/object_vtable_abi_test.cpp`, test by test

TypeInfo.h:303 and External.h:588 name this file "the regression test for the no-virtual-destructor rule". Its
premise (hand-built/minted tables coexisting with compiler tables) is retired: B.3.9 left the mint with 2 storage
rows, and the `tornado.cpp` copy site it cites (line 285) is gone (code/Game/Game/tornado.cpp:31 "Gone: the vtable
COPY site"). It is rewritten, not deleted: it becomes the test of the NEW rule.

| test (line) | today | becomes |
|---|---|---|
| file banner + `static_assert(has_virtual_destructor<FakeBase> == false)` :93 | pins "no virtual dtor" | `FakeBase` declares `virtual ~FakeBase()`; assert `== true`. Banner restates the rule: retail slot 0 → Itanium D1+D0, retail N ≥ 1 → N + 1. |
| `base_raw_slot_dispatch_reaches_the_override` :167 | slot 0 (flags 3) / 1 / 2 / 3 / 4 reach FakeExternal's overrides | SHIFT: TypeName/Type/GetFileVersion/Slot4 read at 2/3/4/5; slot 0/1 become "D1 runs `~FakeExternal` without operator delete; D0 runs it and calls a recording class `operator delete`" on a placement-new'd object (not `ext` on the stack — trap 1). This is the direction-1 proof for the ~160 product raw `vt[N]` reads the other inventories convert. |
| `base_own_vtable_has_exactly_the_three_retail_slots` :202 | Base's table = 3 slots at 0,1,2; `FakeWithVirtualDtor` is the CONTROL proving the shift | Invert the roles: FakeBase's table is D1, D0, TypeName, Type (4 entries, TypeName at 2). Keep the control as `FakeRetailShape` (slot-0 `DeletingDtor(unsigned)`, no dtor) asserting TypeName at 1 — the retired MSVC-compatible layout, so the ONE-slot difference stays a tested fact. |
| `real_class_virtual_call_lands_in_a_hand_built_table` :244 | C++ virtual call on a hand-stamped `void*[3]` | Hand table becomes 4 entries `{hand::D1, hand::D0, hand::TypeName, hand::Type}` (`void(*)(void*)` for both dtors); `p->DeletingDtor(0u)` → a dispatching `p->~FakeBase()` lands on `hand::D1`. Still proves "the call path reads nothing before the address point". |
| `copied_compiler_vtable_is_interchangeable_slot_for_slot` :287 | copy 5 entries, patch [1] | copy 6, patch [2]. |
| `two_base_class_puts_the_second_vptr_at_0x28` :375 | secondary vptr Step at node[0], Enter at [1]; primary Startup at [1] | `FakeComponent` gets a virtual dtor (Component flips); `FakeControllerIface` stays dtor-less (the controller interface is EXEMPT): node[0]/[1] KEEP, primary `VptrOf(&obj)[1]` → `[2]` SHIFT. This test is the one place the exempt/flip boundary inside ONE object (primary flips, secondary does not) is proven — worth a sentence. |
| `embedded_member_shape_reaches_a_secondary_vptr_at_0x38` :440 | FakeEmbeddedNode already ends in `virtual ~` (slots 2,3), Step at [0] | KEEP. |
| `layout_invariants_hold_for_every_shape_in_this_file` :465 | sizes 8 / 0x20 / 0x28 / 0x40 | KEEP (a virtual dtor adds no bytes); add `has_virtual_destructor` on the three flipped fakes. |

## 4. The `InitFields` / `ReleaseFields` seams (question 4)

The seams are defined in the tests as RECORDING markers (vptr at call time): `Engine::UpdateLoop/LoaderLoop/
DialogLoopT/DialogLoaderLoopT` (rb_engine_abi_test.cpp:75-84), `UI::DialogInstance/Element/ElementDescription`
(rb_ui_abi_test.cpp:91-134), `Graphics::Particle/SunLight` (rb_graphics_abi_test.cpp:83-133), `Video::ImageBuffer`
(rb_video_abi_test.cpp:55-60), `System::Thread` (system_abi_stubs). The base destructors that call them are INLINE
in the headers (`~UpdateLoop() { ReleaseFields(); }` UpdateLoop.h:367, `~Element() { ReleaseFields(); }`
Element.h:505, …), so **every stand-in already runs the real base destructor chain today**: explicit
`l->~RbGameLoop()`, `DeletingDtor(0)` bodies (`this->~T()`), and scope exit all reach it.

- **No stand-in relies on skipping the base destructor.** The trivial `DeletingDtor(unsigned) { return this; }`
  overrides are never used to avoid destruction; each test destroys through an explicit `~Stand-in()` or scope exit.
- **The vptr-at-release assertions survive**: Itanium D1/D0 re-stamp the vptr per level exactly as MSVC's chain
  does, so `g_log[i].vptr == UlVt()` etc. stay true (NOT VERIFIED by running).
- **Product destructor bodies that touch engine state**: none is reached. Leaf `~X()` bodies are test-file markers
  (e.g. audio_abi_test.cpp:105 `SoundBuffer::~SoundBuffer() { NoteDtor(this); }`, the 12 `*Data` markers in
  b39_vid_data_test.cpp:88-101, the UI `B39NoteDtor` macros). The only REAL product destructors linked are
  `~External` / `~Internal` / `~Serializer` (§6), and `~Internal` needs `InternalFactory::Get()`, which the tests
  already supply (`FactoryScope`, object_internal_test.cpp:70-84; rb_achievement :40-48).
- The one behaviour that DOES change: the six stack probes whose slot 0 is called raw (§3.1 trap 1).

## 5. How tests instantiate product classes today (question 5)

| pattern | ~files | today | when product dtors become public virtual |
|---|---:|---|---|
| never instantiated (sizeof / offsetof / traits / PMF, unevaluated `decltype(::new …)`) | 17 (the 14 exe-class files, engine_loop, b39_vid_data traits half, video_device/context) | — | compile-time only: `&X::DeletingDtor` lines fail, trait lines flip |
| stack stand-in (`final` subclass, implicit public dtor calling the accessible protected base dtor) | ~15 | scope exit runs the chain | unchanged, except trap 1 |
| placement `::new (buf) T(...)` in a stack buffer + `DeletingDtor(0)` or explicit `~Stand-in()` | ~25 | explicit destroy, no free | `p->~T()`; a QUALIFIED `p->Base::DeletingDtor(0)` (rb_engine 270/279/287, rb_ui 280/299) becomes `p->Base::~Base()`, which needs the dtor accessible from the test → depends on §3b Q3 (public) |
| heap `InternalMemAlloc` / `TypeTreeAlloc` / `std::malloc` + `DeletingDtor(1)` or `Object::Destroy` | ~22 | slot 0 frees via the unit malloc/free seam | `delete p` → class `operator delete` if declared, else `::operator delete` (= `free`, compatible with the unit malloc seams) |
| hand-built MSVC array cookie + `DeletingDtor(3)` | 20 | the inline vector arm | §2 vector row |
| `Object::Destroy(&stackObject)` | object_typeinfo :445 | slot 0 override records, frees nothing | `delete &stack` would be UB → heap or a no-op recording class `operator delete` |

**What breaks at LINK (NOT VERIFIED by building; follows from Itanium):** a virtual destructor puts D0 in every
vtable, and D0 calls the class-scope `operator delete`. The C++ members `X::operator delete` are DECLARED in the
headers (20 UI classes; `SoundBuffer` SoundBuffer.h:143, `SoundEffect`, `PointList`, `PickingMesh`,
`Achievement`, `AchievementInstance`, `Goal`, …) but defined as C++ members nowhere in the repo except
`Video::RenderBuffer::operator delete` (runtime/src/video/context_lifecycle.cpp:315): the 88 retail bodies are
registered BY EXPORT NAME to one free function (`RtClass_OperatorDelete`, runtime/src/object/class_operator_delete.cpp:52;
runtime/src/ui/ui_operator_delete.cpp). Every test TU that emits such a class's vtable (the marker-body files:
audio, pathfinding, rb_achievement, ui_dialog, ui_resources, ui_leaf_c3, rb_ui, …) will fail with undefined
references — and so will the product. This is §3b Q3's work; for the unit build the fix belongs in a registry-free,
linkable TU with a `BANISHED_UNIT_TEST` malloc branch (the existing seam pattern of internal_class.cpp:92), added
to `tests/unit/sources.mk`, which is also the natural place for the recording hook §2 needs.

## 6. The unit build (question 6)

`Makefile:120-141`:

```make
include tests/unit/sources.mk
UNIT_BUILD_DIR = tests/unit/build
UNIT_SRCS = $(wildcard tests/unit/*.cpp) $(UNIT_TREE_SRCS)
UNIT_CXXFLAGS = -std=c++17 -fno-strict-aliasing -fno-lifetime-dse -Wall -Wextra -Wno-invalid-offsetof -g -O0 -D_DEBUG -DBANISHED_UNIT_TEST -MMD -MP
unit: $(UNIT_BUILD_DIR)/unit.exe
	@$(UNIT_BUILD_DIR)/unit.exe
$(UNIT_BUILD_DIR)/unit.exe: $(UNIT_OBJECTS)
	$(CXX) $(UNIT_OBJECTS) -o $@ -static-libgcc -static-libstdc++
```

One exe: every `tests/unit/*.cpp` plus the 28 tree sources `tests/unit/sources.mk` lists (math, crt libm/qsort,
string, filename, array, array_destroy, alloc_door, linkedlist, introsort, type_reflection, external_class,
internal_class, serializer_class, crc, and the eight `*_type_records.cpp`). No `code/Game` source. No test
`#include`s a `.cpp`.

Product code that defines or calls `DeletingDtor` and IS compiled into unit.exe:
- **linked tree sources:** `Object::Base::DeletingDtor` both arms (runtime/src/object/type_reflection.cpp:118,
  frees via `TypeTreeFree`); `Object::External::DeletingDtor` + `~External` (external_class.cpp:93,120,199);
  `Object::Internal::DeletingDtor` + `~Internal` (internal_class.cpp:387,408); `MemoryStreamDtor` calling
  `IO::MemoryStream::DeletingDtor(0)` (internal_class.cpp:168 — product `#else` arm, not the unit arm);
  `~Serializer` raw-calling ReaderWriter slot 0 with 1 (serializer_class.cpp:171-177).
- **header-inline bodies** (448 inline `DeletingDtor(...) {` definitions across runtime/types, e.g.
  UpdateLoop.h:319/432, SoundBuffer.h, Geometry.h, FileStream.h, every UI leaf header): compiled into whichever
  test TU includes them. These disappear with the cutover; the tests that call them are the §2 rows.
- `-fno-lifetime-dse` is already on UNIT_CXXFLAGS (the dtor-null-store lesson applies to D1/D0 as well).

## 7. Estimate, order and open items (question 7)

**Files:** 52 of 82 test files touch the machinery; **50 need edits** (b39_sys_thread_test.cpp and
system_abi_test.cpp are Thread-only and KEEP, at most a comment). Plus a new `slot_check.h`, the arms helper
(in b39_uib_test_util.h or a new shared header), and one `sources.mk` row for the operator-delete TU.

**Mechanical (script):**
- PMF: 1,064 non-dtor lines → `CHECK_SLOT(T, M, retailIndex)` (regex over the byte-literal and `S()` forms);
  128 `&X::DeletingDtor` lines → `CHECK_DTOR_SLOT(T)`. Landable before the cutover (§1.2).
- Traits: ~180 `!has_virtual_destructor` and ~90 `!is_destructible` lines on flipping classes → inverted
  (`is_destructible` only if §3b Q3 makes dtors public), skipping the ~10 exempt-class lines by name.
- 116 `DeletingDtor(0)` → `p->~T()` (drop `== p`, keep counts/logs); ~70 trivial `return this;` overrides deleted;
  ~45 recording overrides → `~Probe() override { Note(...); }`.
- `SlotAt`-style helpers: ~20 helper bodies edited to translate retail → compiler index; call sites unchanged.

**Hand pass (~60 sites):** the 41 vector-arm sites (via one helper), the 40 `DeletingDtor(1)` free assertions
(via one recorder), ~40 DTOR-SLOT reads incl. the 9 `_ZTV[2]` calls and the 6 stack-probe double-destruction
traps, object_vtable_abi_test (§3.2), object_internal / object_typeinfo / object_external Destroy tests,
TailReaderWriter, video_imagebuffer:296, k1's `bad +=` form, the 11 minted-copy / loop-bound tests, and the
~5 mirror hierarchies re-declared with `virtual ~`.

**Order:** (1) land `slot_check.h` + the `SlotAt` translation as a no-op refactor, gated by `make unit` alone;
(2) at the cutover, the per-hierarchy commit flips `CHECK_DTOR_SLOT`, inverts that hierarchy's traits and
rewrites its §2/§3 hand sites in the same commit (a hierarchy's tests cannot pass half-flipped).

**Open items for the B3X_RULES owner:**
1. `IO::ReaderWriter`: flip or exempt — decides 13 PMF lines, ~27 io_vtable raw reads, TailReaderWriter,
   `~Serializer`'s raw slot-0 call (linked into unit.exe), and the product's `rw_vtbl` users.
2. §3b Q2 (`Video::*Data`): decides all of b39_vid_data_test.cpp and 16 video_data_abi_test.cpp reads.
3. §3b Q3: public vs protected destructors (decides ~90 `is_destructible` lines and the qualified base-dtor
   calls), the vector arm's fate (41 test sites), and the class `operator delete` definitions (a link blocker for
   the product and unit.exe alike).
4. `UI::Font`: ui_resources_abi_test.cpp treats Font's slot 0 as an ordinary deleting dtor (probe :344, both
   arms, PMF 0x00); §3a lists "UI::Font's factory-flag slot" (font.cpp:410) as exempt. NOT VERIFIED that these are
   different tables — the font.cpp owner should confirm before Font is put on either list.
5. Mirrors in achievement_goal (`ProbeGoal`, cites `GoalList::Initialize`'s `CALL [RAX+0x48]`): after the flip
   Goal's `GetInstanceType` is compiler slot 10 (0x50); the probe still proves the calling convention, but the
   product raw read it cites belongs to the product inventory.
