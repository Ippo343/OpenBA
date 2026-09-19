# IO / System caller inventory — B.3 step 4 prep (read-only)

Written 2026-09-18, on main `523f8566`. Measured with a comment/string-aware Python lexer
(`strip_comments_and_strings` in the scratchpad script referenced below — comments, `"..."`
and `'...'` literals blanked out before any occurrence is classified), scanning `code/`,
`engine/`, `standalone/`, excluding `standalone/build*/` and `tests/unit/build/` (anchored on
a path separator, so `buildinggoal.cpp` is not excluded). Every count below is **CALL**
(`name(` — a real call), separate from **GUARD** (`if (name)` / `name &&` / ternary test, not
followed by `(`), **CACHED** (assigned into a variable/file-static, or passed as a bare value),
**PRINTF-ARG** (inside a `printf`/`fprintf`/`DLOG`/`TRACE` argument list — flagged separately
because the perf build strips printf arguments, `reference_perf_build_strips_printf_arguments.md`),
and **DECL** (the typedef/extern/initializer lines in `engine_funcs.h`/`.cpp` themselves,
always 2-4 per pointer and excluded from every count below). Script and raw per-pointer output
live in the session scratchpad
(`inv-io-callers/{classify_ptrs2.py,lexer.py,ptrs.py,results.txt,dirbreakdown.txt}`) — not
committed, per the read-only mandate.

**Headline: 2,827 real CALL sites (plus 1,534 GUARD occurrences of the same names) across the
109 pointers inventoried below.** None of this has been touched; this is measurement only.

## 0. Format note

Per pointer: signature, how it's resolved, CALL/GUARD/CACHED/PRINTF counts, a per-directory
split of the CALL sites, and 1-3 representative sites (raw source line, comments intact).
"classes=" in the classification pass names the mangled-name owning class(es) found near the
declaration in `engine_funcs.h` (empty when the comment context didn't carry a mangled name —
still classified correctly by nearby prose).

## 1. The 9 serializer pointers handed over by B.3.3

Verified exact names against `engine/funcs/engine_funcs.h` (all match NEXT_SESSION.md §2a
verbatim). **Re-measured total: 130 CALL sites, not 159** — the 159 in NEXT_SESSION.md was
itself flagged as a token count pending re-measurement (§2c's own rule: "Re-measure call sites
with a comment/string-aware lexer before sizing a batch"); the gap is GUARD occurrences that a
naive `grep -c` folds into the same number the CALL sites use (an `if (fn_ExternalPtr_Serialize)`
guard mentions the name once more per call site, and several description files guard once for a
whole `Serialize()` body covering multiple calls).

| pointer | CALL | GUARD | CACHED | typedef | resolution |
|---|---|---|---|---|---|
| `fn_ExternalPtr_BuildSerialize` | 48 | 33 | 9 | `void(void*, void*)` | resolved in `EngineFuncsInit()`, exe dispatch slot (see `engine_funcs.cpp` initializer near line 3964) |
| `fn_ExternalPtr_Serialize` | 4 | 4 | 0 | `void(void*, void*)` | dispatch slot; FUN_14000bd30 per the header comment (line 3923) |
| `fn_ExternalPtr_Serialize2` | 17 | 13 | 0 | `void(void*, void*)` | dispatch slot; FUN_14000bf40 (line 3927) |
| `fn_ExternalPtr_Serialize3` | 8 | 5 | 0 | `void(void*, void*)` | dispatch slot; **the last reader of exe `0x140007140`** (FUN_140007140, line 3931) |
| `fn_ExternalPtr_SerializeTutorial` | 2 | 2 | 0 | `void(void*, void*)` | dispatch slot |
| `fn_ExternalPtr_Serialize_GraphicsMesh` | 2 | 2 | 1 | `void(void*, void*)` | dispatch slot |
| `fn_ExternalPtr_Serialize_Weather` | 1 | 1 | 1 | `void(void*, void*)` | dispatch slot |
| `fn_InternalBasePtr_Serialize` | 44 | 30 | 10 | `void(void*, void*)` | dispatch slot 0x140152aa0 (`standalone/address_registry.cpp:1208`, currently registered against the exe's own BST walk, not ours) |
| `fn_ExternalRefArray_Serialize` | 4 | 4 | 0 | `void(void*, int, void*, const char*)` | FUN_1400049c0 (line 3936) |
| **Total** | **130** | **94** | **21** | | |

Per-directory split of the 130 CALL sites (all in `code/Game/`, none in `standalone/` or
`engine/` bodies — only the 3 DECL lines each in `engine/funcs/`):

| directory | CALL sites |
|---|---|
| `code/Game/Component` | 91 |
| `code/Game/Work` | 15 |
| `code/Game/Game` | 11 |
| `code/Game/Action` | 9 |
| `code/Game/Tutorial` | 2 |
| `code/Game/Entity` | 2 |

Representative sites:
```
code/Game/Action/consumerawmaterial.cpp:134: fn_ExternalPtr_BuildSerialize(&legacyExtPtr, ser2);
code/Game/Component/createroaddescription.cpp:44: fn_ExternalPtr_Serialize3(s + 0x28, serializer);      // ExternalPtr +0x28
code/Game/Component/highlightdescription.cpp:96: fn_ExternalPtr_Serialize3(s + 0x30, ser);              // ref0 (+0x30)
code/Game/Action/createnaturalresource.cpp:205: fn_InternalBasePtr_Serialize(s + 0x28, ser2);
code/Game/Component/createbridgedescription.cpp:59: fn_ExternalRefArray_Serialize(serializer, 5, s + 0x38, nullptr); // ExtPtr[5] +0x38
```

Exe-address cross-reference: `fn_ExternalPtr_Serialize3` = last reader of `0x140007140`;
`fn_InternalBasePtr_Serialize`'s dispatch slot is `0x140152aa0` (`standalone/address_registry.cpp:1208`,
comment says it walks the retail BST, i.e. still exe-owned, not ours);
`fn_ExternalPtr_BuildSerialize`/`_Serialize`/`_Serialize2` correspond to FUN_14000bd30 / FUN_14000bf40
per the header comments at `engine_funcs.h:3923-3927`.

## 2. Every other IO-family pointer in engine_funcs.h

48 pointers beyond the 9 above (57 total IO-family, minus the 9 serializer ones already
tabled). Grouped by sub-area.

### 2a. `IO::Persistence` (settings store)

| pointer | CALL | GUARD | CACHED | signature |
|---|---|---|---|---|
| `fn_Persistence_SetBool` | 0 | 0 | 1 | `void(void* self, const char*, const char*, bool)` |
| `fn_Persistence_SetInt` | 0 | 0 | 1 | `void(void* self, const char*, const char*, int)` |
| `fn_Persistence_GetSavePath` | 2 | 4 | 0 | `void*(void* self, void* retval)` — non-trivial `String` return |
| `fn_Persistence_WriteSettings` | 1 | 1 | 0 | `void(const void* self)` |
| `fn_Persistence_ReadSettings` | 3 | 3 | 0 | `void(void* self)` |

`fn_Persistence_SetBool`/`SetInt` are **CACHED-only, zero CALL** — both occurrences are the
typedef comment block plus one address-of use in `SaveGameSettings`
(`engine_funcs.h:3708-3729`'s doc comment references them by name but the actual 16-entry
save/read loop calls through `g_pPersistenceSingleton` some other way not yet resolved to these
two typedefs — worth the B34 owner's attention, they may be dead already). Directory split for
the live ones: `code/Game/Dialog` (`optionsdialog.cpp` x3 WriteSettings/ReadSettings site),
`code/Game/Dialog/scenariodialog.cpp` + `code/Game/Tutorial/tutorial_next_scenario.cpp`
(GetSavePath, 1 each).
```
code/Game/Dialog/scenariodialog.cpp:432: fn_Persistence_GetSavePath(persistence, &savePathBuf);
code/Game/Dialog/optionsdialog.cpp:1149: fn_Persistence_WriteSettings(*g_pPersistenceSingleton);
```
Singleton: `extern void** g_pPersistenceSingleton;` (`engine_funcs.h:301`).

### 2b. `IO::FileSystem`

| pointer | CALL | GUARD | signature |
|---|---|---|---|
| `fn_FileSystem_GetDataDirectory` | 2 | 4 | `void*(void* self, void* retval)` — non-trivial `Filename` return |
| `fn_FileSystem_GetSourceDirectory` | 1 | 0 | `void*(void* self, void* retval)` |

Both in `code/Game/Dialog` (`scenariodialog.cpp`, `spritecaptureui.cpp`) and one in
`code/Game/Tutorial/tutorial_next_scenario.cpp`. Two distinct singleton globals:
`g_pFileSystemSingleton` (line 3540, "exref at 0x140151d18") and `g_pFileSystemSingleton2`
(line 1285, "separate exref for GetDataDirectory calls") — **the class has two independently
cached singleton pointers in our engine_funcs**, a fact the B34 owner needs when giving
`IO::FileSystem` a real `Get()`.

### 2c. `IO::Modification` (mod management) + `Mod` entry helpers

| pointer | CALL | GUARD | signature |
|---|---|---|---|
| `fn_Modification_Find` | 2 | 0 | `int(void* self, const void* filename)` |
| `fn_Modification_GetCount` | 4 | 1 | `int(void* self)` |
| `fn_Modification_GetMod` | 6 | 1 | `void*(void* self, int index)` |
| `fn_Modification_Swap` | 3 | 0 | `void(void* self, int, int)` |
| `fn_Modification_Enable` | 1 | 0 | `void(void* self, int, bool)` |
| `fn_Modification_RefreshList` | 2 | 0 | `void(void* self)` |
| `fn_Modification_SaveList` | 1 | 0 | `void(void* self)` |
| `fn_ModEntry_CopyCtor` | 0 | 0 (CACHED 1) | `void*(void* dest, const void* src)` |
| `fn_ModEntry_Assign` | 0 | 0 (CACHED 1) | `void*(void* dest, const void* src)` |
| `fn_ModEntry_Dtor` | 0 | 0 (CACHED 1) | `void(void* self)` |
| `fn_ModArrayCopy` | 0 | 0 (CACHED 1) | `void*(void* dest, const void* src)` |

All 19 CALL sites are in `code/Game/Dialog` — `moddialog.cpp` (13), `moddetailsdialog.cpp` (5),
`mod_dialog_vtables.cpp` (1). The four `fn_ModEntry_*`/`fn_ModArrayCopy` pointers are
**zero-CALL** — each shows exactly one CACHED hit, which on inspection is the typedef comment
plus the extern; no `code/`/`standalone/` file calls them by name today (the `Array<Mod>`
copy path they support may run through a different, uninventoried route — flag for B34, not a
claim they're provably dead, since a caller that copies a Mod by value without a symbol match
could be missed by a name-based lexer).
```
code/Game/Dialog/moddialog.cpp:547: fn_Modification_Swap(modSingleton, index, index - 1);
code/Game/Dialog/moddetailsdialog.cpp:158: void* mod = fn_Modification_GetMod(modification, modIndex);
```
Singleton: `g_ppModificationSingleton` (`engine_funcs.h:2423`, ".rdata at 0x1401519e8" — also
registered as a dispatch/exe-addr row, see §6).

### 2d. `IO::Serializer` core (non-`ExternalPtr`)

| pointer | CALL | GUARD | CACHED | note |
|---|---|---|---|---|
| `g_fn_Serializer_Ctor` | 187 | 138 | 24 | shared global (`g_` prefix, per the header's own comment at line 2549: "to avoid conflicts") |
| `g_fn_Serializer_Dtor` | 187 | 154 | 8 | paired 1:1 with Ctor at every site (same 187) |
| `g_fn_Serialize_Int` | 316 | 109 | 7 | by far the single highest-volume pointer in this inventory |
| `g_fn_Serialize_Bool` | 107 | 50 | 18 | |
| `g_fn_Serializer_GetVersion` | 48 | 17 | 10 | |
| `fn_Serializer_SerializeBulk` | 2 | 1 | 0 | bulk byte serialize |
| `fn_Serializer_GetEncoding` | 7 | 7 | 1 | dispatch slot 0x1401529f8 per header comment (line 5703) |
| `fn_Serializer_Serialize_Uint` | 18 | 12 | 1 | dispatch slot 0x140152a50 |
| `fn_Serializer_Serialize_Float` | 138 | 37 | 7 | |

**These 5 pointers alone (`g_fn_Serializer_Ctor/Dtor`, `g_fn_Serialize_Int/Bool`,
`fn_Serializer_Serialize_Float`) carry 861 of the section's CALL sites** — this is the true
bulk of B.3 step 4's caller-side work, an order of magnitude bigger than the 9 handed-over
serializer pointers in §1. Per-directory split (`g_fn_Serializer_Ctor`, representative of the
Ctor/Dtor/Int/Bool/GetVersion cluster, which travel together almost everywhere a
`Serializer ser1(...)`/`Serializer ser2(...)` local pair is constructed):

| directory | Ctor/Dtor CALL | `g_fn_Serialize_Int` CALL | `fn_Serializer_Serialize_Float` CALL |
|---|---|---|---|
| `code/Game/Component` | 94 | 153 | 97 |
| `code/Game/Action` | 32 | 19 | 0 |
| `code/Game/Game` | 21 | 90 | 41 |
| `code/Game/Tutorial` | 20 | 11 | 0 |
| `code/Game/Dialog` | 6 | 8 | 0 |
| `code/Game/Work` | 6 | 23 | 0 |
| `code/Game/Goal` | 5 | 5 | 0 |
| `code/Game/Entity` | 3 | 7 | 0 |

```
code/Game/Action/animatenaturalresource.cpp:125: g_fn_Serializer_Ctor(ser1, stream, 0);
code/Game/Action/animatenaturalresource.cpp:127: g_fn_Serializer_Dtor(ser1);
code/Game/Action/completeroad.cpp:92: g_fn_Serialize_Int(ser2, s + 0x20, nullptr);
code/Game/Component/animaldescription.cpp:66: fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x28), nullptr);
```

### 2e. Per-class `Serialize` bodies that call through the serializer (not the 9 `ExternalPtr` pointers)

| pointer | CALL | note |
|---|---|---|
| `fn_ElementDescription_Serialize` | 2 | `UI::ElementDescription::Serialize` base call |
| `fn_ComponentBase_Serialize` | 5 | `Component_Serialize` base (exe FUN ~0x14012e2a0 per header comment) |
| `fn_WeatherExternalSerialize` | 1 | |
| `fn_SerializeExternalPtr` | 1 | FUN_140028650 — External* as reference (header line 4001) |
| `fn_Work_SerializeBase` | 1 | `Work::Serialize` base (0x1400f7400) |
| `fn_BST_SerializeHelper` | 1 | serializes an `InternalBasePtr` BST (0x1400296b0) |
| `fn_Goal_BaseSerialize` | 3 | `Goal::Serialize` base |
| `fn_GoalInstance_Persist` | 1 | `GoalInstance::Persist` |
| `g_fn_Time_Serialize` | 32 | `System::Time::Serialize(Serializer&)` — listed again in §3 (it is genuinely both a Time member and a Serializer caller) |
| `fn_BucketTreeSerialize` | 1 | `code/Game/Entity/entitylist.cpp:1386` |
| `fn_System_Color_Serialize` | 6 | `System::Color::Serialize` |
| `fn_Struct60B_Serialize` | 0 (CACHED 1) | zero CALL today |
| `fn_Array_Resize_Stride20/08/14` | 1 / 9 / 2 | array-of-fixed-stride resize helpers used ahead of a serialize loop |
| `fn_Array_Serialize_Stride20` | 1 | |
| `fn_Array_Serialize_External_GraphicsMesh` | 5 | |
| `fn_Array_Serialize_External_Particle` | 1 | |
| `fn_TradeWork_impl_04d0` / `fn_TradeWork_impl_f1a0` | 0 / 0 (CACHED 1 each) | zero CALL — see the "Ser_Slot" cascade below, these are the trade-work dispatch cascade entry points that soft-fail on a miss (header comment at line 5060) |

Nearly all of these are `code/Game/Component` or `code/Game/Game`, with `fn_Goal_BaseSerialize`
in `code/Game/Goal` + `code/Game/Tutorial`, `fn_Work_SerializeBase`/`fn_BST_SerializeHelper` in
`code/Game/Work/buildstructure.cpp` (same file, same body).

### 2f. `Ser_Slot_*` raw dispatch-slot idiom (session-8 promoted externs)

Seven raw dispatch-slot externs typed via `fn_Ser_SerInt_t`/`fn_Ser_SerByte_t`/etc.
(`engine_funcs.h:5891-5920`, "First-class typedefs for the IO::Serializer dispatch slots that
session 8 exercises heavily"):

| pointer | slot address | CALL | GUARD |
|---|---|---|---|
| `fn_Ser_Slot_SerInt_AC8` | 0x140152ac8 | 28 | 17 |
| `fn_Ser_Slot_SerByte_410` | 0x140152410 | 5 | 2 |
| `fn_Ser_Slot_SerFloat_A60` | 0x140152a60 | 0 | 1 |
| `fn_Ser_Slot_SerUShortArr_488` | 0x140152488 | 0 | 1 |
| `fn_Ser_Slot_SerSizedRaw_3F8` | 0x1401523f8 | 1 | 2 |
| `fn_Ser_Slot_SerSizedRaw_400` | 0x140152400 | 1 | 2 |
| `fn_Ser_Slot_SerByteArr_618` | 0x140152618 | 0 | 1 |

`fn_Ser_Slot_SerInt_AC8` (28 calls, mostly `code/Game/Component/tradecomponent_serialize.cpp`)
is the live one; three of the seven (`SerFloat_A60`, `SerUShortArr_488`, `SerByteArr_618`) are
**zero-CALL today** — each appears only in a guard conjunct
(`code/Game/Game/gamegrid_serialize.cpp` builds a big `bool ok = fn_A && fn_B && fn_C && ...`
readiness check that mentions all seven, then only calls the ones the body actually needs).
Note this slot address list overlaps §6's dispatch-slot table — 0x140152a60 is also
`fn_Serializer_Serialize_Float`'s slot per §2d, i.e. **two different typedef'd pointers name
the same retail dispatch slot** (`fn_Serializer_Serialize_Float` and `fn_Ser_Slot_SerFloat_A60`)
— a duplication the B34 rule sheet should resolve to one name before conversion, or the
resave-gate risk is registering the same slot twice under two members.
```
code/Game/Component/tradecomponent_serialize.cpp:221: if (fn_Ser_Slot_SerInt_AC8) fn_Ser_Slot_SerInt_AC8(ser, &count, nullptr);
code/Game/Game/gamegrid_serialize.cpp:243: fn_Ser_Slot_SerByte_410(serializer, elem + 0x28, nullptr);
```

## 3. Every `System`-family pointer not already converted

String/Filename were converted in B.3.1 (per this prompt's framing) and are excluded. 16
pointers matched a `Class@System@@` owning-class mangled name or one of the example prefixes.

| pointer | CALL | GUARD | CACHED | class |
|---|---|---|---|---|
| `fn_MemAlloc` | 524 | 292 | 85 | `System::MemoryAllocator::MemAlloc` (member form, singleton) |
| `fn_MemFree` | 6 | 4 | 0 | `System::MemoryAllocator::MemFree` (member form) |
| `fn_MemFreeStatic` | 616 | 364 | 27 | static free — **the single highest-volume pointer of the whole System family** |
| `fn_MemAllocStatic` | 16 | 3 | 18 | `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`, static form, distinct from the member `fn_MemAlloc` (header note at line 5369) |
| `fn_MemZeroFill` | 0 | 0 | 0 | **zero occurrences anywhere outside its own DECL** — dead on arrival |
| `fn_Time_AsSeconds` | 47 | 17 | 17 | `System::Time::AsSeconds` |
| `fn_Time_Ctor_Float` | 111 | 82 | 4 | `System::Time::Time(float)` |
| `fn_DateTime_AsString` | 1 | 0 | 0 | `System::DateTime::AsString` — single site, `code/Game/Dialog/saveloaddialog.cpp:322` |
| `fn_Configuration_GetVersionString` | 3 | 0 | 0 | `System::Configuration::GetVersionString` |
| `fn_Configuration_CurrentVersion` | 1 | 1 | 0 | static |
| `fn_Configuration_CurrentBuildId` | 1 | 1 | 0 | static |
| `fn_Random_Rand` | 75 | 28 | 26 | `System::Random::Rand(max)` |
| `fn_Random_RandFloat` | 38 | 11 | 22 | |
| `fn_Random_RandTol` | 25 | 6 | 11 | |
| `fn_Random_Default_Ctor` | 13 | 5 | 1 | `System::Random::Random()` default ctor |
| `fn_Random_RandFloatTol` | 31 | 0 | 5 | declared `extern "C"` separately (line 2137), not through the typedef macro the others use |

**Total System-family CALL: 1,508** (`fn_MemAlloc` + `fn_MemFreeStatic` alone are 1,140 of it —
memory alloc/free is bigger by CALL count than every serializer pointer in §1+§2 combined).

Per-directory split for the two heaviest (`fn_MemAlloc`, `fn_MemFreeStatic`) and the `Random`
cluster:

| directory | MemAlloc | MemFreeStatic | Random (4 pointers summed) |
|---|---|---|---|
| `code/Game/Component` | 179 | 199 | 95 |
| `code/Game/Dialog` | 126 | 106 | 8 |
| `code/Game/Game` | 100 | 103 | 44 |
| `code/Game/Work` | 25 | 90 | 6 |
| `code/Game/Action` | 27 | 45 | 1 |
| `code/Game/Tutorial` | 28 | 24 | 2 |
| `code/Game/Player` | 17 | 21 | 1 |
| `code/Game/Goal` | 8 | 9 | 0 |
| `code/Game/Entity` | 5 | 8 | 0 |
| `engine` | 5 | 6 | 1 |
| `standalone` | 4 | 5 | 1 |

```
code/Game/Action/action_base_cnr.cpp:547: void* mem = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8) : nullptr;
code/Game/Action/action_base_hunt.cpp:149: fn_MemFreeStatic(oldBase);
code/Game/Component/animalcomponent.cpp:645: wanderDir = (rng && fn_Random_Rand) ? fn_Random_Rand(rng, 4) : 1;
```

### 3a. Not System, but same surface family — `Save::SaveLoad` (flagged, not in the 16 above)

`Save::SaveLoad` is its own namespace, not `System`, so it is out of the letter of §3's ask —
but it is the save/load control surface the IO step touches most directly, so it's reported
here rather than silently dropped:

| pointer | CALL | GUARD |
|---|---|---|
| `fn_SaveLoad_Dtor` | 4 | 0 |
| `fn_SaveLoad_GetOperation` | 11 | 4 |
| `fn_SaveLoad_SetSaveSprite` | 2 | 1 |
| `fn_SaveLoad_SetMinOperationTime` | 4 | 1 |
| `fn_SaveLoad_SetOperation` | 8 | 5 |
| `fn_SaveLoad_GetSaveCount` | 1 | 0 |
| `fn_SaveLoad_GetSaveInfo` | 9 | 1 |
| `fn_SaveLoad_GetSaveState` | 3 | 1 |
| `fn_LoadScenarioFromSave` | 1 | 0 |
| **Total** | **43** | **13** |

Almost entirely `code/Game/Dialog` (`saveloaddialog.cpp`, `autosavedialog.cpp`,
`dialog_exe_helpers.cpp`, `mainmenudialog.cpp`), with `standalone` touching
`fn_SaveLoad_GetOperation`/`GetSaveInfo` (2/1) and `code/Game/Tutorial` touching 3 of them via
`tutorial_next_scenario.cpp`. Singleton: `g_pSaveLoadSingleton` (line 1095, "exref at
0x140151610 (double-indirect)" — same slot address §6 lists under `reg_unreg_lane1.cpp:173`).

## 4. `SerializerLite` and raw Encoding-slot idioms on the exe side

**`SerializerLite` itself does not appear anywhere in `code/`, `engine/`, or `standalone/`** —
all 11 real hits are in `runtime/` (`Internal.h`, `internal.cpp`, `internal_class.cpp`,
`internal_ptr_array_serialize.cpp`, `external_class_doors.cpp`) plus docs, which is the other
agent's territory per this task's split. The exe side does not talk to `SerializerLite`
directly at all.

What the exe side actually uses is the `SerializerEncodingVCall` inline helper, defined once in
`engine/funcs/engine_funcs.h:3915-3921`:
```cpp
inline void SerializerEncodingVCall(void* serializer, int vtableOffset, void* field) {
    void* encoding = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(serializer) + 8);
    void* vtable = *reinterpret_cast<void**>(encoding);
    typedef void (*VFunc_t)(void*, void*);
    auto fn = *reinterpret_cast<VFunc_t*>(reinterpret_cast<uintptr_t>(vtable) + vtableOffset);
    fn(encoding, field);
}
```
This is a raw `(*(void***)x)[N]` slot call wrapped as a one-line helper — exactly the pattern
§4 asked to find. ~~39 call sites~~ **37 call sites across 22 files**
(**CORRECTED 2026-09-18 by B.3.4 step 1**: a non-comment recount of `SerializerEncodingVCall(`
reads 38 lines, of which one is the inline DEFINITION at `engine/funcs/engine_funcs.h`; the
37 call lines are `tradedescription.cpp` 5, `createbridgedescription.cpp` 5,
`toolbar.cpp`/`terraformdescription`/`populationdescription`/`happinessdescription`/
`fishingdescription`/`createplaceddescription`/`consumeproducedescription` 2 each, and 13
files with 1 each), all `code/Game/Component/*description.cpp`
(20 files, mostly one call each; `createbridgedescription.cpp` and `tradedescription.cpp` have
5 each) plus `code/Game/Game/toolbar.cpp` (2). Representative:
```
code/Game/Component/createbridgedescription.cpp: SerializerEncodingVCall(serializer_24bytes, 0x98, &field);
```
No `Ser_*`/`ENC_`/`WriteInt`/`ReadInt` macro family exists anywhere in `engine/funcs/`,
`engine/types/`, or `code/Game/**/**serialize**` — the only two idioms on the exe side are (a)
the flat `g_fn_Serialize_*`/`fn_Serializer_Serialize_*` typed dispatch pointers (§2d/§2f) and
(b) this one `SerializerEncodingVCall` helper. `code/Game/Component/coll_bridge.h` was checked
and contains no serialize helpers (it's a collision-bridge header, unrelated despite the name
similarity to "bridge" descriptions above).

**Serialize-named function bodies, by directory** (functions matching
`<Name>_Serialize<Suffix>(void* self, ...)`, the free-function convention this tree uses for a
ported `Serialize` body — 183 total):

| directory | count |
|---|---|
| `code/Game/Component` | 129 |
| `code/Game/Game` | 15 |
| `code/Game/Tutorial` | 15 |
| `standalone` | 11 |
| `code/Game/Dialog` | 8 |
| `code/Game/Action` | 4 |
| `code/Game/Work` | 1 |

`code/Game/Component` alone is 70% of all Serialize bodies on the exe side — consistent with
§1-2's per-directory CALL splits, where Component also dominates every serializer pointer.

## 5. IAT-cached copies

**Zero files cache any of the sections 1-3 pointers behind an IAT-slot-with-fallback pattern**
(the `s_fn_X = resolveIAT(0x14015....)` shape B.3.3 found for `fn_InternalBasePtr_Ctor`/
`_DeletePtr`, NEXT_SESSION.md §1.1). No `s_fn_*` local static in `code/`/`standalone/` falls
back to `g_fn_Serializer_*`, `fn_MemAlloc`, `fn_Random_*`, `fn_Time_*`, `fn_Persistence_*`,
`fn_FileSystem_*`, or `fn_Modification_*` by name.

**But there IS a large, different local-caching pattern the B34 owner needs to know about**:
**55 files** cache a `System`/`IO`-namespace function pointer as a `s_fn_*` file-static resolved
directly via `ResolveAddr`/`ResolveExport`/`RSlot` — bypassing `engine_funcs.h` entirely, with
no corresponding global `fn_*` pointer at all. These are NOT duplicates of the sections 1-3
pointers; they are separate, uninventoried resolutions of **additional** `System`-namespace
members this task's keyword sweep didn't have a global name to search for:

| local static | mangled export | files |
|---|---|---|
| `s_fn_ThreadCtor` / `s_fn_ThreadDtor` / `s_fn_ThreadStart` / `s_fn_WaitFinish` | `??0Thread@System@@`, `??1Thread@System@@`, `?Start@Thread@System@@`, `?WaitForFinish@Thread@System@@` | `code/Game/Action/action_pf_astar.cpp`, `code/Game/Action/keystone_a_zone.cpp` |
| `s_fn_CSCtor` / `s_fn_CSDtor` / `s_fn_CSLock` / `s_fn_CSUnlock` | `??0CriticalSection@System@@`, `??1CriticalSection@System@@`, `?Lock@CriticalSection@System@@`, `?Unlock@CriticalSection@System@@` | `code/Game/Game/pathfinder_seed.cpp`, `code/Game/Game/tornado.cpp`, `code/Game/Game/keystone_a_gamedtor.cpp` |
| `s_fn_randFloat` | `?RandFloat@Random@System@@` (a SECOND, independent resolution of the same export `fn_Random_RandFloat` already gets from `engine_funcs.h`) | `code/Game/Action/action_child_ctors2.cpp` |
| `s_fn_MemAllocSA` / `s_fn_MemFree` (as `pf_fn_MemFree_t`) | `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (the STATIC form — distinct from both `fn_MemAlloc` and `fn_MemAllocStatic`, a **third** MemAlloc surface) | `code/Game/Action/action_pf_astar.cpp`, `code/Game/Action/keystone_a_zone.cpp`, `code/Game/Component/citizenai_leisure.cpp` |

Also: `standalone/loader_loop_stubs.cpp`, `standalone/reg_mint_promoted.cpp`,
`standalone/rt_data_mint.cpp` reference `Thread@System@@`/`CriticalSection@System@@` in
resolver/registration context (not necessarily fresh `s_fn_` caches — worth the owner's own
pass, not double-counted here).

**Surprise for the B34 rule sheet**: `System::Thread` and `System::CriticalSection` are real,
frequently-touched retail classes with their own independent per-file resolution paths that
never went through `engine_funcs.h` — they are invisible to any sweep that only reads
`engine/funcs/engine_funcs.h`. If B.3 step 4 gives `System::Thread` a real class (its slots are
implied by the CLAUDE.md main-loop/threading doc), these 5+ call sites are the actual caller
surface, not zero.

## 6. exe dispatch slots

Rows in `standalone/address_registry.cpp` / `standalone/reg_*.cpp` that correspond to the
pointers above (verified against `tools/dispatch_slot_map.py`'s inputs by grepping the same
files it reads — not run, per the read-only mandate).

> **ADDED 2026-09-18 by B.3.4 step 1 (`B34_RULES.md` §6.5g): these rows STAY, for the whole of
> B.** `standalone/address_registry.cpp:1628-1645` is a contiguous block of **14**
> `RegisterDispatchSlot` rows bound to `RtSerializer_*`: 0x140152ac8 (int), 0x140152a60
> (float), 0x140152a58 (bool), 0x140152410 + 0x140152418 (uchar, the second a NameDB alias),
> 0x140152a50 (uint), 0x140152a48 (GetVersion), 0x140152488 (ushort[]), 0x140151278 (int[]),
> 0x1401523f8 (short[]), 0x140152490 + 0x140152400 (uchar[]), 0x140152618 + 0x140152478
> (SerializeBytes). **They are PE IMPORT SLOTS** — retail-shaped exe code reads them; they are
> not our call sites and they do not retire when a caller converts. Converting
> `IO::Serializer` to a real class changes the TARGET's spelling, never the row.
> `tools/dispatch_slot_map.py --check` must stay at zero mismatches across every commit.

| slot address | target | corresponds to |
|---|---|---|
| `0x140152ac8` | `RtSerializer_Serialize_Int` | `g_fn_Serialize_Int` / `fn_Ser_Slot_SerInt_AC8` (§2d/§2f — same slot, two typedef names) |
| `0x140152a60` | `RtSerializer_Serialize_Float` | `fn_Serializer_Serialize_Float` / `fn_Ser_Slot_SerFloat_A60` (same duplication) |
| `0x140152a58` | `RtSerializer_Serialize_Bool` | `g_fn_Serialize_Bool` |
| `0x140152a50` | `RtSerializer_Serialize_UInt` | `fn_Serializer_Serialize_Uint` |
| `0x140152a48` | `RtSerializer_GetVersion` | `g_fn_Serializer_GetVersion` |
| `0x140152aa0` | (InternalBasePtr::Serialize, retail body — not ours) | `fn_InternalBasePtr_Serialize` (§1) |
| `0x140152aa8` | `s_slot_TimeSerialize` (`reg_animal_serialize.cpp:73`) | `g_fn_Time_Serialize` |
| `0x140152b60` | MemAlloc(size, align) static | `fn_MemAllocStatic` |
| `0x140152b68` | MemFree(void*) static | pairs with the above; not in our `fn_*` list under this name — cross-check against `fn_MemFree`/`fn_MemFreeStatic` before B34 assumes it's covered |
| `0x140152b00` | `System::Time::Time(float)` ctor | `fn_Time_Ctor_Float` |
| `0x140151318` | `System::Time::AsSeconds() const` | `fn_Time_AsSeconds` |
| `0x140151610` | SaveLoad singleton double-indirect (`reg_unreg_lane1.cpp:173`) | `g_pSaveLoadSingleton` (§3a) |
| `0x140151d18` | FileSystem singleton (`reg_unreg_lane1.cpp:182`) | `g_pFileSystemSingleton` (§2b) |
| `0x140152a90` | (`reg_unreg_lane1.cpp:233`, unlabeled in this pass — check against InternalBasePtr_Ctor per B.3.3 carry-over item 1) | not one of this inventory's pointers; flag for cross-check with the B.3.3 carry-over |
| `0x1401519e8` | Modification singleton (`reg_mods.cpp:98`) | `g_ppModificationSingleton` (§2c) |

`fn_MemAlloc`'s member-form dispatch slot and `fn_Random_*`'s slots were not found as explicit
`RegisterDispatchSlot`/`RegisterExeAddr` rows in this grep pass — they may resolve purely via
`ResolveExport` at `EngineFuncsInit()` time rather than a `standalone/reg_*.cpp` row (consistent
with `fn_MemAlloc`'s and `fn_Random_Rand`'s CALL sites never mentioning a slot address in their
header comments, unlike the `Serializer`/`Time` family above). Confirm with
`tools/dispatch_slot_map.py --check` before assuming a gap.

## 7. Proposed batch split for the caller work (by directory, disjoint file sets)

Ordered by CALL-count weight (`code/Game/Component` dominates every pointer family, so it is
its own batch rather than folded into a smaller one):

| batch | scope | approx. CALL sites |
|---|---|---|
| **A — Component** | `code/Game/Component/*.cpp` (129 Serialize bodies, all of §2d/§2e/§2f's Component rows, all 39 `SerializerEncodingVCall` sites bar 2, most of `fn_MemAlloc`/`fn_MemFreeStatic`/`Random`) | ~1,196 |
| **B — Game** | `code/Game/Game/*.cpp` (menuscene.cpp, geometry_serialize.cpp, gamegrid_serialize.cpp, serialize_struct60b_serialize.cpp carry the bulk of §2e/§2f's non-Component rows) | ~580 |
| **C — Dialog** | `code/Game/Dialog/*.cpp` (all of §2a/§2b/§2c/§3a — Persistence, FileSystem, Modification, SaveLoad live almost entirely here) | ~369 |
| **D — Action + Work** | `code/Game/Action/*.cpp` + `code/Game/Work/*.cpp` (heavy `g_fn_Serializer_Ctor/Dtor` pairing + `fn_MemFreeStatic`) | ~402 |
| **E — Tutorial + Goal + Entity + Player** | remaining four directories, each individually small | ~247 |
| **F — engine + standalone** | 33 sites total, mostly `fn_MemAlloc`/`fn_MemFreeStatic`/`Random`/`Time`/`SaveLoad` fallbacks and the boot-time resolution code itself | ~33 |

Batches A-F are disjoint by construction (directory-partitioned). None should start until
`B34_RULES.md` answers §2b's coexistence question (`SerializerLite` vs a real `Encoding`
hierarchy) — every batch above touches a `Serializer`/`Encoding` call site, so a caller batch
converting the receiver type before that question is answered would need a second pass.
`fn_MemAlloc`/`fn_MemFreeStatic`/`fn_Random_*`/`fn_Time_*` conversions (System, not IO) have no
such dependency and could run independently/first if the coordinator wants early throughput
while the IO design question is still open.

## 8. Surprises

1. **The 159-vs-130 gap confirms the standing rule again** (§2c of NEXT_SESSION.md): raw
   `grep -c` on the 9 serializer pointers reads 159; the real CALL count is 130, with 94 GUARD
   occurrences of the same 9 names accounting for most of the difference.
2. **`g_fn_Serialize_Int`, `fn_MemFreeStatic`, and `fn_MemAlloc` are the three biggest pointers
   in the entire inventory** (316, 616, 524 CALL respectively) — none of them is in the 9
   handed-over serializer pointers, and only one (`Serialize_Int`) is IO at all. The step-4
   plan's framing ("the 9 serializer pointers... untouched on purpose") undersells the true
   size of the IO surface by roughly 9x once the non-`ExternalPtr` `Serializer` member calls
   are counted (§2d alone is 861 CALL sites).
3. **Two typedef'd pointers alias the same retail dispatch slot**: `fn_Serializer_Serialize_Float`
   and `fn_Ser_Slot_SerFloat_A60` are both `0x140152a60`; `g_fn_Serialize_Int` and
   `fn_Ser_Slot_SerInt_AC8` are both `0x140152ac8`. A real `Serializer` class needs to collapse
   these to one name before conversion or two call-site batches will independently "finish"
   the same member.
   > **DISPOSITION 2026-09-18 by B.3.4 step 1 (`B34_RULES.md` §6.5b): collapse BOTH before
   > caller batch A starts, as the owner's own separately-gated commit — this is the one piece
   > of caller work that must NOT wait for the `IO::Serializer` class.** 0x140152ac8: convert
   > the 28 `fn_Ser_Slot_SerInt_AC8` sites (mostly
   > `code/Game/Component/tradecomponent_serialize.cpp`) to `g_fn_Serialize_Int`, then delete
   > the `fn_Ser_SerInt_t` typedef, extern, initialiser and its guard conjunct in
   > `code/Game/Game/gamegrid_serialize.cpp`. 0x140152a60: `fn_Ser_Slot_SerFloat_A60` is
   > already **zero-CALL**, so it just dies with the other twelve zero-callers.
4. **`fn_Persistence_SetBool`/`SetInt` and all four `fn_ModEntry_*`/`fn_ModArrayCopy` pointers
   are zero-CALL** despite living in the same header blocks as heavily-used siblings — likely
   candidates for the "delete with their typedefs" bucket the B.3.3 closing pass used, but
   flagged rather than asserted dead (a name-based lexer can miss an indirect call through a
   function-pointer table).
5. **`fn_MemZeroFill` has zero occurrences anywhere**, including guards — genuinely
   never-referenced since its declaration.
6. **`System::Thread` and `System::CriticalSection` have an entire independent, uninventoried
   caller surface** (§5) that this task's `engine_funcs.h`-anchored sweep would have missed
   completely if not cross-checked against raw `ResolveExport(...)` calls — `System::Random`
   and the static `MemoryAllocator::MemAlloc` each also have a second, independent local
   resolution path alongside their `engine_funcs.h` global. The B34 owner should sweep
   `ResolveExport("...@System@@...")` and `ResolveExport("...@IO@@...")` directly, not just
   `engine_funcs.h`, when scoping `System::Thread`/`CriticalSection` conversion (out of this
   inventory's IO/System-rest scope as asked, but a blocker-in-waiting if step 4's rule sheet
   assumes `engine_funcs.h` is the complete caller list).
7. **`Save::SaveLoad` is not `System` or `IO`** by namespace but is the save/load control
   surface most directly adjacent to IO's Persistence/FileSystem work (§3a) — 43 CALL sites,
   concentrated in `code/Game/Dialog`, that the B34 rule sheet should explicitly scope in or
   out rather than leave ambiguous.
8. **`fn_LoadScenarioFromSave`'s single caller passes `savedState`/`scenarioIdx` through to
   `GameUpdateLoop`/`GameLoadDialogLoopT`/`LoaderLoop`** (per its header comment) — it is a
   thin wrapper the header itself says is "Called from ScenarioDialog_LoadSelected", so its
   conversion is coupled to `Save::SaveLoad`'s, not free-standing.

## 9. Method and known gaps

- Lexer strips `//`, `/* */`, and `"…"`/`'…'` literals per-file before classifying; comment-only
  and doc-string mentions are excluded from every count above by construction.
- GUARD vs CACHED boundary is heuristic at one shape: `bool ok = fn_A && fn_B;` (assigning a
  guard-chain result to a variable) is classified CACHED, not GUARD, because it is syntactically
  an assignment — semantically it still gates later calls the same way a `GUARD` does. This
  does not affect any CALL count, only the GUARD/CACHED split, which is reported for context
  and not used in the batch sizing in §7.
- `fn_Random_RandFloatTol` is declared `extern "C"` directly in `engine_funcs.h:2137` rather
  than through the typedef+extern macro pair every other pointer uses, so it shows DECL=1 not
  2-4 — not a data error.
