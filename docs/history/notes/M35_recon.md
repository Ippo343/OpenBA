# M3.5 — Serialize helper recon

## Exe-internal helpers

### FUN_1400b00e0 (Geometry ctor/init)
- **Signature:** `Geometry* FUN_1400b00e0(Geometry* param_1)`
- **Size:** ~50 instructions (ctor + field init)
- **Touches:** Calls base class Graphics::Geometry::Geometry ctor, initializes vtable pointer (PTR_TradeWork_impl_ecc0), zeros all fields from +0xc0 to +0x228, constructs embedded VertexBuffer/IndexBuffer objects at specific offsets, sets field at +0x30 to 1.
- **Cascade:** NONE — simple ctor, no factory/type lookup calls.
- **Recommendation:** RegisterExeAddr (trampoline)

### TradeWork_impl_04d0 (Geometry::Serialize)
- **Signature:** (Need explicit decompile; described in menuscene.cpp context as calling helpers below for each field)
- **Size:** uncertain (decompile failed at this address variant)
- **Touches:** Calls FUN_1400988c0 (Transform::Serialize), FUN_14001baf0/FUN_14001bbf0 (array 0x30 resize/serialize), FUN_14001bcc0 (array 0x40 serialize), FUN_14001b8e0 (ExternalPtr 0x70), FUN_1400196a0 (ExternalPtr 0x80), FUN_14001bf10/FUN_14001bff0 (arrays 0x50/0x60).
- **Cascade:** CRITICAL — Contains ExternalPtr serialize (FUN_14001bcc0, FUN_14001bff0) which call `ExternalFactory::FindOrLoad` and `IsDerivedFrom` on loaded externals. These can instantiate new types if references are null during load.
- **Recommendation:** reimpl-clean (decompile in full, reimplement line-by-line with engine_funcs wiring)

### TradeWork_impl_f1a0 (version-gated extra-serialize)
- **Signature:** (decompile failed at address variant)
- **Size:** uncertain
- **Touches:** Conditionally serialize field +0x78 (float, version >= 2 branch) and Field +0x80 (Weather exref, later serialize point).
- **Cascade:** MEDIUM — Calls FUN_1400196a0 (Weather exref serialize) which uses IsDerivedFrom with TypeInfo_Weather global.
- **Recommendation:** RegisterExeAddr (small version-gated block, likely < 20 instructions)

### FUN_1400988c0 (Transform::Serialize)
- **Signature:** `void FUN_1400988c0(Transform* param_1, Stream* param_2)`
- **Size:** ~15 instructions
- **Touches:** Creates Serializer on param_2, calls Math::Transform::Serialize on param_1, Math::Vector3::Serialize at +0x1c, IO::Serializer::Serialize on three floats at +0x28, +0x2c, +0x30.
- **Cascade:** NONE — All callees are Runtime DLL (Math/IO namespace), no type instantiation.
- **Recommendation:** RegisterExeAddr (lightweight wrapper/adapter)

### FUN_14001baf0 (array resize for +0x30)
- **Signature:** `void FUN_14001baf0(longlong* param_1, int param_2)` (reallocation helper)
- **Size:** ~40 instructions
- **Touches:** Reallocates array at param_1, element size = 0x20 (32 bytes per element, memcopy stride = 0x20), count/capacity tracking.
- **Cascade:** NONE — Pure memory management, no object instantiation.
- **Recommendation:** RegisterExeAddr (direct trampoline)

### FUN_14001bbf0 (array serialize for +0x30)
- **Signature:** `void FUN_14001bbf0(Serializer* param_1, uint param_2, longlong param_3)` (count, array base)
- **Size:** ~25 instructions (loop)
- **Touches:** Loops param_2 times, serializes 6 floats + 1 short + 1 short per iteration (0x20 / 8 byte offset += 8). Each element is 0x20 bytes (5 floats + 1 int, or similar packed layout).
- **Cascade:** NONE — IO::Serializer::Serialize calls only.
- **Recommendation:** RegisterExeAddr (direct loop trampoline)

### FUN_140001bb0 (array resize for +0x40 and +0x60)
- **Signature:** `void FUN_140001bb0(longlong* param_1, int param_2)` (reallocation helper)
- **Size:** ~35 instructions
- **Touches:** Reallocates array at param_1, element size = 0x08 (8-byte stride, 8-byte alignment). Single memcopy loop copies qwords.
- **Cascade:** NONE — Pure memory allocation/copy.
- **Recommendation:** RegisterExeAddr (direct trampoline)

### FUN_14001bcc0 (array serialize for +0x40)
- **Signature:** `void FUN_14001bcc0(Serializer* param_1, int param_2, longlong param_3, undefined8 param_4)` (count, array base, + extra param)
- **Size:** ~250 instructions (complex conditional for reference modes)
- **Touches:** Loops param_2 times; per-element: checks Serializer encoding state (IsLoading vs IsSaving), calls `ExternalFactory::FindOrLoad`, `GetReferenceId`, `GetPathForReferenceId`, `IsDerivedFrom(pEVar7, *_typeInfo_exref)`, conditionally deserializes External* and validates type. param_4 is threaded through (origin: type context variable passed from caller).
- **Cascade:** CRITICAL — Direct calls to `ExternalFactory::FindOrLoad` + `IsDerivedFrom` on every element during deserialize. Creates new External instances; if type check fails, can return nullptr to caller, leaving pointers invalid.
- **Recommendation:** reimpl-clean (deserialize per-element with engine_funcs wiring for type safety)

### FUN_14001b8e0 (ExternalPtr::Serialize for +0x70, GraphicsMesh exref)
- **Signature:** `void FUN_14001b8e0(longlong* param_1, Serializer* param_2)` (reference field, serializer)
- **Size:** ~120 instructions
- **Touches:** Checks Serializer encoding (IsLoading vs IsSaving), manages Filename temp objects, calls `ExternalFactory::FindOrLoad`, `ExternalFactory::GetReferenceId`, `ExternalFactory::IsReferenceModeEnabled`, `IsDerivedFrom(pEVar6, *_typeInfo_exref)`. On deserialize: loads External from factory, validates type, assigns back to *param_1.
- **Cascade:** CRITICAL — Same pattern as FUN_14001bcc0: FindOrLoad + IsDerivedFrom. Can instantiate types and fail type checks. typeInfo is global _typeInfo_exref (for GraphicsMesh).
- **Recommendation:** reimpl-clean (implement with engine_funcs type safety)

### FUN_1400196a0 (ExternalPtr::Serialize for +0x80, Weather exref)
- **Signature:** `void FUN_1400196a0(longlong* param_1, Serializer* param_2)` (reference field, serializer)
- **Size:** ~120 instructions (nearly identical to FUN_14001b8e0)
- **Touches:** Identical structure to FUN_14001b8e0, but **compares against TypeInfo_Weather** instead of _typeInfo_exref. Handles reference mode and path-based deserialization the same way.
- **Cascade:** CRITICAL — Same FindOrLoad + IsDerivedFrom risk, but validates against Weather type (different type constant).
- **Recommendation:** reimpl-clean (sister function, verify type constant used and reimplement with engine_funcs)

### FUN_14001bf10 (array resize for +0x50)
- **Signature:** `void FUN_14001bf10(longlong* param_1, int param_2)` (reallocation helper)
- **Size:** ~40 instructions
- **Touches:** Reallocates array at param_1, element size = 0x14 (20 bytes = 5 * sizeof(float)). Multiplier in malloc is `count << 2 + count` = `count * 5`. Stride on memcopy is 0x14. ✓ Confirmed 5-float elements.
- **Cascade:** NONE — Pure memory management.
- **Recommendation:** RegisterExeAddr (direct trampoline)

### FUN_14001bff0 (array serialize for +0x60)
- **Signature:** `void FUN_14001bff0(Serializer* param_1, int param_2, longlong param_3, undefined8 param_4)` (count, array base, extra param)
- **Size:** ~250 instructions
- **Touches:** Nearly identical to FUN_14001bcc0 (same ~250-instruction body, same reference-mode branching). Loops param_2 times; per-element calls `FindOrLoad`, `IsDerivedFrom`, `GetReferenceId`. param_4 threaded through (same origin pattern as FUN_14001bcc0).
- **Cascade:** CRITICAL — Same as FUN_14001bcc0. FindOrLoad + IsDerivedFrom on array of External pointers.
- **Recommendation:** reimpl-clean (or factor out common logic if FUN_14001bcc0 already done)

---

## Runtime DLL exports

Verified existence in Runtime-x64.dll:

- **IO::Serializer::GetEncoding:** `?GetEncoding@Serializer@IO@@QEAAAEBVEncoding@2@XZ` @ 0x180006290
  - Returns const reference to Encoding from serializer's internal state.

- **IO::Stream::GetEncoding:** `?GetEncoding@Stream@IO@@QEBAAEBVEncoding@2@XZ` @ 0x1800b9a60
  - Returns const reference to Encoding from stream at offset +0x10.

- **Math::Vector4::Serialize, Math::Vector3::Serialize, Math::Transform::Serialize, System::Color::Serialize, System::String::Serialize:** (Search pending; assumed to exist as Runtime symbols based on menuscene decomp references; no direct address obtained yet in this scan.)

- **Encoding vtable slot 0 (IsLoading/IsSaving):** Resolved dynamically as `(*(code **)(*(longlong *)pEVar4 + 0x10))(pEVar4)` where pEVar4 is the Encoding*. Returns int (3 = IsLoading state, 0/1 = IsSaving state). Exact export name/ordinal uncertain; likely a pure-virtual method on Encoding vtable.

---

## Open questions / surprises

1. **FUN_14001bcc0 vs FUN_14001bff0 duplication:** Nearly identical large functions (both ~250 instructions, same FindOrLoad/IsDerivedFrom pattern). Confirm whether they are truly duplicated or if one is a wrapper. If identical, can reimplement once and reuse.

2. **param_4 in FUN_14001bcc0 and FUN_14001bff0:** Decompiler shows it as `undefined8 param_4` threaded through both functions with no visible mutation. Origin from caller (TradeWork_impl_04d0 or version-gated serialize) unclear. Likely a type-context variable; inspect disasm to confirm it maps to a specific register or calling-convention slot.

3. **TradeWork_impl_f1a0 address:** Original task lists 0x1400b0f1a0 (0x140 + 0xb0f1a0); decompile attempts failed. Confirm correct address or whether this is version-gated code folded into Geometry::Serialize (0x1400b04d0 variant).

4. **Encoding::IsLoading vtable resolution:** The code calls `(*(code **)(*(longlong *)pEVar5 + 0x10))(pEVar5)` directly; no mangled name found. Likely a pure virt method on Encoding. Confirm slot 0x10 offset and export ordinal for ResolveExport wiring.

5. **Type globals (_typeInfo_exref, TypeInfo_Weather):** Both appear to be exe-resident globals accessed at runtime. Confirm addresses and whether they need RegisterExeAddr or if they're resolved differently.
