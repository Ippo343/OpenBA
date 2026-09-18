# ENDGAME B.3.3 — `Object::Base`, `TypeInfo`, `Factory`, reflection: rules for the step

Scope: ENDGAME.md §B.3 step 3 / §B.1 decisions 2 and 4 — `Object::Base`, the reflection
record, `Object::Factory`, the `DECLARE_TYPE`/`DEFINE_TYPE` macro, then `Object::External`
and `Object::Internal`. Design read: `OBJECT_INVENTORY.md` (this directory, corrected in
place by this step's spot-check). Everything in `B31_RULES.md` applies unchanged — the
two-step shim pattern, the class design rules, the hygiene section, the report format, the
closing-pass shape — and `B32_RULES.md`'s "the template WRAPS the ported core" rule applies
to every template this step introduces.

This is the one step in B with real design work and it has ONE serial owner until §9 step 5.

## 0. Out of scope, and what must not be "fixed" on the way through

- `ExternalPtr<T>`, `InternalBasePtr`, `InternalPtr<T>` are real classes since B.3.1. Do not
  re-open them.
- **`IO::Serializer` is B.3 step 4, after Object.** Every `Serialize` body a converted class
  takes ownership of keeps going through `RtObject::SerializerLite` and the **retail**
  `Encoding` vtable byte offsets. Those offsets DIFFER in the profile build
  (`Internal.h:209-218`): never cross-check a `Serialize` body against the profile
  disassembly, and never "modernise" a `SerializerLite` call into an `IO::Serializer` one.
- **The `ExternalFactory::Add` loader-thread AVL-rotate race is its own investigation**
  (STATUS.md, resave gate README). The conversion must not add a lock, a retry, an ordering
  change or a null check anywhere on that path, and must not silently remove it either: if a
  commit makes the race stop reproducing, that is a red flag to report, not a win. A real
  `ExternalFactory` class changes the spelling of the tree mutation, never its timing.
- Do not touch `deleting_dtor.cpp`'s table mechanism (see §6). Rows retire one at a time.

## 1. Question 1 — `Object::Base`'s slot list. ANSWERED: three slots, slot 1 is `TypeName()`

Measured in Ghidra against `Runtime-x64.dll` (image base 0x180000000) on 2026-09-18.
`??1Base@Object@@UEAA@XZ` @ **0x1800c0360** is `LEA RAX,[0x180154788]; MOV [RCX],RAX; RET`,
so Base's vtable is at **0x180154788**, and it has **exactly three slots**:

| slot | +off | retail address | body | name |
|---|---|---|---|---|
| 0 | +0x00 | 0x1800065b0 | vector deleting dtor, `MOV EDX,0x8` (elem size 8) → `??1Base` @ 0x1800c0360 | `??_GBase@Object@@UEAAPEAXI@Z` |
| 1 | +0x08 | 0x18007ec28 | `_purecall` | `virtual const char* TypeName() const = 0` |
| 2 | +0x10 | 0x1800c0380 | `XOR EAX,EAX; RET` | `?Type@Base@Object@@UEBAIXZ` — `virtual unsigned Type() const` |

Bounded at three because `??_7External@Object@@6B@` starts at **0x1801547a0**
(`??1External@Object@@` @ 0x1800bc950 stamps it and restores 0x180154788 on the way out),
i.e. 0x18 bytes later. Slot 1's identification is positive, not by elimination: External's
slot 1 (0x1800066c0) is `MOV RAX,qword[0x1801f9b40]; RET` and its slot 2 (0x1800066d0) is
`MOV EAX,dword[0x1801f9b38]; RET`, which match the return types of the two exported names
`?TypeName@External@Object@@UEBAPEBDXZ` (PEBD, a pointer) and `?Type@External@Object@@UEBAIXZ`
(I, a dword). The same two-body shape appears per class throughout the DLL
(`?Type@Achievement@1@UEBAIXZ` @ 0x180001020, `?TypeName@Achievement@1@UEBAPEBDXZ` @ 0x180001040).

**Rules that follow.**

1. `Object::Base` is declared with these three virtuals, in this order, and nothing else
   virtual. No `Slot1()` placeholder is needed and none may be introduced.
2. **The slot-2 name is `Type()`, not `GetTypeId()`.** ENDGAME §B.1 decision 2's and decision
   4's sketches say `GetTypeId`; decision 5 ("names are the original ones") wins. The static
   companion is `TypeId()` — also a retail name (`?TypeId@Achievement@1@SAIXZ`, `SA` = public
   static) — so `X::TypeId()` in decision 4 needs no invention either. Anything in the tree
   that today spells the virtual `GetTypeId` becomes `Type`, in the commit that converts it.
3. Slot 0 is `virtual Base* DeletingDtor(unsigned flags)`; `~Base()` is **non-virtual**.
   Deletion goes through `inline void Object::Destroy(Base* p)`, never `delete p`.
4. `External`'s own eight slots (0x1801547a0) are the reference for step 6, recorded here so
   they are not re-derived: `[0]` vdtor (elem 0x20, `??1External` @ 0x1800bc950), `[1]`
   `TypeName`, `[2]` `Type`, `[3]` `virtual int GetFileVersion() const` (pure in External;
   `Achievement::Goal`'s override at 0x18009ee10 is `MOV EAX,0x10000; RET`), `[4]`
   0x1800066e0 `MOV EAX,1; RET` — **`virtual CompileResult Compile(const System::Filename&)`**
   (`?Compile@External@Object@@UEAA?AW4CompileResult@12@AEBVFilename@System@@@Z`; Ghidra names
   the function, and `external.cpp` registers it by name and address. An earlier revision of
   this sheet called it nameless — corrected in step 5; there is no `Slot4()`),
   `[5]` `Serialize(IO::Stream&) -> bool` (pure; Goal's is 0x18009ee20), `[6]`/`[7]` pure in
   External — `Reconstruct` / `Destruct`, named by position off `ExternalList`'s table.

## 2. Question 2 — one reflection record. ANSWERED: `Object::BaseTypeInfo`, no staging list

There is no `Object::TypeInfo` in retail. The record's retail name is **`Object::BaseTypeInfo`**
(`??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z` @ 0x180006320,
`?StripNamespace@BaseTypeInfo@Object@@QEAAXXZ`, `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA`).
So ENDGAME decision 4's `Object::TypeInfo` is renamed to `Object::BaseTypeInfo` and the two
existing descriptions collapse INTO it:

- `RtObject::BaseTypeInfo` (`runtime/types/object/TypeInfo.h:77-129`) — the 0x38-byte layout,
  pinned field by field. Becomes the real class, under the retail name, with the seven offset
  asserts and the `sizeof == 0x38` assert kept verbatim.
- `Object::FactoryRegistration::BaseTypeInfoStorage` (`code/Game/Object/factory_registration.h:41-43`,
  `uint8_t bytes[0x38]`) — **deleted**, not asserted against. It exists only as a staging
  buffer, and the reason it exists is gone.

**Why the exe-side staging list folds away.** `factory_registration.h:1-18` says the pending
list exists because the registrations must build "at CRT-init time … (no Runtime-DLL calls,
since the DLL isn't resolved yet)", and `InvokeAllRegistrations()`
(`factory_registration.cpp:25-50`) then calls `fn_BaseTypeInfo_Ctor` for each entry after
`EngineFuncsInit`. That ctor is now OUR code: `RtObject_BaseTypeInfo_Ctor`
(`runtime/src/object/type_system.cpp:445`, registered at 0x180006320 on line 858). A static
`Object::BaseTypeInfo` with a real constructor can therefore run at CRT-init directly, and
head-insert into `_firstTypeInfo` — which is a zero-initialised pointer, so static-init order
across translation units cannot break it (the only hazard the staging list ever guarded).

Rules:

1. `DEFINE_TYPE(Class, Parent, Ext)` defines `Object::BaseTypeInfo Class::_typeInfo(...)` and
   nothing else. `FactoryRegistration`, `BaseTypeInfoStorage`, `InvokeAllRegistrations`,
   `g_pending` and the call to `InvokeAllRegistrations` in boot die in the commit that empties
   the list. **They did NOT die in step 3 and could not** — `DEFINE_TYPE` defines a MEMBER of
   a real class and all 305 `REGISTER_TYPE` sites still name PODs with hand-built vtables, so
   `REGISTER_TYPE` cannot be a wrapper over it until those types are converted (step 6/8).
   The staging path stays working and now calls a shim that placement-constructs the real
   class; see §9 step 3.
2. `_firstTypeInfo` becomes a plain `static BaseTypeInfo* BaseTypeInfo::_firstTypeInfo`.
   Today `type_system.cpp:74-80` reaches it through `ResolveExport(...)` into a MINTED data
   cell; that mint row and the `RT_DATA_SLOT` door for it go in the same commit
   (regenerate the `.inc`, never hand-edit).
3. **The macro mirrors retail's own macro**, member for member, because retail's is legible in
   the export table as one contiguous block per class (`Achievement` @ 0x180001000..0x1800011f0):
   `private: static const char* _TypeName(); static const char* _ParentTypeName();`
   (`CAPEBDXZ`) · `public: virtual unsigned Type() const;` (`UEBAIXZ`) ·
   `public: static unsigned TypeId();` (`SAIXZ`) · `public: virtual const char* TypeName() const;`
   (`UEBAPEBDXZ`) · per-class `operator new`/`operator new(size_t,void*)`/`operator delete` ·
   `public: static Base* CreateNewObject();` (`SAPEAVBase@Object@@XZ`) ·
   `public: static Base* CreateInPlaceObject(void*);` (`SAPEAVBase@Object@@PEAX@Z`) ·
   `private: static Object::BaseTypeInfo _typeInfo;` (`0VBaseTypeInfo@Object@@A`).
   Do not invent a member retail does not have, and do not drop one because no caller needs
   it yet — `CreateNewObject`/`CreateInPlaceObject` are the two function pointers
   `BaseTypeInfo` holds at +0x20/+0x28.
4. **`BaseTypeInfo::name` is content-only.** Compare by content, never by pointer. The ctor
   stores `name` verbatim and `parentName` with everything up to the last `':'` stripped
   (`StripNamespace`), so a stripped parent CRC matches an unstripped child `name` — that
   asymmetry is load-bearing for `Factory::Startup`'s second pass and must survive.
5. `typeId` is `0xFFFFFFFF` until `Factory::Startup`. `X::TypeId()` must return that sentinel
   before Startup, exactly as the pre-existing ordering traps in `game_main.cpp` expect. Do
   not make `TypeId()` self-initialising.

## 3. Question 3 — `Factory::root`. ANSWERED: yes, still raw `TypeNode*`, and it is B.3.3 scope

`runtime/src/object/type_system.cpp` walks the tree by hand today:
`FindNode` (:144-145, `factory->root` then a manual descend), `LeftMost` (:606),
`Successor` (:616), and `FactoryStartupRun`'s pass-2 parent descend (:712-717). The insert is
a by-address call — `ResolveAddr(Module::Runtime, 0x180030140)` at :661-666 — into
`rtavl::TypeSetInsert` (`runtime/src/collection/avl_trees.cpp:708`, registered at :1176),
which is ported but is NOT `Collection::Set<...>::Insert`. `type_system.cpp:644-660` states
the reason and hands the decision to this step by name: *"Whoever owns `Object::Factory`'s
type header should read this paragraph first."*

Rules:

1. Moving `Factory` onto `Collection::Set` **is B.3.3 scope**, and it lands with
   `Object::BaseTypeInfo` and the macro (§9 step 3), before `Object::Base` — nothing can
   register through a half-converted tree. DONE; the one departure is the allocator policy:
   `Object::TypeTreeAllocator`, not `Collection::DefaultAllocator`, because
   `DefaultAllocator` binds to `set_core.cpp`'s doors and a `Factory` built on it could not
   link into `make unit`. The policy does not affect the node layout.
2. `Object::Factory` is **0x10 bytes, not 8**. `TypeInfo.h:231-234`'s `struct Factory {
   TypeNode* root; }` is a correct VIEW and unusable as STORAGE:
   `??0Factory@Object@@QEAA@XZ` @ 0x1800c09d0 writes `[RCX]=0` and a dword `[RCX+8]=0`, and
   the insert core INCs that dword (0x1800303ac / 0x18003043c) — it is the Set's element
   count. `type_system.cpp:587-593`'s `FactorySet` already has the right shape; the real class
   takes it over and the probe-only view struct is deleted. Keep
   `static_assert(sizeof(Factory) == 0x10)`.
3. **Two node layouts, one `Set`.** The Factory tree's node is 0x50 bytes with
   `{key +0x00, value +0x08, parent +0x30, left +0x38, right +0x40, balance:i16 +0x48,
   height:u16 +0x4a}`; the other family in `avl_trees.cpp` (GROUP B and lane VID2) is 0x30
   with `parent +0x10, left +0x18, right +0x20`. `Collection::Set` selects its ported core by
   element width and already covers this — but **the 0x48 field is TWO 16-bit fields, not an
   int32** (0x180030540 `CMP word [RDX+0x48],-1` and `MOVZX word [RAX+0x4a]`), and
   `TypeInfo.h:209`'s `int32_t balance` is the old spelling kept only so existing readers
   compile. A find written against the wrong layout returns the right answer for a key that
   happens to be the ROOT and is permanently wrong for everything else
   (`reference_one_bst_find_two_node_layouts.md`) — so the conversion is not "done" because a
   lookup worked once. The unit test drives at least three keys at two depths.
4. The retail `Factory` surface, from the export table, is what the class declares:
   `Get()` (static singleton, `?Get@Factory@Object@@SAPEAV12@XZ`), `Startup`, `Shutdown`,
   `CreateObject(uint)`, `CreateObject(uint, void*)`, `GetTypeIdFromName(const char*, bool)`,
   `GetTypeName(uint)`, `GetTypeIdForFileExtension(const char*)`, `IsDerivedFrom(uint,uint)`,
   `IsTypeValid(uint)`, `GetParentType(uint)`, `GetChildType(uint,int)`,
   `GetChildTypeCount(uint)`, `LoadCompilers`, `UnloadCompilers`, private
   `GetTypeFromNameCRC(const char*)`, private static `_singleton`.
   `Object::GetTypeIdByName` is OURS, not retail's; it stays as a free helper over
   `Factory::Get()->GetTypeIdFromName(...)` until step 6 replaces its call sites with
   `X::TypeId()`, then it is deleted.
5. `Factory::Startup`'s faithful null-parent LEA (`type_system.cpp:719-734`) stays faithful:
   an unregistered parent name still yields `(TypeRecord*)8` and still faults on the next
   read. The printf that names the type before the fault stays; it changes no input. **Do not
   turn it into a guard** — see the CLAUDE.md hard rule.

## 4. Question 4 — the coexistence proof. DONE: `tests/unit/object_vtable_abi_test.cpp`

Seven tests, all passing. What they establish, as rules:

1. **A GCC class declared per decision 2 has a vtable that is slot-for-slot a `void*[]` at
   retail indices, in both directions.** A C body reading `(*(void***)obj)[N](obj, …)` off a
   real class reaches the Nth declared virtual with `this` unadjusted; and a real class's
   ordinary virtual call on an object whose vptr was stamped with a hand-built array of free
   functions reaches those free functions. There is no RTTI/offset-to-top entry at or after
   the address point, and no hidden prologue slot. **Per-class migration is therefore sound
   and needs no ABI workaround.**
2. **No virtual destructor, and the test proves the cost of breaking that.** The control class
   in the same file has a `virtual ~T()` and its first user-declared virtual lands at index
   **2**, not 1 — Itanium spends two slots (complete + deleting) exactly as ENDGAME decision 2
   predicted. Any `virtual ~T()` added to an Object-derived class before the "normal virtual
   cutover" silently shifts every slot after it, and every unconverted caller of that class
   then calls the wrong function.
3. **A copied compiler vtable is interchangeable with the compiler's own**, including patching
   individual slots — which is what `tools/rt_mint_gen.py` produced and what
   `code/Game/Game/tornado.cpp:622-640` still does at runtime. So a converted class may be
   handed to unconverted vtable-minting code and vice versa.
4. **Two non-virtual polymorphic bases put the second vptr at +0x28** for the
   `Component`(0x28) + controller shape, the compiler emits the this-adjusting thunk retail
   hand-wrote, and a raw `node = self + 0x28; (*(void***)node)[0](node, …)` call lands on the
   derived override with `this` adjusted back to the complete object. Adding the second base
   costs exactly one pointer.
5. **But +0x28 is the common case, not the rule** — see §5.
6. **`static_assert(offsetof(...))` on a polymorphic type is only conditionally supported** and
   GCC emits `-Winvalid-offsetof`; the tree builds `-Wall -Wextra` with no suppression
   (Makefile:59, :120) and has ~2,100 such asserts. Decision 1 says to keep them, so the
   commit that makes the FIRST type header polymorphic adds `-Wno-invalid-offsetof` to
   `SA_CXXFLAGS` and `UNIT_CXXFLAGS` — one flag, in one commit, called out in the commit
   message — rather than letting the warning count creep or deleting the pins. The test file
   measures offsets from two live addresses instead, so it stays warning-free.

## 5. Question 5 — every site that reads a vptr for identity

Swept across `runtime/`, `code/Game/`, `standalone/`, `dx11/`, `engine/`. **Four live sites and
one dead helper.** Each needs its decision landed BEFORE the class it reads is flipped.

| site | what it compares | decision, and when |
|---|---|---|
| `code/Game/Game/tornado.cpp:627` `if (vt == s_tornadoVtable) return;` | re-entry guard: is this object already wearing the patched copy? Both sides are LOCALLY built arrays (`s_tornadoVtable` is a copy of the live `Graphics::ParticleSystem` table with 8 slots patched, :622-640). | Survives `Object::Base` flipping unchanged — it never names a compiler-generated table. It dies with `Graphics::ParticleSystem`'s own conversion (B.3 step 5), not here. Leave it; do not "tidy" it into a type-id check while the patched copy still exists. |
| `standalone/pausemenu_harness.cpp:237, :250` `*(void**)s_dialog == s_vtable` | liveness: "was the dialog destroyed under us?" Both sides come from the SAME live object at two times. | Survives: a converted `UI::Dialog`'s destructor still re-stamps the base vptr, so the value still changes on destruction. When `UI::Dialog` converts, restate it as `s_dialog->Type() == s_dialogTypeId` — a harness, so it may lag one step. |
| `standalone/stress_harness.cpp:568-575` | uses the vptr as a HASH KEY into a profiler slot table, then reads `vt[2]` as `GetTypeId`. | The key use survives (any stable pointer works). **The hard-coded `vt[2]` becomes `comp->Type()` in the same commit that gives `Component` a real class**, NOT with Base's (re-confirmed at step 4): the object is a `Component` POD, so `Base*` would assert a relationship that does not exist, and since `p->Type()` emits the same `(*(void***)p)[2](p)` there is no behaviour to gain by moving early — only an over-stated claim. |
| `runtime/src/io/file_stream.cpp:393` `if (vptr == ours)` in `NoteSlotDispatch` | attendance counter: did this dispatch arrive through OUR minted `BinaryFileStream` table? | Becomes meaningless the moment `IO::BinaryFileStream` is a real class — that is B.3 step 4, not B.3.3. Delete the counter with that class; do not leave it silently counting zero. |
| `runtime/src/audio/audio_stream_win.cpp:303` `ThreadBaseVtable()` | **nothing.** The comparison the inventory describes no longer exists: the helper is defined and referenced only by a comment at :389. | Zero callers → delete it in this step's closing pass (§9 step 7), by name and by address (0x180155f20). |

`runtime/src/achievement/goal.cpp:115-127, :194` is NOT an identity compare: `BaseVtable()` /
`ExternalVtable()` / `GoalVtable()` are STAMPS, reproducing retail's ctor chain writing each
base's vptr and overwriting it. A real constructor chain reproduces that for free, and the
three helpers and their `ResolveExport("??_7…")` rows die with `Achievement::Goal`'s
conversion. The rule the pattern imposes: **no new code may read a vptr mid-construction.**

Standing rule for the rest of B: **a vptr is never an identity.** Any new "which class is
this?" question is answered by `p->Type()` compared with `X::TypeId()`, or by
`Factory::Get()->IsDerivedFrom(p->Type(), X::TypeId())`. A reviewer who sees a vptr on either
side of `==` in a commit after this one rejects it.

## 6. Question 6 — `deleting_dtor.cpp`

`runtime/src/object/deleting_dtor.cpp` is 135 `RT_DELDTOR(addr, dtorVa, elemSize, name)` rows,
matching `deleting_dtor_internal.h:66`'s `kSpecCount = 135` (the file's own static_assert
guards the count). `Object::Base`'s own row is
`RT_DELDTOR(0x1800065b0, 0x1800c0360, 0x008, "Object::Base")` at `deleting_dtor.cpp:295`.

Rules:

1. **A class's row dies in the same commit that gives that class a real `DeletingDtor` AND
   nothing reads its minted vtable any more.** The second clause was learned at step 4 and is
   the one that binds: a `RT_DELDTOR` row is not only `DeletingDtorCore`'s dispatch table, it
   is also what FILLS slot 0 of the minted vtable. `standalone/rt_data_mint.cpp`'s `OurPort()`
   resolves an `RT_MINT_UNNAMED` slot with `RtPortLookupAddr(dllVa)`, so deleting the row's
   `RegisterRuntimeAddr` turns that slot into a named abort trap — a build-clean, link-clean
   EXEC-of-a-trap at the first virtual call. **Before retiring a row, check
   `rt_data_mint_tables.inc` for a `{ nullptr, <the row's addr>, RT_MINT_UNNAMED }` slot as
   well as the by-name and by-address callers.** One row per commit is normal; a commit that
   converts a class and leaves its row is incomplete, and a commit that removes a row a
   minted vtable still needs is worse.
2. **Row indices are table POSITIONS, not identities.** The header's own MERGE HAZARD banner
   (`deleting_dtor_internal.h:86-96`) records three lanes once appending at the same index.
   Re-count on every merge, and never cite a row by index in a commit message or a comment —
   cite it by retail address and name.
3. **There is no verifier script** — `tools/rt_deleting_dtor_verify.py` does not exist; what
   exists is `tools/gen_deleting_dtor.py`, a one-shot GENERATOR whose output was committed as
   ordinary source, and whose `ROWS` table is now stale relative to the file. So the process
   is: edit the row, update `kSpecCount` in `deleting_dtor_internal.h` in the SAME edit
   (`deleting_dtor.cpp:1187`'s `static_assert(sizeof(kSpecs)/sizeof(kSpecs[0]) == kSpecCount)`
   is the backstop that catches a mismatched pair at build time, not a substitute for
   counting), and re-run `tools/runtime_census.py` — every registration must stay a LITERAL
   address or the census goes blind to it
   (`reference_census_cannot_see_nonliteral_registrations.md`; 12 live ports were once
   invisible for exactly this reason). If this step wants a verifier, writing one is cheap and
   belongs in the commit that retires Base's row.
4. **ANSWERED at the step-3 open — neither file holds a row, and there is exactly one
   table.** `grep -c "RT_DELDTOR("` is 135 in `deleting_dtor.cpp` and **0** in both
   `factory_dtors.cpp` and `deleting_dtor_leaves.cpp`. What they actually are:
   - `factory_dtors.cpp` (544 lines) is not a dtor-table file at all. It is the three
     `??1Factory@Object@@` / `??1InternalFactory@Object@@` / `??1ExternalFactory@Object@@`
     **class** destructors, all `QEAA` (non-virtual), registered by name and address. **Zero**
     of its addresses is a table row address or a table `dtorVa`. Disjoint mechanism; nothing
     in it moves when a row retires. It is, however, where `~Factory`'s real teardown lives
     (0x180008110 walks the whole TypeNode tree and destroys each `children` Array), so it is
     the file a `Object::Factory::Shutdown` port belongs in — not this step.
   - `deleting_dtor_leaves.cpp` (204 lines) is the table's **dependency**, not a second table.
     A `RT_DELDTOR(addr, dtorVa, size, name)` row carries the `??_G` deleting-dtor address AND
     the scalar `??1` address separately, and `DeletingDtorCore` resolves the latter BY
     ADDRESS; this file supplies **11 of those 135 `dtorVa` targets**, the six that used to
     resolve NULL plus five more (0x180058d20, 0x18009e400, 0x1800b9150, 0x1800b9a40,
     **0x1800c0360**, 0x1800dac70, 0x1800e0880, 0x1800fb620, 0x180103a00, 0x1801041f0, …).
   - **The one that matters for step 4:** `??1Base@Object@@` at **0x1800c0360** is one of those
     eleven (`deleting_dtor_leaves.cpp:161-186`, `RtObject_Base_Dtor`, a bare vtable restamp).
     So the commit that retires Base's row must ALSO decide that body's fate — the real
     `~Base()` replaces it — and must delete its two registrations in the same commit, or the
     table's row is gone while a now-unreachable `??1Base` body stays registered by address.
5. Do not redesign the mechanism. Its shape — address in, `(self, flags)` dispatch — is
   already exactly what decision 2's slot 0 is, so it is reference data that shrinks, not a
   second design to reconcile.

## 7. The settled B design points, restated because this step is where they bite

- **The real class takes the RETAIL name** in the retail namespace (`Object::Base`,
  `Object::BaseTypeInfo`, `Object::Factory`, `Object::External`, `Object::Internal`), with
  retail method names (§1.2, §2.3, §3.4). `RtObject::*` PODs are not promoted in place.
- **The old PODs stay trivially destructible** behind one-line shims until their last caller
  converts. `RtSystem::String`'s double-free lesson (B31_RULES) applies verbatim to
  `RtObject::External`, whose ported bodies hold `RtObject::External tmp;` and call
  `RtObject_External_Dtor(&tmp)` by hand: give the POD a destructor and the compiler runs a
  second one.
- **Members of unconverted structs are bridged at the use site**, not by converting the
  container: `reinterpret_cast<Object::External&>(field)` with a `// B.3.3 bridge` comment on
  the line. The comment is mandatory — it is what lets the B.4 cast metric separate a
  transitional cast from a leftover, and the containing class's own conversion removes it.
- **Shims, `fn_*` pointers and registry rows die at zero callers by NAME *and* by retail
  ADDRESS.** "By address" includes the per-file wrappers (`RtAddr<Fn>`, `RtAddrFn`, `ByAddr`,
  named `constexpr` address constants, `RT_DATA_SLOT`), not just `ResolveAddr`. A hit in
  `docs/`, a `// 0x18…` comment or a printf/static_assert string is **not** a caller. A dead
  row that was live is an EXEC-of-0 at runtime, never a link error — the routes are the proof.
- **`.inc` files are regenerated, never hand-edited** (`standalone/rt_export_addrs.inc`,
  `rt_data_mint*.inc`, `runtime/src/object/rt_factories_object.inc`).
- **Templates wrap the ported cores** (B32_RULES): `Collection::Set` over the Factory tree
  calls `rtavl`'s ported insert/rotate bodies, it does not re-derive them.
- No new `BANISHED_*` flag, no `getenv`, no wave/lane/instrument. A port registers
  unconditionally.
- Greps over `standalone/` or `tests/` exclude `standalone/build*/` and `tests/unit/build/`,
  anchored on a path separator — `\build` also swallows `buildinggoal.cpp`.

## 8. Unit tests this step owns

`tests/unit/` registration is automatic (`$(wildcard tests/unit/*.cpp)`); tree sources link
through `tests/unit/sources.mk` with a `UNIT_TREE_SRCS +=` block per core, so parallel batches
merge cleanly.

- `object_vtable_abi_test.cpp` — done (§4). Keep it: it is the regression test for the
  no-virtual-destructor rule.
- `Object::BaseTypeInfo` + the macro: the CRC (`TypeNameCRC`, reflected CRC-32 poly
  0xEDB88320 over the UTF-16LE-widened name) against known ids; `StripNamespace` keeping
  everything after the LAST `':'`; `typeId == 0xFFFFFFFF` before Startup; `_firstTypeInfo`
  head-insert order.
- `Object::Factory`: Startup over a synthetic chain; find at the root AND at two levels down
  (§3.3); duplicate key returns the existing node; parent/children linkage; `IsDerivedFrom`
  across three generations; `GetTypeIdForFileExtension`'s `_stricmp`.
- `Object::Base`: `Destroy(p)` reaching slot 0 with flags 1; `Type()`/`TypeName()` through a
  three-deep chain.

## 9. Order of work

1. ~~Inventory spot-check + these six answers~~ — done, this document; `OBJECT_INVENTORY.md`
   corrected in place.
2. ~~The coexistence unit test~~ — done, `tests/unit/object_vtable_abi_test.cpp`.
3. ~~**`Object::BaseTypeInfo` + `DECLARE_TYPE`/`DEFINE_TYPE` + `Object::Factory` on
   `Collection::Set`.**~~ — done. What landed, and the two places it departs from the plan
   above, both deliberately:
   - `Object::BaseTypeInfo` and `Object::Factory` are real classes in
     `runtime/types/object/TypeInfo.h`; the bodies are `runtime/src/object/type_reflection.cpp`,
     which is registry-free so `make unit` can link it. `RtObject::BaseTypeInfo`,
     `RtObject::TypeRecord` and `RtObject::TypeNode` stay, cross-asserted field for field
     against the classes. `type_system.cpp` keeps every ABI shim and every door and now
     delegates.
   - `Factory` is `Collection::Set<TypeEntry, AVLCompare, TypeTreeAllocator, uint32_t>` with
     `TypeEntry` 0x30 bytes, which is what selects the ported `AvlOps<Node,0x30>` core; the
     node is asserted at 0x50 with `balance`/`height` as two 16-bit fields. The by-address
     call to 0x180030140 is gone from Startup — the template reaches the same
     `PortedRetrace48`. `rtavl::TypeSetInsert` keeps its registration; it now has no caller in
     `runtime/src/object/`.
   - **`_firstTypeInfo` is a plain static and its mint row was NOT deleted — it was
     REDIRECTED.** `standalone/rt_data_mint.cpp`'s `MintStorageCell()` routes that one row's
     storage to `Object::BaseTypeInfo::_firstTypeInfo` at `MintRowAt`, the single point both
     `RegisterMintedData()` and `RtMintRegisterRow()` (and therefore `cutover_manifest`'s
     REQUIRED publish) funnel through, so the name door, the address door 0x1801fc6e0 and the
     class all answer with the same eight bytes. Deleting the generated row instead would mean
     re-running `tools/rt_mint_gen.py` against the DLL, which no step-3 build can verify;
     the row's 8 bytes are now inert and the closing pass (step 7) drops it **with the
     generator**. `MintStorage()` SPLICES the 14 minted nodes in front of whatever CRT-init
     left in the chain rather than assigning the head — assigning would silently drop
     `DEFINE_TYPE` statics the day one exists.
   - **`_singleton@Factory@Object@@` was deliberately NOT moved.** Unlike `_firstTypeInfo` it
     is only ever read at runtime, never from a static initialiser, so the port door answers
     it; `Factory::Get()` uses `RT_DATA_SLOT`. Moving a second cell would have doubled this
     step's unverifiable-without-a-boot surface for no gain. It moves when `Factory::Shutdown`
     is ported (§6.4).
   - **The exe-side staging list STAYED.** `REGISTER_TYPE` could not become a one-line
     `DEFINE_TYPE` wrapper: `DEFINE_TYPE` defines `Class::_typeInfo`, a member of a real C++
     class, and all 305 `REGISTER_TYPE` sites name types that are still PODs with hand-built
     vtables. Folding the list needs those types converted, which is step 6/8. So
     `FactoryRegistration`, `BaseTypeInfoStorage` and `InvokeAllRegistrations` are untouched
     and still work — they now call a shim that placement-constructs the real class. The fold
     happens when the last `REGISTER_TYPE` site becomes a `DEFINE_TYPE` one; §2.1 stands as
     the target, not as step 3's deliverable.
   - `DECLARE_TYPE` emits `CreateNewObject`, `CreateInPlaceObject`, `TypeId()`, `GetTypeInfo()`,
     `_TypeName`, `_ParentTypeName` and `_typeInfo`. It does **not** emit the two VIRTUALS
     (`Type()` slot 2, `TypeName()` slot 1) — there is no base to override until step 4 — nor
     the three `operator new`/`delete` overloads, which are the allocator's own step. Step 4
     adds them here.
   - `-Wno-invalid-offsetof` was **not** needed: neither class is polymorphic, so every
     `static_assert(offsetof(...))` still compiles clean. §4.6 stands for step 4, which is the
     first polymorphic type header.
4. ~~**`Object::Base` as a real class**; `Object::Destroy(p)`.~~ — done. What landed:
   - `Object::Base` in `runtime/types/object/TypeInfo.h`: the three measured slots in retail
     order (`DeletingDtor(unsigned)`, pure `TypeName() const`, `Type() const`), a protected
     NON-virtual `~Base()`, `IsA`/`IsDerivedFrom` as members, and `inline Object::Destroy(p)`.
     `RtObject::Base` stays, size-asserted against it. Bodies in `type_reflection.cpp`.
   - `DECLARE_TYPE` now emits the two virtuals over the class's own `_typeInfo`, and
     `DEFINE_TYPE` initialises the record THROUGH `_TypeName()`/`_ParentTypeName()` so each
     literal has one spelling and the accessors cannot drift from what was registered.
     `operator new`/`delete` are still not emitted — see the macro's banner.
   - The three `RtObject_Base_*` shims now delegate to the class. `AsBase(self)` is safe on an
     object carrying a MINTED vtable — `p->Type()` compiles to `(*(void***)p)[2](p)`, the same
     instruction the hand-written cast was — which is what makes the class usable before any
     subclass converts.
   - **BASE'S `RT_DELDTOR` ROW, `RtObject_Base_Dtor` AND THE MINTED VTABLE ALL STAY.** The
     sweep by name and by address found the readers are very much alive:
     `standalone/cutover_manifest.inc:49` carries `??_7Base@Object@@6B@` as
     `CUTOVER_REQUIRED` with **64 by-name doors**; 22 `RtFactOp::Vptr` rows in the
     `rt_factories_*.inc` replay tables stamp it; 59 non-comment `.cpp` sites read it
     (`goal.cpp`, `achievement_manager.cpp`, `internal.cpp`, `external.cpp`,
     `description_common.h`'s `VtBase()`, the UI description family, the Graphics/Video/Audio
     vtable files…), 134 occurrences over 83 files in all. And
     `rt_data_mint_tables.inc`'s `kVSlots12` — the generated row for
     `??_7Base@Object@@6B@`, which independently agrees it has **3 slots** — fills slot 0 from
     `RtPortLookupAddr(0x1800065b0)`, i.e. straight out of the row. Retiring any of the three
     now would publish a trap table into every live object. **The retirement condition is that
     the last of those readers converts**, which is the END of the fan-out (step 6/7), not
     step 4. A faithful partial beats a complete-looking deletion.
   - `-Wno-invalid-offsetof` was **not** needed and was not added: `Object::Base` is
     polymorphic but has no data members, so no `static_assert(offsetof(...))` is taken on it
     or on anything deriving from it yet. §4.6 now applies to the first CONVERTED class with
     FIELDS — `Object::External` or `Object::Internal`, step 5.
   - `standalone/stress_harness.cpp:568-575`'s `vt[2]`: **left as it is**, with the reason in
     §5. It reads a `Component`, which is still a POD with a hand-built vtable; spelling it
     `Base*` would assert a type relationship that does not exist yet, would have to be
     re-examined when `Component` converts anyway, and changes no instruction — there is no
     correctness to bank, only a claim to over-state.
5. Fan out (opus, worktrees, disjoint file sets, the B31 hygiene block). The two sets are
   disjoint by construction; neither touches `type_system.cpp`, `deleting_dtor*.cpp`,
   `rt_factories_*` or `object_typeinfo_accessors.cpp`, which stay with the owner:
   - **EXTERNAL** — `runtime/src/object/external.cpp`, `external_add.cpp`,
     `external_compiled.{cpp,h}`, `external_find.{cpp,h}`, `external_forceunload.cpp`,
     `external_list.cpp`, `external_load.cpp`, `external_path.h`,
     `external_path_stack.{cpp,h}`, `external_path_win.cpp`, `external_ptr_serialize.cpp`,
     `external_reftable.cpp`, `external_savetext_win.cpp`, `external_set.{cpp,h}`,
     `reference_set.{cpp,h}`, `ext_compilers.cpp` + `runtime/types/object/External.h`
     (13 `.cpp`, 6,323 lines).
   - **INTERNAL** — `runtime/src/object/internal.cpp`, `internal_ptr_array_serialize.cpp`,
     `internalfactory_persist.cpp` + `runtime/types/object/Internal.h`
     (3 `.cpp`, 1,493 lines).
6. Caller batches by `code/Game/<Module>` directory: 628 `GetTypeIdByName` occurrences
   (545 with a literal name) → `X::TypeId()`; 195 `DeletePtr` + 124 `CallSlot0` → `Destroy`;
   the 106 `fn_(Object|Base|External|Internal|Factory)*` typedefs. Sonnet is enough for the
   `GetTypeIdByName` sweep once the first batch sets the pattern; opus for the delete paths.
7. One closing agent: pointers, shims and rows at zero callers by name AND address (including
   `ThreadBaseVtable`, §5); regenerate the `.inc` files; STATUS and the B.4 cast table.

## 10. The gate for every B.3.3 merge (coordinator only)

build exit 0 (debug **and** perf — deletions near init printfs break only the perf exe) ·
`make unit` · `sa-run` + pause-menu quit · `BANISHED_AUTOTUTORIAL=1` **with**
`BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120` (it never self-quits otherwise) · the
Load Game dialog route · `tools/resave_gate/resave_gate.sh Gate` PASS (a ONE-byte DIFF at
+0x18 is the README's known flake: re-run twice before reading it) ·
`tools/dispatch_slot_map.py --check` 0 · `tools/runtime_census.py` whenever a `RT_DELDTOR` row
or a registration moved (§6.3) · `git diff --stat` against the agent's declared file set
before merging ·
`make sa-stress` at each step close — **for the functional signal only** (6000 sim frames, exit
0, empty crash log). Record the printed sim ms/iteration in STATUS.md and move on: perf deltas
are NOT chased while B is open (no re-runs, no A/B of older commits, no profiler, no merge
blocked on the PLAN 5% line). The shims and doors B adds are transitional; perf is revisited
once B is closed, from the numbers recorded at each close.
