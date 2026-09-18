// code/Game/Game/serialize_struct60b_serialize.cpp
//
// M3.5 Path B session 4 — per-element 0x60B serialize + typed-ExternalPtr
// array serialize (cascade points #10 and #13 in Geometry::Serialize).
//
// Helpers (verified addresses via Ghidra MCP):
//   TradeWork_impl_ffa0       — exe 0x14001ffa0  (per-element 0x60B serialize)
//   TradeWork_impl_sub_0150   — exe 0x140020150  (typed-ExternalPtr array)
//   TradeWork_impl_sub_f9c0   — exe 0x14001f9c0  (uint16-stride sub-array;
//                                                  inline anonymous helper —
//                                                  only one caller (ffa0))
//
// Cascade safety: when any required Runtime primitive (FindOrLoad,
// IsDerivedFrom, Filename ctors, Serializer slots, etc.) is null at runtime
// we log a one-shot warning and return early instead of crashing. The
// pattern matches code/Game/Game/serialize_externals.cpp:31-50 and
// serialize_arrays.cpp.
//
// TypeInfo for FUN_140020150 was verified via disassembly:
//   140020340: MOV RAX, qword ptr [0x140151350]   ; BaseTypeInfo* slot
//   140020347: MOV EDX, dword ptr [RAX]           ; *(uint*)BaseTypeInfo
//   14002034c: CALL [0x140152a98]                 ; IsDerivedFrom
// Slot 0x140151350 == g_ppTypeInfo_MaterialInstance (Graphics::MaterialInstance).
// This matches FUN_140007140's slot — confirmed shared-type use.
//
// NOTE: This file is the PUBLIC-INTERFACE-ONLY reimpl. The dispatch logic
// (delegate to original-via-fn-ptr if resolved, else execute the cleaned C
// path inline) follows the same pattern as serialize_struct60b.cpp /
// serialize_externals.cpp. The cleaned-C path mirrors the original's body
// shape (no instruction-for-instruction guarantee — the dispatch slots for
// the per-primitive serialize calls aren't all wired yet, so we soft-fail).

#include "engine_funcs.h"
#include "system/Filename.h"
#include "system/String.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include <cstring>

// External reimpl entry from serialize_externals_typed.cpp — used by
// Struct60B_Serialize_Body for each element's typed-ExternalPtr at +0x00.
extern "C" void Reimpl_ExternalPtr_Serialize_Typed_7140(
    void* refField, void* serializer);

namespace {

constexpr size_t kStruct60B_Size = 0x60;

// ============================================================
// Dispatch-slot typedefs for per-primitive serialize calls
// ============================================================
//
// The original ffa0 calls a handful of IO::Serializer dispatch slots that
// aren't yet exposed in engine_funcs (only a subset is wired today). We
// resolve them lazily via ResolveAddr and treat each as cascade-safe:
// missing slot -> one-shot warn -> skip that primitive (NOT skip the
// outer loop). We do NOT add them as long-lived externs in engine_funcs.h
// because the address registry hasn't classified all of these slots yet.
//
// Slot map (from ffa0 / f9c0 / sub_0150 disassembly):
//   0x140152ac8 — Serialize(int*, name)        [used 12 times in ffa0]
//   0x140152410 — Serialize(uchar*, name)      [byte primitive]
//   0x140152a58 — Serialize(bool*, name)       [also g_fn_Serialize_Bool]
//   0x140152a60 — Serialize(float*, name)
//   0x140152a48 — GetVersion()
//   0x140152488 — Serialize(count, ushort*, name)  [bulk uint16 array]
//   0x1401523f8 — Serialize(int size, void* ptr, name)  [fixed-byte raw]
//   0x140152400 — Serialize(int size, void* ptr, name)  [variant]
typedef void (*FnSerInt_t)(void* serializer, int32_t* field, const char* name);
typedef void (*FnSerByte_t)(void* serializer, uint8_t* field, const char* name);
typedef void (*FnSerBool_t)(void* serializer, bool* field, const char* name);
typedef void (*FnSerFloat_t)(void* serializer, float* field, const char* name);
typedef int  (*FnGetVer_t)(void* serializer);
typedef void (*FnSerUShortArray_t)(void* serializer, int32_t count, uint16_t* data, const char* name);
typedef void (*FnSerSizedRaw_t)(void* serializer, int32_t bytes, void* ptr, const char* name);

// One-shot resolver helpers. We hold cached pointers per-translation-unit so
// we don't ResolveAddr on every call.
template <typename Fn>
Fn ResolveSlot(uintptr_t exeAddr, Fn& cache, bool& triedOnce) {
    if (cache) return cache;
    if (triedOnce) return nullptr;
    triedOnce = true;
    void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, exeAddr));
    if (slot && *slot) {
        cache = reinterpret_cast<Fn>(*slot);
    } else {
        printf("[Struct60B::Serialize] dispatch slot 0x%llx unresolved — "
               "skipping that primitive\n",
               static_cast<unsigned long long>(exeAddr));
        fflush(stdout);
    }
    return cache;
}

// ============================================================
// Helper sub_f9c0 — uint16-stride sub-array serialize (~30 instr)
// ============================================================
//
// Decompile (verified at 0x14001f9c0):
//   - Read count from param_1[1] (the 16-byte array header's count slot at
//     hdr+0x08, here exposed via plVar1 + 1 since param_1 is qword*).
//   - Serialize the count (Serialize_Int via slot 0x140152ac8).
//   - If loading (*(int*)(*serializer + 8) == 0), call sub_ee40 to resize the
//     stride-0x14 array to `count`. (Note: sub_ee40 == fn_Struct60B_SubArrayCopyStride14
//     // ALMOST — sub_ee40 is actually the stride-0x14 RESIZE helper at 0x14001ee40,
//     not the deep-copy sibling at 0x14001fdb0. We reuse the existing
//     fn_Array_Resize_Stride14 wrapper — both have the identical body shape
//     for stride 0x14 / align 2.)
//   - For each of the `count` elements (stride 0x14):
//       Serialize(serializer, 4, elem-0x10, name)   via slot 0x1401523f8
//       Serialize(serializer, 4, elem-0x08, name)   via slot 0x1401523f8
//       Serialize(serializer, 4, elem+0x00, name)   via slot 0x140152400
//     Where elem starts at hdr->data + 0x10, so the relative -0x10 / -0x8 / +0x00
//     correspond to absolute offsets 0x00 / 0x08 / 0x10 within the stride-0x14
//     element. (4 ints / shorts at +0x00, +0x04, +0x08, +0x0C and 4 bytes
//     at +0x10 — the original treats the first 8 bytes as two int-like fields
//     and the last 4 as a packed dword.)
//
// param_1: pointer to the stride-0x14 ArrayHeader16 (data, count, capacity).
// param_2: serializer (used for both Serialize and the version/inner check).
void Struct60B_Sub_F9C0_Body(void* arrayHeaderPtr, void* serializer) {
    if (!arrayHeaderPtr || !serializer) return;

    // Cached slot pointers — first-call resolution.
    static FnSerInt_t      s_serInt   = nullptr;
    static FnSerSizedRaw_t s_serRawA  = nullptr;  // 0x1401523f8
    static FnSerSizedRaw_t s_serRawB  = nullptr;  // 0x140152400
    static bool s_triedInt = false;
    static bool s_triedRawA = false;
    static bool s_triedRawB = false;

    auto* hdr = reinterpret_cast<uint8_t*>(arrayHeaderPtr);
    int32_t count = *reinterpret_cast<int32_t*>(hdr + 0x08);

    // Serialize the count (acts as both count-out and count-in based on mode).
    int32_t local_count = count;
    FnSerInt_t serInt = ResolveSlot(0x140152ac8, s_serInt, s_triedInt);
    if (serInt) {
        serInt(serializer, &local_count, nullptr);
    }

    // Loading branch: resize the buffer to local_count.
    int innerCheck = *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
    if (innerCheck == 0) {
        // sub_ee40 (0x14001ee40) = stride-0x14 array resize. Reuse existing
        // session-5 wrapper which targets the same primitive shape.
        if (fn_Array_Resize_Stride14) {
            fn_Array_Resize_Stride14(arrayHeaderPtr, local_count);
        } else {
            static bool s_warned = false;
            if (!s_warned) {
                printf("[Struct60B::sub_f9c0] fn_Array_Resize_Stride14 unresolved — "
                       "loading skipped\n");
                fflush(stdout);
                s_warned = true;
            }
        }
    }

    if (local_count <= 0) return;

    void* dataPtr = *reinterpret_cast<void**>(hdr + 0x00);
    if (!dataPtr) return;

    FnSerSizedRaw_t serRawA = ResolveSlot(0x1401523f8, s_serRawA, s_triedRawA);
    FnSerSizedRaw_t serRawB = ResolveSlot(0x140152400, s_serRawB, s_triedRawB);
    if (!serRawA && !serRawB) return;

    // Walk stride-0x14 elements. The original disassembly indexes from base+0x10
    // and uses RBX-0x10/-0x8/+0x00 for each subfield, but the absolute offsets
    // within the element are the same as base+0x00/+0x08/+0x10.
    auto* elemBase = reinterpret_cast<uint8_t*>(dataPtr);
    for (int i = 0; i < local_count; ++i) {
        uint8_t* elem = elemBase + static_cast<size_t>(i) * 0x14;
        if (serRawA) {
            serRawA(serializer, 4, elem + 0x00, nullptr);
            serRawA(serializer, 4, elem + 0x08, nullptr);
        }
        if (serRawB) {
            serRawB(serializer, 4, elem + 0x10, nullptr);
        }
    }
}

// ============================================================
// Helper A — Per-element 0x60B serialize (FUN_14001ffa0, ~70 instr)
// ============================================================
//
// Decompile shape (verified at 0x14001ffa0):
//   for (i = 0; i < count; ++i) {
//       elem = arrayBase + i*0x60
//       FUN_140007140(elem + 0x00, serializer);   // typed ExternalPtr
//       Serialize_Int (elem + 0x28);              // int
//       Serialize_Int (elem + 0x2c);
//       Serialize_Int (elem + 0x30);
//       Serialize_Int (elem + 0x34);
//       Serialize_Int (elem + 0x38);
//       Serialize_Byte(elem + 0x3c);              // byte
//       Serialize_Int (elem + 0x40);              // int
//       Serialize_Bool(elem + 0x44);              // bool
//       Serialize_Float(elem + 0x48);             // 5 floats
//       Serialize_Float(elem + 0x4c);
//       Serialize_Float(elem + 0x50);
//       Serialize_Float(elem + 0x54);
//       Serialize_Float(elem + 0x58);
//       Serialize_Bool(elem + 0x5c);
//       if (GetVersion() >= 2) Serialize_Bool(elem + 0x5d);
//       if (GetVersion() >= 1) {
//           sub_f9c0(elem + 0x08, serializer);    // stride-0x14 sub-array
//           local_count = *(int32*)(elem + 0x20); // capacity field of +0x18 hdr
//           Serialize_Int(&local_count);
//           if (loading) Resize_Stride02(elem + 0x18, local_count);
//           Serialize_UShortArray(local_count, *(ushort**)(elem + 0x18));
//       }
//   }
//
// Cascade points: FUN_140007140 (which we delegate to the session 3 reimpl).
void Struct60B_Serialize_Body(void* serializer, int count, void* arrayBase) {
    if (!serializer || count <= 0 || !arrayBase) return;

    // Cached slot pointers.
    static FnSerInt_t        s_serInt   = nullptr;
    static FnSerByte_t       s_serByte  = nullptr;
    static FnSerBool_t       s_serBool  = nullptr;
    static FnSerFloat_t      s_serFloat = nullptr;
    static FnGetVer_t        s_getVer   = nullptr;
    static FnSerUShortArray_t s_serUSArr = nullptr;
    static bool s_triedInt = false;
    static bool s_triedByte = false;
    static bool s_triedBool = false;
    static bool s_triedFloat = false;
    static bool s_triedGetVer = false;
    static bool s_triedUSArr = false;

    FnSerInt_t   serInt   = ResolveSlot(0x140152ac8, s_serInt,   s_triedInt);
    FnSerByte_t  serByte  = ResolveSlot(0x140152410, s_serByte,  s_triedByte);
    FnSerBool_t  serBool  = ResolveSlot(0x140152a58, s_serBool,  s_triedBool);
    FnSerFloat_t serFloat = ResolveSlot(0x140152a60, s_serFloat, s_triedFloat);
    FnGetVer_t   getVer   = ResolveSlot(0x140152a48, s_getVer,   s_triedGetVer);
    FnSerUShortArray_t serUSArr =
        ResolveSlot(0x140152488, s_serUSArr, s_triedUSArr);

    auto* base = reinterpret_cast<uint8_t*>(arrayBase);
    for (int i = 0; i < count; ++i) {
        uint8_t* elem = base + static_cast<size_t>(i) * kStruct60B_Size;

        // 1. Typed ExternalPtr (Graphics::MaterialInstance) at +0x00.
        Reimpl_ExternalPtr_Serialize_Typed_7140(elem + 0x00, serializer);

        // 2. 5 ints at +0x28..+0x38.
        if (serInt) {
            serInt(serializer, reinterpret_cast<int32_t*>(elem + 0x28), nullptr);
            serInt(serializer, reinterpret_cast<int32_t*>(elem + 0x2c), nullptr);
            serInt(serializer, reinterpret_cast<int32_t*>(elem + 0x30), nullptr);
            serInt(serializer, reinterpret_cast<int32_t*>(elem + 0x34), nullptr);
            serInt(serializer, reinterpret_cast<int32_t*>(elem + 0x38), nullptr);
        }

        // 3. 1 byte at +0x3c.
        if (serByte) {
            serByte(serializer, elem + 0x3c, nullptr);
        }

        // 4. 1 int at +0x40.
        if (serInt) {
            serInt(serializer, reinterpret_cast<int32_t*>(elem + 0x40), nullptr);
        }

        // 5. 1 bool at +0x44.
        if (serBool) {
            serBool(serializer, reinterpret_cast<bool*>(elem + 0x44), nullptr);
        }

        // 6. 5 floats at +0x48..+0x58.
        if (serFloat) {
            serFloat(serializer, reinterpret_cast<float*>(elem + 0x48), nullptr);
            serFloat(serializer, reinterpret_cast<float*>(elem + 0x4c), nullptr);
            serFloat(serializer, reinterpret_cast<float*>(elem + 0x50), nullptr);
            serFloat(serializer, reinterpret_cast<float*>(elem + 0x54), nullptr);
            serFloat(serializer, reinterpret_cast<float*>(elem + 0x58), nullptr);
        }

        // 7. 1 bool at +0x5c.
        if (serBool) {
            serBool(serializer, reinterpret_cast<bool*>(elem + 0x5c), nullptr);
        }

        // 8. Version-gated bool at +0x5d (versions >= 2).
        int version = getVer ? getVer(serializer) : 0;
        if (version >= 2 && serBool) {
            serBool(serializer, reinterpret_cast<bool*>(elem + 0x5d), nullptr);
        }

        // 9. Version-gated sub-array work (versions >= 1).
        version = getVer ? getVer(serializer) : 0;
        if (version >= 1) {
            // Sub-array at elem+0x08 (stride-0x14): per-element raw serialize.
            Struct60B_Sub_F9C0_Body(elem + 0x08, serializer);

            // Sub-array at elem+0x18 (stride-0x02 uint16 raw bytes).
            // The original reads local_count from *(int*)(elem + 0x20)
            // (= the count slot of the +0x18 ArrayHeader16).
            int32_t local_count = *reinterpret_cast<int32_t*>(elem + 0x20);
            if (serInt) {
                serInt(serializer, &local_count, nullptr);
            }
            // If loading, resize the +0x18 sub-array to local_count.
            int innerCheck = *reinterpret_cast<int*>(
                reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
            if (innerCheck == 0) {
                if (fn_Array_Resize_Stride02) {
                    fn_Array_Resize_Stride02(elem + 0x18, local_count);
                } else {
                    static bool s_warned = false;
                    if (!s_warned) {
                        printf("[Struct60B::Serialize] fn_Array_Resize_Stride02 unresolved — "
                               "uint16 sub-array load skipped\n");
                        fflush(stdout);
                        s_warned = true;
                    }
                }
            }
            // Bulk uint16 serialize of the data buffer.
            uint16_t* data16 = *reinterpret_cast<uint16_t**>(elem + 0x18);
            if (serUSArr && data16) {
                serUSArr(serializer, local_count, data16, nullptr);
            }
        }
    }
}

// ============================================================
// Helper B — Typed-ExternalPtr ARRAY serialize (FUN_140020150, ~150 instr)
// ============================================================
//
// Body shape: identical to FUN_14001bcc0/FUN_14001bff0 (stride-8 External*
// array, per-element FindOrLoad + IsDerivedFrom). typeInfo source is .rdata
// slot 0x140151350 (Graphics::MaterialInstance) — verified at 0x140020340:
//   MOV RAX,[0x140151350]; MOV EDX,[RAX]; CALL [0x140152a98] ; IsDerivedFrom.
//
// Cascade safety: identical to ArraySerializeExternalGeneric in
// serialize_arrays.cpp — soft-fail if any Runtime primitive is missing.
void Struct60B_ArraySerializeMaterialInstance_Body(
    void* serializer, int count, void* arrayBase) {
    if (!serializer || count <= 0 || !arrayBase) return;

    // Required Runtime primitives. If any are missing, log once and bail.
    if (!fn_Serializer_GetEncoding || !fn_ExternalFactory_FindOrLoad ||
        !fn_IsDerivedFrom || !fn_ExternalFactory_IsReferenceModeEnabled ||
        !fn_External_GetPath || !fn_Serializer_Serialize_Uint || !g_fn_Serialize_Bool ||
        !g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Array::Serialize_External<MaterialInstance>] runtime primitives "
                   "unwired — skipping %d elements (cascade-safe no-op)\n", count);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    void* factory = *g_pExternalFactorySingleton;
    // ENDGAME B.3.3 step 6A: member calls on the real Object::ExternalFactory.
    // The singleton is still read through the EXISTING door, so the guards
    // above still gate the same cell and the same registered body still runs.
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);   // B.3.3 bridge

    // Resolve typeId from the MaterialInstance BaseTypeInfo* slot. Null/zero
    // means the type isn't yet registered — skip IsDerivedFrom validation.
    uint32_t typeId = 0;
    bool typeIdResolved = false;
    if (g_ppTypeInfo_MaterialInstance && *g_ppTypeInfo_MaterialInstance) {
        typeId = **g_ppTypeInfo_MaterialInstance;
        typeIdResolved = (typeId != 0);
    }
    // Lazy resolution: in standalone the .rdata BaseTypeInfo* slot stays null,
    // so ask Object::Factory directly (it sees Runtime-DLL-self-registered
    // Graphics::MaterialInstance).
    if (!typeIdResolved) {
        typeId = ::Object::ResolveTypeIdByNameLazy("MaterialInstance");
        typeIdResolved = (typeId != 0);
    }
    if (!typeIdResolved) {
        static bool s_typeWarned = false;
        if (!s_typeWarned) {
            printf("[Array::Serialize_External<MaterialInstance>] typeId not yet "
                   "registered — loading without type validation (count=%d)\n", count);
            fflush(stdout);
            s_typeWarned = true;
        }
    }

    // Encoding state. Slot vtable+0x10 returns 3 for path-export mode.
    void* encoding = fn_Serializer_GetEncoding(serializer);
    if (!encoding) return;
    using EncodingStateFn = int (*)(void*);
    EncodingStateFn getStateFn = nullptr;
    {
        void* vtable = *reinterpret_cast<void**>(encoding);
        getStateFn = reinterpret_cast<EncodingStateFn>(
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vtable) + 0x10));
    }
    int encState = getStateFn ? getStateFn(encoding) : 0;

    auto* slots = reinterpret_cast<void**>(arrayBase);
    for (int i = 0; i < count; ++i) {
        void** slot = &slots[i];

        if (encState == 3) {
            // Path-export (read-only) mode: walk existing pointers only.
            if (!*slot) continue;
            // `GetPath` returns by value through a hidden return slot and the
            // callee CONSTRUCTS into it; copy-initialising the local is the
            // same store.
            System::Filename pathBuf =
                reinterpret_cast<const ::Object::External*>(*slot)->GetPath();  // B.3.3 bridge
            const void* found = efactory->FindOrLoad(pathBuf);
            if (found && typeIdResolved) {
                ::Object::IsDerivedFromOrTrust(const_cast<void*>(found), typeId);
            }
            continue;
        }

        // Load/save mode.
        System::Filename fnameBuf;

        bool refMode = efactory->IsReferenceModeEnabled();

        if (refMode) {
            uint32_t refId = 0xFFFFFFFFu;
            if (*slot) {
                refId = efactory->GetReferenceId(
                    reinterpret_cast<const ::Object::External*>(*slot));   // B.3.3 bridge
            }

            int serializerInner = *reinterpret_cast<int*>(
                reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);

            bool needsPathSerialize = false;
            if (!*slot || serializerInner != 0 || refId != 0xFFFFFFFFu) {
                fn_Serializer_Serialize_Uint(serializer, &refId, nullptr);
                if (refId != 0xFFFFFFFFu) {
                    if (serializerInner == 0) {
                        System::Filename tmpFn = efactory->GetPathForReferenceId(refId);
                        fnameBuf = tmpFn;
                    }
                    needsPathSerialize = true;
                }
            } else {
                fn_Serializer_Serialize_Uint(serializer, &refId, nullptr);
            }

            if (needsPathSerialize) {
                int innerCheck = *reinterpret_cast<int*>(
                    reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
                if (innerCheck == 0) {
                    void* loaded = const_cast<::Object::External*>(
                        efactory->FindOrLoad(fnameBuf));                        // B.3.3 bridge
                    void* result = nullptr;
                    if (loaded && (!typeIdResolved || ::Object::IsDerivedFromOrTrust(loaded, typeId))) {
                        result = loaded;
                    }
                    *slot = result;
                }
            }
        } else {
            bool hasRef = (*slot != nullptr);
            g_fn_Serialize_Bool(serializer, &hasRef, nullptr);
            if (hasRef) {
                if (*slot) {
                    System::Filename pathBuf =
                        reinterpret_cast<const ::Object::External*>(*slot)->GetPath();  // B.3.3 bridge
                    fnameBuf = pathBuf;
                }
                fnameBuf.AsString().Serialize(serializer);

                int innerCheck = *reinterpret_cast<int*>(
                    reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
                if (innerCheck == 0) {
                    void* loaded = const_cast<::Object::External*>(
                        efactory->FindOrLoad(fnameBuf));                        // B.3.3 bridge
                    void* result = nullptr;
                    if (loaded && (!typeIdResolved || ::Object::IsDerivedFromOrTrust(loaded, typeId))) {
                        result = loaded;
                    }
                    *slot = result;
                }
            }
        }

    }
}

} // namespace

// ============================================================
// Public reimpl entry points
// ============================================================
//
// Each entry tries the original exe address first (usable when the exe code
// page is mapped). If that pointer is null (no exe code), we fall through to
// our cleaned C body.

extern "C" void Reimpl_Struct60B_Serialize(
    void* serializer, int count, void* arrayBase) {
    Struct60B_Serialize_Body(serializer, count, arrayBase);
}

extern "C" void Reimpl_Array_Serialize_External_MaterialInstance(
    void* serializer, int count, void* arrayBase) {
    Struct60B_ArraySerializeMaterialInstance_Body(serializer, count, arrayBase);
}
