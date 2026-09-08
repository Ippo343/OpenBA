# Wave 9 — Lane V: a written verdict for every `[RtRawResidue]` row

**Gate row this fills:** WAVE9.md PRE-CUTOVER GATE CHECKLIST, *"`[RtRawResidue]` verdicts"*.

**Question:** of the 104 requests lane 0 measured going through the three *Raw* doors, which
must be satisfied before `Runtime-x64.dll` can be removed, and which die harmlessly with the
oracle?

**Status: 104 of 104 carry a verdict with stated evidence. 0 undetermined.**

| Verdict | Rows | Meaning at cutover |
|---|---|---|
| **REAL DEPENDENCY** | **26** | must be ported, stubbed or minted — otherwise a call through null |
| **DATA READ** | **29** | an exported symbol read for its contents; 26 covered by lane C2's mint, **3 not** |
| ORACLE-ONLY | 38 | reached only under `BANISHED_RT_DIFF` or at registration; dies harmlessly |
| DEAD | 11 | the raw-door call site no longer exists in the tree |

Full machine-readable table with per-row evidence:
`docs/runtime_port/wave9_rawresidue_verdicts.csv`.

---

## 1. What this measures, and how each verdict was reached

The population is fixed: the **104 distinct requests lane 0 observed** through
`ResolveExportRaw` / `ResolveRuntimeRaw` / `ResolveOrdinalRaw` across nine harness runs
(`wave9_rawresidue_union.txt`). This lane asks what each one *means*, at HEAD.

That is a different question from the one lane V2 is answering. **V2 asks what the SOURCE
would ask for; this lane asks what a RUN actually asked for.** Neither answers the other: a
source site the harness never drove is invisible here, and a request that arrived through a
named constant or a table walk is invisible to a source grep. §6 shows a case where each
found something the other could not.

Each verdict comes from four measured inputs joined per row, not from judgement alone:

1. **A literal sweep of the tree** for every occurrence of the mangled name *and* of any
   named constant bound to it (`kGeometryCtor`, `kExternalDtor`, …), across `code/`,
   `runtime/`, `engine/`, `standalone/` and `dx11/`. Each hit is bucketed by the door that
   encloses it — RAW (`ResolveExportRaw`, `RtOriginal`, `RT_ORIGINAL_ONCE`, `RtVtable`,
   `RtDataExport`, `VerifyBody`, `ResolveRuntimeRaw`), PORT-AWARE (`ResolveExport`,
   `ResolveAddr`, `RT_EXPORT_ONCE`), or REGISTER.
2. **`tools/rawdoor_audit.py --all`** (550 sites) for the per-site `CALL`/`resolve` ×
   `guarded`/`LIVE` classification.
3. **`tools/rawdoor_audit.py --deferred`** (74 sites) to follow a stored pointer to its use.
4. **`standalone/rt_data_mint_tables.inc`** for whether a DATA row is minted.

**A row was only allowed to be DEAD if the sweep found no raw-door site at all**, and the
evidence column then names the port-aware door and the registration that serve it instead.
"It did not appear in a run" was never sufficient and is not used anywhere below.

### Two bugs in my own classifier, both found by disbelieving a clean column

* **Window matching bucketed port-aware sites as raw.** Asking "does *any* door token appear
  in the four lines before the literal" put `mesh.cpp:244` (`RT_EXPORT_ONCE`) in the RAW
  bucket because `mesh.cpp:227` four lines up is `RT_ORIGINAL_ONCE`. Fixed by taking the
  **last** token before the literal, not any token. That alone moved `??DQuaternion`,
  `?Format@String` and `??0Geometry` off the REAL list — three rows that would have been
  reported as remaining cutover work when the repair had already landed. It is the same
  shape as pairing by proximity, which this wave has now paid for twice.
* **A uniformly empty RAW column for the 17 `Serialize` rows was a join defect, not a fact.**
  Their names live in the `kForwarders` table (`serializer.cpp:539-560`), walked by a single
  `ResolveExportRaw` at `:564`; no door token precedes the literal, so the pairing found
  nothing and would have called all 17 DEAD. They are mapped to `:564` explicitly.

---

## 2. The REAL DEPENDENCY list — the remaining cutover work on this door

26 rows. Grouped for reading; the per-row evidence is in §7.

| Area | Symbols | Note |
|---|---|---|
| **IO streams** | `??0/??1CompressedStream`, `??0/??1MemoryStream`, `?Open@CompressedStream`, `?Open@MemoryStream` | `CompressedMemoryStream`'s ctor/dtor/Open delegate to unported base-class bodies |
| **Video buffers** | `?PlatformConstruct@{Vertex,Index,Image}Buffer`, `?PlatformDestruct@ImageBuffer` | the dx11 companion-object factory calls; reached in 8–9 of 9 cells |
| **Graphics** | `?ResetAnimationState@AnimationState`, `?PlayAnimation@AnimationState`, `?SetBounds@Geometry` | accessors returning the pointer; callers are live ctors and `SetMesh` |
| **Allocator** | `?MemAlloc@MemoryAllocator`, `?MemFree@MemoryAllocator` | 8 live sites — **lane E**; the allocator gate is already GO |
| **Audio** | `?FindFree3DEffect@Scene`, `?Stop@Device` | `Stop` is WAVE8_5's stored-completion-callback holdback |
| **System strings** | `?DateAsString@DateTime`, `?TimeAsString@DateTime` | `value_types.cpp:318,320` — bare `RtOriginal` immediate calls |
| **Save** | `?GetDataDirectory@FileSystem`, `?UpdateList/UpdateLoad/UpdateLoadFromFile@SaveLoad` | the `SaveLoad::Update` switch still delegates three of its five arms |
| **UI** | `?CacheText@TextElement` | needs `String::FormatParamList` |
| **Object** | `?PersistObject@InternalFactory` | the read path of `InternalBasePtr::Serialize` |
| **CRT** | `addr:0x18007fbfc` | the DLL's **own** statically-linked `_beginthreadex`, from the `System::Thread` ctor. Not an export, not MinGW/UCRT's. Lane D's item, still open |

**`?MemAlloc` / `?MemFree` are the highest-value row in the table**: 9 of 9 cells, 8 live
sites across `audio`, `object`, `save`, `system`, `ui` and `video`, and lane E's allocator
port already closes them.

---

## 3. DATA READ — 29 rows, 3 of them not minted

All 29 are read live and *used*: a vtable stamped into slot 0, or a singleton slot
dereferenced. None is oracle-only. Cross-checked against `standalone/rt_data_mint_tables.inc`
(the mint shadow is clean, so a hit there is a real answer):

**26 covered.** All 12 singletons in the residue (`AnimationManager`, `Configuration@Video`,
`Device@Audio`, `Device@Input`, `Device@Video`, `Factory@Object`, `FileSystem@IO`,
`InternalFactory@Object`, `MainLoop@Engine`, `Modification@IO`, `Persistence@IO`,
`SaveLoad@Save`), ten vtables, `?_typeInfo@GoalInstance`, `?_firstTypeInfo@BaseTypeInfo`,
`?_frameIndex@ParticleInstance`.

**3 NOT covered — the only open item this lane hands lane C2:**

```
??_7MeshInstance@Graphics@@6B@       graphics/mesh.cpp:271   (0x180154dc0)
??_7EdgeMeshInstance@Graphics@@6B@   graphics/mesh.cpp:275   (0x180154d70)
??_7EdgeMesh@Graphics@@6B@           graphics/mesh.cpp:279   (0x180154c70)
```

All three go through `RtVtable()` → `ResolveExportRaw`, and each is stamped into slot 0 by a
ported constructor. They are the same shape as the ten vtables already in the tables; the
gap looks like coverage rather than a decision. Three of my six round-1 gaps
(`?_firstTypeInfo`, `?_secondsPerTick`, `?_frameIndex`) have since been added.

Two DATA rows are dereferenced with **no null guard**, so they fault rather than degrade if
the mint is not in place before them: `engine/engine.cpp:355` (`*VideoDeviceSingletonSlot()`
in the `MainLoop` ctor) and `video/device.cpp:639` (`*PersistenceSingletonSlot()`).

---

## 4. What moved since round 1 — every finding was acted on

Round 1 reported 70 REAL / 29 DATA / 3 ORACLE-ONLY / 2 DEAD. It is now 26 / 29 / 38 / 11.
The population did not change; the tree did.

| Round-1 finding | State at HEAD |
|---|---|
| **The IO::Serializer registration gate disabled the port at cutover** — 22 rows, the wave's worst. A null resolve made every structural check "fail", `g_portOn` went false, and all 22 bodies fell back to a null `RT_ORIGINAL_ONCE` on the save/load path | **REPAIRED.** `VerifyBody` is three-valued (`kUnresolved`, `serializer.cpp:174`), the loop has an `if (!fn) { ++g_verifyUnresolved; }` arm (`:565`), the port registers anyway (`:654-659`), and every `!g_portOn` fallback is null-checked. All 22 rows are now ORACLE-ONLY |
| **Two oracle calls escaped their guard** — `?IsPaused@Scene@Audio` (audio.cpp:918) and `?GetSprite@ImageElement` (widgets_layout.cpp:290) called the DLL on every ordinary run and would be null calls at cutover | **REPAIRED**, both now inside `if (rt_diff_on__)`, with the reasoning recorded at `widgets_layout.cpp:294-296` |
| **Nine port-bypasses on the raw door** (`??0/??1Geometry`, `??1External`, `??DQuaternion`, `??4String`, `??HSystem`, `??YString`, `?Set@String`, `?AdjustDelimiters`) | **ALL NINE REPAIRED** — converted to `RT_EXPORT_ONCE`/`ResolveExport`. `tools/port_bypass_audit.py` now reports **0 bypasses on both doors** (189 address sites, 437 name sites) |
| **Six DATA symbols missing from the mint** | **3 added** (`?_firstTypeInfo`, `?_secondsPerTick`, `?_frameIndex`); 3 Graphics vtables still open (§3) |

`?_secondsPerTick@Timer@System@@0NA` is worth its own line: it has **no code site of any kind
left**. `engine/engine.cpp:221` and `save/saveload.cpp:168` call the ported
`RtSystem::TimerSecondsPerTick()`, and `saveload.cpp:163-165` names this lane's report as the
reason for the change. It is satisfied twice over — by a port *and* by a mint entry.

---

## 5. ORACLE-ONLY — what "verified" means for the 38

Every one is verified by the gate being named, not inferred:

* **22 rows** are the Serializer registration check (§4), whose cutover behaviour is now the
  `kUnresolved` branch. Evidence quoted per row.
* **13 rows** have every raw site classified `guarded` by `rawdoor_audit --all` — inside
  `if (rt_diff_on__)`, inside an `RT_DIFF_*` argument, or in a function that returns early on
  `if (!RtDiffEnabled())`.
* **3 rows** needed reading, because the *resolve* is unguarded while every *call* through
  the pointer is not: `??1Internal@Object` (`dialog.cpp:897` is an argument to
  `VerifyDtorThunk`, which returns at `dialog.cpp:401` when diffing is off),
  `??_7DialogController@UI` (same call), and `?IsEffectPlaying@Scene@Audio`
  (`audio.cpp:840`, `:1045` store into a local; all five calls are guarded).

**The distinction that carries all three of those: resolve-outside-guard is harmless, because
`ResolveExportRaw` returns null safely with no module handle. Call-outside-guard is fatal.
The residue cannot tell them apart — only reading the site can.**

One caveat that will bite anyone reading only this file: **ORACLE-ONLY is a verdict about the
raw door.** `??1Internal@Object` and `??_7DialogController@UI` are *also* read live through
`ResolveExport` at `dialog.cpp:730-731`. That is port-registry residue, already in
`wave9_residue_union.txt`, and it does not go away because this door is clean.

---

## 6. For lane V2: `rawdoor_audit`'s UNGUARDED-CALL count is a floor

While joining, three sites came back as `resolve LIVE` that are in fact
resolve-and-call-in-one-expression, and are therefore missing from the 27 the tool reports:

```
runtime/src/audio/audio.cpp:439    RT_ORIGINAL_ONCE(FindFreeFn,
                                       "?FindFree3DEffect@…")(self, soundRef, time, pos);
runtime/src/audio/audio.cpp:748    RT_ORIGINAL_ONCE(AudioDeviceStopFn,
                                       "?Stop@Device@Audio@…")(device, p);
runtime/src/save/saveload.cpp:570  RT_ORIGINAL_ONCE(GetDataDirectoryFn,
                                       "?GetDataDirectory@…")(FileSystemSingleton(), &dataDir);
```

In each, the argument list opens on the line **after** the macro, so the tool's
immediate-call test does not fire and it records a bare resolve. All three are live calls
outside every guard, and all three are null calls at cutover — they are in this lane's REAL
DEPENDENCY list on the strength of reading them.

This is the complement of §1's own bug and the reason both lanes exist: **driving
UNGUARDED-CALL to zero is necessary and is not sufficient.** A line-wrapped call is invisible
to it, and its own docstring already excludes the accessor pattern (`return
RT_ORIGINAL_ONCE(...)` called from elsewhere), which is how `?ResetAnimationState`,
`?PlayAnimation`, `?SetBounds` and the four `PlatformConstruct`/`PlatformDestruct` rows reach
the DLL. I have not edited the tool — lane V2 owns it.

---

## 7. The table — all 104 rows

Verdict definitions: **REAL DEPENDENCY** = must be satisfied before the DLL is removed.
**DATA READ** = an exported symbol read for its contents. **ORACLE-ONLY** = reached only from
a diff guard or a registration-time check that is cutover-safe. **DEAD** = no raw-door call
site exists in the tree at HEAD.

### REAL DEPENDENCY — 26 rows

| Symbol | Cells | Raw site(s) at HEAD | Evidence | What satisfies it |
|---|---|---|---|---|
| `??0CompressedStream@IO@@QEAA@XZ` | 5 | runtime/src/io/compressedstream.cpp:189 | rawdoor_audit UNGUARDED-CALL at runtime/src/io/compressedstream.cpp:189 - the resolved pointer is CALLED outside every diff guard, so at cutover this is a call through null | port, stub or mint the callee |
| `??0MemoryStream@IO@@QEAA@XZ` | 5 | runtime/src/io/compressedstream.cpp:191 | rawdoor_audit UNGUARDED-CALL at runtime/src/io/compressedstream.cpp:191 - the resolved pointer is CALLED outside every diff guard, so at cutover this is a call through null | port, stub or mint the callee |
| `??1CompressedStream@IO@@UEAA@XZ` | 5 | runtime/src/io/compressedstream.cpp:315 | rawdoor_audit UNGUARDED-CALL at runtime/src/io/compressedstream.cpp:315 - the resolved pointer is CALLED outside every diff guard, so at cutover this is a call through null | port, stub or mint the callee |
| `??1MemoryStream@IO@@UEAA@XZ` | 5 | runtime/src/io/compressedstream.cpp:314 | rawdoor_audit UNGUARDED-CALL at runtime/src/io/compressedstream.cpp:314 - the resolved pointer is CALLED outside every diff guard, so at cutover this is a call through null | port, stub or mint the callee |
| `?CacheText@TextElement@UI@@AEAAXXZ` | 9 | runtime/src/ui/widgets_layout.cpp:95 | rawdoor_audit UNGUARDED-CALL at runtime/src/ui/widgets_layout.cpp:95 - the resolved pointer is CALLED outside every diff guard, so at cutover this is a call through null | port, stub or mint the callee |
| `?DateAsString@DateTime@System@@QEBA?AVString@2@XZ` | 1 | runtime/src/system/value_types.cpp:318 | rawdoor_audit UNGUARDED-CALL at runtime/src/system/value_types.cpp:318 - the resolved pointer is CALLED outside every diff guard, so at cutover this is a call through null | port, stub or mint the callee |
| `?FindFree3DEffect@Scene@Audio@@AEBAHV?$ExternalPtr@VSoundEffect@Audio@@@Obj…` | 2 | runtime/src/audio/audio.cpp:439 | READ BY HAND. rawdoor_audit classifies this `resolve LIVE`, not UNGUARDED-CALL, because the argument list opens on the NEXT line; the resolve and the call are one expression and the call is outside every diff guard. At cutover it is a call through null. Site: audio/audio.cpp:439 in Audio::Scene::PlayEffect3D | port Audio::Scene::FindFree3DEffect (private but exported) |
| `?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ` | 1 | runtime/src/save/saveload.cpp:570 | READ BY HAND. rawdoor_audit classifies this `resolve LIVE`, not UNGUARDED-CALL, because the argument list opens on the NEXT line; the resolve and the call are one expression and the call is outside every diff guard. At cutover it is a call through null. Site: save/saveload.cpp:570 in Save::SaveLoad::CheckMod | port IO::FileSystem::GetDataDirectory |
| `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` | 9 | runtime/src/audio/audio.cpp:105; runtime/src/object/internal.cpp:110; runtime/src/save/saveload.cpp:125; runtime/src/system/string.cpp:69; runtime/src/ui/sprite.cpp:482; standalone | rawdoor_audit UNGUARDED-CALL at runtime/src/audio/audio.cpp:105, runtime/src/save/saveload.cpp:125 - the resolved pointer is CALLED outside every diff guard, so at cutover this is a call through null | port, stub or mint the callee |
| `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` | 9 | runtime/src/audio/audio.cpp:108; runtime/src/object/internal.cpp:116; runtime/src/save/saveload.cpp:128; runtime/src/system/string.cpp:75; runtime/src/ui/sprite.cpp:143; runtime/sr | rawdoor_audit UNGUARDED-CALL at runtime/src/audio/audio.cpp:108, runtime/src/save/saveload.cpp:128, runtime/src/ui/widgets_layout.cpp:115 - the resolved pointer is CALLED outside every diff guard, so at cutover this is a call through null | port, stub or mint the callee |
| `?Open@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z` | 5 | runtime/src/io/compressedstream.cpp:375 | resolve on a LIVE path at runtime/src/io/compressedstream.cpp:375, and rawdoor_audit follows the pointer: DEFERRED-CALL -> escape-return at :392  (via `ok` in RtIo_CompressedMemoryStream_Open) | port, stub or mint the callee |
| `?Open@MemoryStream@IO@@QEAA_NH@Z` | 5 | runtime/src/save/saveload.cpp:808 | rawdoor_audit UNGUARDED-CALL at runtime/src/save/saveload.cpp:808 - the resolved pointer is CALLED outside every diff guard, so at cutover this is a call through null | port, stub or mint the callee |
| `?PersistObject@InternalFactory@Object@@QEAAPEAVInternal@2@AEAVStream@IO@@IHI@Z` | 5 | runtime/src/object/internal.cpp:624 | resolve on a LIVE path at runtime/src/object/internal.cpp:624, and rawdoor_audit follows the pointer: DEFERRED-CALL -> call at :626  (via `persist` in RtObject_InternalBasePtr_Serialize) | port, stub or mint the callee |
| `?PlatformConstruct@ImageBuffer@Video@@AEAAXXZ` | 9 | runtime/src/video/buffers.cpp:54 | resolve on a LIVE path at runtime/src/video/buffers.cpp:54 in an accessor that RETURNS the pointer, so it escapes to a live caller (verified by reading the callers) | port, stub or mint the callee |
| `?PlatformConstruct@IndexBuffer@Video@@QEAAXXZ` | 9 | runtime/src/graphics/mesh.cpp:256; runtime/src/video/buffers.cpp:60 | resolve on a LIVE path at runtime/src/graphics/mesh.cpp:256, runtime/src/video/buffers.cpp:60 in an accessor that RETURNS the pointer, so it escapes to a live caller (verified by reading the callers) | port, stub or mint the callee |
| `?PlatformConstruct@VertexBuffer@Video@@QEAAXXZ` | 9 | runtime/src/graphics/mesh.cpp:253; runtime/src/video/buffers.cpp:66 | resolve on a LIVE path at runtime/src/graphics/mesh.cpp:253, runtime/src/video/buffers.cpp:66 in an accessor that RETURNS the pointer, so it escapes to a live caller (verified by reading the callers) | port, stub or mint the callee |
| `?PlatformDestruct@ImageBuffer@Video@@AEAAXXZ` | 8 | runtime/src/video/buffers.cpp:57 | resolve on a LIVE path at runtime/src/video/buffers.cpp:57 in an accessor that RETURNS the pointer, so it escapes to a live caller (verified by reading the callers) | port, stub or mint the callee |
| `?PlayAnimation@AnimationState@Graphics@@QEAAXV?$ExternalPtr@VAnimationGroup…` | 7 | runtime/src/graphics/mesh.cpp:240 | READ BY HAND. mesh.cpp:240 is an accessor returning the resolved pointer; called live from RtGraphics_MeshInstance_PlayAnimation | port Graphics::AnimationState::PlayAnimation |
| `?ResetAnimationState@AnimationState@Graphics@@QEAAXXZ` | 9 | runtime/src/graphics/mesh.cpp:237 | resolve on a LIVE path at runtime/src/graphics/mesh.cpp:237 in an accessor that RETURNS the pointer, so it escapes to a live caller (verified by reading the callers) | port, stub or mint the callee |
| `?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z` | 1 | runtime/src/graphics/mesh.cpp:233; runtime/src/graphics/scene.cpp:1039 | READ BY HAND. mesh.cpp:233 is an accessor that RETURNS the resolved pointer; its caller (EdgeMeshInstance::SetMesh) calls it on a normal path. graphics/scene.cpp:1039 is a second raw use | port or stub Graphics::Geometry::SetBounds |
| `?Stop@Device@Audio@@QEAAXPEAUAudioChannel@12@@Z` | 7 | runtime/src/audio/audio.cpp:748 | READ BY HAND. rawdoor_audit classifies this `resolve LIVE`, not UNGUARDED-CALL, because the argument list opens on the NEXT line; the resolve and the call are one expression and the call is outside every diff guard. At cutover it is a call through null. Site: audio/audio.cpp:748 in ReleasePlatformChannels, reached from PauseEffect and StopEffect | port or stub Audio::Device::Stop - WAVE8_5 held it back on the stored-completion-callback root cause |
| `?TimeAsString@DateTime@System@@QEBA?AVString@2@XZ` | 1 | runtime/src/system/value_types.cpp:320 | rawdoor_audit UNGUARDED-CALL at runtime/src/system/value_types.cpp:320 - the resolved pointer is CALLED outside every diff guard, so at cutover this is a call through null | port, stub or mint the callee |
| `?UpdateList@SaveLoad@Save@@QEAAXXZ` | 1 | runtime/src/save/saveload.cpp:663 | rawdoor_audit UNGUARDED-CALL at runtime/src/save/saveload.cpp:663 - the resolved pointer is CALLED outside every diff guard, so at cutover this is a call through null | port, stub or mint the callee |
| `?UpdateLoad@SaveLoad@Save@@QEAAXXZ` | 1 | runtime/src/save/saveload.cpp:666 | rawdoor_audit UNGUARDED-CALL at runtime/src/save/saveload.cpp:666 - the resolved pointer is CALLED outside every diff guard, so at cutover this is a call through null | port, stub or mint the callee |
| `?UpdateLoadFromFile@SaveLoad@Save@@QEAAXXZ` | 4 | runtime/src/save/saveload.cpp:669 | rawdoor_audit UNGUARDED-CALL at runtime/src/save/saveload.cpp:669 - the resolved pointer is CALLED outside every diff guard, so at cutover this is a call through null | port, stub or mint the callee |
| `addr:0x18007fbfc` | 7 | runtime/src/system/thread_win.cpp:152 | thread_win.cpp:152 ResolveRuntimeRaw(0x18007fbfc), DEFERRED-CALL -> escape-return at :153 (via `f` in RtDllBeginThreadEx); called live from RtSystem_Thread_Ctor. The DLL's OWN statically-linked CRT _beginthreadex, not an export and not MinGW/UCRT's | lane D: replace with UCRT _beginthreadex (answering the thread-local CRT state question) or System::Thread cannot start a thread |

### DATA READ — 29 rows

| Symbol | Cells | Raw site(s) at HEAD | Evidence | What satisfies it |
|---|---|---|---|---|
| `??_7Base@Object@@6B@` | 9 | runtime/src/achievement/goal.cpp:114; runtime/src/graphics/mesh.cpp:283; runtime/src/object/external.cpp:167; runtime/src/object/internal.cpp:159 | resolve on a LIVE path at runtime/src/achievement/goal.cpp:114, runtime/src/object/external.cpp:167, runtime/src/object/internal.cpp:159; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/achievement/goal.cpp:114 -> escape-return at :115  (via `p` in BaseVtable) | lane C2 mint covers it |
| `??_7CompiledExternal@Object@@6B@` | 8 | runtime/src/video/buffers.cpp:182 | resolve on a LIVE path at runtime/src/video/buffers.cpp:182; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) | lane C2 mint covers it |
| `??_7CompressedMemoryStream@IO@@6B@` | 5 | runtime/src/io/compressedstream.cpp:146 | resolve on a LIVE path at runtime/src/io/compressedstream.cpp:146; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/io/compressedstream.cpp:146 -> escape-return at :147  (via `v` in VtblCompressedMemoryStream) | lane C2 mint covers it |
| `??_7DialogController@UI@@6B@` | 9 | runtime/src/ui/dialog.cpp:896 | resolve on a LIVE path at runtime/src/ui/dialog.cpp:896; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) | lane C2 mint covers it |
| `??_7EdgeMesh@Graphics@@6B@` | 1 | runtime/src/graphics/mesh.cpp:279 | READ BY HAND. graphics/mesh.cpp:279 via RtVtable(); stamped into slot 0 by the EdgeMesh ctor | **NOT MINTED** — NOT in the mint tables - lane C2 must add it |
| `??_7EdgeMeshInstance@Graphics@@6B@` | 1 | runtime/src/graphics/mesh.cpp:275 | READ BY HAND. graphics/mesh.cpp:275 via RtVtable(); stamped into slot 0 by the EdgeMeshInstance ctor | **NOT MINTED** — NOT in the mint tables - lane C2 must add it |
| `??_7External@Object@@6B@` | 9 | runtime/src/achievement/goal.cpp:118; runtime/src/graphics/mesh.cpp:287; runtime/src/object/external.cpp:163 | resolve on a LIVE path at runtime/src/achievement/goal.cpp:118, runtime/src/object/external.cpp:163; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/achievement/goal.cpp:118 -> escape-return at :119  (via `p` in ExternalVtable) | lane C2 mint covers it |
| `??_7Goal@Achievement@@6B@` | 9 | runtime/src/achievement/goal.cpp:122 | resolve on a LIVE path at runtime/src/achievement/goal.cpp:122; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/achievement/goal.cpp:122 -> escape-return at :123  (via `p` in GoalVtable) | lane C2 mint covers it |
| `??_7GoalInstance@Achievement@@6B@` | 7 | runtime/src/achievement/goal.cpp:126 | resolve on a LIVE path at runtime/src/achievement/goal.cpp:126; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/achievement/goal.cpp:126 -> escape-return at :127  (via `p` in GoalInstanceVtable) | lane C2 mint covers it |
| `??_7ImageBuffer@Video@@6B@` | 9 | runtime/src/video/buffers.cpp:90 | resolve on a LIVE path at runtime/src/video/buffers.cpp:90; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/video/buffers.cpp:90 -> escape-return at :91  (via `v` in ImageBufferVtable) | lane C2 mint covers it |
| `??_7ImageBufferData@Video@@6B@` | 9 | runtime/src/video/buffers.cpp:94; runtime/src/video/companion_data.cpp:130 | resolve on a LIVE path at runtime/src/video/buffers.cpp:94, runtime/src/video/companion_data.cpp:130; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/video/buffers.cpp:94 -> escape-return at :95  (via `v` in ImageBufferDataVtable) | lane C2 mint covers it |
| `??_7MeshInstance@Graphics@@6B@` | 9 | runtime/src/graphics/mesh.cpp:268 | READ BY HAND. graphics/mesh.cpp:271 via RtVtable() -> ResolveExportRaw; the value is stamped into slot 0 by RtGraphics_MeshInstance_Ctor | **NOT MINTED** — NOT in the mint tables - lane C2 must add it |
| `??_7RenderStateData@Video@@6B@` | 7 | runtime/src/video/companion_data.cpp:123; runtime/src/video/device.cpp:535 | resolve on a LIVE path at runtime/src/video/companion_data.cpp:123, runtime/src/video/device.cpp:535; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/video/companion_data.cpp:123 -> escape-return at :123  (via `v` in Vt_RenderStateData) | lane C2 mint covers it |
| `??_7Thread@System@@6B@` | 7 | runtime/src/system/thread_win.cpp:126 | resolve on a LIVE path at runtime/src/system/thread_win.cpp:126; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/system/thread_win.cpp:126 -> escape-return at :127  (via `v` in RtSystemThreadVftable) | lane C2 mint covers it |
| `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA` | 9 | runtime/src/object/type_system.cpp:65 | resolve on a LIVE path at runtime/src/object/type_system.cpp:65; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) | lane C2 mint covers it |
| `?_frameIndex@ParticleInstance@Graphics@@1HA` | 9 | runtime/src/graphics/animation.cpp:189 | resolve on a LIVE path at runtime/src/graphics/animation.cpp:189; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/graphics/animation.cpp:189 -> escape-return at :190  (via `p` in RtFrameIndexSlot) | lane C2 mint covers it |
| `?_singleton@AnimationManager@Graphics@@0PEAV12@EA` | 6 | runtime/src/graphics/mesh.cpp:296 | resolve on a LIVE path at runtime/src/graphics/mesh.cpp:296; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) | lane C2 mint covers it |
| `?_singleton@Configuration@Video@@0PEAV12@EA` | 9 | runtime/src/video/buffers.cpp:107; runtime/src/video/device.cpp:130 | resolve on a LIVE path at runtime/src/video/buffers.cpp:107, runtime/src/video/device.cpp:130; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/video/buffers.cpp:107 -> escape-return at :108  (via `p` in ConfigurationSingletonPtr) | lane C2 mint covers it |
| `?_singleton@Device@Audio@@0PEAV12@EA` | 7 | runtime/src/audio/audio.cpp:728 | resolve on a LIVE path at runtime/src/audio/audio.cpp:728; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) | lane C2 mint covers it |
| `?_singleton@Device@Input@@0PEAV12@EA` | 7 | runtime/src/input/input.cpp:95; standalone/rt_orphan_probe.cpp:793 | resolve on a LIVE path at runtime/src/input/input.cpp:95, standalone/rt_orphan_probe.cpp:793; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) | lane C2 mint covers it |
| `?_singleton@Device@Video@@0PEAV12@EA` | 9 | runtime/src/engine/engine.cpp:211; runtime/src/video/buffers.cpp:103; runtime/src/video/device.cpp:125 | resolve on a LIVE path at runtime/src/engine/engine.cpp:211, runtime/src/video/buffers.cpp:103, runtime/src/video/device.cpp:125; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/engine/engine.cpp:211 -> escape-return at :212  (via `p` in VideoDeviceSingletonSlot) | lane C2 mint covers it |
| `?_singleton@Factory@Object@@0PEAV12@EA` | 9 | runtime/src/object/external.cpp:674; runtime/src/object/type_system.cpp:69 | resolve on a LIVE path at runtime/src/object/external.cpp:674, runtime/src/object/type_system.cpp:69; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) | lane C2 mint covers it |
| `?_singleton@FileSystem@IO@@0PEAV12@EA` | 1 | runtime/src/save/saveload.cpp:152 | READ BY HAND. save/saveload.cpp:152 RtDataExport<void*>(...) -> ResolveExportRaw; the slot is dereferenced live by FileSystemSingleton() and handed to GetDataDirectory | lane C2 mint covers it |
| `?_singleton@InternalFactory@Object@@0PEAV12@EA` | 5 | runtime/src/object/internal.cpp:147 | resolve on a LIVE path at runtime/src/object/internal.cpp:147; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) | lane C2 mint covers it |
| `?_singleton@MainLoop@Engine@@1PEAV12@EA` | 9 | runtime/src/engine/engine.cpp:202; standalone/rt_orphan_probe.cpp:969 | resolve on a LIVE path at runtime/src/engine/engine.cpp:202, standalone/rt_orphan_probe.cpp:969; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/engine/engine.cpp:202 -> escape-return at :203  (via `p` in MainLoopSingletonSlot) | lane C2 mint covers it |
| `?_singleton@Modification@IO@@2PEAV12@EA` | 1 | runtime/src/save/saveload.cpp:156 | READ BY HAND. save/saveload.cpp:156 RtDataExport<uint8_t*>(...); dereferenced live in Save::SaveLoad::CheckMod, which then walks IO::Modification at +0x08/+0x10 | lane C2 mint covers it |
| `?_singleton@Persistence@IO@@0PEAV12@EA` | 9 | runtime/src/audio/audio.cpp:488; runtime/src/audio/audio.cpp:531; runtime/src/input/input.cpp:591; runtime/src/input/input.cpp:619; runtime/src/save/saveload.cpp:160; runtime/src/v | resolve on a LIVE path at runtime/src/audio/audio.cpp:488, runtime/src/audio/audio.cpp:531, runtime/src/input/input.cpp:591, runtime/src/input/input.cpp:619, runtime/src/video/device.cpp:135; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/video/device.cpp:135 -> escape-return at :136  (via `p` in PersistenceSingletonSlot) | lane C2 mint covers it |
| `?_singleton@SaveLoad@Save@@0PEAV12@EA` | 9 | runtime/src/save/saveload.cpp:144 | READ BY HAND. save/saveload.cpp:148 RtDataExport<SaveLoad*>(...); the slot is written by SaveLoad_CtorLogic and read by every SaveLoad entry point | lane C2 mint covers it |
| `?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A` | 2 | runtime/src/achievement/goal.cpp:169 | resolve on a LIVE path at runtime/src/achievement/goal.cpp:169; the resolved address IS the datum (vtable stamped into slot 0, or a singleton slot dereferenced) \| DEFERRED-CALL runtime/src/achievement/goal.cpp:169 -> escape-return at :170  (via `p` in GoalInstanceTypeInfo) | lane C2 mint covers it |

### ORACLE-ONLY — 38 rows

| Symbol | Cells | Raw site(s) at HEAD | Evidence | What satisfies it |
|---|---|---|---|---|
| `??0Geometry@Graphics@@QEAA@XZ` | 9 | runtime/src/graphics/scene.cpp:582 | READ BY HAND. Both live sites are PORT-AWARE now: mesh.cpp:224 and animation.cpp:207 are RT_EXPORT_ONCE (Wave 9 lane R repaired the bypass this lane reported). The only remaining raw site is graphics/scene.cpp:582, inside if (rt_diff_on__) | nothing on this door - the port (graphics/scene.cpp:1720) already serves the live callers |
| `??0Serializer@IO@@QEAA@AEAVStream@1@H@Z` | 9 | runtime/src/io/serializer.cpp:438; runtime/src/io/serializer.cpp:609; runtime/src/io/serializer.cpp:611 | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `??1Geometry@Graphics@@UEAA@XZ` | 7 | (none) | READ BY HAND. mesh.cpp:230 is RT_EXPORT_ONCE (port-aware). No unguarded raw use remains | nothing on this door - ported at graphics/scene.cpp:1723 |
| `??1Internal@Object@@UEAA@XZ` | 9 | runtime/src/object/internal.cpp:699; runtime/src/ui/dialog.cpp:897 | VERIFIED BY READING: ui/dialog.cpp:897 is an ARGUMENT to VerifyDtorThunk, whose first statement is `if (!RtDiffEnabled()) return;` (dialog.cpp:401) -- the resolve runs every boot (hence 9/9 cells), the value is used only under BANISHED_RT_DIFF. The second site, object/internal.cpp:699, is inside the shadow wrapper, registered only under BANISHED_RT_INTERNAL_SHADOW (internal.cpp:772, :817) | nothing on THIS door. The symbol is still read live through the PORT-AWARE door at dialog.cpp:731, which is port-registry residue, not raw residue |
| `??1Serializer@IO@@QEAA@XZ` | 9 | runtime/src/io/serializer.cpp:508; runtime/src/io/serializer.cpp:624; runtime/src/io/serializer.cpp:626 | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z` | 9 | runtime/src/math/transform.cpp:475 | every raw site is inside a diff guard: runtime/src/math/transform.cpp:475 [CALL guarded] | nothing - ResolveExportRaw returns null safely with no module handle (resolve_standalone.cpp) and the null is never called |
| `??HSystem@@YA?AVString@0@AEBV10@0@Z` | 1 | runtime/src/system/string.cpp:221 | every raw site is inside a diff guard: runtime/src/system/string.cpp:221 [resolve guarded] | nothing - ResolveExportRaw returns null safely with no module handle (resolve_standalone.cpp) and the null is never called |
| `?FormatParamCount@String@System@@SAHPEBD@Z` | 6 | runtime/src/system/string.cpp:734 | every raw site is inside a diff guard: runtime/src/system/string.cpp:734 [CALL guarded] | nothing - ResolveExportRaw returns null safely with no module handle (resolve_standalone.cpp) and the null is never called |
| `?FormatParamList@String@System@@SA?AV12@AEBV12@PEBV12@@Z` | 6 | runtime/src/system/string.cpp:816 | every raw site is inside a diff guard: runtime/src/system/string.cpp:816 [resolve guarded] | nothing - ResolveExportRaw returns null safely with no module handle (resolve_standalone.cpp) and the null is never called |
| `?GetEncoding@Serializer@IO@@QEAAAEBVEncoding@2@XZ` | 9 | runtime/src/io/serializer.cpp:377; runtime/src/io/serializer.cpp:384; runtime/src/io/serializer.cpp:586; runtime/src/io/serializer.cpp:588 | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?GetIndexFromName@StringTable@UI@@QEBAHAEBVString@System@@@Z` | 7 | runtime/src/ui/sprite.cpp:615 | every raw site is inside a diff guard: runtime/src/ui/sprite.cpp:615 [resolve guarded] | nothing - ResolveExportRaw returns null safely with no module handle (resolve_standalone.cpp) and the null is never called |
| `?GetSprite@ImageElement@UI@@QEBA?AVSprite@2@XZ` | 5 | runtime/src/ui/widgets_layout.cpp:298 | every raw site is inside a diff guard: runtime/src/ui/widgets_layout.cpp:298 [CALL guarded] | nothing - ResolveExportRaw returns null safely with no module handle (resolve_standalone.cpp) and the null is never called |
| `?GetVersion@Serializer@IO@@QEBAHXZ` | 9 | runtime/src/io/serializer.cpp:356; runtime/src/io/serializer.cpp:363; runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?IsDerivedFrom@Base@Object@@QEBA_NI@Z` | 1 | runtime/src/object/type_system.cpp:242 | every raw site is inside a diff guard: runtime/src/object/type_system.cpp:242 [CALL guarded] | nothing - ResolveExportRaw returns null safely with no module handle (resolve_standalone.cpp) and the null is never called |
| `?IsEffectPlaying@Scene@Audio@@QEBA_NI@Z` | 7 | runtime/src/audio/audio.cpp:350; runtime/src/audio/audio.cpp:409; runtime/src/audio/audio.cpp:810; runtime/src/audio/audio.cpp:840; runtime/src/audio/audio.cpp:1045 | VERIFIED BY READING: audio.cpp:840 and :1045 resolve into a LOCAL outside the guard (that is what the residue recorded), but every call through it is inside RT_DIFF_SCALAR (:350), if (rt_diff_on__) (:409) or an RT_DIFF_SHADOW argument (:811, :840, :1045) | nothing - ResolveExportRaw returns null safely with no module handle and the null is never called |
| `?IsPaused@Scene@Audio@@QEBA_NH@Z` | 7 | runtime/src/audio/audio.cpp:931 | every raw site is inside a diff guard: runtime/src/audio/audio.cpp:931 [CALL guarded] | nothing - ResolveExportRaw returns null safely with no module handle (resolve_standalone.cpp) and the null is never called |
| `?SecondsToTicks@Timer@System@@SA_JM@Z` | 9 | runtime/src/engine/engine.cpp:375; runtime/src/engine/loops.cpp:840; runtime/src/system/timer.cpp:219 | every raw site is inside a diff guard: runtime/src/engine/engine.cpp:375 [resolve guarded], runtime/src/engine/loops.cpp:840 [resolve guarded], runtime/src/system/timer.cpp:219 [CALL guarded] | nothing - ResolveExportRaw returns null safely with no module handle (resolve_standalone.cpp) and the null is never called |
| `?Serialize@Serializer@IO@@QEAAXAEACPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXAEAEPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXAEAFPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXAEAGPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXAEAHPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXAEAIPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXAEAMPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXHPEACPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXHPEADPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXHPEAEPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXHPEAFPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXHPEAGPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXHPEAHPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXHPEAIPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXHPEAMPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Serialize@Serializer@IO@@QEAAXHPEA_NPEBD@Z` | 9 | runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?SerializeBytes@Serializer@IO@@QEAAXHPEAXPEBD@Z` | 9 | runtime/src/io/serializer.cpp:331; runtime/src/io/serializer.cpp:564 (kForwarders loop) | READ BY HAND. The raw resolve is the registration-time structural check: serializer.cpp:564 walks kForwarders through ResolveExportRaw and decodes each shipped forwarder tail-jump slot; :586/:609/:624 byte-compare the three non-jump bodies. CUTOVER-SAFE SINCE 2026-09-07: a null resolve now takes the `if (!fn) { ++g_verifyUnresolved; }` branch (serializer.cpp:565) and VerifyBody returns kUnresolved (:174), so the port REGISTERS anyway (:654-659) instead of setting g_portOn=false. The `!g_portOn` fallbacks (:234, :250, :331, :356, :377, :438, :508) are now null-checked (`if (orig) { orig(...); return; }`), so the null is never called. Lane V reported the PRE-REPAIR version of this as the wave's most dangerous row; the repair is in the tree and the file's own comment at :636-646 records it | nothing further on this door. The port registers with the DLL absent and the bodies dispatch through the ReaderWriter vtable slot, which is our own path |
| `?Set@Persistence@IO@@QEAAXPEBD0H@Z` | 9 | runtime/src/video/device.cpp:650 | READ BY HAND. audio.cpp:492 is RT_EXPORT_ONCE (port-aware). The only raw site, video/device.cpp:650, is inside the `if (rt_diff_on__)` poison block opened at device.cpp:641 -- rawdoor_audit agrees: CALL guarded | nothing on this door |
| `?TicksToSeconds@Timer@System@@SAM_J@Z` | 9 | runtime/src/engine/loops.cpp:842; runtime/src/system/timer.cpp:233 | every raw site is inside a diff guard: runtime/src/engine/loops.cpp:842 [resolve guarded], runtime/src/system/timer.cpp:233 [CALL guarded] | nothing - ResolveExportRaw returns null safely with no module handle (resolve_standalone.cpp) and the null is never called |
| `?ToInteger@String@System@@SAHPEBD@Z` | 2 | runtime/src/system/string.cpp:929 | every raw site is inside a diff guard: runtime/src/system/string.cpp:929 [CALL guarded] | nothing - ResolveExportRaw returns null safely with no module handle (resolve_standalone.cpp) and the null is never called |

### DEAD — 11 rows

| Symbol | Cells | Raw site(s) at HEAD | Evidence | What satisfies it |
|---|---|---|---|---|
| `??1External@Object@@UEAA@XZ` | 8 | runtime/src/graphics/animation.cpp:135 | PROVEN BY SWEEP + READ. The one remaining hit, animation.cpp:135, is the DEFINITION of the named constant kExternalDtor; its single use (animation.cpp:278) is RT_EXPORT_ONCE, the port-aware door. No raw-door USE of this symbol remains in code/ runtime/ engine/ standalone/ dx11/ | already satisfied - ported at object/external.cpp:680 |
| `??4String@System@@QEAAAEAV01@AEBV01@@Z` | 9 | (none) | NO raw-door site remains in the tree at HEAD (literal sweep of code/ runtime/ engine/ standalone/ dx11/, paired by enclosing door). Now reached through the PORT-AWARE door: runtime/src/ui/widgets_layout.cpp:109, engine/funcs/engine_funcs.cpp:2610, standalone/address_registry.cpp:1263. Registered port: runtime/src/system/string.cpp:1102 | already satisfied by the registered port |
| `??YString@System@@QEAA?AV01@PEBD@Z` | 1 | (none) | NO raw-door site remains in the tree at HEAD (literal sweep of code/ runtime/ engine/ standalone/ dx11/, paired by enclosing door). Now reached through the PORT-AWARE door: runtime/src/system/value_types.cpp:419, engine/funcs/engine_funcs.cpp:2875. Registered port: runtime/src/system/string.cpp:1107 | already satisfied by the registered port |
| `?AdjustDelimiters@Filename@System@@AEAAXXZ` | 1 | (none) | NO raw-door site remains in the tree at HEAD (literal sweep of code/ runtime/ engine/ standalone/ dx11/, paired by enclosing door). Now reached through the PORT-AWARE door: runtime/src/engine/loops.cpp:177, runtime/src/save/saveload.cpp:587, engine/funcs/engine_funcs.cpp:1697. Registered port: runtime/src/system/filename.cpp:446 | already satisfied by the registered port |
| `?Format@String@System@@SA?AV12@PEBDZZ` | 9 | (none) | PROVEN BY READ. The one site my pairing bucketed as raw, value_types.cpp:325, is RT_EXPORT_ONCE -- the port-aware door. The RAW token my window matcher saw belongs to the RtOriginal two lines above it, on DateAsString. No raw-door use of String::Format remains | already satisfied - String::Format is ported and reached through ResolveExport |
| `?Release@String@System@@AEAAXXZ` | 9 | (none) | NO raw-door site remains in the tree at HEAD (literal sweep of code/ runtime/ engine/ standalone/ dx11/, paired by enclosing door). Now reached through the PORT-AWARE door: runtime/src/system/value_types.cpp:338, runtime/src/system/value_types.cpp:366, runtime/src/system/value_types.cpp:426. Registered port: runtime/src/system/string.cpp:1113 | already satisfied by the registered port |
| `?RemoveChildren@Element@UI@@QEAAXXZ` | 1 | (none) | NO raw-door site remains in the tree at HEAD (literal sweep of code/ runtime/ engine/ standalone/ dx11/, paired by enclosing door). Now reached through the PORT-AWARE door: engine/funcs/engine_funcs.cpp:1820. Registered port: runtime/src/ui/widgets_layout.cpp:847 | already satisfied by the registered port |
| `?Set@String@System@@QEAAXPEBD@Z` | 9 | (none) | NO raw-door site remains in the tree at HEAD (literal sweep of code/ runtime/ engine/ standalone/ dx11/, paired by enclosing door). Now reached through the PORT-AWARE door: runtime/src/system/value_types.cpp:355, engine/funcs/engine_funcs.cpp:1639. Registered port: runtime/src/system/string.cpp:1108 | already satisfied by the registered port |
| `?SetValue@NumberEditElement@UI@@QEAAXH@Z` | 1 | (none) | NO raw-door site remains in the tree at HEAD (literal sweep of code/ runtime/ engine/ standalone/ dx11/, paired by enclosing door). Now reached through the PORT-AWARE door: runtime/src/ui/widgets_input.cpp:159, engine/funcs/engine_funcs.cpp:1885. Registered port: runtime/src/ui/widgets_input.cpp:670 | already satisfied by the registered port |
| `?ThreadFunc@Thread@System@@CAIPEAX@Z` | 7 | (none) | NO raw-door site remains in the tree at HEAD (literal sweep of code/ runtime/ engine/ standalone/ dx11/, paired by enclosing door). Now reached through the PORT-AWARE door: runtime/src/system/thread_win.cpp:143. Registered port: runtime/src/system/thread_win.cpp:391 | already satisfied by the registered port |
| `?_secondsPerTick@Timer@System@@0NA` | 9 | (none) | PROVEN BY SWEEP + READ. No code site of any kind remains: engine/engine.cpp:221 and save/saveload.cpp:168 now call the PORTED RtSystem::TimerSecondsPerTick(), which owns the storage; saveload.cpp:163-165 names this lane's report as the reason. The only remaining occurrence in the tree is the mint table entry | already satisfied twice over - a port owns the value, and rt_data_mint_tables.inc:1019 carries it as well |
---

## 8. Reproducing this table, and what it does not prove

The join is four commands and a sweep:

```
tools/rawdoor_audit.py --all        # 550 sites: CALL/resolve x guarded/LIVE
tools/rawdoor_audit.py --deferred   # 74 sites: follow a stored pointer to its use
tools/ghidra_mcp/.venv/Scripts/python.exe tools/port_bypass_audit.py   # 0 bypasses, both doors
grep <symbol> standalone/rt_data_mint_tables.inc                       # minted?
```

plus the literal-and-alias sweep described in §1. `port_bypass_audit.py` needs the project
venv — with the system Python it aborts rather than answering degraded, which is the right
behaviour and the reason its number can be trusted.

**What this does not prove.**

* **Residue is not reachability.** The 104 are what nine harness runs *asked for*. A REAL
  DEPENDENCY here executes on a path the harness reached; it is not a ranking, and porting is
  not the only answer — `?UpdateList`/`?UpdateLoad`/`?UpdateLoadFromFile` and the
  screenshot/autosave paths lane 0 named are stub-first candidates.
* **The population is a floor.** Nine runs of one harness. Lane 0's own list of unreached
  paths (screenshot, autosave, achievement award) applies unchanged, and lane V2's source
  sweep is the instrument for those, not this one.
* **This is a snapshot of a moving tree.** Round 1's verdicts were correct when written and
  four of them are now obsolete *because they were acted on*. Re-run the four commands above
  before trusting a line number; do not re-derive a verdict from this file's prose alone.
* **`DEAD` means the raw door is closed, not that the symbol is unused.** Ten of the eleven
  are served by a registered port through the port-aware door; that door has its own residue
  list (`wave9_residue_union.txt`) and its own gate row.
