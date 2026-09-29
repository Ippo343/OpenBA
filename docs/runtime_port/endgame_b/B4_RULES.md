# B.4 rule sheet — the cast metric (written 2026-09-28 from block 0)

Step plan: `NEXT_SESSION.md` §3 (as written at `b2a5434c`). Inventories, tables and scripts:
`b4_session/INVENTORY_{A..F}_*.md`, `b4_session/data/`, `b4_session/scripts/`. Kit: `b4_session/`.
The inventories are maps, not authorities: a worker re-derives every class, slot, offset and width
from retail before it edits.

## 1. What block 0 measured (base `b2a5434c`)

| | |
|---|---:|
| **THE METRIC** `python docs/runtime_port/endgame_b/b4_session/castmetric.py code runtime standalone dx11 engine` | **46,733** |
| … `reinterpret_cast` 28,913 · C-style pointer / `(uintptr_t)p` 14,405 · `static_cast` from `void*` 1,945 · offset-helper calls 958 · `memcpy` of a raw address 256 · `const_cast` 191 · `static_cast<uintptr_t>` base 58 · functional 5 · `void*` chain 2 | |
| `tests/` (own row) | 813 |
| bridge markers (own column, not summed) | 6,849 |
| raw offset accesses (C) | 22,578 on 21,253 lines; receiver class known for 35% |
| C bodies in `code/Game` (A) | 879 declarations in 65 headers, 249 defining files; 131 duplicate a DEFINED member; 71 unreferenced; 152 template instances; 166 with no class |
| C-shaped bodies in `runtime` / `dx11` / `standalone` (B) | 1,938: 649 doors, 515 real, 710 UNREFERENCED |
| members declared and never defined (B) | 771 (+30 whose only body is a C function) |
| the floor (E) | tier R 278 genuine reinterpretations in 74 files; tier I 839 idioms; 27 dx11 seam casts deferred |

Corrections to `NEXT_SESSION.md` §3a / §0 (the read wins): nine `exe_bodies.h`, not seven; §0.3's 39
unreferenced runtime bodies are 710; `-Wold-style-cast` is NOT exact (silent in `extern "C"`);
C-style casts in `code/Game` are 14,233; GameUpdateLoop's header already names every offset read (its
members are `void*`); "byte-level serializer reads" and "`Array<T>` storage" are almost empty floor
categories; dx11 has 10 floor casts of 151.

## 2. The metric and the floor (M)

- **M1.** The one number is `castmetric.py`'s total over PROD. A block hands in
  `castmetric.py <its files> --by-file --tsv` at its base and at its tip. **The total falls and NO
  column rises** in any file. A fall in one spelling with a rise in another is a respelling and is
  rejected. Bridge markers fall or stay.
- **M2. Tier R (stays, marked).** A genuine reinterpretation carries a trailing
  `// CAST-OK: <category>` on its line. Categories (E §1): `alloc_header`, `ptr_int`, `crash_walk`,
  `win32`, `float_bits`, `string_header`, `pe_walk`, `byte_io`, `elem_stride`, `gpu_map`, `com_iid`,
  `crt`, `hash`, `vslot`, and `dx11_seam` (deferred to §4.1 of the plan). A marked line with an
  operand `+ 0x…` field offset or a `self` / `this` receiver is NOT legitimate: the marker is for
  what the cast DOES. New categories need the coordinator.
- **M3. Tier I (stays, by shape, unmarked).** `castmetric.py` exempts exactly three shapes:
  placement `new (static_cast<void*>(x))`; a `void*` cast that is an argument of a printf-family /
  TRACE call (`%p`); `static_cast<T*>(MemAlloc…(…))`. Nothing else is exempt by shape.
- **M4.** There is no file allowlist. `dx11/` is judged by category like every directory.
- **M5. A cast is removed by naming the thing.** Not by a C-style cast, a `static_cast` through
  `void*` / `uintptr_t`, a `memcpy`, a union, an offset helper (`At<T>(p, off)`, `Field<>`, `P64`,
  `Rd`, `ArrayAt` …) or a new helper of any name. File-local offset helpers die in the block that
  owns their file; header helpers (`widget_common.h`, `coll_bridge.h`) die with their last caller.

## 3. Headers (H) — W0

- **H1.** A field a body reads is DECLARED with retail's type and `static_assert(offsetof)`ed;
  `sizeof` is asserted. W0 edits headers only; no body changes; the compiler proves nothing moved.
- **H2. The type is retail's**, from the instructions that read and write it (MOVSS = float, MOVSX /
  MOVZX and the compare's condition code = signedness, the width of the MOV), the serializer overload
  retail calls, and the ctor. Not from the tree's ctor stores and not from the tree's reads: C found
  650 rows where header and read disagree and 117 class+offset pairs read at different widths.
- **H3. Names.** In order: the serializer's / persistence key's string; the profile build's assertion
  text; the resource path the field is loaded from; the accessor's mangled export name; behaviour
  (what retail does with it). A field with none of these keeps a TYPED guess name
  (`field_0x48`), which is legal and counts as named for the metric; never invent a meaning.
  Existing names that retail contradicts (D §5: citizen +0x250/+0x258, +0x1E8) are corrected.
- **H4. Pointer members are typed**: `void* m_x` becomes `T* m_x` with a forward declaration as soon
  as `T` has a header. Layout-neutral.
- **H5. Members with a non-trivial type** (`System::String`, `Collection::Array<T>`,
  `InternalPtr<T>` …) are declared with the real type ONLY if the tree's ctor / dtor of the class
  then does exactly what retail's does. Where declaring it makes a ctor or dtor run that did not run
  before, that is a `behaviour:` commit of its own (gated with resave x5), not a W0 header commit;
  until then the field is declared as raw storage of the right size with a comment naming the real
  type, and its reads are LEFT and listed.
- **H6. Embedded members are fields of the embedded class.** A read of `obj + 0x1234` that lands
  inside a member embedded at `+0x1200` is `obj->member.field`, and the embedded class owns the
  header. Byte-storage members (`uint8_t meshInstance[0x120]`) are split or typed by the block that
  owns the class.
- **H7. A merged store stays two stores.** Where retail zeroes two adjacent fields with one qword
  store, the tree writes the two NAMED fields, in address order. Value-identical; not a behaviour
  commit.
- **H8. A width or signedness the tree has and retail contradicts** is a `behaviour:` commit with
  the retail address of the instruction. A width the tree has at SOME sites and not others: the
  field gets retail's type, every site whose observable result changes is listed in the commit.
- **H9.** One owner per header. The twelve cross-module headers and the two new structs are W0-H's
  and are finished before any W1 block starts: GameUpdateLoop, CitizenComponent, CitizenDescription,
  ModelComponent, CitizenBehavior, EntityList, StorageComponent, BuildComponent, WorkPlaceComponent,
  EntityUI, Profession, CharacterComponent; `GameSettings` (0x34, cell 0x1401c3de0), `GameContainer`
  (0xE0, cell 0x1401c3de8). A W1 block that needs a field in another block's header asks the
  coordinator; it does not edit the header.
- **H10.** `runtime/types` POD mirrors (`Rt<Ns>::X` beside `<Ns>::X`, 67) and file-local shadow
  structs with retail names (`struct Serializer`, `struct Stream`, 45) are deleted by the block that
  owns the real class, once no code line names them.

## 4. Bodies (B) — W1

- **B1. The class and the member come from retail**: the vtable that holds the address (slot →
  member name of that slot in the class's header), the mangled export (Runtime-x64.dll), the ctor /
  dtor shape. NEVER from the C body's name (45 name the wrong slot, 5 the wrong class) and not from
  an old Ghidra label. Retail cannot distinguish a non-virtual member from a free function taking
  the object first: there, the body becomes a member of the class whose FIELDS it reads through its
  first argument; a body that reads no field of its first argument stays free and is listed.
- **B2. The conversion of one body:** declare the member in the class header with its retail
  address in the trailing comment; move the body; the receiver is `this`; every field read is by
  name; every caller changes from `Body(p, …)` to `p->Member(…)` (callers are found by BINDING:
  the name, a function pointer taken, a table initialiser, a test); the C declaration leaves
  `exe_bodies.h` / the `*_funcs.h` header in the same commit. No forwarding wrapper is left behind.
- **B3. A duplicate of a DEFINED member** (A: 131) is compared with the member before it is
  deleted: statement by statement, widths and guards included. Identical → callers are rewired,
  the copy is deleted. Different → read retail; the member gets retail's behaviour in a
  `behaviour:` commit, then the copy is deleted.
- **B4. Template instances** (A: 152; B: 199) become calls of the template's member
  (`Collection::Array<T>::SetSize`, `Set<T>::Find`, `ExternalPtr<T>::Serialize`,
  `ComponentEntity::FindComponent<T>()`), not new free functions. The element type must be a named
  type first.
- **B5. Split bodies stay split.** One retail function ported as several phase bodies
  (`MenuDialogLoopT::Load` in five) becomes several PRIVATE members or stays file-local functions
  taking a typed reference; the pieces are not merged.
- **B6. Signatures are retail's.** Parameters get their real types (an `int` handed to a `void*`
  parameter is fixed at the declaration). Ghidra drops arguments on indirect calls and
  register-passthrough forwarders: read the disassembly. A by-value virtual is `T* M(T* ret, …)`.
  Where retail leaves a register unset the site is LEFT and listed.
- **B7. Read the whole body before moving or deleting it.** Counters, latches, prints, `atexit`
  hooks and seam installs move with it or are named in the commit that drops them.
- **B8. Unreferenced bodies** (A: 71; B: 710) are deleted after B7's read, in blocks of their own
  (R0), with their declarations. A body that is the only port of a retail function B.4 needs as a
  member is NOT deleted: it becomes that member.
- **B9. No behaviour change** outside a commit whose title starts `behaviour:`. Null-receiver reads
  retail performs stay reads (`work_act_consume.cpp:268`, `:302`). No null check is added; guards
  the tree has and retail lacks are listed, not extended (their removal is NEXT_SESSION §0.2's).
- **B10. Downcasts** after a type check are `static_cast` inside the object's own hierarchy; the
  five roots are different hierarchies. A component found by type id is returned TYPED by
  `FindComponent<T>()`.
- **B11. Proof per block:** the debug object files of the block's translation units are compared
  function by function with main's (disassembly normalised for symbol names); a function that
  differs is explained in the hand-in (a member call in place of a C call, a merged store split) or
  it is a defect.
- **B12.** Two bodies are decided here, not converted mechanically: `RtSystem_CriticalSection_Ctor`
  (exported `??0CriticalSection@System@@`: becomes the constructor once its body is read against
  retail's; if the tree's body does more than retail's ctor, the extra stays a named function) and
  `Video_Device_Dtor` (stays a door until the dx11 seam, plan §4.1; listed).
- **B13. ICF.** One retail address can serve several classes (identical-code folding). A body is
  attributed per CALL SITE's class; one shared tree body serving several classes stays one template
  or one base member, never N copies.

## 5. Waves and blocks

Blocks own CLASSES' headers plus the files that define their members; file sets are disjoint
(`b4_session/B4_FILEMAP.md`, generated from C's owner column and A / B's defining files).

| wave | blocks | gate |
|---|---|---|
| **W0** | `R0-RT` dead runtime bodies · `R0-EXE` dead exe bodies + the four grab-bag files split (`ui_panels.h`, `action_pf_ctors.cpp`, `action_child_ctors*.cpp`) · `E0` tier-R markers in the 13 unowned floor files, tier-I shapes in `castmetric.py`, `acceptance.sh` · `W0-H1` / `W0-H2` the twelve headers + the two structs · `W0-RH` runtime headers code/Game reads, `FileInfo::winHandle`, dx11 COM field types | full gate each |
| **W1** | exe: `T` templates + `ComponentEntity::FindComponent<T>` (FIRST, alone) → `CIT` `TRADE` `DLG-a/b/c` `COMP-a/b/c/d` `WORK` `ACT` `GAME` `PLAYER`; runtime: `R-UI` `R-GFX` `R-VIDEO` `R-IO` `R-ENG` `R-CORE`; `U` the unattributed families get a class first (Ghidra), then join a block | full gate each; `uiroutes.sh` on Dialog / UI; `toolroutes.sh` on Player / Action; `devlost.sh` on Video / Graphics / dx11 |
| **W2** | `STANDALONE` (the four game-logic files move to `code/Game/Game/`; harnesses follow their class or are retired) · `TESTS` (the ~200 casts that fall) | full gate |
| **AUDIT** | independent, read-only, BEFORE the C declarations' headers are deleted | — |
| **CLOSE** | `exe_bodies.h` hold only what has no class; numbers; `stepclose.sh` | step close |

Runs at most ~5 agents at once, agents build at `-j3`, the gate at `-j4`; agents never run the game.

## 6. Acceptance (`b4_session/acceptance.sh`)

1. `castmetric.py` PROD total == the number of `CAST-OK` markers (tier R, each category at or under
   its table number) + 0: every unmarked, non-tier-I cast is gone.
2. No offset-helper definition or call remains (`accessor` column 0); `pun` column 0 outside
   `CAST-OK: byte_io` / `float_bits` lines.
3. Bridge markers 0 outside `CAST-OK: dx11_seam` lines.
4. Every declaration left in a `code/Game/<Module>/exe_bodies.h` / `*_funcs.h` is listed by name
   with its reason in `b4_session/LEFT_FREE.md`; `void*` first parameters in `code/Game` are only on
   those.
5. Every class header asserts every field; no `Rt<Ns>::` POD mirror of a real class remains.
6. The door acceptance of the teardown (`b3r_session/acceptance.sh`) stays at target.
7. The gate passes; `stepclose.sh`.

## 7. Run log

(appended by the coordinator per merge: `b4_session/GATE_TABLE.md`)

## 8. Corrections

(appended as the code contradicts this sheet)

1. **M1 and moves (R0-EXE).** Moving a body between files raises the destination file's columns by
   what left the source. A MOVE is judged on the sum over the source and destination files and is
   handed in as a list of (body, from, to); everything else is judged per file.
2. **A3 duplicates are fewer than A counted (R0-EXE).** Where the "defined member" is an empty
   placeholder, the C body is the real port: it BECOMES the member (B2), it is not deleted (B3).
   Known: DecalComponent_Serialize 0x14011b7d0, HerdComponent_OnRemove 0x140121e20,
   MultiObjectComponent_OnCreate 0x1401257c0, NaturalResourceComponent_OnRemove 0x140125c40,
   RawMaterialComponent_Update 0x140129130, StateMachineComponent_OnUpdate2 0x14012b7d0,
   StatusIconComponent_OnUpdate2 0x14012bf40, WorkComponent_Draw 0x14012e6f0, MainMenuDialog_Slot5
   0x1400b3c60, EntityUI_Slot8 0x140066ad0, NoiseUI_Slot4 / _Slot9, SpriteCaptureUI_Serialize /
   _OnCreate. Eight more DIFFER from their member and need retail read first: MapComponent_Draw,
   StateMachineComponent_Serialize, TombstoneComponent_Serialize, MainMenuDialog_Slot4 / _Slot8,
   ResourceLimitUI_Slot4 / _Slot7 / _Slot9.
3. **Citizen home and workplace (W0-H1; D had them backwards).** CitizenComponent +0x238 is the
   HOME (+0x240 the household slot), +0x248 the WORKPLACE. The body the tree and Ghidra call
   `AssignWorkplace` (0x1401163d0) assigns the home; `AssignHousing` (0x140111ee0) assigns the
   workplace. W1-CIT names the members from what retail does.
4. **EntityList is 0xE8** (the shutdown flag at +0xE0). Its `updateTreeCount` +0x70 and
   `postUpdateTreeCount` +0x80 are int32 in retail (0x1400c3fe8 / 0x1400c3ff7), int64 in the tree:
   an H8 `behaviour:` commit, owed.
5. **GameUpdateLoop +0x120 is the tool manager** (`m_toolManager`, a 0xB0 object without a vtable),
   not an EntityUI; +0x130 is not the Weather resource and +0x118 not the Tutorial resource (W0-H2).
   Members still `void*` and why: W0-H2's hand-in, copied into `b4_session/W0_LEFT.md`.
6. **Settings flags are `uint8_t`, not `bool`** (W0-H2): retail copies the checkbox byte raw
   (0x14007248f); a `bool` field would let the compiler drop the tree's `!= 0` in Save. Retail's
   Save passes the raw byte (0x1400989d7): an H8 `behaviour:` commit, owed.
7. **H5 raw-storage members owed as `behaviour:` commits** (each makes a ctor / dtor run):
   CitizenComponent `name` +0x28 (String), `consumedTypes` +0x98 (Array), `meshInstance` +0xB8;
   Profession's three Strings; ModelComponent `meshInstance` / `transform`; StorageComponent's item
   array (retail's dtor frees it, the tree's does not); the Build / WorkPlace arrays; Map's picking
   scene; PlayerController's transform and camera.
8. **Inventory tables mis-tag receivers** (W0-H1, W0-H2): "Profession" rows at +0x20/+0x28 in the
   dialogs are ProfessionList; "EntityList" +0x30/+0x38 are ComponentEntity; "StorageComponent"
   +0x50/+0x58 and "EntityUI" past +0x48 are other classes. B1 stands: the receiver is re-derived.
9. **Scripts run from a worktree** must not exclude their own path: `scripts/E_floor_and_tests.py`
   matches `.claude/worktrees` against the absolute path and writes an empty table there.
