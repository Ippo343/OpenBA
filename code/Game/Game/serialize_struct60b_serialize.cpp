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
#include "graphics/Material.h"   // ENDGAME B.3.9 TI-REST: Graphics::MaterialInstance::TypeId()
#include "system/Filename.h"
#include "system/String.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "io/Serializer.h"     // ENDGAME B.3.4 caller batch B
#include "Game/exe_bodies.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

// Reimpl_ExternalPtr_Serialize_Typed_7140 (serialize_externals_typed.cpp, 0x140007140)
// serializes each element's typed-ExternalPtr at +0x00: Game/exe_bodies.h.

namespace {

constexpr size_t kStruct60B_Size = 0x60;

// ============================================================
// IO::Serializer primitives
// ============================================================
// The original ffa0 / f9c0 call IO::Serializer through PE import slots
// (0x140152ac8 Serialize(int&), 0x140152410 Serialize(uchar&), 0x140152a58
// Serialize(bool&), 0x140152a60 Serialize(float&), 0x140152a48 GetVersion,
// 0x140152488 Serialize(count, ushort*), 0x1401523f8 / 0x140152400 Serialize(size,
// short* / uchar*)). The calls are IO::Serializer members (ENDGAME B.3.4); the
// slot resolves that only gated them went with B3R (every slot is registered).

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

    auto* hdr = reinterpret_cast<uint8_t*>(arrayHeaderPtr);
    int32_t count = *reinterpret_cast<int32_t*>(hdr + 0x08);

    // Serialize the count (acts as both count-out and count-in based on mode).
    int32_t local_count = count;
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(local_count, nullptr);   // B.3.4 bridge

    // Loading branch: resize the buffer to local_count.
    int innerCheck = *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
    if (innerCheck == 0) {
        // sub_ee40 (0x14001ee40) = stride-0x14 array resize. Reuse existing
        // session-5 wrapper which targets the same primitive shape.
        Reimpl_Array_Resize_Stride14(arrayHeaderPtr, local_count);
    }

    if (local_count <= 0) return;

    void* dataPtr = *reinterpret_cast<void**>(hdr + 0x00);
    if (!dataPtr) return;

    // Walk stride-0x14 elements. The original disassembly indexes from base+0x10
    // and uses RBX-0x10/-0x8/+0x00 for each subfield, but the absolute offsets
    // within the element are the same as base+0x00/+0x08/+0x10.
    auto* elemBase = reinterpret_cast<uint8_t*>(dataPtr);
    for (int i = 0; i < local_count; ++i) {
        uint8_t* elem = elemBase + static_cast<size_t>(i) * 0x14;
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(4, reinterpret_cast<short*>(elem + 0x00), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(4, reinterpret_cast<short*>(elem + 0x08), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(4, reinterpret_cast<unsigned char*>(elem + 0x10), nullptr);   // B.3.4 bridge
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

    auto* base = reinterpret_cast<uint8_t*>(arrayBase);
    for (int i = 0; i < count; ++i) {
        uint8_t* elem = base + static_cast<size_t>(i) * kStruct60B_Size;

        // 1. Typed ExternalPtr (Graphics::MaterialInstance) at +0x00.
        Reimpl_ExternalPtr_Serialize_Typed_7140(elem + 0x00, serializer);

        // 2. 5 ints at +0x28..+0x38.
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(elem + 0x28), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(elem + 0x2c), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(elem + 0x30), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(elem + 0x34), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(elem + 0x38), nullptr);   // B.3.4 bridge

        // 3. 1 byte at +0x3c.
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned char*>(elem + 0x3c), nullptr);   // B.3.4 bridge

        // 4. 1 int at +0x40.
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(elem + 0x40), nullptr);   // B.3.4 bridge

        // 5. 1 bool at +0x44.
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(elem + 0x44), nullptr);   // B.3.4 bridge

        // 6. 5 floats at +0x48..+0x58.
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(elem + 0x48), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(elem + 0x4c), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(elem + 0x50), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(elem + 0x54), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(elem + 0x58), nullptr);   // B.3.4 bridge

        // 7. 1 bool at +0x5c.
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(elem + 0x5c), nullptr);   // B.3.4 bridge

        // 8. Version-gated bool at +0x5d (versions >= 2).
        int version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge
        if (version >= 2) {
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(elem + 0x5d), nullptr);   // B.3.4 bridge
        }

        // 9. Version-gated sub-array work (versions >= 1).
        version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge
        if (version >= 1) {
            // Sub-array at elem+0x08 (stride-0x14): per-element raw serialize.
            Struct60B_Sub_F9C0_Body(elem + 0x08, serializer);

            // Sub-array at elem+0x18 (stride-0x02 uint16 raw bytes).
            // The original reads local_count from *(int*)(elem + 0x20)
            // (= the count slot of the +0x18 ArrayHeader16).
            int32_t local_count = *reinterpret_cast<int32_t*>(elem + 0x20);
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(local_count, nullptr);   // B.3.4 bridge
            // If loading, resize the +0x18 sub-array to local_count.
            int innerCheck = *reinterpret_cast<int*>(
                reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
            if (innerCheck == 0) {
                Reimpl_Array_Resize_Stride02(elem + 0x18, local_count);
            }
            // Bulk uint16 serialize of the data buffer.
            uint16_t* data16 = *reinterpret_cast<uint16_t**>(elem + 0x18);
            if (data16) {
                reinterpret_cast<IO::Serializer*>(serializer)->Serialize(local_count, reinterpret_cast<unsigned short*>(data16), nullptr);   // B.3.4 bridge
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
    if (!::Object::ExternalFactory::_singleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Array::Serialize_External<MaterialInstance>] runtime primitives "
                   "unwired — skipping %d elements (cascade-safe no-op)\n", count);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    void* factory = ::Object::ExternalFactory::_singleton;
    // ENDGAME B.3.3 step 6A: member calls on the real Object::ExternalFactory.
    // The singleton is still read through the EXISTING door, so the guards
    // above still gate the same cell and the same registered body still runs.
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);   // B.3.3 bridge

    // MaterialInstance's type id (retail: the BaseTypeInfo* in import slot
    // 0x140151350; ENDGAME B.3.9 TI-REST: the class accessor -- the slot and
    // the by-name fallback are gone). Zero means not yet registered — skip
    // IsDerivedFrom validation.
    uint32_t typeId = Graphics::MaterialInstance::TypeId();
    bool typeIdResolved = (typeId != 0);
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
    IO::Encoding* encoding = const_cast<IO::Encoding*>(&reinterpret_cast<IO::Serializer*>(serializer)->GetEncoding());   // B.3.4 bridge
    if (!encoding) return;
    // Encoding vtable +0x10 is slot 2 == IO::Encoding::GetKind() (B34_RULES §1.1).
    int encState = encoding->GetKind();   // B.3.4 bridge  (+0x10 = Encoding::GetKind)

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
                reinterpret_cast<IO::Serializer*>(serializer)->Serialize(refId, nullptr);   // B.3.4 bridge
                if (refId != 0xFFFFFFFFu) {
                    if (serializerInner == 0) {
                        System::Filename tmpFn = efactory->GetPathForReferenceId(refId);
                        fnameBuf = tmpFn;
                    }
                    needsPathSerialize = true;
                }
            } else {
                reinterpret_cast<IO::Serializer*>(serializer)->Serialize(refId, nullptr);   // B.3.4 bridge
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
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(hasRef, nullptr);   // B.3.4 bridge
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
