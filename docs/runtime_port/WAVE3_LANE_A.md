# Wave 3, Lane A — Object reflection: Base / Type / BaseTypeInfo / Factory

Following [PLAN.md](PLAN.md) §4.2 and §6. Lane A owns the six Object reflection exports our
tree calls (`call_surface_20260906.csv`, `Object` rows), plus their layouts.

Files owned and written by this lane:

| File | What |
|---|---|
| `runtime/types/object/TypeInfo.h` | new — `Base`, `BaseTypeInfo`, `TypeRecord`, `TypeNode`, `Factory` |
| `runtime/src/object/type_system.cpp` | the six ports + `RtObjectTypeSystemRegister()` |
| `docs/runtime_port/WAVE3_LANE_A.md` | this file |

Two surgical edits outside that set, under the exception PLAN §6 step 8 requires — see
[§7](#7-comments-corrected-and-comments-still-wrong).

---

## 1. Result

| | |
|---|---|
| Ported | **6 of 6**, all six registered by name, by address, and (for two) by ordinal |
| Registration count | `RtObjectTypeSystemRegister()` returns **6** |
| Diff sites added | **6** — 3 full oracles, 2 partial (documented), 1 with no oracle at all |
| Census (whole Object namespace, all three lanes) | `Object 21/41 called = 51.2%`, TOTAL 170/877 |
| Measured in a run | **nothing yet** — the integration run is the orchestrator's; every number above is static |

**Nothing in this lane has executed.** The `sa-run` with `BANISHED_RT_DIFF=1` is central to the
wave. Read every claim below as "this is what the DLL does and this is what the port does", not
as "this was observed agreeing". WAVE2.md finding 5 is the standing warning: a registration is
not a port until it runs.

---

## 2. The six functions

All addresses read live in Ghidra 2026-09-06. Retail = `Runtime-x64.dll` (base `0x180000000`),
profile = `Runtime-x64-profile.dll` (the ToolKit build).

| Export | Retail | Profile | Shape |
|---|---|---|---|
| `?IsA@Base@Object@@QEBA_NI@Z` | `0x1800491c0` | `0x1800828f0` | exact-type test, 2 instructions |
| `?IsDerivedFrom@Base@Object@@QEBA_NI@Z` | `0x1800c0390` | `0x18008c670` | AVL find + parent-chain walk |
| `??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z` | `0x180006320` | `0x1800234e0` | 6-field store + head-insert + `:` strip |
| `?CreateObject@Factory@Object@@QEBAPEAVBase@2@I@Z` | `0x1800c0ae0` | `0x18008cde0` | AVL find + tail-jump `info+0x20` |
| `?GetTypeIdFromName@Factory@Object@@QEBAIPEBD_N@Z` | `0x1800c0580` | `0x18008cbe0` | CRC + AVL confirm |
| `?Serialize@Type@Object@@QEAAXAEAVSerializer@IO@@@Z` | `0x1800c0c40` | (not located) | tail-jump `ReaderWriter` vtable +0x98 |

### Export names — verified, none phantom

`reference_resolveexport_phantom_names.md` / WAVE2 finding 4: a `ResolveExport` name the DLL
does not contain returns NULL silently, the address fallback hides it, and the port then
registers under a name nothing resolves. All six were checked against the shipped DLL's export
directory with `pefile` **before** any code was written:

```
tools/ghidra_mcp/.venv/Scripts/python.exe   # pefile over C:\GOG Games\Banished\Runtime-x64.dll
OK  0x1800491c0 ord=3089  ?IsA@Base@Object@@QEBA_NI@Z
OK  0x1800c0390 ord=3110  ?IsDerivedFrom@Base@Object@@QEBA_NI@Z
OK  0x180006320 ord=52    ??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z
OK  0x1800c0ae0 ord=2128  ?CreateObject@Factory@Object@@QEBAPEAVBase@2@I@Z
OK  0x1800c0580 ord=2991  ?GetTypeIdFromName@Factory@Object@@QEBAIPEBD_N@Z
OK  0x1800c0c40 ord=3772  ?Serialize@Type@Object@@QEAAXAEAVSerializer@IO@@@Z
```

### Hidden return slots — none, and that was checked per name

None of the six returns a class by value: the manglings end `_N` (bool), `I` (unsigned),
`PEAVBase@2@` / `PEAV12@` (pointer), `X` (void). So none has the RDX hidden slot that made
`fn_Math_Transform3_Identity` a wild write (`reference_hidden_retslot_wild_write.md`).

One ABI detail that *is* live: **the `BaseTypeInfo` constructor returns `this` in RAX**
(`MOV RAX,R10; RET` at `0x18000639f`), as every MSVC constructor does, even though the ctor
mangling declares no return type. `engine/funcs/engine_funcs.h:3701` types it `void (*)(…)`,
which is harmless for a caller that ignores RAX — but the port returns `this` anyway.

---

## 3. The layouts, and the evidence for each

Full commentary is in `runtime/types/object/TypeInfo.h`; every offset there carries a
`static_assert` and the instruction that pins it. Summary:

### `Object::BaseTypeInfo` — 0x38 bytes

| Off | Field | Pinned by |
|---|---|---|
| 0x00 | `uint32 typeId` (`0xFFFFFFFF` until `Factory::Startup`) | ctor `MOV dword [RCX],0xffffffff`; Startup writes the CRC |
| 0x04 | unused word | never written by the ctor |
| 0x08 | `const char* name` | ctor arg 1; `Factory::Startup` CRCs it; `GetTypeName` returns it |
| 0x10 | `const char* parentName` **namespace-stripped** | ctor arg 2, rewritten past the last `':'`; Startup's parent lookup |
| 0x18 | `const char* fileExtension` | `_stricmp(info+0x18, ext)` in `Factory::GetTypeIdForFileExtension` (`0x1800c04c0`) |
| 0x20 | `Base* (*factory)()` | `CreateObject(id)` tail-jumps it |
| 0x28 | `Base* (*inPlace)(void*)` | `CreateObject(id, void*)` (`0x1800c0aa0`) tail-jumps it |
| 0x30 | `BaseTypeInfo* next` | ctor head-inserts into `_firstTypeInfo` |

**PLAN §2.2 hazard B is resolved: our three hand-declared copies had every OFFSET right.**
`code/Game/Game/game_main.cpp:491` and `standalone/main.cpp:62` both declare the correct
8-field, 0x38-byte struct. One field NAME was wrong in both — see §7.

Only the PARENT name is `:`-stripped, not the type's own name. That asymmetry is what lets
`Factory::Startup`'s second pass match a stripped parent string ("Base") against a child's
unstripped `name` — the registrations pass unqualified names for themselves and sometimes
qualified ones for their parent.

### `Object::Factory`'s AVL tree — `Set<Pair<uint32_t, TypeRecord>, …>`

WAVE2 finding 2 predicted this shape and it is exactly right: the node stores the extracted
**key separately from the value**. The profile build says so verbatim. `Factory::IsDerivedFrom`
(profile `0x18008ce60`) contains

```
System::Debug::Assert("typePtr->_type == type && it->key == type",
                      "Error, type and registered type are out of sync.",
                      "Object\\Factory.cpp", 0x8c)
```

`it->key` is node+0x00 and `typePtr->_type` is node+0x08, and the engine's own assert requires
them to agree. (Same file gives `Object/Factory.h:0x40` for the singleton assert, so the
original source is `code\Runtime\Object\Factory.{h,cpp}`.)

| Off | Field | Pinned by |
|---|---|---|
| 0x00 | `uint32 key` | `MOV ECX,[RAX]` + CMP in all four lookups |
| 0x08 | `TypeRecord value` | `ADD RAX,0x8` at `0x1800c03d5` before the base-chain walk |
| 0x08+0x00 | `uint32 typeId` (`_type`) | the chain compare `CMP dword [RAX],EBX` |
| 0x08+0x08 | `BaseTypeInfo* info` | `MOV RCX,[RAX+0x10]; JMP [RCX+0x20]` in `CreateObject` |
| 0x08+0x10 | `TypeRecord* parent` | `MOV RAX,[RAX+0x10]` in the chain walk; written by Startup pass 2 |
| 0x08+0x18 | `TypeRecord** children` (`Array::_data`) | Startup `puVar12+8` |
| 0x08+0x20 | `int32 childCount` (`_size`) | Startup `puVar12[10]` |
| 0x08+0x24 | `int32 childCapacity` (`_capacity`) | Startup `puVar12[0xb]`, grown `*3` after the first |
| 0x30 | `TypeNode* parent` | in-order successor walk in Startup and `GetTypeIdForFileExtension` |
| 0x38 | `TypeNode* left` | taken when `key > wanted` |
| 0x40 | `TypeNode* right` | taken when `key < wanted` |

The element that gets inserted is built on the stack in `Factory::Startup` and copied by the
Set insert helper `FUN_180030140` (retail), which copies `+0x00`, `+0x08`, `+0x10` and then
copy-constructs the 16-byte `Array` at `+0x18` — confirming the `TypeRecord` is 0x28 bytes and
`Collection::Array` is the `{_data, _size:i32, _capacity:i32}` triple from
WAVE2_COLLECTION.md.

**Explicitly a guess, and labelled as one in the header:** the AVL balance field at `+0x48` and
the node's total size. Nothing in this lane inserts, rebalances or frees a node, so neither is
relied on. Do not allocate a `TypeNode` from that declaration.

### The type id is a CRC-32 of the UTF-16 name

`Factory::GetTypeFromNameCRC` (`0x1800c0930`) widens the ASCII name to UTF-16LE on a 264-byte
stack buffer and CRC-32s the resulting *bytes*, so the id of `"Base"` is the CRC of
`"B\0a\0s\0e\0"`. `Factory::Startup` inlines the same loop.

The table is `System::CRC::_crcTable`, exported at `0x1801f9080`. **All 256 entries were
compared against the textbook reflected CRC-32 table (polynomial `0xEDB88320`) and are
identical**, so the port generates the table instead of reading the DLL's — PLAN §4.1
decision 1 wants ported code to outlive cutover.

```
pefile.get_data(0x1f9080, 1024) vs. locally folded table  ->  identical: True
```

The 264-byte buffer is retail's, and a type name longer than 131 characters overflows it in
retail exactly as it does in the port. No bound check was added; adding one changes behaviour.

---

## 4. What each diff site can and cannot prove

This is the part worth reading closely. Three of the six are genuine oracles; three are not,
and saying which is which is the whole point.

| Site | Kind | Proves | Does not prove |
|---|---|---|---|
| `Object::Base::IsA` | **full** `RT_DIFF_SCALAR` | everything | — |
| `Object::Base::IsDerivedFrom` | **full** `RT_DIFF_SCALAR` | everything | — |
| `Object::Factory::GetTypeIdFromName` | **full** `RT_DIFF_SCALAR` | everything, including the CRC | — |
| `Object::Factory::CreateObject` | **partial** | our AVL walk lands on the same node the DLL's does | the factory dispatch itself |
| `Object::BaseTypeInfo::BaseTypeInfo` | **real shadow** | all seven written fields, against the DLL's own ctor on the same inputs | ordering against other threads (there are none at static-init time) |
| `Object::Type::Serialize` | **none** | only that it executed, and that the ReaderWriter + vtable + slot are non-null | anything about the bytes written |

### `IsA` — full, with one caveat named

Diffing calls the `GetTypeId` virtual twice. That is free here because every implementation of
vtable slot 2 in this engine is `MOV EAX,[rip+typeIdGlobal]; RET`. If a subclass ever gives that
slot a side effect this site becomes unsound. None does today.

### `CreateObject` — a partial oracle, and why not a full one

Running the original as an oracle would construct a **second object** and leak it. So the site
does not. Instead it independently checks the only part the port reimplements — the tree walk —
by calling the DLL's own `Factory::GetTypeName(id)` (`0x1800c0a60`), which performs its own walk
and returns `FindNode(id)->value.info->name`, and comparing that pointer with the one our walk
produced. Different node ⇒ different name pointer ⇒ divergence. The factory call itself is a
single indirect jump through a pointer we did not compute, and is not covered.

### `BaseTypeInfo::BaseTypeInfo` — a real shadow, not a fake one

The brief allowed "state plainly that it is unverified". It does not have to be. The DLL's ctor
is 21 instructions and the **only** global it reads or writes is `_firstTypeInfo`
(`0x1801fc6e0`) — the whole body is in the disassembly, there is no hidden side effect. So the
site can run the original for real and put the list back:

1. snapshot `prevHead = *_firstTypeInfo`;
2. run **ours** for real (list head becomes `self`);
3. rewind `*_firstTypeInfo = prevHead`;
4. run the **DLL's** ctor into a private scratch `BaseTypeInfo`;
5. restore `*_firstTypeInfo = self` — undoing the original's link;
6. compare all seven written fields plus `scratch.next == prevHead` and `self->next == prevHead`.

The comparison is field-by-field, never `memcmp`: `+0x04` is written by neither side, and
comparing it is the Frustum-padding false alarm from WAVE1.md. This runs only under
`BANISHED_RT_DIFF=1`, and only during single-threaded static registration.

### `Type::Serialize` — no oracle exists

The call mutates the stream. Running it twice writes the type id twice and desyncs every
following record — precisely
`reference_slot3_wrapper_must_resolve_first.md` / `reference_serialize_lazy_resolve_and_wrong_slot.md`.
The site is registered so the run reports a real `calls=` count, and its verdict is the only
invariant available without a stream. **Read that site's `divergences=0` as "it executed", not
as "it agreed."** Its name in the summary says `[not diffable]` for that reason.

This is **not** a forwarder, unlike Wave 2's `Time::AsSeconds`. The body is a real transcription:
`ReaderWriter* rw = *(void**)(serializer + 8); rw->vtable[0x98](rw, this, 0)`. The object it
dispatches on is the DLL's, carrying the DLL's vtable, exactly as when retail runs — so the
virtual lands in the DLL either way, and nothing here changes when Wave 4 ports `ReaderWriter`.
`Serializer+0x08` is the `ReaderWriter*` and `Serializer+0x00` the `Stream*`, pinned by the
profile build's `Achievement::Serialize` (`0x180022880`). `IO::Serializer` itself is Wave 4 and
is **not** modelled.

---

## 5. Faithfulness decisions

* **No null guards were added, anywhere.** Retail's lookups deref the found node
  unconditionally: `CreateObject` does `MOV RCX,[RAX+0x10]` on a null RAX at `0x1800c0b03`, and
  `IsDerivedFrom` does `ADD RAX,0x8` on a null and then `CMP dword [8],EBX` at `0x1800c03e0`.
  Both fault on an unregistered type id, and that fault is how a missing registration announces
  itself. CLAUDE.md's rule applies verbatim. **The consequence to know: those faults now happen
  in `reimpl-x64.exe`, not in `Runtime-x64.dll`.** Symbolize with `nm` against the exe.
* **`GetTypeIdFromName`'s `existingOnly` parameter is accepted and ignored**, because retail
  ignores it — the retail disassembly (`0x1800c0580`..`0x1800c05b6`) never touches R8. The
  profile build shows what it was for: it guards
  `Assert("!existingOnly || IsTypeValid(type)", "Type is not registered!", "Object\\Factory.cpp", 0x59)`,
  which retail compiles out. Every call site of ours passes `true`. This is recorded in the
  source so nobody "cleans up" an unused parameter that is faithful.
* **`IsDerivedFrom` loads `Factory::_singleton` BEFORE the virtual `GetTypeId` call**, as retail
  does (`0x1800c039a`–`0x1800c03a6`). Harmless today; kept because it is what the listing says.
* **`Base::IsA` is an exact-type test, not a derivation test.** Worth stating because the two
  are one letter apart at the call site and the exe uses both.
* **`CreateObject(id, void*)` (`0x1800c0aa0`) was NOT ported.** Our tree does not call it
  (`call_surface_20260906.csv` has no row for it). It would be four lines — the same walk
  jumping through `info+0x28` instead of `+0x20` — but "called" is the honest denominator
  (PLAN §4.1 decision 1), and an uncalled port is an unverified port (WAVE1 defect 4).

---

## 6. Registration

`RtObjectTypeSystemRegister()` registers each of the six **by name and by address**, and two by
ordinal. The addresses matter: `engine/funcs/engine_funcs.cpp:1730` and `:2697` resolve
`IsDerivedFrom` and `CreateObject` with an `if (!fn) ResolveAddr(Module::Runtime, 0x1800c0390 /
0x1800c0ae0)` fallback, and `:4552`/`:4566` fall back to `ResolveOrdinal(2991)`/`(3089)`.
Registering only the names would leave any site whose name lookup failed silently on the DLL —
which is WAVE2 finding 4, and the reason `Filename`'s port nearly went inert.

Ordering is already correct and did not need changing: `RuntimePortInit()`
(`standalone/main.cpp:1599`) runs **before** `AddressRegistryInit()` (`:1611`) and
`EngineFuncsInit()` (`:1616`), so the `fn_*` table and the exe's IAT dispatch slots
(`0x140151410` `Base::IsA`, `0x140152a98` `Base::IsDerivedFrom`, `0x1401516b0`
`Type::Serialize`) all pick up the ports.

Every one of the six has live consumers, so no site should report `calls=0`:

| Function | Reached through | Volume |
|---|---|---|
| `Base::IsDerivedFrom` | `fn_IsDerivedFrom`, slot `0x140152a98` | several hundred call sites across `code/Game/Action`, `Component`, `Dialog` — the hottest in the lane |
| `Base::IsA` | `fn_IsA`, slot `0x140151410` | `cropfieldcomponent`, `orchardcomponent`, `toolui`, `optionsdialog` |
| `Factory::GetTypeIdFromName` | `fn_Factory_GetTypeIdFromName` | 8 sites (`mainmenudialog`, `newgamedialog`, `optionsdialog`, `mod_dialog_vtables`, `inputbindings`, `gameupdateloop_load`) |
| `Factory::CreateObject` | `fn_Factory_CreateObject` | `toolbardialog` ×2, `componententity` ×2 |
| `BaseTypeInfo::ctor` | `fn_BaseTypeInfo_Ctor` | `code/Game/Object/factory_registration.cpp:41` — once per registered exe type, in one burst |
| `Type::Serialize` | slot `0x1401516b0` | `uidescription`, `consumeproducedescription`, `sendcomponentmessage`, `controluiconfig` |

**No detour is installed** (as Waves 1 and 2). The DLL's own internal calls to these six still
land in the DLL. That is safe here because these ports read the DLL's live structures at their
real addresses and keep no replica — an object is interchangeable whichever side looked it up.
It stops being safe the moment a lane owns state, which is why `BaseTypeInfo::ctor`'s shadow
puts `_firstTypeInfo` back exactly as our port left it.

Kill switches: `BANISHED_RT_OBJECT=0` (whole namespace), `BANISHED_RT_TYPESYS=0` (this lane).
Both already existed in `rt_object_port.cpp`.

---

## 7. Comments corrected, and comments still wrong

### Corrected at the source (PLAN §6 step 8)

Two edits outside this lane's files, both minimal, both under the exception the brief granted
for PLAN §2.2 hazard B:

1. **`standalone/main.cpp:62` `ProbeBaseTypeInfo`** — field `abstract` renamed
   `fileExtension`, with the evidence in a comment. Every offset in that struct was already
   correct.
2. **`code/Game/Game/game_main.cpp:491` local `Probe`** — same field renamed `ext`, same note.

Neither field is read anywhere (`grep` for `->abstract` / `.abstract` returns nothing on either
struct), so the misnomer was inert. It was also believed: it is where the name came from.

3. **`standalone/main.cpp:521`, the `IsDerivedFrom` crash-handler comment** — a scope note was
   added. See below; this one is a correction of a claim in PLAN.md that the code was about to
   inherit.

### PLAN §2.2 hazard C: "Retire with Wave 3" is WRONG. Recommendation: KEEP, narrowed.

The crash handler matches `Runtime+0xC0390…0xC03A0` with `RCX == 0`. Porting `IsDerivedFrom`
does **not** retire it, because registration redirects **our** call sites only and no detour is
installed — the DLL's own internal callers still execute `Runtime+0xC0390` and this handler
still fires for them. What changed is the other half: a null `this` arriving from *our* code now
faults inside `reimpl-x64.exe`, where this branch cannot see it.

Retire it at **cutover** (Wave 9), when nothing calls the DLL copy any more — or earlier if a
detour is installed on `IsDerivedFrom`, which this lane did not do. The comment in `main.cpp`
now says this.

Incidentally the handler's own facts check out: `0xC039A` is `MOV RAX,[RCX]`, the first null
deref, and `EDX` still holds the type id there (it is copied to EBX at `0xC03A4`).

### Still wrong, NOT fixed by this lane — each needs an owner

| Where | Claim | Truth |
|---|---|---|
| `engine/funcs/engine_funcs.h:3694` | ctor comment says the third string is `abstract` | it is the **file extension** (`Factory::GetTypeIdForFileExtension`, `0x1800c04c0`). The rest of that comment — including "parent[colon-stripped]" and the 0x38 size — is right |
| `code/Game/Object/factory_registration.{h:45,cpp:19,cpp:43}` | `FactoryRegistration::abstractName`, ctor parameter `abstract` | same misnomer, in the type every exe-side registration flows through |
| `PLAN.md` §2.3 | `0x1801fc6e0` labelled `Object::Factory::_firstTypeInfo`; `0x1801fc6e8` labelled "Factory singleton (LoadDialogResource)"; the section says 17 of the 25 globals "have no export" | `0x1801fc6e0` is `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA` — it belongs to **BaseTypeInfo**, not Factory — and `0x1801fc6e8` is `?_singleton@Factory@Object@@0PEAV12@EA`. **Both are exported data symbols**, as is `?_crcTable@CRC@System@@0PAIA` at `0x1801f9080`. The "no export" count needs re-measuring, not just these three rows |
| `PLAN.md` §2.2 hazard C | "Retire with Wave 3" | see above |
| `PLAN.md` §4.3 risk 1 | `Set::Iterator` "is NOT pinned and is labelled a guess" | still true, and still unpinned; this lane never constructs one |

I did not edit `engine_funcs.h`, `factory_registration.*` or `PLAN.md`: the brief restricted
this lane to three files plus the hazard-B exception, and `engine_funcs.h` in particular is
shared with two concurrently-writing lanes. They are listed here so the orchestrator can land
them as one change.

### Ghidra renames (PLAN §6 step 2) — NOT done, and why

The ghidra-mcp rename tools (`rename_function_by_address`, `set_plate_comment`) take no
`program=` argument and act on the *current* program. With three lanes on one shared server and
`switch_program` forbidden by the brief, using them would have been a coin-flip about which
binary got renamed. The retail functions in this lane's scope are already correctly named and
demangled in the Ghidra database (`Object::Base::IsA`, `Object::Factory::CreateObject`, …)
because they are exports; the un-named things nearby are the `FUN_` internals — the Set
insert/rebalance helpers `FUN_180030140` / `FUN_180030380` / `FUN_180009350` / `FUN_1800093e0`,
which belong to whoever ports `Collection::Set`.

---

## 8. Defects found

| # | Defect | Class |
|---|---|---|
| 1 | `BaseTypeInfo+0x18` is the **file extension**, called `abstract` in five places across three files | **PRE-EXISTING**, latent — nothing reads the field. Fixed in two of the five; three listed in §7 |
| 2 | PLAN §2.2 hazard C tells the next reader to delete a crash handler that Wave 3 does **not** make dead | **PRE-EXISTING** (a documentation defect that would have destroyed working diagnostics). Corrected in `main.cpp`; PLAN.md still says it |
| 3 | PLAN §2.3 attributes `_firstTypeInfo` to `Factory` and claims these globals have no export; three of them are exported data symbols | **PRE-EXISTING**, documentation. Listed in §7 |

**No port bugs are claimed, because nothing has run.** Waves 1 and 2 both found their port bugs
in the diff run (`Quaternion::operator*`'s z term; `Thread`'s missing `CREATE_SUSPENDED`), not in
review. Expect the same here.

---

## 9. What the integration run must look for

1. **`calls=` on every one of the six sites, before `divergences=`.** All six have live
   consumers (§6). A `calls=0` on `Object::Base::IsDerivedFrom` in particular would mean the
   registration is not reaching the ~hundreds of `fn_IsDerivedFrom` sites, and the whole lane is
   inert (WAVE1 defect 4 / WAVE2 finding 4).
2. **`Object::BaseTypeInfo::BaseTypeInfo` should show a call count equal to the number of
   exe-side registrations** — `factory_registration.cpp` prints
   `[FactoryReg] Registered N types into Runtime _firstTypeInfo`. If the diff count and N
   disagree, the shadow is not running on every registration.
3. **`[GameMain] Post-Startup _firstTypeInfo: N entries, M have non-FF typeId`** must show
   `M == N` exactly as before the wave. That line is the direct end-to-end check that our ported
   ctor built a chain `Factory::Startup` could walk. A drop in N means head-insertion broke; an
   `M < N` means a name we stored is not CRC-ing to a registered id.
4. **`Object::Type::Serialize [not diffable]`** — its `divergences=0` means nothing. What proves
   it is the save/load pass: a wrong `Type::Serialize` consumes the wrong number of bytes and
   desyncs the record *after* it, on an innocent-looking component. Run the `Save9.sav` load.
5. **Any crash at an address inside `reimpl-x64.exe` with a fault address of `0x8` or
   `0x10`** is the faithful missing-type-id fault (§5), not a port bug — it means some type id
   is not registered. Symbolize with `nm` against `standalone/build/reimpl-x64.exe`; the old
   `Runtime+0xC03E0` signature no longer appears for our callers.
6. **Perf.** `IsDerivedFrom` is one of the hottest functions our tree calls, and PLAN §5's 5%
   gate is already breached and open at the Wave 2 boundary. This lane adds a registry
   indirection to it. It is worth an `sa-stress` A/B with `BANISHED_RT_TYPESYS=0` specifically,
   not just a whole-wave number.

---

## 10. Findings that change other lanes' or other waves' assumptions

* **The Set node shape is now measured, from a live consumer.** `Set<Pair<K,V>, …>::Node` is
  `{Key key @+0x00; Value value @+0x08; Node* parent @+0x30; left @+0x38; right @+0x40}` for a
  0x28-byte value — i.e. `parent/left/right` sit at `align8(sizeof key + sizeof value)`, and
  **the key is stored separately from the value even when the value contains it**. Any lane or
  wave reading a `Set` should start from that, and remember the WAVE2 trap: those offsets are a
  function of `sizeof(T)`, so one ported body cannot serve two element widths
  (`reference_one_bst_find_two_node_layouts.md`).
* **`Collection::Array`'s `{_data, _size:i32, _capacity:i32}` is confirmed a second time**, from
  `Factory::Startup`'s child-array growth (`0x1800c07d0`), independently of WAVE2's evidence.
  Its growth policy is `capacity = needed` on the first growth and `capacity = needed * 3`
  thereafter.
* **`Object::Factory::_singleton`, `BaseTypeInfo::_firstTypeInfo` and `System::CRC::_crcTable`
  are exported data symbols** and need no accessor and no hardcoded address. Lanes B and C
  should check whether the singletons they need (`InternalFactory::_singleton` `0x1801fc6d8`,
  `ExternalFactory::_singleton` `0x1801fc6c8`) are likewise exported — both appear in the
  export table.
* **The type id is a CRC-32 of the UTF-16LE name with the standard `0xEDB88320` table**, and the
  table is regenerable, so type-id computation survives cutover with no DLL dependency. Anything
  that needs a type id from a name can compute it rather than calling the Factory.
* **Vtable slot 2 (`+0x10`) of `Object::Base` is `GetTypeId`**, and every implementation seen is
  a constant load. That is what makes `IsA`/`IsDerivedFrom` diffable at all; a lane that ports a
  `Base` subclass must keep that slot side-effect-free or those two sites become unsound.
* **`Object` exports 7 `??_7` vtables** (`Base` `0x180154788`, `External` `0x1801547a0`,
  `Internal` `0x180154720`, `ExternalList` `0x180154800`, `CompiledExternal` `0x180154a30`,
  `ExtensionCompiler` `0x180155a88`, `ExternalCompiler` `0x180155aa8`). This lane needed none of
  them and makes **no** claim about whether stamping any is safe — per WAVE2 finding 1 that is a
  per-class argument, and lanes B and C own those classes.
