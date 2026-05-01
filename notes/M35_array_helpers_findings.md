# M3.5 — Array helpers findings

This note documents discoveries while reimplementing the 10 helpers used by
`MenuScene::Slot5` (`MenuScene::Serialize`) for the standalone exe build.

## Helpers landed (10 / 10)

| # | Exe addr     | Reimpl symbol                                  | Cascade |
|---|--------------|------------------------------------------------|---------|
| 1 | 0x1400b00e0  | `Reimpl_Geometry_Init`                         | none    |
| 2 | 0x1400988c0  | `Reimpl_Transform_SerializeWrapper`            | none    |
| 3 | 0x14001baf0  | `Reimpl_Array_Resize_Stride20`                 | none    |
| 4 | 0x140001bb0  | `Reimpl_Array_Resize_Stride08`                 | none    |
| 5 | 0x14001bf10  | `Reimpl_Array_Resize_Stride14`                 | none    |
| 6 | 0x14001bbf0  | `Reimpl_Array_Serialize_Stride20`              | none    |
| 7 | 0x14001bcc0  | `Reimpl_Array_Serialize_External_GraphicsMesh` | CRITICAL |
| 8 | 0x14001bff0  | `Reimpl_Array_Serialize_External_Particle`     | CRITICAL |
| 9 | 0x14001b8e0  | `Reimpl_ExternalPtr_Serialize_GraphicsMesh`    | CRITICAL |
|10 | 0x1400196a0  | `Reimpl_ExternalPtr_Serialize_Weather`         | CRITICAL |

All ten registered via `RegisterExeAddr` in
`standalone/address_registry.cpp` ('M3.5' block, in `RegisterCodeFunctions`).

## Shared bodies (per `feedback_share_reimpl_code.md`)

- **Helpers #7 + #8** are byte-identical except for the source of the typeId:
  - #7 reads `*(uint*)*(BaseTypeInfo**)0x140151f20` (GraphicsMesh)
  - #8 reads `*(uint*)*(BaseTypeInfo**)0x140151420` (Particle)
  Both share `ArraySerializeExternalGeneric` in `serialize_arrays.cpp`. The
  per-helper wrapper just resolves the right typeIdPtr.

- **Helpers #9 + #10** are byte-identical except for the typeId source:
  - #9 reads `*(uint*)*(BaseTypeInfo**)0x140151f20` (GraphicsMesh — same slot as #7)
  - #10 reads `*(uint*)0x1401c6198` (Weather — direct dword, no double-deref)
  Both share `ExternalPtrSerializeGeneric` in `serialize_externals.cpp`.

## param_4 mystery (recon question 2)

**Decompiler noise — not a real parameter.** Both `FUN_14001bcc0` and
`FUN_14001bff0` show `undefined8 param_4` threaded through, but:

1. The caller (`MenuScene::Slot5` at 0x1400a3779 / 0x1400a3928) loads only
   `RCX` (serializer), `RDX` (count), `R8` (array base) before the CALL.
   **R9 is never set.**
2. Inside the function, `param_4` is assigned to `local_res20` (which is
   immediately overwritten with `GetReferenceId`'s return) and never read
   afterwards.

So R9 is leftover from prior code, picked up by Ghidra heuristics. Our
reimpl signature drops the parameter entirely (`void serializer, int count,
void* arrayBase`).

This **is not** a bug-class-6 implicit-register-forwarding case — there's no
sub-call that reads R9 either.

## TypeInfo plumbing — three flavors

The original exe uses three different storage shapes for type IDs in the
serialize helpers, all of which we route through:

1. **Pointer-to-BaseTypeInfo (Runtime DLL)** — slots 0x140151f20 (GraphicsMesh)
   and 0x140151420 (Particle). Read with two derefs:
   `g_ppTypeInfo_X = (uint32_t**)slot; typeId = **g_ppTypeInfo_X`.
   These slots are filled at startup by the Runtime DLL's reflection plumbing.

2. **Direct dword (exe-resident BaseTypeInfo)** — `0x1401c6198`. Read with
   one deref: `typeId = *g_pTypeId_Weather`. Patched at runtime by
   `Weather_RegisterType` (factory_registrations.csv:165).

3. **Cascade-safe fallback** — when either of the above is null/zero, we
   skip the `IsDerivedFrom` check and load without type validation. A
   one-shot `printf` warning is emitted. This avoids the NullFactory crash
   pattern from `feedback_factory_cascade_strategy.md`.

## Factory cascade types discovered

The cascade-prone helpers (#7-#10) call
`Object::ExternalFactory::FindOrLoad(Filename)` with paths derived from the
serialized bytestream. The types instantiated through this cascade are:

- **`Graphics::GraphicsMesh`** — used by helpers #7 (array of mesh refs at
  +0x40) and #9 (single ref at +0x70). Mesh resources live under
  `WinData/Mesh/...rsc` and use the `GraphicsMesh` type.
  - Runtime DLL self-registers — no exe-side `REGISTER_TYPE` needed.
  - typeId source: `?_typeInfo@GraphicsMesh@Graphics@@0VBaseTypeInfo@Object@@A`
    (Runtime DLL global), exposed via .rdata pointer slot 0x140151f20.

- **`Graphics::Particle`** — used by helper #8 (array of particle refs at
  +0x60). Particle effects live under `WinData/Particle/...rsc` (e.g.,
  `Tornado.rsc` per factory_registrations.csv:163, which lists
  `Toolbar_StaticInit` registering `"Tornado"` of base `"Graphics::Particle"`).
  - Runtime DLL self-registers — no exe-side `REGISTER_TYPE` needed.
  - typeId source: `?_typeInfo@Particle@Graphics@@0VBaseTypeInfo@Object@@A`
    (Runtime DLL global), exposed via .rdata pointer slot 0x140151420.

- **`Weather`** — used by helper #10 (single ref at +0x80). Weather is
  exe-defined (factory_registrations.csv:165, base `Object::External`).
  - Already a `REGISTER_TYPE` candidate covered by the Weather row in
    `factory_registrations.csv` — should auto-register at standalone startup
    once `Weather_RegisterType` is hooked into `RegisterAllExeTypes`.

### Recommended REGISTER_TYPE additions

None new for **this** task. The cascade types break down as:

- **Runtime-resident:** `GraphicsMesh`, `Particle` — these self-register
  inside Runtime DLL CRT-init (per `project_runtime_type_self_registration.md`).
  Standalone needs **no** action; the .rdata slots will be populated when
  Runtime loads.
- **Exe-resident:** `Weather` — already in factory_registrations.csv.
  Wire it up via the existing factory codegen (no change needed here).

If the cascade still fails at runtime, the next step is to verify that:
(a) Runtime DLL loads BEFORE `EngineFuncsInit` reads slots 0x140151f20 /
    0x140151420 — if not, the deref returns zero typeIds and our reimpl
    falls into the "skip validation" path safely.
(b) `Weather_RegisterType` runs during standalone startup (it's row 165 in
    factory_registrations.csv), so 0x1401c6198 contains a non-zero typeId
    by the time helper #10 fires.

## Surprises / blockers

- **None blocking.** All 10 helpers landed with cascade-safe behaviour.
- Helper #6 (`Array_Serialize_Stride20`) bundles two short writes into a
  single uint32 serialize call as a TODO — the dedicated short-serialize
  primitive isn't wired in `engine_funcs.h`. If the menu scene .rsc payload
  has non-zero values in the trailing 4 bytes per element, this could
  mis-encode them. Worth a watch in any A/B trace. Falls through to original
  trampoline whenever `fn_Array_Serialize_Stride20` is non-null (DLL mode +
  any standalone mode where the original code function pointer resolves).
- The `serialize_arrays.cpp` `GenericArrayResize` assumes `MemAlloc` returns
  zero-initialized memory (matches MSVC heap behaviour); the original
  exe explicitly zeros only when `oldData != *dataSlot` AND `slot >= oldCount`.
  Our reimpl matches this exactly.

## Bug class 6 (implicit register forwarding) — not triggered

Verified by re-reading the parent disassembly (`MenuScene::Slot5` at
0x1400a3779 and 0x1400a3928): only RCX/RDX/R8 are loaded before the calls
to `FUN_14001bcc0` / `FUN_14001bff0`. R9 is dead. Bug class 6 does not
apply here, despite Ghidra's misleading `param_4` decoration.
