# B38_SLOTS.md — raw appendix for B.3 step 8 (the exe-phase classes of `Application-x64.exe`)

Produced 2026-09-26 by block-0 agent 0C from the RETAIL `Application-x64.exe` ONLY (asserted inside the script and before each
batch: 11473 functions, image base 0x140000000; the shared bridge was switched back to `Runtime-x64.dll` afterwards) by
`b38_session/ghidra/Step8Appendix.java` (EXE mode only; read-only, no rename, no annotation), run through GhidraMCP. The complete
console output is `b38_session/step8_appendix_raw.txt` (sections R, T, C, S, S2, A, P); this file is the same data condensed and
post-processed (orientation-aware parents, names, the controller family). Copy from here; do not retype. Rules and verdicts belong
in `B38_RULES.md`.

**Three facts that change the step-8 plan (read these first):**
1. **The exe has NO MSVC RTTI for game classes.** The only `.?AV` type descriptors are `std::bad_exception`, `std::exception` and
   `type_info` (0x1401c25d0..); the Complete Object Locators at 0x14017d780.. are the CRT's. Class names come from the exe's own
   reflection (`Object::BaseTypeInfo` registrations) and each table's `Type()` slot, not from RTTI.
2. **There is NO multiple inheritance in the exe.** No table holds a this-adjusting thunk (`SUB RCX,imm; JMP`: zero found; the
   8 `ADD RCX,+imm; JMP` bodies in Work tables are member forwarders). The "second vptr" of the ControllerComponent family is an
   **EMBEDDED MEMBER object**, not a secondary base: in every ctor it is stamped AFTER the derived class's primary vptr (member
   construction order; a base subobject is stamped before it); its offset VARIES per class (+0x28, +0x30, +0x38, +0x48, +0x58,
   +0x78, +0x88 — a second base would sit at the same offset in all of them); and `ControllerComponent` slot 17 (pure) is
   overridden in each class by `LEA RAX,[RCX+off]; RET` ("return &m_controller"). §2. ENDGAME B.1 decision 2 (`class X : public
   Component, public ControllerComponent`, GCC-emitted thunks) does not match retail. The retail shape is `class X : public
   ControllerComponent { ...; XController m_controller; }` with `XController` deriving from a 9-slot pure interface.
3. **The family is 29 classes, not 56**: 23 with parent literal `ControllerComponent` plus the 6 `ResourceProduceComponent`
   subclasses. `StateMachineComponent` (parent `Component`) embeds the same kind of member. There are 24 distinct member tables.

How each fact was read:
- tables: every `.rdata` address MATERIALISED BY A `LEA` in `.text` whose first qword points into `.text`. The first run also
  took Ghidra's constant-propagated refs on `CALL [RAX+0x48]`, which split four tables; the LEA-only rule fixed that (365 -> 361).
  Slots run to the next table start, a zero qword, or a non-code qword (the class-name literals follow the tables);
- slot classes: raw bytes decoded first. `IMPORT X` = an `FF 25` stub into Runtime-x64.dll (the class inherits the Runtime
  virtual X); `_purecall` = 0x14003addc (the exe's own); `TINY` = RET / `RET 0` / return 0/1/N / `returns literal` /
  `returns dword [ti] = TypeId(X)` / `returns qword [ti+8]` (TypeName) / `returns this+N`; `OWN` = an exe body with its Ghidra
  size; `JMP`/`THUNK` = a tail jump. `[in N tables]` = the same target in N tables; `= parent` = identical to the structural
  parent's slot at that index;
- class name: slot 2 (`Type()`) returns `dword [ti]` of exactly one `BaseTypeInfo` global; the 310 reflected names map 1:1 onto
  310 tables (no two tables share a TypeId). Unreflected tables: `[Runtime NS::X table]` = an exe-emitted table of a Runtime
  class whose ctor the exe inlined (named from its imports); `{...}` = named here from structure or B37_SLOTS §E1/§E3;
  `(label)` = a Ghidra label from earlier sessions (NOT attested). **Ghidra's function labels are unreliable**:
  `AttendanceComponent_Ctor` 0x140044510 and `Action_Action_Ctor7` 0x1400c6970 are DESTRUCTORS (they stamp derived, then base);
- reflection: every call of `??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z` (IAT 0x140152ae0), with
  the arguments forward-simulated: RCX = the type-info global (+0 type id, +8 name pointer), RDX = name, R8 = parent name,
  R9 = third literal (a file extension; only `NameDatabase` has one, `"ndb"`), `[RSP+0x20]` = create, `[RSP+0x28]` =
  create-in-place;
- sizeof: the `MemAlloc` size in the create function (`?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`, IAT 0x140152b60;
  `MOV EDX,8; LEA ECX,[RDX+imm]` constant-tracked). `abstract` = the create function is 0x140003730 (`XOR EAX,EAX; RET`).
  `alloc X` on an unreflected table = a `MemAlloc(X)` site whose next call is a ctor that stamps that table last at this+0
  (heuristic; only for unreflected tables);
- parent (structural): each stamping function is ORIENTED first (a call of a Runtime `??0` import at `this` = ctor, a `??1`
  import or `MemFree` = dtor, else inherited from its callee; a second pass orients the rest by the first pass's edges). The
  parent is then the this+0 stamp just before (ctor) / after (dtor) the class's own stamp, or the table the called base
  ctor/dtor stamps. **All 310 reflected parent literals agree with the structural parent** (§0a is empty); the only
  differences are spelling quirks in the literals (§3a);
- member vptrs: every table stored at this+N (N != 0) in a function that also stamps this+0 (raw SECTION S).


## §0 — index: every table the exe stamps or references (361 tables; 310 reflected classes)

`class` = the name the table's own `Type()` slot returns (slot 2: `MOV EAX,[ti]` of the class's `BaseTypeInfo` global,
so it IS the reflected name), else `(label)` = a Ghidra label from earlier sessions (NOT attested). `sizeof` = the
`MemAlloc` size in the reflected create function, else `alloc X` = every `MemAlloc(X)` site whose next call is a ctor
that stamps this table last at this+0, else `?`. `parent (literal)` = the `BaseTypeInfo` parent string EXACTLY;
`parent (structural)` = the table stamped at this+0 just before this one in a ctor (inline base) or stamped last by
the base ctor the ctor calls first (majority over every ctor). `member vptrs` = tables stamped at this+N (N != 0) in
the ctor/dtor of this class (see §2: they are EMBEDDED members, not secondary bases). `mix` = own/tiny/import/pure/jmp.

| class | table | slots | sizeof | parent (literal) | parent (structural) | member vptrs | reflected | mix |
|---|---|---|---|---|---|---|---|---|
| [Runtime Achievement::AchievementInstance table] | 0x14015e350 | 4 | ? | - | - | - | N | 1/0/3/0/0 |
| [Runtime Graphics::EdgeMesh table] | 0x140159a60 | 9 | ? | - | - | - | N | 1/0/8/0/0 |
| [Runtime Graphics::FrameCopyImageBuffer table] | 0x14015e370 | 12 | ? | - | - | - | N | 1/0/11/0/0 |
| [Runtime Graphics::MaterialInstance table] | 0x14015c400 | 9 | ? | - | - | - | N | 1/0/8/0/0 |
| [Runtime Graphics::MeshInstance table] | 0x14015e240 | 9 | ? | - | - | - | N | 1/0/8/0/0 |
| [Runtime Graphics::ParticleSystemInstance table] | 0x14015ecb0 | 10 | ? | - | - | - | N | 1/0/9/0/0 |
| [Runtime Graphics::SunLight table] | 0x14015e290 | 11 | ? | - | - | - | N | 1/0/10/0/0 |
| [Runtime UI::DialogInstance table] | 0x14015f5a0 | 10 | ? | - | - | - | N | 2/0/8/0/0 |
| [Runtime UI::SpriteCapture table] | 0x14015d550 | 9 | ? | - | - | - | N | 1/0/8/0/0 |
| [Runtime UI::SpriteSheet table] | 0x14015c3b0 | 9 | ? | - | - | - | N | 1/0/8/0/0 |
| [Runtime Video::ImageBuffer table] | 0x14015c8c0 | 10 | ? | - | - | - | N | 1/0/9/0/0 |
| AbsorbResourceComponent | 0x140162000 | 18 | 0x48 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x14015f070, +0x28=0x140161fb0 | Y | 2/16/0/0/0 |
| AbsorbResourceDescription | 0x140162090 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| AchievementDialog | 0x14015b020 | 10 | 0xd0 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 4/3/3/0/0 |
| Action | 0x14015faa0 | 10 | abstract | `Object::Internal` | Object::Internal (Runtime ctor import) | - | Y | 2/7/0/1/0 |
| AmbientEmitterComponent | 0x140158a60 | 16 | 0x48 | `Component` | Component 0x140162920 | - | Y | 3/12/0/0/1 |
| AmbientEmitterDescription | 0x140158ae0 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| AmbientEnvironmentComponent | 0x140158b70 | 16 | 0x48 | `Component` | Component 0x140162920 | - | Y | 2/13/0/0/1 |
| AmbientEnvironmentDescription | 0x140158bf0 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| AnimalComponent | 0x140162110 | 16 | 0x58 | `Component` | Component 0x140162920 | - | Y | 6/10/0/0/0 |
| AnimalDescription | 0x140162190 | 10 | 0x58 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| AnimateNaturalResource | 0x14015fb10 | 10 | 0x30 | `Action` | Action 0x14015faa0 | - | Y | 5/5/0/0/0 |
| AnimAudioEvent | 0x14015e0c0 | 10 | 0x30 | `Graphics::AnimationEvent` | Graphics::AnimationEvent (Runtime ctor import) | - | Y | 6/3/1/0/0 |
| AttendanceComponent | 0x140158ce0 | 18 | 0x58 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x38=0x140158c90, +0x38=0x14015f070 | Y | 6/12/0/0/0 |
| AttendanceDescription | 0x140158d70 | 10 | 0x28 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| AttendanceUI | 0x14015b070 | 11 | 0x38 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 3/2/6/0/0 |
| AutoPickupComponent | 0x140158df0 | 16 | 0x40 | `Component` | Component 0x140162920 | - | Y | 5/11/0/0/0 |
| AutoPickupDescription | 0x140158e70 | 10 | 0x28 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| AutoPickupWork | 0x140160fa0 | 13 | abstract | `Work` | Work 0x140161a80 | - | Y | 3/4/0/6/0 |
| AutoSaveDialog | 0x14015b120 | 10 | 0xd0 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 4/2/3/0/1 |
| BoatComponent | 0x140158f40 | 18 | 0x98 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x78=0x140158ef0, +0x78=0x14015f070 | Y | 2/16/0/0/0 |
| BoatDescription | 0x140158fd0 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| BridgeTool | 0x140161b00 | 15 | 0x40 | `DragTool` | DragTool 0x140161c10 | - | Y | 8/6/1/0/0 |
| BuildComponent | 0x140162260 | 18 | 0x78 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x14015f070, +0x28=0x140162210 | Y | 5/13/0/0/0 |
| BuildDescription | 0x1401622f0 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| BuildingGoal | 0x140160300 | 11 | 0x48 | `Achievement::Goal` | Achievement::Goal (Runtime ctor import) | - | Y | 6/3/2/0/0 |
| BuildRequirementUI | 0x14015b1d0 | 11 | 0x60 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 2/3/6/0/0 |
| BuildRoad | 0x140161020 | 13 | 0x70 | `Work` | Work 0x140161a80 | - | Y | 9/4/0/0/0 |
| BuildStructure | 0x1401610a0 | 13 | 0x70 | `Work` | Work 0x140161a80 | - | Y | 9/4/0/0/0 |
| BuildUI | 0x14015f210 | 11 | 0x50 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/2/5/0/0 |
| CaptureBoxDescription | 0x14015d4a0 | 10 | 0x78 | `ElementDescription` | UI::ElementDescription (Runtime ctor import) | - | Y | 7/2/1/0/0 |
| CaptureBoxElement | 0x14015d440 | 12 | 0xe0 | `Element` | UI::Element (Runtime ctor import) | - | Y | 4/2/6/0/0 |
| CemeteryComponent | 0x1401590a0 | 18 | 0x58 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x140159050, +0x28=0x14015f070 | Y | 4/14/0/0/0 |
| CemeteryDescription | 0x140159130 | 10 | 0x28 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| CemeteryUI | 0x14015b240 | 11 | 0x38 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 3/2/6/0/0 |
| CharacterComponent | 0x140162370 | 16 | 0x80 | `Component` | Component 0x140162920 | - | Y | 7/9/0/0/0 |
| CharacterDescription | 0x1401623f0 | 10 | 0x48 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| CitizenAIComponent | 0x140162470 | 16 | 0xd0 | `Component` | Component 0x140162920 | - | Y | 5/10/0/0/1 |
| CitizenAIDescription | 0x1401624f0 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| CitizenComponent | 0x140162580 | 16 | 0x290 | `Component` | Component 0x140162920 | - | Y | 8/8/0/0/0 |
| CitizenDescription | 0x140162600 | 10 | 0x148 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| CitizenUI | 0x14015ba00 | 11 | 0xe8 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 5/2/4/0/0 |
| ClearArea | 0x140161110 | 13 | 0x78 | `Work` | Work 0x140161a80 | - | Y | 9/4/0/0/0 |
| ClearAreaComponent | 0x1401626d0 | 18 | 0x50 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x14015f070, +0x28=0x140162680 | Y | 2/16/0/0/0 |
| ClearAreaDescription | 0x140162760 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| ClearResourceComponent | 0x140162840 | 18 | 0x48 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x14015f070, +0x28=0x1401627f0 | Y | 2/16/0/0/0 |
| ClearResourceDescription | 0x1401628d0 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| ClearStorage | 0x1401611a0 | 13 | 0x68 | `Work` | Work 0x140161a80 | - | Y | 8/4/0/0/1 |
| ClimateGenerator | 0x14015e140 | 9 | 0x110 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| CompleteRoad | 0x14015fb70 | 10 | 0x30 | `Action` | Action 0x14015faa0 | - | Y | 3/7/0/0/0 |
| CompleteTask | 0x14015fbd0 | 10 | 0x38 | `Action` | Action 0x14015faa0 | - | Y | 5/5/0/0/0 |
| Component | 0x140162920 | 16 | abstract | `Object::Internal` | Object::Internal (Runtime ctor import) | - | Y | 2/14/0/0/0 |
| ComponentDescription | 0x140162a00 | 9 | 0x38 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| ComponentEntity | 0x14015fa78 | 4 | 0x48 | `Object::Internal` | Object::Internal (Runtime ctor import) | - | Y | 2/2/0/0/0 |
| ConsumeProduce | 0x140161220 | 13 | 0x78 | `Work` | Work 0x140161a80 | - | Y | 9/4/0/0/0 |
| ConsumeProduceComponent | 0x140162a80 | 22 | 0x70 | `ResourceProduceComponent` | ResourceProduceComponent 0x1401643f0 | - | Y | 6/16/0/0/0 |
| ConsumeProduceDescription | 0x140162b30 | 10 | 0x58 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| ConsumeProduceUI | 0x14015b2e0 | 11 | 0x40 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/2/5/0/0 |
| ConsumeRawMaterial | 0x14015fc20 | 10 | 0x40 | `Action` | Action 0x14015faa0 | - | Y | 5/5/0/0/0 |
| ControllerComponent | 0x140162b80 | 18 | abstract | `Component` | Component 0x140162920 | - | Y | 2/14/0/2/0 |
| ControlUI | 0x14015b380 | 11 | 0x40 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/2/5/0/0 |
| ControlUIConfig | 0x14015b3e0 | 9 | 0x28 | `UI::DialogControllerConfig` | UI::DialogControllerConfig (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| CreateBridgeDescription | 0x140162c30 | 10 | 0x60 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| CreateDragDescription | 0x140162ca0 | 10 | 0x40 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| CreateFencedDescription | 0x1401591a0 | 10 | 0x78 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| CreateNaturalResource | 0x14015fca0 | 10 | 0x38 | `Action` | Action 0x14015faa0 | - | Y | 4/6/0/0/0 |
| CreatePathDescription | 0x140159210 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| CreatePlacedDescription | 0x140162cf0 | 10 | 0x58 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| CreateRawMaterial | 0x14015fd10 | 10 | 0x58 | `Action` | Action 0x14015faa0 | - | Y | 4/6/0/0/0 |
| CreateRemovalDescription | 0x140162d80 | 10 | 0x48 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| CreateRoadDescription | 0x140162df0 | 10 | 0x30 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| CropFieldComponent | 0x140162ec0 | 18 | 0x68 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x14015f070, +0x28=0x140162e70 | Y | 6/12/0/0/0 |
| CropFieldDescription | 0x140162f50 | 10 | 0x28 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| CropUI | 0x14015f270 | 14 | 0x60 | `SelectResourceUI` | SelectResourceUI 0x14015d330 | - | Y | 7/3/4/0/0 |
| DebugUI | 0x14015b870 | 11 | 0x130 | `DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 5/3/3/0/0 |
| DecalComponent | 0x140162fd0 | 16 | 0x38 | `Component` | Component 0x140162920 | - | Y | 7/9/0/0/0 |
| DecalDescription | 0x140163050 | 10 | 0x50 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| Description | 0x1401629a0 | 9 | abstract | `Object::External` | Object::External (Runtime ctor import) | - | Y | 1/2/1/5/0 |
| DestroyComponent | 0x140163120 | 18 | 0x68 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x14015f070, +0x28=0x1401630d0 | Y | 4/14/0/0/0 |
| DestroyDescription | 0x1401631b0 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| DestroyUI | 0x14015bb00 | 11 | 0x78 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/2/5/0/0 |
| Disease | 0x14015e190 | 9 | 0x48 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| DiseaseComponent | 0x140159290 | 16 | 0x58 | `Component` | Component 0x140162920 | - | Y | 6/10/0/0/0 |
| DiseaseDescription | 0x140159310 | 10 | 0x48 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| DragPlacementTool | 0x140161b90 | 15 | 0x48 | `DragTool` | DragTool 0x140161c10 | - | Y | 8/6/1/0/0 |
| DragTool | 0x140161c10 | 15 | abstract | `Tool` | Tool 0x140161f10 | - | Y | 4/4/1/6/0 |
| DropOffRawMaterial | 0x14015fd80 | 10 | 0x40 | `Action` | Action 0x14015faa0 | - | Y | 4/5/0/0/1 |
| EditTool | 0x140161ca0 | 14 | 0x38 | `Tool` | Tool 0x140161f10 | - | Y | 6/6/1/0/1 |
| EntityUI | 0x14015bb90 | 11 | 0x48 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 6/3/2/0/0 |
| EnvironmentDescription | 0x14015e1f0 | 9 | 0x1b8 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| EventLogUI | 0x14015bc40 | 11 | 0x80 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/3/4/0/0 |
| EventNotifyDialog | 0x14015bca0 | 10 | 0xf0 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 5/3/2/0/0 |
| EventPopupDialog | 0x14015bd80 | 10 | 0xc0 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 3/2/5/0/0 |
| FencedTool | 0x1401605d0 | 15 | 0x70 | `DragTool` | DragTool 0x140161c10 | - | Y | 10/4/1/0/0 |
| FinishTutorialGoal | 0x140160420 | 11 | 0x38 | `Achievement::Goal` | Achievement::Goal (Runtime ctor import) | - | Y | 6/4/1/0/0 |
| FinishTutorialInstance | 0x140160478 | 7 | 0x28 | `Achievement::GoalInstance` | Achievement::GoalInstance (Runtime ctor import) | - | Y | 5/2/0/0/0 |
| FireComponent | 0x140159380 | 16 | 0x48 | `Component` | Component 0x140162920 | - | Y | 4/12/0/0/0 |
| FireDescription | 0x140159400 | 10 | 0x50 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| FireWork | 0x1401612a0 | 13 | 0x60 | `Work` | Work 0x140161a80 | - | Y | 8/4/0/0/1 |
| FishingComponent | 0x140163230 | 22 | 0x58 | `ResourceProduceComponent` | ResourceProduceComponent 0x1401643f0 | - | Y | 8/14/0/0/0 |
| FishingDescription | 0x1401632e0 | 10 | 0x30 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| FishingWork | 0x140161320 | 13 | 0x70 | `Work` | Work 0x140161a80 | - | Y | 9/4/0/0/0 |
| FloorComponent | 0x140163360 | 16 | 0x30 | `Component` | Component 0x140162920 | - | Y | 6/10/0/0/0 |
| FloorDescription | 0x1401633e0 | 10 | 0x30 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| FollowDialog | 0x14015be00 | 10 | 0xd8 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 6/2/2/0/0 |
| ForesterComponent | 0x140163460 | 22 | 0x58 | `ResourceProduceComponent` | ResourceProduceComponent 0x1401643f0 | - | Y | 5/17/0/0/0 |
| ForesterDescription | 0x140163510 | 10 | 0x30 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| ForesterWork | 0x1401613a0 | 13 | 0x60 | `AutoPickupWork` | AutoPickupWork 0x140160fa0 | - | Y | 8/4/0/0/1 |
| FruitComponent | 0x140159450 | 16 | 0x38 | `Component` | Component 0x140162920 | - | Y | 4/12/0/0/0 |
| FruitDescription | 0x1401594d0 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| FrustumOutlineDescription | 0x14015c300 | 10 | 0x80 | `ElementDescription` | UI::ElementDescription (Runtime ctor import) | - | Y | 7/2/1/0/0 |
| FrustumOutlineElement | 0x14015c2a0 | 12 | 0xd8 | `Element` | UI::Element (Runtime ctor import) | - | Y | 4/2/6/0/0 |
| GatherRawMaterials | 0x140161420 | 13 | 0x78 | `Work` | Work 0x140161a80 | - | Y | 10/3/0/0/0 |
| GoalDialog | 0x14015beb0 | 10 | 0xc8 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 4/3/3/0/0 |
| GrowthComponent | 0x140163590 | 16 | 0x50 | `Component` | Component 0x140162920 | - | Y | 7/9/0/0/0 |
| GrowthDescription | 0x140163610 | 10 | 0x68 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| HappinessComponent | 0x1401595d0 | 18 | 0x50 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x30=0x140159580, +0x30=0x14015f070 | Y | 4/14/0/0/0 |
| HappinessDescription | 0x140159660 | 10 | 0x30 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| HarvestArea | 0x1401614a0 | 13 | 0x68 | `AutoPickupWork` | AutoPickupWork 0x140160fa0 | - | Y | 9/4/0/0/0 |
| HarvestOrchard | 0x140161520 | 13 | 0x68 | `AutoPickupWork` | AutoPickupWork 0x140160fa0 | - | Y | 9/4/0/0/0 |
| HealerComponent | 0x140159730 | 18 | 0x48 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x1401596e0, +0x28=0x14015f070 | Y | 3/15/0/0/0 |
| HealerDescription | 0x1401597c0 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| HelpUI | 0x14015bf30 | 11 | 0x78 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/3/4/0/0 |
| HerbalistComponent | 0x140159860 | 22 | 0x50 | `ResourceProduceComponent` | ResourceProduceComponent 0x1401643f0 | - | Y | 5/17/0/0/0 |
| HerbalistDescription | 0x140159910 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| HerbalistWork | 0x1401615a0 | 13 | 0x60 | `AutoPickupWork` | AutoPickupWork 0x140160fa0 | - | Y | 8/4/0/0/1 |
| HerdComponent | 0x140163680 | 16 | 0x60 | `Component` | Component 0x140162920 | - | Y | 5/11/0/0/0 |
| HerdDescription | 0x140163700 | 10 | 0x40 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| HighlightComponent | 0x140159990 | 16 | 0x128 | `Component` | Component 0x140162920 | - | Y | 4/12/0/0/0 |
| HighlightDescription | 0x140159a10 | 10 | 0x40 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| HotKeyToolTipUI | 0x14015bfc0 | 11 | 0x30 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 2/3/6/0/0 |
| HuntAnimal | 0x14015fde0 | 10 | 0x40 | `Action` | Action 0x14015faa0 | - | Y | 6/4/0/0/0 |
| HuntComponent | 0x140163770 | 22 | 0x58 | `ResourceProduceComponent` | ResourceProduceComponent 0x1401643f0 | - | Y | 6/16/0/0/0 |
| HuntDescription | 0x140163820 | 10 | 0x28 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| HuntWork | 0x140161620 | 13 | 0x58 | `AutoPickupWork` | AutoPickupWork 0x140160fa0 | - | Y | 8/4/0/0/1 |
| ImmigrationComponent | 0x140159b30 | 18 | 0x68 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x140159ae0, +0x28=0x14015f070 | Y | 2/16/0/0/0 |
| ImmigrationDescription | 0x140159bc0 | 10 | 0x68 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| ImmigrationUI | 0x14015c080 | 11 | 0x50 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/2/5/0/0 |
| InteractComponent | 0x1401638b0 | 16 | 0x38 | `Component` | Component 0x140162920 | - | Y | 3/13/0/0/0 |
| InteractDescription | 0x140163930 | 10 | 0x58 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| LivestockComponent | 0x1401639b0 | 16 | 0x38 | `Component` | Component 0x140162920 | - | Y | 5/11/0/0/0 |
| LivestockDescription | 0x140163a30 | 10 | 0x30 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| LoaderDialog | 0x14015f300 | 10 | 0xc8 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 5/2/3/0/0 |
| LocationWork | 0x1401616a0 | 13 | 0x50 | `Work` | Work 0x140161a80 | - | Y | 9/4/0/0/0 |
| MainMenuDialog | 0x14015f550 | 10 | 0xc8 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 4/2/4/0/0 |
| MapComponent | 0x140163aa0 | 16 | 0x38 | `Component` | Component 0x140162920 | - | Y | 5/11/0/0/0 |
| MapDescription | 0x140163b20 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| MapGenerator | 0x14015ed80 | 9 | 0x178 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| MapParamsDialog | 0x14015c180 | 10 | 0xf8 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 4/3/3/0/0 |
| MapUI | 0x14015c350 | 11 | 0x50 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 3/3/5/0/0 |
| MarketComponent | 0x140159c40 | 22 | 0x50 | `ResourceProduceComponent` | ResourceProduceComponent 0x1401643f0 | - | Y | 5/17/0/0/0 |
| MarketDescription | 0x140159cf0 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| MarketLocationComponent | 0x140159dd0 | 18 | 0x48 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x140159d80, +0x28=0x14015f070 | Y | 3/15/0/0/0 |
| MarketLocationDescription | 0x140159e60 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| MarketWork | 0x140161710 | 13 | 0x58 | `Work` | Work 0x140161a80 | - | Y | 8/4/0/0/1 |
| MenuScene | 0x14015ede0 | 9 | 0x208 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/4/0/0/0 |
| MessageDialog | 0x14015c480 | 14 | 0xc0 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 7/3/4/0/0 |
| MineComponent | 0x140159ed0 | 16 | 0x28 | `Component` | Component 0x140162920 | - | Y | 2/14/0/0/0 |
| MineDescription | 0x140159f50 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| ModBrowseUI | 0x14015c500 | 11 | 0x30 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 1/3/7/0/0 |
| ModDetailsDialog | 0x14015c670 | 10 | 0xc8 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 3/2/5/0/0 |
| ModDialog | 0x14015c870 | 10 | 0xd8 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 3/2/5/0/0 |
| ModelComponent | 0x140163ba0 | 16 | 0x170 | `Component` | Component 0x140162920 | - | Y | 6/10/0/0/0 |
| ModelDescription | 0x140163c20 | 10 | 0x40 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| ModUploadDialog | 0x14015c6c0 | 10 | 0xb8 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 2/2/6/0/0 |
| MoveTo | 0x14015fe40 | 10 | 0x480 | `Action` | Action 0x14015faa0 | - | Y | 8/2/0/0/0 |
| MultiObjectComponent | 0x140163cf0 | 18 | 0x60 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x14015f070, +0x28=0x140163ca0 | Y | 4/14/0/0/0 |
| MultiObjectDescription | 0x140163d80 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| MusicList | 0x14015ee40 | 9 | 0x30 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| NameDatabase | 0x14015eea0 | 9 | 0x60 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/4/0/0/0 |
| NaturalResourceComponent | 0x140163e10 | 16 | 0x30 | `Component` | Component 0x140162920 | - | Y | 3/13/0/0/0 |
| NaturalResourceDescription | 0x140163e90 | 10 | 0x88 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| NewGameDialog | 0x14015c920 | 10 | 0xae8 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 4/3/3/0/0 |
| NoiseUI | 0x14015ca80 | 11 | 0x70 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 3/3/5/0/0 |
| NomadComponent | 0x14015a020 | 18 | 0x78 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x58=0x140159fd0, +0x58=0x14015f070 | Y | 3/15/0/0/0 |
| NomadDescription | 0x14015a0b0 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| OptionInputBindDialog | 0x14015ce40 | 14 | 0xc8 | `MessageDialog` | MessageDialog 0x14015c480 | - | Y | 8/3/3/0/0 |
| OptionsDialog | 0x14015ceb0 | 10 | 0x1c0 | `public UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 3/3/4/0/0 |
| OrchardComponent | 0x14015a180 | 18 | 0x68 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x14015a130, +0x28=0x14015f070 | Y | 6/12/0/0/0 |
| OrchardDescription | 0x14015a210 | 10 | 0x30 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| OrchardUI | 0x14015cf30 | 14 | 0x48 | `SelectResourceUI` | SelectResourceUI 0x14015d330 | - | Y | 7/3/4/0/0 |
| PageToggleUI | 0x14015cfb0 | 11 | 0x48 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/3/4/0/0 |
| ParticleComponent | 0x14015a290 | 16 | 0x48 | `Component` | Component 0x140162920 | - | Y | 8/8/0/0/0 |
| ParticleDescription | 0x14015a310 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| PastureComponent | 0x140163f60 | 18 | 0x68 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x48=0x14015f070, +0x48=0x140163f10 | Y | 9/9/0/0/0 |
| PastureDescription | 0x140163ff0 | 10 | 0x28 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| PastureUI | 0x14015f610 | 14 | 0x40 | `SelectResourceUI` | SelectResourceUI 0x14015d330 | - | Y | 7/3/4/0/0 |
| PastureWork | 0x1401617a0 | 13 | 0x58 | `AutoPickupWork` | AutoPickupWork 0x140160fa0 | - | Y | 9/4/0/0/0 |
| PathTool | 0x140160680 | 14 | 0x60 | `Tool` | Tool 0x140161f10 | - | Y | 6/6/1/0/1 |
| PerformWork | 0x14015fea0 | 10 | 0x48 | `Action` | Action 0x14015faa0 | - | Y | 7/3/0/0/0 |
| PickingComponent | 0x140164040 | 16 | 0x38 | `Component` | Component 0x140162920 | - | Y | 4/10/0/0/2 |
| PickingDescription | 0x1401640c0 | 10 | 0x40 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| Pickup | 0x140161810 | 13 | 0x60 | `Work` | Work 0x140161a80 | - | Y | 11/2/0/0/0 |
| PickupRawMaterial | 0x14015ff10 | 10 | 0x30 | `Action` | Action 0x14015faa0 | - | Y | 4/6/0/0/0 |
| PickupStoredRawMaterial | 0x14015ff80 | 10 | 0x48 | `Action` | Action 0x14015faa0 | - | Y | 5/5/0/0/0 |
| PlacementTool | 0x140161d20 | 13 | 0x70 | `Tool` | Tool 0x140161f10 | - | Y | 7/5/1/0/0 |
| PlantArea | 0x140161890 | 13 | 0x70 | `Work` | Work 0x140161a80 | - | Y | 9/4/0/0/0 |
| PlantOrchard | 0x140161910 | 13 | 0x70 | `Work` | Work 0x140161a80 | - | Y | 9/4/0/0/0 |
| PlayAnimation | 0x140158950 | 10 | 0x40 | `Action` | Action 0x14015faa0 | - | Y | 5/5/0/0/0 |
| PopulationComponent | 0x14015a360 | 16 | 0x70 | `Component` | Component 0x140162920 | - | Y | 6/10/0/0/0 |
| PopulationDescription | 0x14015a3e0 | 10 | 0x58 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| PopulationGoal | 0x140160370 | 11 | 0x38 | `Achievement::Goal` | Achievement::Goal (Runtime ctor import) | - | Y | 6/3/2/0/0 |
| ProductionUI | 0x14015d070 | 11 | 0x50 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 3/3/5/0/0 |
| Profession | 0x14015efa0 | 9 | 0x60 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| ProfessionList | 0x14015ef50 | 9 | 0x50 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| ProfessionUI | 0x14015d0f0 | 11 | 0x78 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/2/5/0/0 |
| QuarryComponent | 0x14015a490 | 16 | 0x28 | `Component` | Component 0x140162920 | - | Y | 4/12/0/0/0 |
| QuarryDescription | 0x14015a510 | 10 | 0x30 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| RadiusDescription | 0x14015a580 | 10 | 0x30 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| RadiusUI | 0x14015d160 | 11 | 0x38 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 3/2/6/0/0 |
| RawMaterialComponent | 0x140164170 | 16 | 0x40 | `Component` | Component 0x140162920 | - | Y | 5/11/0/0/0 |
| RawMaterialDescription | 0x1401641f0 | 10 | 0x60 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| RemovalTool | 0x140161de0 | 15 | 0x40 | `DragTool` | DragTool 0x140161c10 | - | Y | 8/6/1/0/0 |
| RemoveAnimal | 0x14015ffe0 | 10 | 0x28 | `Action` | Action 0x14015faa0 | - | Y | 5/5/0/0/0 |
| RemoveNaturalResource | 0x140160050 | 10 | 0x38 | `Action` | Action 0x14015faa0 | - | Y | 5/5/0/0/0 |
| ResidenceComponent | 0x1401642c0 | 18 | 0x60 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x14015f070, +0x28=0x140164270 | Y | 4/14/0/0/0 |
| ResidenceDescription | 0x140164350 | 10 | 0x30 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| ResidenceUI | 0x14015f6d0 | 11 | 0x40 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/3/4/0/0 |
| ResourceLimitUI | 0x14015d1f0 | 11 | 0x40 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/2/5/0/0 |
| ResourceLimitUIConfig | 0x14015d250 | 9 | 0x28 | `UI::DialogControllerConfig` | UI::DialogControllerConfig (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| ResourceProduceComponent | 0x1401643f0 | 22 | abstract | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x14015f070, +0x28=0x1401643a0 | Y | 4/17/0/1/0 |
| RoadTool | 0x140161e70 | 15 | 0x48 | `DragTool` | DragTool 0x140161c10 | - | Y | 8/6/1/0/0 |
| RotationSetting | 0x140160710 | 4 | 0x18 | `ToolSetting` | ToolSetting 0x140161ee8 | - | Y | 1/2/1/0/0 |
| SaveLoadDialog | 0x14015f900 | 10 | 0x160 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 5/2/3/0/0 |
| Scenario | 0x14015f020 | 9 | 0x90 | `::Achievement::Achievement` | Achievement::Achievement (Runtime ctor import) | - | Y | 6/2/1/0/0 |
| ScenarioDialog | 0x14015d2d0 | 10 | 0xe8 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 4/3/3/0/0 |
| SelectDescription | 0x14015a5f0 | 10 | 0x48 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| SelectResourceUI | 0x14015d330 | 14 | abstract | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 5/2/4/3/0 |
| SendComponentMessage | 0x1401600c0 | 10 | 0x40 | `Action` | Action 0x14015faa0 | - | Y | 4/6/0/0/0 |
| ShowTool | 0x1401589b0 | 10 | 0x30 | `Action` | Action 0x14015faa0 | - | Y | 5/5/0/0/0 |
| SpawnComponent | 0x14015a670 | 16 | 0x38 | `Component` | Component 0x140162920 | - | Y | 4/12/0/0/0 |
| SpawnDescription | 0x14015a6f0 | 10 | 0x30 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| SpriteCaptureUI | 0x14015d4f0 | 11 | 0x70 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/3/4/0/0 |
| StateMachineComponent | 0x140164520 | 16 | 0x58 | `Component` | Component 0x140162920 | +0x28=0x14015f070, +0x28=0x1401644d0 | Y | 7/9/0/0/0 |
| StateMachineDescription | 0x1401645a0 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| StatGoal | 0x1401604c0 | 11 | 0x80 | `Achievement::Goal` | Achievement::Goal (Runtime ctor import) | - | Y | 6/3/2/0/0 |
| StatusBarUI | 0x14015d810 | 11 | 0x1d0 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/3/4/0/0 |
| StatusIconComponent | 0x140164620 | 16 | 0x108 | `Component` | Component 0x140162920 | +0x28=0x14015f0c0 | Y | 6/10/0/0/0 |
| StatusIconDescription | 0x1401646a0 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| StatusIconUI | 0x14015d8a0 | 11 | 0x40 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 3/2/6/0/0 |
| StatusIconUIConfig | 0x14015d900 | 9 | 0x28 | `UI::DialogControllerConfig` | UI::DialogControllerConfig (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| StorageCapacityUI | 0x14015d950 | 11 | 0x38 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 3/3/5/0/0 |
| StorageComponent | 0x140164720 | 16 | 0x40 | `Component` | Component 0x140162920 | - | Y | 3/13/0/0/0 |
| StorageDescription | 0x1401647a0 | 10 | 0x40 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| StorageDisplayComponent | 0x14015a780 | 16 | 0x38 | `Component` | Component 0x140162920 | - | Y | 5/10/0/0/1 |
| StorageDisplayDescription | 0x14015a800 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| StorageLocationComponent | 0x140164880 | 18 | 0x48 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x14015f070, +0x28=0x140164830 | Y | 3/15/0/0/0 |
| StorageLocationDescription | 0x140164910 | 10 | 0x28 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| StorageUI | 0x14015f970 | 11 | 0x40 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 3/3/5/0/0 |
| TendArea | 0x140161990 | 13 | 0x60 | `Work` | Work 0x140161a80 | - | Y | 8/4/0/0/1 |
| TerraformComponent | 0x14015a880 | 16 | 0x48 | `Component` | Component 0x140162920 | - | Y | 4/12/0/0/0 |
| TerraformDescription | 0x14015a900 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| TerrainGenerator | 0x14015f170 | 9 | 0x90 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| TimeGoal | 0x140160540 | 11 | 0x38 | `Achievement::Goal` | Achievement::Goal (Runtime ctor import) | - | Y | 6/4/1/0/0 |
| TimeGoalInstance | 0x140160598 | 7 | 0x28 | `Achievement::GoalInstance` | Achievement::GoalInstance (Runtime ctor import) | - | Y | 4/3/0/0/0 |
| TitleDescToolTipUI | 0x14015d9f0 | 11 | 0x30 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 2/3/6/0/0 |
| TombstoneComponent | 0x14015a980 | 16 | 0x30 | `Component` | Component 0x140162920 | - | Y | 4/12/0/0/0 |
| TombstoneDescription | 0x14015aa00 | 10 | 0x28 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| Tool | 0x140161f10 | 13 | abstract | `Object::Internal` | Object::Internal (Runtime ctor import) | - | Y | 2/4/1/6/0 |
| Toolbar | 0x140160120 | 9 | 0x68 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| ToolbarDescription | 0x140164980 | 10 | 0x68 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| ToolbarDialog | 0x14015fa00 | 10 | 0x108 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 3/3/3/0/1 |
| ToolSetting | 0x140161ee8 | 4 | abstract | `Object::Internal` | Object::Internal (Runtime ctor import) | - | Y | 1/2/1/0/0 |
| ToolUI | 0x14015da70 | 11 | 0x58 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/3/4/0/0 |
| Tornado | 0x140160190 | 10 | 0x90 | `Graphics::Particle` | Graphics::Particle (Runtime ctor import) | - | Y | 7/2/1/0/0 |
| TornadoComponent | 0x14015aa80 | 16 | 0x68 | `Component` | Component 0x140162920 | - | Y | 4/12/0/0/0 |
| TornadoDescription | 0x14015ab00 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| TownHallUI | 0x14015dce0 | 11 | 0x1e8 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 6/2/3/0/0 |
| TrackerComponent | 0x14015ab80 | 16 | 0x50 | `Component` | Component 0x140162920 | - | Y | 2/14/0/0/0 |
| TrackerDescription | 0x14015ac00 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| TradeComponent | 0x14015acd0 | 18 | 0xa8 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x88=0x14015ac80, +0x88=0x14015f070 | Y | 6/12/0/0/0 |
| TradeDescription | 0x14015ad60 | 10 | 0x70 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| TradeUI | 0x14015de80 | 11 | 0x148 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 8/2/1/0/0 |
| TradeWork | 0x140161a10 | 13 | 0x58 | `Work` | Work 0x140161a80 | - | Y | 8/4/0/0/1 |
| Tutorial | 0x140160740 | 9 | 0x40 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| TutorialGoal | 0x1401607a0 | 9 | abstract | `Object::External` | Object::External (Runtime ctor import) | - | Y | 2/3/1/3/0 |
| TutorialGoalButton | 0x140160800 | 10 | 0x30 | `TutorialGoal` | TutorialGoal 0x1401607a0 | - | Y | 6/3/1/0/0 |
| TutorialGoalCount | 0x140160950 | 10 | 0x50 | `TutorialGoal` | TutorialGoal 0x1401607a0 | - | Y | 6/3/1/0/0 |
| TutorialGoalDialog | 0x140160870 | 10 | 0x40 | `TutorialGoal` | TutorialGoal 0x1401607a0 | - | Y | 6/3/1/0/0 |
| TutorialGoalGameSpeed | 0x1401608e0 | 10 | 0x28 | `TutorialGoal` | TutorialGoal 0x1401607a0 | - | Y | 6/3/1/0/0 |
| TutorialHighlightDialog | 0x1401609e0 | 10 | 0xf0 | `UI::DialogInstance` | UI::DialogInstance (Runtime ctor import) | - | Y | 5/2/3/0/0 |
| TutorialImage | 0x140160a40 | 10 | 0x30 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 6/3/1/0/0 |
| TutorialImageHotKey | 0x140160ab0 | 10 | 0x48 | `TutorialImage` | TutorialImage 0x140160a40 | - | Y | 6/3/1/0/0 |
| TutorialImageInputKey | 0x140160b20 | 10 | 0x38 | `TutorialImage` | TutorialImage 0x140160a40 | - | Y | 6/3/1/0/0 |
| TutorialImageToolbarIcon | 0x140160b90 | 10 | 0x48 | `TutorialImage` | TutorialImage 0x140160a40 | - | Y | 6/3/1/0/0 |
| TutorialInstance | 0x140160bf8 | 4 | 0x38 | `Object::Internal` | Object::Internal (Runtime ctor import) | - | Y | 2/2/0/0/0 |
| TutorialSetup | 0x140160c30 | 11 | abstract | `Object::External` | Object::External (Runtime ctor import) | - | Y | 2/3/1/5/0 |
| TutorialSetupCamera | 0x140160ca0 | 12 | 0x30 | `TutorialSetup` | TutorialSetup 0x140160c30 | - | Y | 6/5/1/0/0 |
| TutorialSetupDialog | 0x140160d20 | 12 | 0x78 | `TutorialSetup` | TutorialSetup 0x140160c30 | - | Y | 8/3/1/0/0 |
| TutorialSetupGameSpeed | 0x140160da0 | 12 | 0x28 | `TutorialSetup` | TutorialSetup 0x140160c30 | - | Y | 6/5/1/0/0 |
| TutorialSetupRestrictPlayer | 0x140160e20 | 12 | 0x68 | `TutorialSetup` | TutorialSetup 0x140160c30 | - | Y | 7/4/1/0/0 |
| TutorialStep | 0x140160e90 | 9 | 0x70 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| TutorialUI | 0x140160f30 | 11 | 0x60 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/3/4/0/0 |
| UIComponent | 0x1401649f0 | 16 | 0x38 | `Component` | Component 0x140162920 | - | Y | 3/13/0/0/0 |
| UIDescription | 0x140164a70 | 10 | 0x48 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| UpgradeUI | 0x14015df00 | 11 | 0x38 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 3/3/5/0/0 |
| UpgradeUIConfig | 0x14015df60 | 9 | 0x28 | `UI::DialogControllerConfig` | UI::DialogControllerConfig (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| ValueDescription | 0x14015add0 | 10 | 0x50 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| Weather | 0x140160290 | 9 | 0x80 | `Object::External` | Object::External (Runtime ctor import) | - | Y | 5/3/1/0/0 |
| WellComponent | 0x14015ae90 | 18 | 0x48 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x14015ae40, +0x28=0x14015f070 | Y | 3/15/0/0/0 |
| WellDescription | 0x14015af20 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| WildAnimalComponent | 0x140164af0 | 16 | 0x30 | `Component` | Component 0x140162920 | - | Y | 3/13/0/0/0 |
| WildAnimalDescription | 0x140164b70 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| Work | 0x140161a80 | 13 | abstract | `Object::Internal` | Object::Internal (Runtime ctor import) | - | Y | 2/4/0/7/0 |
| WorkComponent | 0x140164be0 | 16 | 0x38 | `Component` | Component 0x140162920 | - | Y | 4/12/0/0/0 |
| WorkDescription | 0x140164c60 | 10 | 0x38 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| WorkPlaceComponent | 0x140164d30 | 18 | 0x68 | `ControllerComponent` | ControllerComponent 0x140162b80 | +0x28=0x14015f070, +0x28=0x140164ce0 | Y | 3/15/0/0/0 |
| WorkPlaceDescription | 0x140164dc0 | 10 | 0x30 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| WorkPlaceUI | 0x14015e030 | 11 | 0x68 | `UI::DialogController` | UI::DialogController (Runtime ctor import) | - | Y | 4/2/5/0/0 |
| ZoneComponent | 0x140164e30 | 16 | 0x28 | `Component` | Component 0x140162920 | - | Y | 4/12/0/0/0 |
| ZoneDescription | 0x140164eb0 | 10 | 0x20 | `Description` | Description 0x1401629a0 | - | Y | 6/3/1/0/0 |
| ({AbsorbResourceComponent controller member}) | 0x140161fb0 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({AttendanceComponent controller member}) | 0x140158c90 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| {BoatComponent controller member} | 0x140158ef0 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({BuildComponent controller member}) | 0x140162210 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({CemeteryComponent controller member}) | 0x140159050 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({ClearAreaComponent controller member}) | 0x140162680 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({ClearResourceComponent controller member}) | 0x1401627f0 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| {controller interface, 9 pure slots} | 0x14015f070 | 9 | ? | - | - | - | N | 0/0/0/9/0 |
| ({CropFieldComponent controller member}) | 0x140162e70 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| {CRT __C_specific_handler function pointer, not a vtable} | 0x14017cb78 | 1 | ? | - | - | - | N | 1/0/0/0/0 |
| {CRT _cinit function pointer, not a vtable} | 0x14017cb70 | 1 | ? | - | - | - | N | 0/0/0/0/1 |
| {CRT std::bad_exception vftable} | 0x14017cb88 | 2 | ? | - | {CRT std::exception vftable} 0x14017cc00 | - | N | 2/0/0/0/0 |
| {CRT std::exception vftable} | 0x14017cc00 | 2 | ? | - | - | - | N | 2/0/0/0/0 |
| {CRT vftable} | 0x14017cbf0 | 1 | ? | - | - | - | N | 1/0/0/0/0 |
| ({DestroyComponent controller member}) | 0x1401630d0 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| {exe Graphics::Geometry subclass A} | 0x14015f110 | 9 | alloc 0x230 | - | Graphics::Geometry (Runtime ctor import) | - | N | 3/0/5/0/1 |
| {exe Graphics::Geometry subclass B} | 0x140160240 | 9 | alloc 0xf0 | - | Graphics::Geometry (Runtime ctor import) | - | N | 4/0/5/0/0 |
| {exe Graphics::Geometry subclass, StatusIconComponent member} | 0x14015f0c0 | 9 | ? | - | Graphics::Geometry (Runtime ctor import) | - | N | 3/1/5/0/0 |
| {exe Graphics::ParticleInstance subclass} | 0x1401601e0 | 10 | ? | - | Graphics::ParticleInstance (Runtime ctor import) | - | N | 2/0/8/0/0 |
| {exe System::Thread subclass} | 0x14015ef18 | 2 | alloc 0x1f8 | - | System::Thread (Runtime ctor import) | - | N | 1/0/1/0/0 |
| {GameUpdateLoop} (g_GameUpdateLoop_Vtable) | 0x14015e2f0 | 12 | ? | - | Engine::UpdateLoop (Runtime ctor import) | - | N | 9/2/1/0/0 |
| ({HappinessComponent controller member}) | 0x140159580 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({HealerComponent controller member}) | 0x1401596e0 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({ImmigrationComponent controller member}) | 0x140159ae0 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| {LoaderLoop subclass inner, GameMain} | 0x140158870 | 12 | ? | - | - | - | N | 2/1/9/0/0 |
| {LoaderLoop subclass outer} (g_GameLoaderLoop_Vtable) | 0x1401588d0 | 12 | ? | - | - | - | N | 1/0/11/0/0 |
| ({MarketLocationComponent controller member}) | 0x140159d80 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| {MenuDialogLoopT} (g_MenuDialogLoopT_Vtable) | 0x14015ed00 | 13 | ? | - | Engine::DialogLoopT (Runtime ctor import) | - | N | 9/2/2/0/0 |
| ({MultiObjectComponent controller member}) | 0x140163ca0 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({NomadComponent controller member}) | 0x140159fd0 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({OrchardComponent controller member}) | 0x14015a130 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({PastureComponent controller member}) | 0x140163f10 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({ResidenceComponent controller member}) | 0x140164270 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({ResourceProduceComponent controller member}) | 0x1401643a0 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| {splash DialogLoaderLoopT subclass} (g_GameLoadDialogLoopT_Vtable) | 0x140158800 | 13 | ? | - | - | - | N | 4/1/8/0/0 |
| {StateMachineComponent controller member} | 0x1401644d0 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({StorageLocationComponent controller member}) | 0x140164830 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({TradeComponent controller member}) | 0x14015ac80 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({WellComponent controller member}) | 0x14015ae40 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |
| ({WorkPlaceComponent controller member}) | 0x140164ce0 | 9 | ? | - | {controller interface, 9 pure slots} 0x14015f070 | - | N | 9/0/0/0/0 |

### §0a — reflected classes whose parent literal differs from the structural parent (0)

```
```


## §1 — the exe base classes and intermediates: slot lists (which slots are pure)

Slots 0-3 of every exe class follow the Object layout of B37_SLOTS §2: 0 = the scalar deleting dtor, 1 = `TypeName` (returns
the `BaseTypeInfo` name pointer at ti+8), 2 = `Type` (returns the type id at ti+0), 3 = `Persist` (Internal-rooted) or
`GetFileVersion` (External-rooted). Slots 4+ have no retail names (the exe has no symbols).

- `Component` 0x140162920, 16 slots, **no pure slot**: 0 own dtor, 1 TypeName, 2 Type, 3 own Persist (0x1401189b0, 153 B),
  4..12 and 14..15 shared `RET 0` no-ops, 13 `return 0`. The ctor 0x1401188b0 calls `??0Internal@Object@@QEAA@XZ` then stamps
  0x140162920; every derived ctor calls 0x1401188b0 first.
- `ControllerComponent` 0x140162b80, 18 slots, **pure 16 and 17**: 0-3 own (3 = 0x1401195a0, 48 B), 4..15 = Component's,
  16 = a per-class constant (`return N`: 0, 1, 2 or 4 — see §2), 17 = "return &m_controller" (`LEA RAX,[RCX+off]; RET`).
- the controller interface 0x14015f070: 9 slots, **all 9 pure** (slot 0 as well: it is not a dtor, because the overrides' slot 0 is
  a lookup function, e.g. 0x140002a80). It is stamped at this+0 once (0x140002890) and in every family ctor/dtor at the member
  offset.
- `Description` 0x1401629a0 (parent `Object::External`): pure 3 (GetFileVersion) and 5..8 (Serialize, Reconstruct, Destruct,
  Construct); slot 4 is the imported `Object::External::Compile`.
- `Action` 0x14015faa0: pure 4. `Work` 0x140161a80: pure 4 and 6..11. `Tool` 0x140161f10: pure 4..9. `StateMachine*` in the exe
  is only `StateMachineComponent` 0x140164520 (a plain Component subclass, 16 slots, no pure slot) and its Description; the rest
  are listed below.
- The exe `Dialog*` intermediates: `SelectResourceUI` 0x14015d330 (abstract, parent `UI::DialogController`, 3 children) and
  `MessageDialog` 0x14015c480 (parent `UI::DialogInstance`, 1 child). All other dialogs derive the Runtime `UI::DialogInstance` /
  `UI::DialogController` / `UI::DialogControllerConfig` directly.


```
### Component  table=0x140162920  slots=16  sizeof=abstract  parent="Object::Internal" / structural Object::Internal (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c72a8 create=0x140003730 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014ea10 Component_Register
  [ 0] +0x00 0x140031cc0  OWN Component_ScalarDtor (73 B)
  [ 1] +0x08 0x140031c40  TINY returns qword [0x1401c72b0]
  [ 2] +0x10 0x140031c30  TINY returns dword [0x1401c72a8] = TypeId(Component)
  [ 3] +0x18 0x1401189b0  OWN Component_Serialize (153 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]
### ControllerComponent  table=0x140162b80  slots=18  sizeof=abstract  parent="Component" / structural Component 0x140162920  stop=non-code qword 0x7242657461657243 (string "CreateBridgeDescription")
  reflected: ti=0x1401c73c0 create=0x140003730 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014eb50 ControllerComponent_RegisterType
  [ 0] +0x00 0x140032800  OWN FUN_140032800 (83 B)
  [ 1] +0x08 0x1400327f0  TINY returns qword [0x1401c73c8]
  [ 2] +0x10 0x1400327e0  TINY returns dword [0x1401c73c0] = TypeId(ControllerComponent)
  [ 3] +0x18 0x1401195a0  OWN Component_impl_95a0 (48 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x14003addc  _purecall  [in 12 tables]
  [17] +0x88 0x14003addc  _purecall  [in 12 tables]
### {controller interface, 9 pure slots}  table=0x14015f070  slots=9  sizeof=?  parent=- / structural -  stop=zero qword
  [ 0] +0x00 0x14003addc  _purecall  [in 12 tables]
  [ 1] +0x08 0x14003addc  _purecall  [in 12 tables]
  [ 2] +0x10 0x14003addc  _purecall  [in 12 tables]
  [ 3] +0x18 0x14003addc  _purecall  [in 12 tables]
  [ 4] +0x20 0x14003addc  _purecall  [in 12 tables]
  [ 5] +0x28 0x14003addc  _purecall  [in 12 tables]
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]
### ({ResourceProduceComponent controller member})  table=0x1401643a0  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140038040  OWN ResourceProduceComponent_CtrlVtable_0 (44 B)
  [ 1] +0x08 0x140038070  OWN ResourceProduceComponent_CtrlVtable_1 (81 B)
  [ 2] +0x10 0x1400380d0  OWN ResourceProduceComponent_CtrlVtable_2 (76 B)
  [ 3] +0x18 0x140038120  OWN ResourceProduceComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140038150  OWN ResourceProduceComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140038180  OWN ResourceProduceComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x1400381b0  OWN ResourceProduceComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x1400381e0  OWN ResourceProduceComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140038210  OWN ResourceProduceComponent_Controller_OnRemove (12 B)
### Description  table=0x1401629a0  slots=9  sizeof=abstract  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6e656e6f706d6f43 (string "ComponentDescription")
  reflected: ti=0x1401c72e0 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014ea50 Description_Register
  [ 0] +0x00 0x140031c70  OWN FUN_140031c70 (73 B)
  [ 1] +0x08 0x140031c60  TINY returns qword [0x1401c72e8]
  [ 2] +0x10 0x140031c50  TINY returns dword [0x1401c72e0] = TypeId(Description)
  [ 3] +0x18 0x14003addc  _purecall  [in 12 tables]
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x14003addc  _purecall  [in 12 tables]
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]
### Action  table=0x14015faa0  slots=10  sizeof=abstract  parent="Object::Internal" / structural Object::Internal (Runtime ctor import)  stop=non-code qword 0x4e6574616d696e41 (string "AnimateNaturalResource")
  reflected: ti=0x1401c5da8 create=0x140003730 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d010 Action_RegisterType
  [ 0] +0x00 0x140021c90  OWN ComponentEntity_Slot5 (94 B)
  [ 1] +0x08 0x140021c80  TINY returns qword [0x1401c5db0]
  [ 2] +0x10 0x140021c70  TINY returns dword [0x1401c5da8] = TypeId(Action)
  [ 3] +0x18 0x1400c4b40  OWN ComponentEntity_OnRemove (69 B)
  [ 4] +0x20 0x14003addc  _purecall  [in 12 tables]
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]
  [ 8] +0x40 0x1400c4a30  TINY RET 0 (c2 00 00)  [in 7 tables]
  [ 9] +0x48 0x1400c4a40  TINY return false (xor al,al)  [in 2 tables]
### Work  table=0x140161a80  slots=13  sizeof=abstract  parent="Object::Internal" / structural Object::Internal (Runtime ctor import)  stop=non-code qword 0x6f54656764697242 (string "BridgeTool")
  reflected: ti=0x1401c6cf8 create=0x140003730 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e190 Work_RegisterType
  [ 0] +0x00 0x14002ca60  OWN TradeWork_Slot14 (73 B)
  [ 1] +0x08 0x14002ca50  TINY returns qword [0x1401c6d00]
  [ 2] +0x10 0x14002ca40  TINY returns dword [0x1401c6cf8] = TypeId(Work)
  [ 3] +0x18 0x1400f7400  OWN Work_Shutdown (302 B)
  [ 4] +0x20 0x14003addc  _purecall  [in 12 tables]
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]
  [ 9] +0x48 0x14003addc  _purecall  [in 12 tables]
  [10] +0x50 0x14003addc  _purecall  [in 12 tables]
  [11] +0x58 0x14003addc  _purecall  [in 12 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]
### AutoPickupWork  table=0x140160fa0  slots=13  sizeof=abstract  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x616f52646c697542 (string "BuildRoad")
  reflected: ti=0x1401c6828 create=0x140003730 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014dc10 AutoPickupWork_RegisterType
  [ 0] +0x00 0x1400294e0  OWN AutoPickupWork_ScalarDtor (83 B)
  [ 1] +0x08 0x1400294d0  TINY returns qword [0x1401c6830]
  [ 2] +0x10 0x1400294c0  TINY returns dword [0x1401c6828] = TypeId(AutoPickupWork)
  [ 3] +0x18 0x1400e3f80  OWN AutoPickupWork_Shutdown (48 B)
  [ 4] +0x20 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 8] +0x40 0x1400e3f30  OWN AutoPickupWork_OnRemove (74 B)
  [ 9] +0x48 0x14003addc  _purecall  [in 12 tables]   = parent
  [10] +0x50 0x14003addc  _purecall  [in 12 tables]   = parent
  [11] +0x58 0x14003addc  _purecall  [in 12 tables]   = parent
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
### Tool  table=0x140161f10  slots=13  sizeof=abstract  parent="Object::Internal" / structural Object::Internal (Runtime ctor import)  stop=non-code qword 0x655262726f736241 (string "AbsorbResourceComponent")
  reflected: ti=0x1401c6eb8 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014e390 Tool_StaticInit
  [ 0] +0x00 0x14002e9b0  OWN Tool_ScalarDtor (73 B)
  [ 1] +0x08 0x14002e950  TINY returns qword [0x1401c6ec0]
  [ 2] +0x10 0x14002e940  TINY returns dword [0x1401c6eb8] = TypeId(Tool)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x14003addc  _purecall  [in 12 tables]
  [ 5] +0x28 0x14003addc  _purecall  [in 12 tables]
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]
  [ 9] +0x48 0x14003addc  _purecall  [in 12 tables]
  [10] +0x50 0x1400268a0  TINY return false (xor al,al)  [in 8 tables]
  [11] +0x58 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]
  [12] +0x60 0x140105860  TINY return 1 (mov eax)  [in 2 tables]
### DragTool  table=0x140161c10  slots=15  sizeof=abstract  parent="Tool" / structural Tool 0x140161f10  stop=non-code qword 0x6c6f6f5474696445 (string "EditTool")
  reflected: ti=0x1401c6da0 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014e250 DragTool_RegisterType
  [ 0] +0x00 0x14002da20  OWN DragTool_ScalarDtor (83 B)
  [ 1] +0x08 0x14002da10  TINY returns qword [0x1401c6da8]
  [ 2] +0x10 0x14002da00  TINY returns dword [0x1401c6da0] = TypeId(DragTool)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  [ 4] +0x20 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 5] +0x28 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 6] +0x30 0x1400fc6b0  OWN BridgeTool_Draw (785 B)  [in 4 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 9] +0x48 0x1400fc640  OWN BridgeTool_Update (103 B)  [in 5 tables]
  [10] +0x50 0x1400268a0  TINY return false (xor al,al)  [in 8 tables]   = parent
  [11] +0x58 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]   = parent
  [12] +0x60 0x1400fc9d0  TINY return 5 (mov eax)  [in 6 tables]
  [13] +0x68 0x14003addc  _purecall  [in 12 tables]
  [14] +0x70 0x14003addc  _purecall  [in 12 tables]
### ToolSetting  table=0x140161ee8  slots=4  sizeof=abstract  parent="Object::Internal" / structural Object::Internal (Runtime ctor import)  stop=zero qword
  reflected: ti=0x1401c6ef0 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014e3d0 ToolSetting_RegisterType
  [ 0] +0x00 0x14002e960  OWN RoadTool_Slot15 (73 B)
  [ 1] +0x08 0x14002e930  TINY returns qword [0x1401c6ef8]
  [ 2] +0x10 0x14002e920  TINY returns dword [0x1401c6ef0] = TypeId(ToolSetting)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
### ResourceProduceComponent  table=0x1401643f0  slots=22  sizeof=abstract  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=non-code qword 0x63614d6574617453 (string "StateMachineDescription")
  reflected: ti=0x1401c7d60 create=0x140003730 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f8d0 ResourceProduceComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x1401643a0
  [ 0] +0x00 0x140037d40  OWN ResourceProduceComponent_Controller_Slot10 (115 B)
  [ 1] +0x08 0x140037d30  TINY returns qword [0x1401c7d68]
  [ 2] +0x10 0x140037d20  TINY returns dword [0x1401c7d60] = TypeId(ResourceProduceComponent)
  [ 3] +0x18 0x14012b4a0  OWN ResourceProduceComponent_Controller_Slot13 (118 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x14012b520  OWN HerbalistComponent_Slot13 (112 B)  [in 6 tables]
  [14] +0x70 0x14012b5a0  OWN HerbalistComponent_Slot14 (117 B)  [in 6 tables]
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  [18] +0x90 0x14003addc  _purecall  [in 12 tables]
  [19] +0x98 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]
  [20] +0xa0 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]
  [21] +0xa8 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]
### StateMachineComponent  table=0x140164520  slots=16  sizeof=0x58  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7d98 create=0x1400382d0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f990 StateMachineComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x1401644d0
  [ 0] +0x00 0x1400384a0  OWN StateMachineComponent_ScalarDtor (105 B)
  [ 1] +0x08 0x1400382c0  TINY returns qword [0x1401c7da0]
  [ 2] +0x10 0x1400382b0  TINY returns dword [0x1401c7d98] = TypeId(StateMachineComponent)
  [ 3] +0x18 0x14012b940  OWN StateMachineComponent_Serialize (144 B)
  [ 4] +0x20 0x14012b6a0  OWN StateMachineComponent_Startup (283 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x14012b660  OWN StateMachineComponent_OnAdd (49 B)
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x14012b640  OWN StateMachineComponent_OnAttach (15 B)
  [ 9] +0x48 0x14012b620  OWN StateMachineComponent_Update (16 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x14012b7d0  OWN StateMachineComponent_OnUpdate2 (351 B)
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
### {StateMachineComponent controller member}  table=0x1401644d0  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140038630  OWN (no Ghidra function)
  [ 1] +0x08 0x140038660  OWN (no Ghidra function)
  [ 2] +0x10 0x1400386c0  OWN (no Ghidra function)
  [ 3] +0x18 0x140038710  OWN (no Ghidra function)
  [ 4] +0x20 0x140038740  OWN (no Ghidra function)
  [ 5] +0x28 0x140038770  OWN (no Ghidra function)
  [ 6] +0x30 0x1400387a0  OWN (no Ghidra function)
  [ 7] +0x38 0x1400387d0  OWN (no Ghidra function)
  [ 8] +0x40 0x140038800  OWN (no Ghidra function)
### SelectResourceUI  table=0x14015d330  slots=14  sizeof=abstract  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x6143657469727053 (string "SpriteCaptureUI")
  reflected: ti=0x1401c5590 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014c6c0 RegisterType_SelectResourceUI
  [ 0] +0x00 0x140013ca0  OWN CropUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140013c90  TINY returns qword [0x1401c5598]
  [ 2] +0x10 0x140013c80  TINY returns dword [0x1401c5590] = TypeId(SelectResourceUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x140078be0  OWN CropUI_Serialize (187 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x140078cb0  OWN CropUI_OnCreate (120 B)
  [ 8] +0x40 0x140078d30  OWN OrchardUI_OnRemove (66 B)  [in 4 tables]
  [ 9] +0x48 0x140078d80  OWN SelectResourceUI_Slot9 (21 B)  [in 3 tables]
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  [11] +0x58 0x14003addc  _purecall  [in 12 tables]
  [12] +0x60 0x14003addc  _purecall  [in 12 tables]
  [13] +0x68 0x14003addc  _purecall  [in 12 tables]
### MessageDialog  table=0x14015c480  slots=14  sizeof=0xc0  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x73776f7242646f4d (string "ModBrowseUI")
  reflected: ti=0x1401c51d8 create=0x1400125d0 createInPlace=0x140012620 Cleanup_MessageDialog registered in 0x14014c280 RegisterType_MessageDialog
  [ 0] +0x00 0x140012660  OWN MessageDialog_ScalarDtor (73 B)
  [ 1] +0x08 0x1400125c0  TINY returns qword [0x1401c51e0]
  [ 2] +0x10 0x1400125b0  TINY returns dword [0x1401c51d8] = TypeId(MessageDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x14006d140  OWN MessageDialog_Serialize (380 B)  [in 2 tables]
  [ 5] +0x28 0x14006d0c0  TINY RET 0 (c2 00 00)  [in 2 tables]
  [ 6] +0x30 0x14003a98a  IMPORT ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ  [in 8 tables]
  [ 7] +0x38 0x14003a990  IMPORT ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z  [in 11 tables]
  [ 8] +0x40 0x14006d070  OWN MessageDialog_OnRemove (59 B)  [in 2 tables]
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [10] +0x50 0x14006cff0  OWN MessageDialog_Slot10 (51 B)  [in 2 tables]
  [11] +0x58 0x14006d030  OWN MessageDialog_Slot11 (51 B)  [in 2 tables]
  [12] +0x60 0x14006cfb0  OWN MessageDialog_Slot12 (51 B)  [in 2 tables]
  [13] +0x68 0x14006cf70  OWN MessageDialog_Slot13 (51 B)  [in 2 tables]
### TutorialGoal  table=0x1401607a0  slots=9  sizeof=abstract  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6c6169726f747554 (string "TutorialGoalButton")
  reflected: ti=0x1401c6438 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014d790 TutorialGoal_RegisterType
  [ 0] +0x00 0x140027220  OWN TutorialGoal_ScalarDtor (73 B)
  [ 1] +0x08 0x140027210  TINY returns qword [0x1401c6440]
  [ 2] +0x10 0x140027200  TINY returns dword [0x1401c6438] = TypeId(TutorialGoal)
  [ 3] +0x18 0x1400dec70  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400dec30  OWN TutorialGoal_Slot5 (46 B)
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]
### TutorialImage  table=0x140160a40  slots=10  sizeof=0x30  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6c6169726f747554 (string "TutorialImageHotKey")
  reflected: ti=0x1401c6588 create=0x140027980 createInPlace=0x1400279e0 TutorialImage_Cleanup registered in 0x14014d910 TutorialImage_RegisterType
  [ 0] +0x00 0x140027a80  OWN TutorialImage_ScalarDtor (73 B)
  [ 1] +0x08 0x140027970  TINY returns qword [0x1401c6590]
  [ 2] +0x10 0x140027960  TINY returns dword [0x1401c6588] = TypeId(TutorialImage)
  [ 3] +0x18 0x1400e03a0  TINY return 4096 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400e0330  OWN TutorialImage_Slot5 (103 B)
  [ 6] +0x30 0x140027a30  OWN TutorialImage_Draw (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400279e0  OWN TutorialImage_Cleanup (68 B)
  [ 9] +0x48 0x1400e0300  OWN TutorialImage_Update (34 B)
### TutorialSetup  table=0x140160c30  slots=11  sizeof=abstract  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6c6169726f747554 (string "TutorialSetupCamera")
  reflected: ti=0x1401c66a0 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014da50 TutorialSetup_RegisterType
  [ 0] +0x00 0x140028390  OWN TutorialSetup_ScalarDtor (73 B)
  [ 1] +0x08 0x140028380  TINY returns qword [0x1401c66a8]
  [ 2] +0x10 0x140028370  TINY returns dword [0x1401c66a0] = TypeId(TutorialSetup)
  [ 3] +0x18 0x1400e2040  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400e2000  OWN TutorialSetup_Slot5 (46 B)
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]
  [ 9] +0x48 0x14003addc  _purecall  [in 12 tables]
  [10] +0x50 0x14003addc  _purecall  [in 12 tables]
```

## §2 — the ControllerComponent family and the embedded controller member

Per class: `slot 17` = the member offset, read from the class's own slot-17 body. `member table` = the table the class's ctor
stamps at that offset after the interface 0x14015f070. The ctor order is: `Component_Ctor` -> stamp ControllerComponent
0x140162b80 at +0 -> stamp the class at +0 -> stamp 0x14015f070 at +off -> stamp the member table at +off -> member back-pointer
at +off+0x18. The dtor runs the reverse (class at +0, 0x14015f070 at +off, 0x140162b80 and 0x140162920 at +0).
`-` in the member-table column = a `ResourceProduceComponent` subclass: its ctor calls the ResourceProduceComponent ctor 0x14012b3d0,
so the member is ResourceProduceComponent's (0x1401643a0). Member layout: `engine/types/ControllerComponent.h` is right for
off=0x28 (+0 vptr, +8, +0x10, +0x18 owner back-pointer — the member's slot 3 reads `MOV RCX,[RCX+0x18]`), 0x20 bytes. The member
slots index a per-class global state table (Attendance: 0x1401c81d0, stride 0xf8), not `this`.


| class | table | parent literal | sizeof | slot 17 (controller member at) | member table (stamped at that offset in the ctor) | slot 16 |
|---|---|---|---|---|---|---|
| AbsorbResourceComponent | 0x140162000 | `ControllerComponent` | 0x48 | this+0x28 | 0x140161fb0 | TINY return 0 (xor eax,eax) |
| AttendanceComponent | 0x140158ce0 | `ControllerComponent` | 0x58 | this+0x38 | 0x140158c90 | TINY return 2 (mov eax) |
| BoatComponent | 0x140158f40 | `ControllerComponent` | 0x98 | this+0x78 | 0x140158ef0 | TINY return 0 (xor eax,eax) |
| BuildComponent | 0x140162260 | `ControllerComponent` | 0x78 | this+0x28 | 0x140162210 | TINY return 1 (mov eax) |
| CemeteryComponent | 0x1401590a0 | `ControllerComponent` | 0x58 | this+0x28 | 0x140159050 | TINY return 2 (mov eax) |
| ClearAreaComponent | 0x1401626d0 | `ControllerComponent` | 0x50 | this+0x28 | 0x140162680 | TINY return 0 (xor eax,eax) |
| ClearResourceComponent | 0x140162840 | `ControllerComponent` | 0x48 | this+0x28 | 0x1401627f0 | TINY return 2 (mov eax) |
| ConsumeProduceComponent | 0x140162a80 | `ResourceProduceComponent` | 0x70 | this+0x28 | - | TINY return 2 (mov eax) |
| CropFieldComponent | 0x140162ec0 | `ControllerComponent` | 0x68 | this+0x28 | 0x140162e70 | TINY return 2 (mov eax) |
| DestroyComponent | 0x140163120 | `ControllerComponent` | 0x68 | this+0x28 | 0x1401630d0 | TINY return 4 (mov eax) |
| FishingComponent | 0x140163230 | `ResourceProduceComponent` | 0x58 | this+0x28 | - | TINY return 2 (mov eax) |
| ForesterComponent | 0x140163460 | `ResourceProduceComponent` | 0x58 | this+0x28 | - | TINY return 2 (mov eax) |
| HappinessComponent | 0x1401595d0 | `ControllerComponent` | 0x50 | this+0x30 | 0x140159580 | TINY return 2 (mov eax) |
| HealerComponent | 0x140159730 | `ControllerComponent` | 0x48 | this+0x28 | 0x1401596e0 | TINY return 2 (mov eax) |
| HerbalistComponent | 0x140159860 | `ResourceProduceComponent` | 0x50 | this+0x28 | - | TINY return 2 (mov eax) |
| HuntComponent | 0x140163770 | `ResourceProduceComponent` | 0x58 | this+0x28 | - | TINY return 2 (mov eax) |
| ImmigrationComponent | 0x140159b30 | `ControllerComponent` | 0x68 | this+0x28 | 0x140159ae0 | TINY return 2 (mov eax) |
| MarketComponent | 0x140159c40 | `ResourceProduceComponent` | 0x50 | this+0x28 | - | TINY return 2 (mov eax) |
| MarketLocationComponent | 0x140159dd0 | `ControllerComponent` | 0x48 | this+0x28 | 0x140159d80 | TINY return 2 (mov eax) |
| MultiObjectComponent | 0x140163cf0 | `ControllerComponent` | 0x60 | this+0x28 | 0x140163ca0 | TINY return 2 (mov eax) |
| NomadComponent | 0x14015a020 | `ControllerComponent` | 0x78 | this+0x58 | 0x140159fd0 | TINY return 0 (xor eax,eax) |
| OrchardComponent | 0x14015a180 | `ControllerComponent` | 0x68 | this+0x28 | 0x14015a130 | TINY return 2 (mov eax) |
| PastureComponent | 0x140163f60 | `ControllerComponent` | 0x68 | this+0x48 | 0x140163f10 | TINY return 2 (mov eax) |
| ResidenceComponent | 0x1401642c0 | `ControllerComponent` | 0x60 | this+0x28 | 0x140164270 | TINY return 2 (mov eax) |
| ResourceProduceComponent | 0x1401643f0 | `ControllerComponent` | ? | this+0x28 | 0x1401643a0 | TINY return 2 (mov eax) |
| StorageLocationComponent | 0x140164880 | `ControllerComponent` | 0x48 | this+0x28 | 0x140164830 | TINY return 2 (mov eax) |
| TradeComponent | 0x14015acd0 | `ControllerComponent` | 0xa8 | this+0x88 | 0x14015ac80 | TINY return 2 (mov eax) |
| WellComponent | 0x14015ae90 | `ControllerComponent` | 0x48 | this+0x28 | 0x14015ae40 | TINY return 2 (mov eax) |
| WorkPlaceComponent | 0x140164d30 | `ControllerComponent` | 0x68 | this+0x28 | 0x140164ce0 | TINY return 0 (xor eax,eax) |
| StateMachineComponent (NOT a ControllerComponent; 16 slots, no slot 17) | 0x140164520 | `Component` | 0x58 | - | 0x1401644d0 at +0x28 | - |

The only other non-zero stamp (raw SECTION S) is `StatusIconComponent` 0x140164620, which stamps 0x14015f0c0 (an exe
`Graphics::Geometry` subclass) at +0x28 — also an embedded member. Nothing else in the exe stores a vtable at a non-zero this
offset (raw SECTION S2 — calls with RCX=this+N into a stamping function — is empty).


## §3 — the 310 reflected exe classes (name literal EXACTLY; saves hash the name)

`cip` = create-in-place: `Internal::CreateInPlaceObject` is the Runtime import `?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z`,
used by every `Object::Internal`-rooted class; External-rooted classes pass their own (`<no fn>` = an exe body Ghidra has no
function for). `size=?` with create 0x140003730 = abstract. There are 13 abstract classes: `Component`, `ControllerComponent`,
`ResourceProduceComponent`, `Description`, `Action`, `Work`, `AutoPickupWork`, `Tool`, `DragTool`, `ToolSetting`,
`SelectResourceUI`, `TutorialGoal` and `TutorialSetup`. The list is in the exe's registration order.

### §3a — parent-literal quirks (copy them exactly; they are what retail registers)
- `DebugUI` has parent `"DialogController"` (no `UI::`), `OptionsDialog` has `"public UI::DialogInstance"` and `Scenario` has
  `"::Achievement::Achievement"`. Every other Runtime parent is spelled `NS::Class`. Not measured here: whether the Runtime's
  parent lookup resolves these three.
- Parent-literal census: Description 78, Component 40, UI::DialogController 35 (+1 `DialogController`), ControllerComponent 23,
  Object::External 19, UI::DialogInstance 18 (+1 `public UI::DialogInstance`), Action 17, Work 16, Object::Internal 7,
  ResourceProduceComponent 6, AutoPickupWork 6, Achievement::Goal 5, DragTool 5, UI::DialogControllerConfig 4, Tool 4,
  TutorialGoal 4, TutorialSetup 4, SelectResourceUI 3, TutorialImage 3, ElementDescription 2, Element 2,
  Achievement::GoalInstance 2, MessageDialog 1, Graphics::AnimationEvent 1, ::Achievement::Achievement 1, Graphics::Particle 1,
  ToolSetting 1 (= 310).
- Retail slot counts the brief asked about: `Scenario` 0x14015f020 has 9 slots; the five `Achievement::Goal` subclasses
  (BuildingGoal, FinishTutorialGoal, PopulationGoal, StatGoal, TimeGoal) have 11; the two GoalInstances have 7; GameUpdateLoop
  has 12.


```
"PlayAnimation"                      parent="Action"                       table=0x140158950 size=0x40   create=0x1400011f0 cip=Internal::CreateInPlaceObject ti=0x1401c3df0
"ShowTool"                           parent="Action"                       table=0x1401589b0 size=0x30   create=0x1400013b0 cip=Internal::CreateInPlaceObject ti=0x1401c3e28
"AmbientEmitterComponent"            parent="Component"                    table=0x140158a60 size=0x48   create=0x1400014b0 cip=Internal::CreateInPlaceObject ti=0x1401c3e60
"AmbientEmitterDescription"          parent="Description"                  table=0x140158ae0 size=0x38   create=0x140001510 cip=0x140001580 AmbientEmitterDescription_CreateInPlace ti=0x1401c3e98
"AmbientEnvironmentComponent"        parent="Component"                    table=0x140158b70 size=0x48   create=0x140001ee0 cip=Internal::CreateInPlaceObject ti=0x1401c3ed0
"AmbientEnvironmentDescription"      parent="Description"                  table=0x140158bf0 size=0x38   create=0x140001f40 cip=0x140001fb0 AmbientEnvironmentDescription_CreateInPlace ti=0x1401c3f08
"AttendanceComponent"                parent="ControllerComponent"          table=0x140158ce0 size=0x58   create=0x140002670 cip=Internal::CreateInPlaceObject ti=0x1401c3f40
"AttendanceDescription"              parent="Description"                  table=0x140158d70 size=0x28   create=0x1400026f0 cip=0x140002750 AttendanceDescription_CreateInPlace ti=0x1401c3f78
"AutoPickupComponent"                parent="Component"                    table=0x140158df0 size=0x40   create=0x1400033c0 cip=Internal::CreateInPlaceObject ti=0x1401c3fb0
"AutoPickupDescription"              parent="Description"                  table=0x140158e70 size=0x28   create=0x140003420 cip=0x140003480 AutoPickupDescription_CreateInPlace ti=0x1401c3fe8
"BoatComponent"                      parent="ControllerComponent"          table=0x140158f40 size=0x98   create=0x1400036e0 cip=Internal::CreateInPlaceObject ti=0x1401c4020
"BoatDescription"                    parent="Description"                  table=0x140158fd0 size=0x20   create=0x140003760 cip=0x1400037c0 BoatDescription_CreateInPlace ti=0x1401c4058
"CemeteryComponent"                  parent="ControllerComponent"          table=0x1401590a0 size=0x58   create=0x140004040 cip=Internal::CreateInPlaceObject ti=0x1401c4090
"CemeteryDescription"                parent="Description"                  table=0x140159130 size=0x28   create=0x1400040b0 cip=0x140004110 CemeteryDescription_CreateInPlace ti=0x1401c40c8
"CreateFencedDescription"            parent="Description"                  table=0x1401591a0 size=0x78   create=0x1400048e0 cip=0x140004920 CreateFencedDescription_CreateInPlace ti=0x1401c4100
"CreatePathDescription"              parent="Description"                  table=0x140159210 size=0x20   create=0x140004c30 cip=0x140004c90 CreatePathDescription_CreateInPlace ti=0x1401c4138
"DiseaseComponent"                   parent="Component"                    table=0x140159290 size=0x58   create=0x140004db0 cip=Internal::CreateInPlaceObject ti=0x1401c4170
"DiseaseDescription"                 parent="Description"                  table=0x140159310 size=0x48   create=0x140004e10 cip=0x140004e70 DiseaseDescription_CreateInPlace ti=0x1401c41a8
"FireComponent"                      parent="Component"                    table=0x140159380 size=0x48   create=0x1400055e0 cip=Internal::CreateInPlaceObject ti=0x1401c41e0
"FireDescription"                    parent="Description"                  table=0x140159400 size=0x50   create=0x140005640 cip=0x140005680 FireDescription_CreateInPlace ti=0x1401c4218
"FruitComponent"                     parent="Component"                    table=0x140159450 size=0x38   create=0x140005920 cip=Internal::CreateInPlaceObject ti=0x1401c4250
"FruitDescription"                   parent="Description"                  table=0x1401594d0 size=0x38   create=0x140005980 cip=0x1400059f0 FruitDescription_CreateInPlace ti=0x1401c4288
"HappinessComponent"                 parent="ControllerComponent"          table=0x1401595d0 size=0x50   create=0x140005ba0 cip=Internal::CreateInPlaceObject ti=0x1401c42c0
"HappinessDescription"               parent="Description"                  table=0x140159660 size=0x30   create=0x140005c10 cip=0x140005c80 HappinessDescription_CreateInPlace ti=0x1401c42f8
"HealerComponent"                    parent="ControllerComponent"          table=0x140159730 size=0x48   create=0x140006490 cip=Internal::CreateInPlaceObject ti=0x1401c4330
"HealerDescription"                  parent="Description"                  table=0x1401597c0 size=0x20   create=0x1400064f0 cip=0x140006550 HealerDescription_CreateInPlace ti=0x1401c4368
"HerbalistComponent"                 parent="ResourceProduceComponent"     table=0x140159860 size=0x50   create=0x140006b40 cip=Internal::CreateInPlaceObject ti=0x1401c43a0
"HerbalistDescription"               parent="Description"                  table=0x140159910 size=0x38   create=0x140006bb0 cip=0x140006c20 HerbalistDescription_CreateInPlace ti=0x1401c43d8
"HighlightComponent"                 parent="Component"                    table=0x140159990 size=0x128  create=0x140006e40 cip=Internal::CreateInPlaceObject ti=0x1401c4410
"HighlightDescription"               parent="Description"                  table=0x140159a10 size=0x40   create=0x140006ea0 cip=0x140006f10 HighlightDescription_CreateInPlace ti=0x1401c4448
"ImmigrationComponent"               parent="ControllerComponent"          table=0x140159b30 size=0x68   create=0x140007a20 cip=Internal::CreateInPlaceObject ti=0x1401c4480
"ImmigrationDescription"             parent="Description"                  table=0x140159bc0 size=0x68   create=0x140007a80 cip=0x140007ac0 ImmigrationDescription_CreateInPlace ti=0x1401c44b8
"MarketComponent"                    parent="ResourceProduceComponent"     table=0x140159c40 size=0x50   create=0x140008220 cip=Internal::CreateInPlaceObject ti=0x1401c44f0
"MarketDescription"                  parent="Description"                  table=0x140159cf0 size=0x38   create=0x140008290 cip=0x140008300 MarketDescription_CreateInPlace ti=0x1401c4528
"MarketLocationComponent"            parent="ControllerComponent"          table=0x140159dd0 size=0x48   create=0x1400086c0 cip=Internal::CreateInPlaceObject ti=0x1401c4560
"MarketLocationDescription"          parent="Description"                  table=0x140159e60 size=0x20   create=0x140008720 cip=0x140008780 MarketLocationDescription_CreateInPlace ti=0x1401c4598
"MineComponent"                      parent="Component"                    table=0x140159ed0 size=0x28   create=0x140008dd0 cip=Internal::CreateInPlaceObject ti=0x1401c45d0
"MineDescription"                    parent="Description"                  table=0x140159f50 size=0x20   create=0x140008e30 cip=0x140008e90 MineDescription_CreateInPlace ti=0x1401c4608
"NomadComponent"                     parent="ControllerComponent"          table=0x14015a020 size=0x78   create=0x140009020 cip=Internal::CreateInPlaceObject ti=0x1401c4640
"NomadDescription"                   parent="Description"                  table=0x14015a0b0 size=0x38   create=0x140009090 cip=0x140009100 NomadDescription_CreateInPlace ti=0x1401c4678
"OrchardComponent"                   parent="ControllerComponent"          table=0x14015a180 size=0x68   create=0x140009870 cip=Internal::CreateInPlaceObject ti=0x1401c46b0
"OrchardDescription"                 parent="Description"                  table=0x14015a210 size=0x30   create=0x1400098d0 cip=0x140009940 OrchardDescription_CreateInPlace ti=0x1401c46e8
"ParticleComponent"                  parent="Component"                    table=0x14015a290 size=0x48   create=0x14000a360 cip=Internal::CreateInPlaceObject ti=0x1401c4720
"ParticleDescription"                parent="Description"                  table=0x14015a310 size=0x38   create=0x14000a3c0 cip=0x14000a430 ParticleDescription_CreateInPlace ti=0x1401c4758
"PopulationComponent"                parent="Component"                    table=0x14015a360 size=0x70   create=0x14000af50 cip=Internal::CreateInPlaceObject ti=0x1401c4790
"PopulationDescription"              parent="Description"                  table=0x14015a3e0 size=0x58   create=0x14000afb0 cip=0x14000aff0 PopulationDescription_CreateInPlace ti=0x1401c47c8
"QuarryComponent"                    parent="Component"                    table=0x14015a490 size=0x28   create=0x14000b3d0 cip=Internal::CreateInPlaceObject ti=0x1401c4800
"QuarryDescription"                  parent="Description"                  table=0x14015a510 size=0x30   create=0x14000b430 cip=0x14000b4a0 QuarryDescription_CreateInPlace ti=0x1401c4838
"RadiusDescription"                  parent="Description"                  table=0x14015a580 size=0x30   create=0x14000b9e0 cip=0x14000ba50 RadiusDescription_CreateInPlace ti=0x1401c4870
"SelectDescription"                  parent="Description"                  table=0x14015a5f0 size=0x48   create=0x14000bb90 cip=0x14000bc00 SelectDescription_CreateInPlace ti=0x1401c48a8
"SpawnComponent"                     parent="Component"                    table=0x14015a670 size=0x38   create=0x14000c170 cip=Internal::CreateInPlaceObject ti=0x1401c48e0
"SpawnDescription"                   parent="Description"                  table=0x14015a6f0 size=0x30   create=0x14000c1d0 cip=0x14000c230 SpawnDescription_CreateInPlace ti=0x1401c4918
"StorageDisplayComponent"            parent="Component"                    table=0x14015a780 size=0x38   create=0x14000c770 cip=Internal::CreateInPlaceObject ti=0x1401c4950
"StorageDisplayDescription"          parent="Description"                  table=0x14015a800 size=0x20   create=0x14000c7d0 cip=0x14000c830 StorageDisplayDescription_CreateInPlace ti=0x1401c4988
"TerraformComponent"                 parent="Component"                    table=0x14015a880 size=0x48   create=0x14000cc90 cip=Internal::CreateInPlaceObject ti=0x1401c49c0
"TerraformDescription"               parent="Description"                  table=0x14015a900 size=0x38   create=0x14000ccf0 cip=0x14000cd60 TerraformDescription_CreateInPlace ti=0x1401c49f8
"TombstoneComponent"                 parent="Component"                    table=0x14015a980 size=0x30   create=0x14000d280 cip=Internal::CreateInPlaceObject ti=0x1401c4a30
"TombstoneDescription"               parent="Description"                  table=0x14015aa00 size=0x28   create=0x14000d2e0 cip=0x14000d350 TombstoneDescription_CreateInPlace ti=0x1401c4a68
"TornadoDescription"                 parent="Description"                  table=0x14015ab00 size=0x38   create=0x14000d560 cip=0x14000d5a0 TornadoDescription_CreateInPlace ti=0x1401c4aa0
"TornadoComponent"                   parent="Component"                    table=0x14015aa80 size=0x68   create=0x14000d500 cip=Internal::CreateInPlaceObject ti=0x1401c4ad8
"TrackerComponent"                   parent="Component"                    table=0x14015ab80 size=0x50   create=0x14000d7a0 cip=Internal::CreateInPlaceObject ti=0x1401c4b10
"TrackerDescription"                 parent="Description"                  table=0x14015ac00 size=0x20   create=0x14000d830 cip=0x14000d890 TrackerDescription_CreateInPlace ti=0x1401c4b48
"TradeComponent"                     parent="ControllerComponent"          table=0x14015acd0 size=0xa8   create=0x14000db60 cip=Internal::CreateInPlaceObject ti=0x1401c4b80
"TradeDescription"                   parent="Description"                  table=0x14015ad60 size=0x70   create=0x14000dbd0 cip=0x14000dc10 TradeDescription_CreateInPlace ti=0x1401c4bb8
"ValueDescription"                   parent="Description"                  table=0x14015add0 size=0x50   create=0x14000faa0 cip=0x14000fb10 ValueDescription_CreateInPlace ti=0x1401c4bf0
"WellComponent"                      parent="ControllerComponent"          table=0x14015ae90 size=0x48   create=0x14000fc80 cip=Internal::CreateInPlaceObject ti=0x1401c4c28
"WellDescription"                    parent="Description"                  table=0x14015af20 size=0x20   create=0x14000fce0 cip=0x14000fd40 WellDescription_CreateInPlace ti=0x1401c4c60
"AchievementDialog"                  parent="UI::DialogInstance"           table=0x14015b020 size=0xd0   create=0x140010320 cip=0x140010390 Cleanup_AchievementDialog ti=0x1401c4c98
"AttendanceUI"                       parent="UI::DialogController"         table=0x14015b070 size=0x38   create=0x1400104b0 cip=0x140010510 Cleanup_AttendanceUI ti=0x1401c4cd0
"AutoSaveDialog"                     parent="UI::DialogInstance"           table=0x14015b120 size=0xd0   create=0x1400106c0 cip=0x140010730 Cleanup_AutoSaveDialog ti=0x1401c4d08
"BuildRequirementUI"                 parent="UI::DialogController"         table=0x14015b1d0 size=0x60   create=0x1400107f0 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c4d40
"CemeteryUI"                         parent="UI::DialogController"         table=0x14015b240 size=0x38   create=0x140010900 cip=0x140010960 Cleanup_CemeteryUI ti=0x1401c4d78
"ConsumeProduceUI"                   parent="UI::DialogController"         table=0x14015b2e0 size=0x40   create=0x140010ae0 cip=0x140010b40 Cleanup_ConsumeProduceUI ti=0x1401c4db0
"ControlUI"                          parent="UI::DialogController"         table=0x14015b380 size=0x40   create=0x140010c00 cip=0x140010c60 Cleanup_ControlUI ti=0x1401c4de8
"ControlUIConfig"                    parent="UI::DialogControllerConfig"   table=0x14015b3e0 size=0x28   create=0x140010cd0 cip=0x140010d30 ControlUIConfig_CreateInPlace ti=0x1401c4e20
"DebugUI"                            parent="DialogController"             table=0x14015b870 size=0x130  create=0x140010ec0 cip=0x140010f00 <no fn> ti=0x1401c4e58
"CitizenUI"                          parent="UI::DialogController"         table=0x14015ba00 size=0xe8   create=0x140010f80 cip=0x140010fc0 <no fn> ti=0x1401c4e90
"DestroyUI"                          parent="UI::DialogController"         table=0x14015bb00 size=0x78   create=0x140011130 cip=0x1400111a0 Cleanup_DestroyUI ti=0x1401c4ec8
"EntityUI"                           parent="UI::DialogController"         table=0x14015bb90 size=0x48   create=0x140011270 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c4f00
"EventLogUI"                         parent="UI::DialogController"         table=0x14015bc40 size=0x80   create=0x1400113e0 cip=0x140011450 Cleanup_EventLogUI ti=0x1401c4f38
"EventNotifyDialog"                  parent="UI::DialogInstance"           table=0x14015bca0 size=0xf0   create=0x140011500 cip=0x140011540 <no fn> ti=0x1401c4f70
"EventPopupDialog"                   parent="UI::DialogInstance"           table=0x14015bd80 size=0xc0   create=0x140011920 cip=0x140011970 Cleanup_EventPopupDialog ti=0x1401c4fa8
"FollowDialog"                       parent="UI::DialogInstance"           table=0x14015be00 size=0xd8   create=0x140011a20 cip=0x140011a90 Cleanup_FollowDialog ti=0x1401c4fe0
"GoalDialog"                         parent="UI::DialogInstance"           table=0x14015beb0 size=0xc8   create=0x140011b60 cip=0x140011bc0 Cleanup_GoalDialog ti=0x1401c5018
"HelpUI"                             parent="UI::DialogController"         table=0x14015bf30 size=0x78   create=0x140011ca0 cip=0x140011ce0 <no fn> ti=0x1401c5050
"HotKeyToolTipUI"                    parent="UI::DialogController"         table=0x14015bfc0 size=0x30   create=0x140011e20 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c5088
"ImmigrationUI"                      parent="UI::DialogController"         table=0x14015c080 size=0x50   create=0x140011ee0 cip=0x140011f40 Cleanup_ImmigrationUI ti=0x1401c50c0
"MapParamsDialog"                    parent="UI::DialogInstance"           table=0x14015c180 size=0xf8   create=0x140012100 cip=0x140012140 <no fn> ti=0x1401c50f8
"MapUI"                              parent="UI::DialogController"         table=0x14015c350 size=0x50   create=0x140012240 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c5130
"FrustumOutlineDescription"          parent="ElementDescription"           table=0x14015c300 size=0x80   create=0x1400122c0 cip=0x140012330 FrustumOutlineDescription_CreateInPlace ti=0x1401c5168
"FrustumOutlineElement"              parent="Element"                      table=0x14015c2a0 size=0xd8   create=0x140012400 cip=0x140012470 FrustumOutlineElement_Cleanup ti=0x1401c51a0
"MessageDialog"                      parent="UI::DialogInstance"           table=0x14015c480 size=0xc0   create=0x1400125d0 cip=0x140012620 Cleanup_MessageDialog ti=0x1401c51d8
"ModBrowseUI"                        parent="UI::DialogController"         table=0x14015c500 size=0x30   create=0x1400126d0 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c5210
"ModDetailsDialog"                   parent="UI::DialogInstance"           table=0x14015c670 size=0xc8   create=0x140012790 cip=0x1400127e0 Cleanup_ModDetailsDialog ti=0x1401c5248
"ModDialog"                          parent="UI::DialogInstance"           table=0x14015c870 size=0xd8   create=0x140012900 cip=0x140012970 Cleanup_ModDialog ti=0x1401c5280
"ModUploadDialog"                    parent="UI::DialogInstance"           table=0x14015c6c0 size=0xb8   create=0x140012bc0 cip=0x140012c10 Cleanup_ModUploadDialog ti=0x1401c52b8
"NewGameDialog"                      parent="UI::DialogInstance"           table=0x14015c920 size=0xae8  create=0x140012c70 cip=0x140012cb0 <no fn> ti=0x1401c52f0
"NoiseUI"                            parent="UI::DialogController"         table=0x14015ca80 size=0x70   create=0x140012d30 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c5328
"OptionsDialog"                      parent="public UI::DialogInstance"    table=0x14015ceb0 size=0x1c0  create=0x140012df0 cip=0x140012e30 <no fn> ti=0x1401c5360
"OptionInputBindDialog"              parent="MessageDialog"                table=0x14015ce40 size=0xc8   create=0x140012e60 cip=0x140012ed0 Cleanup_OptionInputBindDialog ti=0x1401c5398
"OrchardUI"                          parent="SelectResourceUI"             table=0x14015cf30 size=0x48   create=0x140012ff0 cip=0x140013070 Cleanup_OrchardUI ti=0x1401c53d0
"PageToggleUI"                       parent="UI::DialogController"         table=0x14015cfb0 size=0x48   create=0x140013160 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c5408
"ProductionUI"                       parent="UI::DialogController"         table=0x14015d070 size=0x50   create=0x140013230 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c5440
"ProfessionUI"                       parent="UI::DialogController"         table=0x14015d0f0 size=0x78   create=0x140013400 cip=0x140013440 <no fn> ti=0x1401c5478
"RadiusUI"                           parent="UI::DialogController"         table=0x14015d160 size=0x38   create=0x140013590 cip=0x1400135f0 Cleanup_RadiusUI ti=0x1401c54b0
"ResourceLimitUI"                    parent="UI::DialogController"         table=0x14015d1f0 size=0x40   create=0x1400136b0 cip=0x140013710 Cleanup_ResourceLimitUI ti=0x1401c54e8
"ResourceLimitUIConfig"              parent="UI::DialogControllerConfig"   table=0x14015d250 size=0x28   create=0x140013780 cip=0x1400137e0 ResourceLimitUIConfig_CreateInPlace ti=0x1401c5520
"ScenarioDialog"                     parent="UI::DialogInstance"           table=0x14015d2d0 size=0xe8   create=0x140013970 cip=0x1400139f0 Cleanup_ScenarioDialog ti=0x1401c5558
"SelectResourceUI"                   parent="UI::DialogController"         table=0x14015d330 size=?      create=0x140003730 cip=0x140003730 AutoPickupWork_Factory_Abstract ti=0x1401c5590
"SpriteCaptureUI"                    parent="UI::DialogController"         table=0x14015d4f0 size=0x70   create=0x140013d10 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c55c8
"CaptureBoxDescription"              parent="ElementDescription"           table=0x14015d4a0 size=0x78   create=0x140013d90 cip=0x140013df0 CaptureBoxDescription_CreateInPlace ti=0x1401c5600
"CaptureBoxElement"                  parent="Element"                      table=0x14015d440 size=0xe0   create=0x140013eb0 cip=0x140013f20 CaptureBoxElement_Cleanup ti=0x1401c5638
"StatusBarUI"                        parent="UI::DialogController"         table=0x14015d810 size=0x1d0  create=0x1400140e0 cip=0x140014120 <no fn> ti=0x1401c5670
"StatusIconUI"                       parent="UI::DialogController"         table=0x14015d8a0 size=0x40   create=0x140014180 cip=0x1400141e0 Cleanup_StatusIconUI ti=0x1401c56a8
"StatusIconUIConfig"                 parent="UI::DialogControllerConfig"   table=0x14015d900 size=0x28   create=0x140014250 cip=0x1400142a0 StatusIconUIConfig_CreateInPlace ti=0x1401c56e0
"StorageCapacityUI"                  parent="UI::DialogController"         table=0x14015d950 size=0x38   create=0x1400143f0 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c5718
"TitleDescToolTipUI"                 parent="UI::DialogController"         table=0x14015d9f0 size=0x30   create=0x1400144c0 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c5750
"ToolUI"                             parent="UI::DialogController"         table=0x14015da70 size=0x58   create=0x140014580 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c5788
"TownHallUI"                         parent="UI::DialogController"         table=0x14015dce0 size=0x1e8  create=0x140014660 cip=0x1400146a0 <no fn> ti=0x1401c57c0
"TradeUI"                            parent="UI::DialogController"         table=0x14015de80 size=0x148  create=0x140015f80 cip=0x140015fc0 <no fn> ti=0x1401c57f8
"UpgradeUI"                          parent="UI::DialogController"         table=0x14015df00 size=0x38   create=0x140016b30 cip=0x140016b90 Cleanup_UpgradeUI ti=0x1401c5830
"UpgradeUIConfig"                    parent="UI::DialogControllerConfig"   table=0x14015df60 size=0x28   create=0x140016c00 cip=0x140016c60 UpgradeUIConfig_CreateInPlace ti=0x1401c5868
"WorkPlaceUI"                        parent="UI::DialogController"         table=0x14015e030 size=0x68   create=0x140016dc0 cip=0x140016e30 Cleanup_WorkPlaceUI ti=0x1401c58a0
"AnimAudioEvent"                     parent="Graphics::AnimationEvent"     table=0x14015e0c0 size=0x30   create=0x140016f00 cip=0x140016f60 AnimAudioEvent_RegisterType_sub_6f60 ti=0x1401c58d8
"ClimateGenerator"                   parent="Object::External"             table=0x14015e140 size=0x110  create=0x140018ef0 cip=0x140018f30 ClimateGenerator_Cleanup ti=0x1401c5910
"Disease"                            parent="Object::External"             table=0x14015e190 size=0x48   create=0x140018fd0 cip=0x140019040 ClimateGenerator_vslot18 ti=0x1401c5948
"EnvironmentDescription"             parent="Object::External"             table=0x14015e1f0 size=0x1b8  create=0x140019120 cip=0x140019160 EnvironmentDescription_CreateInPlace ti=0x1401c5980
"MapGenerator"                       parent="Object::External"             table=0x14015ed80 size=0x178  create=0x14001ade0 cip=0x14001ae20 MapGenerator_Cleanup ti=0x1401c59b8
"MenuScene"                          parent="Object::External"             table=0x14015ede0 size=0x208  create=0x14001b7c0 cip=0x14001b800 MenuScene_OnRemove ti=0x1401c59f0
"MusicList"                          parent="Object::External"             table=0x14015ee40 size=0x30   create=0x14001c260 cip=0x14001c2c0 MenuScene_StaticInit_sub_c2c0 ti=0x1401c5a28
"NameDatabase"                       parent="Object::External"             table=0x14015eea0 size=0x60   create=0x14001c660 cip=0x14001c6d0 MenuScene_Setup_sub_c6d0 ti=0x1401c5a60
"Profession"                         parent="Object::External"             table=0x14015efa0 size=0x60   create=0x14001e530 cip=0x14001e570 Profession_OnRemove ti=0x1401c5a98
"ProfessionList"                     parent="Object::External"             table=0x14015ef50 size=0x50   create=0x14001e5c0 cip=0x14001e630 ProfessionList_Cleanup ti=0x1401c5ad0
"Scenario"                           parent="::Achievement::Achievement"   table=0x14015f020 size=0x90   create=0x14001e9f0 cip=0x14001ea70 Scenario_Cleanup ti=0x1401c5b08
"TerrainGenerator"                   parent="Object::External"             table=0x14015f170 size=0x90   create=0x140020590 cip=0x1400205d0 TerrainGenerator_Cleanup ti=0x1401c5b40
"BuildUI"                            parent="UI::DialogController"         table=0x14015f210 size=0x50   create=0x1400209e0 cip=0x140020a40 Cleanup_BuildUI ti=0x1401c5b78
"CropUI"                             parent="SelectResourceUI"             table=0x14015f270 size=0x60   create=0x140020b10 cip=0x140020b50 <no fn> ti=0x1401c5bb0
"LoaderDialog"                       parent="UI::DialogInstance"           table=0x14015f300 size=0xc8   create=0x140020bb0 cip=0x140020c20 Cleanup_LoaderDialog ti=0x1401c5be8
"MainMenuDialog"                     parent="UI::DialogInstance"           table=0x14015f550 size=0xc8   create=0x140020d10 cip=0x140020d60 Cleanup_MainMenuDialog ti=0x1401c5c20
"PastureUI"                          parent="SelectResourceUI"             table=0x14015f610 size=0x40   create=0x140020e10 cip=0x140020e80 Cleanup_PastureUI ti=0x1401c5c58
"ResidenceUI"                        parent="UI::DialogController"         table=0x14015f6d0 size=0x40   create=0x140020f60 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c5c90
"SaveLoadDialog"                     parent="UI::DialogInstance"           table=0x14015f900 size=0x160  create=0x140021160 cip=0x1400211a0 <no fn> ti=0x1401c5cc8
"StorageUI"                          parent="UI::DialogController"         table=0x14015f970 size=0x40   create=0x140021330 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c5d00
"ToolbarDialog"                      parent="UI::DialogInstance"           table=0x14015fa00 size=0x108  create=0x140021710 cip=0x140021750 <no fn> ti=0x1401c5d38
"ComponentEntity"                    parent="Object::Internal"             table=0x14015fa78 size=0x48   create=0x1400219c0 cip=Internal::CreateInPlaceObject ti=0x1401c5d70
"Action"                             parent="Object::Internal"             table=0x14015faa0 size=?      create=0x140003730 cip=Internal::CreateInPlaceObject ti=0x1401c5da8
"AnimateNaturalResource"             parent="Action"                       table=0x14015fb10 size=0x30   create=0x140021dc0 cip=Internal::CreateInPlaceObject ti=0x1401c5de0
"CompleteRoad"                       parent="Action"                       table=0x14015fb70 size=0x30   create=0x140021ec0 cip=Internal::CreateInPlaceObject ti=0x1401c5e18
"CompleteTask"                       parent="Action"                       table=0x14015fbd0 size=0x38   create=0x140021fc0 cip=Internal::CreateInPlaceObject ti=0x1401c5e50
"ConsumeRawMaterial"                 parent="Action"                       table=0x14015fc20 size=0x40   create=0x1400220c0 cip=Internal::CreateInPlaceObject ti=0x1401c5e88
"CreateNaturalResource"              parent="Action"                       table=0x14015fca0 size=0x38   create=0x1400221d0 cip=Internal::CreateInPlaceObject ti=0x1401c5ec0
"CreateRawMaterial"                  parent="Action"                       table=0x14015fd10 size=0x58   create=0x1400222d0 cip=Internal::CreateInPlaceObject ti=0x1401c5ef8
"DropOffRawMaterial"                 parent="Action"                       table=0x14015fd80 size=0x40   create=0x140022600 cip=Internal::CreateInPlaceObject ti=0x1401c5f30
"HuntAnimal"                         parent="Action"                       table=0x14015fde0 size=0x40   create=0x140022700 cip=Internal::CreateInPlaceObject ti=0x1401c5f68
"MoveTo"                             parent="Action"                       table=0x14015fe40 size=0x480  create=0x1400228c0 cip=Internal::CreateInPlaceObject ti=0x1401c5fa0
"PerformWork"                        parent="Action"                       table=0x14015fea0 size=0x48   create=0x140022b90 cip=Internal::CreateInPlaceObject ti=0x1401c5fd8
"PickupRawMaterial"                  parent="Action"                       table=0x14015ff10 size=0x30   create=0x140022c60 cip=Internal::CreateInPlaceObject ti=0x1401c6010
"PickupStoredRawMaterial"            parent="Action"                       table=0x14015ff80 size=0x48   create=0x140022d60 cip=Internal::CreateInPlaceObject ti=0x1401c6048
"RemoveAnimal"                       parent="Action"                       table=0x14015ffe0 size=0x28   create=0x140022e70 cip=Internal::CreateInPlaceObject ti=0x1401c6080
"RemoveNaturalResource"              parent="Action"                       table=0x140160050 size=0x38   create=0x140022f70 cip=Internal::CreateInPlaceObject ti=0x1401c60b8
"SendComponentMessage"               parent="Action"                       table=0x1401600c0 size=0x40   create=0x140023070 cip=Internal::CreateInPlaceObject ti=0x1401c60f0
"Toolbar"                            parent="Object::External"             table=0x140160120 size=0x68   create=0x140024ba0 cip=0x140024be0 Toolbar_Cleanup ti=0x1401c6128
"Tornado"                            parent="Graphics::Particle"           table=0x140160190 size=0x90   create=0x140024e90 cip=0x140024ed0 <no fn> ti=0x1401c6160
"Weather"                            parent="Object::External"             table=0x140160290 size=0x80   create=0x140024fb0 cip=0x140024ff0 Weather_OnRemove ti=0x1401c6198
"BuildingGoal"                       parent="Achievement::Goal"            table=0x140160300 size=0x48   create=0x140025d80 cip=0x140025de0 BuildingGoal_Cleanup ti=0x1401c61d0
"PopulationGoal"                     parent="Achievement::Goal"            table=0x140160370 size=0x38   create=0x140025f00 cip=0x140025f60 PopulationGoal_Cleanup ti=0x1401c6208
"FinishTutorialGoal"                 parent="Achievement::Goal"            table=0x140160420 size=0x38   create=0x1400260f0 cip=0x140026150 FinishTutorialGoal_Cleanup ti=0x1401c6240
"FinishTutorialInstance"             parent="Achievement::GoalInstance"    table=0x140160478 size=0x28   create=0x140026070 cip=Internal::CreateInPlaceObject ti=0x1401c6278
"StatGoal"                           parent="Achievement::Goal"            table=0x1401604c0 size=0x80   create=0x1400264d0 cip=0x140026510 <no fn> ti=0x1401c62b0
"TimeGoal"                           parent="Achievement::Goal"            table=0x140160540 size=0x38   create=0x1400266f0 cip=0x140026750 TimeGoal_Cleanup ti=0x1401c62e8
"TimeGoalInstance"                   parent="Achievement::GoalInstance"    table=0x140160598 size=0x28   create=0x140026670 cip=Internal::CreateInPlaceObject ti=0x1401c6320
"FencedTool"                         parent="DragTool"                     table=0x1401605d0 size=0x70   create=0x1400268d0 cip=0x140026910 <no fn> ti=0x1401c6358
"PathTool"                           parent="Tool"                         table=0x140160680 size=0x60   create=0x140026b10 cip=0x140026b50 <no fn> ti=0x1401c6390
"RotationSetting"                    parent="ToolSetting"                  table=0x140160710 size=0x18   create=0x140026d20 cip=0x140026d80 RotationSetting_Cleanup ti=0x1401c63c8
"Tutorial"                           parent="Object::External"             table=0x140160740 size=0x40   create=0x140026e50 cip=0x140026eb0 Tutorial_Cleanup ti=0x1401c6400
"TutorialGoal"                       parent="Object::External"             table=0x1401607a0 size=?      create=0x140003730 cip=0x140003730 AutoPickupWork_Factory_Abstract ti=0x1401c6438
"TutorialGoalButton"                 parent="TutorialGoal"                 table=0x140160800 size=0x30   create=0x140027290 cip=0x140027300 TutorialGoalButton_Cleanup ti=0x1401c6470
"TutorialGoalDialog"                 parent="TutorialGoal"                 table=0x140160870 size=0x40   create=0x140027440 cip=0x1400274b0 Cleanup_TutorialGoalDialog ti=0x1401c64a8
"TutorialGoalGameSpeed"              parent="TutorialGoal"                 table=0x1401608e0 size=0x28   create=0x140027630 cip=0x140027690 TutorialGoalGameSpeed_Cleanup ti=0x1401c64e0
"TutorialGoalCount"                  parent="TutorialGoal"                 table=0x140160950 size=0x50   create=0x1400277b0 cip=0x1400277f0 TutorialGoalCount_Update ti=0x1401c6518
"TutorialHighlightDialog"            parent="UI::DialogInstance"           table=0x1401609e0 size=0xf0   create=0x1400278c0 cip=0x140027900 <no fn> ti=0x1401c6550
"TutorialImage"                      parent="Object::External"             table=0x140160a40 size=0x30   create=0x140027980 cip=0x1400279e0 TutorialImage_Cleanup ti=0x1401c6588
"TutorialImageHotKey"                parent="TutorialImage"                table=0x140160ab0 size=0x48   create=0x140027d00 cip=0x140027d40 TutorialImageHotKey_OnRemove ti=0x1401c65c0
"TutorialImageInputKey"              parent="TutorialImage"                table=0x140160b20 size=0x38   create=0x140027ea0 cip=0x140027f00 TutorialImageInputKey_Cleanup ti=0x1401c65f8
"TutorialImageToolbarIcon"           parent="TutorialImage"                table=0x140160b90 size=0x48   create=0x140028020 cip=0x140028060 TutorialImageToolbarIcon_OnRemove ti=0x1401c6630
"TutorialInstance"                   parent="Object::Internal"             table=0x140160bf8 size=0x38   create=0x140028120 cip=Internal::CreateInPlaceObject ti=0x1401c6668
"TutorialSetup"                      parent="Object::External"             table=0x140160c30 size=?      create=0x140003730 cip=0x140003730 AutoPickupWork_Factory_Abstract ti=0x1401c66a0
"TutorialSetupCamera"                parent="TutorialSetup"                table=0x140160ca0 size=0x30   create=0x140028400 cip=0x140028470 TutorialSetupCamera_Cleanup ti=0x1401c66d8
"TutorialSetupDialog"                parent="TutorialSetup"                table=0x140160d20 size=0x78   create=0x1400285b0 cip=0x1400285f0 TutorialSetupDialog_Slot11 ti=0x1401c6710
"TutorialSetupGameSpeed"             parent="TutorialSetup"                table=0x140160da0 size=0x28   create=0x1400289c0 cip=0x140028a20 TutorialSetupGameSpeed_Cleanup ti=0x1401c6748
"TutorialSetupRestrictPlayer"        parent="TutorialSetup"                table=0x140160e20 size=0x68   create=0x140028b40 cip=0x140028b80 TutorialSetupRestrictPlayer_Slot11 ti=0x1401c6780
"TutorialStep"                       parent="Object::External"             table=0x140160e90 size=0x70   create=0x140028c50 cip=0x140028c90 TutorialStep_OnRemove ti=0x1401c67b8
"TutorialUI"                         parent="UI::DialogController"         table=0x140160f30 size=0x60   create=0x140029400 cip=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z ti=0x1401c67f0
"AutoPickupWork"                     parent="Work"                         table=0x140160fa0 size=?      create=0x140003730 cip=Internal::CreateInPlaceObject ti=0x1401c6828
"BuildRoad"                          parent="Work"                         table=0x140161020 size=0x70   create=0x140029560 cip=Internal::CreateInPlaceObject ti=0x1401c6860
"BuildStructure"                     parent="Work"                         table=0x1401610a0 size=0x70   create=0x140029630 cip=Internal::CreateInPlaceObject ti=0x1401c6898
"ClearArea"                          parent="Work"                         table=0x140161110 size=0x78   create=0x140029820 cip=Internal::CreateInPlaceObject ti=0x1401c68d0
"ClearStorage"                       parent="Work"                         table=0x1401611a0 size=0x68   create=0x14002a0b0 cip=Internal::CreateInPlaceObject ti=0x1401c6908
"ConsumeProduce"                     parent="Work"                         table=0x140161220 size=0x78   create=0x14002a150 cip=Internal::CreateInPlaceObject ti=0x1401c6940
"FireWork"                           parent="Work"                         table=0x1401612a0 size=0x60   create=0x14002a1e0 cip=Internal::CreateInPlaceObject ti=0x1401c6978
"FishingWork"                        parent="Work"                         table=0x140161320 size=0x70   create=0x14002a280 cip=Internal::CreateInPlaceObject ti=0x1401c69b0
"ForesterWork"                       parent="AutoPickupWork"               table=0x1401613a0 size=0x60   create=0x14002a310 cip=Internal::CreateInPlaceObject ti=0x1401c69e8
"GatherRawMaterials"                 parent="Work"                         table=0x140161420 size=0x78   create=0x14002a3c0 cip=Internal::CreateInPlaceObject ti=0x1401c6a20
"HarvestArea"                        parent="AutoPickupWork"               table=0x1401614a0 size=0x68   create=0x14002a460 cip=Internal::CreateInPlaceObject ti=0x1401c6a58
"HarvestOrchard"                     parent="AutoPickupWork"               table=0x140161520 size=0x68   create=0x14002a520 cip=Internal::CreateInPlaceObject ti=0x1401c6a90
"HerbalistWork"                      parent="AutoPickupWork"               table=0x1401615a0 size=0x60   create=0x14002a5e0 cip=Internal::CreateInPlaceObject ti=0x1401c6ac8
"HuntWork"                           parent="AutoPickupWork"               table=0x140161620 size=0x58   create=0x14002a690 cip=Internal::CreateInPlaceObject ti=0x1401c6b00
"LocationWork"                       parent="Work"                         table=0x1401616a0 size=0x50   create=0x14002a810 cip=Internal::CreateInPlaceObject ti=0x1401c6b38
"MarketWork"                         parent="Work"                         table=0x140161710 size=0x58   create=0x14002a8d0 cip=Internal::CreateInPlaceObject ti=0x1401c6b70
"PastureWork"                        parent="AutoPickupWork"               table=0x1401617a0 size=0x58   create=0x14002bf00 cip=Internal::CreateInPlaceObject ti=0x1401c6ba8
"Pickup"                             parent="Work"                         table=0x140161810 size=0x60   create=0x14002bfc0 cip=Internal::CreateInPlaceObject ti=0x1401c6be0
"PlantArea"                          parent="Work"                         table=0x140161890 size=0x70   create=0x14002c470 cip=Internal::CreateInPlaceObject ti=0x1401c6c18
"PlantOrchard"                       parent="Work"                         table=0x140161910 size=0x70   create=0x14002c500 cip=Internal::CreateInPlaceObject ti=0x1401c6c50
"TendArea"                           parent="Work"                         table=0x140161990 size=0x60   create=0x14002c590 cip=Internal::CreateInPlaceObject ti=0x1401c6c88
"TradeWork"                          parent="Work"                         table=0x140161a10 size=0x58   create=0x14002c620 cip=Internal::CreateInPlaceObject ti=0x1401c6cc0
"Work"                               parent="Object::Internal"             table=0x140161a80 size=?      create=0x140003730 cip=Internal::CreateInPlaceObject ti=0x1401c6cf8
"BridgeTool"                         parent="DragTool"                     table=0x140161b00 size=0x40   create=0x14002d7a0 cip=0x14002d7e0 <no fn> ti=0x1401c6d30
"DragPlacementTool"                  parent="DragTool"                     table=0x140161b90 size=0x48   create=0x14002d8b0 cip=0x14002d8f0 <no fn> ti=0x1401c6d68
"DragTool"                           parent="Tool"                         table=0x140161c10 size=?      create=0x140003730 cip=0x140003730 AutoPickupWork_Factory_Abstract ti=0x1401c6da0
"EditTool"                           parent="Tool"                         table=0x140161ca0 size=0x38   create=0x14002daa0 cip=0x14002dae0 <no fn> ti=0x1401c6dd8
"PlacementTool"                      parent="Tool"                         table=0x140161d20 size=0x70   create=0x14002dc40 cip=0x14002dc80 <no fn> ti=0x1401c6e10
"RemovalTool"                        parent="DragTool"                     table=0x140161de0 size=0x40   create=0x14002e7a0 cip=0x14002e7e0 <no fn> ti=0x1401c6e48
"RoadTool"                           parent="DragTool"                     table=0x140161e70 size=0x48   create=0x14002e870 cip=0x14002e8b0 <no fn> ti=0x1401c6e80
"Tool"                               parent="Object::Internal"             table=0x140161f10 size=?      create=0x140003730 cip=0x140003730 AutoPickupWork_Factory_Abstract ti=0x1401c6eb8
"ToolSetting"                        parent="Object::Internal"             table=0x140161ee8 size=?      create=0x140003730 cip=0x140003730 AutoPickupWork_Factory_Abstract ti=0x1401c6ef0
"AbsorbResourceComponent"            parent="ControllerComponent"          table=0x140162000 size=0x48   create=0x14002ea20 cip=Internal::CreateInPlaceObject ti=0x1401c6f28
"AbsorbResourceDescription"          parent="Description"                  table=0x140162090 size=0x20   create=0x14002ea80 cip=0x14002eae0 AbsorbResourceDescription_CreateInPlace ti=0x1401c6f60
"AnimalComponent"                    parent="Component"                    table=0x140162110 size=0x58   create=0x14002f020 cip=Internal::CreateInPlaceObject ti=0x1401c6f98
"AnimalDescription"                  parent="Description"                  table=0x140162190 size=0x58   create=0x14002f080 cip=0x14002f0c0 AnimalDescription_CreateInPlace ti=0x1401c6fd0
"BuildComponent"                     parent="ControllerComponent"          table=0x140162260 size=0x78   create=0x14002f2b0 cip=Internal::CreateInPlaceObject ti=0x1401c7008
"BuildDescription"                   parent="Description"                  table=0x1401622f0 size=0x38   create=0x14002f320 cip=0x14002f390 BuildDescription_CreateInPlace ti=0x1401c7040
"CharacterComponent"                 parent="Component"                    table=0x140162370 size=0x80   create=0x14002f960 cip=Internal::CreateInPlaceObject ti=0x1401c7078
"CharacterDescription"               parent="Description"                  table=0x1401623f0 size=0x48   create=0x14002f9c0 cip=0x14002fa00 CharacterDescription_CreateInPlace ti=0x1401c70b0
"CitizenAIComponent"                 parent="Component"                    table=0x140162470 size=0xd0   create=0x14002fb20 cip=Internal::CreateInPlaceObject ti=0x1401c70e8
"CitizenAIDescription"               parent="Description"                  table=0x1401624f0 size=0x20   create=0x14002fb80 cip=0x14002fbe0 CitizenAIDescription_CreateInPlace ti=0x1401c7120
"CitizenComponent"                   parent="Component"                    table=0x140162580 size=0x290  create=0x140030110 cip=Internal::CreateInPlaceObject ti=0x1401c7158
"CitizenDescription"                 parent="Description"                  table=0x140162600 size=0x148  create=0x140030170 cip=0x1400301b0 CitizenDescription_CreateInPlace ti=0x1401c7190
"ClearAreaComponent"                 parent="ControllerComponent"          table=0x1401626d0 size=0x50   create=0x140030ea0 cip=Internal::CreateInPlaceObject ti=0x1401c71c8
"ClearAreaDescription"               parent="Description"                  table=0x140162760 size=0x38   create=0x140030f00 cip=0x140030f80 ClearAreaDescription_CreateInPlace ti=0x1401c7200
"ClearResourceComponent"             parent="ControllerComponent"          table=0x140162840 size=0x48   create=0x1400315a0 cip=Internal::CreateInPlaceObject ti=0x1401c7238
"ClearResourceDescription"           parent="Description"                  table=0x1401628d0 size=0x20   create=0x140031600 cip=0x140031660 ClearResourceDescription_CreateInPlace ti=0x1401c7270
"Component"                          parent="Object::Internal"             table=0x140162920 size=?      create=0x140003730 cip=Internal::CreateInPlaceObject ti=0x1401c72a8
"Description"                        parent="Object::External"             table=0x1401629a0 size=?      create=0x140003730 cip=0x140003730 AutoPickupWork_Factory_Abstract ti=0x1401c72e0
"ComponentDescription"               parent="Object::External"             table=0x140162a00 size=0x38   create=0x140031f30 cip=0x140031f90 ComponentDescription_CreateInPlace ti=0x1401c7318
"ConsumeProduceComponent"            parent="ResourceProduceComponent"     table=0x140162a80 size=0x70   create=0x140032300 cip=Internal::CreateInPlaceObject ti=0x1401c7350
"ConsumeProduceDescription"          parent="Description"                  table=0x140162b30 size=0x58   create=0x140032360 cip=0x1400323a0 ConsumeProduceDescription_CreateInPlace ti=0x1401c7388
"ControllerComponent"                parent="Component"                    table=0x140162b80 size=?      create=0x140003730 cip=Internal::CreateInPlaceObject ti=0x1401c73c0
"CreateBridgeDescription"            parent="Description"                  table=0x140162c30 size=0x60   create=0x140032880 cip=0x1400328c0 CreateBridgeDescription_CreateInPlace ti=0x1401c73f8
"CreateDragDescription"              parent="Description"                  table=0x140162ca0 size=0x40   create=0x140032970 cip=0x1400329b0 CreateDragDescription_CreateInPlace ti=0x1401c7430
"CreatePlacedDescription"            parent="Description"                  table=0x140162cf0 size=0x58   create=0x140032a60 cip=0x140032aa0 CreatePlacedDescription_CreateInPlace ti=0x1401c7468
"CreateRemovalDescription"           parent="Description"                  table=0x140162d80 size=0x48   create=0x140032b70 cip=0x140032be0 CreateRemovalDescription_CreateInPlace ti=0x1401c74a0
"CreateRoadDescription"              parent="Description"                  table=0x140162df0 size=0x30   create=0x140032d30 cip=0x140032da0 CreateRoadDescription_CreateInPlace ti=0x1401c74d8
"CropFieldComponent"                 parent="ControllerComponent"          table=0x140162ec0 size=0x68   create=0x140032ed0 cip=Internal::CreateInPlaceObject ti=0x1401c7510
"CropFieldDescription"               parent="Description"                  table=0x140162f50 size=0x28   create=0x140032f30 cip=0x140032f90 CropFieldDescription_CreateInPlace ti=0x1401c7548
"DecalComponent"                     parent="Component"                    table=0x140162fd0 size=0x38   create=0x1400337c0 cip=Internal::CreateInPlaceObject ti=0x1401c7580
"DecalDescription"                   parent="Description"                  table=0x140163050 size=0x50   create=0x140033820 cip=0x140033860 DecalDescription_CreateInPlace ti=0x1401c75b8
"DestroyComponent"                   parent="ControllerComponent"          table=0x140163120 size=0x68   create=0x140033bc0 cip=Internal::CreateInPlaceObject ti=0x1401c75f0
"DestroyDescription"                 parent="Description"                  table=0x1401631b0 size=0x38   create=0x140033c30 cip=0x140033ca0 DestroyDescription_CreateInPlace ti=0x1401c7628
"FishingComponent"                   parent="ResourceProduceComponent"     table=0x140163230 size=0x58   create=0x140034350 cip=Internal::CreateInPlaceObject ti=0x1401c7660
"FishingDescription"                 parent="Description"                  table=0x1401632e0 size=0x30   create=0x1400343c0 cip=0x140034430 FishingDescription_CreateInPlace ti=0x1401c7698
"FloorComponent"                     parent="Component"                    table=0x140163360 size=0x30   create=0x140034600 cip=Internal::CreateInPlaceObject ti=0x1401c76d0
"FloorDescription"                   parent="Description"                  table=0x1401633e0 size=0x30   create=0x140034660 cip=0x1400346d0 FloorDescription_CreateInPlace ti=0x1401c7708
"ForesterComponent"                  parent="ResourceProduceComponent"     table=0x140163460 size=0x58   create=0x140034870 cip=Internal::CreateInPlaceObject ti=0x1401c7740
"ForesterDescription"                parent="Description"                  table=0x140163510 size=0x30   create=0x1400348e0 cip=0x140034950 ForesterDescription_CreateInPlace ti=0x1401c7778
"GrowthComponent"                    parent="Component"                    table=0x140163590 size=0x50   create=0x140034b10 cip=Internal::CreateInPlaceObject ti=0x1401c77b0
"GrowthDescription"                  parent="Description"                  table=0x140163610 size=0x68   create=0x140034b70 cip=0x140034bb0 GrowthDescription_CreateInPlace ti=0x1401c77e8
"HerdComponent"                      parent="Component"                    table=0x140163680 size=0x60   create=0x140034cd0 cip=Internal::CreateInPlaceObject ti=0x1401c7820
"HerdDescription"                    parent="Description"                  table=0x140163700 size=0x40   create=0x140034d30 cip=0x140034d90 HerdDescription_CreateInPlace ti=0x1401c7858
"HuntComponent"                      parent="ResourceProduceComponent"     table=0x140163770 size=0x58   create=0x140034f30 cip=Internal::CreateInPlaceObject ti=0x1401c7890
"HuntDescription"                    parent="Description"                  table=0x140163820 size=0x28   create=0x140034f90 cip=0x140034ff0 HuntDescription_CreateInPlace ti=0x1401c78c8
"InteractComponent"                  parent="Component"                    table=0x1401638b0 size=0x38   create=0x1400351a0 cip=Internal::CreateInPlaceObject ti=0x1401c7900
"InteractDescription"                parent="Description"                  table=0x140163930 size=0x58   create=0x140035200 cip=0x140035240 InteractDescription_CreateInPlace ti=0x1401c7938
"LivestockComponent"                 parent="Component"                    table=0x1401639b0 size=0x38   create=0x140035480 cip=Internal::CreateInPlaceObject ti=0x1401c7970
"LivestockDescription"               parent="Description"                  table=0x140163a30 size=0x30   create=0x1400354e0 cip=0x140035550 LivestockDescription_CreateInPlace ti=0x1401c79a8
"MapComponent"                       parent="Component"                    table=0x140163aa0 size=0x38   create=0x140035700 cip=Internal::CreateInPlaceObject ti=0x1401c79e0
"MapDescription"                     parent="Description"                  table=0x140163b20 size=0x38   create=0x140035760 cip=0x1400357a0 MapDescription_CreateInPlace ti=0x1401c7a18
"ModelComponent"                     parent="Component"                    table=0x140163ba0 size=0x170  create=0x1400358d0 cip=Internal::CreateInPlaceObject ti=0x1401c7a50
"ModelDescription"                   parent="Description"                  table=0x140163c20 size=0x40   create=0x140035930 cip=0x1400359a0 ModelDescription_CreateInPlace ti=0x1401c7a88
"MultiObjectComponent"               parent="ControllerComponent"          table=0x140163cf0 size=0x60   create=0x140035ef0 cip=Internal::CreateInPlaceObject ti=0x1401c7ac0
"MultiObjectDescription"             parent="Description"                  table=0x140163d80 size=0x20   create=0x140035f50 cip=0x140035fb0 MultiObjectDescription_CreateInPlace ti=0x1401c7af8
"NaturalResourceComponent"           parent="Component"                    table=0x140163e10 size=0x30   create=0x140036550 cip=Internal::CreateInPlaceObject ti=0x1401c7b30
"NaturalResourceDescription"         parent="Description"                  table=0x140163e90 size=0x88   create=0x1400365d0 cip=0x140036610 NaturalResourceDescription_CreateInPlace ti=0x1401c7b68
"PastureComponent"                   parent="ControllerComponent"          table=0x140163f60 size=0x68   create=0x140036730 cip=Internal::CreateInPlaceObject ti=0x1401c7ba0
"PastureDescription"                 parent="Description"                  table=0x140163ff0 size=0x28   create=0x1400367a0 cip=0x140036800 PastureDescription_CreateInPlace ti=0x1401c7bd8
"PickingComponent"                   parent="Component"                    table=0x140164040 size=0x38   create=0x140036da0 cip=Internal::CreateInPlaceObject ti=0x1401c7c10
"PickingDescription"                 parent="Description"                  table=0x1401640c0 size=0x40   create=0x140036e00 cip=0x140036e70 PickingDescription_CreateInPlace ti=0x1401c7c48
"RawMaterialComponent"               parent="Component"                    table=0x140164170 size=0x40   create=0x140037270 cip=Internal::CreateInPlaceObject ti=0x1401c7c80
"RawMaterialDescription"             parent="Description"                  table=0x1401641f0 size=0x60   create=0x1400372d0 cip=0x140037310 RawMaterialDescription_CreateInPlace ti=0x1401c7cb8
"ResidenceComponent"                 parent="ControllerComponent"          table=0x1401642c0 size=0x60   create=0x140037440 cip=Internal::CreateInPlaceObject ti=0x1401c7cf0
"ResidenceDescription"               parent="Description"                  table=0x140164350 size=0x30   create=0x1400374a0 cip=0x140037500 ResidenceDescription_CreateInPlace ti=0x1401c7d28
"ResourceProduceComponent"           parent="ControllerComponent"          table=0x1401643f0 size=?      create=0x140003730 cip=Internal::CreateInPlaceObject ti=0x1401c7d60
"StateMachineComponent"              parent="Component"                    table=0x140164520 size=0x58   create=0x1400382d0 cip=Internal::CreateInPlaceObject ti=0x1401c7d98
"StateMachineDescription"            parent="Description"                  table=0x1401645a0 size=0x20   create=0x140038330 cip=0x140038390 StateMachineDescription_CreateInPlace ti=0x1401c7dd0
"StatusIconComponent"                parent="Component"                    table=0x140164620 size=0x108  create=0x1400388c0 cip=Internal::CreateInPlaceObject ti=0x1401c7e08
"StatusIconDescription"              parent="Description"                  table=0x1401646a0 size=0x38   create=0x140038920 cip=0x140038990 StatusIconDescription_CreateInPlace ti=0x1401c7e40
"StorageComponent"                   parent="Component"                    table=0x140164720 size=0x40   create=0x140038b50 cip=Internal::CreateInPlaceObject ti=0x1401c7e78
"StorageDescription"                 parent="Description"                  table=0x1401647a0 size=0x40   create=0x140038bb0 cip=0x140038c20 StorageDescription_CreateInPlace ti=0x1401c7eb0
"StorageLocationComponent"           parent="ControllerComponent"          table=0x140164880 size=0x48   create=0x140038df0 cip=Internal::CreateInPlaceObject ti=0x1401c7ee8
"StorageLocationDescription"         parent="Description"                  table=0x140164910 size=0x28   create=0x140038e50 cip=0x140038eb0 StorageLocationDescription_CreateInPlace ti=0x1401c7f20
"ToolbarDescription"                 parent="Description"                  table=0x140164980 size=0x68   create=0x140039490 cip=0x1400394d0 ToolbarDescription_CreateInPlace ti=0x1401c7f58
"UIDescription"                      parent="Description"                  table=0x140164a70 size=0x48   create=0x1400395b0 cip=0x140039620 UIDescription_CreateInPlace ti=0x1401c7f90
"UIComponent"                        parent="Component"                    table=0x1401649f0 size=0x38   create=0x140039550 cip=Internal::CreateInPlaceObject ti=0x1401c7fc8
"WildAnimalComponent"                parent="Component"                    table=0x140164af0 size=0x30   create=0x140039c20 cip=Internal::CreateInPlaceObject ti=0x1401c8000
"WildAnimalDescription"              parent="Description"                  table=0x140164b70 size=0x20   create=0x140039c80 cip=0x140039ce0 WildAnimalDescription_CreateInPlace ti=0x1401c8038
"WorkComponent"                      parent="Component"                    table=0x140164be0 size=0x38   create=0x140039e70 cip=Internal::CreateInPlaceObject ti=0x1401c8070
"WorkDescription"                    parent="Description"                  table=0x140164c60 size=0x38   create=0x140039ed0 cip=0x140039f10 WorkDescription_CreateInPlace ti=0x1401c80a8
"WorkPlaceComponent"                 parent="ControllerComponent"          table=0x140164d30 size=0x68   create=0x14003a030 cip=Internal::CreateInPlaceObject ti=0x1401c80e0
"WorkPlaceDescription"               parent="Description"                  table=0x140164dc0 size=0x30   create=0x14003a090 cip=0x14003a100 WorkPlaceDescription_CreateInPlace ti=0x1401c8118
"ZoneComponent"                      parent="Component"                    table=0x140164e30 size=0x28   create=0x14003a6a0 cip=Internal::CreateInPlaceObject ti=0x1401c8150
"ZoneDescription"                    parent="Description"                  table=0x140164eb0 size=0x20   create=0x14003a700 cip=0x14003a760 ZoneDescription_CreateInPlace ti=0x1401c8188
```

## §4 — the 51 unreflected tables

- 24 controller-member tables, plus the interface 0x14015f070 (§2);
- 5 loop subclasses (B37_SLOTS §E1/§E3): GameUpdateLoop 0x14015e2f0 (12 slots), MenuDialogLoopT 0x14015ed00 (13), the splash
  DialogLoaderLoopT subclass 0x140158800 (13), and the LoaderLoop subclasses 0x140158870 (12, GameMain) and 0x1401588d0 (12);
- 11 exe-emitted tables of RUNTIME classes whose ctor the exe inlines (slot 0 is an exe scalar deleting dtor, every other slot an
  import): `Achievement::AchievementInstance` 0x14015e350 (the brief's "never-registered exe AchievementInstance table"),
  `Graphics::{EdgeMesh 0x140159a60, FrameCopyImageBuffer 0x14015e370 (12 slots), MaterialInstance 0x14015c400, MeshInstance
  0x14015e240, ParticleSystemInstance 0x14015ecb0, SunLight 0x14015e290}`, `UI::{DialogInstance 0x14015f5a0, SpriteCapture
  0x14015d550, SpriteSheet 0x14015c3b0}` and `Video::ImageBuffer` 0x14015c8c0 (10 slots). Once the Runtime classes are real,
  each of these is a plain `new NS::X`, not an exe class;
- 5 unreflected exe subclasses of Runtime classes: `System::Thread` 0x14015ef18 (2 slots; `MemAlloc(0x1f8)`),
  `Graphics::Geometry` 0x14015f0c0 (the StatusIconComponent member), 0x14015f110 (`MemAlloc(0x230)`) and 0x140160240
  (`MemAlloc(0xf0)`), and `Graphics::ParticleInstance` 0x1401601e0;
- 5 CRT entries: the std::exception family 0x14017cb88 / 0x14017cbf0 / 0x14017cc00, and two CRT function pointers
  0x14017cb70 / 0x14017cb78.


## §5 — disagreements with the tree noticed while reading (addresses and `file:line`)

1. **A live wrong stamp.** `code/Game/Action/createnaturalresource.cpp:155` and `code/Game/Action/createrawmaterial.cpp:226`
   stamp `ResolveAddr(Module::Exe, 0x14015f700)` as "the Action base vtable" in the scalar dtors. 0x14015f700 is slot 6 of the
   `ResidenceUI` table 0x14015f6d0. Retail stamps **0x14015faa0** (`Action`): `CreateNaturalResource_ScalarDtor` 0x140022240
   (store at 0x14002226e) and `CreateRawMaterial_ScalarDtor` 0x140022350 (0x14002237e).
2. `engine/types/ControllerComponent.h:2-22` models the controller as a fixed sub-object at +0x28 ("components with a secondary
   vtable"). In retail the offset varies (+0x30 Happiness, +0x38 Attendance, +0x48 Pasture, +0x58 Nomad, +0x78 Boat, +0x88
   Trade) and the controller is a member, not a base (§2). `ENDGAME.md` says "56 multiple-inheritance types" and calls
   0x140162b80 the ControllerComponent ctor. In retail 0x140162b80 is the ControllerComponent TABLE, and the family has 29
   classes (§2).
3. `engine/types/Component.h:26` has `ScalarDtor = 0x140162920 // vtable[0] base`, but 0x140162920 is the Component table itself
   (its slot 0 is 0x140031cc0). `Component.h:15-18` names slot 1 GetDescription, 2 Startup, 3 Shutdown and 4 Serialize; in retail
   slot 1 is TypeName, 2 is Type and 3 is the Persist override (Object::Internal layout).
4. Comments in `code/Game/Work/{autopickupwork.cpp:89, harvestarea.cpp:213, harvestorchard.cpp:202, work_base_standalone.cpp:94}`
   call 0x140161300 / 0x140161380 "RTTI type descriptors". The exe has no RTTI: they are slot 12 of `FireWork` 0x1401612a0 and
   slot 12 of `FishingWork` 0x140161320. (The code in those files already uses the right tables.)
5. Ghidra labels vs retail: `AttendanceComponent_Ctor` 0x140044510, `Action_Action_Ctor4` / `_Ctor7` (0x1400c70f0 / 0x1400c6970)
   and the `*_Controller_Slot10` functions (e.g. 0x1400310c0) are destructors. The `*_Controller_ScalarDtor` label on
   controller-member slot 0 (e.g. 0x140002a80) is a lookup function, not a dtor. `g_ReloadDialog_Vtable` 0x14015c480 is
   `MessageDialog`; `g_PauseDialog_Vtable` 0x14015f550 is `MainMenuDialog`. Use the TypeId names in §0.
6. Tree coverage: 350 of the 361 table addresses appear somewhere in `code/ standalone/ engine/` (`*.cpp *.h *.inc`, build
   dirs excluded); 96 are `RegisterExeAddr`'d. The 11 that appear nowhere are listed below. 26 tree hex literals in
   0x140158000-0x140165000 point INSIDE a table (slot addresses); only item 1 uses one as a table start.

```
0x14015d550  [Runtime UI::SpriteCapture table]
0x14015e290  [Runtime Graphics::SunLight table]
0x14015e370  [Runtime Graphics::FrameCopyImageBuffer table]
0x14015ecb0  [Runtime Graphics::ParticleSystemInstance table]
0x140161ee8  ToolSetting
0x1401629a0  Description
0x14017cb70  {CRT _cinit function pointer, not a vtable}
0x14017cb78  {CRT __C_specific_handler function pointer, not a vtable}
0x14017cb88  {CRT std::bad_exception vftable}
0x14017cbf0  {CRT vftable}
0x14017cc00  {CRT std::exception vftable}
```

Tree hex literals that point inside a table (slot addresses):

```
0x1401591e8 = table 0x1401591a0 slot 9
0x1401593ff = table 0x140159380 slot 15
0x14015a1e8 = table 0x14015a180 slot 13
0x14015d140 = table 0x14015d0f0 slot 10
0x14015eda8 = table 0x14015ed80 slot 5
0x14015ee08 = table 0x14015ede0 slot 5
0x14015f0b7 = table 0x14015f070 slot 8
0x14015f700 = table 0x14015f6d0 slot 6
0x14015f980 = table 0x14015f970 slot 2
0x14015fa48 = table 0x14015fa00 slot 9
0x1401601d8 = table 0x140160190 slot 9
0x1401612e0 = table 0x1401612a0 slot 8
0x140161300 = table 0x1401612a0 slot 12
0x140161340 = table 0x140161320 slot 4
0x140161380 = table 0x140161320 slot 12
0x140161b30 = table 0x140161b00 slot 6
0x140161bc0 = table 0x140161b90 slot 6
0x140161c40 = table 0x140161c10 slot 6
0x140161e10 = table 0x140161de0 slot 6
0x140162290 = table 0x140162260 slot 6
0x1401625b8 = table 0x140162580 slot 7
0x1401629a8 = table 0x1401629a0 slot 1
0x1401631a0 = table 0x140163120 slot 16
0x1401631a8 = table 0x140163120 slot 17
0x140164550 = table 0x140164520 slot 6
0x140164e60 = table 0x140164e30 slot 6
```

## §6 — every table, slot by slot (sorted by class name)

```
### [Runtime Achievement::AchievementInstance table]  table=0x14015e350  slots=4  sizeof=?  parent=- / structural -  stop=next table
  [ 0] +0x00 0x140019d90  OWN FUN_140019d90 (49 B)
  [ 1] +0x08 0x14003aaf2  IMPORT ?TypeName@AchievementInstance@Achievement@@UEBAPEBDXZ
  [ 2] +0x10 0x14003aaf8  IMPORT ?Type@AchievementInstance@Achievement@@UEBAIXZ
  [ 3] +0x18 0x14003aada  IMPORT ?Persist@AchievementInstance@Achievement@@UEAAXAEAVStream@IO@@@Z
  stamped by (1): other+0x0 at 0x14009766f in 0x140096b00 MainMenuDialog_impl_6b00
### [Runtime Graphics::EdgeMesh table]  table=0x140159a60  slots=9  sizeof=?  parent=- / structural -  stop=non-code qword 0x74617267696d6d49 (string "ImmigrationComponent")
  [ 0] +0x00 0x140006de0  OWN FUN_140006de0 (49 B)
  [ 1] +0x08 0x14003a93c  IMPORT ?TypeName@EdgeMesh@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x14003a942  IMPORT ?Type@EdgeMesh@Graphics@@UEBAIXZ
  [ 3] +0x18 0x14003a918  IMPORT ?GetFileVersion@EdgeMesh@Graphics@@UEBAHXZ
  [ 4] +0x20 0x14003a91e  IMPORT ?Compile@EdgeMesh@Graphics@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z
  [ 5] +0x28 0x14003a924  IMPORT ?Serialize@EdgeMesh@Graphics@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x14003a936  IMPORT ?Reconstruct@EdgeMesh@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x14003a92a  IMPORT ?Destruct@EdgeMesh@Graphics@@UEAAXXZ
  [ 8] +0x40 0x14003a930  IMPORT ?Construct@EdgeMesh@Graphics@@UEAAPEAVExternal@Object@@XZ
  stamped by (1): other+0x0 at 0x14004c317 in 0x14004b930 TradeWork_impl_b930
### [Runtime Graphics::FrameCopyImageBuffer table]  table=0x14015e370  slots=12  sizeof=?  parent=- / structural -  stop=non-code qword 0x6d61472f656d6147 (string "Game/Game")
  [ 0] +0x00 0x140019d50  OWN FUN_140019d50 (49 B)
  [ 1] +0x08 0x14003ab10  IMPORT ?TypeName@FrameCopyImageBuffer@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x14003ab16  IMPORT ?Type@FrameCopyImageBuffer@Graphics@@UEBAIXZ
  [ 3] +0x18 0x14003aae0  IMPORT ?GetFileVersion@FrameCopyImageBuffer@Graphics@@UEBAHXZ
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x14003aae6  IMPORT ?Serialize@FrameCopyImageBuffer@Graphics@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x14003ab0a  IMPORT ?Reconstruct@FrameCopyImageBuffer@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x14003aafe  IMPORT ?Destruct@FrameCopyImageBuffer@Graphics@@UEAAXXZ
  [ 8] +0x40 0x14003ab04  IMPORT ?Construct@FrameCopyImageBuffer@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 9] +0x48 0x14003aa86  IMPORT ?Create@FrameCopyImageBuffer@Graphics@@UEAA_NW4ImageType@ImageBuffer@Video@@W4Usage@45@W4Format@45@IHHW4FilterMode@45@W4AddressMode@45@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z
  [10] +0x50 0x14003aa8c  IMPORT ?Generate@FrameCopyImageBuffer@Graphics@@UEBAXAEAVContext@Video@@H@Z
  [11] +0x58 0x14003aaec  IMPORT ?Render@FrameCopyImageBuffer@Graphics@@UEBAXAEAVRenderer@2@AEBVScene@2@AEBVCamera@2@H@Z
  stamped by (1): other+0x0 at 0x140095e01 in 0x140095da0 ModDialog_impl_6010_sub_5da0
### [Runtime Graphics::MaterialInstance table]  table=0x14015c400  slots=9  sizeof=?  parent=- / structural -  stop=non-code qword 0x446567617373654d (string "MessageDialog")
  [ 0] +0x00 0x1400121a0  OWN FUN_1400121a0 (49 B)
  [ 1] +0x08 0x14003aa08  IMPORT ?TypeName@MaterialInstance@Graphics@@UEBAPEBDXZ
  [ 2] +0x10 0x14003aa0e  IMPORT ?Type@MaterialInstance@Graphics@@UEBAIXZ
  [ 3] +0x18 0x14003a996  IMPORT ?GetFileVersion@MaterialInstance@Graphics@@UEBAHXZ
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x14003a99c  IMPORT ?Serialize@MaterialInstance@Graphics@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x14003aa02  IMPORT ?Reconstruct@MaterialInstance@Graphics@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x14003a9f6  IMPORT ?Destruct@MaterialInstance@Graphics@@UEAAXXZ
  [ 8] +0x40 0x14003a9fc  IMPORT ?Construct@MaterialInstance@Graphics@@UEAAPEAVExternal@Object@@XZ
  stamped by (4): other+0x0 at 0x14006cb5a in 0x14006cb00 FrustumOutlineDescription_Slot14_sub_cb00; other+0x0 at 0x14006fb27 in 0x14006f150 ModDialog_Serialize; other+0x0 at 0x1400718e8 in 0x140071890 FUN_140071890; other+0x0 at 0x1400b722e in 0x1400b6ce0 operator_impl_6ce0
### [Runtime Graphics::MeshInstance table]  table=0x14015e240  slots=9  sizeof=?  parent=- / structural -  stop=zero qword
  [ 0] +0x00 0x140019e80  OWN EnvironmentDescription_vslot10 (49 B)
  [ 1] +0x08 0x14003aa80  IMPORT ?Update@MeshInstance@Graphics@@UEAAXAEBVTime@System@@@Z
  [ 2] +0x10 0x14003aabc  IMPORT ?Draw@MeshInstance@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z
  [ 3] +0x18 0x14003aac2  IMPORT ?AddDrawItems@MeshInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
  [ 4] +0x20 0x14003aa7a  IMPORT ?UpdateAttached@Geometry@Graphics@@UEAAXXZ  [in 7 tables]
  [ 5] +0x28 0x14003aac8  IMPORT ?GetSubTransform@MeshInstance@Graphics@@UEBA?AVTransform@Math@@H@Z
  [ 6] +0x30 0x14003aace  IMPORT ?GetInstanceTransform@MeshInstance@Graphics@@UEBA?AVMatrix@Math@@HM@Z
  [ 7] +0x38 0x14003aad4  IMPORT ?SetSharedInstanceData@MeshInstance@Graphics@@UEBAHAEAVContext@Video@@HM@Z
  [ 8] +0x40 0x14003aaaa  IMPORT ?Clean@Geometry@Graphics@@UEAA_NXZ  [in 7 tables]
  stamped by (5): other+0x0 at 0x140097378 in 0x140096b00 MainMenuDialog_impl_6b00; other+0x0 at 0x14009ab56 in 0x14009a930 MenuScene_impl_a930; other+0x0 at 0x14009ad3c in 0x14009a930 MenuScene_impl_a930; other+0x0 at 0x1400dcfcf in 0x1400dcd50 TimeGoalInstance_Slot11; other+0x0 at 0x1400fe785 in 0x1400fe580 PlacementTool_Startup
  other refs (1):   0x14009aef7 MenuScene_impl_a930+1479 : LEA R14,[0x14015e240]
### [Runtime Graphics::ParticleSystemInstance table]  table=0x14015ecb0  slots=10  sizeof=?  parent=- / structural -  stop=next table
  [ 0] +0x00 0x14001a410  OWN FUN_14001a410 (49 B)
  [ 1] +0x08 0x14003ab28  IMPORT ?Update@ParticleSystemInstance@Graphics@@UEAAXAEBVTime@System@@@Z
  [ 2] +0x10 0x14003ab2e  IMPORT ?Draw@ParticleInstance@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z  [in 2 tables]
  [ 3] +0x18 0x14003ab34  IMPORT ?AddDrawItems@ParticleInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z  [in 2 tables]
  [ 4] +0x20 0x14003aa7a  IMPORT ?UpdateAttached@Geometry@Graphics@@UEAAXXZ  [in 7 tables]
  [ 5] +0x28 0x14003aaa4  IMPORT ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z  [in 6 tables]
  [ 6] +0x30 0x14003ab3a  IMPORT ?GetInstanceTransform@ParticleInstance@Graphics@@UEBA?AVMatrix@Math@@HM@Z  [in 2 tables]
  [ 7] +0x38 0x14003ab1c  IMPORT ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z  [in 6 tables]
  [ 8] +0x40 0x14003aaaa  IMPORT ?Clean@Geometry@Graphics@@UEAA_NXZ  [in 7 tables]
  [ 9] +0x48 0x14003ab40  IMPORT ?SetParticle@ParticleSystemInstance@Graphics@@UEAAXV?$ExternalPtr@VParticle@Graphics@@@Object@@@Z
  stamped by (1): other+0x0 at 0x14009af5c in 0x14009a930 MenuScene_impl_a930
  other refs (1):   0x14009b1e8 MenuScene_impl_a930+2232 : LEA RBX,[0x14015ecb0]
### [Runtime Graphics::SunLight table]  table=0x14015e290  slots=11  sizeof=?  parent=- / structural -  stop=zero qword
  [ 0] +0x00 0x140019e40  OWN FUN_140019e40 (49 B)
  [ 1] +0x08 0x14003aa92  IMPORT ?Update@SunLight@Graphics@@UEAAXAEBVTime@System@@@Z
  [ 2] +0x10 0x14003aa98  IMPORT ?Draw@SunLight@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z
  [ 3] +0x18 0x14003aa9e  IMPORT ?AddDrawItems@SunLight@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
  [ 4] +0x20 0x14003aa7a  IMPORT ?UpdateAttached@Geometry@Graphics@@UEAAXXZ  [in 7 tables]
  [ 5] +0x28 0x14003aaa4  IMPORT ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z  [in 6 tables]
  [ 6] +0x30 0x14003ab22  IMPORT ?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z  [in 4 tables]
  [ 7] +0x38 0x14003ab1c  IMPORT ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z  [in 6 tables]
  [ 8] +0x40 0x14003aaaa  IMPORT ?Clean@Geometry@Graphics@@UEAA_NXZ  [in 7 tables]
  [ 9] +0x48 0x14003aab0  IMPORT ?DrawShadowMap@SunLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HH@Z
  [10] +0x50 0x14003aab6  IMPORT ?Draw@SunLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HHHH@Z
  stamped by (2): other+0x0 at 0x140097172 in 0x140096b00 MainMenuDialog_impl_6b00; other+0x0 at 0x14009aaf3 in 0x14009a930 MenuScene_impl_a930
### [Runtime UI::DialogInstance table]  table=0x14015f5a0  slots=10  sizeof=?  parent=- / structural -  stop=non-code qword 0x5565727574736150 (string "PastureUI")
  [ 0] +0x00 0x140020cb0  OWN MainMenuDialog_Slot10 (49 B)
  [ 1] +0x08 0x14003ab4c  IMPORT ?TypeName@DialogInstance@UI@@UEBAPEBDXZ
  [ 2] +0x10 0x14003ab52  IMPORT ?Type@DialogInstance@UI@@UEBAIXZ
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x14003aa14  IMPORT ?OnOpened@DialogInstance@UI@@UEAAXXZ  [in 2 tables]
  [ 5] +0x28 0x14003a94e  IMPORT ?OnClosed@DialogInstance@UI@@UEAAXXZ  [in 12 tables]
  [ 6] +0x30 0x14003a98a  IMPORT ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ  [in 8 tables]
  [ 7] +0x38 0x14003a990  IMPORT ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z  [in 11 tables]
  [ 8] +0x40 0x14003aa1a  OWN OnElementEvent (6 B)  [in 4 tables]
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (5): other+0x0 at 0x1400b4335 in 0x1400b3cd0 MainMenuDialog_Serialize; other+0x0 at 0x1400fd5eb in 0x1400fd3b0 EditTool_SelectEntity; other+0x0 at 0x1400ffb2d in 0x1400ff9e0 FUN_1400ff9e0; other+0x0 at 0x140101b9c in 0x140101a80 FUN_140101a80; other+0x0 at 0x14010238c in 0x140101ee0 EntityUI_impl_1ee0
### [Runtime UI::SpriteCapture table]  table=0x14015d550  slots=9  sizeof=?  parent=- / structural -  stop=non-code qword 0x6142737574617453 (string "StatusBarUI")
  [ 0] +0x00 0x140013f80  OWN SpriteCaptureUI_Slot12 (49 B)
  [ 1] +0x08 0x14003aa6e  IMPORT ?TypeName@SpriteCapture@UI@@UEBAPEBDXZ
  [ 2] +0x10 0x14003aa74  IMPORT ?Type@SpriteCapture@UI@@UEBAIXZ
  [ 3] +0x18 0x14003aa50  IMPORT ?GetFileVersion@SpriteCapture@UI@@UEBAHXZ
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x14003aa56  IMPORT ?Serialize@SpriteCapture@UI@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x14003aa68  IMPORT ?Reconstruct@SpriteCapture@UI@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x14003aa5c  IMPORT ?Destruct@SpriteCapture@UI@@UEAAXXZ
  [ 8] +0x40 0x14003aa62  IMPORT ?Construct@SpriteCapture@UI@@UEAAPEAVExternal@Object@@XZ
  stamped by (1): other+0x0 at 0x140079312 in 0x1400792b0 FUN_1400792b0
### [Runtime UI::SpriteSheet table]  table=0x14015c3b0  slots=9  sizeof=?  parent=- / structural -  stop=zero qword
  [ 0] +0x00 0x1400121e0  OWN MapUI_Slot12 (49 B)
  [ 1] +0x08 0x14003a9ea  IMPORT ?TypeName@SpriteSheet@UI@@UEBAPEBDXZ
  [ 2] +0x10 0x14003a9f0  IMPORT ?Type@SpriteSheet@UI@@UEBAIXZ
  [ 3] +0x18 0x14003a9a2  IMPORT ?GetFileVersion@SpriteSheet@UI@@UEBAHXZ
  [ 4] +0x20 0x14003a9a8  IMPORT ?Compile@SpriteSheet@UI@@UEAA?AW4CompileResult@External@Object@@AEBVFilename@System@@@Z
  [ 5] +0x28 0x14003a9ae  IMPORT ?Serialize@SpriteSheet@UI@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x14003a9e4  IMPORT ?Reconstruct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x14003a9d8  IMPORT ?Destruct@SpriteSheet@UI@@UEAAXXZ
  [ 8] +0x40 0x14003a9de  IMPORT ?Construct@SpriteSheet@UI@@UEAAPEAVExternal@Object@@XZ
  stamped by (4): other+0x0 at 0x14006cc7f in 0x14006cb00 FrustumOutlineDescription_Slot14_sub_cb00; other+0x0 at 0x14006fc16 in 0x14006f150 ModDialog_Serialize; other+0x0 at 0x1400719e5 in 0x140071890 FUN_140071890; other+0x0 at 0x1400b7314 in 0x1400b6ce0 operator_impl_6ce0
### [Runtime Video::ImageBuffer table]  table=0x14015c8c0  slots=10  sizeof=?  parent=- / structural -  stop=non-code qword 0x616f6c7055646f4d (string "ModUploadDialog")
  [ 0] +0x00 0x1400129d0  OWN ModDialog_Slot10 (49 B)
  [ 1] +0x08 0x14003aa44  IMPORT ?TypeName@ImageBuffer@Video@@UEBAPEBDXZ
  [ 2] +0x10 0x14003aa4a  IMPORT ?Type@ImageBuffer@Video@@UEBAIXZ
  [ 3] +0x18 0x14003aa26  IMPORT ?GetFileVersion@ImageBuffer@Video@@UEBAHXZ
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x14003aa2c  IMPORT ?Serialize@ImageBuffer@Video@@UEAA_NAEAVStream@IO@@@Z
  [ 6] +0x30 0x14003aa3e  IMPORT ?Reconstruct@ImageBuffer@Video@@UEAAPEAVExternal@Object@@XZ
  [ 7] +0x38 0x14003aa32  IMPORT ?Destruct@ImageBuffer@Video@@UEAAXXZ
  [ 8] +0x40 0x14003aa38  IMPORT ?Construct@ImageBuffer@Video@@UEAAPEAVExternal@Object@@XZ
  [ 9] +0x48 0x14003aa20  IMPORT ?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@W4Format@12@IHHW4FilterMode@12@W4AddressMode@12@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z
  stamped by (7): other+0x0 at 0x14006faa4 in 0x14006f150 ModDialog_Serialize; other+0x0 at 0x140071505 in 0x1400714c0 NoiseUI_Serialize; other+0x0 at 0x1400a4ecf in 0x1400a4e80 FUN_1400a4e80; other+0x0 at 0x1400a4f4a in 0x1400a4e80 FUN_1400a4e80; other+0x0 at 0x1400a51ae in 0x1400a5140 ModDialog_impl_5140; other+0x0 at 0x1400a5226 in 0x1400a5140 ModDialog_impl_5140; other+0x0 at 0x1400ac7c8 in 0x1400ac780 TradeWork_impl_sub_c780
### AbsorbResourceComponent  table=0x140162000  slots=18  sizeof=0x48  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c6f28 create=0x14002ea20 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e410 AbsorbResourceComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x140161fb0
  [ 0] +0x00 0x14002ebf0  OWN AbsorbResourceComponent_Controller_Slot10 (115 B)
  [ 1] +0x08 0x14002ea10  TINY returns qword [0x1401c6f30]
  [ 2] +0x10 0x14002ea00  TINY returns dword [0x1401c6f28] = TypeId(AbsorbResourceComponent)
  [ 3] +0x18 0x140105c00  OWN AbsorbResourceComponent_Controller_Slot13 (76 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x140003730  TINY return 0 (xor eax,eax)  [in 5 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (3): this+0x0 at 0x14002ec14 in 0x14002ebf0 AbsorbResourceComponent_Controller_Slot10; this+0x0 at 0x140105c79 in 0x140105c60 AbsorbResourceComponent_Ctor; this+0x0 at 0x140105ced in 0x140105cc0 AbsorbResourceComponent_Ctor
### AbsorbResourceDescription  table=0x140162090  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x65446c616d696e41 (string "AnimalDescription")
  reflected: ti=0x1401c6f60 create=0x14002ea80 createInPlace=0x14002eae0 AbsorbResourceDescription_CreateInPlace registered in 0x14014e450 AbsorbResourceDescription_Register
  [ 0] +0x00 0x14002eb90  OWN AbsorbResourceComponent_vslot18 (83 B)
  [ 1] +0x08 0x14002ea70  TINY returns qword [0x1401c6f68]
  [ 2] +0x10 0x14002ea60  TINY returns dword [0x1401c6f60] = TypeId(AbsorbResourceDescription)
  [ 3] +0x18 0x140105d90  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140105d50  OWN FUN_140105d50 (46 B)
  [ 6] +0x30 0x14002eb30  OWN FUN_14002eb30 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14002eb80  OWN (no Ghidra function)
  [ 9] +0x48 0x14002eae0  OWN AbsorbResourceDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x14002eac5 in 0x14002ea80 AbsorbResourceDescription_Factory; this+0x0 at 0x14002eb14 in 0x14002eae0 AbsorbResourceDescription_CreateInPlace; this+0x0 at 0x14002eb69 in 0x14002eb30 FUN_14002eb30; this+0x0 at 0x14002ebb4 in 0x14002eb90 AbsorbResourceComponent_vslot18; this+0x0 at 0x140105db9 in 0x140105da0 FUN_140105da0; this+0x0 at 0x140105e0e in 0x140105de0 FUN_140105de0
### AchievementDialog  table=0x14015b020  slots=10  sizeof=0xd0  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c4c98 create=0x140010320 createInPlace=0x140010390 Cleanup_AchievementDialog registered in 0x14014bc80 RegisterType_AchievementDialog
  [ 0] +0x00 0x1400103f0  OWN AchievementDialog_ScalarDtor (105 B)
  [ 1] +0x08 0x140010310  TINY returns qword [0x1401c4ca0]
  [ 2] +0x10 0x140010300  TINY returns dword [0x1401c4c98] = TypeId(AchievementDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x14005d490  OWN AchievementDialog_Serialize (1687 B)
  [ 5] +0x28 0x14003a94e  IMPORT ?OnClosed@DialogInstance@UI@@UEAAXXZ  [in 12 tables]
  [ 6] +0x30 0x14005d240  OWN AchievementDialog_Draw (436 B)
  [ 7] +0x38 0x14005d480  TINY RET 0 (c2 00 00)
  [ 8] +0x40 0x14005d440  OWN AchievementDialog_OnRemove (56 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (7): other+0x0 at 0x14001035d in 0x140010320 Factory_AchievementDialog; this+0x0 at 0x1400103ba in 0x140010390 Cleanup_AchievementDialog; this+0x0 at 0x140010414 in 0x1400103f0 AchievementDialog_ScalarDtor; this+0x0 at 0x14005db4d in 0x14005db30 AchievementDialog_CopyCtor; this+0x0 at 0x14005dbd2 in 0x14005db90 AchievementDialog_Ctor3; this+0x0 at 0x14005dc24 in 0x14005dc00 AchievementDialog_Ctor4; other+0x0 at 0x1400b35bc in 0x1400b34c0 AchievementDialog_Ctor
### Action  table=0x14015faa0  slots=10  sizeof=abstract  parent="Object::Internal" / structural Object::Internal (Runtime ctor import)  stop=non-code qword 0x4e6574616d696e41 (string "AnimateNaturalResource")
  reflected: ti=0x1401c5da8 create=0x140003730 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d010 Action_RegisterType
  [ 0] +0x00 0x140021c90  OWN ComponentEntity_Slot5 (94 B)
  [ 1] +0x08 0x140021c80  TINY returns qword [0x1401c5db0]
  [ 2] +0x10 0x140021c70  TINY returns dword [0x1401c5da8] = TypeId(Action)
  [ 3] +0x18 0x1400c4b40  OWN ComponentEntity_OnRemove (69 B)
  [ 4] +0x20 0x14003addc  _purecall  [in 12 tables]
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]
  [ 8] +0x40 0x1400c4a30  TINY RET 0 (c2 00 00)  [in 7 tables]
  [ 9] +0x48 0x1400c4a40  TINY return false (xor al,al)  [in 2 tables]
  stamped by (116): other+0x0 at 0x14000122b in 0x1400011f0 PlayAnimation_Factory; this+0x0 at 0x14000128e in 0x140001260 PlayAnimation_ScalarDtor; other+0x0 at 0x1400013eb in 0x1400013b0 ShowTool_Factory; this+0x0 at 0x14000144e in 0x140001420 ShowTool_ScalarDtor; this+0x0 at 0x140021cb4 in 0x140021c90 ComponentEntity_Slot5; other+0x0 at 0x140021dfb in 0x140021dc0 AnimateNaturalResource_Factory; this+0x0 at 0x140021e5e in 0x140021e30 AnimateNaturalResource_ScalarDtor; other+0x0 at 0x140021efb in 0x140021ec0 CompleteRoad_Factory; this+0x0 at 0x140021f5e in 0x140021f30 CompleteRoad_ScalarDtor; other+0x0 at 0x140021ffb in 0x140021fc0 CompleteTask_Factory; this+0x0 at 0x14002205e in 0x140022030 CompleteTask_ScalarDtor; other+0x0 at 0x1400220fb in 0x1400220c0 ConsumeRawMaterial_Factory ...
  other refs (3):   0x1400bad9c PerformWork_Action_CopyCtor+940 : LEA R12,[0x14015faa0];   0x1400bdecb ShowTool_Action_Ctor3+427 : LEA R12,[0x14015faa0];   0x1400be1d6 ShowTool_Action_Ctor3+1206 : LEA R12,[0x14015faa0]
### AmbientEmitterComponent  table=0x140158a60  slots=16  sizeof=0x48  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c3e60 create=0x1400014b0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014a600 AmbientEmitterComponent_RegisterType
  [ 0] +0x00 0x1400016d0  OWN AmbientEmitterComponent_ScalarDtor (106 B)
  [ 1] +0x08 0x1400014a0  TINY returns qword [0x1401c3e68]
  [ 2] +0x10 0x140001490  TINY returns dword [0x1401c3e60] = TypeId(AmbientEmitterComponent)
  [ 3] +0x18 0x140042f30  OWN AmbientEmitterComponent_Shutdown (290 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140042ba0  OWN AmbientEmitterComponent_Draw (87 B)
  [ 7] +0x38 0x140042b90  JMP 0x140042d70 FUN_140042d70
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x1400016f4 in 0x1400016d0 AmbientEmitterComponent_ScalarDtor; this+0x0 at 0x14004307d in 0x140043060 AmbientEmitterComponent_Ctor; this+0x0 at 0x1400430e8 in 0x1400430c0 AmbientEmitterComponent_Ctor
### AmbientEmitterDescription  table=0x140158ae0  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x45746e6569626d41 (string "AmbientEnvironmentComponent")
  reflected: ti=0x1401c3e98 create=0x140001510 createInPlace=0x140001580 AmbientEmitterDescription_CreateInPlace registered in 0x14014a640 AmbientEmitterDescription_Register
  [ 0] +0x00 0x140001660  OWN AmbientEmitterComponent_vslot16 (106 B)
  [ 1] +0x08 0x140001500  TINY returns qword [0x1401c3ea0]
  [ 2] +0x10 0x1400014f0  TINY returns dword [0x1401c3e98] = TypeId(AmbientEmitterDescription)
  [ 3] +0x18 0x140043250  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400431b0  OWN FUN_1400431b0 (149 B)
  [ 6] +0x30 0x1400015e0  OWN FUN_1400015e0 (83 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140001650  OWN (no Ghidra function)
  [ 9] +0x48 0x140001580  OWN AmbientEmitterDescription_CreateInPlace (82 B)
  stamped by (6): other+0x0 at 0x140001555 in 0x140001510 AmbientEmitterDescription_Factory; this+0x0 at 0x1400015b4 in 0x140001580 AmbientEmitterDescription_CreateInPlace; this+0x0 at 0x140001619 in 0x1400015e0 FUN_1400015e0; this+0x0 at 0x140001684 in 0x140001660 AmbientEmitterComponent_vslot16; this+0x0 at 0x14004327d in 0x140043260 FUN_140043260; this+0x0 at 0x1400432ee in 0x1400432c0 FUN_1400432c0
### AmbientEnvironmentComponent  table=0x140158b70  slots=16  sizeof=0x48  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c3ed0 create=0x140001ee0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014a680 AmbientEnvironmentComponent_RegisterType
  [ 0] +0x00 0x140002170  OWN AmbientEnvironmentComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x140001ed0  TINY returns qword [0x1401c3ed8]
  [ 2] +0x10 0x140001ec0  TINY returns dword [0x1401c3ed0] = TypeId(AmbientEnvironmentComponent)
  [ 3] +0x18 0x1400437e0  OWN AmbientEnvironmentComponent_Shutdown (159 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140043690  JMP 0x140043310 AmbientEnvironmentComponent_Update_Impl
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140002194 in 0x140002170 AmbientEnvironmentComponent_ScalarDtor; this+0x0 at 0x1400438a9 in 0x140043890 AmbientEnvironmentComponent_Ctor; this+0x0 at 0x1400438f8 in 0x1400438d0 AmbientEnvironmentComponent_Ctor
### AmbientEnvironmentDescription  table=0x140158bf0  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6c6c6f72746e6f43 (string "ControllerComponent")
  reflected: ti=0x1401c3f08 create=0x140001f40 createInPlace=0x140001fb0 AmbientEnvironmentDescription_CreateInPlace registered in 0x14014a6c0 AmbientEnvironmentDescription_Register
  [ 0] +0x00 0x140002100  OWN AmbientEnvironmentComponent_vslot16 (106 B)
  [ 1] +0x08 0x140001f30  TINY returns qword [0x1401c3f10]
  [ 2] +0x10 0x140001f20  TINY returns dword [0x1401c3f08] = TypeId(AmbientEnvironmentDescription)
  [ 3] +0x18 0x140043a70  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140043950  OWN FUN_140043950 (267 B)
  [ 6] +0x30 0x140002010  OWN FUN_140002010 (93 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140002070  OWN (no Ghidra function)
  [ 9] +0x48 0x140001fb0  OWN AmbientEnvironmentDescription_CreateInPlace (92 B)
  stamped by (6): other+0x0 at 0x140001f85 in 0x140001f40 AmbientEnvironmentDescription_Factory; this+0x0 at 0x140001fe4 in 0x140001fb0 AmbientEnvironmentDescription_CreateInPlace; this+0x0 at 0x140002049 in 0x140002010 FUN_140002010; this+0x0 at 0x140002124 in 0x140002100 AmbientEnvironmentComponent_vslot16; this+0x0 at 0x140043a9d in 0x140043a80 FUN_140043a80; this+0x0 at 0x140043b0e in 0x140043ae0 FUN_140043ae0
### AnimalComponent  table=0x140162110  slots=16  sizeof=0x58  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c6f98 create=0x14002f020 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e510 AnimalComponent_RegisterType
  [ 0] +0x00 0x14002f170  OWN AnimalComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x14002f010  TINY returns qword [0x1401c6fa0]
  [ 2] +0x10 0x14002f000  TINY returns dword [0x1401c6f98] = TypeId(AnimalComponent)
  [ 3] +0x18 0x140106e40  OWN AnimalComponent_Serialize (311 B)
  [ 4] +0x20 0x140106d50  OWN AnimalComponent_Startup (109 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140106c20  OWN AnimalComponent_OnAdd (298 B)
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140106420  OWN AnimalComponent_Update (1763 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140106dd0  OWN AnimalComponent_OnUpdate2 (104 B)
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x14002f194 in 0x14002f170 AnimalComponent_ScalarDtor; this+0x0 at 0x140106f99 in 0x140106f80 AnimalComponent_Ctor; this+0x0 at 0x140106fe8 in 0x140106fc0 AnimalComponent_Ctor
### AnimalDescription  table=0x140162190  slots=10  sizeof=0x58  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x736544646c697542 (string "BuildDescription")
  reflected: ti=0x1401c6fd0 create=0x14002f080 createInPlace=0x14002f0c0 AnimalDescription_CreateInPlace registered in 0x14014e550 AnimalDescription_Register
  [ 0] +0x00 0x14002f100  OWN AnimalComponent_GetTypeId (106 B)
  [ 1] +0x08 0x14002f070  TINY returns qword [0x1401c6fd8]
  [ 2] +0x10 0x14002f060  TINY returns dword [0x1401c6fd0] = TypeId(AnimalDescription)
  [ 3] +0x18 0x140107170  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140107050  OWN AnimalComponent_OnCreate (275 B)
  [ 6] +0x30 0x14002f0d0  OWN AnimalComponent_OnDestroy (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14002f0f0  OWN AnimalComponent_OnCleanup (12 B)
  [ 9] +0x48 0x14002f0c0  OWN AnimalDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x14002f124 in 0x14002f100 AnimalComponent_GetTypeId; this+0x0 at 0x14010719d in 0x140107180 FUN_140107180; this+0x0 at 0x14010720e in 0x1401071e0 FUN_1401071e0
### AnimateNaturalResource  table=0x14015fb10  slots=10  sizeof=0x30  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x6574656c706d6f43 (string "CompleteRoad")
  reflected: ti=0x1401c5de0 create=0x140021dc0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d050 AnimateNaturalResource_RegisterType
  [ 0] +0x00 0x140021e30  OWN AnimateNaturalResource_ScalarDtor (104 B)
  [ 1] +0x08 0x140021db0  TINY returns qword [0x1401c5de8]
  [ 2] +0x10 0x140021da0  TINY returns dword [0x1401c5de0] = TypeId(AnimateNaturalResource)
  [ 3] +0x18 0x1400c5660  OWN AnimateNaturalResource_Shutdown (196 B)
  [ 4] +0x20 0x1400c5600  OWN AnimateNaturalResource_Serialize (80 B)
  [ 5] +0x28 0x1400c55b0  OWN AnimateNaturalResource_Slot5 (73 B)
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400c55a0  TINY RET 0 (c2 00 00)
  [ 9] +0x48 0x1400c5560  OWN AnimateNaturalResource_Update (46 B)
  stamped by (5): other+0x0 at 0x140021e11 in 0x140021dc0 AnimateNaturalResource_Factory; this+0x0 at 0x140021e54 in 0x140021e30 AnimateNaturalResource_ScalarDtor; this+0x0 at 0x1400c574d in 0x1400c5730 Action_Action_Ctor16; this+0x0 at 0x1400c57b7 in 0x1400c5780 AnimateNaturalResource_Action_Ctor; this+0x0 at 0x1400c5820 in 0x1400c57d0 AnimateNaturalResource_Action_CopyCtor
### AnimAudioEvent  table=0x14015e0c0  slots=10  sizeof=0x30  parent="Graphics::AnimationEvent" / structural Graphics::AnimationEvent (Runtime ctor import)  stop=non-code qword 0x3a3a7463656a624f (string "Object::External")
  reflected: ti=0x1401c58d8 create=0x140016f00 createInPlace=0x140016f60 AnimAudioEvent_RegisterType_sub_6f60 registered in 0x14014ca80 AnimAudioEvent_RegisterType
  [ 0] +0x00 0x140017000  OWN WorkPlaceUI_vslot18 (73 B)
  [ 1] +0x08 0x140016ef0  TINY returns qword [0x1401c58e0]
  [ 2] +0x10 0x140016ee0  TINY returns dword [0x1401c58d8] = TypeId(AnimAudioEvent)
  [ 3] +0x18 0x140087f30  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x140087ed0  OWN FUN_140087ed0 (85 B)
  [ 6] +0x30 0x140016fb0  OWN FUN_140016fb0 (74 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140087e40  OWN FUN_140087e40 (134 B)
  [ 9] +0x48 0x140016f60  OWN AnimAudioEvent_RegisterType_sub_6f60 (73 B)
  stamped by (6): other+0x0 at 0x140016f3b in 0x140016f00 AnimAudioEvent_Factory; this+0x0 at 0x140016f8a in 0x140016f60 AnimAudioEvent_RegisterType_sub_6f60; this+0x0 at 0x140016fdf in 0x140016fb0 FUN_140016fb0; this+0x0 at 0x140017024 in 0x140017000 WorkPlaceUI_vslot18; this+0x0 at 0x140087f59 in 0x140087f40 FUN_140087f40; this+0x0 at 0x140087f94 in 0x140087f70 FUN_140087f70
### AttendanceComponent  table=0x140158ce0  slots=18  sizeof=0x58  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c3f40 create=0x140002670 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014a700 AttendanceComponent_RegisterType
  member vptrs: +0x38=0x140158c90, +0x38=0x14015f070
  [ 0] +0x00 0x140002860  OWN AttendanceComponent_Controller_Slot10 (48 B)
  [ 1] +0x08 0x140002660  TINY returns qword [0x1401c3f48]
  [ 2] +0x10 0x140002650  TINY returns dword [0x1401c3f40] = TypeId(AttendanceComponent)
  [ 3] +0x18 0x140044410  OWN AttendanceComponent_Controller_Slot13 (237 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x1400443d0  OWN AttendanceComponent_OnCreate (51 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140044370  OWN AttendanceComponent_Update (85 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140043f10  OWN AttendanceComponent_Slot13 (68 B)
  [14] +0x70 0x140043e80  OWN AttendanceComponent_Slot14 (124 B)
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x1400026b0  TINY returns this+0x38
  stamped by (2): this+0x0 at 0x14004452d in 0x140044510 AttendanceComponent_Ctor; this+0x0 at 0x1400445b2 in 0x140044580 AttendanceComponent_Ctor
### AttendanceDescription  table=0x140158d70  slots=10  sizeof=0x28  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6b6369506f747541 (string "AutoPickupComponent")
  reflected: ti=0x1401c3f78 create=0x1400026f0 createInPlace=0x140002750 AttendanceDescription_CreateInPlace registered in 0x14014a740 AttendanceDescription_Register
  [ 0] +0x00 0x140002800  OWN AttendanceComponent_vslot18 (83 B)
  [ 1] +0x08 0x1400026e0  TINY returns qword [0x1401c3f80]
  [ 2] +0x10 0x1400026d0  TINY returns dword [0x1401c3f78] = TypeId(AttendanceDescription)
  [ 3] +0x18 0x140044700  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400446b0  OWN FUN_1400446b0 (71 B)
  [ 6] +0x30 0x1400027a0  OWN FUN_1400027a0 (76 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400027f0  OWN (no Ghidra function)
  [ 9] +0x48 0x140002750  OWN AttendanceDescription_CreateInPlace (75 B)
  stamped by (6): other+0x0 at 0x140002735 in 0x1400026f0 AttendanceDescription_Factory; this+0x0 at 0x140002784 in 0x140002750 AttendanceDescription_CreateInPlace; this+0x0 at 0x1400027d9 in 0x1400027a0 FUN_1400027a0; this+0x0 at 0x140002824 in 0x140002800 AttendanceComponent_vslot18; this+0x0 at 0x140044729 in 0x140044710 FUN_140044710; this+0x0 at 0x14004477e in 0x140044750 FUN_140044750
### AttendanceUI  table=0x14015b070  slots=11  sizeof=0x38  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x6e61646e65747441 (string "AttendanceUI")
  reflected: ti=0x1401c4cd0 create=0x1400104b0 createInPlace=0x140010510 Cleanup_AttendanceUI registered in 0x14014bcc0 RegisterType_AttendanceUI
  [ 0] +0x00 0x140010560  OWN AchievementDialog_Slot10 (73 B)
  [ 1] +0x08 0x1400104a0  TINY returns qword [0x1401c4cd8]
  [ 2] +0x10 0x140010490  TINY returns dword [0x1401c4cd0] = TypeId(AttendanceUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x14005dd40  OWN AchievementDialog_Slot14 (256 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14005dc50  OWN AttendanceUI_OnCreate (224 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14003a972  IMPORT ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z  [in 11 tables]
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (5): other+0x0 at 0x1400104eb in 0x1400104b0 Factory_AttendanceUI; this+0x0 at 0x14001053a in 0x140010510 Cleanup_AttendanceUI; this+0x0 at 0x140010584 in 0x140010560 AchievementDialog_Slot10; this+0x0 at 0x14005de69 in 0x14005de50 AttendanceUI_Ctor; this+0x0 at 0x14005dea4 in 0x14005de80 AttendanceUI_CopyCtor
### AutoPickupComponent  table=0x140158df0  slots=16  sizeof=0x40  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c3fb0 create=0x1400033c0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014a800 AutoPickupComponent_RegisterType
  [ 0] +0x00 0x140003590  OWN AutoPickupComponent_ScalarDtor (106 B)
  [ 1] +0x08 0x1400033b0  TINY returns qword [0x1401c3fb8]
  [ 2] +0x10 0x1400033a0  TINY returns dword [0x1401c3fb0] = TypeId(AutoPickupComponent)
  [ 3] +0x18 0x1400449d0  OWN AutoPickupComponent_Shutdown (162 B)
  [ 4] +0x20 0x140044990  OWN AutoPickupComponent_Serialize (58 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140044970  OWN AutoPickupComponent_Slot13 (15 B)
  [14] +0x70 0x140044930  OWN AutoPickupComponent_Slot14 (49 B)
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x1400035b4 in 0x140003590 AutoPickupComponent_ScalarDtor; this+0x0 at 0x140044a9d in 0x140044a80 AutoPickupComponent_Ctor; this+0x0 at 0x140044b08 in 0x140044ae0 AutoPickupComponent_Ctor
### AutoPickupDescription  table=0x140158e70  slots=10  sizeof=0x28  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x706d6f4374616f42 (string "BoatComponent")
  reflected: ti=0x1401c3fe8 create=0x140003420 createInPlace=0x140003480 AutoPickupDescription_CreateInPlace registered in 0x14014a840 AutoPickupDescription_Register
  [ 0] +0x00 0x140003530  OWN AutoPickupComponent_vslot16 (83 B)
  [ 1] +0x08 0x140003410  TINY returns qword [0x1401c3ff0]
  [ 2] +0x10 0x140003400  TINY returns dword [0x1401c3fe8] = TypeId(AutoPickupDescription)
  [ 3] +0x18 0x140044bb0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140044b60  OWN FUN_140044b60 (71 B)
  [ 6] +0x30 0x1400034d0  OWN FUN_1400034d0 (73 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140003520  OWN (no Ghidra function)
  [ 9] +0x48 0x140003480  OWN AutoPickupDescription_CreateInPlace (72 B)
  stamped by (6): other+0x0 at 0x140003465 in 0x140003420 AutoPickupDescription_Factory; this+0x0 at 0x1400034b4 in 0x140003480 AutoPickupDescription_CreateInPlace; this+0x0 at 0x140003509 in 0x1400034d0 FUN_1400034d0; this+0x0 at 0x140003554 in 0x140003530 AutoPickupComponent_vslot16; this+0x0 at 0x140044bd9 in 0x140044bc0 FUN_140044bc0; this+0x0 at 0x140044c2e in 0x140044c00 FUN_140044c00
### AutoPickupWork  table=0x140160fa0  slots=13  sizeof=abstract  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x616f52646c697542 (string "BuildRoad")
  reflected: ti=0x1401c6828 create=0x140003730 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014dc10 AutoPickupWork_RegisterType
  [ 0] +0x00 0x1400294e0  OWN AutoPickupWork_ScalarDtor (83 B)
  [ 1] +0x08 0x1400294d0  TINY returns qword [0x1401c6830]
  [ 2] +0x10 0x1400294c0  TINY returns dword [0x1401c6828] = TypeId(AutoPickupWork)
  [ 3] +0x18 0x1400e3f80  OWN AutoPickupWork_Shutdown (48 B)
  [ 4] +0x20 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 8] +0x40 0x1400e3f30  OWN AutoPickupWork_OnRemove (74 B)
  [ 9] +0x48 0x14003addc  _purecall  [in 12 tables]   = parent
  [10] +0x50 0x14003addc  _purecall  [in 12 tables]   = parent
  [11] +0x58 0x14003addc  _purecall  [in 12 tables]   = parent
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (19): this+0x0 at 0x140029504 in 0x1400294e0 AutoPickupWork_ScalarDtor; other+0x0 at 0x14004b1b0 in 0x14004b160 HerbalistWork_Ctor; this+0x0 at 0x1400e3fd9 in 0x1400e3fc0 Work_CopyCtor; this+0x0 at 0x1400e4030 in 0x1400e4000 AutoPickupWork_Ctor; this+0x0 at 0x1400e40a7 in 0x1400e4050 AutoPickupWork_Ctor; this+0x0 at 0x1400eba6b in 0x1400eba00 Work_Ctor; this+0x0 at 0x1400ebad0 in 0x1400ebaa0 ForesterWork_Ctor; this+0x0 at 0x1400ed48a in 0x1400ed410 Work_Ctor7; this+0x0 at 0x1400ed4f8 in 0x1400ed4c0 HarvestArea_Ctor; this+0x0 at 0x1400edfda in 0x1400edf60 Work_Ctor6; this+0x0 at 0x1400ee096 in 0x1400ee060 HarvestOrchard_Ctor; this+0x0 at 0x1400eecab in 0x1400eec40 Work_Ctor4 ...
### AutoSaveDialog  table=0x14015b120  slots=10  sizeof=0xd0  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x716552646c697542 (string "BuildRequirementUI")
  reflected: ti=0x1401c4d08 create=0x1400106c0 createInPlace=0x140010730 Cleanup_AutoSaveDialog registered in 0x14014bd00 RegisterType_AutoSaveDialog
  [ 0] +0x00 0x140010780  OWN AutoSaveDialog_ScalarDtor (73 B)
  [ 1] +0x08 0x1400106b0  TINY returns qword [0x1401c4d10]
  [ 2] +0x10 0x1400106a0  TINY returns dword [0x1401c4d08] = TypeId(AutoSaveDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x14005e110  JMP 0x14005e120 thunk_helper_e120
  [ 5] +0x28 0x14003a94e  IMPORT ?OnClosed@DialogInstance@UI@@UEAAXXZ  [in 12 tables]
  [ 6] +0x30 0x14005dfc0  OWN AutoSaveDialog_Draw (198 B)
  [ 7] +0x38 0x14005e090  OWN AutoSaveDialog_OnCreate (78 B)
  [ 8] +0x40 0x14005e0f0  OWN AutoSaveDialog_OnRemove (12 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (6): other+0x0 at 0x1400106fd in 0x1400106c0 Factory_AutoSaveDialog; this+0x0 at 0x14001075a in 0x140010730 Cleanup_AutoSaveDialog; this+0x0 at 0x1400107a4 in 0x140010780 AutoSaveDialog_ScalarDtor; this+0x0 at 0x14005e219 in 0x14005e200 AutoSaveDialog_Ctor; this+0x0 at 0x14005e272 in 0x14005e230 AutoSaveDialog_CopyCtor; this+0x0 at 0x14005e344 in 0x14005e320 AutoSaveDialog_Ctor3
### BoatComponent  table=0x140158f40  slots=18  sizeof=0x98  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c4020 create=0x1400036e0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014a880 BoatComponent_RegisterType
  member vptrs: +0x78=0x140158ef0, +0x78=0x14015f070
  [ 0] +0x00 0x140003970  OWN BoatComponent_ScalarDtor (48 B)
  [ 1] +0x08 0x1400036d0  TINY returns qword [0x1401c4028]
  [ 2] +0x10 0x1400036c0  TINY returns dword [0x1401c4020] = TypeId(BoatComponent)
  [ 3] +0x18 0x1400465d0  OWN BoatComponent_Serialize (397 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x140003730  TINY return 0 (xor eax,eax)  [in 5 tables]
  [17] +0x88 0x140003720  TINY returns this+0x78
  stamped by (2): this+0x0 at 0x140046792 in 0x140046770 BoatComponent_Ctor; this+0x0 at 0x14004682d in 0x140046800 BoatComponent_Ctor
### BoatDescription  table=0x140158fd0  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x79726574656d6543 (string "CemeteryComponent")
  reflected: ti=0x1401c4058 create=0x140003760 createInPlace=0x1400037c0 BoatDescription_CreateInPlace registered in 0x14014a8c0 BoatDescription_Register
  [ 0] +0x00 0x140003910  OWN BoatComponent_GetParentType (83 B)
  [ 1] +0x08 0x140003750  TINY returns qword [0x1401c4060]
  [ 2] +0x10 0x140003740  TINY returns dword [0x1401c4058] = TypeId(BoatDescription)
  [ 3] +0x18 0x140046a00  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400469c0  OWN BoatComponent_GetDescription (46 B)
  [ 6] +0x30 0x140003810  OWN BoatComponent_OnCleanup (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140003860  OWN (no Ghidra function)
  [ 9] +0x48 0x1400037c0  OWN BoatDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x1400037a5 in 0x140003760 BoatDescription_Factory; this+0x0 at 0x1400037f4 in 0x1400037c0 BoatDescription_CreateInPlace; this+0x0 at 0x140003849 in 0x140003810 BoatComponent_OnCleanup; this+0x0 at 0x140003934 in 0x140003910 BoatComponent_GetParentType; this+0x0 at 0x140046a29 in 0x140046a10 FUN_140046a10; this+0x0 at 0x140046a7e in 0x140046a50 FUN_140046a50
### BridgeTool  table=0x140161b00  slots=15  sizeof=0x40  parent="DragTool" / structural DragTool 0x140161c10  stop=non-code qword 0x63616c5067617244 (string "DragPlacementTool")
  reflected: ti=0x1401c6d30 create=0x14002d7a0 createInPlace=0x14002d7e0 <no fn> registered in 0x14014e1d0 BridgeTool_RegisterType
  [ 0] +0x00 0x14002d7f0  OWN BridgeTool_ScalarDtor (93 B)
  [ 1] +0x08 0x14002d790  TINY returns qword [0x1401c6d38]
  [ 2] +0x10 0x14002d780  TINY returns dword [0x1401c6d30] = TypeId(BridgeTool)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  [ 4] +0x20 0x1400fb7a0  OWN BridgeTool_Serialize (146 B)
  [ 5] +0x28 0x1400fb790  TINY RET 0 (c2 00 00)
  [ 6] +0x30 0x1400fc6b0  OWN BridgeTool_Draw (785 B)  [in 4 tables]   = parent
  [ 7] +0x38 0x1400fb2c0  TINY RET 0 (c2 00 00)
  [ 8] +0x40 0x1400fadc0  OWN BridgeTool_OnRemove (1273 B)
  [ 9] +0x48 0x1400fc640  OWN BridgeTool_Update (103 B)  [in 5 tables]   = parent
  [10] +0x50 0x1400268a0  TINY return false (xor al,al)  [in 8 tables]   = parent
  [11] +0x58 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]   = parent
  [12] +0x60 0x1400fc9d0  TINY return 5 (mov eax)  [in 6 tables]   = parent
  [13] +0x68 0x1400f9cd0  OWN BridgeTool_Slot13 (4110 B)
  [14] +0x70 0x1400facf0  OWN BridgeTool_Slot14 (134 B)
  stamped by (3): this+0x0 at 0x14002d814 in 0x14002d7f0 BridgeTool_ScalarDtor; this+0x0 at 0x1400fb859 in 0x1400fb840 Tool_Ctor3; this+0x0 at 0x1400fb8e5 in 0x1400fb890 BridgeTool_Ctor
### BuildComponent  table=0x140162260  slots=18  sizeof=0x78  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c7008 create=0x14002f2b0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e590 BuildComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x140162210
  [ 0] +0x00 0x14002f4d0  OWN BuildComponent_Controller_Slot10 (48 B)
  [ 1] +0x08 0x14002f2a0  TINY returns qword [0x1401c7010]
  [ 2] +0x10 0x14002f290  TINY returns dword [0x1401c7008] = TypeId(BuildComponent)
  [ 3] +0x18 0x140108d80  OWN BuildComponent_Controller_Slot13 (418 B)
  [ 4] +0x20 0x140108d60  TINY RET 0 (c2 00 00)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140108d40  OWN BuildComponent_Draw (22 B)
  [ 7] +0x38 0x140108cb0  OWN BuildComponent_OnCreate (123 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140108d70  TINY RET 0 (c2 00 00)
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140108710  OWN BuildComponent_Slot15 (9 B)
  [16] +0x80 0x14002f2f0  TINY return 1 (mov eax)
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (2): this+0x0 at 0x140108f52 in 0x140108f30 BuildComponent_Ctor; this+0x0 at 0x140108ff2 in 0x140108fc0 BuildComponent_Ctor
### BuildDescription  table=0x1401622f0  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6574636172616843 (string "CharacterDescription")
  reflected: ti=0x1401c7040 create=0x14002f320 createInPlace=0x14002f390 BuildDescription_CreateInPlace registered in 0x14014e5d0 BuildDescription_Register
  [ 0] +0x00 0x14002f460  OWN BuildComponent_vslot18 (106 B)
  [ 1] +0x08 0x14002f310  TINY returns qword [0x1401c7048]
  [ 2] +0x10 0x14002f300  TINY returns dword [0x1401c7040] = TypeId(BuildDescription)
  [ 3] +0x18 0x140109220  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140109140  OWN BuildDescription_Serialize (218 B)
  [ 6] +0x30 0x14002f3f0  OWN FUN_14002f3f0 (85 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14002f450  OWN (no Ghidra function)
  [ 9] +0x48 0x14002f390  OWN BuildDescription_CreateInPlace (84 B)
  stamped by (6): other+0x0 at 0x14002f365 in 0x14002f320 BuildDescription_Factory; this+0x0 at 0x14002f3c4 in 0x14002f390 BuildDescription_CreateInPlace; this+0x0 at 0x14002f429 in 0x14002f3f0 FUN_14002f3f0; this+0x0 at 0x14002f484 in 0x14002f460 BuildComponent_vslot18; this+0x0 at 0x14010924d in 0x140109230 FUN_140109230; this+0x0 at 0x1401092be in 0x140109290 FUN_140109290
### BuildingGoal  table=0x140160300  slots=11  sizeof=0x48  parent="Achievement::Goal" / structural Achievement::Goal (Runtime ctor import)  stop=non-code qword 0x6974616c75706f50 (string "PopulationGoal")
  reflected: ti=0x1401c61d0 create=0x140025d80 createInPlace=0x140025de0 BuildingGoal_Cleanup registered in 0x14014d4d0 BuildingGoal_RegisterType
  [ 0] +0x00 0x140025e80  OWN BuildingGoal_ScalarDtor (86 B)
  [ 1] +0x08 0x140025d70  TINY returns qword [0x1401c61d8]
  [ 2] +0x10 0x140025d60  TINY returns dword [0x1401c61d0] = TypeId(BuildingGoal)
  [ 3] +0x18 0x1400d7790  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400d76a0  OWN BuildingGoal_Slot5 (222 B)
  [ 6] +0x30 0x140025e30  OWN BuildingGoal_Draw (73 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400d7580  OWN BuildingGoal_OnRemove (270 B)
  [ 9] +0x48 0x14003ab5e  IMPORT ?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ  [in 3 tables]
  [10] +0x50 0x140025de0  OWN BuildingGoal_Cleanup (72 B)
  stamped by (6): other+0x0 at 0x140025dbb in 0x140025d80 BuildingGoal_Factory; this+0x0 at 0x140025e0a in 0x140025de0 BuildingGoal_Cleanup; this+0x0 at 0x140025e5f in 0x140025e30 BuildingGoal_Draw; this+0x0 at 0x140025ea4 in 0x140025e80 BuildingGoal_ScalarDtor; this+0x0 at 0x1400d77bd in 0x1400d77a0 BuildingGoal_Ctor; this+0x0 at 0x1400d7804 in 0x1400d77e0 BuildingGoal_CopyCtor
### BuildRequirementUI  table=0x14015b1d0  slots=11  sizeof=0x60  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x79726574656d6543 (string "CemeteryUI")
  reflected: ti=0x1401c4d40 create=0x1400107f0 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014bd40 RegisterType_BuildRequirementUI
  [ 0] +0x00 0x140010860  OWN BuildRequirementUI_ScalarDtor (124 B)
  [ 1] +0x08 0x1400107e0  TINY returns qword [0x1401c4d48]
  [ 2] +0x10 0x1400107d0  TINY returns dword [0x1401c4d40] = TypeId(BuildRequirementUI)
  [ 3] +0x18 0x14005ee80  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x14005e7c0  OWN BuildRequirementUI_Serialize (1721 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14003a97e  IMPORT ?OnUpdateLayout@DialogController@UI@@UEAAXXZ  [in 10 tables]
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14003a972  IMPORT ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z  [in 11 tables]
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x14001082b in 0x1400107f0 Factory_BuildRequirementUI; this+0x0 at 0x140010889 in 0x140010860 BuildRequirementUI_ScalarDtor; this+0x0 at 0x14005eeb2 in 0x14005ee90 BuildRequirementUI_Ctor; this+0x0 at 0x14005ef24 in 0x14005ef00 BuildRequirementUI_CopyCtor
### BuildRoad  table=0x140161020  slots=13  sizeof=0x70  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x727453646c697542 (string "BuildStructure")
  reflected: ti=0x1401c6860 create=0x140029560 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014dc50 BuildRoad_RegisterType
  [ 0] +0x00 0x1400295b0  OWN BuildRoad_ScalarDtor (83 B)
  [ 1] +0x08 0x140029550  TINY returns qword [0x1401c6868]
  [ 2] +0x10 0x140029540  TINY returns dword [0x1401c6860] = TypeId(BuildRoad)
  [ 3] +0x18 0x1400e4980  OWN BuildRoad_Shutdown (262 B)
  [ 4] +0x20 0x1400e4190  OWN BuildRoad_Serialize (1899 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400e4150  OWN BuildRoad_Draw (55 B)
  [ 7] +0x38 0x1400e4120  OWN BuildRoad_OnCreate (37 B)
  [ 8] +0x40 0x1400e4910  OWN BuildRoad_OnRemove (69 B)
  [ 9] +0x48 0x1400e40c0  OWN BuildRoad_Update (78 B)
  [10] +0x50 0x1400e4960  OWN BuildRoad_Slot10 (25 B)
  [11] +0x58 0x1400295a0  OWN BuildRoad_Slot11 (10 B)  [in 2 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (4): this+0x0 at 0x1400295d4 in 0x1400295b0 BuildRoad_ScalarDtor; this+0x0 at 0x1400e4aa9 in 0x1400e4a90 Work_Ctor8; this+0x0 at 0x1400e4b27 in 0x1400e4ad0 BuildRoad_Ctor; this+0x0 at 0x1400e4b90 in 0x1400e4b50 BuildRoad_Ctor
### BuildStructure  table=0x1401610a0  slots=13  sizeof=0x70  parent="Work" / structural Work 0x140161a80  stop=zero qword
  reflected: ti=0x1401c6898 create=0x140029630 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014dc90 BuildStructure_RegisterType
  [ 0] +0x00 0x140029680  OWN BuildStructure_ScalarDtor (48 B)
  [ 1] +0x08 0x140029620  TINY returns qword [0x1401c68a0]
  [ 2] +0x10 0x140029610  TINY returns dword [0x1401c6898] = TypeId(BuildStructure)
  [ 3] +0x18 0x1400e5850  OWN BuildStructure_Shutdown (194 B)
  [ 4] +0x20 0x1400e4fc0  OWN BuildStructure_Serialize (2009 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400e4f80  OWN BuildStructure_Draw (46 B)
  [ 7] +0x38 0x1400e4f40  OWN BuildStructure_OnCreate (44 B)
  [ 8] +0x40 0x1400e57a0  OWN BuildStructure_OnRemove (132 B)
  [ 9] +0x48 0x1400e4be0  OWN BuildStructure_Update (844 B)
  [10] +0x50 0x1400e5830  OWN BuildStructure_Slot10 (25 B)
  [11] +0x58 0x140029670  OWN BuildStructure_Slot11 (9 B)
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (3): this+0x0 at 0x1400e5947 in 0x1400e5920 Work_Ctor9; this+0x0 at 0x1400e59ff in 0x1400e59c0 BuildStructure_Ctor; this+0x0 at 0x1400e5ae7 in 0x1400e5a90 BuildStructure_Ctor
### BuildUI  table=0x14015f210  slots=11  sizeof=0x50  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x4955706f7243 (string "CropUI")
  reflected: ti=0x1401c5b78 create=0x1400209e0 createInPlace=0x140020a40 Cleanup_BuildUI registered in 0x14014cd90 RegisterType_BuildUI
  [ 0] +0x00 0x140020a90  OWN BuildUI_ScalarDtor (96 B)
  [ 1] +0x08 0x1400209d0  TINY returns qword [0x1401c5b80]
  [ 2] +0x10 0x1400209c0  TINY returns dword [0x1401c5b78] = TypeId(BuildUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x1400b1f40  OWN BuildUI_Serialize (2355 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x1400b1a50  OWN BuildUI_OnCreate (1095 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x1400b1ea0  OWN BuildUI_Update (148 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (5): other+0x0 at 0x140020a1b in 0x1400209e0 Factory_BuildUI; this+0x0 at 0x140020a6a in 0x140020a40 Cleanup_BuildUI; this+0x0 at 0x140020ab4 in 0x140020a90 BuildUI_ScalarDtor; this+0x0 at 0x1400b289d in 0x1400b2880 BuildUI_Ctor; this+0x0 at 0x1400b28f4 in 0x1400b28d0 BuildUI_CopyCtor
### CaptureBoxDescription  table=0x14015d4a0  slots=10  sizeof=0x78  parent="ElementDescription" / structural UI::ElementDescription (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c5600 create=0x140013d90 createInPlace=0x140013df0 CaptureBoxDescription_CreateInPlace registered in 0x14014c740 CaptureBoxDescription_RegisterType
  [ 0] +0x00 0x140014010  OWN CaptureBoxElement_Slot12 (87 B)
  [ 1] +0x08 0x140013d80  TINY returns qword [0x1401c5608]
  [ 2] +0x10 0x140013d70  TINY returns dword [0x1401c5600] = TypeId(CaptureBoxDescription)
  [ 3] +0x18 0x140079210  OWN CaptureBoxElement_Slot15 (19 B)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400791a0  OWN CaptureBoxDescription_Slot5 (104 B)
  [ 6] +0x30 0x140013e40  OWN CaptureBoxDescription_Draw (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400790a0  OWN CaptureBoxDescription_OnRemove (235 B)
  [ 9] +0x48 0x140013df0  OWN CaptureBoxDescription_CreateInPlace (74 B)
  stamped by (6): other+0x0 at 0x140013dcb in 0x140013d90 CaptureBoxDescription_Factory; this+0x0 at 0x140013e1a in 0x140013df0 CaptureBoxDescription_CreateInPlace; this+0x0 at 0x140013e6f in 0x140013e40 CaptureBoxDescription_Draw; this+0x0 at 0x140014034 in 0x140014010 CaptureBoxElement_Slot12; this+0x0 at 0x14007924d in 0x140079230 CaptureBoxDescription_Ctor; this+0x0 at 0x140079294 in 0x140079270 CaptureBoxDescription_CopyCtor
### CaptureBoxElement  table=0x14015d440  slots=12  sizeof=0xe0  parent="Element" / structural UI::Element (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c5638 create=0x140013eb0 createInPlace=0x140013f20 CaptureBoxElement_Cleanup registered in 0x14014c780 CaptureBoxElement_RegisterType
  [ 0] +0x00 0x140014070  OWN CaptureBoxElement_ScalarDtor (73 B)
  [ 1] +0x08 0x140013ea0  TINY returns qword [0x1401c5640]
  [ 2] +0x10 0x140013e90  TINY returns dword [0x1401c5638] = TypeId(CaptureBoxElement)
  [ 3] +0x18 0x14003a9b4  IMPORT ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z  [in 2 tables]
  [ 4] +0x20 0x140078fb0  OWN CaptureBoxElement_OnCreate (35 B)
  [ 5] +0x28 0x140078ee0  OWN CaptureBoxElement_Slot5 (192 B)
  [ 6] +0x30 0x140078da0  OWN CaptureBoxElement_Draw (309 B)
  [ 7] +0x38 0x14003a9ba  IMPORT ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z  [in 2 tables]
  [ 8] +0x40 0x14003a9c0  IMPORT ?Enable@Element@UI@@UEAAX_N@Z  [in 2 tables]
  [ 9] +0x48 0x14003a9c6  IMPORT ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z  [in 2 tables]
  [10] +0x50 0x14003a9d2  IMPORT ?SetColor@Element@UI@@UEAAXI@Z  [in 2 tables]
  [11] +0x58 0x14003a9cc  IMPORT ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ  [in 2 tables]
  stamped by (6): other+0x0 at 0x140013eed in 0x140013eb0 CaptureBoxElement_Factory; this+0x0 at 0x140013f4a in 0x140013f20 CaptureBoxElement_Cleanup; this+0x0 at 0x140014094 in 0x140014070 CaptureBoxElement_ScalarDtor; this+0x0 at 0x140079029 in 0x140079010 CaptureBoxElement_Ctor; this+0x0 at 0x140079064 in 0x140079040 CaptureBoxElement_CopyCtor; other+0x0 at 0x1400790ed in 0x1400790a0 CaptureBoxDescription_OnRemove
### CemeteryComponent  table=0x1401590a0  slots=18  sizeof=0x58  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c4090 create=0x140004040 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014a980 CemeteryComponent_RegisterType
  member vptrs: +0x28=0x140159050, +0x28=0x14015f070
  [ 0] +0x00 0x140004220  OWN CemeteryComponent_Controller_Slot10 (48 B)
  [ 1] +0x08 0x140004030  TINY returns qword [0x1401c4098]
  [ 2] +0x10 0x140004020  TINY returns dword [0x1401c4090] = TypeId(CemeteryComponent)
  [ 3] +0x18 0x1400475c0  OWN CemeteryComponent_Controller_Slot13 (172 B)
  [ 4] +0x20 0x1400475b0  TINY RET 0 (c2 00 00)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140047200  OWN CemeteryComponent_Draw (934 B)
  [ 7] +0x38 0x1400471c0  OWN CemeteryComponent_OnCreate (51 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (2): this+0x0 at 0x14004769d in 0x140047680 CemeteryComponent_Ctor; this+0x0 at 0x140047722 in 0x1400476f0 CemeteryComponent_Ctor
### CemeteryDescription  table=0x140159130  slots=10  sizeof=0x28  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6546657461657243 (string "CreateFencedDescription")
  reflected: ti=0x1401c40c8 create=0x1400040b0 createInPlace=0x140004110 CemeteryDescription_CreateInPlace registered in 0x14014a9c0 CemeteryDescription_Register
  [ 0] +0x00 0x1400041c0  OWN CemeteryComponent_vslot18 (83 B)
  [ 1] +0x08 0x1400040a0  TINY returns qword [0x1401c40d0]
  [ 2] +0x10 0x140004090  TINY returns dword [0x1401c40c8] = TypeId(CemeteryDescription)
  [ 3] +0x18 0x140047850  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140047800  OWN FUN_140047800 (67 B)
  [ 6] +0x30 0x140004160  OWN FUN_140004160 (77 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400041b0  OWN (no Ghidra function)
  [ 9] +0x48 0x140004110  OWN CemeteryDescription_CreateInPlace (76 B)
  stamped by (6): other+0x0 at 0x1400040f5 in 0x1400040b0 CemeteryDescription_Factory; this+0x0 at 0x140004144 in 0x140004110 CemeteryDescription_CreateInPlace; this+0x0 at 0x140004199 in 0x140004160 FUN_140004160; this+0x0 at 0x1400041e4 in 0x1400041c0 CemeteryComponent_vslot18; this+0x0 at 0x140047879 in 0x140047860 FUN_140047860; this+0x0 at 0x1400478ce in 0x1400478a0 FUN_1400478a0
### CemeteryUI  table=0x14015b240  slots=11  sizeof=0x38  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x50656d75736e6f43 (string "ConsumeProduceUI")
  reflected: ti=0x1401c4d78 create=0x140010900 createInPlace=0x140010960 Cleanup_CemeteryUI registered in 0x14014bd80 RegisterType_CemeteryUI
  [ 0] +0x00 0x1400109b0  OWN CemeteryUI_ScalarDtor (73 B)
  [ 1] +0x08 0x1400108f0  TINY returns qword [0x1401c4d80]
  [ 2] +0x10 0x1400108e0  TINY returns dword [0x1401c4d78] = TypeId(CemeteryUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x14005f090  OWN CemeteryUI_Serialize (234 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14005ef50  OWN CemeteryUI_OnCreate (306 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14003a972  IMPORT ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z  [in 11 tables]
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (5): other+0x0 at 0x14001093b in 0x140010900 Factory_CemeteryUI; this+0x0 at 0x14001098a in 0x140010960 Cleanup_CemeteryUI; this+0x0 at 0x1400109d4 in 0x1400109b0 CemeteryUI_ScalarDtor; this+0x0 at 0x14005f199 in 0x14005f180 CemeteryUI_Ctor; this+0x0 at 0x14005f1d4 in 0x14005f1b0 CemeteryUI_CopyCtor
### CharacterComponent  table=0x140162370  slots=16  sizeof=0x80  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7078 create=0x14002f960 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e690 CharacterComponent_RegisterType
  [ 0] +0x00 0x14002faa0  OWN CharacterComponent_ScalarDtor (96 B)
  [ 1] +0x08 0x14002f950  TINY returns qword [0x1401c7080]
  [ 2] +0x10 0x14002f940  TINY returns dword [0x1401c7078] = TypeId(CharacterComponent)
  [ 3] +0x18 0x140109ab0  OWN CharacterComponent_Serialize (523 B)
  [ 4] +0x20 0x140109a30  OWN CharacterComponent_Startup (117 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x1401099c0  OWN CharacterComponent_OnAdd (98 B)
  [ 7] +0x38 0x140109980  OWN CharacterComponent_OnRemove (53 B)
  [ 8] +0x40 0x1401094c0  OWN CharacterComponent_OnAttach (1202 B)
  [ 9] +0x48 0x140109480  OWN CharacterComponent_Update (58 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x14002fac4 in 0x14002faa0 CharacterComponent_ScalarDtor; this+0x0 at 0x140109ced in 0x140109cd0 CharacterComponent_Ctor; this+0x0 at 0x140109d48 in 0x140109d20 CharacterComponent_Ctor
### CharacterDescription  table=0x1401623f0  slots=10  sizeof=0x48  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x416e657a69746943 (string "CitizenAIComponent")
  reflected: ti=0x1401c70b0 create=0x14002f9c0 createInPlace=0x14002fa00 CharacterDescription_CreateInPlace registered in 0x14014e6d0 CharacterDescription_Register
  [ 0] +0x00 0x14002fa40  OWN CharacterComponent_GetTypeId (83 B)
  [ 1] +0x08 0x14002f9b0  TINY returns qword [0x1401c70b8]
  [ 2] +0x10 0x14002f9a0  TINY returns dword [0x1401c70b0] = TypeId(CharacterDescription)
  [ 3] +0x18 0x140109ec0  TINY return 6 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140109dd0  OWN CharacterComponent_OnCreate (233 B)
  [ 6] +0x30 0x14002fa10  OWN CharacterComponent_OnDestroy (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14002fa30  OWN CharacterComponent_OnCleanup (12 B)
  [ 9] +0x48 0x14002fa00  OWN CharacterDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x14002fa64 in 0x14002fa40 CharacterComponent_GetTypeId; this+0x0 at 0x140109ee9 in 0x140109ed0 FUN_140109ed0; this+0x0 at 0x140109f3e in 0x140109f10 FUN_140109f10
### CitizenAIComponent  table=0x140162470  slots=16  sizeof=0xd0  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c70e8 create=0x14002fb20 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e710 CitizenAIComponent_RegisterType
  [ 0] +0x00 0x14002fcf0  OWN CitizenAIComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x14002fb10  TINY returns qword [0x1401c70f0]
  [ 2] +0x10 0x14002fb00  TINY returns dword [0x1401c70e8] = TypeId(CitizenAIComponent)
  [ 3] +0x18 0x14010f960  OWN CitizenAIComponent_Serialize (449 B)
  [ 4] +0x20 0x14010f920  OWN CitizenAIComponent_Startup (55 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x14010f880  JMP 0x14010f890 CitizenAIComponent_CheckAge
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x14010f3e0  OWN CitizenAIComponent_Update (950 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x14010f340  OWN CitizenAIComponent_OnReset (150 B)
  stamped by (3): this+0x0 at 0x14002fd14 in 0x14002fcf0 CitizenAIComponent_ScalarDtor; this+0x0 at 0x14010fb49 in 0x14010fb30 CitizenAIComponent_Ctor; this+0x0 at 0x14010fb9d in 0x14010fb70 CitizenAIComponent_Ctor
### CitizenAIDescription  table=0x1401624f0  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x446e657a69746943 (string "CitizenDescription")
  reflected: ti=0x1401c7120 create=0x14002fb80 createInPlace=0x14002fbe0 CitizenAIDescription_CreateInPlace registered in 0x14014e750 CitizenAIDescription_Register
  [ 0] +0x00 0x14002fc90  OWN CitizenAIComponent_ControllerDtor (83 B)
  [ 1] +0x08 0x14002fb70  TINY returns qword [0x1401c7128]
  [ 2] +0x10 0x14002fb60  TINY returns dword [0x1401c7120] = TypeId(CitizenAIDescription)
  [ 3] +0x18 0x14010fc80  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14010fc40  OWN CitizenAIComponent_OnSerializeSettings (46 B)
  [ 6] +0x30 0x14002fc30  OWN FUN_14002fc30 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14002fc80  OWN (no Ghidra function)
  [ 9] +0x48 0x14002fbe0  OWN CitizenAIDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x14002fbc5 in 0x14002fb80 CitizenAIDescription_Factory; this+0x0 at 0x14002fc14 in 0x14002fbe0 CitizenAIDescription_CreateInPlace; this+0x0 at 0x14002fc69 in 0x14002fc30 FUN_14002fc30; this+0x0 at 0x14002fcb4 in 0x14002fc90 CitizenAIComponent_ControllerDtor; this+0x0 at 0x14010fca9 in 0x14010fc90 FUN_14010fc90; this+0x0 at 0x14010fcfe in 0x14010fcd0 FUN_14010fcd0
### CitizenComponent  table=0x140162580  slots=16  sizeof=0x290  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7158 create=0x140030110 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e790 CitizenComponent_RegisterType
  [ 0] +0x00 0x140030270  OWN CitizenComponent_ScalarDtor (48 B)
  [ 1] +0x08 0x140030100  TINY returns qword [0x1401c7160]
  [ 2] +0x10 0x1400300f0  TINY returns dword [0x1401c7158] = TypeId(CitizenComponent)
  [ 3] +0x18 0x140114160  OWN CitizenComponent_Serialize (1411 B)
  [ 4] +0x20 0x140114050  OWN CitizenComponent_Startup (144 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140114000  OWN CitizenComponent_OnAdd (59 B)
  [ 7] +0x38 0x140113ca0  OWN CitizenComponent_OnRemove (843 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140112e70  OWN CitizenComponent_Update (3055 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x1401140f0  OWN CitizenComponent_OnUpdate2 (96 B)
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140112260  OWN CitizenComponent_NeedsUpdate (290 B)
  stamped by (2): this+0x0 at 0x14011470d in 0x1401146f0 CitizenComponent_Ctor; this+0x0 at 0x140114798 in 0x140114770 CitizenComponent_Ctor
### CitizenDescription  table=0x140162600  slots=10  sizeof=0x148  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6572417261656c43 (string "ClearAreaComponent")
  reflected: ti=0x1401c7190 create=0x140030170 createInPlace=0x1400301b0 CitizenDescription_CreateInPlace registered in 0x14014e7d0 CitizenDescription_Register
  [ 0] +0x00 0x1400301f0  OWN CitizenComponent_GetTypeId (115 B)
  [ 1] +0x08 0x140030160  TINY returns qword [0x1401c7198]
  [ 2] +0x10 0x140030150  TINY returns dword [0x1401c7190] = TypeId(CitizenDescription)
  [ 3] +0x18 0x140114e00  TINY return 17 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140114900  OWN CitizenComponent_OnCreate (1272 B)
  [ 6] +0x30 0x1400301c0  OWN CitizenComponent_OnDestroy (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400301e0  OWN CitizenComponent_OnCleanup (12 B)
  [ 9] +0x48 0x1400301b0  OWN CitizenDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x140030214 in 0x1400301f0 CitizenComponent_GetTypeId; this+0x0 at 0x140114e2d in 0x140114e10 FUN_140114e10; this+0x0 at 0x140114e9e in 0x140114e70 FUN_140114e70
### CitizenUI  table=0x14015ba00  slots=11  sizeof=0xe8  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x55796f7274736544 (string "DestroyUI")
  reflected: ti=0x1401c4e90 create=0x140010f80 createInPlace=0x140010fc0 <no fn> registered in 0x14014bec0 RegisterType_CitizenUI
  [ 0] +0x00 0x140010fd0  OWN CitizenUI_ScalarDtor (48 B)
  [ 1] +0x08 0x140010f70  TINY returns qword [0x1401c4e98]
  [ 2] +0x10 0x140010f60  TINY returns dword [0x1401c4e90] = TypeId(CitizenUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x1400651a0  OWN CitizenUI_Serialize (2020 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x140065130  OWN CitizenUI_Draw (96 B)
  [ 7] +0x38 0x140064ae0  OWN CitizenUI_OnCreate (1537 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x1400650f0  OWN EventNotifyDialog_Slot9 (47 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (2): this+0x0 at 0x1400659b7 in 0x140065990 CitizenUI_Ctor; this+0x0 at 0x140065a64 in 0x140065a40 CitizenUI_Ctor
### ClearArea  table=0x140161110  slots=13  sizeof=0x78  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x6572417261656c43 (string "ClearArea")
  reflected: ti=0x1401c68d0 create=0x140029820 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014dcd0 ClearArea_RegisterType
  [ 0] +0x00 0x140029870  OWN BuildStructure_Slot14 (48 B)
  [ 1] +0x08 0x140029810  TINY returns qword [0x1401c68d8]
  [ 2] +0x10 0x140029800  TINY returns dword [0x1401c68d0] = TypeId(ClearArea)
  [ 3] +0x18 0x1400e6740  OWN ClearArea_Shutdown (187 B)
  [ 4] +0x20 0x1400e5ee0  OWN ClearArea_Serialize (1897 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400e5e30  OWN ClearArea_Draw (156 B)
  [ 7] +0x38 0x1400e5e00  OWN ClearArea_OnCreate (41 B)
  [ 8] +0x40 0x1400e6650  OWN ClearArea_OnRemove (188 B)
  [ 9] +0x48 0x1400e5b10  OWN ClearArea_Update (741 B)
  [10] +0x50 0x1400e6720  OWN ClearArea_Slot10 (25 B)
  [11] +0x58 0x140029860  OWN ClearArea_Slot11 (9 B)  [in 13 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (3): this+0x0 at 0x1400e6834 in 0x1400e6810 Work_Ctor10; this+0x0 at 0x1400e6927 in 0x1400e68d0 ClearArea_Ctor; this+0x0 at 0x1400e699e in 0x1400e6960 ClearArea_Ctor
### ClearAreaComponent  table=0x1401626d0  slots=18  sizeof=0x50  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c71c8 create=0x140030ea0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e810 ClearAreaComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x140162680
  [ 0] +0x00 0x1400310c0  OWN ClearAreaComponent_Controller_Slot10 (115 B)
  [ 1] +0x08 0x140030e90  TINY returns qword [0x1401c71d0]
  [ 2] +0x10 0x140030e80  TINY returns dword [0x1401c71c8] = TypeId(ClearAreaComponent)
  [ 3] +0x18 0x140117ec0  OWN ClearAreaComponent_Controller_Slot13 (118 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x140003730  TINY return 0 (xor eax,eax)  [in 5 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (3): this+0x0 at 0x1400310e4 in 0x1400310c0 ClearAreaComponent_Controller_Slot10; this+0x0 at 0x140117f59 in 0x140117f40 ClearAreaComponent_Ctor; this+0x0 at 0x140117fcd in 0x140117fa0 ClearAreaComponent_Ctor
### ClearAreaDescription  table=0x140162760  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x7365527261656c43 (string "ClearResourceComponent")
  reflected: ti=0x1401c7200 create=0x140030f00 createInPlace=0x140030f80 ClearAreaDescription_CreateInPlace registered in 0x14014e850 ClearAreaDescription_Register
  [ 0] +0x00 0x140031060  OWN ClearAreaComponent_vslot18 (83 B)
  [ 1] +0x08 0x140030ef0  TINY returns qword [0x1401c7208]
  [ 2] +0x10 0x140030ee0  TINY returns dword [0x1401c7200] = TypeId(ClearAreaDescription)
  [ 3] +0x18 0x1401181c0  TINY return 6 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1401180f0  OWN FUN_1401180f0 (193 B)
  [ 6] +0x30 0x140030fe0  OWN FUN_140030fe0 (97 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140031050  OWN (no Ghidra function)
  [ 9] +0x48 0x140030f80  OWN ClearAreaDescription_CreateInPlace (96 B)
  stamped by (6): other+0x0 at 0x140030f45 in 0x140030f00 ClearAreaDescription_Factory; this+0x0 at 0x140030fb4 in 0x140030f80 ClearAreaDescription_CreateInPlace; this+0x0 at 0x140031019 in 0x140030fe0 FUN_140030fe0; this+0x0 at 0x140031084 in 0x140031060 ClearAreaComponent_vslot18; this+0x0 at 0x1401181e9 in 0x1401181d0 FUN_1401181d0; this+0x0 at 0x14011823e in 0x140118210 FUN_140118210
### ClearResourceComponent  table=0x140162840  slots=18  sizeof=0x48  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c7238 create=0x1400315a0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e910 ClearResourceComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x1401627f0
  [ 0] +0x00 0x140031770  OWN ClearResourceComponent_Controller_Slot10 (115 B)
  [ 1] +0x08 0x140031590  TINY returns qword [0x1401c7240]
  [ 2] +0x10 0x140031580  TINY returns dword [0x1401c7238] = TypeId(ClearResourceComponent)
  [ 3] +0x18 0x1401185e0  OWN ClearResourceComponent_Controller_Slot13 (76 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (3): this+0x0 at 0x140031794 in 0x140031770 ClearResourceComponent_Controller_Slot10; this+0x0 at 0x140118659 in 0x140118640 ClearResourceComponent_Ctor; this+0x0 at 0x1401186cd in 0x1401186a0 ClearResourceComponent_Ctor
### ClearResourceDescription  table=0x1401628d0  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=next table
  reflected: ti=0x1401c7270 create=0x140031600 createInPlace=0x140031660 ClearResourceDescription_CreateInPlace registered in 0x14014e950 ClearResourceDescription_Register
  [ 0] +0x00 0x140031710  OWN ClearResourceComponent_vslot18 (83 B)
  [ 1] +0x08 0x1400315f0  TINY returns qword [0x1401c7278]
  [ 2] +0x10 0x1400315e0  TINY returns dword [0x1401c7270] = TypeId(ClearResourceDescription)
  [ 3] +0x18 0x140118770  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140118730  OWN FUN_140118730 (46 B)
  [ 6] +0x30 0x1400316b0  OWN FUN_1400316b0 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140031700  OWN (no Ghidra function)
  [ 9] +0x48 0x140031660  OWN ClearResourceDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x140031645 in 0x140031600 ClearResourceDescription_Factory; this+0x0 at 0x140031694 in 0x140031660 ClearResourceDescription_CreateInPlace; this+0x0 at 0x1400316e9 in 0x1400316b0 FUN_1400316b0; this+0x0 at 0x140031734 in 0x140031710 ClearResourceComponent_vslot18; this+0x0 at 0x140118799 in 0x140118780 FUN_140118780; this+0x0 at 0x1401187ee in 0x1401187c0 FUN_1401187c0
### ClearStorage  table=0x1401611a0  slots=13  sizeof=0x68  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x50656d75736e6f43 (string "ConsumeProduce")
  reflected: ti=0x1401c6908 create=0x14002a0b0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014dd10 ClearStorage_RegisterType
  [ 0] +0x00 0x14002a100  OWN ClearStorage_ScalarDtor (48 B)
  [ 1] +0x08 0x14002a0a0  TINY returns qword [0x1401c6910]
  [ 2] +0x10 0x14002a090  TINY returns dword [0x1401c6908] = TypeId(ClearStorage)
  [ 3] +0x18 0x1400e7350  OWN ClearStorage_Shutdown (86 B)
  [ 4] +0x20 0x1400e6c50  OWN ClearStorage_Serialize (1605 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400e6c30  OWN ClearStorage_Draw (20 B)
  [ 7] +0x38 0x1400e6c20  THUNK this+88 -> 0x140025520 CitizenAI_impl_5520
  [ 8] +0x40 0x1400e72a0  OWN ClearStorage_OnRemove (132 B)
  [ 9] +0x48 0x1400e69f0  OWN ClearStorage_Update (547 B)
  [10] +0x50 0x1400e7330  OWN ClearStorage_Slot10 (25 B)
  [11] +0x58 0x14002a0f0  OWN ClearStorage_Slot11 (9 B)  [in 2 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (3): this+0x0 at 0x1400e73d7 in 0x1400e73b0 Work_Ctor11; this+0x0 at 0x1400e7480 in 0x1400e7450 ClearStorage_Ctor; this+0x0 at 0x1400e7507 in 0x1400e74b0 ClearStorage_Ctor
### ClimateGenerator  table=0x14015e140  slots=9  sizeof=0x110  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x65736165736944 (string "Disease")
  reflected: ti=0x1401c5910 create=0x140018ef0 createInPlace=0x140018f30 ClimateGenerator_Cleanup registered in 0x14014cac0 ClimateGenerator_RegisterType
  [ 0] +0x00 0x140018f60  OWN ClimateGenerator_ScalarDtor (77 B)  [in 2 tables]
  [ 1] +0x08 0x140018ee0  TINY returns qword [0x1401c5918]
  [ 2] +0x10 0x140018ed0  TINY returns dword [0x1401c5910] = TypeId(ClimateGenerator)
  [ 3] +0x18 0x1400930b0  TINY return 3 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x140092fa0  OWN ClimateGenerator_Slot5 (257 B)
  [ 6] +0x30 0x140018f40  OWN ClimateGenerator_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140018f30  OWN ClimateGenerator_Cleanup (12 B)
  stamped by (1): this+0x0 at 0x1400930e4 in 0x1400930c0 ClimateGenerator_Ctor
### CompleteRoad  table=0x14015fb70  slots=10  sizeof=0x30  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x6574656c706d6f43 (string "CompleteTask")
  reflected: ti=0x1401c5e18 create=0x140021ec0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d090 CompleteRoad_RegisterType
  [ 0] +0x00 0x140021f30  OWN CompleteRoad_ScalarDtor (104 B)
  [ 1] +0x08 0x140021eb0  TINY returns qword [0x1401c5e20]
  [ 2] +0x10 0x140021ea0  TINY returns dword [0x1401c5e18] = TypeId(CompleteRoad)
  [ 3] +0x18 0x1400c5880  OWN CompleteRoad_Shutdown (166 B)
  [ 4] +0x20 0x1400c5850  OWN CompleteRoad_Serialize (42 B)
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]   = parent
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400c4a30  TINY RET 0 (c2 00 00)  [in 7 tables]   = parent
  [ 9] +0x48 0x1400c4a40  TINY return false (xor al,al)  [in 2 tables]   = parent
  stamped by (5): other+0x0 at 0x140021f0e in 0x140021ec0 CompleteRoad_Factory; this+0x0 at 0x140021f54 in 0x140021f30 CompleteRoad_ScalarDtor; this+0x0 at 0x1400c594d in 0x1400c5930 Action_Action_Ctor11; this+0x0 at 0x1400c59b7 in 0x1400c5980 CompleteRoad_Action_Ctor; this+0x0 at 0x1400c5a22 in 0x1400c59d0 CompleteRoad_Action_CopyCtor
### CompleteTask  table=0x14015fbd0  slots=10  sizeof=0x38  parent="Action" / structural Action 0x14015faa0  stop=next table
  reflected: ti=0x1401c5e50 create=0x140021fc0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d0d0 CompleteTask_RegisterType
  [ 0] +0x00 0x140022030  OWN CompleteTask_ScalarDtor (104 B)
  [ 1] +0x08 0x140021fb0  TINY returns qword [0x1401c5e58]
  [ 2] +0x10 0x140021fa0  TINY returns dword [0x1401c5e50] = TypeId(CompleteTask)
  [ 3] +0x18 0x1400c5be0  OWN CompleteTask_Shutdown (263 B)
  [ 4] +0x20 0x1400c5b40  OWN CompleteTask_Serialize (145 B)
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]   = parent
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400c5ac0  OWN CompleteTask_OnRemove (112 B)
  [ 9] +0x48 0x1400c5a50  OWN CompleteTask_Update (92 B)
  stamped by (5): other+0x0 at 0x14002200e in 0x140021fc0 CompleteTask_Factory; this+0x0 at 0x140022054 in 0x140022030 CompleteTask_ScalarDtor; this+0x0 at 0x1400c5d0d in 0x1400c5cf0 Action_Action_Ctor13; this+0x0 at 0x1400c5d77 in 0x1400c5d40 CompleteTask_Action_Ctor; this+0x0 at 0x1400c5df0 in 0x1400c5da0 CompleteTask_Action_CopyCtor
### Component  table=0x140162920  slots=16  sizeof=abstract  parent="Object::Internal" / structural Object::Internal (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c72a8 create=0x140003730 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014ea10 Component_Register
  [ 0] +0x00 0x140031cc0  OWN Component_ScalarDtor (73 B)
  [ 1] +0x08 0x140031c40  TINY returns qword [0x1401c72b0]
  [ 2] +0x10 0x140031c30  TINY returns dword [0x1401c72a8] = TypeId(Component)
  [ 3] +0x18 0x1401189b0  OWN Component_Serialize (153 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]
  stamped by (123): this+0x0 at 0x140001712 in 0x1400016d0 AmbientEmitterComponent_ScalarDtor; this+0x0 at 0x14000219e in 0x140002170 AmbientEnvironmentComponent_ScalarDtor; this+0x0 at 0x1400035d2 in 0x140003590 AutoPickupComponent_ScalarDtor; this+0x0 at 0x140004fae in 0x140004f80 DiseaseComponent_ScalarDtor; this+0x0 at 0x14000574e in 0x140005720 FireComponent_ScalarDtor; this+0x0 at 0x140005b4e in 0x140005b20 FruitComponent_ScalarDtor; this+0x0 at 0x140005dfb in 0x140005db0 HappinessComponent_Controller_Slot10; this+0x0 at 0x1400066ab in 0x140006660 HealerComponent_Controller_Slot10; this+0x0 at 0x140006db5 in 0x140006d60 HerbalistComponent_ScalarDtor; this+0x0 at 0x140008495 in 0x140008440 MarketComponent_ScalarDtor; this+0x0 at 0x14000894b in 0x140008900 MarketLocationComponent_Controller_Slot10; this+0x0 at 0x140008fce in 0x140008fa0 MineComponent_ScalarDtor ...
### ComponentDescription  table=0x140162a00  slots=9  sizeof=0x38  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x50656d75736e6f43 (string "ConsumeProduceComponent")
  reflected: ti=0x1401c7318 create=0x140031f30 createInPlace=0x140031f90 ComponentDescription_CreateInPlace registered in 0x14014ea90 ComponentDescription_Register
  [ 0] +0x00 0x140032030  OWN FUN_140032030 (96 B)
  [ 1] +0x08 0x140031f20  TINY returns qword [0x1401c7320]
  [ 2] +0x10 0x140031f10  TINY returns dword [0x1401c7318] = TypeId(ComponentDescription)
  [ 3] +0x18 0x140118b90  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x140118af0  OWN FUN_140118af0 (142 B)
  [ 6] +0x30 0x140031fe0  OWN FUN_140031fe0 (76 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140031f90  OWN ComponentDescription_CreateInPlace (75 B)
  stamped by (6): other+0x0 at 0x140031f6b in 0x140031f30 ComponentDescription_Factory; this+0x0 at 0x140031fba in 0x140031f90 ComponentDescription_CreateInPlace; this+0x0 at 0x14003200f in 0x140031fe0 FUN_140031fe0; this+0x0 at 0x140032054 in 0x140032030 FUN_140032030; this+0x0 at 0x140118bbd in 0x140118ba0 FUN_140118ba0; this+0x0 at 0x140118c14 in 0x140118bf0 FUN_140118bf0
### ComponentEntity  table=0x14015fa78  slots=4  sizeof=0x48  parent="Object::Internal" / structural Object::Internal (Runtime ctor import)  stop=zero qword
  reflected: ti=0x1401c5d70 create=0x1400219c0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014cfd0 ComponentEntity_RegisterType
  [ 0] +0x00 0x140021a00  OWN ComponentEntity_ScalarDtor (48 B)
  [ 1] +0x08 0x1400219b0  TINY returns qword [0x1401c5d78]
  [ 2] +0x10 0x1400219a0  TINY returns dword [0x1401c5d70] = TypeId(ComponentEntity)
  [ 3] +0x18 0x1400c3140  OWN ComponentEntity_Shutdown (646 B)
  stamped by (3): this+0x0 at 0x1400c3574 in 0x1400c3550 ComponentEntity_Ctor; this+0x0 at 0x1400c3644 in 0x1400c3620 ComponentEntity_Ctor; this+0x0 at 0x1400c3752 in 0x1400c3720 ComponentEntity_CopyCtor
### ConsumeProduce  table=0x140161220  slots=13  sizeof=0x78  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x6b726f5765726946 (string "FireWork")
  reflected: ti=0x1401c6940 create=0x14002a150 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014dd50 ConsumeProduce_RegisterType
  [ 0] +0x00 0x14002a190  OWN ConsumeProduceAction_ScalarDtor (48 B)
  [ 1] +0x08 0x14002a140  TINY returns qword [0x1401c6948]
  [ 2] +0x10 0x14002a130  TINY returns dword [0x1401c6940] = TypeId(ConsumeProduce)
  [ 3] +0x18 0x1400e9020  OWN ConsumeProduceAction_Shutdown (352 B)
  [ 4] +0x20 0x1400e7840  OWN ConsumeProduceAction_Serialize (5851 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400e77c0  OWN ConsumeProduceAction_Draw (120 B)
  [ 7] +0x38 0x1400e7780  OWN ConsumeProduceAction_OnCreate (46 B)
  [ 8] +0x40 0x1400e8f30  OWN ConsumeProduceAction_OnRemove (118 B)
  [ 9] +0x48 0x1400e7530  OWN ConsumeProduceAction_Update (578 B)
  [10] +0x50 0x1400e8fb0  OWN ConsumeProduceAction_Slot10 (75 B)
  [11] +0x58 0x14002a0f0  OWN ClearStorage_Slot11 (9 B)  [in 2 tables]
  [12] +0x60 0x1400e9010  TINY RET 0 (c2 00 00)
  stamped by (3): this+0x0 at 0x1400e91b2 in 0x1400e9190 Work_Ctor12; this+0x0 at 0x1400e924a in 0x1400e9200 ConsumeProduceAction_Ctor; this+0x0 at 0x1400e9397 in 0x1400e9340 ConsumeProduce_Ctor
### ConsumeProduceComponent  table=0x140162a80  slots=22  sizeof=0x70  parent="ResourceProduceComponent" / structural ResourceProduceComponent 0x1401643f0  stop=next table
  reflected: ti=0x1401c7350 create=0x140032300 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014ead0 ConsumeProduceComponent_RegisterType
  [ 0] +0x00 0x140032460  OWN ConsumeProduceComponent_ScalarDtor (48 B)
  [ 1] +0x08 0x1400322f0  TINY returns qword [0x1401c7358]
  [ 2] +0x10 0x1400322e0  TINY returns dword [0x1401c7350] = TypeId(ConsumeProduceComponent)
  [ 3] +0x18 0x140118e00  OWN ConsumeProduceComponent_Shutdown (230 B)
  [ 4] +0x20 0x140118ef0  OWN ConsumeProduceComponent_Serialize (215 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x14012b520  OWN HerbalistComponent_Slot13 (112 B)  [in 6 tables]   = parent
  [14] +0x70 0x14012b5a0  OWN HerbalistComponent_Slot14 (117 B)  [in 6 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]   = parent
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]   = parent
  [18] +0x90 0x140118d00  OWN ConsumeProduceComponent_vslot18 (239 B)
  [19] +0x98 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  [20] +0xa0 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  [21] +0xa8 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (2): this+0x0 at 0x140118fed in 0x140118fd0 ResourceProduceComponent_Ctor3; this+0x0 at 0x140119078 in 0x140119050 ConsumeProduceComponent_Ctor
### ConsumeProduceDescription  table=0x140162b30  slots=10  sizeof=0x58  parent="Description" / structural Description 0x1401629a0  stop=next table
  reflected: ti=0x1401c7388 create=0x140032360 createInPlace=0x1400323a0 ConsumeProduceDescription_CreateInPlace registered in 0x14014eb10 ConsumeProduceDescription_Register
  [ 0] +0x00 0x1400323e0  OWN FUN_1400323e0 (119 B)
  [ 1] +0x08 0x140032350  TINY returns qword [0x1401c7390]
  [ 2] +0x10 0x140032340  TINY returns dword [0x1401c7388] = TypeId(ConsumeProduceDescription)
  [ 3] +0x18 0x140119200  TINY return 12 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1401190e0  OWN FUN_1401190e0 (271 B)
  [ 6] +0x30 0x1400323b0  OWN FUN_1400323b0 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400323d0  OWN (no Ghidra function)
  [ 9] +0x48 0x1400323a0  OWN ConsumeProduceDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x140032404 in 0x1400323e0 FUN_1400323e0; this+0x0 at 0x14011922d in 0x140119210 FUN_140119210; this+0x0 at 0x14011929e in 0x140119270 ConsumeProduceDescription_sub_9270
### ConsumeProduceUI  table=0x14015b2e0  slots=11  sizeof=0x40  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x556c6f72746e6f43 (string "ControlUI")
  reflected: ti=0x1401c4db0 create=0x140010ae0 createInPlace=0x140010b40 Cleanup_ConsumeProduceUI registered in 0x14014bdc0 RegisterType_ConsumeProduceUI
  [ 0] +0x00 0x140010b90  OWN ConsumeProduceUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140010ad0  TINY returns qword [0x1401c4db8]
  [ 2] +0x10 0x140010ac0  TINY returns dword [0x1401c4db0] = TypeId(ConsumeProduceUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x14005f3d0  OWN ConsumeProduceUI_Serialize (923 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14005f1f0  OWN ConsumeProduceUI_OnCreate (301 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14005f330  OWN ConsumeProduceUI_Update (147 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (5): other+0x0 at 0x140010b1b in 0x140010ae0 Factory_ConsumeProduceUI; this+0x0 at 0x140010b6a in 0x140010b40 Cleanup_ConsumeProduceUI; this+0x0 at 0x140010bb4 in 0x140010b90 ConsumeProduceUI_ScalarDtor; this+0x0 at 0x14005f799 in 0x14005f780 ConsumeProduceUI_Ctor; this+0x0 at 0x14005f7d4 in 0x14005f7b0 ConsumeProduceUI_CopyCtor
### ConsumeRawMaterial  table=0x14015fc20  slots=10  sizeof=0x40  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x52656d75736e6f43 (string "ConsumeRawMaterial")
  reflected: ti=0x1401c5e88 create=0x1400220c0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d110 ConsumeRawMaterial_RegisterType
  [ 0] +0x00 0x140022130  OWN CompleteTask_Slot10 (124 B)
  [ 1] +0x08 0x1400220b0  TINY returns qword [0x1401c5e90]
  [ 2] +0x10 0x1400220a0  TINY returns dword [0x1401c5e88] = TypeId(ConsumeRawMaterial)
  [ 3] +0x18 0x1400c6120  OWN CompleteTask_Slot13 (374 B)
  [ 4] +0x20 0x1400c5f50  OWN CompleteTask_Slot14 (456 B)
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]   = parent
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400c5e30  OWN ConsumeRawMaterial_OnRemove (213 B)
  [ 9] +0x48 0x1400c5f10  OWN ConsumeRawMaterial_Update (46 B)
  stamped by (6): other+0x0 at 0x14002210e in 0x1400220c0 ConsumeRawMaterial_Factory; this+0x0 at 0x140022154 in 0x140022130 CompleteTask_Slot10; this+0x0 at 0x1400c62fd in 0x1400c62e0 Action_Action_Ctor9; this+0x0 at 0x1400c6387 in 0x1400c6350 ConsumeRawMaterial_Action_Ctor; this+0x0 at 0x1400c63f4 in 0x1400c63b0 ConsumeRawMaterial_Action_CopyCtor; this+0x0 at 0x1400c647f in 0x1400c6440 ConsumeRawMaterial_Action_Ctor3
### ControllerComponent  table=0x140162b80  slots=18  sizeof=abstract  parent="Component" / structural Component 0x140162920  stop=non-code qword 0x7242657461657243 (string "CreateBridgeDescription")
  reflected: ti=0x1401c73c0 create=0x140003730 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014eb50 ControllerComponent_RegisterType
  [ 0] +0x00 0x140032800  OWN FUN_140032800 (83 B)
  [ 1] +0x08 0x1400327f0  TINY returns qword [0x1401c73c8]
  [ 2] +0x10 0x1400327e0  TINY returns dword [0x1401c73c0] = TypeId(ControllerComponent)
  [ 3] +0x18 0x1401195a0  OWN Component_impl_95a0 (48 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x14003addc  _purecall  [in 12 tables]
  [17] +0x88 0x14003addc  _purecall  [in 12 tables]
  stamped by (72): this+0x0 at 0x140005df1 in 0x140005db0 HappinessComponent_Controller_Slot10; this+0x0 at 0x1400066a1 in 0x140006660 HealerComponent_Controller_Slot10; this+0x0 at 0x140006dab in 0x140006d60 HerbalistComponent_ScalarDtor; this+0x0 at 0x14000848b in 0x140008440 MarketComponent_ScalarDtor; this+0x0 at 0x140008941 in 0x140008900 MarketLocationComponent_Controller_Slot10; this+0x0 at 0x140009271 in 0x140009230 NomadComponent_Controller_Slot10; this+0x0 at 0x14000fe91 in 0x14000fe50 WellComponent_Controller_Slot10; this+0x0 at 0x14002ec31 in 0x14002ebf0 AbsorbResourceComponent_Controller_Slot10; this+0x0 at 0x140031101 in 0x1400310c0 ClearAreaComponent_Controller_Slot10; this+0x0 at 0x1400317b1 in 0x140031770 ClearResourceComponent_Controller_Slot10; this+0x0 at 0x140032824 in 0x140032800 FUN_140032800; this+0x0 at 0x140033e11 in 0x140033dd0 DestroyComponent_Controller_Slot10 ...
### ControlUI  table=0x14015b380  slots=11  sizeof=0x40  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=zero qword
  reflected: ti=0x1401c4de8 create=0x140010c00 createInPlace=0x140010c60 Cleanup_ControlUI registered in 0x14014be00 RegisterType_ControlUI
  [ 0] +0x00 0x140010e20  OWN ControlUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140010bf0  TINY returns qword [0x1401c4df0]
  [ 2] +0x10 0x140010be0  TINY returns dword [0x1401c4de8] = TypeId(ControlUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x14005f9b0  OWN ControlUI_Serialize (328 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14005f870  OWN ControlUI_OnCreate (299 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14005f7f0  OWN ControlUI_Update (122 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (5): other+0x0 at 0x140010c3b in 0x140010c00 Factory_ControlUI; this+0x0 at 0x140010c8a in 0x140010c60 Cleanup_ControlUI; this+0x0 at 0x140010e44 in 0x140010e20 ControlUI_ScalarDtor; this+0x0 at 0x14005fb19 in 0x14005fb00 ControlUI_Ctor; this+0x0 at 0x14005fb54 in 0x14005fb30 ControlUI_CopyCtor
### ControlUIConfig  table=0x14015b3e0  slots=9  sizeof=0x28  parent="UI::DialogControllerConfig" / structural UI::DialogControllerConfig (Runtime ctor import)  stop=non-code qword 0x6f43676f6c616944 (string "DialogController")
  reflected: ti=0x1401c4e20 create=0x140010cd0 createInPlace=0x140010d30 ControlUIConfig_CreateInPlace registered in 0x14014be40 ControlUIConfig_Register
  [ 0] +0x00 0x140010dd0  OWN ControlUI_Slot12 (73 B)
  [ 1] +0x08 0x140010cc0  TINY returns qword [0x1401c4e28]
  [ 2] +0x10 0x140010cb0  TINY returns dword [0x1401c4e20] = TypeId(ControlUIConfig)
  [ 3] +0x18 0x14005fbd0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x14005fb70  OWN ControlUI_vslot17 (86 B)
  [ 6] +0x30 0x140010d80  OWN ControlUI_vslot18 (65 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140010d30  OWN ControlUIConfig_CreateInPlace (66 B)
  stamped by (6): other+0x0 at 0x140010d0b in 0x140010cd0 ControlUIConfig_Factory; this+0x0 at 0x140010d5a in 0x140010d30 ControlUIConfig_CreateInPlace; this+0x0 at 0x140010daf in 0x140010d80 ControlUI_vslot18; this+0x0 at 0x140010df4 in 0x140010dd0 ControlUI_Slot12; this+0x0 at 0x14005fbf9 in 0x14005fbe0 FUN_14005fbe0; this+0x0 at 0x14005fc34 in 0x14005fc10 FUN_14005fc10
### CreateBridgeDescription  table=0x140162c30  slots=10  sizeof=0x60  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x7244657461657243 (string "CreateDragDescription")
  reflected: ti=0x1401c73f8 create=0x140032880 createInPlace=0x1400328c0 CreateBridgeDescription_CreateInPlace registered in 0x14014eb90 CreateBridgeDescription_RegisterType
  [ 0] +0x00 0x1400328f0  OWN CreateBridgeDescription_ScalarDtor (83 B)
  [ 1] +0x08 0x140032870  TINY returns qword [0x1401c7400]
  [ 2] +0x10 0x140032860  TINY returns dword [0x1401c73f8] = TypeId(CreateBridgeDescription)
  [ 3] +0x18 0x1401196a0  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1401195d0  OWN CreateBridgeDescription_Slot5 (195 B)
  [ 6] +0x30 0x1400328d0  OWN CreateBridgeDescription_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]
  [ 9] +0x48 0x1400328c0  OWN CreateBridgeDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x140032914 in 0x1400328f0 CreateBridgeDescription_ScalarDtor; this+0x0 at 0x1401196c9 in 0x1401196b0 CreateBridgeDescription_Ctor; this+0x0 at 0x14011971e in 0x1401196f0 CreateBridgeDescription_Ctor
### CreateDragDescription  table=0x140162ca0  slots=10  sizeof=0x40  parent="Description" / structural Description 0x1401629a0  stop=next table
  reflected: ti=0x1401c7430 create=0x140032970 createInPlace=0x1400329b0 CreateDragDescription_CreateInPlace registered in 0x14014ebd0 CreateDragDescription_RegisterType
  [ 0] +0x00 0x1400329e0  OWN CreateDragDescription_ScalarDtor (83 B)
  [ 1] +0x08 0x140032960  TINY returns qword [0x1401c7438]
  [ 2] +0x10 0x140032950  TINY returns dword [0x1401c7430] = TypeId(CreateDragDescription)
  [ 3] +0x18 0x140119810  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140119750  OWN CreateDragDescription_Slot5 (178 B)
  [ 6] +0x30 0x1400329c0  OWN CreateDragDescription_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]
  [ 9] +0x48 0x1400329b0  OWN CreateDragDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x140032a04 in 0x1400329e0 CreateDragDescription_ScalarDtor; this+0x0 at 0x140119839 in 0x140119820 CreateDragDescription_Ctor; this+0x0 at 0x14011988e in 0x140119860 CreateDragDescription_Ctor
### CreateFencedDescription  table=0x1401591a0  slots=10  sizeof=0x78  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6150657461657243 (string "CreatePathDescription")
  reflected: ti=0x1401c4100 create=0x1400048e0 createInPlace=0x140004920 CreateFencedDescription_CreateInPlace registered in 0x14014aac0 CreateFencedDescription_RegisterType
  [ 0] +0x00 0x140004960  OWN CreateFencedDescription_ScalarDtor (83 B)
  [ 1] +0x08 0x1400048d0  TINY returns qword [0x1401c4108]
  [ 2] +0x10 0x1400048c0  TINY returns dword [0x1401c4100] = TypeId(CreateFencedDescription)
  [ 3] +0x18 0x1400479c0  TINY return 3 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400478f0  OWN CreateFencedDescription_Slot5 (197 B)
  [ 6] +0x30 0x140004930  OWN CreateFencedDescription_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]
  [ 9] +0x48 0x140004920  OWN CreateFencedDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x140004984 in 0x140004960 CreateFencedDescription_ScalarDtor; this+0x0 at 0x1400479e9 in 0x1400479d0 CreateFencedDescription_Ctor; this+0x0 at 0x140047a3e in 0x140047a10 CreateFencedDescription_Ctor
### CreateNaturalResource  table=0x14015fca0  slots=10  sizeof=0x38  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x6152657461657243 (string "CreateRawMaterial")
  reflected: ti=0x1401c5ec0 create=0x1400221d0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d150 CreateNaturalResource_RegisterType
  [ 0] +0x00 0x140022240  OWN CreateNaturalResource_ScalarDtor (104 B)
  [ 1] +0x08 0x1400221c0  TINY returns qword [0x1401c5ec8]
  [ 2] +0x10 0x1400221b0  TINY returns dword [0x1401c5ec0] = TypeId(CreateNaturalResource)
  [ 3] +0x18 0x1400c6890  OWN CreateNaturalResource_Shutdown (210 B)
  [ 4] +0x20 0x1400c6650  OWN CreateNaturalResource_Serialize (557 B)
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]   = parent
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400c4a30  TINY RET 0 (c2 00 00)  [in 7 tables]   = parent
  [ 9] +0x48 0x1400c6610  OWN CreateNaturalResource_Update (46 B)
  stamped by (6): other+0x0 at 0x14002221e in 0x1400221d0 CreateNaturalResource_Factory; this+0x0 at 0x140022264 in 0x140022240 CreateNaturalResource_ScalarDtor; other+0x0 at 0x1400be062 in 0x1400bdd20 ShowTool_Action_Ctor3; this+0x0 at 0x1400c698d in 0x1400c6970 Action_Action_Ctor7; this+0x0 at 0x1400c69f7 in 0x1400c69c0 CreateNaturalResource_Action_Ctor; this+0x0 at 0x1400c6a6c in 0x1400c6a20 CreateNaturalResource_Action_CopyCtor
### CreatePathDescription  table=0x140159210  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x4365736165736944 (string "DiseaseComponent")
  reflected: ti=0x1401c4138 create=0x140004c30 createInPlace=0x140004c90 CreatePathDescription_CreateInPlace registered in 0x14014ab00 CreatePathDescription_RegisterType
  [ 0] +0x00 0x140004d30  OWN CreatePathDescription_ScalarDtor (83 B)
  [ 1] +0x08 0x140004c20  TINY returns qword [0x1401c4140]
  [ 2] +0x10 0x140004c10  TINY returns dword [0x1401c4138] = TypeId(CreatePathDescription)
  [ 3] +0x18 0x140047ab0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140047a70  OWN CreatePathDescription_Slot5 (46 B)
  [ 6] +0x30 0x140004ce0  OWN CreatePathDescription_Draw (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]
  [ 9] +0x48 0x140004c90  OWN CreatePathDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x140004c75 in 0x140004c30 CreatePathDescription_Factory; this+0x0 at 0x140004cc4 in 0x140004c90 CreatePathDescription_CreateInPlace; this+0x0 at 0x140004d19 in 0x140004ce0 CreatePathDescription_Draw; this+0x0 at 0x140004d54 in 0x140004d30 CreatePathDescription_ScalarDtor; this+0x0 at 0x140047ad9 in 0x140047ac0 CreatePathDescription_Ctor; this+0x0 at 0x140047b2e in 0x140047b00 CreatePathDescription_CopyCtor
### CreatePlacedDescription  table=0x140162cf0  slots=10  sizeof=0x58  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6c50657461657243 (string "CreatePlacedDescription")
  reflected: ti=0x1401c7468 create=0x140032a60 createInPlace=0x140032aa0 CreatePlacedDescription_CreateInPlace registered in 0x14014ec10 CreatePlacedDescription_RegisterType
  [ 0] +0x00 0x140032ad0  OWN CreateDragDescription_Slot10 (117 B)
  [ 1] +0x08 0x140032a50  TINY returns qword [0x1401c7470]
  [ 2] +0x10 0x140032a40  TINY returns dword [0x1401c7468] = TypeId(CreatePlacedDescription)
  [ 3] +0x18 0x1401199f0  TINY return 4 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1401198d0  OWN CreateDragDescription_Slot15 (270 B)
  [ 6] +0x30 0x140032ab0  OWN CreatePlacedDescription_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]
  [ 9] +0x48 0x140032aa0  OWN CreatePlacedDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x140032af4 in 0x140032ad0 CreateDragDescription_Slot10; this+0x0 at 0x140119a1d in 0x140119a00 CreatePlacedDescription_Ctor; this+0x0 at 0x140119a8e in 0x140119a60 CreatePlacedDescription_Ctor
### CreateRawMaterial  table=0x14015fd10  slots=10  sizeof=0x58  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x5266664f706f7244 (string "DropOffRawMaterial")
  reflected: ti=0x1401c5ef8 create=0x1400222d0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d190 CreateRawMaterial_RegisterType
  [ 0] +0x00 0x140022350  OWN CreateRawMaterial_ScalarDtor (104 B)
  [ 1] +0x08 0x1400222c0  TINY returns qword [0x1401c5f00]
  [ 2] +0x10 0x1400222b0  TINY returns dword [0x1401c5ef8] = TypeId(CreateRawMaterial)
  [ 3] +0x18 0x1400c6f30  OWN CreateRawMaterial_Shutdown (437 B)
  [ 4] +0x20 0x1400c6b40  OWN CreateRawMaterial_Serialize (994 B)
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]   = parent
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400c4a30  TINY RET 0 (c2 00 00)  [in 7 tables]   = parent
  [ 9] +0x48 0x1400c6aa0  OWN CreateRawMaterial_Update (146 B)
  stamped by (5): other+0x0 at 0x14002231e in 0x1400222d0 CreateRawMaterial_Factory; this+0x0 at 0x140022374 in 0x140022350 CreateRawMaterial_ScalarDtor; this+0x0 at 0x1400c710d in 0x1400c70f0 Action_Action_Ctor4; this+0x0 at 0x1400c7177 in 0x1400c7140 CreateRawMaterial_Action_Ctor; this+0x0 at 0x1400c71ec in 0x1400c71a0 CreateRawMaterial_Action_CopyCtor
### CreateRemovalDescription  table=0x140162d80  slots=10  sizeof=0x48  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6f52657461657243 (string "CreateRoadDescription")
  reflected: ti=0x1401c74a0 create=0x140032b70 createInPlace=0x140032be0 CreateRemovalDescription_CreateInPlace registered in 0x14014ec50 CreateRemovalDescription_RegisterType
  [ 0] +0x00 0x140032ca0  OWN CreateRemovalDescription_ScalarDtor (106 B)
  [ 1] +0x08 0x140032b60  TINY returns qword [0x1401c74a8]
  [ 2] +0x10 0x140032b50  TINY returns dword [0x1401c74a0] = TypeId(CreateRemovalDescription)
  [ 3] +0x18 0x140119bc0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140119ac0  OWN CreateRemovalDescription_Slot5 (235 B)
  [ 6] +0x30 0x140032c40  OWN CreateRemovalDescription_Draw (90 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]
  [ 9] +0x48 0x140032be0  OWN CreateRemovalDescription_CreateInPlace (89 B)
  stamped by (6): other+0x0 at 0x140032bb5 in 0x140032b70 CreateRemovalDescription_Factory; this+0x0 at 0x140032c14 in 0x140032be0 CreateRemovalDescription_CreateInPlace; this+0x0 at 0x140032c79 in 0x140032c40 CreateRemovalDescription_Draw; this+0x0 at 0x140032cc4 in 0x140032ca0 CreateRemovalDescription_ScalarDtor; this+0x0 at 0x140119bed in 0x140119bd0 CreateRemovalDescription_Ctor; this+0x0 at 0x140119c5e in 0x140119c30 CreateRemovalDescription_CopyCtor
### CreateRoadDescription  table=0x140162df0  slots=10  sizeof=0x30  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6c656946706f7243 (string "CropFieldComponent")
  reflected: ti=0x1401c74d8 create=0x140032d30 createInPlace=0x140032da0 CreateRoadDescription_CreateInPlace registered in 0x14014ec90 CreateRoadDescription_RegisterType
  [ 0] +0x00 0x140032e50  OWN CreateRoadDescription_ScalarDtor (83 B)
  [ 1] +0x08 0x140032d20  TINY returns qword [0x1401c74e0]
  [ 2] +0x10 0x140032d10  TINY returns dword [0x1401c74d8] = TypeId(CreateRoadDescription)
  [ 3] +0x18 0x140119ce0  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140119c80  OWN CreateRoadDescription_Slot5 (85 B)
  [ 6] +0x30 0x140032df0  OWN CreateRoadDescription_Draw (81 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]
  [ 9] +0x48 0x140032da0  OWN CreateRoadDescription_CreateInPlace (80 B)
  stamped by (6): other+0x0 at 0x140032d75 in 0x140032d30 CreateRoadDescription_Factory; this+0x0 at 0x140032dd4 in 0x140032da0 CreateRoadDescription_CreateInPlace; this+0x0 at 0x140032e29 in 0x140032df0 CreateRoadDescription_Draw; this+0x0 at 0x140032e74 in 0x140032e50 CreateRoadDescription_ScalarDtor; this+0x0 at 0x140119d09 in 0x140119cf0 CreateRoadDescription_Ctor; this+0x0 at 0x140119d5e in 0x140119d30 CreateRoadDescription_CopyCtor
### CropFieldComponent  table=0x140162ec0  slots=18  sizeof=0x68  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c7510 create=0x140032ed0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014ecd0 CropFieldComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x140162e70
  [ 0] +0x00 0x1400330a0  OWN CropFieldComponent_Controller_Slot10 (48 B)
  [ 1] +0x08 0x140032ec0  TINY returns qword [0x1401c7518]
  [ 2] +0x10 0x140032eb0  TINY returns dword [0x1401c7510] = TypeId(CropFieldComponent)
  [ 3] +0x18 0x14011ae70  OWN CropFieldComponent_Controller_Slot13 (226 B)
  [ 4] +0x20 0x14011ae60  OWN CropFieldComponent_Controller_Slot14 (9 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x14011ada0  OWN CropFieldComponent_Slot13 (177 B)
  [14] +0x70 0x14011acc0  OWN CropFieldComponent_Slot14 (210 B)
  [15] +0x78 0x14011ab50  OWN CropFieldComponent_Slot15 (361 B)
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (2): this+0x0 at 0x14011af7d in 0x14011af60 CropFieldComponent_Ctor; this+0x0 at 0x14011b002 in 0x14011afd0 CropFieldComponent_Ctor
### CropFieldDescription  table=0x140162f50  slots=10  sizeof=0x28  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6d6f436c61636544 (string "DecalComponent")
  reflected: ti=0x1401c7548 create=0x140032f30 createInPlace=0x140032f90 CropFieldDescription_CreateInPlace registered in 0x14014ed10 CropFieldDescription_Register
  [ 0] +0x00 0x140033040  OWN CropFieldComponent_vslot18 (83 B)
  [ 1] +0x08 0x140032f20  TINY returns qword [0x1401c7550]
  [ 2] +0x10 0x140032f10  TINY returns dword [0x1401c7548] = TypeId(CropFieldDescription)
  [ 3] +0x18 0x14011b2f0  TINY return 3 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14011b270  OWN FUN_14011b270 (111 B)
  [ 6] +0x30 0x140032fe0  OWN FUN_140032fe0 (77 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140033030  OWN (no Ghidra function)
  [ 9] +0x48 0x140032f90  OWN CropFieldDescription_CreateInPlace (76 B)
  stamped by (6): other+0x0 at 0x140032f75 in 0x140032f30 CropFieldDescription_Factory; this+0x0 at 0x140032fc4 in 0x140032f90 CropFieldDescription_CreateInPlace; this+0x0 at 0x140033019 in 0x140032fe0 FUN_140032fe0; this+0x0 at 0x140033064 in 0x140033040 CropFieldComponent_vslot18; this+0x0 at 0x14011b319 in 0x14011b300 FUN_14011b300; this+0x0 at 0x14011b36e in 0x14011b340 FUN_14011b340
### CropUI  table=0x14015f270  slots=14  sizeof=0x60  parent="SelectResourceUI" / structural SelectResourceUI 0x14015d330  stop=non-code qword 0x6944726564616f4c (string "LoaderDialog")
  reflected: ti=0x1401c5bb0 create=0x140020b10 createInPlace=0x140020b50 <no fn> registered in 0x14014cdd0 RegisterType_CropUI
  [ 0] +0x00 0x140020b60  OWN SelectResourceUI_ScalarDtor (48 B)
  [ 1] +0x08 0x140020b00  TINY returns qword [0x1401c5bb8]
  [ 2] +0x10 0x140020af0  TINY returns dword [0x1401c5bb0] = TypeId(CropUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  [ 4] +0x20 0x1400b2b50  OWN CropUI_Startup (184 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]   = parent
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]   = parent
  [ 7] +0x38 0x1400b2a50  OWN CropUI_Update (235 B)
  [ 8] +0x40 0x140078d30  OWN OrchardUI_OnRemove (66 B)  [in 4 tables]   = parent
  [ 9] +0x48 0x140078d80  OWN SelectResourceUI_Slot9 (21 B)  [in 3 tables]   = parent
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]   = parent
  [11] +0x58 0x1400b2920  OWN CropUI_OnEvent (141 B)
  [12] +0x60 0x1400b29d0  OWN FUN_1400b29d0 (114 B)
  [13] +0x68 0x1400b29c0  TINY return 0 (xor eax,eax)
  stamped by (2): this+0x0 at 0x1400b2c37 in 0x1400b2c10 CropUI_CopyCtor; this+0x0 at 0x1400b2ccb in 0x1400b2c90 CropUI_Ctor
### DebugUI  table=0x14015b870  slots=11  sizeof=0x130  parent="DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x556e657a69746943 (string "CitizenUI")
  reflected: ti=0x1401c4e58 create=0x140010ec0 createInPlace=0x140010f00 <no fn> registered in 0x14014be80 RegisterType_DebugUI
  [ 0] +0x00 0x140010f10  OWN DebugUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140010eb0  TINY returns qword [0x1401c4e60]
  [ 2] +0x10 0x140010ea0  TINY returns dword [0x1401c4e58] = TypeId(DebugUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x1400619e0  OWN DebugUI_Serialize (11269 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x1400645f0  TINY RET 0 (c2 00 00)
  [ 7] +0x38 0x140061280  OWN DebugUI_OnCreate (682 B)
  [ 8] +0x40 0x1400610c0  OWN DebugUI_OnRemove (435 B)
  [ 9] +0x48 0x140060bc0  OWN DebugUI_Slot9 (1033 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (3): this+0x0 at 0x140010f34 in 0x140010f10 DebugUI_ScalarDtor; this+0x0 at 0x140064619 in 0x140064600 DebugUI_Ctor; this+0x0 at 0x140064654 in 0x140064630 DebugUI_Ctor
### DecalComponent  table=0x140162fd0  slots=16  sizeof=0x38  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7580 create=0x1400337c0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014edd0 DecalComponent_RegisterType
  [ 0] +0x00 0x1400338d0  OWN DecalComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x1400337b0  TINY returns qword [0x1401c7588]
  [ 2] +0x10 0x1400337a0  TINY returns dword [0x1401c7580] = TypeId(DecalComponent)
  [ 3] +0x18 0x14011b820  OWN DecalComponent_Shutdown (127 B)
  [ 4] +0x20 0x14011b7d0  OWN DecalComponent_Serialize (57 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x14011b560  OWN DecalComponent_Draw (498 B)
  [ 7] +0x38 0x14011b510  OWN DecalComponent_OnCreate (61 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x14011b810  OWN DecalComponent_Slot11 (7 B)
  [12] +0x60 0x14011b760  OWN DecalComponent_Slot12 (102 B)
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x1400338f4 in 0x1400338d0 DecalComponent_ScalarDtor; this+0x0 at 0x14011b8c9 in 0x14011b8b0 DecalComponent_Ctor; this+0x0 at 0x14011b918 in 0x14011b8f0 DecalComponent_Ctor
### DecalDescription  table=0x140163050  slots=10  sizeof=0x50  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x43796f7274736544 (string "DestroyComponent")
  reflected: ti=0x1401c75b8 create=0x140033820 createInPlace=0x140033860 DecalDescription_CreateInPlace registered in 0x14014ee10 DecalDescription_Register
  [ 0] +0x00 0x1400338a0  OWN DecalComponent_vslot16 (48 B)
  [ 1] +0x08 0x140033810  TINY returns qword [0x1401c75c0]
  [ 2] +0x10 0x140033800  TINY returns dword [0x1401c75b8] = TypeId(DecalDescription)
  [ 3] +0x18 0x14011bb00  TINY return 4 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14011b980  OWN TradeWork_impl_b980 (370 B)
  [ 6] +0x30 0x140033870  OWN FUN_140033870 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140033890  OWN (no Ghidra function)
  [ 9] +0x48 0x140033860  OWN DecalDescription_CreateInPlace (12 B)
  stamped by (2): this+0x0 at 0x14011bb32 in 0x14011bb10 DecalComponent_helper_bb10; this+0x0 at 0x14011bbae in 0x14011bb80 DecalDescription_sub_bb80
### Description  table=0x1401629a0  slots=9  sizeof=abstract  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6e656e6f706d6f43 (string "ComponentDescription")
  reflected: ti=0x1401c72e0 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014ea50 Description_Register
  [ 0] +0x00 0x140031c70  OWN FUN_140031c70 (73 B)
  [ 1] +0x08 0x140031c60  TINY returns qword [0x1401c72e8]
  [ 2] +0x10 0x140031c50  TINY returns dword [0x1401c72e0] = TypeId(Description)
  [ 3] +0x18 0x14003addc  _purecall  [in 12 tables]
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x14003addc  _purecall  [in 12 tables]
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]
  stamped by (405): other+0x0 at 0x14000154b in 0x140001510 AmbientEmitterDescription_Factory; this+0x0 at 0x1400015aa in 0x140001580 AmbientEmitterDescription_CreateInPlace; this+0x0 at 0x14000160f in 0x1400015e0 FUN_1400015e0; this+0x0 at 0x1400016a2 in 0x140001660 AmbientEmitterComponent_vslot16; other+0x0 at 0x140001f7b in 0x140001f40 AmbientEnvironmentDescription_Factory; this+0x0 at 0x140001fda in 0x140001fb0 AmbientEnvironmentDescription_CreateInPlace; this+0x0 at 0x14000203f in 0x140002010 FUN_140002010; this+0x0 at 0x140002142 in 0x140002100 AmbientEnvironmentComponent_vslot16; other+0x0 at 0x14000272b in 0x1400026f0 AttendanceDescription_Factory; this+0x0 at 0x14000277a in 0x140002750 AttendanceDescription_CreateInPlace; this+0x0 at 0x1400027cf in 0x1400027a0 FUN_1400027a0; this+0x0 at 0x14000282e in 0x140002800 AttendanceComponent_vslot18 ...
### DestroyComponent  table=0x140163120  slots=18  sizeof=0x68  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c75f0 create=0x140033bc0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014ee50 DestroyComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x1401630d0
  [ 0] +0x00 0x140033dd0  OWN DestroyComponent_Controller_Slot10 (115 B)
  [ 1] +0x08 0x140033bb0  TINY returns qword [0x1401c75f8]
  [ 2] +0x10 0x140033ba0  TINY returns dword [0x1401c75f0] = TypeId(DestroyComponent)
  [ 3] +0x18 0x14011db50  OWN DestroyComponent_Controller_Slot13 (201 B)
  [ 4] +0x20 0x14011db00  OWN DestroyComponent_Controller_Slot14 (59 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x14011daf0  OWN DestroyComponent_MsgReceive_ProgressCredit (9 B)
  [16] +0x80 0x140033c00  TINY return 4 (mov eax)
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (3): this+0x0 at 0x140033df4 in 0x140033dd0 DestroyComponent_Controller_Slot10; this+0x0 at 0x14011dc39 in 0x14011dc20 DestroyComponent_Ctor; this+0x0 at 0x14011dcad in 0x14011dc80 DestroyComponent_Ctor
### DestroyDescription  table=0x1401631b0  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x43676e6968736946 (string "FishingComponent")
  reflected: ti=0x1401c7628 create=0x140033c30 createInPlace=0x140033ca0 DestroyDescription_CreateInPlace registered in 0x14014ee90 DestroyDescription_Register
  [ 0] +0x00 0x140033d70  OWN DestroyComponent_vslot18 (83 B)
  [ 1] +0x08 0x140033c20  TINY returns qword [0x1401c7630]
  [ 2] +0x10 0x140033c10  TINY returns dword [0x1401c7628] = TypeId(DestroyDescription)
  [ 3] +0x18 0x14011def0  TINY return 5 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14011de70  OWN FUN_14011de70 (121 B)
  [ 6] +0x30 0x140033d00  OWN FUN_140033d00 (91 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140033d60  OWN (no Ghidra function)
  [ 9] +0x48 0x140033ca0  OWN DestroyDescription_CreateInPlace (90 B)
  stamped by (6): other+0x0 at 0x140033c75 in 0x140033c30 DestroyDescription_Factory; this+0x0 at 0x140033cd4 in 0x140033ca0 DestroyDescription_CreateInPlace; this+0x0 at 0x140033d39 in 0x140033d00 FUN_140033d00; this+0x0 at 0x140033d94 in 0x140033d70 DestroyComponent_vslot18; this+0x0 at 0x14011df19 in 0x14011df00 FUN_14011df00; this+0x0 at 0x14011df6e in 0x14011df40 FUN_14011df40
### DestroyUI  table=0x14015bb00  slots=11  sizeof=0x78  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x4955797469746e45 (string "EntityUI")
  reflected: ti=0x1401c4ec8 create=0x140011130 createInPlace=0x1400111a0 Cleanup_DestroyUI registered in 0x14014bf00 RegisterType_DestroyUI
  [ 0] +0x00 0x140011200  OWN DestroyUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140011120  TINY returns qword [0x1401c4ed0]
  [ 2] +0x10 0x140011110  TINY returns dword [0x1401c4ec8] = TypeId(DestroyUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x140066150  OWN DestroyUI_Serialize (1215 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x140065b00  OWN DestroyUI_OnCreate (1398 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x140066080  OWN DestroyUI_Update (198 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (5): other+0x0 at 0x14001116b in 0x140011130 Factory_DestroyUI; this+0x0 at 0x1400111ca in 0x1400111a0 Cleanup_DestroyUI; this+0x0 at 0x140011224 in 0x140011200 DestroyUI_ScalarDtor; this+0x0 at 0x140066639 in 0x140066620 DestroyUI_Ctor; this+0x0 at 0x140066674 in 0x140066650 DestroyUI_CopyCtor
### Disease  table=0x14015e190  slots=9  sizeof=0x48  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6d6e6f7269766e45 (string "EnvironmentDescription")
  reflected: ti=0x1401c5948 create=0x140018fd0 createInPlace=0x140019040 ClimateGenerator_vslot18 registered in 0x14014cb00 ClimateGenerator_StaticInit
  [ 0] +0x00 0x140018f60  OWN ClimateGenerator_ScalarDtor (77 B)  [in 2 tables]
  [ 1] +0x08 0x140018fc0  TINY returns qword [0x1401c5950]
  [ 2] +0x10 0x140018fb0  TINY returns dword [0x1401c5948] = TypeId(Disease)
  [ 3] +0x18 0x140093390  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400932d0  OWN ClimateGenerator_vslot15 (172 B)
  [ 6] +0x30 0x1400190a0  OWN ClimateGenerator_vslot16 (92 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140019040  OWN ClimateGenerator_vslot18 (91 B)
  stamped by (4): other+0x0 at 0x14001900b in 0x140018fd0 ClimateGenerator_StaticInit_sub_8fd0; this+0x0 at 0x14001906a in 0x140019040 ClimateGenerator_vslot18; this+0x0 at 0x1400190cf in 0x1400190a0 ClimateGenerator_vslot16; this+0x0 at 0x1400933c4 in 0x1400933a0 FUN_1400933a0
### DiseaseComponent  table=0x140159290  slots=16  sizeof=0x58  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c4170 create=0x140004db0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014ab40 DiseaseComponent_RegisterType
  [ 0] +0x00 0x140004f80  OWN DiseaseComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x140004da0  TINY returns qword [0x1401c4178]
  [ 2] +0x10 0x140004d90  TINY returns dword [0x1401c4170] = TypeId(DiseaseComponent)
  [ 3] +0x18 0x140048ca0  OWN DiseaseComponent_Shutdown (196 B)
  [ 4] +0x20 0x140048c50  OWN DiseaseComponent_Serialize (59 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140048c00  OWN DiseaseComponent_Draw (59 B)
  [ 7] +0x38 0x140048bb0  OWN DiseaseComponent_OnCreate (67 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140048b90  OWN DiseaseComponent_Update (16 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140004fa4 in 0x140004f80 DiseaseComponent_ScalarDtor; this+0x0 at 0x140048d89 in 0x140048d70 DiseaseComponent_Ctor; this+0x0 at 0x140048dd8 in 0x140048db0 DiseaseComponent_Ctor
### DiseaseDescription  table=0x140159310  slots=10  sizeof=0x48  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x706d6f4365726946 (string "FireComponent")
  reflected: ti=0x1401c41a8 create=0x140004e10 createInPlace=0x140004e70 DiseaseDescription_CreateInPlace registered in 0x14014ab80 DiseaseDescription_Register
  [ 0] +0x00 0x140004f20  OWN DiseaseComponent_vslot16 (83 B)
  [ 1] +0x08 0x140004e00  TINY returns qword [0x1401c41b0]
  [ 2] +0x10 0x140004df0  TINY returns dword [0x1401c41a8] = TypeId(DiseaseDescription)
  [ 3] +0x18 0x140048f20  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140048e40  OWN FUN_140048e40 (215 B)
  [ 6] +0x30 0x140004ec0  OWN FUN_140004ec0 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140004f10  OWN (no Ghidra function)
  [ 9] +0x48 0x140004e70  OWN DiseaseDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x140004e55 in 0x140004e10 DiseaseDescription_Factory; this+0x0 at 0x140004ea4 in 0x140004e70 DiseaseDescription_CreateInPlace; this+0x0 at 0x140004ef9 in 0x140004ec0 FUN_140004ec0; this+0x0 at 0x140004f44 in 0x140004f20 DiseaseComponent_vslot16; this+0x0 at 0x140048f49 in 0x140048f30 FUN_140048f30; this+0x0 at 0x140048f9e in 0x140048f70 FUN_140048f70
### DragPlacementTool  table=0x140161b90  slots=15  sizeof=0x48  parent="DragTool" / structural DragTool 0x140161c10  stop=zero qword
  reflected: ti=0x1401c6d68 create=0x14002d8b0 createInPlace=0x14002d8f0 <no fn> registered in 0x14014e210 DragPlacementTool_RegisterType
  [ 0] +0x00 0x14002d900  OWN DragPlacementTool_ScalarDtor (93 B)
  [ 1] +0x08 0x14002d8a0  TINY returns qword [0x1401c6d70]
  [ 2] +0x10 0x14002d890  TINY returns dword [0x1401c6d68] = TypeId(DragPlacementTool)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  [ 4] +0x20 0x1400fc3e0  OWN DragPlacementTool_Serialize (247 B)
  [ 5] +0x28 0x1400fc3d0  TINY RET 0 (c2 00 00)
  [ 6] +0x30 0x1400fc6b0  OWN BridgeTool_Draw (785 B)  [in 4 tables]   = parent
  [ 7] +0x38 0x1400fc3c0  TINY RET 0 (c2 00 00)
  [ 8] +0x40 0x1400fbe40  OWN DragPlacementTool_OnRemove (1402 B)
  [ 9] +0x48 0x1400fc640  OWN BridgeTool_Update (103 B)  [in 5 tables]   = parent
  [10] +0x50 0x1400268a0  TINY return false (xor al,al)  [in 8 tables]   = parent
  [11] +0x58 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]   = parent
  [12] +0x60 0x1400fc9d0  TINY return 5 (mov eax)  [in 6 tables]   = parent
  [13] +0x68 0x1400fb900  OWN DragPlacementTool_Slot13 (461 B)
  [14] +0x70 0x1400fbae0  OWN DragPlacementTool_Slot14 (736 B)
  stamped by (3): this+0x0 at 0x14002d924 in 0x14002d900 DragPlacementTool_ScalarDtor; this+0x0 at 0x1400fc4f9 in 0x1400fc4e0 Tool_Ctor4; this+0x0 at 0x1400fc585 in 0x1400fc530 DragPlacementTool_Ctor
### DragTool  table=0x140161c10  slots=15  sizeof=abstract  parent="Tool" / structural Tool 0x140161f10  stop=non-code qword 0x6c6f6f5474696445 (string "EditTool")
  reflected: ti=0x1401c6da0 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014e250 DragTool_RegisterType
  [ 0] +0x00 0x14002da20  OWN DragTool_ScalarDtor (83 B)
  [ 1] +0x08 0x14002da10  TINY returns qword [0x1401c6da8]
  [ 2] +0x10 0x14002da00  TINY returns dword [0x1401c6da0] = TypeId(DragTool)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  [ 4] +0x20 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 5] +0x28 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 6] +0x30 0x1400fc6b0  OWN BridgeTool_Draw (785 B)  [in 4 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]   = parent
  [ 9] +0x48 0x1400fc640  OWN BridgeTool_Update (103 B)  [in 5 tables]
  [10] +0x50 0x1400268a0  TINY return false (xor al,al)  [in 8 tables]   = parent
  [11] +0x58 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]   = parent
  [12] +0x60 0x1400fc9d0  TINY return 5 (mov eax)  [in 6 tables]
  [13] +0x68 0x14003addc  _purecall  [in 12 tables]
  [14] +0x70 0x14003addc  _purecall  [in 12 tables]
  stamped by (18): this+0x0 at 0x14002695e in 0x140026930 TimeGoalInstance_OnCreate; this+0x0 at 0x14002d81e in 0x14002d7f0 BridgeTool_ScalarDtor; this+0x0 at 0x14002d92e in 0x14002d900 DragPlacementTool_ScalarDtor; this+0x0 at 0x14002da44 in 0x14002da20 DragTool_ScalarDtor; this+0x0 at 0x14002e81e in 0x14002e7f0 RemovalTool_ScalarDtor; this+0x0 at 0x14002e8ee in 0x14002e8c0 RoadTool_ScalarDtor; this+0x0 at 0x1400dd043 in 0x1400dd020 Tool_Ctor; this+0x0 at 0x1400dd0a8 in 0x1400dd070 FencedTool_Ctor; this+0x0 at 0x1400fb863 in 0x1400fb840 Tool_Ctor3; this+0x0 at 0x1400fb8c8 in 0x1400fb890 BridgeTool_Ctor; this+0x0 at 0x1400fc503 in 0x1400fc4e0 Tool_Ctor4; this+0x0 at 0x1400fc568 in 0x1400fc530 DragPlacementTool_Ctor ...
### DropOffRawMaterial  table=0x14015fd80  slots=10  sizeof=0x40  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x6d696e41746e7548 (string "HuntAnimal")
  reflected: ti=0x1401c5f30 create=0x140022600 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d1d0 DropOffRawMaterial_RegisterType
  [ 0] +0x00 0x140022670  OWN DropOffRawMaterial_ScalarDtor (104 B)
  [ 1] +0x08 0x1400225f0  TINY returns qword [0x1401c5f38]
  [ 2] +0x10 0x1400225e0  TINY returns dword [0x1401c5f30] = TypeId(DropOffRawMaterial)
  [ 3] +0x18 0x1400c7b10  OWN DropOffRawMaterial_Shutdown (258 B)
  [ 4] +0x20 0x1400c7300  OWN DropOffRawMaterial_Serialize (616 B)
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]   = parent
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400c72f0  JMP 0x1400c7570 Entity_impl_7570
  [ 9] +0x48 0x1400c7270  OWN DropOffRawMaterial_Update (108 B)
  stamped by (5): other+0x0 at 0x14002264e in 0x140022600 DropOffRawMaterial_Factory; this+0x0 at 0x140022694 in 0x140022670 DropOffRawMaterial_ScalarDtor; this+0x0 at 0x1400c7c3d in 0x1400c7c20 Action_Action_Ctor3; this+0x0 at 0x1400c7ca7 in 0x1400c7c70 DropOffRawMaterial_Action_Ctor; this+0x0 at 0x1400c7d1c in 0x1400c7cd0 DropOffRawMaterial_Action_CopyCtor
### EditTool  table=0x140161ca0  slots=14  sizeof=0x38  parent="Tool" / structural Tool 0x140161f10  stop=non-code qword 0x6e656d6563616c50 (string "PlacementTool")
  reflected: ti=0x1401c6dd8 create=0x14002daa0 createInPlace=0x14002dae0 <no fn> registered in 0x14014e290 EditTool_RegisterType
  [ 0] +0x00 0x14002db00  OWN EditTool_ScalarDtor (83 B)
  [ 1] +0x08 0x14002da90  TINY returns qword [0x1401c6de0]
  [ 2] +0x10 0x14002da80  TINY returns dword [0x1401c6dd8] = TypeId(EditTool)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  [ 4] +0x20 0x1400fd2f0  TINY RET 0 (c2 00 00)
  [ 5] +0x28 0x1400fd2e0  JMP 0x1400fcc90 EditTool_Deselect_CloseInfoDialog
  [ 6] +0x30 0x1400fd030  OWN EditTool_Draw (595 B)
  [ 7] +0x38 0x1400fcda0  OWN EditTool_Startup_PositionInfoDialog (637 B)
  [ 8] +0x40 0x1400fcd90  TINY RET 0 (c2 00 00)
  [ 9] +0x48 0x1400fd2d0  TINY return false (xor al,al)
  [10] +0x50 0x1400fd290  OWN EditTool_Slot10 (46 B)
  [11] +0x58 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]   = parent
  [12] +0x60 0x140105860  TINY return 1 (mov eax)  [in 2 tables]   = parent
  [13] +0x68 0x14002daf0  OWN EditTool_Slot13 (11 B)
  stamped by (3): this+0x0 at 0x14002db24 in 0x14002db00 EditTool_ScalarDtor; this+0x0 at 0x1400fd319 in 0x1400fd300 Tool_Ctor8; this+0x0 at 0x1400fd37e in 0x1400fd340 EditTool_Ctor
### EntityUI  table=0x14015bb90  slots=11  sizeof=0x48  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x676f4c746e657645 (string "EventLogUI")
  reflected: ti=0x1401c4f00 create=0x140011270 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014bf40 RegisterType_EntityUI
  [ 0] +0x00 0x1400112d0  OWN EntityUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140011260  TINY returns qword [0x1401c4f08]
  [ 2] +0x10 0x140011250  TINY returns dword [0x1401c4f00] = TypeId(EntityUI)
  [ 3] +0x18 0x1400672a0  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x1400670d0  OWN EntityUI_Serialize (445 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x140067090  OWN EntityUI_Draw (46 B)
  [ 7] +0x38 0x140066d80  OWN EntityUI_OnCreate (659 B)
  [ 8] +0x40 0x140066ad0  OWN EntityUI_Slot8_PerFrameInput (670 B)
  [ 9] +0x48 0x140067050  OWN EntityUI_Slot9 (55 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x1400112ab in 0x140011270 Factory_EntityUI; this+0x0 at 0x1400112f4 in 0x1400112d0 EntityUI_ScalarDtor; this+0x0 at 0x1400672c9 in 0x1400672b0 EntityUI_Ctor; this+0x0 at 0x140067304 in 0x1400672e0 EntityUI_CopyCtor
### EnvironmentDescription  table=0x14015e1f0  slots=9  sizeof=0x1b8  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x73252e6925 (string "%i.%s")
  reflected: ti=0x1401c5980 create=0x140019120 createInPlace=0x140019160 EnvironmentDescription_CreateInPlace registered in 0x14014cb40 EnvironmentDescription_RegisterType
  [ 0] +0x00 0x140019190  OWN EnvironmentDescription_ScalarDtor (86 B)
  [ 1] +0x08 0x140019110  TINY returns qword [0x1401c5988]
  [ 2] +0x10 0x140019100  TINY returns dword [0x1401c5980] = TypeId(EnvironmentDescription)
  [ 3] +0x18 0x1400941b0  TINY return 7 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x140094040  OWN EnvironmentDescription_Slot5 (352 B)
  [ 6] +0x30 0x140019170  OWN EnvironmentDescription_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140019160  OWN EnvironmentDescription_CreateInPlace (12 B)
  stamped by (1): this+0x0 at 0x1400941e4 in 0x1400941c0 EnvironmentDescription_Ctor
### EventLogUI  table=0x14015bc40  slots=11  sizeof=0x80  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=zero qword
  reflected: ti=0x1401c4f38 create=0x1400113e0 createInPlace=0x140011450 Cleanup_EventLogUI registered in 0x14014bf80 RegisterType_EventLogUI
  [ 0] +0x00 0x1400114b0  OWN EventLogUI_ScalarDtor (48 B)
  [ 1] +0x08 0x1400113d0  TINY returns qword [0x1401c4f40]
  [ 2] +0x10 0x1400113c0  TINY returns dword [0x1401c4f38] = TypeId(EventLogUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x14003a984  IMPORT ?OnCreated@DialogController@UI@@UEAAXXZ  [in 5 tables]
  [ 5] +0x28 0x140067640  OWN EventLogUI_Slot5 (1283 B)
  [ 6] +0x30 0x140067630  TINY RET 0 (c2 00 00)
  [ 7] +0x38 0x140067440  OWN EventLogUI_OnCreate (384 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x1400675d0  OWN EventLogUI_Slot9 (84 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x14001141b in 0x1400113e0 Factory_EventLogUI; this+0x0 at 0x14001147a in 0x140011450 Cleanup_EventLogUI; this+0x0 at 0x140067b72 in 0x140067b50 EventLogUI_Ctor; this+0x0 at 0x140067bf4 in 0x140067bd0 EventLogUI_CopyCtor
### EventNotifyDialog  table=0x14015bca0  slots=10  sizeof=0xf0  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x746f4e746e657645 (string "EventNotifyDialog")
  reflected: ti=0x1401c4f70 create=0x140011500 createInPlace=0x140011540 <no fn> registered in 0x14014bfc0 RegisterType_EventNotifyDialog
  [ 0] +0x00 0x140011550  OWN EventLogUI_Slot12 (118 B)
  [ 1] +0x08 0x1400114f0  TINY returns qword [0x1401c4f78]
  [ 2] +0x10 0x1400114e0  TINY returns dword [0x1401c4f70] = TypeId(EventNotifyDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x1400686d0  OWN EventNotifyDialog_Serialize (880 B)
  [ 5] +0x28 0x1400686c0  TINY RET 0 (c2 00 00)
  [ 6] +0x30 0x1400680c0  OWN EventNotifyDialog_Draw (1230 B)
  [ 7] +0x38 0x140067c30  OWN EventNotifyDialog_OnCreate (1089 B)
  [ 8] +0x40 0x140068680  OWN EventNotifyDialog_OnEvent (45 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (4): this+0x0 at 0x140011574 in 0x140011550 EventLogUI_Slot12; this+0x0 at 0x140068a6d in 0x140068a50 EventNotifyDialog_Ctor; this+0x0 at 0x140068af2 in 0x140068ab0 EventNotifyDialog_CopyCtor; this+0x0 at 0x140068b84 in 0x140068b60 EventNotifyDialog_Ctor
### EventPopupDialog  table=0x14015bd80  slots=10  sizeof=0xc0  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x6944776f6c6c6f46 (string "FollowDialog")
  reflected: ti=0x1401c4fa8 create=0x140011920 createInPlace=0x140011970 Cleanup_EventPopupDialog registered in 0x14014c000 RegisterType_EventPopupDialog
  [ 0] +0x00 0x1400119b0  OWN EventPopupDialog_ScalarDtor (73 B)
  [ 1] +0x08 0x140011910  TINY returns qword [0x1401c4fb0]
  [ 2] +0x10 0x140011900  TINY returns dword [0x1401c4fa8] = TypeId(EventPopupDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x140068c20  OWN EventPopupDialog_Serialize (516 B)
  [ 5] +0x28 0x14003a94e  IMPORT ?OnClosed@DialogInstance@UI@@UEAAXXZ  [in 12 tables]
  [ 6] +0x30 0x14003a98a  IMPORT ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ  [in 8 tables]
  [ 7] +0x38 0x14003a990  IMPORT ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z  [in 11 tables]
  [ 8] +0x40 0x140068bd0  OWN EventPopupDialog_OnRemove (59 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (7): other+0x0 at 0x14001195d in 0x140011920 Factory_EventPopupDialog; this+0x0 at 0x14001199a in 0x140011970 Cleanup_EventPopupDialog; this+0x0 at 0x1400119d4 in 0x1400119b0 EventPopupDialog_ScalarDtor; other+0x0 at 0x140067e12 in 0x140067c30 EventNotifyDialog_OnCreate; this+0x0 at 0x140068e49 in 0x140068e30 EventPopupDialog_Ctor; this+0x0 at 0x140068e84 in 0x140068e60 EventPopupDialog_CopyCtor; this+0x0 at 0x140068ee1 in 0x140068ea0 EventPopupDialog_Ctor3
### FencedTool  table=0x1401605d0  slots=15  sizeof=0x70  parent="DragTool" / structural DragTool 0x140161c10  stop=non-code qword 0x6c6f6f5467617244 (string "DragTool")
  reflected: ti=0x1401c6358 create=0x1400268d0 createInPlace=0x140026910 <no fn> registered in 0x14014d690 FencedTool_RegisterType
  [ 0] +0x00 0x140026930  OWN TimeGoalInstance_OnCreate (93 B)
  [ 1] +0x08 0x1400268c0  TINY returns qword [0x1401c6360]
  [ 2] +0x10 0x1400268b0  TINY returns dword [0x1401c6358] = TypeId(FencedTool)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  [ 4] +0x20 0x1400dcd50  OWN TimeGoalInstance_Slot11 (709 B)
  [ 5] +0x28 0x1400dccc0  OWN TimeGoalInstance_Slot12 (64 B)
  [ 6] +0x30 0x1400dcbe0  OWN TimeGoalInstance_Slot13 (207 B)
  [ 7] +0x38 0x1400dcad0  OWN TimeGoalInstance_Slot14 (259 B)
  [ 8] +0x40 0x1400dc440  OWN TimeGoalInstance_Slot15 (1673 B)
  [ 9] +0x48 0x1400dcd10  OWN FencedTool_Update (53 B)
  [10] +0x50 0x1400268a0  TINY return false (xor al,al)  [in 8 tables]   = parent
  [11] +0x58 0x140026920  OWN FencedTool_Slot11 (12 B)  [in 2 tables]
  [12] +0x60 0x1400fc9d0  TINY return 5 (mov eax)  [in 6 tables]   = parent
  [13] +0x68 0x1400dae00  OWN FencedTool_Slot13 (1660 B)
  [14] +0x70 0x1400db490  OWN FencedTool_Slot14 (1063 B)
  stamped by (3): this+0x0 at 0x140026954 in 0x140026930 TimeGoalInstance_OnCreate; this+0x0 at 0x1400dd039 in 0x1400dd020 Tool_Ctor; this+0x0 at 0x1400dd0c5 in 0x1400dd070 FencedTool_Ctor
### FinishTutorialGoal  table=0x140160420  slots=11  sizeof=0x38  parent="Achievement::Goal" / structural Achievement::Goal (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c6240 create=0x1400260f0 createInPlace=0x140026150 FinishTutorialGoal_Cleanup registered in 0x14014d550 FinishTutorialGoal_RegisterType
  [ 0] +0x00 0x140026260  OWN FinishTutorialGoal_ScalarDtor (73 B)
  [ 1] +0x08 0x1400260e0  TINY returns qword [0x1401c6248]
  [ 2] +0x10 0x1400260d0  TINY returns dword [0x1401c6240] = TypeId(FinishTutorialGoal)
  [ 3] +0x18 0x1400d79d0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400d7970  OWN FinishTutorialGoal_Slot5 (89 B)
  [ 6] +0x30 0x1400261a0  OWN FinishTutorialGoal_Draw (67 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400d7960  TINY return 0 (xor eax,eax)
  [ 9] +0x48 0x1400261f0  OWN FinishTutorialGoal_Update (12 B)
  [10] +0x50 0x140026150  OWN FinishTutorialGoal_Cleanup (66 B)
  stamped by (6): other+0x0 at 0x14002612b in 0x1400260f0 FinishTutorialGoal_Factory; this+0x0 at 0x14002617a in 0x140026150 FinishTutorialGoal_Cleanup; this+0x0 at 0x1400261cf in 0x1400261a0 FinishTutorialGoal_Draw; this+0x0 at 0x140026284 in 0x140026260 FinishTutorialGoal_ScalarDtor; this+0x0 at 0x1400d79f9 in 0x1400d79e0 FinishTutorialGoal_Ctor; this+0x0 at 0x1400d7a34 in 0x1400d7a10 FinishTutorialGoal_CopyCtor
### FinishTutorialInstance  table=0x140160478  slots=7  sizeof=0x28  parent="Achievement::GoalInstance" / structural Achievement::GoalInstance (Runtime ctor import)  stop=non-code qword 0x6c616f4774617453 (string "StatGoal")
  reflected: ti=0x1401c6278 create=0x140026070 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d590 FinishTutorialInstance_RegisterType
  [ 0] +0x00 0x140026200  OWN FinishTutorialGoal_Slot11 (87 B)
  [ 1] +0x08 0x140026060  TINY returns qword [0x1401c6280]
  [ 2] +0x10 0x140026050  TINY returns dword [0x1401c6278] = TypeId(FinishTutorialInstance)
  [ 3] +0x18 0x1400d7bf0  OWN FinishTutorialGoal_Slot14 (120 B)
  [ 4] +0x20 0x1400d7a90  OWN FinishTutorialGoal_Slot15 (279 B)
  [ 5] +0x28 0x1400d7a50  OWN FinishTutorialInstance_Slot5 (47 B)
  [ 6] +0x30 0x1400d7bb0  OWN FinishTutorialInstance_Draw (43 B)
  stamped by (4): other+0x0 at 0x1400260ab in 0x140026070 FinishTutorialInstance_Factory; this+0x0 at 0x140026224 in 0x140026200 FinishTutorialGoal_Slot11; this+0x0 at 0x1400d7c8d in 0x1400d7c70 FinishTutorialInstance_Ctor; this+0x0 at 0x1400d7cd4 in 0x1400d7cb0 FinishTutorialInstance_CopyCtor
### FireComponent  table=0x140159380  slots=16  sizeof=0x48  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c41e0 create=0x1400055e0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014abc0 FireComponent_RegisterType
  [ 0] +0x00 0x140005720  OWN FireComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x1400055d0  TINY returns qword [0x1401c41e8]
  [ 2] +0x10 0x1400055c0  TINY returns dword [0x1401c41e0] = TypeId(FireComponent)
  [ 3] +0x18 0x140049a90  OWN FireComponent_Shutdown (157 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140048fb0  OWN FireComponent_Update (204 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140049090  OWN FireComponent_Slot15 (141 B)
  stamped by (3): this+0x0 at 0x140005744 in 0x140005720 FireComponent_ScalarDtor; this+0x0 at 0x140049b59 in 0x140049b40 FireComponent_Ctor; this+0x0 at 0x140049ba8 in 0x140049b80 FireComponent_Ctor
### FireDescription  table=0x140159400  slots=10  sizeof=0x50  parent="Description" / structural Description 0x1401629a0  stop=next table
  reflected: ti=0x1401c4218 create=0x140005640 createInPlace=0x140005680 FireDescription_CreateInPlace registered in 0x14014ac00 FireDescription_Register
  [ 0] +0x00 0x1400056c0  OWN FireComponent_vslot16 (83 B)
  [ 1] +0x08 0x140005630  TINY returns qword [0x1401c4220]
  [ 2] +0x10 0x140005620  TINY returns dword [0x1401c4218] = TypeId(FireDescription)
  [ 3] +0x18 0x140049d10  TINY return 4 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140049c00  OWN FUN_140049c00 (261 B)
  [ 6] +0x30 0x140005690  OWN FUN_140005690 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400056b0  OWN (no Ghidra function)
  [ 9] +0x48 0x140005680  OWN FireDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x1400056e4 in 0x1400056c0 FireComponent_vslot16; this+0x0 at 0x140049d39 in 0x140049d20 FUN_140049d20; this+0x0 at 0x140049d8e in 0x140049d60 FireDescription_sub_9d60
### FireWork  table=0x1401612a0  slots=13  sizeof=0x60  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x57676e6968736946 (string "FishingWork")
  reflected: ti=0x1401c6978 create=0x14002a1e0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014dd90 FireWork_RegisterType
  [ 0] +0x00 0x14002a230  OWN FireWork_ScalarDtor (48 B)
  [ 1] +0x08 0x14002a1d0  TINY returns qword [0x1401c6980]
  [ 2] +0x10 0x14002a1c0  TINY returns dword [0x1401c6978] = TypeId(FireWork)
  [ 3] +0x18 0x1400e9a50  OWN FireWork_Shutdown (104 B)
  [ 4] +0x20 0x1400e9620  OWN FireWork_Serialize (876 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400e9600  OWN FireWork_Draw (20 B)
  [ 7] +0x38 0x1400e95f0  THUNK this+80 -> 0x140025520 CitizenAI_impl_5520
  [ 8] +0x40 0x1400e99a0  OWN FireWork_OnRemove (132 B)
  [ 9] +0x48 0x1400e93c0  OWN FireWork_Update (547 B)
  [10] +0x50 0x1400e9a30  OWN FireWork_Slot10 (25 B)
  [11] +0x58 0x14002a220  OWN FireWork_Slot11 (9 B)
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (3): this+0x0 at 0x1400e9ae7 in 0x1400e9ac0 Work_Ctor13; this+0x0 at 0x1400e9ba7 in 0x1400e9b50 FireWork_Ctor; this+0x0 at 0x1400e9c08 in 0x1400e9bd0 FireWork_Ctor
### FishingComponent  table=0x140163230  slots=22  sizeof=0x58  parent="ResourceProduceComponent" / structural ResourceProduceComponent 0x1401643f0  stop=next table
  reflected: ti=0x1401c7660 create=0x140034350 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014ef50 FishingComponent_RegisterType
  [ 0] +0x00 0x140034560  OWN FishingComponent_ScalarDtor (125 B)
  [ 1] +0x08 0x140034340  TINY returns qword [0x1401c7668]
  [ 2] +0x10 0x140034330  TINY returns dword [0x1401c7660] = TypeId(FishingComponent)
  [ 3] +0x18 0x14011f050  OWN FishingComponent_Shutdown (108 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x14012b520  OWN HerbalistComponent_Slot13 (112 B)  [in 6 tables]   = parent
  [14] +0x70 0x14012b5a0  OWN HerbalistComponent_Slot14 (117 B)  [in 6 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]   = parent
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]   = parent
  [18] +0x90 0x14011efe0  OWN FishingComponent_vslot18 (99 B)
  [19] +0x98 0x14011ee80  OWN FishingComponent_vslot19 (66 B)
  [20] +0xa0 0x14011ee50  OWN FUN_14011ee50 (33 B)
  [21] +0xa8 0x14011ec10  OWN FUN_14011ec10 (557 B)
  stamped by (4): other+0x0 at 0x14003438a in 0x140034350 FishingComponent_Factory; this+0x0 at 0x140034584 in 0x140034560 FishingComponent_ScalarDtor; this+0x0 at 0x14011f0e9 in 0x14011f0d0 ResourceProduceComponent_Ctor4; this+0x0 at 0x14011f153 in 0x14011f130 FishingComponent_Ctor
### FishingDescription  table=0x1401632e0  slots=10  sizeof=0x30  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x736544726f6f6c46 (string "FloorDescription")
  reflected: ti=0x1401c7698 create=0x1400343c0 createInPlace=0x140034430 FishingDescription_CreateInPlace registered in 0x14014ef90 FishingDescription_Register
  [ 0] +0x00 0x140034500  OWN FUN_140034500 (83 B)
  [ 1] +0x08 0x1400343b0  TINY returns qword [0x1401c76a0]
  [ 2] +0x10 0x1400343a0  TINY returns dword [0x1401c7698] = TypeId(FishingDescription)
  [ 3] +0x18 0x14011f200  TINY return 4 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14011f170  OWN FUN_14011f170 (128 B)
  [ 6] +0x30 0x140034490  OWN FUN_140034490 (91 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400344f0  OWN (no Ghidra function)
  [ 9] +0x48 0x140034430  OWN FishingDescription_CreateInPlace (90 B)
  stamped by (6): other+0x0 at 0x140034405 in 0x1400343c0 FishingDescription_Factory; this+0x0 at 0x140034464 in 0x140034430 FishingDescription_CreateInPlace; this+0x0 at 0x1400344c9 in 0x140034490 FUN_140034490; this+0x0 at 0x140034524 in 0x140034500 FUN_140034500; this+0x0 at 0x14011f229 in 0x14011f210 FUN_14011f210; this+0x0 at 0x14011f27e in 0x14011f250 FUN_14011f250
### FishingWork  table=0x140161320  slots=13  sizeof=0x70  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x7265747365726f46 (string "ForesterWork")
  reflected: ti=0x1401c69b0 create=0x14002a280 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014ddd0 FishingWork_RegisterType
  [ 0] +0x00 0x14002a2c0  OWN FishingWork_ScalarDtor (48 B)
  [ 1] +0x08 0x14002a270  TINY returns qword [0x1401c69b8]
  [ 2] +0x10 0x14002a260  TINY returns dword [0x1401c69b0] = TypeId(FishingWork)
  [ 3] +0x18 0x1400ea910  OWN FishingWork_Shutdown (301 B)
  [ 4] +0x20 0x1400e9f20  OWN FishingWork_Serialize (2307 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400e9eb0  OWN FishingWork_Draw (93 B)
  [ 7] +0x38 0x1400e9e80  OWN FishingWork_OnCreate (41 B)
  [ 8] +0x40 0x1400ea830  OWN FishingWork_OnRemove (118 B)
  [ 9] +0x48 0x1400e9c30  OWN FishingWork_Update (578 B)
  [10] +0x50 0x1400ea8b0  OWN FishingWork_Slot10 (75 B)
  [11] +0x58 0x140029860  OWN ClearArea_Slot11 (9 B)  [in 13 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (3): this+0x0 at 0x1400eaa72 in 0x1400eaa50 Work_Ctor14; this+0x0 at 0x1400eab04 in 0x1400eaac0 FishingWork_Ctor; this+0x0 at 0x1400eac37 in 0x1400eabe0 FishingWork_Ctor
### FloorComponent  table=0x140163360  slots=16  sizeof=0x30  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c76d0 create=0x140034600 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014efd0 FloorComponent_RegisterType
  [ 0] +0x00 0x1400347f0  OWN FloorComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x1400345f0  TINY returns qword [0x1401c76d8]
  [ 2] +0x10 0x1400345e0  TINY returns dword [0x1401c76d0] = TypeId(FloorComponent)
  [ 3] +0x18 0x14011f560  OWN FloorComponent_Shutdown (90 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x14011f340  OWN FloorComponent_Draw (65 B)
  [ 7] +0x38 0x14011f2a0  OWN FloorComponent_OnCreate (145 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x14011f540  OWN FloorComponent_OnUpdate2 (12 B)
  [12] +0x60 0x14011f520  OWN FloorComponent_Slot12 (11 B)
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140034814 in 0x1400347f0 FloorComponent_ScalarDtor; this+0x0 at 0x14011f5d9 in 0x14011f5c0 FloorComponent_Ctor; this+0x0 at 0x14011f628 in 0x14011f600 FloorComponent_Ctor
### FloorDescription  table=0x1401633e0  slots=10  sizeof=0x30  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x7265747365726f46 (string "ForesterComponent")
  reflected: ti=0x1401c7708 create=0x140034660 createInPlace=0x1400346d0 FloorDescription_CreateInPlace registered in 0x14014f010 FloorDescription_Register
  [ 0] +0x00 0x140034790  OWN FloorComponent_vslot16 (83 B)
  [ 1] +0x08 0x140034650  TINY returns qword [0x1401c7710]
  [ 2] +0x10 0x140034640  TINY returns dword [0x1401c7708] = TypeId(FloorDescription)
  [ 3] +0x18 0x14011f6e0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14011f680  OWN FUN_14011f680 (85 B)
  [ 6] +0x30 0x140034720  OWN FUN_140034720 (81 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140034780  OWN (no Ghidra function)
  [ 9] +0x48 0x1400346d0  OWN FloorDescription_CreateInPlace (80 B)
  stamped by (6): other+0x0 at 0x1400346a5 in 0x140034660 FloorDescription_Factory; this+0x0 at 0x140034704 in 0x1400346d0 FloorDescription_CreateInPlace; this+0x0 at 0x140034759 in 0x140034720 FUN_140034720; this+0x0 at 0x1400347b4 in 0x140034790 FloorComponent_vslot16; this+0x0 at 0x14011f709 in 0x14011f6f0 FUN_14011f6f0; this+0x0 at 0x14011f75e in 0x14011f730 FUN_14011f730
### FollowDialog  table=0x14015be00  slots=10  sizeof=0xd8  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x6c6169446c616f47 (string "GoalDialog")
  reflected: ti=0x1401c4fe0 create=0x140011a20 createInPlace=0x140011a90 Cleanup_FollowDialog registered in 0x14014c040 RegisterType_FollowDialog
  [ 0] +0x00 0x140011af0  OWN FollowDialog_ScalarDtor (73 B)
  [ 1] +0x08 0x140011a10  TINY returns qword [0x1401c4fe8]
  [ 2] +0x10 0x140011a00  TINY returns dword [0x1401c4fe0] = TypeId(FollowDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x140069340  OWN FollowDialog_Serialize (456 B)
  [ 5] +0x28 0x140069320  OWN FollowDialog_Slot5 (19 B)
  [ 6] +0x30 0x140068f70  OWN FollowDialog_Draw (694 B)
  [ 7] +0x38 0x140069250  OWN FollowDialog_OnCreate (202 B)
  [ 8] +0x40 0x140069230  OWN FollowDialog_Slot8 (18 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (6): other+0x0 at 0x140011a5d in 0x140011a20 Factory_FollowDialog; this+0x0 at 0x140011aba in 0x140011a90 Cleanup_FollowDialog; this+0x0 at 0x140011b14 in 0x140011af0 FollowDialog_ScalarDtor; this+0x0 at 0x140069529 in 0x140069510 FollowDialog_Ctor; this+0x0 at 0x140069564 in 0x140069540 FollowDialog_CopyCtor; this+0x0 at 0x1400695e2 in 0x1400695a0 FollowDialog_Ctor3
### ForesterComponent  table=0x140163460  slots=22  sizeof=0x58  parent="ResourceProduceComponent" / structural ResourceProduceComponent 0x1401643f0  stop=next table
  reflected: ti=0x1401c7740 create=0x140034870 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f050 ForesterComponent_RegisterType
  [ 0] +0x00 0x140034a70  OWN ForesterComponent_ScalarDtor (125 B)
  [ 1] +0x08 0x140034860  TINY returns qword [0x1401c7748]
  [ 2] +0x10 0x140034850  TINY returns dword [0x1401c7740] = TypeId(ForesterComponent)
  [ 3] +0x18 0x14011f960  OWN ForesterComponent_Shutdown (108 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x14011f920  OWN ForesterComponent_Slot13 (44 B)
  [14] +0x70 0x14011f890  OWN ForesterComponent_Slot14 (125 B)
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]   = parent
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]   = parent
  [18] +0x90 0x14011f780  OWN ForesterWork_CopyCtor (261 B)
  [19] +0x98 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  [20] +0xa0 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  [21] +0xa8 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (4): other+0x0 at 0x1400348aa in 0x140034870 ForesterComponent_Factory; this+0x0 at 0x140034a94 in 0x140034a70 ForesterComponent_ScalarDtor; this+0x0 at 0x14011f9f9 in 0x14011f9e0 ResourceProduceComponent_Ctor5; this+0x0 at 0x14011fa63 in 0x14011fa40 ForesterComponent_Ctor
### ForesterDescription  table=0x140163510  slots=10  sizeof=0x30  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6f436874776f7247 (string "GrowthComponent")
  reflected: ti=0x1401c7778 create=0x1400348e0 createInPlace=0x140034950 ForesterDescription_CreateInPlace registered in 0x14014f090 ForesterDescription_Register
  [ 0] +0x00 0x140034a10  OWN FUN_140034a10 (83 B)
  [ 1] +0x08 0x1400348d0  TINY returns qword [0x1401c7780]
  [ 2] +0x10 0x1400348c0  TINY returns dword [0x1401c7778] = TypeId(ForesterDescription)
  [ 3] +0x18 0x14011fb00  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14011fa80  OWN FUN_14011fa80 (107 B)
  [ 6] +0x30 0x1400349b0  OWN FUN_1400349b0 (78 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140034a00  OWN (no Ghidra function)
  [ 9] +0x48 0x140034950  OWN ForesterDescription_CreateInPlace (83 B)
  stamped by (6): other+0x0 at 0x140034925 in 0x1400348e0 ForesterDescription_Factory; this+0x0 at 0x140034984 in 0x140034950 ForesterDescription_CreateInPlace; this+0x0 at 0x1400349e9 in 0x1400349b0 FUN_1400349b0; this+0x0 at 0x140034a34 in 0x140034a10 FUN_140034a10; this+0x0 at 0x14011fb29 in 0x14011fb10 FUN_14011fb10; this+0x0 at 0x14011fb7e in 0x14011fb50 FUN_14011fb50
### ForesterWork  table=0x1401613a0  slots=13  sizeof=0x60  parent="AutoPickupWork" / structural AutoPickupWork 0x140160fa0  stop=non-code qword 0x6152726568746147 (string "GatherRawMaterials")
  reflected: ti=0x1401c69e8 create=0x14002a310 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014de10 ForesterWork_RegisterType
  [ 0] +0x00 0x14002a370  OWN ForesterWork_ScalarDtor (48 B)
  [ 1] +0x08 0x14002a300  TINY returns qword [0x1401c69f0]
  [ 2] +0x10 0x14002a2f0  TINY returns dword [0x1401c69e8] = TypeId(ForesterWork)
  [ 3] +0x18 0x1400eb970  OWN ForesterWork_Shutdown (132 B)
  [ 4] +0x20 0x1400eaec0  OWN ForesterWork_Serialize (2512 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400eaea0  OWN ForesterWork_Draw (20 B)
  [ 7] +0x38 0x1400eae90  THUNK this+72 -> 0x140025520 CitizenAI_impl_5520
  [ 8] +0x40 0x1400eb8a0  OWN ForesterWork_OnRemove (104 B)
  [ 9] +0x48 0x1400eac60  OWN ForesterWork_Update (547 B)
  [10] +0x50 0x1400eb910  OWN ForesterWork_Slot10 (75 B)
  [11] +0x58 0x140029860  OWN ClearArea_Slot11 (9 B)  [in 13 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (5): other+0x0 at 0x14002a34a in 0x14002a310 ForesterWork_Factory; this+0x0 at 0x1400eba27 in 0x1400eba00 Work_Ctor; this+0x0 at 0x1400ebada in 0x1400ebaa0 ForesterWork_Ctor; this+0x0 at 0x1400ebb23 in 0x1400ebb00 ForesterWork_Ctor3; other+0x0 at 0x14011f849 in 0x14011f780 ForesterWork_CopyCtor
### FruitComponent  table=0x140159450  slots=16  sizeof=0x38  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c4250 create=0x140005920 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014ac40 FruitComponent_RegisterType
  [ 0] +0x00 0x140005b20  OWN FruitComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x140005910  TINY returns qword [0x1401c4258]
  [ 2] +0x10 0x140005900  TINY returns dword [0x1401c4250] = TypeId(FruitComponent)
  [ 3] +0x18 0x14004a070  OWN FruitComponent_Shutdown (156 B)
  [ 4] +0x20 0x14004a030  OWN FruitComponent_Serialize (55 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140049de0  OWN FruitComponent_Update (459 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140005b44 in 0x140005b20 FruitComponent_ScalarDtor; this+0x0 at 0x14004a139 in 0x14004a120 FruitComponent_Ctor; this+0x0 at 0x14004a188 in 0x14004a160 FruitComponent_Ctor
### FruitDescription  table=0x1401594d0  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6d6f437469757246 (string "FruitComponent")
  reflected: ti=0x1401c4288 create=0x140005980 createInPlace=0x1400059f0 FruitDescription_CreateInPlace registered in 0x14014ac80 FruitDescription_Register
  [ 0] +0x00 0x140005ac0  OWN FruitComponent_vslot16 (83 B)
  [ 1] +0x08 0x140005970  TINY returns qword [0x1401c4290]
  [ 2] +0x10 0x140005960  TINY returns dword [0x1401c4288] = TypeId(FruitDescription)
  [ 3] +0x18 0x14004a260  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14004a1e0  OWN FUN_14004a1e0 (121 B)
  [ 6] +0x30 0x140005a50  OWN FUN_140005a50 (86 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140005ab0  OWN (no Ghidra function)
  [ 9] +0x48 0x1400059f0  OWN FruitDescription_CreateInPlace (85 B)
  stamped by (6): other+0x0 at 0x1400059c5 in 0x140005980 FruitDescription_Factory; this+0x0 at 0x140005a24 in 0x1400059f0 FruitDescription_CreateInPlace; this+0x0 at 0x140005a89 in 0x140005a50 FUN_140005a50; this+0x0 at 0x140005ae4 in 0x140005ac0 FruitComponent_vslot16; this+0x0 at 0x14004a289 in 0x14004a270 FUN_14004a270; this+0x0 at 0x14004a2de in 0x14004a2b0 FUN_14004a2b0
### FrustumOutlineDescription  table=0x14015c300  slots=10  sizeof=0x80  parent="ElementDescription" / structural UI::ElementDescription (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c5168 create=0x1400122c0 createInPlace=0x140012330 FrustumOutlineDescription_CreateInPlace registered in 0x14014c200 FrustumOutlineDescription_RegisterType
  [ 0] +0x00 0x140012500  OWN FrustumOutlineElement_Slot12 (87 B)
  [ 1] +0x08 0x1400122b0  TINY returns qword [0x1401c5170]
  [ 2] +0x10 0x1400122a0  TINY returns dword [0x1401c5168] = TypeId(FrustumOutlineDescription)
  [ 3] +0x18 0x14006c500  OWN FrustumOutlineElement_Slot15 (19 B)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x14006c460  OWN FrustumOutlineDescription_Slot5 (140 B)
  [ 6] +0x30 0x140012390  OWN FrustumOutlineDescription_Draw (77 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14006c340  OWN FrustumOutlineDescription_OnRemove (279 B)
  [ 9] +0x48 0x140012330  OWN FrustumOutlineDescription_CreateInPlace (82 B)
  stamped by (6): other+0x0 at 0x1400122fb in 0x1400122c0 FrustumOutlineDescription_Factory; this+0x0 at 0x14001235a in 0x140012330 FrustumOutlineDescription_CreateInPlace; this+0x0 at 0x1400123bf in 0x140012390 FrustumOutlineDescription_Draw; this+0x0 at 0x140012524 in 0x140012500 FrustumOutlineElement_Slot12; this+0x0 at 0x14006c53d in 0x14006c520 FrustumOutlineDescription_Ctor; this+0x0 at 0x14006c584 in 0x14006c560 FrustumOutlineDescription_CopyCtor
### FrustumOutlineElement  table=0x14015c2a0  slots=12  sizeof=0xd8  parent="Element" / structural UI::Element (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c51a0 create=0x140012400 createInPlace=0x140012470 FrustumOutlineElement_Cleanup registered in 0x14014c240 FrustumOutlineElement_RegisterType
  [ 0] +0x00 0x140012560  OWN FrustumOutlineElement_ScalarDtor (73 B)
  [ 1] +0x08 0x1400123f0  TINY returns qword [0x1401c51a8]
  [ 2] +0x10 0x1400123e0  TINY returns dword [0x1401c51a0] = TypeId(FrustumOutlineElement)
  [ 3] +0x18 0x14003a9b4  IMPORT ?Persist@Element@UI@@UEAAXAEAVStream@IO@@@Z  [in 2 tables]
  [ 4] +0x20 0x14006c290  OWN FrustumOutlineElement_OnCreate (39 B)
  [ 5] +0x28 0x14006c1c0  OWN FrustumOutlineElement_Slot5 (192 B)
  [ 6] +0x30 0x14006be90  OWN FrustumOutlineElement_Draw (805 B)
  [ 7] +0x38 0x14003a9ba  IMPORT ?Update@Element@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@12@@Z  [in 2 tables]
  [ 8] +0x40 0x14003a9c0  IMPORT ?Enable@Element@UI@@UEAAX_N@Z  [in 2 tables]
  [ 9] +0x48 0x14003a9c6  IMPORT ?Contains@Element@UI@@UEBA_NHHPEAVUserInterface@2@@Z  [in 2 tables]
  [10] +0x50 0x14003a9d2  IMPORT ?SetColor@Element@UI@@UEAAXI@Z  [in 2 tables]
  [11] +0x58 0x14003a9cc  IMPORT ?GetBorder@Element@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@XZ  [in 2 tables]
  stamped by (6): other+0x0 at 0x14001243d in 0x140012400 FrustumOutlineElement_Factory; this+0x0 at 0x14001249a in 0x140012470 FrustumOutlineElement_Cleanup; this+0x0 at 0x140012584 in 0x140012560 FrustumOutlineElement_ScalarDtor; this+0x0 at 0x14006c2d9 in 0x14006c2c0 FrustumOutlineElement_Ctor; this+0x0 at 0x14006c314 in 0x14006c2f0 FrustumOutlineElement_CopyCtor; other+0x0 at 0x14006c38d in 0x14006c340 FrustumOutlineDescription_OnRemove
### GatherRawMaterials  table=0x140161420  slots=13  sizeof=0x78  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x4174736576726148 (string "HarvestArea")
  reflected: ti=0x1401c6a20 create=0x14002a3c0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014de50 GatherRawMaterials_RegisterType
  [ 0] +0x00 0x14002a410  OWN GatherRawMaterials_ScalarDtor (48 B)
  [ 1] +0x08 0x14002a3b0  TINY returns qword [0x1401c6a28]
  [ 2] +0x10 0x14002a3a0  TINY returns dword [0x1401c6a20] = TypeId(GatherRawMaterials)
  [ 3] +0x18 0x1400ec560  OWN GatherRawMaterials_Shutdown (230 B)
  [ 4] +0x20 0x1400ebe40  OWN GatherRawMaterials_Serialize (1594 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400ebdf0  OWN GatherRawMaterials_Draw (67 B)
  [ 7] +0x38 0x1400ebdb0  OWN GatherRawMaterials_OnCreate (50 B)
  [ 8] +0x40 0x1400ec480  OWN GatherRawMaterials_OnRemove (132 B)
  [ 9] +0x48 0x1400ebb50  OWN GatherRawMaterials_Update (597 B)
  [10] +0x50 0x1400ec510  OWN GatherRawMaterials_Slot10 (25 B)
  [11] +0x58 0x14002a400  OWN GatherRawMaterials_Slot11 (9 B)
  [12] +0x60 0x1400ec530  OWN GatherRawMaterials_Slot12 (37 B)
  stamped by (3): this+0x0 at 0x1400ec677 in 0x1400ec650 Work_Ctor15; this+0x0 at 0x1400ec74d in 0x1400ec710 GatherRawMaterials_Ctor; this+0x0 at 0x1400ec8f7 in 0x1400ec8a0 GatherRawMaterials_Ctor
### GoalDialog  table=0x14015beb0  slots=10  sizeof=0xc8  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x4955706c6548 (string "HelpUI")
  reflected: ti=0x1401c5018 create=0x140011b60 createInPlace=0x140011bc0 Cleanup_GoalDialog registered in 0x14014c080 RegisterType_GoalDialog
  [ 0] +0x00 0x140011c10  OWN GoalDialog_ScalarDtor (105 B)
  [ 1] +0x08 0x140011b50  TINY returns qword [0x1401c5020]
  [ 2] +0x10 0x140011b40  TINY returns dword [0x1401c5018] = TypeId(GoalDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x140069810  OWN GoalDialog_Serialize (1517 B)
  [ 5] +0x28 0x14003a94e  IMPORT ?OnClosed@DialogInstance@UI@@UEAAXXZ  [in 12 tables]
  [ 6] +0x30 0x140069640  OWN GoalDialog_Draw (310 B)
  [ 7] +0x38 0x140069800  TINY RET 0 (c2 00 00)
  [ 8] +0x40 0x1400697c0  OWN GoalDialog_OnRemove (56 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (7): other+0x0 at 0x140011b9d in 0x140011b60 Factory_GoalDialog; this+0x0 at 0x140011bea in 0x140011bc0 Cleanup_GoalDialog; this+0x0 at 0x140011c34 in 0x140011c10 GoalDialog_ScalarDtor; this+0x0 at 0x140069e2d in 0x140069e10 GoalDialog_CopyCtor; this+0x0 at 0x140069eb2 in 0x140069e70 GoalDialog_Ctor3; this+0x0 at 0x140069f04 in 0x140069ee0 GoalDialog_Ctor4; other+0x0 at 0x1400b331c in 0x1400b3220 GoalDialog_Ctor
### GrowthComponent  table=0x140163590  slots=16  sizeof=0x50  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c77b0 create=0x140034b10 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f0d0 GrowthComponent_RegisterType
  [ 0] +0x00 0x140034c50  OWN GrowthComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x140034b00  TINY returns qword [0x1401c77b8]
  [ 2] +0x10 0x140034af0  TINY returns dword [0x1401c77b0] = TypeId(GrowthComponent)
  [ 3] +0x18 0x1401207c0  OWN GrowthComponent_Shutdown (219 B)
  [ 4] +0x20 0x1401206f0  OWN GrowthComponent_Serialize (197 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140120690  TINY RET 0 (c2 00 00)
  [ 7] +0x38 0x140120560  OWN GrowthComponent_OnCreate (283 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140120050  OWN GrowthComponent_Update (942 B)
  [10] +0x50 0x14011fea0  OWN GrowthComponent_Slot10 (411 B)
  [11] +0x58 0x1401206a0  OWN GrowthComponent_Slot11 (64 B)
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140034c74 in 0x140034c50 GrowthComponent_ScalarDtor; this+0x0 at 0x1401208c9 in 0x1401208b0 GrowthComponent_Ctor; this+0x0 at 0x140120918 in 0x1401208f0 GrowthComponent_Ctor
### GrowthDescription  table=0x140163610  slots=10  sizeof=0x68  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x706d6f4364726548 (string "HerdComponent")
  reflected: ti=0x1401c77e8 create=0x140034b70 createInPlace=0x140034bb0 GrowthDescription_CreateInPlace registered in 0x14014f110 GrowthDescription_Register
  [ 0] +0x00 0x140034bf0  OWN GrowthComponent_ControllerDtor (83 B)
  [ 1] +0x08 0x140034b60  TINY returns qword [0x1401c77f0]
  [ 2] +0x10 0x140034b50  TINY returns dword [0x1401c77e8] = TypeId(GrowthDescription)
  [ 3] +0x18 0x140120b00  TINY return 3 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140120980  OWN FUN_140120980 (377 B)
  [ 6] +0x30 0x140034bc0  OWN FUN_140034bc0 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140034be0  OWN (no Ghidra function)
  [ 9] +0x48 0x140034bb0  OWN GrowthDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x140034c14 in 0x140034bf0 GrowthComponent_ControllerDtor; this+0x0 at 0x140120b29 in 0x140120b10 FUN_140120b10; this+0x0 at 0x140120b7e in 0x140120b50 GrowthDescription_sub_0b50
### HappinessComponent  table=0x1401595d0  slots=18  sizeof=0x50  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c42c0 create=0x140005ba0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014acc0 HappinessComponent_RegisterType
  member vptrs: +0x30=0x140159580, +0x30=0x14015f070
  [ 0] +0x00 0x140005db0  OWN HappinessComponent_Controller_Slot10 (115 B)
  [ 1] +0x08 0x140005b90  TINY returns qword [0x1401c42c8]
  [ 2] +0x10 0x140005b80  TINY returns dword [0x1401c42c0] = TypeId(HappinessComponent)
  [ 3] +0x18 0x14004aa80  OWN HappinessComponent_Controller_Slot13 (118 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x14004aa20  OWN HappinessComponent_OnCreate (78 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x14004a780  OWN HappinessComponent_Slot15 (437 B)
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140005be0  TINY returns this+0x30
  stamped by (3): this+0x0 at 0x140005dd4 in 0x140005db0 HappinessComponent_Controller_Slot10; this+0x0 at 0x14004ab19 in 0x14004ab00 HappinessComponent_Ctor; this+0x0 at 0x14004ab92 in 0x14004ab60 HappinessComponent_Ctor
### HappinessDescription  table=0x140159660  slots=10  sizeof=0x30  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6f4372656c616548 (string "HealerComponent")
  reflected: ti=0x1401c42f8 create=0x140005c10 createInPlace=0x140005c80 HappinessDescription_CreateInPlace registered in 0x14014ad00 HappinessDescription_Register
  [ 0] +0x00 0x140005d50  OWN HappinessComponent_vslot18 (83 B)
  [ 1] +0x08 0x140005c00  TINY returns qword [0x1401c4300]
  [ 2] +0x10 0x140005bf0  TINY returns dword [0x1401c42f8] = TypeId(HappinessDescription)
  [ 3] +0x18 0x14004ad50  TINY return 3 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14004acb0  OWN FUN_14004acb0 (149 B)
  [ 6] +0x30 0x140005ce0  OWN FUN_140005ce0 (88 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140005d40  OWN (no Ghidra function)
  [ 9] +0x48 0x140005c80  OWN HappinessDescription_CreateInPlace (89 B)
  stamped by (6): other+0x0 at 0x140005c55 in 0x140005c10 HappinessDescription_Factory; this+0x0 at 0x140005cb4 in 0x140005c80 HappinessDescription_CreateInPlace; this+0x0 at 0x140005d19 in 0x140005ce0 FUN_140005ce0; this+0x0 at 0x140005d74 in 0x140005d50 HappinessComponent_vslot18; this+0x0 at 0x14004ad79 in 0x14004ad60 FUN_14004ad60; this+0x0 at 0x14004adce in 0x14004ada0 FUN_14004ada0
### HarvestArea  table=0x1401614a0  slots=13  sizeof=0x68  parent="AutoPickupWork" / structural AutoPickupWork 0x140160fa0  stop=non-code qword 0x4f74736576726148 (string "HarvestOrchard")
  reflected: ti=0x1401c6a58 create=0x14002a460 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014de90 HarvestArea_RegisterType
  [ 0] +0x00 0x14002a4d0  OWN HarvestArea_ScalarDtor (48 B)
  [ 1] +0x08 0x14002a450  TINY returns qword [0x1401c6a60]
  [ 2] +0x10 0x14002a440  TINY returns dword [0x1401c6a58] = TypeId(HarvestArea)
  [ 3] +0x18 0x1400ed350  OWN HarvestArea_Shutdown (186 B)
  [ 4] +0x20 0x1400eccd0  OWN HarvestArea_Serialize (1421 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400ecc20  OWN HarvestArea_Draw (166 B)
  [ 7] +0x38 0x1400ecb60  OWN HarvestArea_OnCreate (172 B)
  [ 8] +0x40 0x1400ed270  OWN HarvestArea_OnRemove (108 B)
  [ 9] +0x48 0x1400ec930  OWN HarvestArea_Update (553 B)
  [10] +0x50 0x1400ed2f0  OWN HarvestArea_Slot10 (75 B)
  [11] +0x58 0x140029860  OWN ClearArea_Slot11 (9 B)  [in 13 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (4): other+0x0 at 0x14002a49a in 0x14002a460 HarvestArea_Factory; this+0x0 at 0x1400ed434 in 0x1400ed410 Work_Ctor7; this+0x0 at 0x1400ed502 in 0x1400ed4c0 HarvestArea_Ctor; this+0x0 at 0x1400ed5b3 in 0x1400ed590 HarvestArea_CopyCtor
### HarvestOrchard  table=0x140161520  slots=13  sizeof=0x68  parent="AutoPickupWork" / structural AutoPickupWork 0x140160fa0  stop=non-code qword 0x73696c6162726548 (string "HerbalistWork")
  reflected: ti=0x1401c6a90 create=0x14002a520 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014ded0 HarvestOrchard_RegisterType
  [ 0] +0x00 0x14002a590  OWN HarvestOrchard_ScalarDtor (48 B)
  [ 1] +0x08 0x14002a510  TINY returns qword [0x1401c6a98]
  [ 2] +0x10 0x14002a500  TINY returns dword [0x1401c6a90] = TypeId(HarvestOrchard)
  [ 3] +0x18 0x1400edea0  OWN HarvestOrchard_Shutdown (186 B)
  [ 4] +0x20 0x1400ed8c0  OWN HarvestOrchard_Serialize (1262 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400ed880  OWN HarvestOrchard_Draw (53 B)
  [ 7] +0x38 0x1400ed810  OWN HarvestOrchard_OnCreate (100 B)
  [ 8] +0x40 0x1400eddc0  OWN HarvestOrchard_OnRemove (108 B)
  [ 9] +0x48 0x1400ed5e0  OWN HarvestOrchard_Update (553 B)
  [10] +0x50 0x1400ede40  OWN HarvestOrchard_Slot10 (75 B)
  [11] +0x58 0x140029860  OWN ClearArea_Slot11 (9 B)  [in 13 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (4): other+0x0 at 0x14002a55a in 0x14002a520 HarvestOrchard_Factory; this+0x0 at 0x1400edf84 in 0x1400edf60 Work_Ctor6; this+0x0 at 0x1400ee033 in 0x1400ee010 HarvestOrchard_CopyCtor; this+0x0 at 0x1400ee0a0 in 0x1400ee060 HarvestOrchard_Ctor
### HealerComponent  table=0x140159730  slots=18  sizeof=0x48  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c4330 create=0x140006490 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014adc0 HealerComponent_RegisterType
  member vptrs: +0x28=0x1401596e0, +0x28=0x14015f070
  [ 0] +0x00 0x140006660  OWN HealerComponent_Controller_Slot10 (115 B)
  [ 1] +0x08 0x140006480  TINY returns qword [0x1401c4338]
  [ 2] +0x10 0x140006470  TINY returns dword [0x1401c4330] = TypeId(HealerComponent)
  [ 3] +0x18 0x14004aeb0  OWN HealerComponent_Controller_Slot13 (76 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x14004ae70  OWN HealerComponent_OnCreate (51 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (3): this+0x0 at 0x140006684 in 0x140006660 HealerComponent_Controller_Slot10; this+0x0 at 0x14004af29 in 0x14004af10 HealerComponent_Ctor; this+0x0 at 0x14004afa2 in 0x14004af70 HealerComponent_Ctor
### HealerDescription  table=0x1401597c0  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x656372756f736552 (string "ResourceProduceComponent")
  reflected: ti=0x1401c4368 create=0x1400064f0 createInPlace=0x140006550 HealerDescription_CreateInPlace registered in 0x14014ae00 HealerDescription_Register
  [ 0] +0x00 0x140006600  OWN HealerComponent_vslot18 (83 B)
  [ 1] +0x08 0x1400064e0  TINY returns qword [0x1401c4370]
  [ 2] +0x10 0x1400064d0  TINY returns dword [0x1401c4368] = TypeId(HealerDescription)
  [ 3] +0x18 0x14004b0d0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14004b090  OWN FUN_14004b090 (46 B)
  [ 6] +0x30 0x1400065a0  OWN FUN_1400065a0 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400065f0  OWN (no Ghidra function)
  [ 9] +0x48 0x140006550  OWN HealerDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x140006535 in 0x1400064f0 HealerDescription_Factory; this+0x0 at 0x140006584 in 0x140006550 HealerDescription_CreateInPlace; this+0x0 at 0x1400065d9 in 0x1400065a0 FUN_1400065a0; this+0x0 at 0x140006624 in 0x140006600 HealerComponent_vslot18; this+0x0 at 0x14004b0f9 in 0x14004b0e0 FUN_14004b0e0; this+0x0 at 0x14004b14e in 0x14004b120 FUN_14004b120
### HelpUI  table=0x14015bf30  slots=11  sizeof=0x78  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x6f5479654b746f48 (string "HotKeyToolTipUI")
  reflected: ti=0x1401c5050 create=0x140011ca0 createInPlace=0x140011ce0 <no fn> registered in 0x14014c0c0 RegisterType_HelpUI
  [ 0] +0x00 0x140011cf0  OWN HelpUI_ScalarDtor (96 B)
  [ 1] +0x08 0x140011c90  TINY returns qword [0x1401c5058]
  [ 2] +0x10 0x140011c80  TINY returns dword [0x1401c5050] = TypeId(HelpUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x14006a500  OWN HelpUI_Serialize (968 B)
  [ 5] +0x28 0x14006a4f0  TINY RET 0 (c2 00 00)
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14006a230  OWN HelpUI_OnCreate (82 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14006a290  OWN HelpUI_Update (590 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (3): this+0x0 at 0x140011d14 in 0x140011cf0 HelpUI_ScalarDtor; this+0x0 at 0x14006a8ed in 0x14006a8d0 HelpUI_Ctor; this+0x0 at 0x14006a944 in 0x14006a920 HelpUI_Ctor
### HerbalistComponent  table=0x140159860  slots=22  sizeof=0x50  parent="ResourceProduceComponent" / structural ResourceProduceComponent 0x1401643f0  stop=next table
  reflected: ti=0x1401c43a0 create=0x140006b40 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014aec0 HerbalistComponent_RegisterType
  [ 0] +0x00 0x140006d60  OWN HerbalistComponent_ScalarDtor (125 B)
  [ 1] +0x08 0x140006b30  TINY returns qword [0x1401c43a8]
  [ 2] +0x10 0x140006b20  TINY returns dword [0x1401c43a0] = TypeId(HerbalistComponent)
  [ 3] +0x18 0x14004b200  OWN HerbalistComponent_Shutdown (48 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x14012b520  OWN HerbalistComponent_Slot13 (112 B)  [in 6 tables]   = parent
  [14] +0x70 0x14012b5a0  OWN HerbalistComponent_Slot14 (117 B)  [in 6 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]   = parent
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]   = parent
  [18] +0x90 0x14004b160  OWN HerbalistWork_Ctor (142 B)
  [19] +0x98 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  [20] +0xa0 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  [21] +0xa8 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (4): other+0x0 at 0x140006b7a in 0x140006b40 HerbalistComponent_Factory; this+0x0 at 0x140006d84 in 0x140006d60 HerbalistComponent_ScalarDtor; this+0x0 at 0x14004b259 in 0x14004b240 ResourceProduceComponent_Ctor; this+0x0 at 0x14004b2c3 in 0x14004b2a0 HerbalistComponent_Ctor
### HerbalistDescription  table=0x140159910  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6867696c68676948 (string "HighlightComponent")
  reflected: ti=0x1401c43d8 create=0x140006bb0 createInPlace=0x140006c20 HerbalistDescription_CreateInPlace registered in 0x14014af00 HerbalistDescription_Register
  [ 0] +0x00 0x140006cf0  OWN FUN_140006cf0 (106 B)
  [ 1] +0x08 0x140006ba0  TINY returns qword [0x1401c43e0]
  [ 2] +0x10 0x140006b90  TINY returns dword [0x1401c43d8] = TypeId(HerbalistDescription)
  [ 3] +0x18 0x14004b380  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14004b2e0  OWN FUN_14004b2e0 (152 B)
  [ 6] +0x30 0x140006c80  OWN FUN_140006c80 (86 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140006ce0  OWN (no Ghidra function)
  [ 9] +0x48 0x140006c20  OWN HerbalistDescription_CreateInPlace (85 B)
  stamped by (6): other+0x0 at 0x140006bf5 in 0x140006bb0 HerbalistDescription_Factory; this+0x0 at 0x140006c54 in 0x140006c20 HerbalistDescription_CreateInPlace; this+0x0 at 0x140006cb9 in 0x140006c80 FUN_140006c80; this+0x0 at 0x140006d14 in 0x140006cf0 FUN_140006cf0; this+0x0 at 0x14004b3ad in 0x14004b390 FUN_14004b390; this+0x0 at 0x14004b41e in 0x14004b3f0 FUN_14004b3f0
### HerbalistWork  table=0x1401615a0  slots=13  sizeof=0x60  parent="AutoPickupWork" / structural AutoPickupWork 0x140160fa0  stop=non-code qword 0x6b726f57746e7548 (string "HuntWork")
  reflected: ti=0x1401c6ac8 create=0x14002a5e0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014df10 HerbalistWork_RegisterType
  [ 0] +0x00 0x14002a640  OWN HerbalistWork_ScalarDtor (48 B)
  [ 1] +0x08 0x14002a5d0  TINY returns qword [0x1401c6ad0]
  [ 2] +0x10 0x14002a5c0  TINY returns dword [0x1401c6ac8] = TypeId(HerbalistWork)
  [ 3] +0x18 0x1400eebb0  OWN HerbalistWork_Shutdown (132 B)
  [ 4] +0x20 0x1400ee3b0  OWN HerbalistWork_Serialize (1824 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400ee390  OWN HerbalistWork_Draw (20 B)
  [ 7] +0x38 0x1400ee380  THUNK this+72 -> 0x140025520 CitizenAI_impl_5520
  [ 8] +0x40 0x1400eeae0  OWN HerbalistWork_OnRemove (104 B)
  [ 9] +0x48 0x1400ee150  OWN HerbalistWork_Update (547 B)
  [10] +0x50 0x1400eeb50  OWN HerbalistWork_Slot10 (75 B)
  [11] +0x58 0x140029860  OWN ClearArea_Slot11 (9 B)  [in 13 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (5): other+0x0 at 0x14002a61a in 0x14002a5e0 HerbalistWork_Factory; other+0x0 at 0x14004b1ba in 0x14004b160 HerbalistWork_Ctor; this+0x0 at 0x1400eec67 in 0x1400eec40 Work_Ctor4; this+0x0 at 0x1400eed03 in 0x1400eece0 HerbalistWork_Ctor3; this+0x0 at 0x1400eed6a in 0x1400eed30 HerbalistWork_CopyCtor
### HerdComponent  table=0x140163680  slots=16  sizeof=0x60  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7820 create=0x140034cd0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f150 HerdComponent_RegisterType
  [ 0] +0x00 0x140034ea0  OWN HerdComponent_ScalarDtor (106 B)
  [ 1] +0x08 0x140034cc0  TINY returns qword [0x1401c7828]
  [ 2] +0x10 0x140034cb0  TINY returns dword [0x1401c7820] = TypeId(HerdComponent)
  [ 3] +0x18 0x140121f90  OWN HerdComponent_Serialize (339 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140121f40  OWN HerdComponent_OnAdd (59 B)
  [ 7] +0x38 0x140121e20  OWN HerdComponent_OnRemove (271 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140121bf0  OWN HerdComponent_Update (543 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140034ec4 in 0x140034ea0 HerdComponent_ScalarDtor; this+0x0 at 0x14012210d in 0x1401220f0 HerdComponent_Ctor; this+0x0 at 0x140122178 in 0x140122150 HerdComponent_Ctor
### HerdDescription  table=0x140163700  slots=10  sizeof=0x40  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x63736544746e7548 (string "HuntDescription")
  reflected: ti=0x1401c7858 create=0x140034d30 createInPlace=0x140034d90 HerdDescription_CreateInPlace registered in 0x14014f190 HerdDescription_Register
  [ 0] +0x00 0x140034e40  OWN HerdComponent_GetTypeId (83 B)
  [ 1] +0x08 0x140034d20  TINY returns qword [0x1401c7860]
  [ 2] +0x10 0x140034d10  TINY returns dword [0x1401c7858] = TypeId(HerdDescription)
  [ 3] +0x18 0x1401222a0  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1401221e0  OWN HerdComponent_OnCreate (175 B)
  [ 6] +0x30 0x140034de0  OWN HerdComponent_OnDestroy (77 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140034e30  OWN HerdComponent_OnCleanup (12 B)
  [ 9] +0x48 0x140034d90  OWN HerdDescription_CreateInPlace (76 B)
  stamped by (6): other+0x0 at 0x140034d75 in 0x140034d30 HerdDescription_Factory; this+0x0 at 0x140034dc4 in 0x140034d90 HerdDescription_CreateInPlace; this+0x0 at 0x140034e19 in 0x140034de0 HerdComponent_OnDestroy; this+0x0 at 0x140034e64 in 0x140034e40 HerdComponent_GetTypeId; this+0x0 at 0x1401222c9 in 0x1401222b0 FUN_1401222b0; this+0x0 at 0x14012231e in 0x1401222f0 FUN_1401222f0
### HighlightComponent  table=0x140159990  slots=16  sizeof=0x128  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c4410 create=0x140006e40 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014af40 HighlightComponent_RegisterType
  [ 0] +0x00 0x140007040  OWN HighlightComponent_ScalarDtor (48 B)
  [ 1] +0x08 0x140006e30  TINY returns qword [0x1401c4418]
  [ 2] +0x10 0x140006e20  TINY returns dword [0x1401c4410] = TypeId(HighlightComponent)
  [ 3] +0x18 0x14004c880  OWN HighlightComponent_Shutdown (48 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x14004c850  OWN HighlightComponent_OnRemove (23 B)
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x14004c870  OWN HighlightComponent_Slot12 (10 B)
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (2): this+0x0 at 0x14004c8dd in 0x14004c8c0 HighlightComponent_Ctor; this+0x0 at 0x14004c958 in 0x14004c930 HighlightComponent_Ctor
### HighlightDescription  table=0x140159a10  slots=10  sizeof=0x40  parent="Description" / structural Description 0x1401629a0  stop=next table
  reflected: ti=0x1401c4448 create=0x140006ea0 createInPlace=0x140006f10 HighlightDescription_CreateInPlace registered in 0x14014af80 HighlightDescription_Register
  [ 0] +0x00 0x140006fe0  OWN HighlightComponent_vslot16 (96 B)
  [ 1] +0x08 0x140006e90  TINY returns qword [0x1401c4450]
  [ 2] +0x10 0x140006e80  TINY returns dword [0x1401c4448] = TypeId(HighlightDescription)
  [ 3] +0x18 0x14004cad0  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14004c9d0  OWN FUN_14004c9d0 (245 B)
  [ 6] +0x30 0x140006f70  OWN FUN_140006f70 (87 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140006fd0  OWN (no Ghidra function)
  [ 9] +0x48 0x140006f10  OWN HighlightDescription_CreateInPlace (86 B)
  stamped by (6): other+0x0 at 0x140006ee5 in 0x140006ea0 HighlightDescription_Factory; this+0x0 at 0x140006f44 in 0x140006f10 HighlightDescription_CreateInPlace; this+0x0 at 0x140006fa9 in 0x140006f70 FUN_140006f70; this+0x0 at 0x140007004 in 0x140006fe0 HighlightComponent_vslot16; this+0x0 at 0x14004cafd in 0x14004cae0 FUN_14004cae0; this+0x0 at 0x14004cb5e in 0x14004cb30 FUN_14004cb30
### HotKeyToolTipUI  table=0x14015bfc0  slots=11  sizeof=0x30  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x656c6f6877 (string "whole")
  reflected: ti=0x1401c5088 create=0x140011e20 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014c100 RegisterType_HotKeyToolTipUI
  [ 0] +0x00 0x140011e70  OWN HotKeyToolTipUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140011e10  TINY returns qword [0x1401c5090]
  [ 2] +0x10 0x140011e00  TINY returns dword [0x1401c5088] = TypeId(HotKeyToolTipUI)
  [ 3] +0x18 0x14006ab80  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x14006a980  OWN HotKeyToolTipUI_Serialize (495 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14003a97e  IMPORT ?OnUpdateLayout@DialogController@UI@@UEAAXXZ  [in 10 tables]
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14003a972  IMPORT ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z  [in 11 tables]
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x140011e5b in 0x140011e20 Factory_HotKeyToolTipUI; this+0x0 at 0x140011e94 in 0x140011e70 HotKeyToolTipUI_ScalarDtor; this+0x0 at 0x14006aba9 in 0x14006ab90 HotKeyToolTipUI_Ctor; this+0x0 at 0x14006abe4 in 0x14006abc0 HotKeyToolTipUI_CopyCtor
### HuntAnimal  table=0x14015fde0  slots=10  sizeof=0x40  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x6f5465766f4d (string "MoveTo")
  reflected: ti=0x1401c5f68 create=0x140022700 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d210 HuntAnimal_RegisterType
  [ 0] +0x00 0x140022770  OWN HuntAnimal_ScalarDtor (104 B)
  [ 1] +0x08 0x1400226f0  TINY returns qword [0x1401c5f70]
  [ 2] +0x10 0x1400226e0  TINY returns dword [0x1401c5f68] = TypeId(HuntAnimal)
  [ 3] +0x18 0x1400c8260  OWN HuntAnimal_Shutdown (226 B)
  [ 4] +0x20 0x1400c8250  OWN HuntAnimal_Serialize (9 B)
  [ 5] +0x28 0x1400c8240  OWN HuntAnimal_Slot5 (9 B)
  [ 6] +0x30 0x1400c7de0  OWN HuntAnimal_Draw (1109 B)
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400c4a30  TINY RET 0 (c2 00 00)  [in 7 tables]   = parent
  [ 9] +0x48 0x1400c7d60  OWN HuntAnimal_Update (121 B)
  stamped by (5): other+0x0 at 0x14002274e in 0x140022700 HuntAnimal_Factory; this+0x0 at 0x140022794 in 0x140022770 HuntAnimal_ScalarDtor; this+0x0 at 0x1400c836d in 0x1400c8350 Action_Action_Ctor19; this+0x0 at 0x1400c83d7 in 0x1400c83a0 HuntAnimal_Action_Ctor; this+0x0 at 0x1400c8447 in 0x1400c8400 HuntAnimal_Action_CopyCtor
### HuntComponent  table=0x140163770  slots=22  sizeof=0x58  parent="ResourceProduceComponent" / structural ResourceProduceComponent 0x1401643f0  stop=next table
  reflected: ti=0x1401c7890 create=0x140034f30 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f1d0 HuntComponent_RegisterType
  [ 0] +0x00 0x140035100  OWN HuntComponent_ScalarDtor (125 B)
  [ 1] +0x08 0x140034f20  TINY returns qword [0x1401c7898]
  [ 2] +0x10 0x140034f10  TINY returns dword [0x1401c7890] = TypeId(HuntComponent)
  [ 3] +0x18 0x140122820  OWN HuntComponent_Shutdown (87 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140122740  OWN HuntComponent_Update (203 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x14012b520  OWN HerbalistComponent_Slot13 (112 B)  [in 6 tables]   = parent
  [14] +0x70 0x14012b5a0  OWN HerbalistComponent_Slot14 (117 B)  [in 6 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]   = parent
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]   = parent
  [18] +0x90 0x1401226a0  OWN HuntComponent_CreateWork (143 B)
  [19] +0x98 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  [20] +0xa0 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  [21] +0xa8 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (3): this+0x0 at 0x140035124 in 0x140035100 HuntComponent_ScalarDtor; this+0x0 at 0x140122899 in 0x140122880 ResourceProduceComponent_Ctor6; this+0x0 at 0x140122908 in 0x1401228e0 HuntComponent_Ctor
### HuntDescription  table=0x140163820  slots=10  sizeof=0x28  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x7463617265746e49 (string "InteractComponent")
  reflected: ti=0x1401c78c8 create=0x140034f90 createInPlace=0x140034ff0 HuntDescription_CreateInPlace registered in 0x14014f210 HuntDescription_Register
  [ 0] +0x00 0x1400350a0  OWN FUN_1400350a0 (83 B)
  [ 1] +0x08 0x140034f80  TINY returns qword [0x1401c78d0]
  [ 2] +0x10 0x140034f70  TINY returns dword [0x1401c78c8] = TypeId(HuntDescription)
  [ 3] +0x18 0x1401229e0  TINY return 3 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140122960  OWN FUN_140122960 (111 B)
  [ 6] +0x30 0x140035040  OWN FUN_140035040 (76 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140035090  OWN (no Ghidra function)
  [ 9] +0x48 0x140034ff0  OWN HuntDescription_CreateInPlace (75 B)
  stamped by (6): other+0x0 at 0x140034fd5 in 0x140034f90 HuntDescription_Factory; this+0x0 at 0x140035024 in 0x140034ff0 HuntDescription_CreateInPlace; this+0x0 at 0x140035079 in 0x140035040 FUN_140035040; this+0x0 at 0x1400350c4 in 0x1400350a0 FUN_1400350a0; this+0x0 at 0x140122a09 in 0x1401229f0 FUN_1401229f0; this+0x0 at 0x140122a5e in 0x140122a30 FUN_140122a30
### HuntWork  table=0x140161620  slots=13  sizeof=0x58  parent="AutoPickupWork" / structural AutoPickupWork 0x140160fa0  stop=non-code qword 0x6e6f697461636f4c (string "LocationWork")
  reflected: ti=0x1401c6b00 create=0x14002a690 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014df50 HuntWork_RegisterType
  [ 0] +0x00 0x14002a700  OWN HuntWork_ScalarDtor (48 B)
  [ 1] +0x08 0x14002a680  TINY returns qword [0x1401c6b08]
  [ 2] +0x10 0x14002a670  TINY returns dword [0x1401c6b00] = TypeId(HuntWork)
  [ 3] +0x18 0x1400ef600  OWN HuntWork_Shutdown (114 B)
  [ 4] +0x20 0x1400eeff0  OWN HuntWork_Serialize (1333 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400eefd0  OWN HuntWork_Draw (20 B)
  [ 7] +0x38 0x1400eefc0  THUNK this+72 -> 0x140025520 CitizenAI_impl_5520
  [ 8] +0x40 0x1400ef530  OWN HuntWork_OnRemove (104 B)
  [ 9] +0x48 0x1400eed90  OWN HuntWork_Update (547 B)
  [10] +0x50 0x1400ef5a0  OWN HuntWork_Slot10 (75 B)
  [11] +0x58 0x140029860  OWN ClearArea_Slot11 (9 B)  [in 13 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (5): other+0x0 at 0x14002a6ca in 0x14002a690 HuntWork_Factory; this+0x0 at 0x1400ef6a7 in 0x1400ef680 Work_Ctor3; this+0x0 at 0x1400ef75a in 0x1400ef720 HuntWork_Ctor; this+0x0 at 0x1400ef7a3 in 0x1400ef780 HuntWork_CopyCtor; other+0x0 at 0x1401226fa in 0x1401226a0 HuntComponent_CreateWork
### ImmigrationComponent  table=0x140159b30  slots=18  sizeof=0x68  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c4480 create=0x140007a20 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014afc0 ImmigrationComponent_RegisterType
  member vptrs: +0x28=0x140159ae0, +0x28=0x14015f070
  [ 0] +0x00 0x140007b70  OWN ImmigrationComponent_Controller_Slot10 (48 B)
  [ 1] +0x08 0x140007a10  TINY returns qword [0x1401c4488]
  [ 2] +0x10 0x140007a00  TINY returns dword [0x1401c4480] = TypeId(ImmigrationComponent)
  [ 3] +0x18 0x14004e4b0  OWN ImmigrationComponent_Controller_Slot13 (274 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (2): this+0x0 at 0x14004e5ed in 0x14004e5d0 ImmigrationComponent_Ctor; this+0x0 at 0x14004e66d in 0x14004e640 ImmigrationComponent_Ctor
### ImmigrationDescription  table=0x140159bc0  slots=10  sizeof=0x68  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6f4374656b72614d (string "MarketComponent")
  reflected: ti=0x1401c44b8 create=0x140007a80 createInPlace=0x140007ac0 ImmigrationDescription_CreateInPlace registered in 0x14014b000 ImmigrationDescription_Register
  [ 0] +0x00 0x140007b00  OWN ImmigrationComponent_vslot18 (106 B)
  [ 1] +0x08 0x140007a70  TINY returns qword [0x1401c44c0]
  [ 2] +0x10 0x140007a60  TINY returns dword [0x1401c44b8] = TypeId(ImmigrationDescription)
  [ 3] +0x18 0x14004e930  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14004e7f0  OWN FUN_14004e7f0 (307 B)
  [ 6] +0x30 0x140007ad0  OWN FUN_140007ad0 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140007af0  OWN (no Ghidra function)
  [ 9] +0x48 0x140007ac0  OWN ImmigrationDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x140007b24 in 0x140007b00 ImmigrationComponent_vslot18; this+0x0 at 0x14004e95d in 0x14004e940 FUN_14004e940; this+0x0 at 0x14004e9ce in 0x14004e9a0 ImmigrationDescription_sub_e9a0
### ImmigrationUI  table=0x14015c080  slots=11  sizeof=0x50  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x6d6172615070614d (string "MapParamsDialog")
  reflected: ti=0x1401c50c0 create=0x140011ee0 createInPlace=0x140011f40 Cleanup_ImmigrationUI registered in 0x14014c140 RegisterType_ImmigrationUI
  [ 0] +0x00 0x140011fd0  OWN ImmigrationUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140011ed0  TINY returns qword [0x1401c50c8]
  [ 2] +0x10 0x140011ec0  TINY returns dword [0x1401c50c0] = TypeId(ImmigrationUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x14006b330  OWN ImmigrationUI_Serialize (565 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14006b130  OWN ImmigrationUI_OnCreate (455 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14006b300  OWN ImmigrationUI_Slot9 (34 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (5): other+0x0 at 0x140011f1b in 0x140011ee0 Factory_ImmigrationUI; this+0x0 at 0x140011f6a in 0x140011f40 Cleanup_ImmigrationUI; this+0x0 at 0x140011ff4 in 0x140011fd0 ImmigrationUI_ScalarDtor; this+0x0 at 0x14006b589 in 0x14006b570 ImmigrationUI_Ctor; this+0x0 at 0x14006b5c4 in 0x14006b5a0 ImmigrationUI_CopyCtor
### InteractComponent  table=0x1401638b0  slots=16  sizeof=0x38  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7900 create=0x1400351a0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f250 InteractComponent_RegisterType
  [ 0] +0x00 0x1400352f0  OWN InteractComponent_ScalarDtor (106 B)
  [ 1] +0x08 0x140035190  TINY returns qword [0x1401c7908]
  [ 2] +0x10 0x140035180  TINY returns dword [0x1401c7900] = TypeId(InteractComponent)
  [ 3] +0x18 0x140123340  OWN InteractComponent_Shutdown (205 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140123110  OWN InteractComponent_Draw (545 B)
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140035314 in 0x1400352f0 InteractComponent_ScalarDtor; this+0x0 at 0x14012343d in 0x140123420 InteractComponent_Ctor; this+0x0 at 0x1401234a8 in 0x140123480 InteractComponent_Ctor
### InteractDescription  table=0x140163930  slots=10  sizeof=0x58  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x636f74736576694c (string "LivestockComponent")
  reflected: ti=0x1401c7938 create=0x140035200 createInPlace=0x140035240 InteractDescription_CreateInPlace registered in 0x14014f290 InteractDescription_Register
  [ 0] +0x00 0x140035280  OWN InteractComponent_vslot16 (112 B)
  [ 1] +0x08 0x1400351f0  TINY returns qword [0x1401c7940]
  [ 2] +0x10 0x1400351e0  TINY returns dword [0x1401c7938] = TypeId(InteractDescription)
  [ 3] +0x18 0x1401236a0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140123500  OWN FUN_140123500 (399 B)
  [ 6] +0x30 0x140035250  OWN FUN_140035250 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140035270  OWN (no Ghidra function)
  [ 9] +0x48 0x140035240  OWN InteractDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x1400352a4 in 0x140035280 InteractComponent_vslot16; this+0x0 at 0x1401236cd in 0x1401236b0 FUN_1401236b0; this+0x0 at 0x14012373e in 0x140123710 InteractDescription_sub_3710
### LivestockComponent  table=0x1401639b0  slots=16  sizeof=0x38  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7970 create=0x140035480 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f2d0 LivestockComponent_RegisterType
  [ 0] +0x00 0x140035680  OWN LivestockComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x140035470  TINY returns qword [0x1401c7978]
  [ 2] +0x10 0x140035460  TINY returns dword [0x1401c7970] = TypeId(LivestockComponent)
  [ 3] +0x18 0x140123e10  OWN LivestockComponent_Serialize (135 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140123c70  OWN LivestockComponent_OnAdd (63 B)
  [ 7] +0x38 0x140123c00  OWN LivestockComponent_OnRemove (103 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140123b50  OWN LivestockComponent_Update (164 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x1400356a4 in 0x140035680 LivestockComponent_ScalarDtor; this+0x0 at 0x140123eb9 in 0x140123ea0 LivestockComponent_Ctor; this+0x0 at 0x140123f08 in 0x140123ee0 LivestockComponent_Ctor
### LivestockDescription  table=0x140163a30  slots=10  sizeof=0x30  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6f706d6f4370614d (string "MapComponent")
  reflected: ti=0x1401c79a8 create=0x1400354e0 createInPlace=0x140035550 LivestockDescription_CreateInPlace registered in 0x14014f310 LivestockDescription_Register
  [ 0] +0x00 0x140035620  OWN LivestockComponent_GetTypeId (83 B)
  [ 1] +0x08 0x1400354d0  TINY returns qword [0x1401c79b0]
  [ 2] +0x10 0x1400354c0  TINY returns dword [0x1401c79a8] = TypeId(LivestockDescription)
  [ 3] +0x18 0x140123ff0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140123f60  OWN LivestockComponent_OnCreate (125 B)
  [ 6] +0x30 0x1400355b0  OWN LivestockComponent_OnDestroy (91 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140035610  OWN LivestockComponent_OnCleanup (12 B)
  [ 9] +0x48 0x140035550  OWN LivestockDescription_CreateInPlace (90 B)
  stamped by (6): other+0x0 at 0x140035525 in 0x1400354e0 LivestockDescription_Factory; this+0x0 at 0x140035584 in 0x140035550 LivestockDescription_CreateInPlace; this+0x0 at 0x1400355e9 in 0x1400355b0 LivestockComponent_OnDestroy; this+0x0 at 0x140035644 in 0x140035620 LivestockComponent_GetTypeId; this+0x0 at 0x140124019 in 0x140124000 FUN_140124000; this+0x0 at 0x14012406e in 0x140124040 FUN_140124040
### LoaderDialog  table=0x14015f300  slots=10  sizeof=0xc8  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x756e654d6e69614d (string "MainMenuDialog")
  reflected: ti=0x1401c5be8 create=0x140020bb0 createInPlace=0x140020c20 Cleanup_LoaderDialog registered in 0x14014ce10 RegisterType_LoaderDialog
  [ 0] +0x00 0x140020c70  OWN LoaderDialog_ScalarDtor (63 B)
  [ 1] +0x08 0x140020ba0  TINY returns qword [0x1401c5bf0]
  [ 2] +0x10 0x140020b90  TINY returns dword [0x1401c5be8] = TypeId(LoaderDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x1400b2d90  OWN LoaderDialog_Serialize (83 B)
  [ 5] +0x28 0x14003a94e  IMPORT ?OnClosed@DialogInstance@UI@@UEAAXXZ  [in 12 tables]
  [ 6] +0x30 0x1400b2d00  OWN LoaderDialog_Draw (68 B)
  [ 7] +0x38 0x1400b2d50  OWN LoaderDialog_OnCreate (48 B)
  [ 8] +0x40 0x14003aa1a  OWN OnElementEvent (6 B)  [in 4 tables]
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (5): other+0x0 at 0x140020bed in 0x140020bb0 Factory_LoaderDialog; this+0x0 at 0x140020c4a in 0x140020c20 Cleanup_LoaderDialog; other+0x0 at 0x1400990f7 in 0x140099070 LoaderDialog_Ctor3; this+0x0 at 0x1400b2e14 in 0x1400b2df0 LoaderDialog_Ctor; this+0x0 at 0x1400b2e82 in 0x1400b2e40 LoaderDialog_CopyCtor
### LocationWork  table=0x1401616a0  slots=13  sizeof=0x50  parent="Work" / structural Work 0x140161a80  stop=zero qword
  reflected: ti=0x1401c6b38 create=0x14002a810 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014df90 LocationWork_RegisterType
  [ 0] +0x00 0x14002a850  OWN LocationWork_ScalarDtor (83 B)
  [ 1] +0x08 0x14002a800  TINY returns qword [0x1401c6b40]
  [ 2] +0x10 0x14002a7f0  TINY returns dword [0x1401c6b38] = TypeId(LocationWork)
  [ 3] +0x18 0x1400efba0  OWN LocationWork_Shutdown (119 B)
  [ 4] +0x20 0x1400ef880  OWN LocationWork_Serialize (594 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400ef850  OWN LocationWork_Draw (39 B)
  [ 7] +0x38 0x1400ef820  OWN LocationWork_OnCreate (37 B)
  [ 8] +0x40 0x1400efaf0  OWN LocationWork_OnRemove (69 B)
  [ 9] +0x48 0x1400ef7c0  OWN LocationWork_Update (78 B)
  [10] +0x50 0x1400efb40  OWN LocationWork_Slot10 (75 B)
  [11] +0x58 0x1400295a0  OWN BuildRoad_Slot11 (10 B)  [in 2 tables]
  [12] +0x60 0x1400efae0  TINY RET 0 (c2 00 00)
  stamped by (5): this+0x0 at 0x14002a874 in 0x14002a850 LocationWork_ScalarDtor; other+0x0 at 0x140043d0f in 0x140043c90 LocationWork_Ctor; this+0x0 at 0x1400efc39 in 0x1400efc20 Work_Ctor16; this+0x0 at 0x1400efcb7 in 0x1400efc60 LocationWork_Ctor; this+0x0 at 0x1400efd00 in 0x1400efcd0 LocationWork_CopyCtor
### MainMenuDialog  table=0x14015f550  slots=10  sizeof=0xc8  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c5c20 create=0x140020d10 createInPlace=0x140020d60 Cleanup_MainMenuDialog registered in 0x14014ce50 RegisterType_MainMenuDialog
  [ 0] +0x00 0x140020da0  OWN MainMenuDialog_ScalarDtor (73 B)
  [ 1] +0x08 0x140020d00  TINY returns qword [0x1401c5c28]
  [ 2] +0x10 0x140020cf0  TINY returns dword [0x1401c5c20] = TypeId(MainMenuDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x1400b3cd0  OWN MainMenuDialog_Serialize (1904 B)
  [ 5] +0x28 0x1400b3c60  OWN MainMenuDialog_Slot5 (96 B)
  [ 6] +0x30 0x14003a98a  IMPORT ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ  [in 8 tables]
  [ 7] +0x38 0x14003a990  IMPORT ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z  [in 11 tables]
  [ 8] +0x40 0x1400b3b50  OWN MainMenuDialog_OnRemove (264 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (8): other+0x0 at 0x140020d4d in 0x140020d10 Factory_MainMenuDialog; this+0x0 at 0x140020d8a in 0x140020d60 Cleanup_MainMenuDialog; this+0x0 at 0x140020dc4 in 0x140020da0 MainMenuDialog_ScalarDtor; other+0x0 at 0x14009babc in 0x14009ba30 MainMenuDialog_Ctor; this+0x0 at 0x1400b4469 in 0x1400b4450 MainMenuDialog_Ctor3; this+0x0 at 0x1400b44a4 in 0x1400b4480 MainMenuDialog_Ctor4; this+0x0 at 0x1400b4501 in 0x1400b44c0 MainMenuDialog_Ctor5; other+0x0 at 0x140101e60 in 0x140101ce0 EntityUI_OpenPauseMenu
### MapComponent  table=0x140163aa0  slots=16  sizeof=0x38  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c79e0 create=0x140035700 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f350 MapComponent_RegisterType
  [ 0] +0x00 0x140035850  OWN MapComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x1400356f0  TINY returns qword [0x1401c79e8]
  [ 2] +0x10 0x1400356e0  TINY returns dword [0x1401c79e0] = TypeId(MapComponent)
  [ 3] +0x18 0x1401243e0  OWN MapComponent_Shutdown (162 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140124370  OWN MapComponent_Draw (65 B)
  [ 7] +0x38 0x1401242f0  OWN MapComponent_OnCreate (111 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x1401243c0  OWN MapComponent_Slot12 (11 B)
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140035874 in 0x140035850 MapComponent_ScalarDtor; this+0x0 at 0x1401244a9 in 0x140124490 MapComponent_Ctor; this+0x0 at 0x1401244f8 in 0x1401244d0 MapComponent_Ctor
### MapDescription  table=0x140163b20  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x7365446c65646f4d (string "ModelDescription")
  reflected: ti=0x1401c7a18 create=0x140035760 createInPlace=0x1400357a0 MapDescription_CreateInPlace registered in 0x14014f390 MapDescription_Register
  [ 0] +0x00 0x1400357e0  OWN MapComponent_vslot16 (97 B)
  [ 1] +0x08 0x140035750  TINY returns qword [0x1401c7a20]
  [ 2] +0x10 0x140035740  TINY returns dword [0x1401c7a18] = TypeId(MapDescription)
  [ 3] +0x18 0x140124610  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140124560  OWN FUN_140124560 (158 B)
  [ 6] +0x30 0x1400357b0  OWN FUN_1400357b0 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400357d0  OWN (no Ghidra function)
  [ 9] +0x48 0x1400357a0  OWN MapDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x140035804 in 0x1400357e0 MapComponent_vslot16; this+0x0 at 0x14012463d in 0x140124620 FUN_140124620; this+0x0 at 0x14012469e in 0x140124670 MapDescription_sub_4670
### MapGenerator  table=0x14015ed80  slots=9  sizeof=0x178  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6e656353756e654d (string "MenuScene")
  reflected: ti=0x1401c59b8 create=0x14001ade0 createInPlace=0x14001ae20 MapGenerator_Cleanup registered in 0x14014cb80 MapGenerator_RegisterType
  [ 0] +0x00 0x14001ae70  OWN MapGenerator_ScalarDtor (48 B)
  [ 1] +0x08 0x14001add0  TINY returns qword [0x1401c59c0]
  [ 2] +0x10 0x14001adc0  TINY returns dword [0x1401c59b8] = TypeId(MapGenerator)
  [ 3] +0x18 0x1400a3020  TINY return 8 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400a2980  OWN MapGenerator_Slot5 (1687 B)
  [ 6] +0x30 0x14001ae30  OWN MapGenerator_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14001ae20  OWN MapGenerator_Cleanup (12 B)
  stamped by (1): this+0x0 at 0x1400a3054 in 0x1400a3030 MapGenerator_Ctor
### MapParamsDialog  table=0x14015c180  slots=10  sizeof=0xf8  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x44746e656d656c45 (string "ElementDescription")
  reflected: ti=0x1401c50f8 create=0x140012100 createInPlace=0x140012140 <no fn> registered in 0x14014c180 RegisterType_MapParamsDialog
  [ 0] +0x00 0x140012150  OWN MapParamsDialog_ScalarDtor (73 B)
  [ 1] +0x08 0x1400120f0  TINY returns qword [0x1401c5100]
  [ 2] +0x10 0x1400120e0  TINY returns dword [0x1401c50f8] = TypeId(MapParamsDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x14006b810  OWN MapParamsDialog_Serialize (1329 B)
  [ 5] +0x28 0x14006b800  TINY RET 0 (c2 00 00)
  [ 6] +0x30 0x14006b790  OWN MapParamsDialog_Draw (103 B)
  [ 7] +0x38 0x14003a990  IMPORT ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z  [in 11 tables]
  [ 8] +0x40 0x14006b5f0  OWN MapParamsDialog_OnRemove (88 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (4): this+0x0 at 0x140012174 in 0x140012150 MapParamsDialog_ScalarDtor; this+0x0 at 0x14006bd69 in 0x14006bd50 MapParamsDialog_Ctor; this+0x0 at 0x14006bdc2 in 0x14006bd80 MapParamsDialog_CopyCtor; this+0x0 at 0x14006be44 in 0x14006be20 MapParamsDialog_Ctor
### MapUI  table=0x14015c350  slots=11  sizeof=0x50  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=zero qword
  reflected: ti=0x1401c5130 create=0x140012240 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014c1c0 RegisterType_MapUI
  [ 0] +0x00 0x1400124d0  OWN FrustumOutlineDescription_Slot10 (48 B)
  [ 1] +0x08 0x140012230  TINY returns qword [0x1401c5138]
  [ 2] +0x10 0x140012220  TINY returns dword [0x1401c5130] = TypeId(MapUI)
  [ 3] +0x18 0x14006ce80  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x14006ca80  OWN FrustumOutlineDescription_Slot14 (109 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14003a97e  IMPORT ?OnUpdateLayout@DialogController@UI@@UEAAXXZ  [in 10 tables]
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14006c9e0  OWN MapUI_Update (152 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (3): other+0x0 at 0x14001227b in 0x140012240 Factory_MapUI; this+0x0 at 0x14006cead in 0x14006ce90 MapUI_Ctor; this+0x0 at 0x14006cf44 in 0x14006cf20 MapUI_CopyCtor
### MarketComponent  table=0x140159c40  slots=22  sizeof=0x50  parent="ResourceProduceComponent" / structural ResourceProduceComponent 0x1401643f0  stop=next table
  reflected: ti=0x1401c44f0 create=0x140008220 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b0c0 MarketComponent_RegisterType
  [ 0] +0x00 0x140008440  OWN MarketComponent_ScalarDtor (125 B)
  [ 1] +0x08 0x140008210  TINY returns qword [0x1401c44f8]
  [ 2] +0x10 0x140008200  TINY returns dword [0x1401c44f0] = TypeId(MarketComponent)
  [ 3] +0x18 0x14004ead0  OWN MarketComponent_Shutdown (48 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x14012b520  OWN HerbalistComponent_Slot13 (112 B)  [in 6 tables]   = parent
  [14] +0x70 0x14012b5a0  OWN HerbalistComponent_Slot14 (117 B)  [in 6 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]   = parent
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]   = parent
  [18] +0x90 0x14004ea40  OWN MarketWork_Ctor (133 B)
  [19] +0x98 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  [20] +0xa0 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  [21] +0xa8 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (4): other+0x0 at 0x14000825a in 0x140008220 MarketComponent_Factory; this+0x0 at 0x140008464 in 0x140008440 MarketComponent_ScalarDtor; this+0x0 at 0x14004eb29 in 0x14004eb10 ResourceProduceComponent_CopyCtor; this+0x0 at 0x14004eb93 in 0x14004eb70 MarketComponent_Ctor
### MarketDescription  table=0x140159cf0  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6f4c74656b72614d (string "MarketLocationComponent")
  reflected: ti=0x1401c4528 create=0x140008290 createInPlace=0x140008300 MarketDescription_CreateInPlace registered in 0x14014b100 MarketDescription_Register
  [ 0] +0x00 0x1400083d0  OWN FUN_1400083d0 (106 B)
  [ 1] +0x08 0x140008280  TINY returns qword [0x1401c4530]
  [ 2] +0x10 0x140008270  TINY returns dword [0x1401c4528] = TypeId(MarketDescription)
  [ 3] +0x18 0x14004ec80  TINY return 3 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14004ebb0  OWN FUN_14004ebb0 (199 B)
  [ 6] +0x30 0x140008360  OWN FUN_140008360 (86 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400083c0  OWN (no Ghidra function)
  [ 9] +0x48 0x140008300  OWN MarketDescription_CreateInPlace (85 B)
  stamped by (6): other+0x0 at 0x1400082d5 in 0x140008290 MarketDescription_Factory; this+0x0 at 0x140008334 in 0x140008300 MarketDescription_CreateInPlace; this+0x0 at 0x140008399 in 0x140008360 FUN_140008360; this+0x0 at 0x1400083f4 in 0x1400083d0 FUN_1400083d0; this+0x0 at 0x14004ecad in 0x14004ec90 FUN_14004ec90; this+0x0 at 0x14004ed1e in 0x14004ecf0 FUN_14004ecf0
### MarketLocationComponent  table=0x140159dd0  slots=18  sizeof=0x48  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c4560 create=0x1400086c0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b140 MarketLocationComponent_RegisterType
  member vptrs: +0x28=0x140159d80, +0x28=0x14015f070
  [ 0] +0x00 0x140008900  OWN MarketLocationComponent_Controller_Slot10 (115 B)
  [ 1] +0x08 0x1400086b0  TINY returns qword [0x1401c4568]
  [ 2] +0x10 0x1400086a0  TINY returns dword [0x1401c4560] = TypeId(MarketLocationComponent)
  [ 3] +0x18 0x14004efa0  OWN MarketLocationComponent_Controller_Slot13 (76 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x14004ef60  OWN MarketLocationComponent_OnCreate (46 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (3): this+0x0 at 0x140008924 in 0x140008900 MarketLocationComponent_Controller_Slot10; this+0x0 at 0x14004f019 in 0x14004f000 MarketLocationComponent_Ctor; this+0x0 at 0x14004f092 in 0x14004f060 MarketLocationComponent_Ctor
### MarketLocationDescription  table=0x140159e60  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x706d6f43656e694d (string "MineComponent")
  reflected: ti=0x1401c4598 create=0x140008720 createInPlace=0x140008780 MarketLocationDescription_CreateInPlace registered in 0x14014b180 MarketLocationDescription_Register
  [ 0] +0x00 0x1400088a0  OWN MarketLocationComponent_vslot18 (83 B)
  [ 1] +0x08 0x140008710  TINY returns qword [0x1401c45a0]
  [ 2] +0x10 0x140008700  TINY returns dword [0x1401c4598] = TypeId(MarketLocationDescription)
  [ 3] +0x18 0x14004f1b0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14004f170  OWN FUN_14004f170 (46 B)
  [ 6] +0x30 0x1400087d0  OWN FUN_1400087d0 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140008820  OWN (no Ghidra function)
  [ 9] +0x48 0x140008780  OWN MarketLocationDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x140008765 in 0x140008720 MarketLocationDescription_Factory; this+0x0 at 0x1400087b4 in 0x140008780 MarketLocationDescription_CreateInPlace; this+0x0 at 0x140008809 in 0x1400087d0 FUN_1400087d0; this+0x0 at 0x1400088c4 in 0x1400088a0 MarketLocationComponent_vslot18; this+0x0 at 0x14004f1d9 in 0x14004f1c0 FUN_14004f1c0; this+0x0 at 0x14004f22e in 0x14004f200 FUN_14004f200
### MarketWork  table=0x140161710  slots=13  sizeof=0x58  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x6f5774656b72614d (string "MarketWork")
  reflected: ti=0x1401c6b70 create=0x14002a8d0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014dfd0 MarketWork_RegisterType
  [ 0] +0x00 0x14002a910  OWN LocationWork_Slot14 (125 B)
  [ 1] +0x08 0x14002a8c0  TINY returns qword [0x1401c6b78]
  [ 2] +0x10 0x14002a8b0  TINY returns dword [0x1401c6b70] = TypeId(MarketWork)
  [ 3] +0x18 0x1400f1d70  OWN MarketWork_Shutdown (387 B)
  [ 4] +0x20 0x1400eff80  OWN MarketWork_Serialize (7444 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400eff60  OWN MarketWork_Draw (20 B)
  [ 7] +0x38 0x1400eff50  THUNK this+72 -> 0x14002aaa0 MarketWork_sub_aaa0
  [ 8] +0x40 0x1400f1ca0  OWN MarketWork_OnRemove (104 B)
  [ 9] +0x48 0x1400efd20  OWN MarketWork_Update (547 B)
  [10] +0x50 0x1400f1d10  OWN MarketWork_Slot10 (75 B)
  [11] +0x58 0x140029860  OWN ClearArea_Slot11 (9 B)  [in 13 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (5): this+0x0 at 0x14002a939 in 0x14002a910 LocationWork_Slot14; other+0x0 at 0x14004ea90 in 0x14004ea40 MarketWork_Ctor; this+0x0 at 0x1400f1f22 in 0x1400f1f00 Work_Ctor17; this+0x0 at 0x1400f1fc7 in 0x1400f1f70 MarketWork_Ctor; this+0x0 at 0x1400f2070 in 0x1400f2040 MarketWork_CopyCtor
### MenuScene  table=0x14015ede0  slots=9  sizeof=0x208  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x73694c636973754d (string "MusicList")
  reflected: ti=0x1401c59f0 create=0x14001b7c0 createInPlace=0x14001b800 MenuScene_OnRemove registered in 0x14014cbc0 MenuScene_RegisterType
  [ 0] +0x00 0x14001b830  OWN MenuScene_ScalarDtor (48 B)
  [ 1] +0x08 0x14001b7b0  TINY returns qword [0x1401c59f8]
  [ 2] +0x10 0x14001b7a0  TINY returns dword [0x1401c59f0] = TypeId(MenuScene)
  [ 3] +0x18 0x1400a3960  TINY return 1 (mov eax)
  [ 4] +0x20 0x1400a3620  TINY return 1 (mov eax)
  [ 5] +0x28 0x1400a3630  OWN MenuScene_Slot5 (799 B)
  [ 6] +0x30 0x14001b810  OWN MenuScene_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14001b800  OWN MenuScene_OnRemove (12 B)
  stamped by (2): this+0x0 at 0x1400a3992 in 0x1400a3970 MenuScene_Ctor; this+0x0 at 0x1400a3d99 in 0x1400a3d70 MenuScene_Ctor
### MessageDialog  table=0x14015c480  slots=14  sizeof=0xc0  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x73776f7242646f4d (string "ModBrowseUI")
  reflected: ti=0x1401c51d8 create=0x1400125d0 createInPlace=0x140012620 Cleanup_MessageDialog registered in 0x14014c280 RegisterType_MessageDialog
  [ 0] +0x00 0x140012660  OWN MessageDialog_ScalarDtor (73 B)
  [ 1] +0x08 0x1400125c0  TINY returns qword [0x1401c51e0]
  [ 2] +0x10 0x1400125b0  TINY returns dword [0x1401c51d8] = TypeId(MessageDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x14006d140  OWN MessageDialog_Serialize (380 B)  [in 2 tables]
  [ 5] +0x28 0x14006d0c0  TINY RET 0 (c2 00 00)  [in 2 tables]
  [ 6] +0x30 0x14003a98a  IMPORT ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ  [in 8 tables]
  [ 7] +0x38 0x14003a990  IMPORT ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z  [in 11 tables]
  [ 8] +0x40 0x14006d070  OWN MessageDialog_OnRemove (59 B)  [in 2 tables]
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [10] +0x50 0x14006cff0  OWN MessageDialog_Slot10 (51 B)  [in 2 tables]
  [11] +0x58 0x14006d030  OWN MessageDialog_Slot11 (51 B)  [in 2 tables]
  [12] +0x60 0x14006cfb0  OWN MessageDialog_Slot12 (51 B)  [in 2 tables]
  [13] +0x68 0x14006cf70  OWN MessageDialog_Slot13 (51 B)  [in 2 tables]
  stamped by (16): other+0x0 at 0x14001260d in 0x1400125d0 Factory_MessageDialog; this+0x0 at 0x14001264a in 0x140012620 Cleanup_MessageDialog; this+0x0 at 0x140012684 in 0x140012660 MessageDialog_ScalarDtor; other+0x0 at 0x140012e9d in 0x140012e60 Factory_OptionInputBindDialog; this+0x0 at 0x140012efa in 0x140012ed0 Cleanup_OptionInputBindDialog; this+0x0 at 0x140012fa4 in 0x140012f80 OptionInputBindDialog_ScalarDtor; this+0x0 at 0x14006d2e9 in 0x14006d2d0 MessageDialog_Ctor5; this+0x0 at 0x14006d341 in 0x14006d300 MessageDialog_Ctor6; this+0x0 at 0x14006d384 in 0x14006d360 MessageDialog_Ctor7; other+0x0 at 0x14006ebcd in 0x14006ea70 MessageDialog_Ctor; this+0x0 at 0x140071d72 in 0x140071d30 OptionInputBindDialog_Ctor; this+0x0 at 0x140071dd4 in 0x140071db0 OptionInputBindDialog_CopyCtor ...
  other refs (1):   0x1400b6236 MessageDialog_CopyCtor+86 : LEA R14,[0x14015c480]
### MineComponent  table=0x140159ed0  slots=16  sizeof=0x28  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c45d0 create=0x140008dd0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b280 MineComponent_RegisterType
  [ 0] +0x00 0x140008fa0  OWN MineComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x140008dc0  TINY returns qword [0x1401c45d8]
  [ 2] +0x10 0x140008db0  TINY returns dword [0x1401c45d0] = TypeId(MineComponent)
  [ 3] +0x18 0x14004f260  OWN MineComponent_Shutdown (48 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x14004f250  TINY RET 0 (c2 00 00)
  [ 7] +0x38 0x14004f240  TINY RET 0 (c2 00 00)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140008fc4 in 0x140008fa0 MineComponent_ScalarDtor; this+0x0 at 0x14004f2b9 in 0x14004f2a0 MineComponent_Ctor; this+0x0 at 0x14004f308 in 0x14004f2e0 MineComponent_Ctor
### MineDescription  table=0x140159f50  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6d6f4364616d6f4e (string "NomadComponent")
  reflected: ti=0x1401c4608 create=0x140008e30 createInPlace=0x140008e90 MineDescription_CreateInPlace registered in 0x14014b2c0 MineDescription_Register
  [ 0] +0x00 0x140008f40  OWN MineComponent_vslot16 (83 B)
  [ 1] +0x08 0x140008e20  TINY returns qword [0x1401c4610]
  [ 2] +0x10 0x140008e10  TINY returns dword [0x1401c4608] = TypeId(MineDescription)
  [ 3] +0x18 0x14004f390  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14004f350  OWN FUN_14004f350 (46 B)
  [ 6] +0x30 0x140008ee0  OWN FUN_140008ee0 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140008f30  OWN (no Ghidra function)
  [ 9] +0x48 0x140008e90  OWN MineDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x140008e75 in 0x140008e30 MineDescription_Factory; this+0x0 at 0x140008ec4 in 0x140008e90 MineDescription_CreateInPlace; this+0x0 at 0x140008f19 in 0x140008ee0 FUN_140008ee0; this+0x0 at 0x140008f64 in 0x140008f40 MineComponent_vslot16; this+0x0 at 0x14004f3b9 in 0x14004f3a0 FUN_14004f3a0; this+0x0 at 0x14004f40e in 0x14004f3e0 FUN_14004f3e0
### ModBrowseUI  table=0x14015c500  slots=11  sizeof=0x30  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x6961746544646f4d (string "ModDetailsDialog")
  reflected: ti=0x1401c5210 create=0x1400126d0 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014c2c0 RegisterType_ModBrowseUI
  [ 0] +0x00 0x140012720  OWN ModBrowseUI_ScalarDtor (73 B)
  [ 1] +0x08 0x1400126c0  TINY returns qword [0x1401c5218]
  [ 2] +0x10 0x1400126b0  TINY returns dword [0x1401c5210] = TypeId(ModBrowseUI)
  [ 3] +0x18 0x14006d390  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x14003a984  IMPORT ?OnCreated@DialogController@UI@@UEAAXXZ  [in 5 tables]
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14003a97e  IMPORT ?OnUpdateLayout@DialogController@UI@@UEAAXXZ  [in 10 tables]
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14003a972  IMPORT ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z  [in 11 tables]
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x14001270b in 0x1400126d0 Factory_ModBrowseUI; this+0x0 at 0x140012744 in 0x140012720 ModBrowseUI_ScalarDtor; this+0x0 at 0x14006d3b9 in 0x14006d3a0 ModBrowseUI_Ctor; this+0x0 at 0x14006d3f4 in 0x14006d3d0 ModBrowseUI_CopyCtor
### ModDetailsDialog  table=0x14015c670  slots=10  sizeof=0xc8  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c5248 create=0x140012790 createInPlace=0x1400127e0 Cleanup_ModDetailsDialog registered in 0x14014c300 RegisterType_ModDetailsDialog
  [ 0] +0x00 0x140012890  OWN ModDetailsDialog_ScalarDtor (73 B)
  [ 1] +0x08 0x140012780  TINY returns qword [0x1401c5250]
  [ 2] +0x10 0x140012770  TINY returns dword [0x1401c5248] = TypeId(ModDetailsDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x14006d700  OWN ModDetailsDialog_Serialize (2942 B)
  [ 5] +0x28 0x14003a94e  IMPORT ?OnClosed@DialogInstance@UI@@UEAAXXZ  [in 12 tables]
  [ 6] +0x30 0x14003a98a  IMPORT ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ  [in 8 tables]
  [ 7] +0x38 0x14003a990  IMPORT ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z  [in 11 tables]
  [ 8] +0x40 0x14006d6a0  OWN ModDetailsDialog_OnRemove (89 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (7): other+0x0 at 0x1400127cd in 0x140012790 Factory_ModDetailsDialog; this+0x0 at 0x14001280a in 0x1400127e0 Cleanup_ModDetailsDialog; this+0x0 at 0x1400128b4 in 0x140012890 ModDetailsDialog_ScalarDtor; this+0x0 at 0x14006e3c9 in 0x14006e3b0 ModDetailsDialog_Ctor; this+0x0 at 0x14006e421 in 0x14006e3e0 ModDetailsDialog_CopyCtor; this+0x0 at 0x14006e474 in 0x14006e450 ModDetailsDialog_Ctor3; other+0x0 at 0x14006e5a6 in 0x14006e490 ModDetailsDialog_Ctor4
### ModDialog  table=0x14015c870  slots=10  sizeof=0xd8  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c5280 create=0x140012900 createInPlace=0x140012970 Cleanup_ModDialog registered in 0x14014c340 RegisterType_ModDialog
  [ 0] +0x00 0x140012a30  OWN ModDialog_ScalarDtor (48 B)
  [ 1] +0x08 0x1400128f0  TINY returns qword [0x1401c5288]
  [ 2] +0x10 0x1400128e0  TINY returns dword [0x1401c5280] = TypeId(ModDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x14006f150  OWN ModDialog_Serialize (3498 B)
  [ 5] +0x28 0x14003a94e  IMPORT ?OnClosed@DialogInstance@UI@@UEAAXXZ  [in 12 tables]
  [ 6] +0x30 0x14006e480  IMPORT ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ
  [ 7] +0x38 0x14003a990  IMPORT ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z  [in 11 tables]
  [ 8] +0x40 0x14006ed40  OWN ModDialog_OnRemove (376 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (5): other+0x0 at 0x14001293d in 0x140012900 Factory_ModDialog; this+0x0 at 0x14001299a in 0x140012970 Cleanup_ModDialog; this+0x0 at 0x14006ff24 in 0x14006ff00 ModDialog_Ctor; this+0x0 at 0x140070072 in 0x140070030 ModDialog_CopyCtor; this+0x0 at 0x1400700e4 in 0x1400700c0 ModDialog_Ctor3
### ModelComponent  table=0x140163ba0  slots=16  sizeof=0x170  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7a50 create=0x1400358d0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f3d0 ModelComponent_RegisterType
  [ 0] +0x00 0x140035ad0  OWN ModelComponent_ScalarDtor (97 B)
  [ 1] +0x08 0x1400358c0  TINY returns qword [0x1401c7a58]
  [ 2] +0x10 0x1400358b0  TINY returns dword [0x1401c7a50] = TypeId(ModelComponent)
  [ 3] +0x18 0x140124b70  OWN ModelComponent_Shutdown (337 B)
  [ 4] +0x20 0x140124ac0  OWN ModelComponent_Serialize (163 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140124a30  OWN ModelComponent_Draw (134 B)
  [ 7] +0x38 0x1401249d0  OWN ModelComponent_OnCreate (88 B)
  [ 8] +0x40 0x140124960  OWN ModelComponent_OnRemove (95 B)
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140035af4 in 0x140035ad0 ModelComponent_ScalarDtor; this+0x0 at 0x140124ced in 0x140124cd0 ModelComponent_Ctor; this+0x0 at 0x140124d48 in 0x140124d20 ModelComponent_Ctor
### ModelDescription  table=0x140163c20  slots=10  sizeof=0x40  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6a624f69746c754d (string "MultiObjectDescription")
  reflected: ti=0x1401c7a88 create=0x140035930 createInPlace=0x1400359a0 ModelDescription_CreateInPlace registered in 0x14014f410 ModelDescription_Register
  [ 0] +0x00 0x140035a70  OWN ModelComponent_vslot16 (96 B)
  [ 1] +0x08 0x140035920  TINY returns qword [0x1401c7a90]
  [ 2] +0x10 0x140035910  TINY returns dword [0x1401c7a88] = TypeId(ModelDescription)
  [ 3] +0x18 0x140124ef0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140124dc0  OWN FUN_140124dc0 (285 B)
  [ 6] +0x30 0x140035a00  OWN FUN_140035a00 (86 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140035a60  OWN (no Ghidra function)
  [ 9] +0x48 0x1400359a0  OWN ModelDescription_CreateInPlace (85 B)
  stamped by (6): other+0x0 at 0x140035975 in 0x140035930 ModelDescription_Factory; this+0x0 at 0x1400359d4 in 0x1400359a0 ModelDescription_CreateInPlace; this+0x0 at 0x140035a39 in 0x140035a00 FUN_140035a00; this+0x0 at 0x140035a94 in 0x140035a70 ModelComponent_vslot16; this+0x0 at 0x140124f1d in 0x140124f00 FUN_140124f00; this+0x0 at 0x140124f7e in 0x140124f50 FUN_140124f50
### ModUploadDialog  table=0x14015c6c0  slots=10  sizeof=0xb8  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x6f6c616944646f4d (string "ModDialog")
  reflected: ti=0x1401c52b8 create=0x140012bc0 createInPlace=0x140012c10 Cleanup_ModUploadDialog registered in 0x14014c380 RegisterType_ModUploadDialog
  [ 0] +0x00 0x140012840  OWN ModDetailsDialog_Slot10 (73 B)
  [ 1] +0x08 0x140012830  TINY returns qword [0x1401c52c0]
  [ 2] +0x10 0x140012820  TINY returns dword [0x1401c52b8] = TypeId(ModUploadDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x14003aa14  IMPORT ?OnOpened@DialogInstance@UI@@UEAAXXZ  [in 2 tables]
  [ 5] +0x28 0x14003a94e  IMPORT ?OnClosed@DialogInstance@UI@@UEAAXXZ  [in 12 tables]
  [ 6] +0x30 0x14003a98a  IMPORT ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ  [in 8 tables]
  [ 7] +0x38 0x14003a990  IMPORT ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z  [in 11 tables]
  [ 8] +0x40 0x14003aa1a  OWN OnElementEvent (6 B)  [in 4 tables]
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (5): this+0x0 at 0x140012864 in 0x140012840 ModDetailsDialog_Slot10; other+0x0 at 0x140012bfd in 0x140012bc0 Factory_ModUploadDialog; this+0x0 at 0x140012c3a in 0x140012c10 Cleanup_ModUploadDialog; other+0x0 at 0x14006d503 in 0x14006d400 ModUploadDialog_Ctor; other+0x0 at 0x14006d653 in 0x14006d550 ModUploadDialog_CopyCtor
### MoveTo  table=0x14015fe40  slots=10  sizeof=0x480  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x576d726f66726550 (string "PerformWork")
  reflected: ti=0x1401c5fa0 create=0x1400228c0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d250 MoveTo_RegisterType
  [ 0] +0x00 0x140022900  OWN MoveTo_ScalarDtor (104 B)
  [ 1] +0x08 0x1400228b0  TINY returns qword [0x1401c5fa8]
  [ 2] +0x10 0x1400228a0  TINY returns dword [0x1401c5fa0] = TypeId(MoveTo)
  [ 3] +0x18 0x1400c8ec0  OWN MoveTo_Shutdown (452 B)
  [ 4] +0x20 0x1400c8ab0  OWN MoveTo_Serialize (342 B)
  [ 5] +0x28 0x1400c8570  OWN MoveTo_Slot5 (1017 B)
  [ 6] +0x30 0x1400c8970  OWN MoveTo_Draw (307 B)
  [ 7] +0x38 0x1400c8520  OWN MoveTo_OnCreate (59 B)
  [ 8] +0x40 0x1400c84d0  OWN MoveTo_OnRemove (59 B)
  [ 9] +0x48 0x1400c8480  OWN MoveTo_Update (59 B)
  stamped by (4): this+0x0 at 0x140022924 in 0x140022900 MoveTo_ScalarDtor; this+0x0 at 0x1400c90ad in 0x1400c9090 Action_Action_Ctor14; this+0x0 at 0x1400c9117 in 0x1400c90e0 MoveTo_Ctor; this+0x0 at 0x1400c91c5 in 0x1400c9170 MoveTo_Action_Ctor
### MultiObjectComponent  table=0x140163cf0  slots=18  sizeof=0x60  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c7ac0 create=0x140035ef0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f450 MultiObjectComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x140163ca0
  [ 0] +0x00 0x1400360c0  OWN MultiObjectComponent_Controller_Slot10 (48 B)
  [ 1] +0x08 0x140035ee0  TINY returns qword [0x1401c7ac8]
  [ 2] +0x10 0x140035ed0  TINY returns dword [0x1401c7ac0] = TypeId(MultiObjectComponent)
  [ 3] +0x18 0x140125880  OWN MultiObjectComponent_Controller_Slot13 (369 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x1401257c0  OWN MultiObjectComponent_OnCreate (185 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140125490  OWN MultiObjectComponent_Slot12 (191 B)
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (2): this+0x0 at 0x140125a1d in 0x140125a00 MultiObjectComponent_Ctor; this+0x0 at 0x140125aa2 in 0x140125a70 MultiObjectComponent_Ctor
### MultiObjectDescription  table=0x140163d80  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x526c61727574614e (string "NaturalResourceComponent")
  reflected: ti=0x1401c7af8 create=0x140035f50 createInPlace=0x140035fb0 MultiObjectDescription_CreateInPlace registered in 0x14014f490 MultiObjectDescription_Register
  [ 0] +0x00 0x140036060  OWN MultiObjectComponent_vslot18 (83 B)
  [ 1] +0x08 0x140035f40  TINY returns qword [0x1401c7b00]
  [ 2] +0x10 0x140035f30  TINY returns dword [0x1401c7af8] = TypeId(MultiObjectDescription)
  [ 3] +0x18 0x140125b90  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140125b50  OWN FUN_140125b50 (46 B)
  [ 6] +0x30 0x140036000  OWN FUN_140036000 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140036050  OWN (no Ghidra function)
  [ 9] +0x48 0x140035fb0  OWN MultiObjectDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x140035f95 in 0x140035f50 MultiObjectDescription_Factory; this+0x0 at 0x140035fe4 in 0x140035fb0 MultiObjectDescription_CreateInPlace; this+0x0 at 0x140036039 in 0x140036000 FUN_140036000; this+0x0 at 0x140036084 in 0x140036060 MultiObjectComponent_vslot18; this+0x0 at 0x140125bb9 in 0x140125ba0 FUN_140125ba0; this+0x0 at 0x140125c0e in 0x140125be0 FUN_140125be0
### MusicList  table=0x14015ee40  slots=9  sizeof=0x30  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x62646e (string "ndb")
  reflected: ti=0x1401c5a28 create=0x14001c260 createInPlace=0x14001c2c0 MenuScene_StaticInit_sub_c2c0 registered in 0x14014cc00 MenuScene_StaticInit
  [ 0] +0x00 0x14001c360  OWN MenuScene_vslot12 (96 B)
  [ 1] +0x08 0x14001c250  TINY returns qword [0x1401c5a30]
  [ 2] +0x10 0x14001c240  TINY returns dword [0x1401c5a28] = TypeId(MusicList)
  [ 3] +0x18 0x1400a3f80  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400a3e90  OWN MenuScene_vslot17 (230 B)
  [ 6] +0x30 0x14001c310  OWN MenuScene_vslot18 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14001c2c0  OWN MenuScene_StaticInit_sub_c2c0 (68 B)
  stamped by (6): other+0x0 at 0x14001c29b in 0x14001c260 MenuScene_StaticInit_sub_c260; this+0x0 at 0x14001c2ea in 0x14001c2c0 MenuScene_StaticInit_sub_c2c0; this+0x0 at 0x14001c33f in 0x14001c310 MenuScene_vslot18; this+0x0 at 0x14001c384 in 0x14001c360 MenuScene_vslot12; this+0x0 at 0x1400a3fad in 0x1400a3f90 FUN_1400a3f90; this+0x0 at 0x1400a4004 in 0x1400a3fe0 FUN_1400a3fe0
### NameDatabase  table=0x14015eea0  slots=9  sizeof=0x60  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6d614e2f656d6147 (string "Game/Names/Female.ndb")
  reflected: ti=0x1401c5a60 create=0x14001c660 createInPlace=0x14001c6d0 MenuScene_Setup_sub_c6d0 registered in 0x14014cc40 MenuScene_Setup
  [ 0] +0x00 0x14001c790  OWN FUN_14001c790 (48 B)
  [ 1] +0x08 0x14001c650  TINY returns qword [0x1401c5a68]
  [ 2] +0x10 0x14001c640  TINY returns dword [0x1401c5a60] = TypeId(NameDatabase)
  [ 3] +0x18 0x1400a4780  TINY return 1025 (mov eax)
  [ 4] +0x20 0x1400a4490  TINY return 1 (mov eax)
  [ 5] +0x28 0x1400a45e0  OWN TradeWork_impl_45e0 (404 B)
  [ 6] +0x30 0x14001c730  OWN FUN_14001c730 (93 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14001c6d0  OWN MenuScene_Setup_sub_c6d0 (92 B)
  stamped by (5): other+0x0 at 0x14001c69b in 0x14001c660 MenuScene_Setup_sub_c660; this+0x0 at 0x14001c6fa in 0x14001c6d0 MenuScene_Setup_sub_c6d0; this+0x0 at 0x14001c75f in 0x14001c730 FUN_14001c730; this+0x0 at 0x1400a47b2 in 0x1400a4790 FUN_1400a4790; this+0x0 at 0x1400a4844 in 0x1400a4820 FUN_1400a4820
### NaturalResourceComponent  table=0x140163e10  slots=16  sizeof=0x30  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7b30 create=0x140036550 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f550 NaturalResourceComponent_RegisterType
  [ 0] +0x00 0x1400366b0  OWN NaturalResourceComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x140036540  TINY returns qword [0x1401c7b38]
  [ 2] +0x10 0x140036530  TINY returns dword [0x1401c7b30] = TypeId(NaturalResourceComponent)
  [ 3] +0x18 0x140125cf0  OWN NaturalResourceComponent_Shutdown (126 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140125c40  OWN NaturalResourceComponent_OnRemove (163 B)
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (4): other+0x0 at 0x14003658a in 0x140036550 NaturalResourceComponent_Factory; this+0x0 at 0x1400366d4 in 0x1400366b0 NaturalResourceComponent_ScalarDtor; this+0x0 at 0x140125d99 in 0x140125d80 NaturalResourceComponent_Ctor; this+0x0 at 0x140125de3 in 0x140125dc0 NaturalResourceComponent_CopyCtor
### NaturalResourceDescription  table=0x140163e90  slots=10  sizeof=0x88  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x4465727574736150 (string "PastureDescription")
  reflected: ti=0x1401c7b68 create=0x1400365d0 createInPlace=0x140036610 NaturalResourceDescription_CreateInPlace registered in 0x14014f590 NaturalResourceDescription_Register
  [ 0] +0x00 0x140036650  OWN NaturalResourceComponent_vslot16 (83 B)
  [ 1] +0x08 0x1400365c0  TINY returns qword [0x1401c7b70]
  [ 2] +0x10 0x1400365b0  TINY returns dword [0x1401c7b68] = TypeId(NaturalResourceDescription)
  [ 3] +0x18 0x140125fc0  TINY return 4 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140125e10  OWN FUN_140125e10 (423 B)
  [ 6] +0x30 0x140036620  OWN FUN_140036620 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140036640  OWN (no Ghidra function)
  [ 9] +0x48 0x140036610  OWN NaturalResourceDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x140036674 in 0x140036650 NaturalResourceComponent_vslot16; this+0x0 at 0x140125fe9 in 0x140125fd0 FUN_140125fd0; this+0x0 at 0x140126043 in 0x140126010 NaturalResourceDescription_sub_6010
### NewGameDialog  table=0x14015c920  slots=10  sizeof=0xae8  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x44656d614777654e (string "NewGameDialog")
  reflected: ti=0x1401c52f0 create=0x140012c70 createInPlace=0x140012cb0 <no fn> registered in 0x14014c3c0 RegisterType_NewGameDialog
  [ 0] +0x00 0x140012cc0  OWN NewGameDialog_ScalarDtor (73 B)
  [ 1] +0x08 0x140012c60  TINY returns qword [0x1401c52f8]
  [ 2] +0x10 0x140012c50  TINY returns dword [0x1401c52f0] = TypeId(NewGameDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x1400706d0  OWN NewGameDialog_Serialize (2359 B)
  [ 5] +0x28 0x1400706c0  TINY RET 0 (c2 00 00)
  [ 6] +0x30 0x140070650  OWN NewGameDialog_Draw (103 B)
  [ 7] +0x38 0x14003a990  IMPORT ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z  [in 11 tables]
  [ 8] +0x40 0x140070110  OWN NewGameDialog_OnRemove (131 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (4): this+0x0 at 0x140012ce4 in 0x140012cc0 NewGameDialog_ScalarDtor; this+0x0 at 0x140071029 in 0x140071010 NewGameDialog_Ctor; this+0x0 at 0x140071082 in 0x140071040 NewGameDialog_CopyCtor; this+0x0 at 0x140071149 in 0x140071120 NewGameDialog_Ctor
### NoiseUI  table=0x14015ca80  slots=11  sizeof=0x70  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x44736e6f6974704f (string "OptionsDialog")
  reflected: ti=0x1401c5328 create=0x140012d30 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014c400 RegisterType_NoiseUI
  [ 0] +0x00 0x140012da0  OWN NoiseUI_ScalarDtor (48 B)
  [ 1] +0x08 0x140012d20  TINY returns qword [0x1401c5330]
  [ 2] +0x10 0x140012d10  TINY returns dword [0x1401c5328] = TypeId(NoiseUI)
  [ 3] +0x18 0x140071bb0  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x1400714c0  OWN NoiseUI_Serialize (967 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14003a97e  IMPORT ?OnUpdateLayout@DialogController@UI@@UEAAXXZ  [in 10 tables]
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x1400714a0  OWN NoiseUI_Slot9 (21 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (3): other+0x0 at 0x140012d6b in 0x140012d30 Factory_NoiseUI; this+0x0 at 0x140071bdd in 0x140071bc0 NoiseUI_Ctor; this+0x0 at 0x140071c54 in 0x140071c30 NoiseUI_CopyCtor
### NomadComponent  table=0x14015a020  slots=18  sizeof=0x78  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c4640 create=0x140009020 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b300 NomadComponent_RegisterType
  member vptrs: +0x58=0x140159fd0, +0x58=0x14015f070
  [ 0] +0x00 0x140009230  OWN NomadComponent_Controller_Slot10 (115 B)
  [ 1] +0x08 0x140009010  TINY returns qword [0x1401c4648]
  [ 2] +0x10 0x140009000  TINY returns dword [0x1401c4640] = TypeId(NomadComponent)
  [ 3] +0x18 0x14004fc40  OWN NomadComponent_Controller_Slot13 (211 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x14004fd20  OWN NomadComponent_OnCreate (42 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x140003730  TINY return 0 (xor eax,eax)  [in 5 tables]
  [17] +0x88 0x140009060  TINY returns this+0x58
  stamped by (3): this+0x0 at 0x140009254 in 0x140009230 NomadComponent_Controller_Slot10; this+0x0 at 0x14004fd69 in 0x14004fd50 NomadComponent_Ctor; this+0x0 at 0x14004fde2 in 0x14004fdb0 NomadComponent_Ctor
### NomadDescription  table=0x14015a0b0  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x436472616863724f (string "OrchardComponent")
  reflected: ti=0x1401c4678 create=0x140009090 createInPlace=0x140009100 NomadDescription_CreateInPlace registered in 0x14014b340 NomadDescription_Register
  [ 0] +0x00 0x1400091d0  OWN NomadComponent_vslot18 (83 B)
  [ 1] +0x08 0x140009080  TINY returns qword [0x1401c4680]
  [ 2] +0x10 0x140009070  TINY returns dword [0x1401c4678] = TypeId(NomadDescription)
  [ 3] +0x18 0x14004ff90  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14004ff20  OWN FUN_14004ff20 (95 B)
  [ 6] +0x30 0x140009160  OWN FUN_140009160 (83 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400091c0  OWN (no Ghidra function)
  [ 9] +0x48 0x140009100  OWN NomadDescription_CreateInPlace (82 B)
  stamped by (6): other+0x0 at 0x1400090d5 in 0x140009090 NomadDescription_Factory; this+0x0 at 0x140009134 in 0x140009100 NomadDescription_CreateInPlace; this+0x0 at 0x140009199 in 0x140009160 FUN_140009160; this+0x0 at 0x1400091f4 in 0x1400091d0 NomadComponent_vslot18; this+0x0 at 0x14004ffb9 in 0x14004ffa0 FUN_14004ffa0; this+0x0 at 0x14005000e in 0x14004ffe0 FUN_14004ffe0
### OptionInputBindDialog  table=0x14015ce40  slots=14  sizeof=0xc8  parent="MessageDialog" / structural MessageDialog 0x14015c480  stop=next table
  reflected: ti=0x1401c5398 create=0x140012e60 createInPlace=0x140012ed0 Cleanup_OptionInputBindDialog registered in 0x14014c480 RegisterType_OptionInputBindDialog
  [ 0] +0x00 0x140012f80  OWN OptionInputBindDialog_ScalarDtor (73 B)
  [ 1] +0x08 0x140012e50  TINY returns qword [0x1401c53a0]
  [ 2] +0x10 0x140012e40  TINY returns dword [0x1401c5398] = TypeId(OptionInputBindDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]   = parent
  [ 4] +0x20 0x14006d140  OWN MessageDialog_Serialize (380 B)  [in 2 tables]   = parent
  [ 5] +0x28 0x14006d0c0  TINY RET 0 (c2 00 00)  [in 2 tables]   = parent
  [ 6] +0x30 0x14003a98a  IMPORT ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ  [in 8 tables]   = parent
  [ 7] +0x38 0x140071c80  OWN OptionInputBindDialog_OnCreate (166 B)
  [ 8] +0x40 0x14006d070  OWN MessageDialog_OnRemove (59 B)  [in 2 tables]   = parent
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]   = parent
  [10] +0x50 0x14006cff0  OWN MessageDialog_Slot10 (51 B)  [in 2 tables]   = parent
  [11] +0x58 0x14006d030  OWN MessageDialog_Slot11 (51 B)  [in 2 tables]   = parent
  [12] +0x60 0x14006cfb0  OWN MessageDialog_Slot12 (51 B)  [in 2 tables]   = parent
  [13] +0x68 0x14006cf70  OWN MessageDialog_Slot13 (51 B)  [in 2 tables]   = parent
  stamped by (4): other+0x0 at 0x140012ea7 in 0x140012e60 Factory_OptionInputBindDialog; this+0x0 at 0x140012f04 in 0x140012ed0 Cleanup_OptionInputBindDialog; this+0x0 at 0x140071d7c in 0x140071d30 OptionInputBindDialog_Ctor; this+0x0 at 0x140071dde in 0x140071db0 OptionInputBindDialog_CopyCtor
### OptionsDialog  table=0x14015ceb0  slots=10  sizeof=0x1c0  parent="public UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x65527463656c6553 (string "SelectResourceUI")
  reflected: ti=0x1401c5360 create=0x140012df0 createInPlace=0x140012e30 <no fn> registered in 0x14014c440 RegisterType_OptionsDialog
  [ 0] +0x00 0x140012f30  OWN OptionInputBindDialog_Slot14 (73 B)
  [ 1] +0x08 0x140012de0  TINY returns qword [0x1401c5368]
  [ 2] +0x10 0x140012dd0  TINY returns dword [0x1401c5360] = TypeId(OptionsDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x1400742d0  OWN OptionsDialog_Serialize (3655 B)
  [ 5] +0x28 0x140073480  TINY RET 0 (c2 00 00)
  [ 6] +0x30 0x14003a98a  IMPORT ?OnUpdateLayout@DialogInstance@UI@@UEAAXXZ  [in 8 tables]
  [ 7] +0x38 0x14003a990  IMPORT ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z  [in 11 tables]
  [ 8] +0x40 0x140072f20  OWN OptionsDialog_OnRemove (1369 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (4): this+0x0 at 0x140012f54 in 0x140012f30 OptionInputBindDialog_Slot14; this+0x0 at 0x140075139 in 0x140075120 OptionsDialog_Ctor; this+0x0 at 0x140075192 in 0x140075150 OptionsDialog_CopyCtor; this+0x0 at 0x1400752a4 in 0x140075280 OptionsDialog_Ctor
### OrchardComponent  table=0x14015a180  slots=18  sizeof=0x68  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c46b0 create=0x140009870 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b400 OrchardComponent_RegisterType
  member vptrs: +0x28=0x14015a130, +0x28=0x14015f070
  [ 0] +0x00 0x140009a70  OWN OrchardComponent_Controller_Slot10 (48 B)
  [ 1] +0x08 0x140009860  TINY returns qword [0x1401c46b8]
  [ 2] +0x10 0x140009850  TINY returns dword [0x1401c46b0] = TypeId(OrchardComponent)
  [ 3] +0x18 0x140051de0  OWN OrchardComponent_Controller_Slot13 (226 B)
  [ 4] +0x20 0x140051dd0  OWN OrchardComponent_Controller_Slot14 (9 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140051c90  OWN OrchardComponent_Slot13 (306 B)
  [14] +0x70 0x140051ba0  OWN OrchardComponent_Slot14 (232 B)
  [15] +0x78 0x140051990  OWN OrchardComponent_Slot15 (513 B)
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (2): this+0x0 at 0x140051eed in 0x140051ed0 OrchardComponent_Ctor; this+0x0 at 0x140051f72 in 0x140051f40 OrchardComponent_Ctor
### OrchardDescription  table=0x14015a210  slots=10  sizeof=0x30  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x656c636974726150 (string "ParticleComponent")
  reflected: ti=0x1401c46e8 create=0x1400098d0 createInPlace=0x140009940 OrchardDescription_CreateInPlace registered in 0x14014b440 OrchardDescription_Register
  [ 0] +0x00 0x140009a10  OWN OrchardComponent_vslot18 (83 B)
  [ 1] +0x08 0x1400098c0  TINY returns qword [0x1401c46f0]
  [ 2] +0x10 0x1400098b0  TINY returns dword [0x1401c46e8] = TypeId(OrchardDescription)
  [ 3] +0x18 0x1400523a0  TINY return 3 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140052300  OWN FUN_140052300 (147 B)
  [ 6] +0x30 0x1400099a0  OWN FUN_1400099a0 (91 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140009a00  OWN (no Ghidra function)
  [ 9] +0x48 0x140009940  OWN OrchardDescription_CreateInPlace (90 B)
  stamped by (6): other+0x0 at 0x140009915 in 0x1400098d0 OrchardDescription_Factory; this+0x0 at 0x140009974 in 0x140009940 OrchardDescription_CreateInPlace; this+0x0 at 0x1400099d9 in 0x1400099a0 FUN_1400099a0; this+0x0 at 0x140009a34 in 0x140009a10 OrchardComponent_vslot18; this+0x0 at 0x1400523c9 in 0x1400523b0 FUN_1400523b0; this+0x0 at 0x14005241e in 0x1400523f0 FUN_1400523f0
### OrchardUI  table=0x14015cf30  slots=14  sizeof=0x48  parent="SelectResourceUI" / structural SelectResourceUI 0x14015d330  stop=non-code qword 0x67676f5465676150 (string "PageToggleUI")
  reflected: ti=0x1401c53d0 create=0x140012ff0 createInPlace=0x140013070 Cleanup_OrchardUI registered in 0x14014c4c0 RegisterType_OrchardUI
  [ 0] +0x00 0x1400130e0  OWN OrchardUI_ScalarDtor (83 B)
  [ 1] +0x08 0x140012fe0  TINY returns qword [0x1401c53d8]
  [ 2] +0x10 0x140012fd0  TINY returns dword [0x1401c53d0] = TypeId(OrchardUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  [ 4] +0x20 0x1400755b0  OWN OrchardUI_Serialize (186 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]   = parent
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]   = parent
  [ 7] +0x38 0x1400754b0  OWN OrchardUI_OnCreate (235 B)
  [ 8] +0x40 0x140078d30  OWN OrchardUI_OnRemove (66 B)  [in 4 tables]   = parent
  [ 9] +0x48 0x140078d80  OWN SelectResourceUI_Slot9 (21 B)  [in 3 tables]   = parent
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]   = parent
  [11] +0x58 0x140075380  OWN OrchardUI_Slot11 (141 B)
  [12] +0x60 0x140075430  OWN OrchardUI_Slot12 (114 B)
  [13] +0x68 0x140075420  TINY return 1 (mov eax)
  stamped by (5): other+0x0 at 0x140013042 in 0x140012ff0 Factory_OrchardUI; this+0x0 at 0x1400130b1 in 0x140013070 Cleanup_OrchardUI; this+0x0 at 0x140013104 in 0x1400130e0 OrchardUI_ScalarDtor; this+0x0 at 0x140075689 in 0x140075670 CropUI_Ctor; this+0x0 at 0x1400756eb in 0x1400756b0 OrchardUI_Ctor
### PageToggleUI  table=0x14015cfb0  slots=11  sizeof=0x48  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x69746375646f7250 (string "ProductionUI")
  reflected: ti=0x1401c5408 create=0x140013160 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014c500 RegisterType_PageToggleUI
  [ 0] +0x00 0x1400131c0  OWN PageToggleUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140013150  TINY returns qword [0x1401c5410]
  [ 2] +0x10 0x140013140  TINY returns dword [0x1401c5408] = TypeId(PageToggleUI)
  [ 3] +0x18 0x1400759a0  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x140075850  OWN PageToggleUI_Serialize (315 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x140075780  OWN PageToggleUI_OnCreate (108 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x140075710  OWN PageToggleUI_Update (102 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x14001319b in 0x140013160 Factory_PageToggleUI; this+0x0 at 0x1400131e4 in 0x1400131c0 PageToggleUI_ScalarDtor; this+0x0 at 0x1400759c9 in 0x1400759b0 PageToggleUI_Ctor; this+0x0 at 0x140075a04 in 0x1400759e0 PageToggleUI_CopyCtor
### ParticleComponent  table=0x14015a290  slots=16  sizeof=0x48  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c4720 create=0x14000a360 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b500 ParticleComponent_RegisterType
  [ 0] +0x00 0x14000a560  OWN ParticleComponent_ScalarDtor (48 B)
  [ 1] +0x08 0x14000a350  TINY returns qword [0x1401c4728]
  [ 2] +0x10 0x14000a340  TINY returns dword [0x1401c4720] = TypeId(ParticleComponent)
  [ 3] +0x18 0x140053270  OWN ParticleComponent_Shutdown (144 B)
  [ 4] +0x20 0x140053230  OWN ParticleComponent_Serialize (58 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140052f70  OWN ParticleComponent_Draw (278 B)
  [ 7] +0x38 0x140052e80  OWN ParticleComponent_OnCreate (221 B)
  [ 8] +0x40 0x140052b30  OWN ParticleComponent_OnRemove (833 B)
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x1400531b0  OWN ParticleComponent_Slot11 (118 B)
  [12] +0x60 0x140053090  OWN ParticleComponent_Slot12 (275 B)
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (2): this+0x0 at 0x14005333c in 0x140053310 ParticleComponent_Ctor; this+0x0 at 0x1400533f8 in 0x1400533d0 ParticleComponent_Ctor
### ParticleDescription  table=0x14015a310  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=next table
  reflected: ti=0x1401c4758 create=0x14000a3c0 createInPlace=0x14000a430 ParticleDescription_CreateInPlace registered in 0x14014b540 ParticleDescription_Register
  [ 0] +0x00 0x14000a500  OWN ParticleComponent_vslot16 (96 B)
  [ 1] +0x08 0x14000a3b0  TINY returns qword [0x1401c4760]
  [ 2] +0x10 0x14000a3a0  TINY returns dword [0x1401c4758] = TypeId(ParticleDescription)
  [ 3] +0x18 0x1400535b0  TINY return 7 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140053460  OWN FUN_140053460 (315 B)
  [ 6] +0x30 0x14000a490  OWN FUN_14000a490 (83 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14000a4f0  OWN (no Ghidra function)
  [ 9] +0x48 0x14000a430  OWN ParticleDescription_CreateInPlace (82 B)
  stamped by (6): other+0x0 at 0x14000a405 in 0x14000a3c0 ParticleDescription_Factory; this+0x0 at 0x14000a464 in 0x14000a430 ParticleDescription_CreateInPlace; this+0x0 at 0x14000a4c9 in 0x14000a490 FUN_14000a490; this+0x0 at 0x14000a524 in 0x14000a500 ParticleComponent_vslot16; this+0x0 at 0x1400535dd in 0x1400535c0 FUN_1400535c0; this+0x0 at 0x14005363e in 0x140053610 FUN_140053610
### PastureComponent  table=0x140163f60  slots=18  sizeof=0x68  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c7ba0 create=0x140036730 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f5d0 PastureComponent_RegisterType
  member vptrs: +0x48=0x14015f070, +0x48=0x140163f10
  [ 0] +0x00 0x140036910  OWN PastureComponent_Controller_Slot10 (48 B)
  [ 1] +0x08 0x140036720  TINY returns qword [0x1401c7ba8]
  [ 2] +0x10 0x140036710  TINY returns dword [0x1401c7ba0] = TypeId(PastureComponent)
  [ 3] +0x18 0x140127f40  OWN PastureComponent_Controller_Slot13 (295 B)
  [ 4] +0x20 0x140127a50  OWN PastureComponent_Controller_Slot14 (9 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140127a30  OWN PastureComponent_Draw (23 B)
  [ 7] +0x38 0x1401277e0  OWN PastureComponent_OnRemove_ReleaseAnimals (580 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140127540  OWN PastureComponent_Slot12 (466 B)
  [13] +0x68 0x140127dd0  OWN PastureComponent_Slot13 (358 B)
  [14] +0x70 0x140127a60  OWN PastureComponent_Slot14 (867 B)
  [15] +0x78 0x140127720  OWN PastureComponent_Slot15 (184 B)
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140036770  TINY returns this+0x48
  stamped by (2): this+0x0 at 0x14012808d in 0x140128070 PastureComponent_Ctor; this+0x0 at 0x140128112 in 0x1401280e0 PastureComponent_Ctor
### PastureDescription  table=0x140163ff0  slots=10  sizeof=0x28  parent="Description" / structural Description 0x1401629a0  stop=next table
  reflected: ti=0x1401c7bd8 create=0x1400367a0 createInPlace=0x140036800 PastureDescription_CreateInPlace registered in 0x14014f610 PastureDescription_Register
  [ 0] +0x00 0x1400368b0  OWN PastureComponent_vslot18 (83 B)
  [ 1] +0x08 0x140036790  TINY returns qword [0x1401c7be0]
  [ 2] +0x10 0x140036780  TINY returns dword [0x1401c7bd8] = TypeId(PastureDescription)
  [ 3] +0x18 0x140128280  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140128220  OWN FUN_140128220 (90 B)
  [ 6] +0x30 0x140036850  OWN FUN_140036850 (76 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400368a0  OWN (no Ghidra function)
  [ 9] +0x48 0x140036800  OWN PastureDescription_CreateInPlace (75 B)
  stamped by (6): other+0x0 at 0x1400367e5 in 0x1400367a0 PastureDescription_Factory; this+0x0 at 0x140036834 in 0x140036800 PastureDescription_CreateInPlace; this+0x0 at 0x140036889 in 0x140036850 FUN_140036850; this+0x0 at 0x1400368d4 in 0x1400368b0 PastureComponent_vslot18; this+0x0 at 0x1401282a9 in 0x140128290 FUN_140128290; this+0x0 at 0x1401282fe in 0x1401282d0 FUN_1401282d0
### PastureUI  table=0x14015f610  slots=14  sizeof=0x40  parent="SelectResourceUI" / structural SelectResourceUI 0x14015d330  stop=non-code qword 0x636e656469736552 (string "ResidenceUI")
  reflected: ti=0x1401c5c58 create=0x140020e10 createInPlace=0x140020e80 Cleanup_PastureUI registered in 0x14014ce90 RegisterType_PastureUI
  [ 0] +0x00 0x140020ee0  OWN PastureUI_ScalarDtor (83 B)
  [ 1] +0x08 0x140020e00  TINY returns qword [0x1401c5c60]
  [ 2] +0x10 0x140020df0  TINY returns dword [0x1401c5c58] = TypeId(PastureUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  [ 4] +0x20 0x1400b5090  OWN PastureUI_Serialize (192 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]   = parent
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]   = parent
  [ 7] +0x38 0x1400b4e50  OWN PastureUI_OnCreate (326 B)
  [ 8] +0x40 0x140078d30  OWN OrchardUI_OnRemove (66 B)  [in 4 tables]   = parent
  [ 9] +0x48 0x1400b4e20  OWN PastureUI_Slot9 (41 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]   = parent
  [11] +0x58 0x1400b4c40  OWN PastureUI_SetLivestockType (161 B)
  [12] +0x60 0x1400b4d00  OWN PastureUI_Slot12 (114 B)
  [13] +0x68 0x1400b4cf0  TINY return 2 (mov eax)
  stamped by (5): other+0x0 at 0x140020e62 in 0x140020e10 Factory_PastureUI; this+0x0 at 0x140020ec1 in 0x140020e80 Cleanup_PastureUI; this+0x0 at 0x140020f04 in 0x140020ee0 PastureUI_ScalarDtor; this+0x0 at 0x1400b5179 in 0x1400b5160 CropUI_Ctor3; this+0x0 at 0x1400b51db in 0x1400b51a0 PastureUI_Ctor
### PastureWork  table=0x1401617a0  slots=13  sizeof=0x58  parent="AutoPickupWork" / structural AutoPickupWork 0x140160fa0  stop=non-code qword 0x70756b636950 (string "Pickup")
  reflected: ti=0x1401c6ba8 create=0x14002bf00 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e010 PastureWork_RegisterType
  [ 0] +0x00 0x14002bf70  OWN PastureWork_ScalarDtor (48 B)
  [ 1] +0x08 0x14002bef0  TINY returns qword [0x1401c6bb0]
  [ 2] +0x10 0x14002bee0  TINY returns dword [0x1401c6ba8] = TypeId(PastureWork)
  [ 3] +0x18 0x1400f2c60  OWN PastureWork_Shutdown (114 B)
  [ 4] +0x20 0x1400f2370  OWN PastureWork_Serialize (2067 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400f2350  OWN PastureWork_Draw (20 B)
  [ 7] +0x38 0x1400f22c0  OWN PastureWork_OnCreate (137 B)
  [ 8] +0x40 0x1400f2b90  OWN PastureWork_OnRemove (104 B)
  [ 9] +0x48 0x1400f2090  OWN PastureWork_Update (547 B)
  [10] +0x50 0x1400f2c00  OWN PastureWork_Slot10 (75 B)
  [11] +0x58 0x140029860  OWN ClearArea_Slot11 (9 B)  [in 13 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (4): other+0x0 at 0x14002bf3a in 0x14002bf00 PastureWork_Factory; this+0x0 at 0x1400f2d07 in 0x1400f2ce0 Work_Ctor5; this+0x0 at 0x1400f2dba in 0x1400f2d80 PastureWork_Ctor; this+0x0 at 0x1400f2e03 in 0x1400f2de0 PastureWork_CopyCtor
### PathTool  table=0x140160680  slots=14  sizeof=0x60  parent="Tool" / structural Tool 0x140161f10  stop=non-code qword 0x6e6f697461746f52 (string "RotationSetting")
  reflected: ti=0x1401c6390 create=0x140026b10 createInPlace=0x140026b50 <no fn> registered in 0x14014d6d0 PathTool_RegisterType
  [ 0] +0x00 0x140026b70  OWN PathTool_ScalarDtor (48 B)
  [ 1] +0x08 0x140026b00  TINY returns qword [0x1401c6398]
  [ 2] +0x10 0x140026af0  TINY returns dword [0x1401c6390] = TypeId(PathTool)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  [ 4] +0x20 0x1400de940  TINY RET 0 (c2 00 00)
  [ 5] +0x28 0x1400de930  JMP 0x1400dd250 thunk_helper_d250
  [ 6] +0x30 0x1400de6b0  OWN PathTool_Draw (524 B)
  [ 7] +0x38 0x1400dd700  TINY RET 0 (c2 00 00)
  [ 8] +0x40 0x1400dd3b0  OWN PathTool_OnRemove (834 B)
  [ 9] +0x48 0x1400de920  TINY return false (xor al,al)
  [10] +0x50 0x1400de8d0  OWN PathTool_Slot10 (46 B)
  [11] +0x58 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]   = parent
  [12] +0x60 0x1400de910  TINY return 4 (mov eax)
  [13] +0x68 0x140026b60  OWN PathTool_Slot13 (11 B)
  stamped by (2): this+0x0 at 0x1400de972 in 0x1400de950 Tool_CopyCtor; this+0x0 at 0x1400dea18 in 0x1400de9e0 PathTool_Ctor
### PerformWork  table=0x14015fea0  slots=10  sizeof=0x48  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x615270756b636950 (string "PickupRawMaterial")
  reflected: ti=0x1401c5fd8 create=0x140022b90 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d290 PerformWork_RegisterType
  [ 0] +0x00 0x140022bd0  OWN PerformWork_ScalarDtor (104 B)
  [ 1] +0x08 0x140022b80  TINY returns qword [0x1401c5fe0]
  [ 2] +0x10 0x140022b70  TINY returns dword [0x1401c5fd8] = TypeId(PerformWork)
  [ 3] +0x18 0x1400c9550  OWN PerformWork_Shutdown (262 B)
  [ 4] +0x20 0x1400c9420  OWN PerformWork_Serialize (284 B)
  [ 5] +0x28 0x1400c93c0  OWN PerformWork_Slot5 (77 B)
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c9370  OWN PerformWork_OnCreate (65 B)
  [ 8] +0x40 0x1400c9320  OWN PerformWork_OnRemove (65 B)
  [ 9] +0x48 0x1400c92d0  OWN PerformWork_Update (59 B)
  stamped by (25): this+0x0 at 0x140022bf4 in 0x140022bd0 PerformWork_ScalarDtor; other+0x0 at 0x1400baba6 in 0x1400ba9f0 PerformWork_Action_CopyCtor; other+0x0 at 0x1400bad69 in 0x1400ba9f0 PerformWork_Action_CopyCtor; other+0x0 at 0x1400baea7 in 0x1400ba9f0 PerformWork_Action_CopyCtor; other+0x0 at 0x1400baf88 in 0x1400ba9f0 PerformWork_Action_CopyCtor; other+0x0 at 0x1400bb6a0 in 0x1400bb490 Pathfinding_WalkTowards; other+0x0 at 0x1400bc9fa in 0x1400bc790 PerformWork_Action_Ctor11; other+0x0 at 0x1400bcb9e in 0x1400bc790 PerformWork_Action_Ctor11; other+0x0 at 0x1400bcf63 in 0x1400bccb0 PerformWork_Action_Ctor10; other+0x0 at 0x1400bd316 in 0x1400bd070 PerformWork_Action_Ctor5; other+0x0 at 0x1400bd67e in 0x1400bd420 PerformWork_Action_Ctor6; other+0x0 at 0x1400bd94a in 0x1400bd790 PerformWork_Action_Ctor8 ...
  other refs (1):   0x1400bada3 PerformWork_Action_CopyCtor+947 : LEA R13,[0x14015fea0]
### PickingComponent  table=0x140164040  slots=16  sizeof=0x38  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7c10 create=0x140036da0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f6d0 PickingComponent_RegisterType
  [ 0] +0x00 0x140036fa0  OWN PickingComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x140036d90  TINY returns qword [0x1401c7c18]
  [ 2] +0x10 0x140036d80  TINY returns dword [0x1401c7c10] = TypeId(PickingComponent)
  [ 3] +0x18 0x140128bb0  OWN PickingComponent_Shutdown (119 B)
  [ 4] +0x20 0x140128c30  OWN PickingComponent_Serialize (9 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140128b90  JMP 0x140128f50 Entity_impl_8f50
  [ 7] +0x38 0x140128b50  OWN PickingComponent_OnCreate (46 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140128ba0  JMP 0x140128f50 Entity_impl_8f50
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140036fc4 in 0x140036fa0 PickingComponent_ScalarDtor; this+0x0 at 0x140128c59 in 0x140128c40 PickingComponent_Ctor; this+0x0 at 0x140128ca8 in 0x140128c80 PickingComponent_Ctor
### PickingDescription  table=0x1401640c0  slots=10  sizeof=0x40  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x43676e696b636950 (string "PickingComponent")
  reflected: ti=0x1401c7c48 create=0x140036e00 createInPlace=0x140036e70 PickingDescription_CreateInPlace registered in 0x14014f710 PickingDescription_Register
  [ 0] +0x00 0x140036f40  OWN PickingComponent_vslot16 (96 B)
  [ 1] +0x08 0x140036df0  TINY returns qword [0x1401c7c50]
  [ 2] +0x10 0x140036de0  TINY returns dword [0x1401c7c48] = TypeId(PickingDescription)
  [ 3] +0x18 0x140128e00  TINY return 4 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140128d00  OWN FUN_140128d00 (249 B)
  [ 6] +0x30 0x140036ed0  OWN FUN_140036ed0 (86 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140036f30  OWN (no Ghidra function)
  [ 9] +0x48 0x140036e70  OWN PickingDescription_CreateInPlace (85 B)
  stamped by (6): other+0x0 at 0x140036e45 in 0x140036e00 PickingDescription_Factory; this+0x0 at 0x140036ea4 in 0x140036e70 PickingDescription_CreateInPlace; this+0x0 at 0x140036f09 in 0x140036ed0 FUN_140036ed0; this+0x0 at 0x140036f64 in 0x140036f40 PickingComponent_vslot16; this+0x0 at 0x140128e2d in 0x140128e10 FUN_140128e10; this+0x0 at 0x140128e8e in 0x140128e60 FUN_140128e60
### Pickup  table=0x140161810  slots=13  sizeof=0x60  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x657241746e616c50 (string "PlantArea")
  reflected: ti=0x1401c6be0 create=0x14002bfc0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e050 Pickup_RegisterType
  [ 0] +0x00 0x14002c010  OWN Pickup_ScalarDtor (83 B)
  [ 1] +0x08 0x14002bfb0  TINY returns qword [0x1401c6be8]
  [ 2] +0x10 0x14002bfa0  TINY returns dword [0x1401c6be0] = TypeId(Pickup)
  [ 3] +0x18 0x1400f3aa0  OWN Pickup_Shutdown (171 B)
  [ 4] +0x20 0x1400f2f00  OWN Pickup_Serialize (2537 B)
  [ 5] +0x28 0x1400f38f0  OWN Pickup_Slot5 (239 B)
  [ 6] +0x30 0x1400f2ec0  OWN Pickup_Draw (46 B)
  [ 7] +0x38 0x1400f2e80  OWN Pickup_OnCreate (44 B)
  [ 8] +0x40 0x1400f39f0  OWN Pickup_OnRemove (74 B)
  [ 9] +0x48 0x1400f2e20  OWN Pickup_Update (85 B)
  [10] +0x50 0x1400f3a50  OWN Pickup_Slot10 (25 B)
  [11] +0x58 0x14002c000  OWN Pickup_Slot11 (10 B)
  [12] +0x60 0x1400f3a70  OWN Pickup_Slot12 (40 B)
  stamped by (5): this+0x0 at 0x14002c034 in 0x14002c010 Pickup_ScalarDtor; this+0x0 at 0x1400f3b79 in 0x1400f3b60 Work_Ctor18; this+0x0 at 0x1400f3bd5 in 0x1400f3ba0 Pickup_Ctor; this+0x0 at 0x1400f3c57 in 0x1400f3c00 Pickup_Ctor; other+0x0 at 0x140129258 in 0x1401291d0 RawMaterialComponent_Draw
### PickupRawMaterial  table=0x14015ff10  slots=10  sizeof=0x30  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x745370756b636950 (string "PickupStoredRawMaterial")
  reflected: ti=0x1401c6010 create=0x140022c60 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d2d0 PickupRawMaterial_RegisterType
  [ 0] +0x00 0x140022cd0  OWN PickupRawMaterial_ScalarDtor (104 B)
  [ 1] +0x08 0x140022c50  TINY returns qword [0x1401c6018]
  [ 2] +0x10 0x140022c40  TINY returns dword [0x1401c6010] = TypeId(PickupRawMaterial)
  [ 3] +0x18 0x1400c99e0  OWN PickupRawMaterial_Shutdown (207 B)
  [ 4] +0x20 0x1400c98d0  OWN PickupRawMaterial_Serialize (254 B)
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]   = parent
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400c4a30  TINY RET 0 (c2 00 00)  [in 7 tables]   = parent
  [ 9] +0x48 0x1400c9860  OWN PickupRawMaterial_Update (92 B)
  stamped by (5): other+0x0 at 0x140022cae in 0x140022c60 PickupRawMaterial_Factory; this+0x0 at 0x140022cf4 in 0x140022cd0 PickupRawMaterial_ScalarDtor; this+0x0 at 0x1400c9add in 0x1400c9ac0 Action_Action_Ctor12; this+0x0 at 0x1400c9b47 in 0x1400c9b10 PickupRawMaterial_Action_Ctor; this+0x0 at 0x1400c9bb7 in 0x1400c9b70 PickupRawMaterial_Action_CopyCtor
### PickupStoredRawMaterial  table=0x14015ff80  slots=10  sizeof=0x48  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x6e4165766f6d6552 (string "RemoveAnimal")
  reflected: ti=0x1401c6048 create=0x140022d60 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d310 PickupStoredRawMaterial_RegisterType
  [ 0] +0x00 0x140022dd0  OWN PickupStoredRawMaterial_ScalarDtor (124 B)
  [ 1] +0x08 0x140022d50  TINY returns qword [0x1401c6050]
  [ 2] +0x10 0x140022d40  TINY returns dword [0x1401c6048] = TypeId(PickupStoredRawMaterial)
  [ 3] +0x18 0x1400ca0d0  OWN PickupStoredRawMaterial_Shutdown (342 B)
  [ 4] +0x20 0x1400c9d40  OWN PickupStoredRawMaterial_Serialize (905 B)
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]   = parent
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400c9c70  OWN PickupStoredRawMaterial_OnRemove (190 B)
  [ 9] +0x48 0x1400c9bf0  OWN PickupStoredRawMaterial_Update (108 B)
  stamped by (5): other+0x0 at 0x140022dae in 0x140022d60 PickupStoredRawMaterial_Factory; this+0x0 at 0x140022df4 in 0x140022dd0 PickupStoredRawMaterial_ScalarDtor; this+0x0 at 0x1400ca3ed in 0x1400ca3d0 Action_Action_Ctor10; this+0x0 at 0x1400ca477 in 0x1400ca440 PickupStoredRawMaterial_Action_Ctor; this+0x0 at 0x1400ca56d in 0x1400ca520 PickupStoredRawMaterial_Action_CopyCtor
### PlacementTool  table=0x140161d20  slots=13  sizeof=0x70  parent="Tool" / structural Tool 0x140161f10  stop=non-code qword 0x502f676f6c616944 (string "Dialog/Pause.rsc")
  reflected: ti=0x1401c6e10 create=0x14002dc40 createInPlace=0x14002dc80 <no fn> registered in 0x14014e2d0 PlacementTool_RegisterType
  [ 0] +0x00 0x14002dc90  OWN PlacementTool_ScalarDtor (83 B)
  [ 1] +0x08 0x14002dc30  TINY returns qword [0x1401c6e18]
  [ 2] +0x10 0x14002dc20  TINY returns dword [0x1401c6e10] = TypeId(PlacementTool)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  [ 4] +0x20 0x1400fe580  OWN PlacementTool_Startup (628 B)
  [ 5] +0x28 0x1400fe4e0  OWN PlacementTool_Shutdown (147 B)
  [ 6] +0x30 0x1400fe2a0  OWN PlacementTool_Draw (552 B)
  [ 7] +0x38 0x1400fdee0  OWN PlacementTool_OnCreate (939 B)
  [ 8] +0x40 0x1400fdcc0  OWN PlacementTool_OnRemove (535 B)
  [ 9] +0x48 0x1400fe4d0  TINY return false (xor al,al)
  [10] +0x50 0x1400268a0  TINY return false (xor al,al)  [in 8 tables]   = parent
  [11] +0x58 0x140026920  OWN FencedTool_Slot11 (12 B)  [in 2 tables]
  [12] +0x60 0x1400fe800  TINY return 5 (mov eax)
  stamped by (3): this+0x0 at 0x14002dcb4 in 0x14002dc90 PlacementTool_ScalarDtor; this+0x0 at 0x1400fe829 in 0x1400fe810 Tool_Ctor9; this+0x0 at 0x1400fe888 in 0x1400fe850 PlacementTool_Ctor
### PlantArea  table=0x140161890  slots=13  sizeof=0x70  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x63724f746e616c50 (string "PlantOrchard")
  reflected: ti=0x1401c6c18 create=0x14002c470 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e090 PlantArea_RegisterType
  [ 0] +0x00 0x14002c4b0  OWN PlantArea_ScalarDtor (48 B)
  [ 1] +0x08 0x14002c460  TINY returns qword [0x1401c6c20]
  [ 2] +0x10 0x14002c450  TINY returns dword [0x1401c6c18] = TypeId(PlantArea)
  [ 3] +0x18 0x1400f4540  OWN PlantArea_Shutdown (172 B)
  [ 4] +0x20 0x1400f4010  OWN PlantArea_Serialize (1085 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400f3f60  OWN PlantArea_Draw (166 B)
  [ 7] +0x38 0x1400f3ea0  OWN PlantArea_OnCreate (172 B)
  [ 8] +0x40 0x1400f4460  OWN PlantArea_OnRemove (108 B)
  [ 9] +0x48 0x1400f3c70  OWN PlantArea_Update (553 B)
  [10] +0x50 0x1400f44e0  OWN PlantArea_Slot10 (75 B)
  [11] +0x58 0x140029860  OWN ClearArea_Slot11 (9 B)  [in 13 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (3): this+0x0 at 0x1400f4624 in 0x1400f4600 Work_Ctor19; this+0x0 at 0x1400f46d9 in 0x1400f46a0 PlantArea_Ctor; this+0x0 at 0x1400f47c7 in 0x1400f4770 PlantArea_Ctor
### PlantOrchard  table=0x140161910  slots=13  sizeof=0x70  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x61657241646e6554 (string "TendArea")
  reflected: ti=0x1401c6c50 create=0x14002c500 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e0d0 PlantOrchard_RegisterType
  [ 0] +0x00 0x14002c540  OWN PlantOrchard_ScalarDtor (48 B)
  [ 1] +0x08 0x14002c4f0  TINY returns qword [0x1401c6c58]
  [ 2] +0x10 0x14002c4e0  TINY returns dword [0x1401c6c50] = TypeId(PlantOrchard)
  [ 3] +0x18 0x1400f4f50  OWN PlantOrchard_Shutdown (172 B)
  [ 4] +0x20 0x1400f4af0  OWN PlantOrchard_Serialize (875 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400f4a90  OWN PlantOrchard_Draw (85 B)
  [ 7] +0x38 0x1400f4a20  OWN PlantOrchard_OnCreate (99 B)
  [ 8] +0x40 0x1400f4e70  OWN PlantOrchard_OnRemove (108 B)
  [ 9] +0x48 0x1400f47f0  OWN PlantOrchard_Update (553 B)
  [10] +0x50 0x1400f4ef0  OWN PlantOrchard_Slot10 (75 B)
  [11] +0x58 0x140029860  OWN ClearArea_Slot11 (9 B)  [in 13 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (3): this+0x0 at 0x1400f5034 in 0x1400f5010 Work_Ctor20; this+0x0 at 0x1400f5107 in 0x1400f50b0 PlantOrchard_Ctor; this+0x0 at 0x1400f5179 in 0x1400f5140 PlantOrchard_Ctor
### PlayAnimation  table=0x140158950  slots=10  sizeof=0x40  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x6c6f6f54776f6853 (string "ShowTool")
  reflected: ti=0x1401c3df0 create=0x1400011f0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014a580 PlayAnimation_RegisterType
  [ 0] +0x00 0x140001260  OWN PlayAnimation_ScalarDtor (104 B)
  [ 1] +0x08 0x1400011e0  TINY returns qword [0x1401c3df8]
  [ 2] +0x10 0x1400011d0  TINY returns dword [0x1401c3df0] = TypeId(PlayAnimation)
  [ 3] +0x18 0x140042480  OWN PlayAnimation_Shutdown (208 B)
  [ 4] +0x20 0x140042420  OWN PlayAnimation_Serialize (88 B)
  [ 5] +0x28 0x140042400  OWN PlayAnimation_Slot5 (21 B)
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400423f0  TINY RET 0 (c2 00 00)
  [ 9] +0x48 0x1400423b0  OWN PlayAnimation_Update (46 B)
  stamped by (8): other+0x0 at 0x14000123e in 0x1400011f0 PlayAnimation_Factory; this+0x0 at 0x140001284 in 0x140001260 PlayAnimation_ScalarDtor; this+0x0 at 0x14004257d in 0x140042560 Action_Action_Ctor; this+0x0 at 0x1400425f7 in 0x1400425b0 PlayAnimation_Action_CopyCtor; this+0x0 at 0x140042667 in 0x140042630 PlayAnimation_Action_Ctor3; other+0x0 at 0x1400ba8fe in 0x1400ba6b0 PlayAnimation_Action_Ctor4; other+0x0 at 0x1400c265a in 0x1400c2490 PlayAnimation_Action_Ctor; other+0x0 at 0x1400c282b in 0x1400c26c0 PlayAnimation_Action_Ctor5
### PopulationComponent  table=0x14015a360  slots=16  sizeof=0x70  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c4790 create=0x14000af50 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b580 PopulationComponent_RegisterType
  [ 0] +0x00 0x14000b0a0  OWN PopulationComponent_ScalarDtor (48 B)
  [ 1] +0x08 0x14000af40  TINY returns qword [0x1401c4798]
  [ 2] +0x10 0x14000af30  TINY returns dword [0x1401c4790] = TypeId(PopulationComponent)
  [ 3] +0x18 0x1400544a0  OWN PopulationComponent_Shutdown (431 B)
  [ 4] +0x20 0x140054220  OWN PopulationComponent_Serialize (627 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x1400541c0  OWN PopulationComponent_Draw (59 B)
  [ 7] +0x38 0x140054180  OWN PopulationComponent_OnCreate (54 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140054070  OWN PopulationComponent_Update (256 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140054210  TINY RET 0 (c2 00 00)
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (2): this+0x0 at 0x140054682 in 0x140054660 PopulationComponent_Ctor; this+0x0 at 0x1400546f8 in 0x1400546d0 PopulationComponent_Ctor
### PopulationDescription  table=0x14015a3e0  slots=10  sizeof=0x58  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6974616c75706f50 (string "PopulationComponent")
  reflected: ti=0x1401c47c8 create=0x14000afb0 createInPlace=0x14000aff0 PopulationDescription_CreateInPlace registered in 0x14014b5c0 PopulationDescription_Register
  [ 0] +0x00 0x14000b030  OWN PopulationComponent_vslot16 (97 B)
  [ 1] +0x08 0x14000afa0  TINY returns qword [0x1401c47d0]
  [ 2] +0x10 0x14000af90  TINY returns dword [0x1401c47c8] = TypeId(PopulationDescription)
  [ 3] +0x18 0x1400548e0  TINY return 3 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400547e0  OWN FUN_1400547e0 (250 B)
  [ 6] +0x30 0x14000b000  OWN FUN_14000b000 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14000b020  OWN (no Ghidra function)
  [ 9] +0x48 0x14000aff0  OWN PopulationDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x14000b054 in 0x14000b030 PopulationComponent_vslot16; this+0x0 at 0x14005490d in 0x1400548f0 FUN_1400548f0; this+0x0 at 0x14005496e in 0x140054940 PopulationDescription_sub_4940
### PopulationGoal  table=0x140160370  slots=11  sizeof=0x38  parent="Achievement::Goal" / structural Achievement::Goal (Runtime ctor import)  stop=non-code qword 0x6d65766569686341 (string "Achievement::GoalInstance")
  reflected: ti=0x1401c6208 create=0x140025f00 createInPlace=0x140025f60 PopulationGoal_Cleanup registered in 0x14014d510 PopulationGoal_RegisterType
  [ 0] +0x00 0x140026000  OWN PopulationGoal_ScalarDtor (73 B)
  [ 1] +0x08 0x140025ef0  TINY returns qword [0x1401c6210]
  [ 2] +0x10 0x140025ee0  TINY returns dword [0x1401c6208] = TypeId(PopulationGoal)
  [ 3] +0x18 0x1400d78e0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400d7870  OWN PopulationGoal_Slot5 (93 B)
  [ 6] +0x30 0x140025fb0  OWN PopulationGoal_Draw (66 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400d7820  OWN PopulationGoal_OnRemove (73 B)
  [ 9] +0x48 0x14003ab5e  IMPORT ?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ  [in 3 tables]
  [10] +0x50 0x140025f60  OWN PopulationGoal_Cleanup (65 B)
  stamped by (6): other+0x0 at 0x140025f3b in 0x140025f00 PopulationGoal_Factory; this+0x0 at 0x140025f8a in 0x140025f60 PopulationGoal_Cleanup; this+0x0 at 0x140025fdf in 0x140025fb0 PopulationGoal_Draw; this+0x0 at 0x140026024 in 0x140026000 PopulationGoal_ScalarDtor; this+0x0 at 0x1400d7909 in 0x1400d78f0 PopulationGoal_Ctor; this+0x0 at 0x1400d7944 in 0x1400d7920 PopulationGoal_CopyCtor
### ProductionUI  table=0x14015d070  slots=11  sizeof=0x50  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x69737365666f7250 (string "ProfessionUI")
  reflected: ti=0x1401c5440 create=0x140013230 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014c540 RegisterType_ProductionUI
  [ 0] +0x00 0x1400132b0  OWN ProductionUI_ScalarDtor (102 B)
  [ 1] +0x08 0x140013220  TINY returns qword [0x1401c5448]
  [ 2] +0x10 0x140013210  TINY returns dword [0x1401c5440] = TypeId(ProductionUI)
  [ 3] +0x18 0x140076300  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x140075da0  OWN ProductionUI_Serialize (1358 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x140075a20  OWN ProductionUI_OnCreate (34 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14003a972  IMPORT ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z  [in 11 tables]
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x14001326b in 0x140013230 Factory_ProductionUI; this+0x0 at 0x1400132d4 in 0x1400132b0 ProductionUI_ScalarDtor; this+0x0 at 0x14007632d in 0x140076310 FUN_140076310; this+0x0 at 0x140076384 in 0x140076360 ProductionUI_Ctor
### Profession  table=0x14015efa0  slots=9  sizeof=0x60  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6f6972616e656353 (string "Scenario")
  reflected: ti=0x1401c5a98 create=0x14001e530 createInPlace=0x14001e570 Profession_OnRemove registered in 0x14014cc90 Profession_RegisterType
  [ 0] +0x00 0x14001e6f0  OWN ProfessionList_Slot10 (99 B)
  [ 1] +0x08 0x14001e520  TINY returns qword [0x1401c5aa0]
  [ 2] +0x10 0x14001e510  TINY returns dword [0x1401c5a98] = TypeId(Profession)
  [ 3] +0x18 0x1400a9020  TINY return 3 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400a8f10  OWN ProfessionList_Slot15 (262 B)
  [ 6] +0x30 0x14001e580  OWN Profession_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14001e570  OWN Profession_OnRemove (12 B)
  stamped by (1): this+0x0 at 0x1400a9054 in 0x1400a9030 Profession_Ctor
### ProfessionList  table=0x14015ef50  slots=9  sizeof=0x50  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=zero qword
  reflected: ti=0x1401c5ad0 create=0x14001e5c0 createInPlace=0x14001e630 ProfessionList_Cleanup registered in 0x14014ccd0 ProfessionList_RegisterType
  [ 0] +0x00 0x14001e760  OWN ProfessionList_ScalarDtor (86 B)
  [ 1] +0x08 0x14001e5b0  TINY returns qword [0x1401c5ad8]
  [ 2] +0x10 0x14001e5a0  TINY returns dword [0x1401c5ad0] = TypeId(ProfessionList)
  [ 3] +0x18 0x1400a8eb0  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400a8e30  OWN ProfessionList_Slot5 (109 B)
  [ 6] +0x30 0x14001e690  OWN ProfessionList_Draw (85 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14001e630  OWN ProfessionList_Cleanup (84 B)
  stamped by (4): other+0x0 at 0x14001e5fb in 0x14001e5c0 ProfessionList_Factory; this+0x0 at 0x14001e65a in 0x14001e630 ProfessionList_Cleanup; this+0x0 at 0x14001e6bf in 0x14001e690 ProfessionList_Draw; this+0x0 at 0x1400a8ee4 in 0x1400a8ec0 ProfessionList_Ctor
### ProfessionUI  table=0x14015d0f0  slots=11  sizeof=0x78  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x4955737569646152 (string "RadiusUI")
  reflected: ti=0x1401c5478 create=0x140013400 createInPlace=0x140013440 <no fn> registered in 0x14014c580 RegisterType_ProfessionUI
  [ 0] +0x00 0x140013450  OWN ProfessionUI_ScalarDtor (48 B)
  [ 1] +0x08 0x1400133f0  TINY returns qword [0x1401c5480]
  [ 2] +0x10 0x1400133e0  TINY returns dword [0x1401c5478] = TypeId(ProfessionUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x14003a984  IMPORT ?OnCreated@DialogController@UI@@UEAAXXZ  [in 5 tables]
  [ 5] +0x28 0x140076ca0  OWN ProfessionUI_Slot5 (2091 B)
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14003a97e  IMPORT ?OnUpdateLayout@DialogController@UI@@UEAAXXZ  [in 10 tables]
  [ 8] +0x40 0x1400765a0  OWN ProfessionUI_OnRemove (1152 B)
  [ 9] +0x48 0x140076c10  OWN ProfessionUI_Update (133 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (2): this+0x0 at 0x140077507 in 0x1400774e0 ProfessionUI_Ctor; this+0x0 at 0x1400775c4 in 0x1400775a0 ProfessionUI_Ctor
### QuarryComponent  table=0x14015a490  slots=16  sizeof=0x28  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c4800 create=0x14000b3d0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b600 QuarryComponent_RegisterType
  [ 0] +0x00 0x14000b5d0  OWN QuarryComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x14000b3c0  TINY returns qword [0x1401c4808]
  [ 2] +0x10 0x14000b3b0  TINY returns dword [0x1401c4800] = TypeId(QuarryComponent)
  [ 3] +0x18 0x140055230  OWN QuarryComponent_Shutdown (48 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140055220  TINY RET 0 (c2 00 00)
  [ 7] +0x38 0x140055210  TINY RET 0 (c2 00 00)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x1400550f0  OWN QuarryComponent_Slot12 (268 B)
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140054b80  OWN QuarryComponent_Slot15 (1383 B)
  stamped by (3): this+0x0 at 0x14000b5f4 in 0x14000b5d0 QuarryComponent_ScalarDtor; this+0x0 at 0x140055289 in 0x140055270 QuarryComponent_Ctor; this+0x0 at 0x1400552d8 in 0x1400552b0 QuarryComponent_Ctor
### QuarryDescription  table=0x14015a510  slots=10  sizeof=0x30  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6544737569646152 (string "RadiusDescription")
  reflected: ti=0x1401c4838 create=0x14000b430 createInPlace=0x14000b4a0 QuarryDescription_CreateInPlace registered in 0x14014b640 QuarryDescription_Register
  [ 0] +0x00 0x14000b570  OWN QuarryComponent_vslot16 (83 B)
  [ 1] +0x08 0x14000b420  TINY returns qword [0x1401c4840]
  [ 2] +0x10 0x14000b410  TINY returns dword [0x1401c4838] = TypeId(QuarryDescription)
  [ 3] +0x18 0x140055390  TINY return 4 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140055320  OWN FUN_140055320 (103 B)
  [ 6] +0x30 0x14000b500  OWN FUN_14000b500 (91 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14000b560  OWN (no Ghidra function)
  [ 9] +0x48 0x14000b4a0  OWN QuarryDescription_CreateInPlace (90 B)
  stamped by (6): other+0x0 at 0x14000b475 in 0x14000b430 QuarryDescription_Factory; this+0x0 at 0x14000b4d4 in 0x14000b4a0 QuarryDescription_CreateInPlace; this+0x0 at 0x14000b539 in 0x14000b500 FUN_14000b500; this+0x0 at 0x14000b594 in 0x14000b570 QuarryComponent_vslot16; this+0x0 at 0x1400553b9 in 0x1400553a0 FUN_1400553a0; this+0x0 at 0x14005540e in 0x1400553e0 FUN_1400553e0
### RadiusDescription  table=0x14015a580  slots=10  sizeof=0x30  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x65447463656c6553 (string "SelectDescription")
  reflected: ti=0x1401c4870 create=0x14000b9e0 createInPlace=0x14000ba50 RadiusDescription_CreateInPlace registered in 0x14014b680 RadiusDescription_RegisterType
  [ 0] +0x00 0x14000bb10  OWN RadiusDescription_ScalarDtor (83 B)
  [ 1] +0x08 0x14000b9d0  TINY returns qword [0x1401c4878]
  [ 2] +0x10 0x14000b9c0  TINY returns dword [0x1401c4870] = TypeId(RadiusDescription)
  [ 3] +0x18 0x140055590  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140055530  OWN RadiusDescription_Slot5 (85 B)
  [ 6] +0x30 0x14000bab0  OWN RadiusDescription_Draw (84 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]
  [ 9] +0x48 0x14000ba50  OWN RadiusDescription_CreateInPlace (83 B)
  stamped by (6): other+0x0 at 0x14000ba25 in 0x14000b9e0 RadiusDescription_Factory; this+0x0 at 0x14000ba84 in 0x14000ba50 RadiusDescription_CreateInPlace; this+0x0 at 0x14000bae9 in 0x14000bab0 RadiusDescription_Draw; this+0x0 at 0x14000bb34 in 0x14000bb10 RadiusDescription_ScalarDtor; this+0x0 at 0x1400555b9 in 0x1400555a0 RadiusDescription_Ctor; this+0x0 at 0x14005560e in 0x1400555e0 RadiusDescription_CopyCtor
### RadiusUI  table=0x14015d160  slots=11  sizeof=0x38  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x656372756f736552 (string "ResourceLimitUI")
  reflected: ti=0x1401c54b0 create=0x140013590 createInPlace=0x1400135f0 Cleanup_RadiusUI registered in 0x14014c5c0 RegisterType_RadiusUI
  [ 0] +0x00 0x140013640  OWN RadiusUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140013580  TINY returns qword [0x1401c54b8]
  [ 2] +0x10 0x140013570  TINY returns dword [0x1401c54b0] = TypeId(RadiusUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x14003a984  IMPORT ?OnCreated@DialogController@UI@@UEAAXXZ  [in 5 tables]
  [ 5] +0x28 0x140077650  OWN RadiusUI_Slot5 (280 B)
  [ 6] +0x30 0x140077600  OWN RadiusUI_Draw (61 B)
  [ 7] +0x38 0x14003a97e  IMPORT ?OnUpdateLayout@DialogController@UI@@UEAAXXZ  [in 10 tables]
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14003a972  IMPORT ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z  [in 11 tables]
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (5): other+0x0 at 0x1400135cb in 0x140013590 Factory_RadiusUI; this+0x0 at 0x14001361a in 0x1400135f0 Cleanup_RadiusUI; this+0x0 at 0x140013664 in 0x140013640 RadiusUI_ScalarDtor; this+0x0 at 0x140077789 in 0x140077770 RadiusUI_Ctor; this+0x0 at 0x1400777c4 in 0x1400777a0 RadiusUI_CopyCtor
### RawMaterialComponent  table=0x140164170  slots=16  sizeof=0x40  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7c80 create=0x140037270 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f750 RawMaterialComponent_RegisterType
  [ 0] +0x00 0x1400373c0  OWN RawMaterialComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x140037260  TINY returns qword [0x1401c7c88]
  [ 2] +0x10 0x140037250  TINY returns dword [0x1401c7c80] = TypeId(RawMaterialComponent)
  [ 3] +0x18 0x140129330  OWN RawMaterialComponent_Shutdown (138 B)
  [ 4] +0x20 0x1401292c0  OWN RawMaterialComponent_Serialize (93 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x1401291d0  OWN RawMaterialComponent_Draw (219 B)
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140129130  OWN RawMaterialComponent_Update (148 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x1400373e4 in 0x1400373c0 RawMaterialComponent_ScalarDtor; this+0x0 at 0x1401293d9 in 0x1401293c0 RawMaterialComponent_Ctor; this+0x0 at 0x140129428 in 0x140129400 RawMaterialComponent_Ctor
### RawMaterialDescription  table=0x1401641f0  slots=10  sizeof=0x60  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x636e656469736552 (string "ResidenceDescription")
  reflected: ti=0x1401c7cb8 create=0x1400372d0 createInPlace=0x140037310 RawMaterialDescription_CreateInPlace registered in 0x14014f790 RawMaterialDescription_Register
  [ 0] +0x00 0x140037350  OWN RawMaterialDescription_ScalarDtor (108 B)
  [ 1] +0x08 0x1400372c0  TINY returns qword [0x1401c7cc0]
  [ 2] +0x10 0x1400372b0  TINY returns dword [0x1401c7cb8] = TypeId(RawMaterialDescription)
  [ 3] +0x18 0x140129630  TINY return 8 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140129520  OWN RawMaterialDescription_Slot5 (258 B)
  [ 6] +0x30 0x140037320  OWN RawMaterialDescription_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140037340  OWN RawMaterialDescription_OnRemove (12 B)
  [ 9] +0x48 0x140037310  OWN RawMaterialDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x140037374 in 0x140037350 RawMaterialDescription_ScalarDtor; this+0x0 at 0x14012965d in 0x140129640 RawMaterialDescription_Ctor; this+0x0 at 0x1401296ce in 0x1401296a0 RawMaterialDescription_CopyCtor
### RemovalTool  table=0x140161de0  slots=15  sizeof=0x40  parent="DragTool" / structural DragTool 0x140161c10  stop=non-code qword 0x6c6f6f5464616f52 (string "RoadTool")
  reflected: ti=0x1401c6e48 create=0x14002e7a0 createInPlace=0x14002e7e0 <no fn> registered in 0x14014e310 RemovalTool_RegisterType
  [ 0] +0x00 0x14002e7f0  OWN RemovalTool_ScalarDtor (93 B)
  [ 1] +0x08 0x14002e790  TINY returns qword [0x1401c6e50]
  [ 2] +0x10 0x14002e780  TINY returns dword [0x1401c6e48] = TypeId(RemovalTool)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  [ 4] +0x20 0x140104bd0  OWN RemovalTool_Serialize (146 B)
  [ 5] +0x28 0x140104bc0  TINY RET 0 (c2 00 00)
  [ 6] +0x30 0x1400fc6b0  OWN BridgeTool_Draw (785 B)  [in 4 tables]   = parent
  [ 7] +0x38 0x140104bb0  TINY RET 0 (c2 00 00)
  [ 8] +0x40 0x140104050  OWN RemovalTool_OnRemove (1436 B)
  [ 9] +0x48 0x1400fc640  OWN BridgeTool_Update (103 B)  [in 5 tables]   = parent
  [10] +0x50 0x1400268a0  TINY return false (xor al,al)  [in 8 tables]   = parent
  [11] +0x58 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]   = parent
  [12] +0x60 0x1400fc9d0  TINY return 5 (mov eax)  [in 6 tables]   = parent
  [13] +0x68 0x1401035e0  OWN RemovalTool_Slot13 (2306 B)
  [14] +0x70 0x140103ef0  OWN RemovalTool_Slot14 (227 B)
  stamped by (3): this+0x0 at 0x14002e814 in 0x14002e7f0 RemovalTool_ScalarDtor; this+0x0 at 0x140104c89 in 0x140104c70 Tool_Ctor6; this+0x0 at 0x140104d15 in 0x140104cc0 RemovalTool_Ctor
### RemoveAnimal  table=0x14015ffe0  slots=10  sizeof=0x28  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x614e65766f6d6552 (string "RemoveNaturalResource")
  reflected: ti=0x1401c6080 create=0x140022e70 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d350 RemoveAnimal_RegisterType
  [ 0] +0x00 0x140022ee0  OWN RemoveAnimal_ScalarDtor (104 B)
  [ 1] +0x08 0x140022e60  TINY returns qword [0x1401c6088]
  [ 2] +0x10 0x140022e50  TINY returns dword [0x1401c6080] = TypeId(RemoveAnimal)
  [ 3] +0x18 0x1400ca870  OWN RemoveAnimal_Shutdown (159 B)
  [ 4] +0x20 0x1400ca7f0  OWN RemoveAnimal_Serialize (112 B)
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]   = parent
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400ca760  OWN RemoveAnimal_OnRemove (128 B)
  [ 9] +0x48 0x1400ca720  OWN RemoveAnimal_Update (46 B)
  stamped by (6): other+0x0 at 0x140022ec1 in 0x140022e70 RemoveAnimal_Factory; this+0x0 at 0x140022f04 in 0x140022ee0 RemoveAnimal_ScalarDtor; other+0x0 at 0x1400bc2a6 in 0x1400bbea0 RemoveAnimal_Action_Ctor; this+0x0 at 0x1400ca93d in 0x1400ca920 Action_Action_Ctor6; this+0x0 at 0x1400ca9a7 in 0x1400ca970 RemoveAnimal_Action_CopyCtor; this+0x0 at 0x1400ca9ff in 0x1400ca9c0 RemoveAnimal_Action_Ctor3
### RemoveNaturalResource  table=0x140160050  slots=10  sizeof=0x38  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x706d6f43646e6553 (string "SendComponentMessage")
  reflected: ti=0x1401c60b8 create=0x140022f70 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d390 RemoveNaturalResource_RegisterType
  [ 0] +0x00 0x140022fe0  OWN RemoveNaturalResource_ScalarDtor (104 B)
  [ 1] +0x08 0x140022f60  TINY returns qword [0x1401c60c0]
  [ 2] +0x10 0x140022f50  TINY returns dword [0x1401c60b8] = TypeId(RemoveNaturalResource)
  [ 3] +0x18 0x1400cac50  OWN RemoveNaturalResource_Shutdown (226 B)
  [ 4] +0x20 0x1400cab10  OWN RemoveNaturalResource_Serialize (300 B)
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]   = parent
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400caaa0  OWN RemoveNaturalResource_OnRemove (101 B)
  [ 9] +0x48 0x1400caa20  OWN RemoveNaturalResource_Update (108 B)
  stamped by (6): other+0x0 at 0x140022fbe in 0x140022f70 RemoveNaturalResource_Factory; this+0x0 at 0x140023004 in 0x140022fe0 RemoveNaturalResource_ScalarDtor; other+0x0 at 0x1400c178b in 0x1400c1530 ShowTool_Action_Ctor4; this+0x0 at 0x1400cad5d in 0x1400cad40 Action_Action_Ctor8; this+0x0 at 0x1400cadc7 in 0x1400cad90 RemoveNaturalResource_Action_Ctor; this+0x0 at 0x1400cae40 in 0x1400cadf0 RemoveNaturalResource_Action_CopyCtor
### ResidenceComponent  table=0x1401642c0  slots=18  sizeof=0x60  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c7cf0 create=0x140037440 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f7d0 ResidenceComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x140164270
  [ 0] +0x00 0x140037610  OWN ResidenceComponent_Controller_Slot10 (125 B)
  [ 1] +0x08 0x140037430  TINY returns qword [0x1401c7cf8]
  [ 2] +0x10 0x140037420  TINY returns dword [0x1401c7cf0] = TypeId(ResidenceComponent)
  [ 3] +0x18 0x14012a170  OWN ResidenceComponent_Controller_Slot13 (183 B)
  [ 4] +0x20 0x14012a120  OWN ResidenceComponent_Controller_Slot14 (62 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x14012a0e0  OWN ResidenceComponent_OnRemove (57 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (3): this+0x0 at 0x140037634 in 0x140037610 ResidenceComponent_Controller_Slot10; this+0x0 at 0x14012a24d in 0x14012a230 ResidenceComponent_Ctor; this+0x0 at 0x14012a2d2 in 0x14012a2a0 ResidenceComponent_Ctor
### ResidenceDescription  table=0x140164350  slots=10  sizeof=0x30  parent="Description" / structural Description 0x1401629a0  stop=next table
  reflected: ti=0x1401c7d28 create=0x1400374a0 createInPlace=0x140037500 ResidenceDescription_CreateInPlace registered in 0x14014f810 ResidenceDescription_Register
  [ 0] +0x00 0x1400375b0  OWN ResidenceComponent_vslot18 (83 B)
  [ 1] +0x08 0x140037490  TINY returns qword [0x1401c7d30]
  [ 2] +0x10 0x140037480  TINY returns dword [0x1401c7d28] = TypeId(ResidenceDescription)
  [ 3] +0x18 0x14012a460  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14012a3d0  OWN FUN_14012a3d0 (125 B)
  [ 6] +0x30 0x140037550  OWN FUN_140037550 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400375a0  OWN (no Ghidra function)
  [ 9] +0x48 0x140037500  OWN ResidenceDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x1400374e5 in 0x1400374a0 ResidenceDescription_Factory; this+0x0 at 0x140037534 in 0x140037500 ResidenceDescription_CreateInPlace; this+0x0 at 0x140037589 in 0x140037550 FUN_140037550; this+0x0 at 0x1400375d4 in 0x1400375b0 ResidenceComponent_vslot18; this+0x0 at 0x14012a489 in 0x14012a470 FUN_14012a470; this+0x0 at 0x14012a4de in 0x14012a4b0 FUN_14012a4b0
### ResidenceUI  table=0x14015f6d0  slots=11  sizeof=0x40  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x64616f4c65766153 (string "SaveLoadDialog")
  reflected: ti=0x1401c5c90 create=0x140020f60 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014ced0 RegisterType_ResidenceUI
  [ 0] +0x00 0x140020fc0  OWN ResidenceUI_ScalarDtor (96 B)
  [ 1] +0x08 0x140020f50  TINY returns qword [0x1401c5c98]
  [ 2] +0x10 0x140020f40  TINY returns dword [0x1401c5c90] = TypeId(ResidenceUI)
  [ 3] +0x18 0x1400b5db0  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x1400b5850  OWN ResidenceUI_Serialize (1355 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x1400b51f0  OWN ResidenceUI_Slot7_PopulateRoster (1190 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x1400b5820  OWN ResidenceUI_Slot9 (42 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x140020f9b in 0x140020f60 Factory_ResidenceUI; this+0x0 at 0x140020fe4 in 0x140020fc0 ResidenceUI_ScalarDtor; this+0x0 at 0x1400b5ddd in 0x1400b5dc0 ResidenceUI_Ctor; this+0x0 at 0x1400b5e34 in 0x1400b5e10 ResidenceUI_CopyCtor
### ResourceLimitUI  table=0x14015d1f0  slots=11  sizeof=0x40  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=zero qword
  reflected: ti=0x1401c54e8 create=0x1400136b0 createInPlace=0x140013710 Cleanup_ResourceLimitUI registered in 0x14014c600 RegisterType_ResourceLimitUI
  [ 0] +0x00 0x1400138d0  OWN ResourceLimitUI_ScalarDtor (73 B)
  [ 1] +0x08 0x1400136a0  TINY returns qword [0x1401c54f0]
  [ 2] +0x10 0x140013690  TINY returns dword [0x1401c54e8] = TypeId(ResourceLimitUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x140077970  OWN ResourceLimitUI_Serialize (298 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x1400778d0  OWN ResourceLimitUI_OnCreate (153 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x1400777e0  OWN ResourceLimitUI_Update (219 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (5): other+0x0 at 0x1400136eb in 0x1400136b0 Factory_ResourceLimitUI; this+0x0 at 0x14001373a in 0x140013710 Cleanup_ResourceLimitUI; this+0x0 at 0x1400138f4 in 0x1400138d0 ResourceLimitUI_ScalarDtor; this+0x0 at 0x140077ab9 in 0x140077aa0 ResourceLimitUI_Ctor; this+0x0 at 0x140077af4 in 0x140077ad0 ResourceLimitUI_CopyCtor
### ResourceLimitUIConfig  table=0x14015d250  slots=9  sizeof=0x28  parent="UI::DialogControllerConfig" / structural UI::DialogControllerConfig (Runtime ctor import)  stop=non-code qword 0x6f6972616e656353 (string "ScenarioDialog")
  reflected: ti=0x1401c5520 create=0x140013780 createInPlace=0x1400137e0 ResourceLimitUIConfig_CreateInPlace registered in 0x14014c640 ResourceLimitUIConfig_Register
  [ 0] +0x00 0x140013880  OWN ResourceLimitUI_Slot12 (73 B)
  [ 1] +0x08 0x140013770  TINY returns qword [0x1401c5528]
  [ 2] +0x10 0x140013760  TINY returns dword [0x1401c5520] = TypeId(ResourceLimitUIConfig)
  [ 3] +0x18 0x140077b60  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x140077b10  OWN ResourceLimitUI_vslot17 (74 B)
  [ 6] +0x30 0x140013830  OWN ResourceLimitUI_vslot18 (66 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400137e0  OWN ResourceLimitUIConfig_CreateInPlace (65 B)
  stamped by (6): other+0x0 at 0x1400137bb in 0x140013780 ResourceLimitUIConfig_Factory; this+0x0 at 0x14001380a in 0x1400137e0 ResourceLimitUIConfig_CreateInPlace; this+0x0 at 0x14001385f in 0x140013830 ResourceLimitUI_vslot18; this+0x0 at 0x1400138a4 in 0x140013880 ResourceLimitUI_Slot12; this+0x0 at 0x140077b89 in 0x140077b70 FUN_140077b70; this+0x0 at 0x140077bc4 in 0x140077ba0 FUN_140077ba0
### ResourceProduceComponent  table=0x1401643f0  slots=22  sizeof=abstract  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=non-code qword 0x63614d6574617453 (string "StateMachineDescription")
  reflected: ti=0x1401c7d60 create=0x140003730 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f8d0 ResourceProduceComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x1401643a0
  [ 0] +0x00 0x140037d40  OWN ResourceProduceComponent_Controller_Slot10 (115 B)
  [ 1] +0x08 0x140037d30  TINY returns qword [0x1401c7d68]
  [ 2] +0x10 0x140037d20  TINY returns dword [0x1401c7d60] = TypeId(ResourceProduceComponent)
  [ 3] +0x18 0x14012b4a0  OWN ResourceProduceComponent_Controller_Slot13 (118 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x14012b520  OWN HerbalistComponent_Slot13 (112 B)  [in 6 tables]
  [14] +0x70 0x14012b5a0  OWN HerbalistComponent_Slot14 (117 B)  [in 6 tables]
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  [18] +0x90 0x14003addc  _purecall  [in 12 tables]
  [19] +0x98 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]
  [20] +0xa0 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]
  [21] +0xa8 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]
  stamped by (14): this+0x0 at 0x140006d8e in 0x140006d60 HerbalistComponent_ScalarDtor; this+0x0 at 0x14000846e in 0x140008440 MarketComponent_ScalarDtor; this+0x0 at 0x14003458e in 0x140034560 FishingComponent_ScalarDtor; this+0x0 at 0x140034a9e in 0x140034a70 ForesterComponent_ScalarDtor; this+0x0 at 0x14003512e in 0x140035100 HuntComponent_ScalarDtor; this+0x0 at 0x140037d64 in 0x140037d40 ResourceProduceComponent_Controller_Slot10; this+0x0 at 0x14004b263 in 0x14004b240 ResourceProduceComponent_Ctor; this+0x0 at 0x14004eb33 in 0x14004eb10 ResourceProduceComponent_CopyCtor; this+0x0 at 0x14011900b in 0x140118fd0 ResourceProduceComponent_Ctor3; this+0x0 at 0x14011f0f3 in 0x14011f0d0 ResourceProduceComponent_Ctor4; this+0x0 at 0x14011fa03 in 0x14011f9e0 ResourceProduceComponent_Ctor5; this+0x0 at 0x1401228a3 in 0x140122880 ResourceProduceComponent_Ctor6 ...
### RoadTool  table=0x140161e70  slots=15  sizeof=0x48  parent="DragTool" / structural DragTool 0x140161c10  stop=next table
  reflected: ti=0x1401c6e80 create=0x14002e870 createInPlace=0x14002e8b0 <no fn> registered in 0x14014e350 RoadTool_RegisterType
  [ 0] +0x00 0x14002e8c0  OWN RoadTool_ScalarDtor (93 B)
  [ 1] +0x08 0x14002e860  TINY returns qword [0x1401c6e88]
  [ 2] +0x10 0x14002e850  TINY returns dword [0x1401c6e80] = TypeId(RoadTool)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  [ 4] +0x20 0x1401056e0  OWN RoadTool_Serialize (146 B)
  [ 5] +0x28 0x1401056d0  TINY RET 0 (c2 00 00)
  [ 6] +0x30 0x140105690  OWN RoadTool_Draw (56 B)
  [ 7] +0x38 0x140105680  TINY RET 0 (c2 00 00)
  [ 8] +0x40 0x140105190  OWN RoadTool_OnRemove (1254 B)
  [ 9] +0x48 0x1400fc640  OWN BridgeTool_Update (103 B)  [in 5 tables]   = parent
  [10] +0x50 0x1400268a0  TINY return false (xor al,al)  [in 8 tables]   = parent
  [11] +0x58 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]   = parent
  [12] +0x60 0x1400fc9d0  TINY return 5 (mov eax)  [in 6 tables]   = parent
  [13] +0x68 0x140104d30  OWN RoadTool_Slot13 (670 B)
  [14] +0x70 0x140104fe0  OWN RoadTool_Slot14 (426 B)
  stamped by (3): this+0x0 at 0x14002e8e4 in 0x14002e8c0 RoadTool_ScalarDtor; this+0x0 at 0x140105799 in 0x140105780 Tool_Ctor5; this+0x0 at 0x140105825 in 0x1401057d0 RoadTool_Ctor
### RotationSetting  table=0x140160710  slots=4  sizeof=0x18  parent="ToolSetting" / structural ToolSetting 0x140161ee8  stop=non-code qword 0x6c6169726f747554 (string "Tutorial")
  reflected: ti=0x1401c63c8 create=0x140026d20 createInPlace=0x140026d80 RotationSetting_Cleanup registered in 0x14014d710 RotationSetting_RegisterType
  [ 0] +0x00 0x140026dd0  OWN RotationSetting_ScalarDtor (83 B)
  [ 1] +0x08 0x140026d10  TINY returns qword [0x1401c63d0]
  [ 2] +0x10 0x140026d00  TINY returns dword [0x1401c63c8] = TypeId(RotationSetting)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]   = parent
  stamped by (5): other+0x0 at 0x140026d65 in 0x140026d20 RotationSetting_Factory; this+0x0 at 0x140026db4 in 0x140026d80 RotationSetting_Cleanup; this+0x0 at 0x140026df4 in 0x140026dd0 RotationSetting_ScalarDtor; this+0x0 at 0x1400dea59 in 0x1400dea40 RotationSetting_Ctor; this+0x0 at 0x1400deaae in 0x1400dea80 RotationSetting_CopyCtor
### SaveLoadDialog  table=0x14015f900  slots=10  sizeof=0x160  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x55656761726f7453 (string "StorageUI")
  reflected: ti=0x1401c5cc8 create=0x140021160 createInPlace=0x1400211a0 <no fn> registered in 0x14014cf10 RegisterType_SaveLoadDialog
  [ 0] +0x00 0x1400211b0  OWN SaveLoadDialog_ScalarDtor (48 B)
  [ 1] +0x08 0x140021150  TINY returns qword [0x1401c5cd0]
  [ 2] +0x10 0x140021140  TINY returns dword [0x1401c5cc8] = TypeId(SaveLoadDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x1400b7d00  OWN SaveLoadDialog_Serialize (987 B)
  [ 5] +0x28 0x14003a94e  IMPORT ?OnClosed@DialogInstance@UI@@UEAAXXZ  [in 12 tables]
  [ 6] +0x30 0x1400b6880  OWN SaveLoadDialog_Draw (1113 B)
  [ 7] +0x38 0x1400b79b0  OWN SaveLoadDialog_OnCreate (835 B)
  [ 8] +0x40 0x1400b67e0  OWN SaveLoadDialog_OnRemove (149 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (3): this+0x0 at 0x1400b8117 in 0x1400b80f0 SaveLoadDialog_Ctor; this+0x0 at 0x1400b81e4 in 0x1400b81c0 SaveLoadDialog_Ctor; this+0x0 at 0x1400b82b2 in 0x1400b8270 SaveLoadDialog_CopyCtor
### Scenario  table=0x14015f020  slots=9  sizeof=0x90  parent="::Achievement::Achievement" / structural Achievement::Achievement (Runtime ctor import)  stop=zero qword
  reflected: ti=0x1401c5b08 create=0x14001e9f0 createInPlace=0x14001ea70 Scenario_Cleanup registered in 0x14014cd10 Scenario_RegisterType
  [ 0] +0x00 0x14001eb40  OWN Scenario_ScalarDtor (101 B)
  [ 1] +0x08 0x14001e9e0  TINY returns qword [0x1401c5b10]
  [ 2] +0x10 0x14001e9d0  TINY returns dword [0x1401c5b08] = TypeId(Scenario)
  [ 3] +0x18 0x1400a9460  OWN Scenario_Shutdown (20 B)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400a9370  OWN Scenario_Slot5 (225 B)
  [ 6] +0x30 0x14001ead0  OWN Scenario_Draw (97 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14001ea70  OWN Scenario_Cleanup (96 B)
  stamped by (6): other+0x0 at 0x14001ea2d in 0x14001e9f0 Scenario_Factory; this+0x0 at 0x14001ea9a in 0x14001ea70 Scenario_Cleanup; this+0x0 at 0x14001eaff in 0x14001ead0 Scenario_Draw; this+0x0 at 0x14001eb64 in 0x14001eb40 Scenario_ScalarDtor; this+0x0 at 0x1400a949d in 0x1400a9480 Scenario_Ctor; this+0x0 at 0x1400a94f4 in 0x1400a94d0 Scenario_CopyCtor
### ScenarioDialog  table=0x14015d2d0  slots=10  sizeof=0xe8  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x65527463656c6573 (string "selectResource")
  reflected: ti=0x1401c5558 create=0x140013970 createInPlace=0x1400139f0 Cleanup_ScenarioDialog registered in 0x14014c680 RegisterType_ScenarioDialog
  [ 0] +0x00 0x140013af0  OWN ScenarioDialog_ScalarDtor (105 B)
  [ 1] +0x08 0x140013960  TINY returns qword [0x1401c5560]
  [ 2] +0x10 0x140013950  TINY returns dword [0x1401c5558] = TypeId(ScenarioDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x140078010  OWN ScenarioDialog_Serialize (1594 B)
  [ 5] +0x28 0x14003a94e  IMPORT ?OnClosed@DialogInstance@UI@@UEAAXXZ  [in 12 tables]
  [ 6] +0x30 0x140077be0  OWN ScenarioDialog_Draw (179 B)
  [ 7] +0x38 0x140078000  TINY RET 0 (c2 00 00)
  [ 8] +0x40 0x140077f80  OWN ScenarioDialog_OnRemove (122 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (6): other+0x0 at 0x1400139ad in 0x140013970 Factory_ScenarioDialog; this+0x0 at 0x140013a1a in 0x1400139f0 Cleanup_ScenarioDialog; this+0x0 at 0x140013b14 in 0x140013af0 ScenarioDialog_ScalarDtor; this+0x0 at 0x14007866d in 0x140078650 ScenarioDialog_Ctor; this+0x0 at 0x1400786f2 in 0x1400786b0 ScenarioDialog_CopyCtor; this+0x0 at 0x140078774 in 0x140078750 ScenarioDialog_Ctor3
### SelectDescription  table=0x14015a5f0  slots=10  sizeof=0x48  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6d6f436e77617053 (string "SpawnComponent")
  reflected: ti=0x1401c48a8 create=0x14000bb90 createInPlace=0x14000bc00 SelectDescription_CreateInPlace registered in 0x14014b6c0 SelectDescription_RegisterType
  [ 0] +0x00 0x14000bcc0  OWN SelectDescription_ScalarDtor (108 B)
  [ 1] +0x08 0x14000bb80  TINY returns qword [0x1401c48b0]
  [ 2] +0x10 0x14000bb70  TINY returns dword [0x1401c48a8] = TypeId(SelectDescription)
  [ 3] +0x18 0x1400556c0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140055630  OWN SelectDescription_Slot5 (132 B)
  [ 6] +0x30 0x14000bc60  OWN SelectDescription_Draw (90 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]
  [ 9] +0x48 0x14000bc00  OWN SelectDescription_CreateInPlace (89 B)
  stamped by (6): other+0x0 at 0x14000bbd5 in 0x14000bb90 SelectDescription_Factory; this+0x0 at 0x14000bc34 in 0x14000bc00 SelectDescription_CreateInPlace; this+0x0 at 0x14000bc99 in 0x14000bc60 SelectDescription_Draw; this+0x0 at 0x14000bce4 in 0x14000bcc0 SelectDescription_ScalarDtor; this+0x0 at 0x1400556ed in 0x1400556d0 SelectDescription_Ctor; this+0x0 at 0x14005575e in 0x140055730 SelectDescription_CopyCtor
### SelectResourceUI  table=0x14015d330  slots=14  sizeof=abstract  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x6143657469727053 (string "SpriteCaptureUI")
  reflected: ti=0x1401c5590 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014c6c0 RegisterType_SelectResourceUI
  [ 0] +0x00 0x140013ca0  OWN CropUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140013c90  TINY returns qword [0x1401c5598]
  [ 2] +0x10 0x140013c80  TINY returns dword [0x1401c5590] = TypeId(SelectResourceUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x140078be0  OWN CropUI_Serialize (187 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x140078cb0  OWN CropUI_OnCreate (120 B)
  [ 8] +0x40 0x140078d30  OWN OrchardUI_OnRemove (66 B)  [in 4 tables]
  [ 9] +0x48 0x140078d80  OWN SelectResourceUI_Slot9 (21 B)  [in 3 tables]
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  [11] +0x58 0x14003addc  _purecall  [in 12 tables]
  [12] +0x60 0x14003addc  _purecall  [in 12 tables]
  [13] +0x68 0x14003addc  _purecall  [in 12 tables]
  stamped by (15): other+0x0 at 0x14001302b in 0x140012ff0 Factory_OrchardUI; this+0x0 at 0x14001309a in 0x140013070 Cleanup_OrchardUI; this+0x0 at 0x14001310e in 0x1400130e0 OrchardUI_ScalarDtor; this+0x0 at 0x140013cc4 in 0x140013ca0 CropUI_ScalarDtor; other+0x0 at 0x140020e4b in 0x140020e10 Factory_PastureUI; this+0x0 at 0x140020eaa in 0x140020e80 Cleanup_PastureUI; this+0x0 at 0x140020f0e in 0x140020ee0 PastureUI_ScalarDtor; this+0x0 at 0x140075693 in 0x140075670 CropUI_Ctor; this+0x0 at 0x1400756d4 in 0x1400756b0 OrchardUI_Ctor; this+0x0 at 0x140078b84 in 0x140078b60 CropUI_Ctor4; this+0x0 at 0x140078bc9 in 0x140078bb0 CropUI_Ctor5; this+0x0 at 0x1400b2c6e in 0x1400b2c10 CropUI_CopyCtor ...
### SendComponentMessage  table=0x1401600c0  slots=10  sizeof=0x40  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x7261626c6f6f54 (string "Toolbar")
  reflected: ti=0x1401c60f0 create=0x140023070 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d3d0 SendComponentMessage_RegisterType
  [ 0] +0x00 0x1400230f0  OWN SendComponentMessage_ScalarDtor (104 B)
  [ 1] +0x08 0x140023060  TINY returns qword [0x1401c60f8]
  [ 2] +0x10 0x140023050  TINY returns dword [0x1401c60f0] = TypeId(SendComponentMessage)
  [ 3] +0x18 0x1400cb000  OWN SendComponentMessage_Shutdown (277 B)
  [ 4] +0x20 0x1400caef0  OWN SendComponentMessage_Serialize (254 B)
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]   = parent
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x1400c4ba0  TINY RET 0 (c2 00 00)  [in 15 tables]   = parent
  [ 8] +0x40 0x1400c4a30  TINY RET 0 (c2 00 00)  [in 7 tables]   = parent
  [ 9] +0x48 0x1400cae80  OWN SendComponentMessage_Update (95 B)
  stamped by (5): other+0x0 at 0x1400230be in 0x140023070 SendComponentMessage_Factory; this+0x0 at 0x140023114 in 0x1400230f0 SendComponentMessage_ScalarDtor; this+0x0 at 0x1400cb13d in 0x1400cb120 Action_Action_Ctor5; this+0x0 at 0x1400cb1a7 in 0x1400cb170 SendComponentMessage_Action_Ctor; this+0x0 at 0x1400cb221 in 0x1400cb1d0 SendComponentMessage_Action_CopyCtor
### ShowTool  table=0x1401589b0  slots=10  sizeof=0x30  parent="Action" / structural Action 0x14015faa0  stop=non-code qword 0x6e656e6f706d6f43 (string "Component")
  reflected: ti=0x1401c3e28 create=0x1400013b0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014a5c0 ShowTool_RegisterType
  [ 0] +0x00 0x140001420  OWN ShowTool_ScalarDtor (104 B)
  [ 1] +0x08 0x1400013a0  TINY returns qword [0x1401c3e30]
  [ 2] +0x10 0x140001390  TINY returns dword [0x1401c3e28] = TypeId(ShowTool)
  [ 3] +0x18 0x140042780  OWN ShowTool_Shutdown (178 B)
  [ 4] +0x20 0x140042710  OWN ShowTool_Serialize (99 B)
  [ 5] +0x28 0x1400c4a90  TINY return 0 (xor eax,eax)  [in 13 tables]   = parent
  [ 6] +0x30 0x1400c4b90  TINY RET 0 (c2 00 00)  [in 16 tables]   = parent
  [ 7] +0x38 0x140042700  TINY RET 0 (c2 00 00)
  [ 8] +0x40 0x1400426d0  OWN ShowTool_OnRemove (32 B)
  [ 9] +0x48 0x140042680  OWN ShowTool_Update (59 B)
  stamped by (7): other+0x0 at 0x140001401 in 0x1400013b0 ShowTool_Factory; this+0x0 at 0x140001444 in 0x140001420 ShowTool_ScalarDtor; this+0x0 at 0x14004285d in 0x140042840 Action_Action_CopyCtor; this+0x0 at 0x1400428d7 in 0x140042890 ShowTool_Action_Ctor; this+0x0 at 0x140042947 in 0x140042910 ShowTool_Action_CopyCtor; other+0x0 at 0x1400be596 in 0x1400bdd20 ShowTool_Action_Ctor3; other+0x0 at 0x1400c1d38 in 0x1400c1530 ShowTool_Action_Ctor4
### SpawnComponent  table=0x14015a670  slots=16  sizeof=0x38  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c48e0 create=0x14000c170 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b700 SpawnComponent_RegisterType
  [ 0] +0x00 0x14000c350  OWN SpawnComponent_ScalarDtor (106 B)
  [ 1] +0x08 0x14000c160  TINY returns qword [0x1401c48e8]
  [ 2] +0x10 0x14000c150  TINY returns dword [0x1401c48e0] = TypeId(SpawnComponent)
  [ 3] +0x18 0x140055ee0  OWN SpawnComponent_Shutdown (233 B)
  [ 4] +0x20 0x140055e90  OWN SpawnComponent_Serialize (62 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140055ad0  OWN SpawnComponent_Update (478 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x14000c374 in 0x14000c350 SpawnComponent_ScalarDtor; this+0x0 at 0x140055fed in 0x140055fd0 SpawnComponent_Ctor; this+0x0 at 0x140056058 in 0x140056030 SpawnComponent_Ctor
### SpawnDescription  table=0x14015a6f0  slots=10  sizeof=0x30  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x44656761726f7453 (string "StorageDisplayComponent")
  reflected: ti=0x1401c4918 create=0x14000c1d0 createInPlace=0x14000c230 SpawnDescription_CreateInPlace registered in 0x14014b740 SpawnDescription_Register
  [ 0] +0x00 0x14000c2e0  OWN SpawnComponent_vslot16 (106 B)
  [ 1] +0x08 0x14000c1c0  TINY returns qword [0x1401c4920]
  [ 2] +0x10 0x14000c1b0  TINY returns dword [0x1401c4918] = TypeId(SpawnDescription)
  [ 3] +0x18 0x140056180  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400560b0  OWN FUN_1400560b0 (194 B)
  [ 6] +0x30 0x14000c280  OWN FUN_14000c280 (79 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14000c2d0  OWN (no Ghidra function)
  [ 9] +0x48 0x14000c230  OWN SpawnDescription_CreateInPlace (78 B)
  stamped by (6): other+0x0 at 0x14000c215 in 0x14000c1d0 SpawnDescription_Factory; this+0x0 at 0x14000c264 in 0x14000c230 SpawnDescription_CreateInPlace; this+0x0 at 0x14000c2b9 in 0x14000c280 FUN_14000c280; this+0x0 at 0x14000c304 in 0x14000c2e0 SpawnComponent_vslot16; this+0x0 at 0x1400561ad in 0x140056190 FUN_140056190; this+0x0 at 0x14005621e in 0x1400561f0 FUN_1400561f0
### SpriteCaptureUI  table=0x14015d4f0  slots=11  sizeof=0x70  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=zero qword
  reflected: ti=0x1401c55c8 create=0x140013d10 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014c700 RegisterType_SpriteCaptureUI
  [ 0] +0x00 0x140013fc0  OWN CaptureBoxDescription_Slot10 (73 B)
  [ 1] +0x08 0x140013d00  TINY returns qword [0x1401c55d0]
  [ 2] +0x10 0x140013cf0  TINY returns dword [0x1401c55c8] = TypeId(SpriteCaptureUI)
  [ 3] +0x18 0x140079850  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x140079640  OWN CaptureBoxDescription_Slot14 (509 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14003a97e  IMPORT ?OnUpdateLayout@DialogController@UI@@UEAAXXZ  [in 10 tables]
  [ 8] +0x40 0x1400793a0  OWN SpriteCaptureUI_OnRemove (622 B)
  [ 9] +0x48 0x140079620  OWN SpriteCaptureUI_Update (21 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x140013d4b in 0x140013d10 Factory_SpriteCaptureUI; this+0x0 at 0x140013fe4 in 0x140013fc0 CaptureBoxDescription_Slot10; this+0x0 at 0x140079879 in 0x140079860 SpriteCaptureUI_Ctor; this+0x0 at 0x1400798b4 in 0x140079890 SpriteCaptureUI_CopyCtor
### StateMachineComponent  table=0x140164520  slots=16  sizeof=0x58  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7d98 create=0x1400382d0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014f990 StateMachineComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x1401644d0
  [ 0] +0x00 0x1400384a0  OWN StateMachineComponent_ScalarDtor (105 B)
  [ 1] +0x08 0x1400382c0  TINY returns qword [0x1401c7da0]
  [ 2] +0x10 0x1400382b0  TINY returns dword [0x1401c7d98] = TypeId(StateMachineComponent)
  [ 3] +0x18 0x14012b940  OWN StateMachineComponent_Serialize (144 B)
  [ 4] +0x20 0x14012b6a0  OWN StateMachineComponent_Startup (283 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x14012b660  OWN StateMachineComponent_OnAdd (49 B)
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x14012b640  OWN StateMachineComponent_OnAttach (15 B)
  [ 9] +0x48 0x14012b620  OWN StateMachineComponent_Update (16 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x14012b7d0  OWN StateMachineComponent_OnUpdate2 (351 B)
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x1400384c4 in 0x1400384a0 StateMachineComponent_ScalarDtor; this+0x0 at 0x14012b9f9 in 0x14012b9e0 StateMachineComponent_Ctor; this+0x0 at 0x14012ba58 in 0x14012ba30 StateMachineComponent_Ctor
### StateMachineDescription  table=0x1401645a0  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6349737574617453 (string "StatusIconComponent")
  reflected: ti=0x1401c7dd0 create=0x140038330 createInPlace=0x140038390 StateMachineDescription_CreateInPlace registered in 0x14014f9d0 StateMachineDescription_Register
  [ 0] +0x00 0x140038440  OWN StateMachineComponent_GetTypeId (83 B)
  [ 1] +0x08 0x140038320  TINY returns qword [0x1401c7dd8]
  [ 2] +0x10 0x140038310  TINY returns dword [0x1401c7dd0] = TypeId(StateMachineDescription)
  [ 3] +0x18 0x14012bb40  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14012bb00  OWN StateMachineComponent_OnCreate (46 B)
  [ 6] +0x30 0x1400383e0  OWN StateMachineComponent_OnDestroy (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140038430  OWN StateMachineComponent_OnCleanup (12 B)
  [ 9] +0x48 0x140038390  OWN StateMachineDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x140038375 in 0x140038330 StateMachineDescription_Factory; this+0x0 at 0x1400383c4 in 0x140038390 StateMachineDescription_CreateInPlace; this+0x0 at 0x140038419 in 0x1400383e0 StateMachineComponent_OnDestroy; this+0x0 at 0x140038464 in 0x140038440 StateMachineComponent_GetTypeId; this+0x0 at 0x14012bb69 in 0x14012bb50 FUN_14012bb50; this+0x0 at 0x14012bbbe in 0x14012bb90 FUN_14012bb90
### StatGoal  table=0x1401604c0  slots=11  sizeof=0x80  parent="Achievement::Goal" / structural Achievement::Goal (Runtime ctor import)  stop=non-code qword 0x6c616f47656d6954 (string "TimeGoalInstance")
  reflected: ti=0x1401c62b0 create=0x1400264d0 createInPlace=0x140026510 <no fn> registered in 0x14014d5d0 StatGoal_RegisterType
  [ 0] +0x00 0x140026540  OWN FinishTutorialInstance_vslot9 (108 B)
  [ 1] +0x08 0x1400264c0  TINY returns qword [0x1401c62b8]
  [ 2] +0x10 0x1400264b0  TINY returns dword [0x1401c62b0] = TypeId(StatGoal)
  [ 3] +0x18 0x1400daa10  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400da8b0  OWN FinishTutorialInstance_vslot14 (331 B)
  [ 6] +0x30 0x140026520  OWN FinishTutorialInstance_vslot15 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400da720  OWN FinishTutorialInstance_vslot17 (343 B)
  [ 9] +0x48 0x14003ab5e  IMPORT ?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ  [in 3 tables]
  [10] +0x50 0x140026510  OWN (no Ghidra function)
  stamped by (3): this+0x0 at 0x140026564 in 0x140026540 FinishTutorialInstance_vslot9; this+0x0 at 0x1400daa3d in 0x1400daa20 FUN_1400daa20; this+0x0 at 0x1400daaa4 in 0x1400daa80 StatGoal_Ctor
### StatusBarUI  table=0x14015d810  slots=11  sizeof=0x1d0  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x6349737574617453 (string "StatusIconUI")
  reflected: ti=0x1401c5670 create=0x1400140e0 createInPlace=0x140014120 <no fn> registered in 0x14014c7c0 RegisterType_StatusBarUI
  [ 0] +0x00 0x140014130  OWN StatusBarUI_ScalarDtor (48 B)
  [ 1] +0x08 0x1400140d0  TINY returns qword [0x1401c5678]
  [ 2] +0x10 0x1400140c0  TINY returns dword [0x1401c5670] = TypeId(StatusBarUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x14003a984  IMPORT ?OnCreated@DialogController@UI@@UEAAXXZ  [in 5 tables]
  [ 5] +0x28 0x14007a3d0  OWN StatusBarUI_Slot5 (2712 B)
  [ 6] +0x30 0x14007a3c0  TINY RET 0 (c2 00 00)
  [ 7] +0x38 0x1400798d0  OWN StatusBarUI_OnCreate (2107 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14007a3a0  OWN StatusBarUI_Update (26 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (2): this+0x0 at 0x14007ae97 in 0x14007ae70 StatusBarUI_Ctor; this+0x0 at 0x14007af44 in 0x14007af20 StatusBarUI_Ctor
### StatusIconComponent  table=0x140164620  slots=16  sizeof=0x108  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7e08 create=0x1400388c0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014fa90 StatusIconComponent_RegisterType
  member vptrs: +0x28=0x14015f0c0
  [ 0] +0x00 0x140038ac0  OWN StatusIconComponent_ScalarDtor (102 B)
  [ 1] +0x08 0x1400388b0  TINY returns qword [0x1401c7e10]
  [ 2] +0x10 0x1400388a0  TINY returns dword [0x1401c7e08] = TypeId(StatusIconComponent)
  [ 3] +0x18 0x14012bff0  OWN StatusIconComponent_Shutdown (116 B)
  [ 4] +0x20 0x14012bed0  OWN StatusIconComponent_Serialize (102 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x14012beb0  OWN StatusIconComponent_OnCreate (15 B)
  [ 8] +0x40 0x14012bbd0  OWN StatusIconComponent_OnRemove (728 B)
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x14012bf40  OWN StatusIconComponent_Slot11 (156 B)
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140038ae4 in 0x140038ac0 StatusIconComponent_ScalarDtor; this+0x0 at 0x14012c08d in 0x14012c070 StatusIconComponent_Ctor; this+0x0 at 0x14012c0e8 in 0x14012c0c0 StatusIconComponent_Ctor
### StatusIconDescription  table=0x1401646a0  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x44656761726f7453 (string "StorageDescription")
  reflected: ti=0x1401c7e40 create=0x140038920 createInPlace=0x140038990 StatusIconDescription_CreateInPlace registered in 0x14014fad0 StatusIconDescription_Register
  [ 0] +0x00 0x140038a60  OWN StatusIconComponent_vslot16 (83 B)
  [ 1] +0x08 0x140038910  TINY returns qword [0x1401c7e48]
  [ 2] +0x10 0x140038900  TINY returns dword [0x1401c7e40] = TypeId(StatusIconDescription)
  [ 3] +0x18 0x14012c1f0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14012c180  OWN FUN_14012c180 (103 B)
  [ 6] +0x30 0x1400389f0  OWN FUN_1400389f0 (91 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140038a50  OWN (no Ghidra function)
  [ 9] +0x48 0x140038990  OWN StatusIconDescription_CreateInPlace (90 B)
  stamped by (6): other+0x0 at 0x140038965 in 0x140038920 StatusIconDescription_Factory; this+0x0 at 0x1400389c4 in 0x140038990 StatusIconDescription_CreateInPlace; this+0x0 at 0x140038a29 in 0x1400389f0 FUN_1400389f0; this+0x0 at 0x140038a84 in 0x140038a60 StatusIconComponent_vslot16; this+0x0 at 0x14012c219 in 0x14012c200 FUN_14012c200; this+0x0 at 0x14012c26e in 0x14012c240 FUN_14012c240
### StatusIconUI  table=0x14015d8a0  slots=11  sizeof=0x40  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=zero qword
  reflected: ti=0x1401c56a8 create=0x140014180 createInPlace=0x1400141e0 Cleanup_StatusIconUI registered in 0x14014c800 RegisterType_StatusIconUI
  [ 0] +0x00 0x140014370  OWN StatusIconUI_ScalarDtor (96 B)
  [ 1] +0x08 0x140014170  TINY returns qword [0x1401c56b0]
  [ 2] +0x10 0x140014160  TINY returns dword [0x1401c56a8] = TypeId(StatusIconUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x14007b270  OWN StatusIconUI_Serialize (241 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14007b0a0  OWN StatusIconUI_OnCreate (456 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14003a972  IMPORT ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z  [in 11 tables]
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (5): other+0x0 at 0x1400141bb in 0x140014180 Factory_StatusIconUI; this+0x0 at 0x14001420a in 0x1400141e0 Cleanup_StatusIconUI; this+0x0 at 0x140014394 in 0x140014370 StatusIconUI_ScalarDtor; this+0x0 at 0x14007b38d in 0x14007b370 StatusIconUI_Ctor; this+0x0 at 0x14007b3e4 in 0x14007b3c0 StatusIconUI_CopyCtor
### StatusIconUIConfig  table=0x14015d900  slots=9  sizeof=0x28  parent="UI::DialogControllerConfig" / structural UI::DialogControllerConfig (Runtime ctor import)  stop=zero qword
  reflected: ti=0x1401c56e0 create=0x140014250 createInPlace=0x1400142a0 StatusIconUIConfig_CreateInPlace registered in 0x14014c840 StatusIconUIConfig_Register
  [ 0] +0x00 0x140014320  OWN StatusIconUI_Slot12 (73 B)
  [ 1] +0x08 0x140014240  TINY returns qword [0x1401c56e8]
  [ 2] +0x10 0x140014230  TINY returns dword [0x1401c56e0] = TypeId(StatusIconUIConfig)
  [ 3] +0x18 0x14007b450  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x14007b400  OWN StatusIconUI_vslot17 (71 B)
  [ 6] +0x30 0x1400142e0  OWN StatusIconUI_vslot18 (63 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400142a0  OWN StatusIconUIConfig_CreateInPlace (62 B)
  stamped by (6): other+0x0 at 0x14001428b in 0x140014250 StatusIconUIConfig_Factory; this+0x0 at 0x1400142ca in 0x1400142a0 StatusIconUIConfig_CreateInPlace; this+0x0 at 0x14001430f in 0x1400142e0 StatusIconUI_vslot18; this+0x0 at 0x140014344 in 0x140014320 StatusIconUI_Slot12; this+0x0 at 0x14007b479 in 0x14007b460 FUN_14007b460; this+0x0 at 0x14007b4b4 in 0x14007b490 FUN_14007b490
### StorageCapacityUI  table=0x14015d950  slots=11  sizeof=0x38  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x43656761726f7453 (string "StorageCapacityUI")
  reflected: ti=0x1401c5718 create=0x1400143f0 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014c880 RegisterType_StorageCapacityUI
  [ 0] +0x00 0x140014450  OWN StorageCapacityUI_ScalarDtor (73 B)
  [ 1] +0x08 0x1400143e0  TINY returns qword [0x1401c5720]
  [ 2] +0x10 0x1400143d0  TINY returns dword [0x1401c5718] = TypeId(StorageCapacityUI)
  [ 3] +0x18 0x14007b6b0  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x14007b600  OWN StorageCapacityUI_Serialize (156 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14007b4d0  OWN StorageCapacityUI_OnCreate (283 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14003a972  IMPORT ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z  [in 11 tables]
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x14001442b in 0x1400143f0 Factory_StorageCapacityUI; this+0x0 at 0x140014474 in 0x140014450 StorageCapacityUI_ScalarDtor; this+0x0 at 0x14007b6d9 in 0x14007b6c0 StorageCapacityUI_Ctor; this+0x0 at 0x14007b714 in 0x14007b6f0 StorageCapacityUI_CopyCtor
### StorageComponent  table=0x140164720  slots=16  sizeof=0x40  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7e78 create=0x140038b50 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014fb10 StorageComponent_RegisterType
  [ 0] +0x00 0x140038d60  OWN StorageComponent_ScalarDtor (106 B)
  [ 1] +0x08 0x140038b40  TINY returns qword [0x1401c7e80]
  [ 2] +0x10 0x140038b30  TINY returns dword [0x1401c7e78] = TypeId(StorageComponent)
  [ 3] +0x18 0x14012c730  OWN StorageComponent_Shutdown (311 B)
  [ 4] +0x20 0x14012c720  TINY RET 0 (c2 00 00)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x14012c640  OWN StorageComponent_Draw (212 B)
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140038d84 in 0x140038d60 StorageComponent_ScalarDtor; this+0x0 at 0x14012c88d in 0x14012c870 StorageComponent_Ctor; this+0x0 at 0x14012c8f8 in 0x14012c8d0 StorageComponent_Ctor
### StorageDescription  table=0x1401647a0  slots=10  sizeof=0x40  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x4c656761726f7453 (string "StorageLocationDescription")
  reflected: ti=0x1401c7eb0 create=0x140038bb0 createInPlace=0x140038c20 StorageDescription_CreateInPlace registered in 0x14014fb50 StorageDescription_Register
  [ 0] +0x00 0x140038cf0  OWN StorageComponent_vslot16 (106 B)
  [ 1] +0x08 0x140038ba0  TINY returns qword [0x1401c7eb8]
  [ 2] +0x10 0x140038b90  TINY returns dword [0x1401c7eb0] = TypeId(StorageDescription)
  [ 3] +0x18 0x14012caa0  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14012c9a0  OWN FUN_14012c9a0 (241 B)
  [ 6] +0x30 0x140038c80  OWN FUN_140038c80 (89 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140038ce0  OWN (no Ghidra function)
  [ 9] +0x48 0x140038c20  OWN StorageDescription_CreateInPlace (88 B)
  stamped by (6): other+0x0 at 0x140038bf5 in 0x140038bb0 StorageDescription_Factory; this+0x0 at 0x140038c54 in 0x140038c20 StorageDescription_CreateInPlace; this+0x0 at 0x140038cb9 in 0x140038c80 FUN_140038c80; this+0x0 at 0x140038d14 in 0x140038cf0 StorageComponent_vslot16; this+0x0 at 0x14012cacd in 0x14012cab0 FUN_14012cab0; this+0x0 at 0x14012cb3e in 0x14012cb10 FUN_14012cb10
### StorageDisplayComponent  table=0x14015a780  slots=16  sizeof=0x38  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c4950 create=0x14000c770 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b780 StorageDisplayComponent_RegisterType
  [ 0] +0x00 0x14000c940  OWN StorageDisplayComponent_ScalarDtor (96 B)
  [ 1] +0x08 0x14000c760  TINY returns qword [0x1401c4958]
  [ 2] +0x10 0x14000c750  TINY returns dword [0x1401c4950] = TypeId(StorageDisplayComponent)
  [ 3] +0x18 0x140056970  OWN StorageDisplayComponent_Shutdown (48 B)
  [ 4] +0x20 0x140056930  TINY RET 0 (c2 00 00)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140056730  OWN StorageDisplayComponent_Draw (504 B)
  [ 7] +0x38 0x1400566b0  OWN StorageDisplayComponent_OnCreate (109 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x1400566a0  JMP 0x1400563e0 Entity_impl_63e0
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140056940  OWN StorageDisplayComponent_Slot11 (28 B)
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x14000c964 in 0x14000c940 StorageDisplayComponent_ScalarDtor; this+0x0 at 0x1400569cd in 0x1400569b0 StorageDisplayComponent_Ctor; this+0x0 at 0x140056a28 in 0x140056a00 StorageDisplayComponent_Ctor
### StorageDisplayDescription  table=0x14015a800  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x726f666172726554 (string "TerraformComponent")
  reflected: ti=0x1401c4988 create=0x14000c7d0 createInPlace=0x14000c830 StorageDisplayDescription_CreateInPlace registered in 0x14014b7c0 StorageDisplayDescription_Register
  [ 0] +0x00 0x14000c8e0  OWN StorageDisplayComponent_vslot16 (83 B)
  [ 1] +0x08 0x14000c7c0  TINY returns qword [0x1401c4990]
  [ 2] +0x10 0x14000c7b0  TINY returns dword [0x1401c4988] = TypeId(StorageDisplayDescription)
  [ 3] +0x18 0x140056ac0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140056a80  OWN FUN_140056a80 (46 B)
  [ 6] +0x30 0x14000c880  OWN FUN_14000c880 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14000c8d0  OWN (no Ghidra function)
  [ 9] +0x48 0x14000c830  OWN StorageDisplayDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x14000c815 in 0x14000c7d0 StorageDisplayDescription_Factory; this+0x0 at 0x14000c864 in 0x14000c830 StorageDisplayDescription_CreateInPlace; this+0x0 at 0x14000c8b9 in 0x14000c880 FUN_14000c880; this+0x0 at 0x14000c904 in 0x14000c8e0 StorageDisplayComponent_vslot16; this+0x0 at 0x140056ae9 in 0x140056ad0 FUN_140056ad0; this+0x0 at 0x140056b3e in 0x140056b10 FUN_140056b10
### StorageLocationComponent  table=0x140164880  slots=18  sizeof=0x48  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c7ee8 create=0x140038df0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014fb90 StorageLocationComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x140164830
  [ 0] +0x00 0x140038fc0  OWN StorageLocationComponent_Controller_Slot10 (115 B)
  [ 1] +0x08 0x140038de0  TINY returns qword [0x1401c7ef0]
  [ 2] +0x10 0x140038dd0  TINY returns dword [0x1401c7ee8] = TypeId(StorageLocationComponent)
  [ 3] +0x18 0x14012d2a0  OWN StorageLocationComponent_Controller_Slot13 (76 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x14012d270  OWN StorageLocationComponent_OnCreate (42 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (3): this+0x0 at 0x140038fe4 in 0x140038fc0 StorageLocationComponent_Controller_Slot10; this+0x0 at 0x14012d319 in 0x14012d300 StorageLocationComponent_Ctor; this+0x0 at 0x14012d392 in 0x14012d360 StorageLocationComponent_Ctor
### StorageLocationDescription  table=0x140164910  slots=10  sizeof=0x28  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x447261626c6f6f54 (string "ToolbarDescription")
  reflected: ti=0x1401c7f20 create=0x140038e50 createInPlace=0x140038eb0 StorageLocationDescription_CreateInPlace registered in 0x14014fbd0 StorageLocationDescription_Register
  [ 0] +0x00 0x140038f60  OWN StorageLocationComponent_vslot18 (83 B)
  [ 1] +0x08 0x140038e40  TINY returns qword [0x1401c7f28]
  [ 2] +0x10 0x140038e30  TINY returns dword [0x1401c7f20] = TypeId(StorageLocationDescription)
  [ 3] +0x18 0x14012d4c0  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14012d470  OWN FUN_14012d470 (74 B)
  [ 6] +0x30 0x140038f00  OWN FUN_140038f00 (76 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140038f50  OWN (no Ghidra function)
  [ 9] +0x48 0x140038eb0  OWN StorageLocationDescription_CreateInPlace (75 B)
  stamped by (6): other+0x0 at 0x140038e95 in 0x140038e50 StorageLocationDescription_Factory; this+0x0 at 0x140038ee4 in 0x140038eb0 StorageLocationDescription_CreateInPlace; this+0x0 at 0x140038f39 in 0x140038f00 FUN_140038f00; this+0x0 at 0x140038f84 in 0x140038f60 StorageLocationComponent_vslot18; this+0x0 at 0x14012d4e9 in 0x14012d4d0 FUN_14012d4d0; this+0x0 at 0x14012d53e in 0x14012d510 FUN_14012d510
### StorageUI  table=0x14015f970  slots=11  sizeof=0x40  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x447261626c6f6f54 (string "ToolbarDialog")
  reflected: ti=0x1401c5d00 create=0x140021330 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014cf50 RegisterType_StorageUI
  [ 0] +0x00 0x140021390  OWN StorageUI_ScalarDtor (96 B)
  [ 1] +0x08 0x140021320  TINY returns qword [0x1401c5d08]
  [ 2] +0x10 0x140021310  TINY returns dword [0x1401c5d00] = TypeId(StorageUI)
  [ 3] +0x18 0x1400b8ca0  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x1400b8820  OWN StorageUI_Serialize (1136 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x1400b8450  OWN StorageUI_OnCreate (965 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14003a972  IMPORT ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z  [in 11 tables]
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x14002136b in 0x140021330 Factory_StorageUI; this+0x0 at 0x1400213b4 in 0x140021390 StorageUI_ScalarDtor; this+0x0 at 0x1400b8ccd in 0x1400b8cb0 StorageUI_Ctor; this+0x0 at 0x1400b8d24 in 0x1400b8d00 StorageUI_CopyCtor
### TendArea  table=0x140161990  slots=13  sizeof=0x60  parent="Work" / structural Work 0x140161a80  stop=non-code qword 0x726f576564617254 (string "TradeWork")
  reflected: ti=0x1401c6c88 create=0x14002c590 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e110 TendArea_RegisterType
  [ 0] +0x00 0x14002c5d0  OWN TendArea_ScalarDtor (48 B)
  [ 1] +0x08 0x14002c580  TINY returns qword [0x1401c6c90]
  [ 2] +0x10 0x14002c570  TINY returns dword [0x1401c6c88] = TypeId(TendArea)
  [ 3] +0x18 0x1400f5900  OWN TendArea_Shutdown (104 B)
  [ 4] +0x20 0x1400f5490  OWN TendArea_Serialize (907 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400f5470  OWN TendArea_Draw (20 B)
  [ 7] +0x38 0x1400f5460  THUNK this+72 -> 0x140025520 CitizenAI_impl_5520
  [ 8] +0x40 0x1400f5830  OWN TendArea_OnRemove (104 B)
  [ 9] +0x48 0x1400f5230  OWN TendArea_Update (547 B)
  [10] +0x50 0x1400f58a0  OWN TendArea_Slot10 (75 B)
  [11] +0x58 0x140029860  OWN ClearArea_Slot11 (9 B)  [in 13 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (4): this+0x0 at 0x1400f5997 in 0x1400f5970 Work_Ctor21; this+0x0 at 0x1400f5a57 in 0x1400f5a00 TendArea_Ctor; this+0x0 at 0x1400f5ab8 in 0x1400f5a80 TendArea_CopyCtor; other+0x0 at 0x14011a166 in 0x14011a100 TendArea_Ctor
### TerraformComponent  table=0x14015a880  slots=16  sizeof=0x48  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c49c0 create=0x14000cc90 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b800 TerraformComponent_RegisterType
  [ 0] +0x00 0x14000ce90  OWN TerraformComponent_ScalarDtor (48 B)
  [ 1] +0x08 0x14000cc80  TINY returns qword [0x1401c49c8]
  [ 2] +0x10 0x14000cc70  TINY returns dword [0x1401c49c0] = TypeId(TerraformComponent)
  [ 3] +0x18 0x140057500  OWN TerraformComponent_Shutdown (158 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x1400574f0  TINY RET 0 (c2 00 00)
  [ 7] +0x38 0x140057350  OWN TerraformComponent_OnCreate (397 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140057310  OWN TerraformComponent_Slot12 (45 B)
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (2): this+0x0 at 0x1400575d2 in 0x1400575b0 TerraformComponent_Ctor; this+0x0 at 0x140057648 in 0x140057620 TerraformComponent_Ctor
### TerraformDescription  table=0x14015a900  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6e6f7473626d6f54 (string "TombstoneComponent")
  reflected: ti=0x1401c49f8 create=0x14000ccf0 createInPlace=0x14000cd60 TerraformDescription_CreateInPlace registered in 0x14014b840 TerraformDescription_Register
  [ 0] +0x00 0x14000ce30  OWN TerraformComponent_vslot16 (83 B)
  [ 1] +0x08 0x14000cce0  TINY returns qword [0x1401c4a00]
  [ 2] +0x10 0x14000ccd0  TINY returns dword [0x1401c49f8] = TypeId(TerraformDescription)
  [ 3] +0x18 0x140057730  TINY return 3 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400576a0  OWN FUN_1400576a0 (123 B)
  [ 6] +0x30 0x14000cdc0  OWN FUN_14000cdc0 (83 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14000ce20  OWN (no Ghidra function)
  [ 9] +0x48 0x14000cd60  OWN TerraformDescription_CreateInPlace (82 B)
  stamped by (6): other+0x0 at 0x14000cd35 in 0x14000ccf0 TerraformDescription_Factory; this+0x0 at 0x14000cd94 in 0x14000cd60 TerraformDescription_CreateInPlace; this+0x0 at 0x14000cdf9 in 0x14000cdc0 FUN_14000cdc0; this+0x0 at 0x14000ce54 in 0x14000ce30 TerraformComponent_vslot16; this+0x0 at 0x140057759 in 0x140057740 FUN_140057740; this+0x0 at 0x1400577ae in 0x140057780 FUN_140057780
### TerrainGenerator  table=0x14015f170  slots=9  sizeof=0x90  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x4955646c697542 (string "BuildUI")
  reflected: ti=0x1401c5b40 create=0x140020590 createInPlace=0x1400205d0 TerrainGenerator_Cleanup registered in 0x14014cd50 TerrainGenerator_RegisterType
  [ 0] +0x00 0x140020600  OWN TerrainGenerator_ScalarDtor (48 B)
  [ 1] +0x08 0x140020580  TINY returns qword [0x1401c5b48]
  [ 2] +0x10 0x140020570  TINY returns dword [0x1401c5b40] = TypeId(TerrainGenerator)
  [ 3] +0x18 0x1400b1800  TINY return 4 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400b1690  OWN TerrainGenerator_Slot5 (352 B)
  [ 6] +0x30 0x1400205e0  OWN TerrainGenerator_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400205d0  OWN TerrainGenerator_Cleanup (12 B)
  stamped by (1): this+0x0 at 0x1400b1834 in 0x1400b1810 TerrainGenerator_Ctor
### TimeGoal  table=0x140160540  slots=11  sizeof=0x38  parent="Achievement::Goal" / structural Achievement::Goal (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c62e8 create=0x1400266f0 createInPlace=0x140026750 TimeGoal_Cleanup registered in 0x14014d610 TimeGoal_RegisterType
  [ 0] +0x00 0x140026850  OWN TimeGoal_ScalarDtor (73 B)
  [ 1] +0x08 0x1400266e0  TINY returns qword [0x1401c62f0]
  [ 2] +0x10 0x1400266d0  TINY returns dword [0x1401c62e8] = TypeId(TimeGoal)
  [ 3] +0x18 0x1400daba0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400dab20  OWN TimeGoal_Slot5 (111 B)
  [ 6] +0x30 0x1400267a0  OWN TimeGoal_Draw (70 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400dab10  TINY return 0 (xor eax,eax)
  [ 9] +0x48 0x1400267f0  OWN TimeGoal_Update (12 B)
  [10] +0x50 0x140026750  OWN TimeGoal_Cleanup (69 B)
  stamped by (6): other+0x0 at 0x14002672b in 0x1400266f0 TimeGoal_Factory; this+0x0 at 0x14002677a in 0x140026750 TimeGoal_Cleanup; this+0x0 at 0x1400267cf in 0x1400267a0 TimeGoal_Draw; this+0x0 at 0x140026874 in 0x140026850 TimeGoal_ScalarDtor; this+0x0 at 0x1400dabc9 in 0x1400dabb0 TimeGoal_Ctor; this+0x0 at 0x1400dac04 in 0x1400dabe0 TimeGoal_CopyCtor
### TimeGoalInstance  table=0x140160598  slots=7  sizeof=0x28  parent="Achievement::GoalInstance" / structural Achievement::GoalInstance (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c6320 create=0x140026670 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014d650 TimeGoalInstance_RegisterType
  [ 0] +0x00 0x140026800  OWN TimeGoal_Slot11 (73 B)
  [ 1] +0x08 0x140026660  TINY returns qword [0x1401c6328]
  [ 2] +0x10 0x140026650  TINY returns dword [0x1401c6320] = TypeId(TimeGoalInstance)
  [ 3] +0x18 0x1400dad30  OWN TimeGoal_Slot14 (88 B)
  [ 4] +0x20 0x1400daca0  TINY RET 0 (c2 00 00)
  [ 5] +0x28 0x1400dac30  OWN TimeGoalInstance_Slot5 (102 B)
  [ 6] +0x30 0x1400dacb0  OWN TimeGoalInstance_Draw (108 B)
  stamped by (4): other+0x0 at 0x1400266ab in 0x140026670 TimeGoalInstance_Factory; this+0x0 at 0x140026824 in 0x140026800 TimeGoal_Slot11; this+0x0 at 0x1400dada9 in 0x1400dad90 TimeGoalInstance_Ctor; this+0x0 at 0x1400dade4 in 0x1400dadc0 TimeGoalInstance_CopyCtor
### TitleDescToolTipUI  table=0x14015d9f0  slots=11  sizeof=0x30  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x49556c6f6f54 (string "ToolUI")
  reflected: ti=0x1401c5750 create=0x1400144c0 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014c8c0 RegisterType_TitleDescToolTipUI
  [ 0] +0x00 0x140014510  OWN TitleDescToolTipUI_ScalarDtor (73 B)
  [ 1] +0x08 0x1400144b0  TINY returns qword [0x1401c5758]
  [ 2] +0x10 0x1400144a0  TINY returns dword [0x1401c5750] = TypeId(TitleDescToolTipUI)
  [ 3] +0x18 0x14007b8a0  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x14007b730  OWN TitleDescToolTipUI_Serialize (358 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14003a97e  IMPORT ?OnUpdateLayout@DialogController@UI@@UEAAXXZ  [in 10 tables]
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x14003a972  IMPORT ?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z  [in 11 tables]
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x1400144fb in 0x1400144c0 Factory_TitleDescToolTipUI; this+0x0 at 0x140014534 in 0x140014510 TitleDescToolTipUI_ScalarDtor; this+0x0 at 0x14007b8c9 in 0x14007b8b0 TitleDescToolTipUI_Ctor; this+0x0 at 0x14007b904 in 0x14007b8e0 TitleDescToolTipUI_CopyCtor
### TombstoneComponent  table=0x14015a980  slots=16  sizeof=0x30  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c4a30 create=0x14000d280 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b880 TombstoneComponent_RegisterType
  [ 0] +0x00 0x14000d480  OWN TombstoneComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x14000d270  TINY returns qword [0x1401c4a38]
  [ 2] +0x10 0x14000d260  TINY returns dword [0x1401c4a30] = TypeId(TombstoneComponent)
  [ 3] +0x18 0x1400578e0  OWN TombstoneComponent_Shutdown (121 B)
  [ 4] +0x20 0x140057860  OWN TombstoneComponent_Serialize (120 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x1400577d0  OWN TombstoneComponent_Update (131 B)
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x14000d4a4 in 0x14000d480 TombstoneComponent_ScalarDtor; this+0x0 at 0x140057979 in 0x140057960 TombstoneComponent_Ctor; this+0x0 at 0x1400579c8 in 0x1400579a0 TombstoneComponent_Ctor
### TombstoneDescription  table=0x14015aa00  slots=10  sizeof=0x28  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x436f64616e726f54 (string "TornadoComponent")
  reflected: ti=0x1401c4a68 create=0x14000d2e0 createInPlace=0x14000d350 TombstoneDescription_CreateInPlace registered in 0x14014b8c0 TombstoneDescription_Register
  [ 0] +0x00 0x14000d420  OWN TombstoneComponent_vslot16 (83 B)
  [ 1] +0x08 0x14000d2d0  TINY returns qword [0x1401c4a70]
  [ 2] +0x10 0x14000d2c0  TINY returns dword [0x1401c4a68] = TypeId(TombstoneDescription)
  [ 3] +0x18 0x140057a80  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140057a20  OWN FUN_140057a20 (89 B)
  [ 6] +0x30 0x14000d3b0  OWN FUN_14000d3b0 (83 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14000d410  OWN (no Ghidra function)
  [ 9] +0x48 0x14000d350  OWN TombstoneDescription_CreateInPlace (82 B)
  stamped by (6): other+0x0 at 0x14000d325 in 0x14000d2e0 TombstoneDescription_Factory; this+0x0 at 0x14000d384 in 0x14000d350 TombstoneDescription_CreateInPlace; this+0x0 at 0x14000d3e9 in 0x14000d3b0 FUN_14000d3b0; this+0x0 at 0x14000d444 in 0x14000d420 TombstoneComponent_vslot16; this+0x0 at 0x140057aa9 in 0x140057a90 FUN_140057a90; this+0x0 at 0x140057afe in 0x140057ad0 FUN_140057ad0
### Tool  table=0x140161f10  slots=13  sizeof=abstract  parent="Object::Internal" / structural Object::Internal (Runtime ctor import)  stop=non-code qword 0x655262726f736241 (string "AbsorbResourceComponent")
  reflected: ti=0x1401c6eb8 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014e390 Tool_StaticInit
  [ 0] +0x00 0x14002e9b0  OWN Tool_ScalarDtor (73 B)
  [ 1] +0x08 0x14002e950  TINY returns qword [0x1401c6ec0]
  [ 2] +0x10 0x14002e940  TINY returns dword [0x1401c6eb8] = TypeId(Tool)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x14003addc  _purecall  [in 12 tables]
  [ 5] +0x28 0x14003addc  _purecall  [in 12 tables]
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]
  [ 9] +0x48 0x14003addc  _purecall  [in 12 tables]
  [10] +0x50 0x1400268a0  TINY return false (xor al,al)  [in 8 tables]
  [11] +0x58 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]
  [12] +0x60 0x140105860  TINY return 1 (mov eax)  [in 2 tables]
  stamped by (29): this+0x0 at 0x140026968 in 0x140026930 TimeGoalInstance_OnCreate; this+0x0 at 0x14002d828 in 0x14002d7f0 BridgeTool_ScalarDtor; this+0x0 at 0x14002d938 in 0x14002d900 DragPlacementTool_ScalarDtor; this+0x0 at 0x14002da4e in 0x14002da20 DragTool_ScalarDtor; this+0x0 at 0x14002db2e in 0x14002db00 EditTool_ScalarDtor; this+0x0 at 0x14002dcbe in 0x14002dc90 PlacementTool_ScalarDtor; this+0x0 at 0x14002e828 in 0x14002e7f0 RemovalTool_ScalarDtor; this+0x0 at 0x14002e8f8 in 0x14002e8c0 RoadTool_ScalarDtor; this+0x0 at 0x14002e9d4 in 0x14002e9b0 Tool_ScalarDtor; this+0x0 at 0x1400dd04d in 0x1400dd020 Tool_Ctor; this+0x0 at 0x1400dd094 in 0x1400dd070 FencedTool_Ctor; this+0x0 at 0x1400de9b4 in 0x1400de950 Tool_CopyCtor ...
### Toolbar  table=0x140160120  slots=9  sizeof=0x68  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x7363696870617247 (string "Graphics::Particle")
  reflected: ti=0x1401c6128 create=0x140024ba0 createInPlace=0x140024be0 Toolbar_Cleanup registered in 0x14014d410 Toolbar_RegisterType
  [ 0] +0x00 0x140024c10  OWN Toolbar_ScalarDtor (88 B)
  [ 1] +0x08 0x140024b90  TINY returns qword [0x1401c6130]
  [ 2] +0x10 0x140024b80  TINY returns dword [0x1401c6128] = TypeId(Toolbar)
  [ 3] +0x18 0x1400d2120  TINY return 4 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400d2010  OWN Toolbar_Slot5 (253 B)
  [ 6] +0x30 0x140024bf0  OWN Toolbar_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140024be0  OWN Toolbar_Cleanup (12 B)
  stamped by (1): this+0x0 at 0x1400d2154 in 0x1400d2130 Toolbar_Ctor
### ToolbarDescription  table=0x140164980  slots=10  sizeof=0x68  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6972637365444955 (string "UIDescription")
  reflected: ti=0x1401c7f58 create=0x140039490 createInPlace=0x1400394d0 ToolbarDescription_CreateInPlace registered in 0x14014fcd0 ToolbarDescription_RegisterType
  [ 0] +0x00 0x140039500  OWN ToolbarDescription_ScalarDtor (48 B)
  [ 1] +0x08 0x140039480  TINY returns qword [0x1401c7f60]
  [ 2] +0x10 0x140039470  TINY returns dword [0x1401c7f58] = TypeId(ToolbarDescription)
  [ 3] +0x18 0x14012db60  TINY return 4 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14012da30  OWN ToolbarDescription_Slot5 (297 B)
  [ 6] +0x30 0x1400394e0  OWN ToolbarDescription_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]
  [ 9] +0x48 0x1400394d0  OWN ToolbarDescription_CreateInPlace (12 B)
  stamped by (2): this+0x0 at 0x14012db97 in 0x14012db70 ToolbarDescription_Ctor; this+0x0 at 0x14012dc5e in 0x14012dc30 ToolbarDescription_Ctor
### ToolbarDialog  table=0x14015fa00  slots=10  sizeof=0x108  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x6e656e6f706d6f43 (string "ComponentEntity")
  reflected: ti=0x1401c5d38 create=0x140021710 createInPlace=0x140021750 <no fn> registered in 0x14014cf90 RegisterType_ToolbarDialog
  [ 0] +0x00 0x140021760  OWN ToolbarDialog_ScalarDtor (48 B)
  [ 1] +0x08 0x140021700  TINY returns qword [0x1401c5d40]
  [ 2] +0x10 0x1400216f0  TINY returns dword [0x1401c5d38] = TypeId(ToolbarDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x1400b97c0  TINY RET 0 (c2 00 00)
  [ 5] +0x28 0x1400b97b0  JMP 0x1400b9730 FUN_1400b9730
  [ 6] +0x30 0x1400b8d40  OWN ToolbarDialog_Draw (638 B)
  [ 7] +0x38 0x14003a990  IMPORT ?OnUpdate@DialogInstance@UI@@UEAAXAEBVTime@System@@@Z  [in 11 tables]
  [ 8] +0x40 0x1400b92a0  OWN ToolbarDialog_OnRemove (1108 B)
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (3): this+0x0 at 0x1400b97f2 in 0x1400b97d0 ToolbarDialog_Ctor; this+0x0 at 0x1400b98b4 in 0x1400b9890 ToolbarDialog_Ctor; this+0x0 at 0x1400b996f in 0x1400b9910 ToolbarDialog_UpdateUI
### ToolSetting  table=0x140161ee8  slots=4  sizeof=abstract  parent="Object::Internal" / structural Object::Internal (Runtime ctor import)  stop=zero qword
  reflected: ti=0x1401c6ef0 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014e3d0 ToolSetting_RegisterType
  [ 0] +0x00 0x14002e960  OWN RoadTool_Slot15 (73 B)
  [ 1] +0x08 0x14002e930  TINY returns qword [0x1401c6ef8]
  [ 2] +0x10 0x14002e920  TINY returns dword [0x1401c6ef0] = TypeId(ToolSetting)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  stamped by (8): other+0x0 at 0x140026d5b in 0x140026d20 RotationSetting_Factory; this+0x0 at 0x140026daa in 0x140026d80 RotationSetting_Cleanup; this+0x0 at 0x140026dfe in 0x140026dd0 RotationSetting_ScalarDtor; this+0x0 at 0x14002e984 in 0x14002e960 RoadTool_Slot15; this+0x0 at 0x1400dea63 in 0x1400dea40 RotationSetting_Ctor; this+0x0 at 0x1400deaa4 in 0x1400dea80 RotationSetting_CopyCtor; this+0x0 at 0x140105894 in 0x140105870 FUN_140105870; this+0x0 at 0x1401058c9 in 0x1401058b0 FUN_1401058b0
### ToolUI  table=0x14015da70  slots=11  sizeof=0x58  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x6c6c61486e776f54 (string "TownHallUI")
  reflected: ti=0x1401c5788 create=0x140014580 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014c900 RegisterType_ToolUI
  [ 0] +0x00 0x1400145f0  OWN ToolUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140014570  TINY returns qword [0x1401c5790]
  [ 2] +0x10 0x140014560  TINY returns dword [0x1401c5788] = TypeId(ToolUI)
  [ 3] +0x18 0x14007c070  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x14007bd70  OWN ToolUI_Serialize (754 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14003a97e  IMPORT ?OnUpdateLayout@DialogController@UI@@UEAAXXZ  [in 10 tables]
  [ 8] +0x40 0x14007b9b0  OWN ToolUI_OnRemove (632 B)
  [ 9] +0x48 0x14007bc30  OWN ToolUI_Update (311 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x1400145bb in 0x140014580 Factory_ToolUI; this+0x0 at 0x140014614 in 0x1400145f0 ToolUI_ScalarDtor; this+0x0 at 0x14007c099 in 0x14007c080 ToolUI_Ctor; this+0x0 at 0x14007c0d4 in 0x14007c0b0 ToolUI_CopyCtor
### Tornado  table=0x140160190  slots=10  sizeof=0x90  parent="Graphics::Particle" / structural Graphics::Particle (Runtime ctor import)  stop=next table
  reflected: ti=0x1401c6160 create=0x140024e90 createInPlace=0x140024ed0 <no fn> registered in 0x14014d450 Toolbar_StaticInit
  [ 0] +0x00 0x140024f00  OWN Toolbar_vslot14 (73 B)
  [ 1] +0x08 0x140024e80  TINY returns qword [0x1401c6168]
  [ 2] +0x10 0x140024e70  TINY returns dword [0x1401c6160] = TypeId(Tornado)
  [ 3] +0x18 0x1400d2370  OWN GetFileVersion_wrapper (18 B)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400d2210  OWN Toolbar_vslot19 (339 B)
  [ 6] +0x30 0x140024ee0  OWN FUN_140024ee0 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400d21b0  OWN FUN_1400d21b0 (87 B)
  [ 9] +0x48 0x140024ed0  OWN (no Ghidra function)
  stamped by (3): this+0x0 at 0x140024f24 in 0x140024f00 Toolbar_vslot14; this+0x0 at 0x1400d23a9 in 0x1400d2390 FUN_1400d2390; this+0x0 at 0x1400d23e4 in 0x1400d23c0 FUN_1400d23c0
### TornadoComponent  table=0x14015aa80  slots=16  sizeof=0x68  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c4ad8 create=0x14000d500 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b940 TornadoComponent_RegisterType
  [ 0] +0x00 0x14000d640  OWN TornadoComponent_ScalarDtor (126 B)
  [ 1] +0x08 0x14000d4f0  TINY returns qword [0x1401c4ae0]
  [ 2] +0x10 0x14000d4e0  TINY returns dword [0x1401c4ad8] = TypeId(TornadoComponent)
  [ 3] +0x18 0x140058b80  OWN TornadoComponent_Shutdown (241 B)
  [ 4] +0x20 0x1400585a0  OWN TornadoComponent_Serialize (981 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140057cd0  OWN TornadoComponent_OnRemove (859 B)
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x14000d669 in 0x14000d640 TornadoComponent_ScalarDtor; this+0x0 at 0x140058ca2 in 0x140058c80 TornadoComponent_Ctor; this+0x0 at 0x140058d18 in 0x140058cf0 TornadoComponent_Ctor
### TornadoDescription  table=0x14015ab00  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x4372656b63617254 (string "TrackerComponent")
  reflected: ti=0x1401c4aa0 create=0x14000d560 createInPlace=0x14000d5a0 TornadoDescription_CreateInPlace registered in 0x14014b900 TornadoDescription_Register
  [ 0] +0x00 0x14000d5e0  OWN TornadoComponent_vslot16 (83 B)
  [ 1] +0x08 0x14000d550  TINY returns qword [0x1401c4aa8]
  [ 2] +0x10 0x14000d540  TINY returns dword [0x1401c4aa0] = TypeId(TornadoDescription)
  [ 3] +0x18 0x140058e30  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140058d90  OWN FUN_140058d90 (140 B)
  [ 6] +0x30 0x14000d5b0  OWN FUN_14000d5b0 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14000d5d0  OWN (no Ghidra function)
  [ 9] +0x48 0x14000d5a0  OWN TornadoDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x14000d604 in 0x14000d5e0 TornadoComponent_vslot16; this+0x0 at 0x140058e59 in 0x140058e40 FUN_140058e40; this+0x0 at 0x140058eae in 0x140058e80 TornadoDescription_sub_8e80
### TownHallUI  table=0x14015dce0  slots=11  sizeof=0x1e8  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x49556564617254 (string "TradeUI")
  reflected: ti=0x1401c57c0 create=0x140014660 createInPlace=0x1400146a0 <no fn> registered in 0x14014c940 RegisterType_TownHallUI
  [ 0] +0x00 0x1400146b0  OWN TownHallUI_ScalarDtor (48 B)
  [ 1] +0x08 0x140014650  TINY returns qword [0x1401c57c8]
  [ 2] +0x10 0x140014640  TINY returns dword [0x1401c57c0] = TypeId(TownHallUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x140081000  OWN TownHallUI_Serialize (202 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x14007d550  OWN TownHallUI_OnCreate (43 B)
  [ 8] +0x40 0x14007e550  OWN TownHallUI_OnRemove (76 B)
  [ 9] +0x48 0x14007c180  OWN TownHallUI_OnRemove (147 B)
  [10] +0x50 0x140080db0  OWN TownHallUI_Slot10 (575 B)
  stamped by (2): this+0x0 at 0x1400810f4 in 0x1400810d0 TownHallUI_CopyCtor; this+0x0 at 0x1400812a4 in 0x140081280 TownHallUI_Ctor
### TrackerComponent  table=0x14015ab80  slots=16  sizeof=0x50  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c4b10 create=0x14000d7a0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014b980 TrackerComponent_RegisterType
  [ 0] +0x00 0x14000d9a0  OWN TrackerComponent_ScalarDtor (48 B)
  [ 1] +0x08 0x14000d790  TINY returns qword [0x1401c4b18]
  [ 2] +0x10 0x14000d780  TINY returns dword [0x1401c4b10] = TypeId(TrackerComponent)
  [ 3] +0x18 0x140059130  OWN TrackerComponent_Shutdown (326 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): other+0x0 at 0x14000d7da in 0x14000d7a0 TrackerComponent_Factory; this+0x0 at 0x1400592a2 in 0x140059280 TrackerComponent_Ctor; this+0x0 at 0x140059313 in 0x1400592f0 TrackerComponent_CopyCtor
### TrackerDescription  table=0x14015ac00  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6d6f436564617254 (string "TradeComponent")
  reflected: ti=0x1401c4b48 create=0x14000d830 createInPlace=0x14000d890 TrackerDescription_CreateInPlace registered in 0x14014b9c0 TrackerDescription_Register
  [ 0] +0x00 0x14000d940  OWN TrackerComponent_vslot16 (83 B)
  [ 1] +0x08 0x14000d820  TINY returns qword [0x1401c4b50]
  [ 2] +0x10 0x14000d810  TINY returns dword [0x1401c4b48] = TypeId(TrackerDescription)
  [ 3] +0x18 0x140059400  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400593c0  OWN FUN_1400593c0 (46 B)
  [ 6] +0x30 0x14000d8e0  OWN FUN_14000d8e0 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14000d930  OWN (no Ghidra function)
  [ 9] +0x48 0x14000d890  OWN TrackerDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x14000d875 in 0x14000d830 TrackerDescription_Factory; this+0x0 at 0x14000d8c4 in 0x14000d890 TrackerDescription_CreateInPlace; this+0x0 at 0x14000d919 in 0x14000d8e0 FUN_14000d8e0; this+0x0 at 0x14000d964 in 0x14000d940 TrackerComponent_vslot16; this+0x0 at 0x140059429 in 0x140059410 FUN_140059410; this+0x0 at 0x14005947e in 0x140059450 FUN_140059450
### TradeComponent  table=0x14015acd0  slots=18  sizeof=0xa8  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c4b80 create=0x14000db60 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014ba00 TradeComponent_RegisterType
  member vptrs: +0x88=0x14015ac80, +0x88=0x14015f070
  [ 0] +0x00 0x14000dc80  OWN TradeComponent_Controller_Slot10 (48 B)
  [ 1] +0x08 0x14000db50  TINY returns qword [0x1401c4b88]
  [ 2] +0x10 0x14000db40  TINY returns dword [0x1401c4b80] = TypeId(TradeComponent)
  [ 3] +0x18 0x14005bd50  OWN TradeComponent_Controller_Slot13 (672 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x14005c1d0  OWN TradeComponent_Draw (64 B)
  [ 7] +0x38 0x14005c190  OWN TradeComponent_OnCreate (46 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x14005c140  OWN TradeComponent_Slot13 (68 B)
  [14] +0x70 0x14005c000  OWN TradeComponent_Slot14 (308 B)
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x14000dba0  TINY returns this+0x88
  stamped by (2): this+0x0 at 0x14005c367 in 0x14005c340 TradeComponent_Ctor; this+0x0 at 0x14005c462 in 0x14005c430 TradeComponent_Ctor
### TradeDescription  table=0x14015ad60  slots=10  sizeof=0x70  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x73654465756c6156 (string "ValueDescription")
  reflected: ti=0x1401c4bb8 create=0x14000dbd0 createInPlace=0x14000dc10 TradeDescription_CreateInPlace registered in 0x14014ba40 TradeDescription_Register
  [ 0] +0x00 0x14000dc50  OWN TradeComponent_vslot18 (48 B)
  [ 1] +0x08 0x14000dbc0  TINY returns qword [0x1401c4bc0]
  [ 2] +0x10 0x14000dbb0  TINY returns dword [0x1401c4bb8] = TypeId(TradeDescription)
  [ 3] +0x18 0x14005ca00  TINY return 7 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14005c7f0  OWN TradeComponent_impl_c7f0 (508 B)
  [ 6] +0x30 0x14000dc20  OWN FUN_14000dc20 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14000dc40  OWN (no Ghidra function)
  [ 9] +0x48 0x14000dc10  OWN TradeDescription_CreateInPlace (12 B)
  stamped by (2): this+0x0 at 0x14005ca32 in 0x14005ca10 TradeComponent_impl_ca10; this+0x0 at 0x14005cace in 0x14005caa0 TradeDescription_sub_caa0
### TradeUI  table=0x14015de80  slots=11  sizeof=0x148  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x5565646172677055 (string "UpgradeUI")
  reflected: ti=0x1401c57f8 create=0x140015f80 createInPlace=0x140015fc0 <no fn> registered in 0x14014c980 RegisterType_TradeUI
  [ 0] +0x00 0x140016020  OWN TradeUI_ScalarDtor (48 B)
  [ 1] +0x08 0x140015f70  TINY returns qword [0x1401c5800]
  [ 2] +0x10 0x140015f60  TINY returns dword [0x1401c57f8] = TypeId(TradeUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x140084dd0  OWN TradeUI_Serialize (6890 B)
  [ 5] +0x28 0x140082a70  OWN TradeUI_Slot5 (160 B)
  [ 6] +0x30 0x140082a00  OWN TradeUI_Draw (96 B)
  [ 7] +0x38 0x140082150  OWN TradeUI_OnCreate (1612 B)
  [ 8] +0x40 0x1400827b0  OWN TradeUI_OnRemove (152 B)
  [ 9] +0x48 0x140081f20  OWN TradeUI_Update (549 B)
  [10] +0x50 0x1400868c0  OWN TradeUI_Slot10 (386 B)
  stamped by (2): this+0x0 at 0x140086a72 in 0x140086a50 TradeUI_Ctor; this+0x0 at 0x140086ba4 in 0x140086b80 TradeUI_Ctor
### TradeWork  table=0x140161a10  slots=13  sizeof=0x58  parent="Work" / structural Work 0x140161a80  stop=zero qword
  reflected: ti=0x1401c6cc0 create=0x14002c620 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e150 TradeWork_RegisterType
  [ 0] +0x00 0x14002c660  OWN TradeWork_ScalarDtor (48 B)
  [ 1] +0x08 0x14002c610  TINY returns qword [0x1401c6cc8]
  [ 2] +0x10 0x14002c600  TINY returns dword [0x1401c6cc0] = TypeId(TradeWork)
  [ 3] +0x18 0x1400f6cb0  OWN TradeWork_Shutdown (86 B)
  [ 4] +0x20 0x1400f5d40  OWN TradeWork_Serialize (3730 B)
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]   = parent
  [ 6] +0x30 0x1400f5d20  OWN TradeWork_Draw (20 B)
  [ 7] +0x38 0x1400f5d10  THUNK this+72 -> 0x14002c690 TradeWork_impl_c690
  [ 8] +0x40 0x1400f6be0  OWN TradeWork_OnRemove (104 B)
  [ 9] +0x48 0x1400f5ae0  OWN TradeWork_Update (547 B)
  [10] +0x50 0x1400f6c50  OWN TradeWork_Slot10 (75 B)
  [11] +0x58 0x140029860  OWN ClearArea_Slot11 (9 B)  [in 13 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]   = parent
  stamped by (4): other+0x0 at 0x14005c0a1 in 0x14005c000 TradeComponent_Slot14; this+0x0 at 0x1400f6d37 in 0x1400f6d10 Work_Ctor22; this+0x0 at 0x1400f6df7 in 0x1400f6da0 TradeWork_Ctor; this+0x0 at 0x1400f6ed0 in 0x1400f6ea0 TradeWork_Ctor
### Tutorial  table=0x140160740  slots=9  sizeof=0x40  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6c6169726f747554 (string "TutorialGoal")
  reflected: ti=0x1401c6400 create=0x140026e50 createInPlace=0x140026eb0 Tutorial_Cleanup registered in 0x14014d750 Tutorial_RegisterType
  [ 0] +0x00 0x140026f50  OWN Tutorial_ScalarDtor (96 B)
  [ 1] +0x08 0x140026e40  TINY returns qword [0x1401c6408]
  [ 2] +0x10 0x140026e30  TINY returns dword [0x1401c6400] = TypeId(Tutorial)
  [ 3] +0x18 0x1400deb80  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400dead0  OWN Tutorial_Slot5 (163 B)
  [ 6] +0x30 0x140026f00  OWN Tutorial_Draw (80 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140026eb0  OWN Tutorial_Cleanup (79 B)
  stamped by (6): other+0x0 at 0x140026e8b in 0x140026e50 Tutorial_Factory; this+0x0 at 0x140026eda in 0x140026eb0 Tutorial_Cleanup; this+0x0 at 0x140026f2f in 0x140026f00 Tutorial_Draw; this+0x0 at 0x140026f74 in 0x140026f50 Tutorial_ScalarDtor; this+0x0 at 0x1400debad in 0x1400deb90 Tutorial_Ctor; this+0x0 at 0x1400dec04 in 0x1400debe0 Tutorial_CopyCtor
### TutorialGoal  table=0x1401607a0  slots=9  sizeof=abstract  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6c6169726f747554 (string "TutorialGoalButton")
  reflected: ti=0x1401c6438 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014d790 TutorialGoal_RegisterType
  [ 0] +0x00 0x140027220  OWN TutorialGoal_ScalarDtor (73 B)
  [ 1] +0x08 0x140027210  TINY returns qword [0x1401c6440]
  [ 2] +0x10 0x140027200  TINY returns dword [0x1401c6438] = TypeId(TutorialGoal)
  [ 3] +0x18 0x1400dec70  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400dec30  OWN TutorialGoal_Slot5 (46 B)
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]
  stamped by (24): this+0x0 at 0x140027244 in 0x140027220 TutorialGoal_ScalarDtor; other+0x0 at 0x1400272cb in 0x140027290 TutorialGoalButton_Factory; this+0x0 at 0x14002732a in 0x140027300 TutorialGoalButton_Cleanup; this+0x0 at 0x14002737f in 0x140027350 TutorialGoalButton_Draw; this+0x0 at 0x1400273e9 in 0x1400273b0 TutorialGoalButton_ScalarDtor; other+0x0 at 0x14002747b in 0x140027440 Factory_TutorialGoalDialog; this+0x0 at 0x1400274da in 0x1400274b0 Cleanup_TutorialGoalDialog; this+0x0 at 0x14002754f in 0x140027520 TutorialGoalDialog_Draw; this+0x0 at 0x1400275d9 in 0x140027590 TutorialGoalDialog_ScalarDtor; other+0x0 at 0x14002766b in 0x140027630 TutorialGoalGameSpeed_Factory; this+0x0 at 0x1400276ba in 0x140027690 TutorialGoalGameSpeed_Cleanup; this+0x0 at 0x14002770f in 0x1400276e0 TutorialGoalGameSpeed_Draw ...
### TutorialGoalButton  table=0x140160800  slots=10  sizeof=0x30  parent="TutorialGoal" / structural TutorialGoal 0x1401607a0  stop=non-code qword 0x6c6169726f747554 (string "TutorialGoalDialog")
  reflected: ti=0x1401c6470 create=0x140027290 createInPlace=0x140027300 TutorialGoalButton_Cleanup registered in 0x14014d7d0 TutorialGoalButton_RegisterType
  [ 0] +0x00 0x1400273b0  OWN TutorialGoalButton_ScalarDtor (97 B)
  [ 1] +0x08 0x140027280  TINY returns qword [0x1401c6478]
  [ 2] +0x10 0x140027270  TINY returns dword [0x1401c6470] = TypeId(TutorialGoalButton)
  [ 3] +0x18 0x1400dee30  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400deda0  OWN TutorialGoalButton_Slot5 (128 B)
  [ 6] +0x30 0x140027350  OWN TutorialGoalButton_Draw (81 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400dece0  OWN TutorialGoalButton_OnRemove (180 B)
  [ 9] +0x48 0x140027300  OWN TutorialGoalButton_Cleanup (80 B)
  stamped by (6): other+0x0 at 0x1400272d5 in 0x140027290 TutorialGoalButton_Factory; this+0x0 at 0x140027334 in 0x140027300 TutorialGoalButton_Cleanup; this+0x0 at 0x140027389 in 0x140027350 TutorialGoalButton_Draw; this+0x0 at 0x1400273d4 in 0x1400273b0 TutorialGoalButton_ScalarDtor; this+0x0 at 0x1400dee5d in 0x1400dee40 TutorialGoal_Ctor6; this+0x0 at 0x1400deebe in 0x1400dee90 TutorialGoalButton_Ctor
### TutorialGoalCount  table=0x140160950  slots=10  sizeof=0x50  parent="TutorialGoal" / structural TutorialGoal 0x1401607a0  stop=non-code qword 0x6c6169726f747554 (string "TutorialHighlightDialog")
  reflected: ti=0x1401c6518 create=0x1400277b0 createInPlace=0x1400277f0 TutorialGoalCount_Update registered in 0x14014d890 TutorialGoalCount_RegisterType
  [ 0] +0x00 0x140027820  OWN TutorialGoalCount_ScalarDtor (113 B)
  [ 1] +0x08 0x1400277a0  TINY returns qword [0x1401c6520]
  [ 2] +0x10 0x140027790  TINY returns dword [0x1401c6518] = TypeId(TutorialGoalCount)
  [ 3] +0x18 0x1400df9f0  TINY return 5 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400df900  OWN TutorialGoalCount_Slot5 (226 B)
  [ 6] +0x30 0x140027800  OWN TutorialGoalCount_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400df290  OWN TutorialGoalCount_OnRemove (1586 B)
  [ 9] +0x48 0x1400277f0  OWN TutorialGoalCount_Update (12 B)
  stamped by (3): this+0x0 at 0x140027844 in 0x140027820 TutorialGoalCount_ScalarDtor; this+0x0 at 0x1400dfa1d in 0x1400dfa00 TutorialGoal_Ctor; this+0x0 at 0x1400dfa8e in 0x1400dfa60 TutorialGoalCount_Ctor
### TutorialGoalDialog  table=0x140160870  slots=10  sizeof=0x40  parent="TutorialGoal" / structural TutorialGoal 0x1401607a0  stop=non-code qword 0x6c6169726f747554 (string "TutorialGoalGameSpeed")
  reflected: ti=0x1401c64a8 create=0x140027440 createInPlace=0x1400274b0 Cleanup_TutorialGoalDialog registered in 0x14014d810 RegisterType_TutorialGoalDialog
  [ 0] +0x00 0x140027590  OWN TutorialGoalDialog_ScalarDtor (113 B)
  [ 1] +0x08 0x140027430  TINY returns qword [0x1401c64b0]
  [ 2] +0x10 0x140027420  TINY returns dword [0x1401c64a8] = TypeId(TutorialGoalDialog)
  [ 3] +0x18 0x1400df080  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400defd0  OWN TutorialGoalDialog_Slot5 (169 B)
  [ 6] +0x30 0x140027520  OWN TutorialGoalDialog_Draw (98 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400deee0  OWN TutorialGoalDialog_OnRemove (226 B)
  [ 9] +0x48 0x1400274b0  OWN Cleanup_TutorialGoalDialog (98 B)
  stamped by (6): other+0x0 at 0x140027485 in 0x140027440 Factory_TutorialGoalDialog; this+0x0 at 0x1400274e4 in 0x1400274b0 Cleanup_TutorialGoalDialog; this+0x0 at 0x140027559 in 0x140027520 TutorialGoalDialog_Draw; this+0x0 at 0x1400275b4 in 0x140027590 TutorialGoalDialog_ScalarDtor; this+0x0 at 0x1400df0ad in 0x1400df090 TutorialGoal_CopyCtor; this+0x0 at 0x1400df11e in 0x1400df0f0 TutorialGoalDialog_Ctor
### TutorialGoalGameSpeed  table=0x1401608e0  slots=10  sizeof=0x28  parent="TutorialGoal" / structural TutorialGoal 0x1401607a0  stop=non-code qword 0x6c6169726f747554 (string "TutorialGoalCount")
  reflected: ti=0x1401c64e0 create=0x140027630 createInPlace=0x140027690 TutorialGoalGameSpeed_Cleanup registered in 0x14014d850 TutorialGoalGameSpeed_RegisterType
  [ 0] +0x00 0x140027730  OWN TutorialGoalGameSpeed_ScalarDtor (83 B)
  [ 1] +0x08 0x140027620  TINY returns qword [0x1401c64e8]
  [ 2] +0x10 0x140027610  TINY returns dword [0x1401c64e0] = TypeId(TutorialGoalGameSpeed)
  [ 3] +0x18 0x1400df1f0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400df170  OWN TutorialGoalGameSpeed_Slot5 (113 B)
  [ 6] +0x30 0x1400276e0  OWN TutorialGoalGameSpeed_Draw (76 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400df150  OWN TutorialGoalGameSpeed_OnRemove (24 B)
  [ 9] +0x48 0x140027690  OWN TutorialGoalGameSpeed_Cleanup (75 B)
  stamped by (6): other+0x0 at 0x140027675 in 0x140027630 TutorialGoalGameSpeed_Factory; this+0x0 at 0x1400276c4 in 0x140027690 TutorialGoalGameSpeed_Cleanup; this+0x0 at 0x140027719 in 0x1400276e0 TutorialGoalGameSpeed_Draw; this+0x0 at 0x140027754 in 0x140027730 TutorialGoalGameSpeed_ScalarDtor; this+0x0 at 0x1400df219 in 0x1400df200 TutorialGoal_Ctor3; this+0x0 at 0x1400df26e in 0x1400df240 TutorialGoalGameSpeed_Ctor
### TutorialHighlightDialog  table=0x1401609e0  slots=10  sizeof=0xf0  parent="UI::DialogInstance" / structural UI::DialogInstance (Runtime ctor import)  stop=non-code qword 0x6c6169726f747554 (string "TutorialImage")
  reflected: ti=0x1401c6550 create=0x1400278c0 createInPlace=0x140027900 <no fn> registered in 0x14014d8d0 RegisterType_TutorialHighlightDialog
  [ 0] +0x00 0x140027910  OWN TutorialHighlightDialog_ScalarDtor (73 B)
  [ 1] +0x08 0x1400278b0  TINY returns qword [0x1401c6558]
  [ 2] +0x10 0x1400278a0  TINY returns dword [0x1401c6550] = TypeId(TutorialHighlightDialog)
  [ 3] +0x18 0x14003a948  IMPORT ?Persist@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  [ 4] +0x20 0x1400e0020  OWN TutorialHighlightDialog_Serialize (370 B)
  [ 5] +0x28 0x14003a94e  IMPORT ?OnClosed@DialogInstance@UI@@UEAAXXZ  [in 12 tables]
  [ 6] +0x30 0x1400dffe0  OWN TutorialHighlightDialog_Draw (19 B)
  [ 7] +0x38 0x1400e0000  OWN TutorialHighlightDialog_OnCreate (11 B)
  [ 8] +0x40 0x14003aa1a  OWN OnElementEvent (6 B)  [in 4 tables]
  [ 9] +0x48 0x14003a954  IMPORT ?OnSerializeSettings@DialogInstance@UI@@UEAAXAEAVStream@IO@@@Z  [in 21 tables]
  stamped by (4): this+0x0 at 0x140027934 in 0x140027910 TutorialHighlightDialog_ScalarDtor; this+0x0 at 0x1400e01b9 in 0x1400e01a0 TutorialHighlightDialog_Ctor; this+0x0 at 0x1400e0212 in 0x1400e01d0 TutorialHighlightDialog_CopyCtor; this+0x0 at 0x1400e02b4 in 0x1400e0290 TutorialHighlightDialog_Ctor
### TutorialImage  table=0x140160a40  slots=10  sizeof=0x30  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6c6169726f747554 (string "TutorialImageHotKey")
  reflected: ti=0x1401c6588 create=0x140027980 createInPlace=0x1400279e0 TutorialImage_Cleanup registered in 0x14014d910 TutorialImage_RegisterType
  [ 0] +0x00 0x140027a80  OWN TutorialImage_ScalarDtor (73 B)
  [ 1] +0x08 0x140027970  TINY returns qword [0x1401c6590]
  [ 2] +0x10 0x140027960  TINY returns dword [0x1401c6588] = TypeId(TutorialImage)
  [ 3] +0x18 0x1400e03a0  TINY return 4096 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400e0330  OWN TutorialImage_Slot5 (103 B)
  [ 6] +0x30 0x140027a30  OWN TutorialImage_Draw (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400279e0  OWN TutorialImage_Cleanup (68 B)
  [ 9] +0x48 0x1400e0300  OWN TutorialImage_Update (34 B)
  stamped by (18): other+0x0 at 0x1400279bb in 0x140027980 TutorialImage_Factory; this+0x0 at 0x140027a0a in 0x1400279e0 TutorialImage_Cleanup; this+0x0 at 0x140027a5f in 0x140027a30 TutorialImage_Draw; this+0x0 at 0x140027aa4 in 0x140027a80 TutorialImage_ScalarDtor; this+0x0 at 0x140027dae in 0x140027d70 TutorialImageHotKey_ScalarDtor; other+0x0 at 0x140027edb in 0x140027ea0 TutorialImageInputKey_Factory; this+0x0 at 0x140027f2a in 0x140027f00 TutorialImageInputKey_Cleanup; this+0x0 at 0x140027f7f in 0x140027f50 TutorialImageInputKey_Draw; this+0x0 at 0x140027fce in 0x140027fa0 TutorialImageInputKey_ScalarDtor; this+0x0 at 0x1400280ce in 0x140028090 TutorialImageToolbarIcon_ScalarDtor; this+0x0 at 0x1400e03c9 in 0x1400e03b0 TutorialImage_Ctor3; this+0x0 at 0x1400e0404 in 0x1400e03e0 TutorialImage_Ctor4 ...
### TutorialImageHotKey  table=0x140160ab0  slots=10  sizeof=0x48  parent="TutorialImage" / structural TutorialImage 0x140160a40  stop=non-code qword 0x6c6169726f747554 (string "TutorialImageInputKey")
  reflected: ti=0x1401c65c0 create=0x140027d00 createInPlace=0x140027d40 TutorialImageHotKey_OnRemove registered in 0x14014d950 TutorialImageHotKey_RegisterType
  [ 0] +0x00 0x140027d70  OWN TutorialImageHotKey_ScalarDtor (102 B)
  [ 1] +0x08 0x140027cf0  TINY returns qword [0x1401c65c8]
  [ 2] +0x10 0x140027ce0  TINY returns dword [0x1401c65c0] = TypeId(TutorialImageHotKey)
  [ 3] +0x18 0x1400e0620  TINY return 4097 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400e0540  OWN TutorialImageHotKey_Slot5 (205 B)
  [ 6] +0x30 0x140027d50  OWN TutorialImageHotKey_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]   = parent
  [ 8] +0x40 0x140027d40  OWN TutorialImageHotKey_OnRemove (12 B)
  [ 9] +0x48 0x1400e0420  OWN TutorialImageHotKey_Update (279 B)
  stamped by (3): this+0x0 at 0x140027d94 in 0x140027d70 TutorialImageHotKey_ScalarDtor; this+0x0 at 0x1400e064d in 0x1400e0630 TutorialImage_Ctor5; this+0x0 at 0x1400e06b8 in 0x1400e0680 TutorialImageHotKey_Ctor
### TutorialImageInputKey  table=0x140160b20  slots=10  sizeof=0x38  parent="TutorialImage" / structural TutorialImage 0x140160a40  stop=non-code qword 0x6c6169726f747554 (string "TutorialImageToolbarIcon")
  reflected: ti=0x1401c65f8 create=0x140027ea0 createInPlace=0x140027f00 TutorialImageInputKey_Cleanup registered in 0x14014d990 TutorialImageInputKey_RegisterType
  [ 0] +0x00 0x140027fa0  OWN TutorialImageInputKey_ScalarDtor (83 B)
  [ 1] +0x08 0x140027e90  TINY returns qword [0x1401c6600]
  [ 2] +0x10 0x140027e80  TINY returns dword [0x1401c65f8] = TypeId(TutorialImageInputKey)
  [ 3] +0x18 0x1400e08a0  TINY return 4097 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400e07f0  OWN TutorialImageInputKey_Slot5 (167 B)
  [ 6] +0x30 0x140027f50  OWN TutorialImageInputKey_Draw (79 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]   = parent
  [ 8] +0x40 0x140027f00  OWN TutorialImageInputKey_Cleanup (78 B)
  [ 9] +0x48 0x1400e06e0  OWN TutorialImageInputKey_Update (253 B)
  stamped by (6): other+0x0 at 0x140027eef in 0x140027ea0 TutorialImageInputKey_Factory; this+0x0 at 0x140027f3e in 0x140027f00 TutorialImageInputKey_Cleanup; this+0x0 at 0x140027f93 in 0x140027f50 TutorialImageInputKey_Draw; this+0x0 at 0x140027fc4 in 0x140027fa0 TutorialImageInputKey_ScalarDtor; this+0x0 at 0x1400e08c9 in 0x1400e08b0 TutorialImage_Ctor; this+0x0 at 0x1400e0928 in 0x1400e08f0 TutorialImageInputKey_Ctor
### TutorialImageToolbarIcon  table=0x140160b90  slots=10  sizeof=0x48  parent="TutorialImage" / structural TutorialImage 0x140160a40  stop=non-code qword 0x6c6169726f747554 (string "TutorialInstance")
  reflected: ti=0x1401c6630 create=0x140028020 createInPlace=0x140028060 TutorialImageToolbarIcon_OnRemove registered in 0x14014d9d0 TutorialImageToolbarIcon_RegisterType
  [ 0] +0x00 0x140028090  OWN TutorialImageToolbarIcon_ScalarDtor (102 B)
  [ 1] +0x08 0x140028010  TINY returns qword [0x1401c6638]
  [ 2] +0x10 0x140028000  TINY returns dword [0x1401c6630] = TypeId(TutorialImageToolbarIcon)
  [ 3] +0x18 0x1400e0bd0  TINY return 4097 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400e0af0  OWN TutorialImageToolbarIcon_Slot5 (205 B)
  [ 6] +0x30 0x140028070  OWN TutorialImageToolbarIcon_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]   = parent
  [ 8] +0x40 0x140028060  OWN TutorialImageToolbarIcon_OnRemove (12 B)
  [ 9] +0x48 0x1400e0940  OWN TutorialImageToolbarIcon_Update (421 B)
  stamped by (3): this+0x0 at 0x1400280b4 in 0x140028090 TutorialImageToolbarIcon_ScalarDtor; this+0x0 at 0x1400e0bfd in 0x1400e0be0 TutorialImage_CopyCtor; this+0x0 at 0x1400e0c68 in 0x1400e0c30 TutorialImageToolbarIcon_Ctor
### TutorialInstance  table=0x140160bf8  slots=4  sizeof=0x38  parent="Object::Internal" / structural Object::Internal (Runtime ctor import)  stop=non-code qword 0x6c6169726f747554 (string "TutorialSetup")
  reflected: ti=0x1401c6668 create=0x140028120 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014da10 TutorialInstance_RegisterType
  [ 0] +0x00 0x1400281a0  OWN TutorialImageToolbarIcon_vslot13 (73 B)
  [ 1] +0x08 0x140028110  TINY returns qword [0x1401c6670]
  [ 2] +0x10 0x140028100  TINY returns dword [0x1401c6668] = TypeId(TutorialInstance)
  [ 3] +0x18 0x1400e1520  OWN TutorialImageToolbarIcon_vslot16 (161 B)
  stamped by (5): other+0x0 at 0x14002815b in 0x140028120 TutorialInstance_Factory; this+0x0 at 0x1400281c4 in 0x1400281a0 TutorialImageToolbarIcon_vslot13; other+0x0 at 0x1400d7ad8 in 0x1400d7a90 FinishTutorialGoal_Slot15; this+0x0 at 0x1400e15e9 in 0x1400e15d0 FUN_1400e15d0; this+0x0 at 0x1400e1cf4 in 0x1400e1cd0 FUN_1400e1cd0
### TutorialSetup  table=0x140160c30  slots=11  sizeof=abstract  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6c6169726f747554 (string "TutorialSetupCamera")
  reflected: ti=0x1401c66a0 create=0x140003730 createInPlace=0x140003730 AutoPickupWork_Factory_Abstract registered in 0x14014da50 TutorialSetup_RegisterType
  [ 0] +0x00 0x140028390  OWN TutorialSetup_ScalarDtor (73 B)
  [ 1] +0x08 0x140028380  TINY returns qword [0x1401c66a8]
  [ 2] +0x10 0x140028370  TINY returns dword [0x1401c66a0] = TypeId(TutorialSetup)
  [ 3] +0x18 0x1400e2040  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400e2000  OWN TutorialSetup_Slot5 (46 B)
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]
  [ 9] +0x48 0x14003addc  _purecall  [in 12 tables]
  [10] +0x50 0x14003addc  _purecall  [in 12 tables]
  stamped by (20): this+0x0 at 0x1400283b4 in 0x140028390 TutorialSetup_ScalarDtor; other+0x0 at 0x14002843b in 0x140028400 TutorialSetupCamera_Factory; this+0x0 at 0x14002849a in 0x140028470 TutorialSetupCamera_Cleanup; this+0x0 at 0x1400284ff in 0x1400284d0 TutorialSetupCamera_Draw; this+0x0 at 0x14002855e in 0x140028530 TutorialSetupCamera_ScalarDtor; other+0x0 at 0x1400289fb in 0x1400289c0 TutorialSetupGameSpeed_Factory; this+0x0 at 0x140028a4a in 0x140028a20 TutorialSetupGameSpeed_Cleanup; this+0x0 at 0x140028a9f in 0x140028a70 TutorialSetupGameSpeed_Draw; this+0x0 at 0x140028aee in 0x140028ac0 TutorialSetupGameSpeed_ScalarDtor; this+0x0 at 0x140028bfe in 0x140028bb0 TutorialSetupRestrictPlayer_ScalarDtor; this+0x0 at 0x1400e2069 in 0x1400e2050 TutorialSetup_Ctor4; this+0x0 at 0x1400e20a4 in 0x1400e2080 TutorialSetup_Ctor5 ...
### TutorialSetupCamera  table=0x140160ca0  slots=12  sizeof=0x30  parent="TutorialSetup" / structural TutorialSetup 0x140160c30  stop=non-code qword 0x6c6169726f747554 (string "TutorialSetupDialog")
  reflected: ti=0x1401c66d8 create=0x140028400 createInPlace=0x140028470 TutorialSetupCamera_Cleanup registered in 0x14014da90 TutorialSetupCamera_RegisterType
  [ 0] +0x00 0x140028530  OWN TutorialSetupCamera_ScalarDtor (83 B)
  [ 1] +0x08 0x1400283f0  TINY returns qword [0x1401c66e0]
  [ 2] +0x10 0x1400283e0  TINY returns dword [0x1401c66d8] = TypeId(TutorialSetupCamera)
  [ 3] +0x18 0x1400e21b0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400e2120  OWN TutorialSetupCamera_Slot5 (125 B)
  [ 6] +0x30 0x1400284d0  OWN TutorialSetupCamera_Draw (85 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400e20d0  OWN TutorialSetupCamera_OnRemove (67 B)
  [ 9] +0x48 0x1400e20c0  TINY RET 0 (c2 00 00)
  [10] +0x50 0x1400e20b0  TINY RET 0 (c2 00 00)
  [11] +0x58 0x140028470  OWN TutorialSetupCamera_Cleanup (84 B)
  stamped by (6): other+0x0 at 0x140028445 in 0x140028400 TutorialSetupCamera_Factory; this+0x0 at 0x1400284a4 in 0x140028470 TutorialSetupCamera_Cleanup; this+0x0 at 0x140028509 in 0x1400284d0 TutorialSetupCamera_Draw; this+0x0 at 0x140028554 in 0x140028530 TutorialSetupCamera_ScalarDtor; this+0x0 at 0x1400e21d9 in 0x1400e21c0 TutorialSetup_Ctor6; this+0x0 at 0x1400e222e in 0x1400e2200 TutorialSetupCamera_Ctor
### TutorialSetupDialog  table=0x140160d20  slots=12  sizeof=0x78  parent="TutorialSetup" / structural TutorialSetup 0x140160c30  stop=non-code qword 0x6c6169726f747554 (string "TutorialSetupGameSpeed")
  reflected: ti=0x1401c6710 create=0x1400285b0 createInPlace=0x1400285f0 TutorialSetupDialog_Slot11 registered in 0x14014dad0 RegisterType_TutorialSetupDialog
  [ 0] +0x00 0x140028620  OWN TutorialSetupDialog_ScalarDtor (48 B)
  [ 1] +0x08 0x1400285a0  TINY returns qword [0x1401c6718]
  [ 2] +0x10 0x140028590  TINY returns dword [0x1401c6710] = TypeId(TutorialSetupDialog)
  [ 3] +0x18 0x1400e2be0  TINY return 6 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400e2a60  OWN TutorialSetupDialog_Slot5 (364 B)
  [ 6] +0x30 0x140028600  OWN TutorialSetupDialog_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400e25f0  OWN TutorialSetupDialog_OnRemove (265 B)
  [ 9] +0x48 0x1400e24c0  OWN TutorialSetupDialog_Update (29 B)
  [10] +0x50 0x1400e2460  OWN TutorialSetupDialog_Slot10 (75 B)
  [11] +0x58 0x1400285f0  OWN TutorialSetupDialog_Slot11 (12 B)
  stamped by (2): this+0x0 at 0x1400e2c14 in 0x1400e2bf0 TutorialSetup_CopyCtor; this+0x0 at 0x1400e2cee in 0x1400e2cc0 TutorialSetupDialog_Ctor
### TutorialSetupGameSpeed  table=0x140160da0  slots=12  sizeof=0x28  parent="TutorialSetup" / structural TutorialSetup 0x140160c30  stop=non-code qword 0x6c6169726f747554 (string "TutorialSetupRestrictPlayer")
  reflected: ti=0x1401c6748 create=0x1400289c0 createInPlace=0x140028a20 TutorialSetupGameSpeed_Cleanup registered in 0x14014db10 TutorialSetupGameSpeed_RegisterType
  [ 0] +0x00 0x140028ac0  OWN TutorialSetupGameSpeed_ScalarDtor (83 B)
  [ 1] +0x08 0x1400289b0  TINY returns qword [0x1401c6750]
  [ 2] +0x10 0x1400289a0  TINY returns dword [0x1401c6748] = TypeId(TutorialSetupGameSpeed)
  [ 3] +0x18 0x1400e2df0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400e2d90  OWN TutorialSetupGameSpeed_Slot5 (89 B)
  [ 6] +0x30 0x140028a70  OWN TutorialSetupGameSpeed_Draw (80 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400e2d60  OWN TutorialSetupGameSpeed_OnRemove (39 B)
  [ 9] +0x48 0x1400e2d50  TINY RET 0 (c2 00 00)
  [10] +0x50 0x1400e2d40  TINY RET 0 (c2 00 00)
  [11] +0x58 0x140028a20  OWN TutorialSetupGameSpeed_Cleanup (79 B)
  stamped by (6): other+0x0 at 0x140028a05 in 0x1400289c0 TutorialSetupGameSpeed_Factory; this+0x0 at 0x140028a54 in 0x140028a20 TutorialSetupGameSpeed_Cleanup; this+0x0 at 0x140028aa9 in 0x140028a70 TutorialSetupGameSpeed_Draw; this+0x0 at 0x140028ae4 in 0x140028ac0 TutorialSetupGameSpeed_ScalarDtor; this+0x0 at 0x1400e2e19 in 0x1400e2e00 TutorialSetup_Ctor3; this+0x0 at 0x1400e2e6e in 0x1400e2e40 TutorialSetupGameSpeed_Ctor
### TutorialSetupRestrictPlayer  table=0x140160e20  slots=12  sizeof=0x68  parent="TutorialSetup" / structural TutorialSetup 0x140160c30  stop=non-code qword 0x6c6169726f747554 (string "TutorialStep")
  reflected: ti=0x1401c6780 create=0x140028b40 createInPlace=0x140028b80 TutorialSetupRestrictPlayer_Slot11 registered in 0x14014db50 TutorialSetupRestrictPlayer_RegisterType
  [ 0] +0x00 0x140028bb0  OWN TutorialSetupRestrictPlayer_ScalarDtor (118 B)
  [ 1] +0x08 0x140028b30  TINY returns qword [0x1401c6788]
  [ 2] +0x10 0x140028b20  TINY returns dword [0x1401c6780] = TypeId(TutorialSetupRestrictPlayer)
  [ 3] +0x18 0x1400e31b0  TINY return 4 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x1400e3090  OWN TutorialSetupRestrictPlayer_Slot5 (271 B)
  [ 6] +0x30 0x140028b90  OWN TutorialSetupRestrictPlayer_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400e2f20  OWN TutorialSetupRestrictPlayer_OnRemove (353 B)
  [ 9] +0x48 0x1400e2f10  TINY RET 0 (c2 00 00)
  [10] +0x50 0x1400e2e90  OWN TutorialSetupRestrictPlayer_Slot10 (121 B)
  [11] +0x58 0x140028b80  OWN TutorialSetupRestrictPlayer_Slot11 (12 B)
  stamped by (3): this+0x0 at 0x140028bd4 in 0x140028bb0 TutorialSetupRestrictPlayer_ScalarDtor; this+0x0 at 0x1400e31dd in 0x1400e31c0 TutorialSetup_Ctor; this+0x0 at 0x1400e324e in 0x1400e3220 TutorialSetupRestrictPlayer_Ctor
### TutorialStep  table=0x140160e90  slots=9  sizeof=0x70  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6c6169726f747554 (string "TutorialUI")
  reflected: ti=0x1401c67b8 create=0x140028c50 createInPlace=0x140028c90 TutorialStep_OnRemove registered in 0x14014db90 TutorialStep_RegisterType
  [ 0] +0x00 0x140028cc0  OWN TutorialStep_ScalarDtor (48 B)
  [ 1] +0x08 0x140028c40  TINY returns qword [0x1401c67c0]
  [ 2] +0x10 0x140028c30  TINY returns dword [0x1401c67b8] = TypeId(TutorialStep)
  [ 3] +0x18 0x1400e33e0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400e32a0  OWN TutorialStep_Slot5 (314 B)
  [ 6] +0x30 0x140028ca0  OWN TutorialStep_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140028c90  OWN TutorialStep_OnRemove (12 B)
  stamped by (2): this+0x0 at 0x1400e3412 in 0x1400e33f0 TutorialStep_Ctor; this+0x0 at 0x1400e34a4 in 0x1400e3480 TutorialStep_Ctor
### TutorialUI  table=0x140160f30  slots=11  sizeof=0x60  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x6b6369506f747541 (string "AutoPickupWork")
  reflected: ti=0x1401c67f0 create=0x140029400 createInPlace=IMPORT ?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z registered in 0x14014dbd0 RegisterType_TutorialUI
  [ 0] +0x00 0x140029470  OWN TutorialUI_ScalarDtor (73 B)
  [ 1] +0x08 0x1400293f0  TINY returns qword [0x1401c67f8]
  [ 2] +0x10 0x1400293e0  TINY returns dword [0x1401c67f0] = TypeId(TutorialUI)
  [ 3] +0x18 0x1400e3aa0  TINY RET 0 (c2 00 00)
  [ 4] +0x20 0x1400e37a0  OWN TutorialUI_Serialize (749 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x1400e3700  OWN TutorialUI_OnCreate (152 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x1400e36e0  OWN TutorialUI_OnRemove (21 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (4): other+0x0 at 0x14002943b in 0x140029400 Factory_TutorialUI; this+0x0 at 0x140029494 in 0x140029470 TutorialUI_ScalarDtor; this+0x0 at 0x1400e3ac9 in 0x1400e3ab0 TutorialUI_Ctor; this+0x0 at 0x1400e3b04 in 0x1400e3ae0 TutorialUI_CopyCtor
### UIComponent  table=0x1401649f0  slots=16  sizeof=0x38  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c7fc8 create=0x140039550 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014fd50 UIComponent_RegisterType
  [ 0] +0x00 0x140039760  OWN UIComponent_ScalarDtor (106 B)
  [ 1] +0x08 0x140039540  TINY returns qword [0x1401c7fd0]
  [ 2] +0x10 0x140039530  TINY returns dword [0x1401c7fc8] = TypeId(UIComponent)
  [ 3] +0x18 0x14012dd00  OWN UIComponent_Shutdown (144 B)
  [ 4] +0x20 0x14012dca0  OWN UIComponent_Serialize (87 B)
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x140118830  TINY RET 0 (c2 00 00)  [in 34 tables]   = parent
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140039784 in 0x140039760 UIComponent_ScalarDtor; this+0x0 at 0x14012ddbd in 0x14012dda0 UIComponent_Ctor; this+0x0 at 0x14012de28 in 0x14012de00 UIComponent_Ctor
### UIDescription  table=0x140164a70  slots=10  sizeof=0x48  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6d696e41646c6957 (string "WildAnimalComponent")
  reflected: ti=0x1401c7f90 create=0x1400395b0 createInPlace=0x140039620 UIDescription_CreateInPlace registered in 0x14014fd10 UIDescription_Register
  [ 0] +0x00 0x140039700  OWN UIComponent_vslot16 (96 B)
  [ 1] +0x08 0x1400395a0  TINY returns qword [0x1401c7f98]
  [ 2] +0x10 0x140039590  TINY returns dword [0x1401c7f90] = TypeId(UIDescription)
  [ 3] +0x18 0x14012dfa0  TINY return 4 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14012de80  OWN FUN_14012de80 (272 B)
  [ 6] +0x30 0x140039680  OWN FUN_140039680 (89 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x1400396e0  OWN (no Ghidra function)
  [ 9] +0x48 0x140039620  OWN UIDescription_CreateInPlace (88 B)
  stamped by (6): other+0x0 at 0x1400395f5 in 0x1400395b0 UIDescription_Factory; this+0x0 at 0x140039654 in 0x140039620 UIDescription_CreateInPlace; this+0x0 at 0x1400396b9 in 0x140039680 FUN_140039680; this+0x0 at 0x140039724 in 0x140039700 UIComponent_vslot16; this+0x0 at 0x14012dfcd in 0x14012dfb0 FUN_14012dfb0; this+0x0 at 0x14012e02e in 0x14012e000 FUN_14012e000
### UpgradeUI  table=0x14015df00  slots=11  sizeof=0x38  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=zero qword
  reflected: ti=0x1401c5830 create=0x140016b30 createInPlace=0x140016b90 Cleanup_UpgradeUI registered in 0x14014c9c0 RegisterType_UpgradeUI
  [ 0] +0x00 0x140016d50  OWN UpgradeUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140016b20  TINY returns qword [0x1401c5838]
  [ 2] +0x10 0x140016b10  TINY returns dword [0x1401c5830] = TypeId(UpgradeUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x140086d90  OWN UpgradeUI_Serialize (413 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x140086d80  TINY RET 0 (c2 00 00)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x140086c90  OWN UpgradeUI_Update (220 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (5): other+0x0 at 0x140016b6b in 0x140016b30 Factory_UpgradeUI; this+0x0 at 0x140016bba in 0x140016b90 Cleanup_UpgradeUI; this+0x0 at 0x140016d74 in 0x140016d50 UpgradeUI_ScalarDtor; this+0x0 at 0x140086f59 in 0x140086f40 UpgradeUI_Ctor; this+0x0 at 0x140086f94 in 0x140086f70 UpgradeUI_CopyCtor
### UpgradeUIConfig  table=0x14015df60  slots=9  sizeof=0x28  parent="UI::DialogControllerConfig" / structural UI::DialogControllerConfig (Runtime ctor import)  stop=non-code qword 0x63616c506b726f57 (string "WorkPlaceUI")
  reflected: ti=0x1401c5868 create=0x140016c00 createInPlace=0x140016c60 UpgradeUIConfig_CreateInPlace registered in 0x14014ca00 UpgradeUIConfig_Register
  [ 0] +0x00 0x140016d00  OWN UpgradeUI_Slot12 (73 B)
  [ 1] +0x08 0x140016bf0  TINY returns qword [0x1401c5870]
  [ 2] +0x10 0x140016be0  TINY returns dword [0x1401c5868] = TypeId(UpgradeUIConfig)
  [ 3] +0x18 0x140087000  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x140086fb0  OWN UpgradeUI_vslot17 (67 B)
  [ 6] +0x30 0x140016cb0  OWN UpgradeUI_vslot18 (67 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140016c60  OWN UpgradeUIConfig_CreateInPlace (66 B)
  stamped by (6): other+0x0 at 0x140016c3b in 0x140016c00 UpgradeUIConfig_Factory; this+0x0 at 0x140016c8a in 0x140016c60 UpgradeUIConfig_CreateInPlace; this+0x0 at 0x140016cdf in 0x140016cb0 UpgradeUI_vslot18; this+0x0 at 0x140016d24 in 0x140016d00 UpgradeUI_Slot12; this+0x0 at 0x140087029 in 0x140087010 FUN_140087010; this+0x0 at 0x140087064 in 0x140087040 FUN_140087040
### ValueDescription  table=0x14015add0  slots=10  sizeof=0x50  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x706d6f436c6c6557 (string "WellComponent")
  reflected: ti=0x1401c4bf0 create=0x14000faa0 createInPlace=0x14000fb10 ValueDescription_CreateInPlace registered in 0x14014bb00 ValueDescription_RegisterType
  [ 0] +0x00 0x14000fbf0  OWN ValueDescription_ScalarDtor (108 B)
  [ 1] +0x08 0x14000fa90  TINY returns qword [0x1401c4bf8]
  [ 2] +0x10 0x14000fa80  TINY returns dword [0x1401c4bf0] = TypeId(ValueDescription)
  [ 3] +0x18 0x14005ce10  TINY return 3 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14005cd60  OWN ValueDescription_Slot5 (165 B)
  [ 6] +0x30 0x14000fb80  OWN ValueDescription_Draw (98 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140004950  OWN CreateFencedDescription_OnRemove (10 B)  [in 19 tables]
  [ 9] +0x48 0x14000fb10  OWN ValueDescription_CreateInPlace (97 B)
  stamped by (6): other+0x0 at 0x14000fae5 in 0x14000faa0 ValueDescription_Factory; this+0x0 at 0x14000fb44 in 0x14000fb10 ValueDescription_CreateInPlace; this+0x0 at 0x14000fbb9 in 0x14000fb80 ValueDescription_Draw; this+0x0 at 0x14000fc14 in 0x14000fbf0 ValueDescription_ScalarDtor; this+0x0 at 0x14005ce3d in 0x14005ce20 ValueDescription_Ctor; this+0x0 at 0x14005ceae in 0x14005ce80 ValueDescription_CopyCtor
### Weather  table=0x140160290  slots=9  sizeof=0x80  parent="Object::External" / structural Object::External (Runtime ctor import)  stop=non-code qword 0x6d65766569686341 (string "Achievement::Goal")
  reflected: ti=0x1401c6198 create=0x140024fb0 createInPlace=0x140024ff0 Weather_OnRemove registered in 0x14014d490 Weather_RegisterType
  [ 0] +0x00 0x140025020  OWN Weather_ScalarDtor (63 B)
  [ 1] +0x08 0x140024fa0  TINY returns qword [0x1401c61a0]
  [ 2] +0x10 0x140024f90  TINY returns dword [0x1401c6198] = TypeId(Weather)
  [ 3] +0x18 0x1400d3e40  TINY return 5 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]
  [ 5] +0x28 0x1400d3c90  OWN Weather_Slot5 (415 B)
  [ 6] +0x30 0x140025000  OWN Weather_Draw (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140024ff0  OWN Weather_OnRemove (12 B)
  stamped by (1): this+0x0 at 0x1400d3e74 in 0x1400d3e50 Weather_Ctor
### WellComponent  table=0x14015ae90  slots=18  sizeof=0x48  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c4c28 create=0x14000fc80 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014bb40 WellComponent_RegisterType
  member vptrs: +0x28=0x14015ae40, +0x28=0x14015f070
  [ 0] +0x00 0x14000fe50  OWN WellComponent_Controller_Slot10 (115 B)
  [ 1] +0x08 0x14000fc70  TINY returns qword [0x1401c4c30]
  [ 2] +0x10 0x14000fc60  TINY returns dword [0x1401c4c28] = TypeId(WellComponent)
  [ 3] +0x18 0x14005cfa0  OWN WellComponent_Controller_Slot13 (76 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x14005cf60  OWN WellComponent_OnCreate (47 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x1400026c0  TINY return 2 (mov eax)  [in 22 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (3): this+0x0 at 0x14000fe74 in 0x14000fe50 WellComponent_Controller_Slot10; this+0x0 at 0x14005d019 in 0x14005d000 WellComponent_Ctor; this+0x0 at 0x14005d092 in 0x14005d060 WellComponent_Ctor
### WellDescription  table=0x14015af20  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x6d65766569686341 (string "AchievementDialog")
  reflected: ti=0x1401c4c60 create=0x14000fce0 createInPlace=0x14000fd40 WellDescription_CreateInPlace registered in 0x14014bb80 WellDescription_Register
  [ 0] +0x00 0x14000fdf0  OWN WellComponent_vslot18 (83 B)
  [ 1] +0x08 0x14000fcd0  TINY returns qword [0x1401c4c68]
  [ 2] +0x10 0x14000fcc0  TINY returns dword [0x1401c4c60] = TypeId(WellDescription)
  [ 3] +0x18 0x14005d1b0  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14005d170  OWN FUN_14005d170 (46 B)
  [ 6] +0x30 0x14000fd90  OWN FUN_14000fd90 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14000fde0  OWN (no Ghidra function)
  [ 9] +0x48 0x14000fd40  OWN WellDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x14000fd25 in 0x14000fce0 WellDescription_Factory; this+0x0 at 0x14000fd74 in 0x14000fd40 WellDescription_CreateInPlace; this+0x0 at 0x14000fdc9 in 0x14000fd90 FUN_14000fd90; this+0x0 at 0x14000fe14 in 0x14000fdf0 WellComponent_vslot18; this+0x0 at 0x14005d1d9 in 0x14005d1c0 FUN_14005d1c0; this+0x0 at 0x14005d22e in 0x14005d200 FUN_14005d200
### WildAnimalComponent  table=0x140164af0  slots=16  sizeof=0x30  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c8000 create=0x140039c20 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014fd90 WildAnimalComponent_RegisterType
  [ 0] +0x00 0x140039df0  OWN WildAnimalComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x140039c10  TINY returns qword [0x1401c8008]
  [ 2] +0x10 0x140039c00  TINY returns dword [0x1401c8000] = TypeId(WildAnimalComponent)
  [ 3] +0x18 0x14012e300  OWN WildAnimalComponent_Serialize (119 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x14012e2b0  OWN WildAnimalComponent_OnRemove (63 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140039e14 in 0x140039df0 WildAnimalComponent_ScalarDtor; this+0x0 at 0x14012e399 in 0x14012e380 WildAnimalComponent_Ctor; this+0x0 at 0x14012e3e8 in 0x14012e3c0 WildAnimalComponent_Ctor
### WildAnimalDescription  table=0x140164b70  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x637365446b726f57 (string "WorkDescription")
  reflected: ti=0x1401c8038 create=0x140039c80 createInPlace=0x140039ce0 WildAnimalDescription_CreateInPlace registered in 0x14014fdd0 WildAnimalDescription_Register
  [ 0] +0x00 0x140039d90  OWN WildAnimalComponent_GetTypeId (83 B)
  [ 1] +0x08 0x140039c70  TINY returns qword [0x1401c8040]
  [ 2] +0x10 0x140039c60  TINY returns dword [0x1401c8038] = TypeId(WildAnimalDescription)
  [ 3] +0x18 0x14012e480  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14012e440  OWN WildAnimalComponent_OnCreate (46 B)
  [ 6] +0x30 0x140039d30  OWN WildAnimalComponent_OnDestroy (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140039d80  OWN WildAnimalComponent_OnCleanup (12 B)
  [ 9] +0x48 0x140039ce0  OWN WildAnimalDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x140039cc5 in 0x140039c80 WildAnimalDescription_Factory; this+0x0 at 0x140039d14 in 0x140039ce0 WildAnimalDescription_CreateInPlace; this+0x0 at 0x140039d69 in 0x140039d30 WildAnimalComponent_OnDestroy; this+0x0 at 0x140039db4 in 0x140039d90 WildAnimalComponent_GetTypeId; this+0x0 at 0x14012e4a9 in 0x14012e490 FUN_14012e490; this+0x0 at 0x14012e4fe in 0x14012e4d0 FUN_14012e4d0
### Work  table=0x140161a80  slots=13  sizeof=abstract  parent="Object::Internal" / structural Object::Internal (Runtime ctor import)  stop=non-code qword 0x6f54656764697242 (string "BridgeTool")
  reflected: ti=0x1401c6cf8 create=0x140003730 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014e190 Work_RegisterType
  [ 0] +0x00 0x14002ca60  OWN TradeWork_Slot14 (73 B)
  [ 1] +0x08 0x14002ca50  TINY returns qword [0x1401c6d00]
  [ 2] +0x10 0x14002ca40  TINY returns dword [0x1401c6cf8] = TypeId(Work)
  [ 3] +0x18 0x1400f7400  OWN Work_Shutdown (302 B)
  [ 4] +0x20 0x14003addc  _purecall  [in 12 tables]
  [ 5] +0x28 0x1400f7540  TINY return false (xor al,al)  [in 22 tables]
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]
  [ 9] +0x48 0x14003addc  _purecall  [in 12 tables]
  [10] +0x50 0x14003addc  _purecall  [in 12 tables]
  [11] +0x58 0x14003addc  _purecall  [in 12 tables]
  [12] +0x60 0x140006b10  TINY RET 0 (c2 00 00)  [in 25 tables]
  stamped by (47): this+0x0 at 0x14002950e in 0x1400294e0 AutoPickupWork_ScalarDtor; this+0x0 at 0x1400295de in 0x1400295b0 BuildRoad_ScalarDtor; this+0x0 at 0x14002a87e in 0x14002a850 LocationWork_ScalarDtor; this+0x0 at 0x14002a95f in 0x14002a910 LocationWork_Slot14; this+0x0 at 0x14002c03e in 0x14002c010 Pickup_ScalarDtor; this+0x0 at 0x14002ca84 in 0x14002ca60 TradeWork_Slot14; this+0x0 at 0x1400e3fe3 in 0x1400e3fc0 Work_CopyCtor; this+0x0 at 0x1400e4074 in 0x1400e4050 AutoPickupWork_Ctor; this+0x0 at 0x1400e4ab3 in 0x1400e4a90 Work_Ctor8; this+0x0 at 0x1400e4af4 in 0x1400e4ad0 BuildRoad_Ctor; this+0x0 at 0x1400e599d in 0x1400e5920 Work_Ctor9; this+0x0 at 0x1400e5ab4 in 0x1400e5a90 BuildStructure_Ctor ...
### WorkComponent  table=0x140164be0  slots=16  sizeof=0x38  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c8070 create=0x140039e70 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014fe10 WorkComponent_RegisterType
  [ 0] +0x00 0x140039fb0  OWN WorkComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x140039e60  TINY returns qword [0x1401c8078]
  [ 2] +0x10 0x140039e50  TINY returns dword [0x1401c8070] = TypeId(WorkComponent)
  [ 3] +0x18 0x14012e710  OWN WorkComponent_Shutdown (196 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x14012e6f0  OWN WorkComponent_Draw (23 B)
  [ 7] +0x38 0x14012e6c0  OWN WorkComponent_OnCreate (37 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x14012e6b0  TINY return 0 (xor eax,eax)
  [14] +0x70 0x14012e6a0  TINY RET 0 (c2 00 00)
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x140039fd4 in 0x140039fb0 WorkComponent_ScalarDtor; this+0x0 at 0x14012e7f9 in 0x14012e7e0 WorkComponent_Ctor; this+0x0 at 0x14012e848 in 0x14012e820 WorkComponent_Ctor
### WorkDescription  table=0x140164c60  slots=10  sizeof=0x38  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x63616c506b726f57 (string "WorkPlaceComponent")
  reflected: ti=0x1401c80a8 create=0x140039ed0 createInPlace=0x140039f10 WorkDescription_CreateInPlace registered in 0x14014fe50 WorkDescription_Register
  [ 0] +0x00 0x140039f50  OWN WorkComponent_vslot16 (83 B)
  [ 1] +0x08 0x140039ec0  TINY returns qword [0x1401c80b0]
  [ 2] +0x10 0x140039eb0  TINY returns dword [0x1401c80a8] = TypeId(WorkDescription)
  [ 3] +0x18 0x14012e960  TINY return 4 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14012e8b0  OWN FUN_14012e8b0 (161 B)
  [ 6] +0x30 0x140039f20  OWN FUN_140039f20 (29 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x140039f40  OWN (no Ghidra function)
  [ 9] +0x48 0x140039f10  OWN WorkDescription_CreateInPlace (12 B)
  stamped by (3): this+0x0 at 0x140039f74 in 0x140039f50 WorkComponent_vslot16; this+0x0 at 0x14012e989 in 0x14012e970 FUN_14012e970; this+0x0 at 0x14012e9de in 0x14012e9b0 WorkDescription_sub_e9b0
### WorkPlaceComponent  table=0x140164d30  slots=18  sizeof=0x68  parent="ControllerComponent" / structural ControllerComponent 0x140162b80  stop=next table
  reflected: ti=0x1401c80e0 create=0x14003a030 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014fe90 WorkPlaceComponent_RegisterType
  member vptrs: +0x28=0x14015f070, +0x28=0x140164ce0
  [ 0] +0x00 0x14003a220  OWN WorkPlaceComponent_Controller_Slot10 (48 B)
  [ 1] +0x08 0x14003a020  TINY returns qword [0x1401c80e8]
  [ 2] +0x10 0x14003a010  TINY returns dword [0x1401c80e0] = TypeId(WorkPlaceComponent)
  [ 3] +0x18 0x14012f120  OWN WorkPlaceComponent_Controller_Slot13 (325 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x140118820  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 7] +0x38 0x14012f090  OWN WorkPlaceComponent_OnRemove (126 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  [16] +0x80 0x140003730  TINY return 0 (xor eax,eax)  [in 5 tables]
  [17] +0x88 0x140004080  TINY returns this+0x28  [in 23 tables]
  stamped by (2): this+0x0 at 0x14012f28d in 0x14012f270 WorkPlaceComponent_Ctor; this+0x0 at 0x14012f312 in 0x14012f2e0 WorkPlaceComponent_Ctor
### WorkPlaceDescription  table=0x140164dc0  slots=10  sizeof=0x30  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0x63736544656e6f5a (string "ZoneDescription")
  reflected: ti=0x1401c8118 create=0x14003a090 createInPlace=0x14003a100 WorkPlaceDescription_CreateInPlace registered in 0x14014fed0 WorkPlaceDescription_Register
  [ 0] +0x00 0x14003a1c0  OWN WorkPlaceComponent_vslot18 (83 B)
  [ 1] +0x08 0x14003a080  TINY returns qword [0x1401c8120]
  [ 2] +0x10 0x14003a070  TINY returns dword [0x1401c8118] = TypeId(WorkPlaceDescription)
  [ 3] +0x18 0x14012f480  TINY return 2 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x14012f420  OWN FUN_14012f420 (81 B)
  [ 6] +0x30 0x14003a160  OWN FUN_14003a160 (79 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14003a1b0  OWN (no Ghidra function)
  [ 9] +0x48 0x14003a100  OWN WorkPlaceDescription_CreateInPlace (84 B)
  stamped by (6): other+0x0 at 0x14003a0d5 in 0x14003a090 WorkPlaceDescription_Factory; this+0x0 at 0x14003a134 in 0x14003a100 WorkPlaceDescription_CreateInPlace; this+0x0 at 0x14003a199 in 0x14003a160 FUN_14003a160; this+0x0 at 0x14003a1e4 in 0x14003a1c0 WorkPlaceComponent_vslot18; this+0x0 at 0x14012f4a9 in 0x14012f490 FUN_14012f490; this+0x0 at 0x14012f4fe in 0x14012f4d0 FUN_14012f4d0
### WorkPlaceUI  table=0x14015e030  slots=11  sizeof=0x68  parent="UI::DialogController" / structural UI::DialogController (Runtime ctor import)  stop=non-code qword 0x696475416d696e41 (string "AnimAudioEvent")
  reflected: ti=0x1401c58a0 create=0x140016dc0 createInPlace=0x140016e30 Cleanup_WorkPlaceUI registered in 0x14014ca40 RegisterType_WorkPlaceUI
  [ 0] +0x00 0x140016e90  OWN WorkPlaceUI_ScalarDtor (73 B)
  [ 1] +0x08 0x140016db0  TINY returns qword [0x1401c58a8]
  [ 2] +0x10 0x140016da0  TINY returns dword [0x1401c58a0] = TypeId(WorkPlaceUI)
  [ 3] +0x18 0x14003a95a  IMPORT ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z  [in 36 tables]
  [ 4] +0x20 0x140087940  OWN WorkPlaceUI_Serialize (1143 B)
  [ 5] +0x28 0x14003a960  IMPORT ?OnOpened@DialogController@UI@@UEAAXXZ  [in 33 tables]
  [ 6] +0x30 0x14003a966  IMPORT ?OnClosed@DialogController@UI@@UEAAXXZ  [in 32 tables]
  [ 7] +0x38 0x140087080  OWN WorkPlaceUI_OnCreate (1406 B)
  [ 8] +0x40 0x14003a96c  IMPORT ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  [in 28 tables]
  [ 9] +0x48 0x1400877e0  OWN WorkPlaceUI_Update (343 B)
  [10] +0x50 0x14003a978  IMPORT ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z  [in 37 tables]
  stamped by (5): other+0x0 at 0x140016dfb in 0x140016dc0 Factory_WorkPlaceUI; this+0x0 at 0x140016e5a in 0x140016e30 Cleanup_WorkPlaceUI; this+0x0 at 0x140016eb4 in 0x140016e90 WorkPlaceUI_ScalarDtor; this+0x0 at 0x140087dd9 in 0x140087dc0 WorkPlaceUI_Ctor; this+0x0 at 0x140087e14 in 0x140087df0 WorkPlaceUI_CopyCtor
### ZoneComponent  table=0x140164e30  slots=16  sizeof=0x28  parent="Component" / structural Component 0x140162920  stop=next table
  reflected: ti=0x1401c8150 create=0x14003a6a0 createInPlace=IMPORT ?CreateInPlaceObject@Internal@Object@@SAPEAVBase@2@PEAX@Z registered in 0x14014ff90 ZoneComponent_RegisterType
  [ 0] +0x00 0x14003a870  OWN ZoneComponent_ScalarDtor (83 B)
  [ 1] +0x08 0x14003a690  TINY returns qword [0x1401c8158]
  [ 2] +0x10 0x14003a680  TINY returns dword [0x1401c8150] = TypeId(ZoneComponent)
  [ 3] +0x18 0x1401302f0  OWN ZoneComponent_Shutdown (128 B)
  [ 4] +0x20 0x140118a50  TINY RET 0 (c2 00 00)  [in 40 tables]   = parent
  [ 5] +0x28 0x140118a60  TINY RET 0 (c2 00 00)  [in 70 tables]   = parent
  [ 6] +0x30 0x1401301f0  OWN ZoneComponent_Draw (247 B)
  [ 7] +0x38 0x140130080  OWN ZoneComponent_OnCreate (349 B)
  [ 8] +0x40 0x140118a70  TINY RET 0 (c2 00 00)  [in 62 tables]   = parent
  [ 9] +0x48 0x140118a80  TINY RET 0 (c2 00 00)  [in 51 tables]   = parent
  [10] +0x50 0x140118a90  TINY RET 0 (c2 00 00)  [in 69 tables]   = parent
  [11] +0x58 0x140118aa0  TINY RET 0 (c2 00 00)  [in 58 tables]   = parent
  [12] +0x60 0x140118ab0  TINY RET 0 (c2 00 00)  [in 61 tables]   = parent
  [13] +0x68 0x140118ac0  TINY return 0 (xor eax,eax)  [in 56 tables]   = parent
  [14] +0x70 0x140118ad0  TINY RET 0 (c2 00 00)  [in 56 tables]   = parent
  [15] +0x78 0x140118ae0  TINY RET 0 (c2 00 00)  [in 60 tables]   = parent
  stamped by (3): this+0x0 at 0x14003a894 in 0x14003a870 ZoneComponent_ScalarDtor; this+0x0 at 0x140130399 in 0x140130380 ZoneComponent_Ctor; this+0x0 at 0x1401303e8 in 0x1401303c0 ZoneComponent_Ctor
### ZoneDescription  table=0x140164eb0  slots=10  sizeof=0x20  parent="Description" / structural Description 0x1401629a0  stop=non-code qword 0xf19930522
  reflected: ti=0x1401c8188 create=0x14003a700 createInPlace=0x14003a760 ZoneDescription_CreateInPlace registered in 0x14014ffd0 ZoneDescription_Register
  [ 0] +0x00 0x14003a810  OWN ZoneComponent_vslot16 (83 B)
  [ 1] +0x08 0x14003a6f0  TINY returns qword [0x1401c8190]
  [ 2] +0x10 0x14003a6e0  TINY returns dword [0x1401c8188] = TypeId(ZoneDescription)
  [ 3] +0x18 0x140130470  TINY return 1 (mov eax)
  [ 4] +0x20 0x14003a912  IMPORT ?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z  [in 124 tables]   = parent
  [ 5] +0x28 0x140130430  OWN FUN_140130430 (46 B)
  [ 6] +0x30 0x14003a7b0  OWN FUN_14003a7b0 (69 B)
  [ 7] +0x38 0x140001640  OWN CitizenComponent_GetDescription (8 B)  [in 119 tables]
  [ 8] +0x40 0x14003a800  OWN (no Ghidra function)
  [ 9] +0x48 0x14003a760  OWN ZoneDescription_CreateInPlace (68 B)
  stamped by (6): other+0x0 at 0x14003a745 in 0x14003a700 ZoneDescription_Factory; this+0x0 at 0x14003a794 in 0x14003a760 ZoneDescription_CreateInPlace; this+0x0 at 0x14003a7e9 in 0x14003a7b0 FUN_14003a7b0; this+0x0 at 0x14003a834 in 0x14003a810 ZoneComponent_vslot16; this+0x0 at 0x140130499 in 0x140130480 FUN_140130480; this+0x0 at 0x1401304ee in 0x1401304c0 FUN_1401304c0
### ({AbsorbResourceComponent controller member})  table=0x140161fb0  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x14002ed90  OWN AbsorbResourceComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x14002edc0  OWN AbsorbResourceComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x14002ee20  OWN AbsorbResourceComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x14002ee70  OWN AbsorbResourceComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x14002eea0  OWN AbsorbResourceComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x14002eed0  OWN AbsorbResourceComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x14002ef00  OWN AbsorbResourceComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x14002ef30  OWN AbsorbResourceComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x14002ef60  OWN AbsorbResourceComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x140105d14 in 0x140105cc0 AbsorbResourceComponent_Ctor
### ({AttendanceComponent controller member})  table=0x140158c90  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140002a80  OWN AttendanceComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x140002ab0  OWN AttendanceComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x140002b10  OWN AttendanceComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x140002b60  OWN AttendanceComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140002b90  OWN AttendanceComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140002bc0  OWN AttendanceComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140002bf0  OWN AttendanceComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x140002c20  OWN AttendanceComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140002c50  OWN AttendanceComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x38 at 0x1400445e1 in 0x140044580 AttendanceComponent_Ctor
### {BoatComponent controller member}  table=0x140158ef0  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140003c30  OWN (no Ghidra function)
  [ 1] +0x08 0x140003c60  OWN (no Ghidra function)
  [ 2] +0x10 0x140003cc0  OWN (no Ghidra function)
  [ 3] +0x18 0x140003d10  OWN (no Ghidra function)
  [ 4] +0x20 0x140003d40  OWN (no Ghidra function)
  [ 5] +0x28 0x140003d70  OWN (no Ghidra function)
  [ 6] +0x30 0x140003da0  OWN (no Ghidra function)
  [ 7] +0x38 0x140003dd0  OWN (no Ghidra function)
  [ 8] +0x40 0x140003e00  OWN (no Ghidra function)
  stamped by (1): this+0x78 at 0x140046885 in 0x140046800 BoatComponent_Ctor
### ({BuildComponent controller member})  table=0x140162210  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x14002f6d0  OWN BuildComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x14002f700  OWN BuildComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x14002f760  OWN BuildComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x14002f7b0  OWN BuildComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x14002f7e0  OWN BuildComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x14002f810  OWN BuildComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x14002f840  OWN BuildComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x14002f870  OWN BuildComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x14002f8a0  OWN BuildComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x140109019 in 0x140108fc0 BuildComponent_Ctor
### ({CemeteryComponent controller member})  table=0x140159050  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140004410  OWN CemeteryComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x140004440  OWN CemeteryComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x1400044a0  OWN CemeteryComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x1400044f0  OWN CemeteryComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140004520  OWN CemeteryComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140004550  OWN CemeteryComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140004580  OWN CemeteryComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x1400045b0  OWN CemeteryComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x1400045e0  OWN CemeteryComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x140047749 in 0x1400476f0 CemeteryComponent_Ctor
### ({ClearAreaComponent controller member})  table=0x140162680  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140031310  OWN ClearAreaComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x140031340  OWN ClearAreaComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x1400313a0  OWN ClearAreaComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x1400313f0  OWN ClearAreaComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140031420  OWN ClearAreaComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140031450  OWN ClearAreaComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140031480  OWN ClearAreaComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x1400314b0  OWN ClearAreaComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x1400314e0  OWN ClearAreaComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x140117ff4 in 0x140117fa0 ClearAreaComponent_Ctor
### ({ClearResourceComponent controller member})  table=0x1401627f0  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x1400319c0  OWN ClearResourceComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x1400319f0  OWN ClearResourceComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x140031a50  OWN ClearResourceComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x140031aa0  OWN ClearResourceComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140031ad0  OWN ClearResourceComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140031b00  OWN ClearResourceComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140031b30  OWN ClearResourceComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x140031b60  OWN ClearResourceComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140031b90  OWN ClearResourceComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x1401186f4 in 0x1401186a0 ClearResourceComponent_Ctor
### {controller interface, 9 pure slots}  table=0x14015f070  slots=9  sizeof=?  parent=- / structural -  stop=zero qword
  [ 0] +0x00 0x14003addc  _purecall  [in 12 tables]
  [ 1] +0x08 0x14003addc  _purecall  [in 12 tables]
  [ 2] +0x10 0x14003addc  _purecall  [in 12 tables]
  [ 3] +0x18 0x14003addc  _purecall  [in 12 tables]
  [ 4] +0x20 0x14003addc  _purecall  [in 12 tables]
  [ 5] +0x28 0x14003addc  _purecall  [in 12 tables]
  [ 6] +0x30 0x14003addc  _purecall  [in 12 tables]
  [ 7] +0x38 0x14003addc  _purecall  [in 12 tables]
  [ 8] +0x40 0x14003addc  _purecall  [in 12 tables]
  stamped by (74): this+0x0 at 0x1400028a8 in 0x140002890 FUN_140002890; this+0x30 at 0x140005de7 in 0x140005db0 HappinessComponent_Controller_Slot10; this+0x28 at 0x140006697 in 0x140006660 HealerComponent_Controller_Slot10; this+0x28 at 0x140006da1 in 0x140006d60 HerbalistComponent_ScalarDtor; this+0x28 at 0x140008481 in 0x140008440 MarketComponent_ScalarDtor; this+0x28 at 0x140008937 in 0x140008900 MarketLocationComponent_Controller_Slot10; this+0x58 at 0x140009267 in 0x140009230 NomadComponent_Controller_Slot10; this+0x28 at 0x14000fe87 in 0x14000fe50 WellComponent_Controller_Slot10; this+0x28 at 0x14002ec27 in 0x14002ebf0 AbsorbResourceComponent_Controller_Slot10; this+0x28 at 0x1400310f7 in 0x1400310c0 ClearAreaComponent_Controller_Slot10; this+0x28 at 0x1400317a7 in 0x140031770 ClearResourceComponent_Controller_Slot10; this+0x28 at 0x140033e07 in 0x140033dd0 DestroyComponent_Controller_Slot10 ...
### ({CropFieldComponent controller member})  table=0x140162e70  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140033340  OWN CropFieldComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x140033370  OWN CropFieldComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x1400333d0  OWN CropFieldComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x140033420  OWN CropFieldComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140033450  OWN CropFieldComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140033480  OWN CropFieldComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x1400334b0  OWN CropFieldComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x1400334e0  OWN CropFieldComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140033510  OWN CropFieldComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x14011b029 in 0x14011afd0 CropFieldComponent_Ctor
### {CRT __C_specific_handler function pointer, not a vtable}  table=0x14017cb78  slots=1  sizeof=?  parent=- / structural -  stop=non-code qword 0x14017d780
  [ 0] +0x00 0x140132754  OWN __DestructExceptionObject (104 B)
  other refs (1):   0x14003b9cc __C_specific_handler+172 : LEA RCX,[0x14017cb78]
### {CRT _cinit function pointer, not a vtable}  table=0x14017cb70  slots=1  sizeof=?  parent=- / structural -  stop=next table
  [ 0] +0x00 0x1401326c4  JMP 0x1401326cc thunk_helper_26cc
  other refs (1):   0x14003b446 _cinit+22 : LEA RCX,[0x14017cb70]
### {CRT std::bad_exception vftable}  table=0x14017cb88  slots=2  sizeof=?  parent=- / structural {CRT std::exception vftable} 0x14017cc00  stop=non-code qword 0x6563786520646162 (string "bad exception")
  [ 0] +0x00 0x1401327f8  OWN meta_ptr_vslot1 (57 B)
  [ 1] +0x08 0x1401352a4  OWN meta_ptr_vslot2 (18 B)  [in 2 tables]   = parent
  stamped by (3): this+0x0 at 0x1401327d9 in 0x1401327c4 FUN_1401327c4; this+0x0 at 0x14013280e in 0x1401327f8 meta_ptr_vslot1; other+0xffffffffffffffe7 at 0x140132d9c in 0x1401328f8 FUN_1401328f8
### {CRT std::exception vftable}  table=0x14017cc00  slots=2  sizeof=?  parent=- / structural -  stop=non-code qword 0x206e776f6e6b6e55 (string "Unknown exception")
  [ 0] +0x00 0x1401351e4  OWN meta_ptr_vslot3 (57 B)
  [ 1] +0x08 0x1401352a4  OWN meta_ptr_vslot2 (18 B)  [in 2 tables]
  stamped by (4): this+0x0 at 0x14013514a in 0x140135134 exception; this+0x0 at 0x140135179 in 0x140135164 exception; this+0x0 at 0x140135197 in 0x140135190 _Tidy_thunk; this+0x0 at 0x1401351fa in 0x1401351e4 meta_ptr_vslot3
### {CRT vftable}  table=0x14017cbf0  slots=1  sizeof=?  parent=- / structural -  stop=non-code qword 0x14017d8c8
  [ 0] +0x00 0x1401350f8  OWN meta_ptr_vslot14 (57 B)
  stamped by (1): this+0x0 at 0x14013510e in 0x1401350f8 meta_ptr_vslot14
### ({DestroyComponent controller member})  table=0x1401630d0  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x1400340c0  OWN DestroyComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x1400340f0  OWN DestroyComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x140034150  OWN DestroyComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x1400341a0  OWN DestroyComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x1400341d0  OWN DestroyComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140034200  OWN DestroyComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140034230  OWN DestroyComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x140034260  OWN DestroyComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140034290  OWN DestroyComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x14011dcd4 in 0x14011dc80 DestroyComponent_Ctor
### {exe Graphics::Geometry subclass A}  table=0x14015f110  slots=9  sizeof=alloc 0x230  parent=- / structural Graphics::Geometry (Runtime ctor import)  stop=non-code qword 0x476e696172726554 (string "TerrainGenerator")
  [ 0] +0x00 0x14001ecc0  OWN TradeWork_impl_ecc0 (48 B)
  [ 1] +0x08 0x1400aed40  JMP 0x1400aad50 CitizenAI_impl_ad50
  [ 2] +0x10 0x1400aea10  OWN FUN_1400aea10 (803 B)
  [ 3] +0x18 0x1400ae490  OWN FUN_1400ae490 (1387 B)
  [ 4] +0x20 0x14003aa7a  IMPORT ?UpdateAttached@Geometry@Graphics@@UEAAXXZ  [in 7 tables]
  [ 5] +0x28 0x14003aaa4  IMPORT ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z  [in 6 tables]
  [ 6] +0x30 0x14003ab22  IMPORT ?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z  [in 4 tables]
  [ 7] +0x38 0x14003ab1c  IMPORT ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z  [in 6 tables]
  [ 8] +0x40 0x14003aaaa  IMPORT ?Clean@Geometry@Graphics@@UEAA_NXZ  [in 7 tables]
  stamped by (2): this+0x0 at 0x1400aed74 in 0x1400aed50 TradeWork_impl_ed50; this+0x0 at 0x1400b0109 in 0x1400b00e0 FUN_1400b00e0
### {exe Graphics::Geometry subclass B}  table=0x140160240  slots=9  sizeof=alloc 0xf0  parent=- / structural Graphics::Geometry (Runtime ctor import)  stop=zero qword
  [ 0] +0x00 0x140025060  OWN FUN_140025060 (48 B)
  [ 1] +0x08 0x1400d2e60  OWN FUN_1400d2e60 (582 B)
  [ 2] +0x10 0x1400d3120  OWN FUN_1400d3120 (2783 B)
  [ 3] +0x18 0x1400d30b0  OWN FUN_1400d30b0 (101 B)
  [ 4] +0x20 0x14003aa7a  IMPORT ?UpdateAttached@Geometry@Graphics@@UEAAXXZ  [in 7 tables]
  [ 5] +0x28 0x14003aaa4  IMPORT ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z  [in 6 tables]
  [ 6] +0x30 0x14003ab22  IMPORT ?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z  [in 4 tables]
  [ 7] +0x38 0x14003ab1c  IMPORT ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z  [in 6 tables]
  [ 8] +0x40 0x14003aaaa  IMPORT ?Clean@Geometry@Graphics@@UEAA_NXZ  [in 7 tables]
  stamped by (2): this+0x0 at 0x1400d3c32 in 0x1400d3c10 FUN_1400d3c10; this+0x0 at 0x1400d3f34 in 0x1400d3f10 FUN_1400d3f10
### {exe Graphics::Geometry subclass, StatusIconComponent member}  table=0x14015f0c0  slots=9  sizeof=?  parent=- / structural Graphics::Geometry (Runtime ctor import)  stop=zero qword
  [ 0] +0x00 0x14001ebb0  OWN FUN_14001ebb0 (63 B)
  [ 1] +0x08 0x1400aa430  TINY RET 0 (c2 00 00)
  [ 2] +0x10 0x1400a9c10  OWN FUN_1400a9c10 (2071 B)
  [ 3] +0x18 0x1400a9b90  OWN FUN_1400a9b90 (110 B)
  [ 4] +0x20 0x14003aa7a  IMPORT ?UpdateAttached@Geometry@Graphics@@UEAAXXZ  [in 7 tables]
  [ 5] +0x28 0x14003aaa4  IMPORT ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z  [in 6 tables]
  [ 6] +0x30 0x14003ab22  IMPORT ?GetInstanceTransform@Geometry@Graphics@@UEBA?AVMatrix@Math@@HM@Z  [in 4 tables]
  [ 7] +0x38 0x14003ab1c  IMPORT ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z  [in 6 tables]
  [ 8] +0x40 0x14003aaaa  IMPORT ?Clean@Geometry@Graphics@@UEAA_NXZ  [in 7 tables]
  stamped by (2): this+0x0 at 0x1400aa504 in 0x1400aa4e0 FUN_1400aa4e0; this+0x28 at 0x14012c105 in 0x14012c0c0 StatusIconComponent_Ctor
### {exe Graphics::ParticleInstance subclass}  table=0x1401601e0  slots=10  sizeof=?  parent=- / structural Graphics::ParticleInstance (Runtime ctor import)  stop=non-code qword 0x72656874616557 (string "Weather")
  [ 0] +0x00 0x140024f50  OWN FUN_140024f50 (63 B)
  [ 1] +0x08 0x1400d2470  OWN CharacterComponent_impl_2470 (2473 B)
  [ 2] +0x10 0x14003ab2e  IMPORT ?Draw@ParticleInstance@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z  [in 2 tables]
  [ 3] +0x18 0x14003ab34  IMPORT ?AddDrawItems@ParticleInstance@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z  [in 2 tables]
  [ 4] +0x20 0x14003aa7a  IMPORT ?UpdateAttached@Geometry@Graphics@@UEAAXXZ  [in 7 tables]
  [ 5] +0x28 0x14003aaa4  IMPORT ?GetSubTransform@Geometry@Graphics@@UEBA?AVTransform@Math@@H@Z  [in 6 tables]
  [ 6] +0x30 0x14003ab3a  IMPORT ?GetInstanceTransform@ParticleInstance@Graphics@@UEBA?AVMatrix@Math@@HM@Z  [in 2 tables]
  [ 7] +0x38 0x14003ab1c  IMPORT ?SetSharedInstanceData@Geometry@Graphics@@UEBAHAEAVContext@Video@@HM@Z  [in 6 tables]
  [ 8] +0x40 0x14003aaaa  IMPORT ?Clean@Geometry@Graphics@@UEAA_NXZ  [in 7 tables]
  [ 9] +0x48 0x14003ab58  IMPORT ?SetParticle@ParticleInstance@Graphics@@UEAAXV?$ExternalPtr@VParticle@Graphics@@@Object@@@Z
  stamped by (2): other+0x0 at 0x1400d21ed in 0x1400d21b0 FUN_1400d21b0; this+0x0 at 0x1400d2e44 in 0x1400d2e20 FUN_1400d2e20
### {exe System::Thread subclass}  table=0x14015ef18  slots=2  sizeof=alloc 0x1f8  parent=- / structural System::Thread (Runtime ctor import)  stop=non-code qword 0x69737365666f7250 (string "Profession")
  [ 0] +0x00 0x1400a6360  OWN FUN_1400a6360 (190 B)
  [ 1] +0x08 0x14003ab46  IMPORT ?OnCompleted@Thread@System@@UEAAXXZ
  stamped by (2): this+0x0 at 0x1400a6744 in 0x1400a6720 BuildStructure_impl_6720; this+0x0 at 0x1400a6826 in 0x1400a67f0 FUN_1400a67f0
### {GameUpdateLoop} (g_GameUpdateLoop_Vtable)  table=0x14015e2f0  slots=12  sizeof=?  parent=- / structural Engine::UpdateLoop (Runtime ctor import)  stop=next table
  [ 0] +0x00 0x140019dd0  OWN FUN_140019dd0 (98 B)
  [ 1] +0x08 0x140097c00  OWN FUN_140097c00 (832 B)
  [ 2] +0x10 0x1400977c0  OWN CitizenComponent_impl_77c0 (1068 B)
  [ 3] +0x18 0x140096380  OWN FUN_140096380 (1181 B)
  [ 4] +0x20 0x140096010  OWN ModDialog_impl_6010 (868 B)
  [ 5] +0x28 0x140096000  TINY RET 0 (c2 00 00)
  [ 6] +0x30 0x140096b00  OWN MainMenuDialog_impl_6b00 (3244 B)
  [ 7] +0x38 0x1400959c0  OWN TradeComponent_impl_59c0 (192 B)
  [ 8] +0x40 0x140095970  OWN FUN_140095970 (70 B)
  [ 9] +0x48 0x140095930  OWN FUN_140095930 (54 B)
  [10] +0x50 0x140019d40  TINY returns literal "Game/Game" @0x14015e3d0
  [11] +0x58 0x14003a8f4  IMPORT ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ  [in 4 tables]
  stamped by (3): this+0x0 at 0x140019df4 in 0x140019dd0 FUN_140019dd0; this+0x0 at 0x14009802d in 0x140098010 FUN_140098010; this+0x0 at 0x14009809c in 0x140098060 FUN_140098060
### ({HappinessComponent controller member})  table=0x140159580  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140006000  OWN HappinessComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x140006030  OWN HappinessComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x140006090  OWN HappinessComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x1400060e0  OWN HappinessComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140006110  OWN HappinessComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140006140  OWN HappinessComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140006170  OWN HappinessComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x1400061a0  OWN HappinessComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x1400061d0  OWN HappinessComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x30 at 0x14004abbd in 0x14004ab60 HappinessComponent_Ctor
### ({HealerComponent controller member})  table=0x1401596e0  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x1400068a0  OWN HealerComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x1400068d0  OWN HealerComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x140006930  OWN HealerComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x140006980  OWN HealerComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x1400069b0  OWN HealerComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x1400069e0  OWN HealerComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140006a10  OWN HealerComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x140006a40  OWN HealerComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140006a70  OWN HealerComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x14004afc9 in 0x14004af70 HealerComponent_Ctor
### ({ImmigrationComponent controller member})  table=0x140159ae0  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140007e10  OWN ImmigrationComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x140007e40  OWN ImmigrationComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x140007ea0  OWN ImmigrationComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x140007ef0  OWN ImmigrationComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140007f20  OWN ImmigrationComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140007f50  OWN ImmigrationComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140007f80  OWN ImmigrationComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x140007fb0  OWN ImmigrationComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140007fe0  OWN ImmigrationComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x14004e694 in 0x14004e640 ImmigrationComponent_Ctor
### {LoaderLoop subclass inner, GameMain}  table=0x140158870  slots=12  sizeof=?  parent=- / structural -  stop=next table
  [ 0] +0x00 0x140001150  OWN FUN_140001150 (63 B)
  [ 1] +0x08 0x14003a8e2  IMPORT ?Startup@LoaderLoop@Engine@@UEAAXXZ  [in 2 tables]
  [ 2] +0x10 0x14003a8dc  IMPORT ?Shutdown@LoaderLoop@Engine@@UEAAXXZ  [in 2 tables]
  [ 3] +0x18 0x14003a8d6  IMPORT ?Update@LoaderLoop@Engine@@UEAA_NAEBVTime@System@@@Z  [in 2 tables]
  [ 4] +0x20 0x14003a8d0  IMPORT ?Draw@LoaderLoop@Engine@@UEAAXM@Z  [in 2 tables]
  [ 5] +0x28 0x14003a90c  IMPORT ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ  [in 4 tables]
  [ 6] +0x30 0x140001120  OWN FUN_140001120 (38 B)
  [ 7] +0x38 0x14003a906  IMPORT ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z  [in 4 tables]
  [ 8] +0x40 0x14003a8c4  IMPORT ?OnVideoError@LoaderLoop@Engine@@UEAAXXZ  [in 3 tables]
  [ 9] +0x48 0x14003a900  IMPORT ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ  [in 3 tables]
  [10] +0x50 0x140001110  TINY returns literal "Game/Global" @0x1401587a8
  [11] +0x58 0x14003a8f4  IMPORT ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ  [in 4 tables]
  stamped by (1): other+0x0 at 0x140042222 in 0x140041ef0 GameMain
### {LoaderLoop subclass outer} (g_GameLoaderLoop_Vtable)  table=0x1401588d0  slots=12  sizeof=?  parent=- / structural -  stop=non-code qword 0x6e6f69746341 (string "Action")
  [ 0] +0x00 0x140001190  OWN FUN_140001190 (49 B)
  [ 1] +0x08 0x14003a8e2  IMPORT ?Startup@LoaderLoop@Engine@@UEAAXXZ  [in 2 tables]
  [ 2] +0x10 0x14003a8dc  IMPORT ?Shutdown@LoaderLoop@Engine@@UEAAXXZ  [in 2 tables]
  [ 3] +0x18 0x14003a8d6  IMPORT ?Update@LoaderLoop@Engine@@UEAA_NAEBVTime@System@@@Z  [in 2 tables]
  [ 4] +0x20 0x14003a8d0  IMPORT ?Draw@LoaderLoop@Engine@@UEAAXM@Z  [in 2 tables]
  [ 5] +0x28 0x14003a90c  IMPORT ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ  [in 4 tables]
  [ 6] +0x30 0x14003a8ca  IMPORT ?Load@LoaderLoop@Engine@@UEAAXXZ
  [ 7] +0x38 0x14003a906  IMPORT ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z  [in 4 tables]
  [ 8] +0x40 0x14003a8c4  IMPORT ?OnVideoError@LoaderLoop@Engine@@UEAAXXZ  [in 3 tables]
  [ 9] +0x48 0x14003a900  IMPORT ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ  [in 3 tables]
  [10] +0x50 0x14003a8fa  IMPORT ?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ  [in 2 tables]
  [11] +0x58 0x14003a8f4  IMPORT ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ  [in 4 tables]
  stamped by (7): other+0x0 at 0x14004226f in 0x140041ef0 GameMain; other+0x0 at 0x1400b4702 in 0x1400b4540 FUN_1400b4540; other+0x0 at 0x1400b4970 in 0x1400b4790 FUN_1400b4790; other+0x0 at 0x1400b4bb3 in 0x1400b49f0 FUN_1400b49f0; other+0x0 at 0x1400b7caa in 0x1400b79b0 SaveLoadDialog_OnCreate; other+0x0 at 0x1400e1125 in 0x1400e0d70 FinishTutorialInstance_helper_0d70; other+0x0 at 0x1400e368e in 0x1400e34e0 TutorialUI_helper_34e0
### ({MarketLocationComponent controller member})  table=0x140159d80  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140008b40  OWN MarketLocationComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x140008b70  OWN MarketLocationComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x140008bd0  OWN MarketLocationComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x140008c20  OWN MarketLocationComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140008c50  OWN MarketLocationComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140008c80  OWN MarketLocationComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140008cb0  OWN MarketLocationComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x140008ce0  OWN MarketLocationComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140008d10  OWN MarketLocationComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x14004f0b9 in 0x14004f060 MarketLocationComponent_Ctor
### {MenuDialogLoopT} (g_MenuDialogLoopT_Vtable)  table=0x14015ed00  slots=13  sizeof=?  parent=- / structural Engine::DialogLoopT (Runtime ctor import)  stop=non-code qword 0x72656e654770614d (string "MapGenerator")
  [ 0] +0x00 0x14001a2e0  OWN FUN_14001a2e0 (48 B)
  [ 1] +0x08 0x14009b930  OWN OptionsDialog_impl_b930 (239 B)
  [ 2] +0x10 0x14009b280  OWN FUN_14009b280 (439 B)
  [ 3] +0x18 0x14009b6e0  OWN MainMenuDialog_impl_b6e0 (578 B)
  [ 4] +0x20 0x14009b440  OWN FUN_14009b440 (655 B)
  [ 5] +0x28 0x14003a90c  IMPORT ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ  [in 4 tables]
  [ 6] +0x30 0x14009a930  OWN MenuScene_impl_a930 (2269 B)
  [ 7] +0x38 0x14003a906  IMPORT ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z  [in 4 tables]
  [ 8] +0x40 0x14009a8d0  OWN (no Ghidra function)
  [ 9] +0x48 0x14009a8b0  OWN (no Ghidra function)
  [10] +0x50 0x14001a2d0  TINY returns literal "Game/Menu" @0x14015ec60
  [11] +0x58 0x14001a2c0  TINY return 1 (mov al)
  [12] +0x60 0x14009ba30  OWN MainMenuDialog_Ctor (188 B)
  stamped by (1): this+0x0 at 0x14009bb24 in 0x14009bb00 FUN_14009bb00
### ({MultiObjectComponent controller member})  table=0x140163ca0  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x1400362c0  OWN MultiObjectComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x1400362f0  OWN MultiObjectComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x140036350  OWN MultiObjectComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x1400363a0  OWN MultiObjectComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x1400363d0  OWN MultiObjectComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140036400  OWN MultiObjectComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140036430  OWN MultiObjectComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x140036460  OWN MultiObjectComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140036490  OWN MultiObjectComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x140125ac9 in 0x140125a70 MultiObjectComponent_Ctor
### ({NomadComponent controller member})  table=0x140159fd0  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140009520  OWN NomadComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x140009550  OWN NomadComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x1400095b0  OWN NomadComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x140009600  OWN NomadComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140009630  OWN NomadComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140009660  OWN NomadComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140009690  OWN NomadComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x1400096c0  OWN NomadComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x1400096f0  OWN NomadComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x58 at 0x14004fe20 in 0x14004fdb0 NomadComponent_Ctor
### ({OrchardComponent controller member})  table=0x14015a130  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140009d10  OWN OrchardComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x140009d40  OWN OrchardComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x140009da0  OWN OrchardComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x140009df0  OWN OrchardComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140009e20  OWN OrchardComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140009e50  OWN OrchardComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140009e80  OWN OrchardComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x140009eb0  OWN OrchardComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140009ee0  OWN OrchardComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x140051f99 in 0x140051f40 OrchardComponent_Ctor
### ({PastureComponent controller member})  table=0x140163f10  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140036b10  OWN PastureComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x140036b40  OWN PastureComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x140036ba0  OWN PastureComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x140036bf0  OWN PastureComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140036c20  OWN PastureComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140036c50  OWN PastureComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140036c80  OWN PastureComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x140036cb0  OWN PastureComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140036ce0  OWN PastureComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x48 at 0x140128148 in 0x1401280e0 PastureComponent_Ctor
### ({ResidenceComponent controller member})  table=0x140164270  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140037860  OWN ResidenceComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x140037890  OWN ResidenceComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x1400378f0  OWN ResidenceComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x140037940  OWN ResidenceComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140037970  OWN ResidenceComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x1400379a0  OWN ResidenceComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x1400379d0  OWN ResidenceComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x140037a00  OWN ResidenceComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140037a30  OWN ResidenceComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x14012a2f9 in 0x14012a2a0 ResidenceComponent_Ctor
### ({ResourceProduceComponent controller member})  table=0x1401643a0  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140038040  OWN ResourceProduceComponent_CtrlVtable_0 (44 B)
  [ 1] +0x08 0x140038070  OWN ResourceProduceComponent_CtrlVtable_1 (81 B)
  [ 2] +0x10 0x1400380d0  OWN ResourceProduceComponent_CtrlVtable_2 (76 B)
  [ 3] +0x18 0x140038120  OWN ResourceProduceComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140038150  OWN ResourceProduceComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140038180  OWN ResourceProduceComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x1400381b0  OWN ResourceProduceComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x1400381e0  OWN ResourceProduceComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140038210  OWN ResourceProduceComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x14012b424 in 0x14012b3d0 ResourceProduceComponent_Ctor
### {splash DialogLoaderLoopT subclass} (g_GameLoadDialogLoopT_Vtable)  table=0x140158800  slots=13  sizeof=?  parent=- / structural -  stop=zero qword
  [ 0] +0x00 0x140001010  OWN FUN_140001010 (63 B)
  [ 1] +0x08 0x140098fa0  OWN OptionsDialog_impl_8fa0 (190 B)
  [ 2] +0x10 0x140098f90  TINY RET 0 (c2 00 00)
  [ 3] +0x18 0x140098da0  OWN FUN_140098da0 (489 B)
  [ 4] +0x20 0x14003a8ee  IMPORT ?Draw@DialogLoaderLoopT@Engine@@UEAAXM@Z
  [ 5] +0x28 0x14003a90c  IMPORT ?DrawFailed@UpdateLoop@Engine@@UEAAXXZ  [in 4 tables]
  [ 6] +0x30 0x14003a8e8  IMPORT ?Load@DialogLoaderLoopT@Engine@@UEAAXXZ
  [ 7] +0x38 0x14003a906  IMPORT ?SaveRestore@UpdateLoop@Engine@@UEAAX_NPEAVSavedState@Save@@@Z  [in 4 tables]
  [ 8] +0x40 0x14003a8c4  IMPORT ?OnVideoError@LoaderLoop@Engine@@UEAAXXZ  [in 3 tables]
  [ 9] +0x48 0x14003a900  IMPORT ?OnVideoRestart@UpdateLoop@Engine@@UEAAXXZ  [in 3 tables]
  [10] +0x50 0x14003a8fa  IMPORT ?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ  [in 2 tables]
  [11] +0x58 0x14003a8f4  IMPORT ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ  [in 4 tables]
  [12] +0x60 0x140099070  OWN LoaderDialog_Ctor3 (185 B)
  stamped by (8): other+0x0 at 0x1400420be in 0x140041ef0 GameMain; other+0x0 at 0x140042187 in 0x140041ef0 GameMain; other+0x0 at 0x1400b4668 in 0x1400b4540 FUN_1400b4540; other+0x0 at 0x1400b48da in 0x1400b4790 FUN_1400b4790; other+0x0 at 0x1400b4b16 in 0x1400b49f0 FUN_1400b49f0; other+0x0 at 0x1400b7c09 in 0x1400b79b0 SaveLoadDialog_OnCreate; other+0x0 at 0x1400e1096 in 0x1400e0d70 FinishTutorialInstance_helper_0d70; other+0x0 at 0x1400e35f8 in 0x1400e34e0 TutorialUI_helper_34e0
### {StateMachineComponent controller member}  table=0x1401644d0  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140038630  OWN (no Ghidra function)
  [ 1] +0x08 0x140038660  OWN (no Ghidra function)
  [ 2] +0x10 0x1400386c0  OWN (no Ghidra function)
  [ 3] +0x18 0x140038710  OWN (no Ghidra function)
  [ 4] +0x20 0x140038740  OWN (no Ghidra function)
  [ 5] +0x28 0x140038770  OWN (no Ghidra function)
  [ 6] +0x30 0x1400387a0  OWN (no Ghidra function)
  [ 7] +0x38 0x1400387d0  OWN (no Ghidra function)
  [ 8] +0x40 0x140038800  OWN (no Ghidra function)
  stamped by (1): this+0x28 at 0x14012ba7f in 0x14012ba30 StateMachineComponent_Ctor
### ({StorageLocationComponent controller member})  table=0x140164830  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140039200  OWN StorageLocationComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x140039230  OWN StorageLocationComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x140039290  OWN StorageLocationComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x1400392e0  OWN StorageLocationComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x140039310  OWN StorageLocationComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x140039340  OWN StorageLocationComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140039370  OWN StorageLocationComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x1400393a0  OWN StorageLocationComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x1400393d0  OWN StorageLocationComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x14012d3b9 in 0x14012d360 StorageLocationComponent_Ctor
### ({TradeComponent controller member})  table=0x14015ac80  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x14000dfe0  OWN TradeComponent_CtrlVtable_0 (44 B)
  [ 1] +0x08 0x14000e010  OWN TradeComponent_CtrlVtable_1 (81 B)
  [ 2] +0x10 0x14000e070  OWN TradeComponent_CtrlVtable_2 (76 B)
  [ 3] +0x18 0x14000e0c0  OWN TradeComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x14000e0f0  OWN TradeComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x14000e120  OWN TradeComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x14000e150  OWN TradeComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x14000e180  OWN TradeComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x14000e1b0  OWN TradeComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x88 at 0x14005c4cb in 0x14005c430 TradeComponent_Ctor
### ({WellComponent controller member})  table=0x14015ae40  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x140010090  OWN WellComponent_Controller_ScalarDtor (44 B)
  [ 1] +0x08 0x1400100c0  OWN WellComponent_Controller_Slot1 (81 B)
  [ 2] +0x10 0x140010120  OWN WellComponent_Controller_Slot2 (76 B)
  [ 3] +0x18 0x140010170  OWN WellComponent_Controller_Shutdown (39 B)
  [ 4] +0x20 0x1400101a0  OWN WellComponent_Controller_Serialize (39 B)
  [ 5] +0x28 0x1400101d0  OWN WellComponent_Controller_Slot5 (46 B)
  [ 6] +0x30 0x140010200  OWN WellComponent_Controller_Draw (39 B)
  [ 7] +0x38 0x140010230  OWN WellComponent_Controller_OnCreate (39 B)
  [ 8] +0x40 0x140010260  OWN WellComponent_Controller_OnRemove (12 B)
  stamped by (1): this+0x28 at 0x14005d0b9 in 0x14005d060 WellComponent_Ctor
### ({WorkPlaceComponent controller member})  table=0x140164ce0  slots=9  sizeof=?  parent=- / structural {controller interface, 9 pure slots} 0x14015f070  stop=zero qword
  [ 0] +0x00 0x14003a410  OWN WorkPlaceComponent_CtrlVtable_0 (44 B)
  [ 1] +0x08 0x14003a440  OWN WorkPlaceComponent_CtrlVtable_1 (81 B)
  [ 2] +0x10 0x14003a4a0  OWN WorkPlaceComponent_CtrlVtable_2 (76 B)
  [ 3] +0x18 0x14003a4f0  OWN WorkPlaceComponent_CtrlVtable_3 (39 B)
  [ 4] +0x20 0x14003a520  OWN WorkPlaceComponent_CtrlVtable_4 (39 B)
  [ 5] +0x28 0x14003a550  OWN WorkPlaceComponent_CtrlVtable_5 (46 B)
  [ 6] +0x30 0x14003a580  OWN WorkPlaceComponent_CtrlVtable_6 (39 B)
  [ 7] +0x38 0x14003a5b0  OWN WorkPlaceComponent_CtrlVtable_7 (39 B)
  [ 8] +0x40 0x14003a5e0  OWN WorkPlaceComponent_CtrlVtable_8 (12 B)
  stamped by (1): this+0x28 at 0x14012f339 in 0x14012f2e0 WorkPlaceComponent_Ctor
```

## §7 — how this was measured

- Script: `b38_session/ghidra/Step8Appendix.java` run through `run_ghidra_script` on `Application-x64.exe` (it asserts the name
  and 11473 functions and aborts otherwise). There were two runs; the second one (LEA-only candidates, `returns this+N` decode)
  is the raw file.
- Post-processing (orientation, names, family table, index): `C:/Users/USPC/AppData/Local/Temp/claude/b38/0C/build_md.py` and
  `assemble.py` over the raw file. Tree cross-reference: `grep -rhoE "RegisterExeAddr\(\s*0x14[0-9a-fA-F]+" code standalone
  engine --include=*.cpp --exclude-dir='build*'` (1280 unique addresses) and `grep -rhoE "0x1401[5-6][0-9a-fA-F]{4}\b" code
  standalone engine --include=*.cpp --include=*.h --include=*.inc --exclude-dir='build*'` (719 unique).
- NOT measured: the tree's hand-built array per class (the block-0A/0B inventory); retail slot semantics beyond the tiny-body
  decode; the sizeof of the 13 abstract bases (nothing allocates them); whether the Runtime resolves the three quirky parent
  literals; the sizes of the five loop subclasses (see B37_SLOTS §E2 for the allocations read there).
