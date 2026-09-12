// ============================================================================
// runtime/src/ui/clipregiondescription.cpp — UI::ClipRegionDescription, whole
// ============================================================================
//
// ??_7ClipRegionDescription@UI@@6B@ (retail 0x180156350), ten slots:
//   0  deleting dtor      0x180037eb0  runtime/src/object/deleting_dtor.cpp row
//   1  TypeName           0x180037990  here
//   2  Type               0x180037970  here
//   3  GetFileVersion     0x1800cc0c0  here  (returns 0x104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800cc010  here
//   6  Reconstruct        0x180037c30  here
//   7  Destruct           0x180037d90  here
//   8  CreateInstance     0x1800cbec0  here
//   9  Construct          0x180037ce0  here
// plus ??1ClipRegionDescription@UI@@UEAA@XZ (0x1800cc0d0).
//
// ----------------------------------------------------------------------------
// THE CONSTRUCTOR IS INLINED AND THERE IS NO OUT-OF-LINE ??0 — like
// GroupDescription, unlike the rest of this batch.
// ----------------------------------------------------------------------------
// `Construct` (0x180037ce0), `Reconstruct` (0x180037c30) and both factory steps
// each open-code the same store sequence; the DLL exports no
// `??0ClipRegionDescription` and the kClipRegionDescription row in
// runtime/src/ui/factory_ui1_tables.inc has a null ctor for that reason.
// `ConstructBody` below is that sequence written once, transcribed from
// 0x180037cfa..0x180037d78. Two details Ghidra's C loses:
//   * `+0x40` is written as ONE QWORD holding 3 (`MOV qword [RCX+0x40],3` at
//     0x180037d44), covering both dwords there;
//   * `+0x38` and `+0x3c` are two SEPARATE DWORDS (0 and -1).
// The three dead base-vtable stamps (Base, then External over it unread, then
// ElementDescription) are reproduced.
//
// Layout: runtime/types/ui/DescriptionsBatch1.h. sizeof 0x78. Both ints start
// at 0x64 — 100, the default clip extent.
// ============================================================================
#include "description_common.h"
#include "../../types/ui/DescriptionsBatch1.h"

namespace {

using namespace rtdesc;

void* VtClipRegionDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblClipRegionDescription); }
void* VtClipRegionElement()     { return RT_DATA_SLOT(void*, RtUI::kVtblClipRegionElement); }
const uint8_t* ClipRegionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ClipRegionDescription@UI@@0VBaseTypeInfo@Object@@A");
}

void ConstructBody(uint8_t* p) {
    void** v = reinterpret_cast<void**>(p);

    *v = VtBase();                                          // 180037d01
    *v = VtExternal();                                      // 180037d0b
    // `LEA RAX,[RCX+8]` then `MOV [RAX],RDX` — retail writes +0x08 through an
    // intermediate pointer (the SEH frame's visible temporary) and +0x10/+0x18
    // directly. Byte-identical; the temporary costs nothing to drop.
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;             // 180037d19
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;             // 180037d1c
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;             // 180037d20
    *v = VtElementDescription();                            // 180037d2b
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;             // 180037d2e
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;             // 180037d32
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;             // 180037d36
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;             // 180037d3a  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;            // 180037d3d  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;             // 180037d44  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;             // 180037d4c
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;             // 180037d50
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;             // 180037d54
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;             // 180037d58
    *v = VtClipRegionDescription();                         // 180037d63
    *reinterpret_cast<int32_t*>(p + 0x68)  = 0x64;          // 180037d66  DWORD
    *reinterpret_cast<int32_t*>(p + 0x6c)  = 0x64;          // 180037d6d  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;             // 180037d74
}

}  // namespace

extern "C" uint32_t RtUi_ClipRegionDescription_Type(const void*) {
    const uint8_t* ti = ClipRegionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_ClipRegionDescription_TypeName(const void*) {
    const uint8_t* ti = ClipRegionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_ClipRegionDescription_GetFileVersion(const void*) { return 0x104; }

extern "C" void* RtUi_ClipRegionDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                   // 180037cf4
    ConstructBody(static_cast<uint8_t*>(selfv));
    return selfv;                                           // 180037d78
}
extern "C" void* RtUi_ClipRegionDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                    // 180037c47
    ConstructBody(static_cast<uint8_t*>(selfv));
    return selfv;                                           // 180037ccc
}
extern "C" void RtUi_ClipRegionDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1ClipRegionDescription@UI@@UEAA@XZ   0x1800cc0d0
// ============================================================================
extern "C" void RtUi_ClipRegionDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtClipRegionDescription();                         // 1800cc0ed
    *v = VtElementDescription();                            // 1800cc0f7
    ReleaseStringField(p, 0x60);                            // 1800cc0fa

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800cc12c JMP
}

// ============================================================================
// ?Serialize@ClipRegionDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800cc010
// ============================================================================
extern "C" bool RtUi_ClipRegionDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    ElementDescriptionSerialize(p, stream);                 // 1800cc02a

    Serializer ser = { stream, nullptr };
    void* rw = MakeReaderWriter(stream, &ser);

    RwScalar(rw, kRwScalarInt, p + 0x68);                   // 1800cc06e
    RwScalar(rw, kRwScalarInt, p + 0x6c);                   // 1800cc083

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0x70, &ser);     // 1800cc092

    DestroyReaderWriter(rw);                                // 1800cc0aa
    return true;
}

// ============================================================================
// ?CreateInstance@ClipRegionDescription@UI@@...   retail 0x1800cbec0
// ============================================================================
// RCX = this, RDX = the hidden return slot, R8 = Dialog*.
//
// ClipRegionElement is 0xd8 and its constructor is INLINED here, with THREE
// stores whose widths are not uniform and one of which is UNALIGNED:
//     1800cbf42  MOV dword [RDI+0xc0],0x64
//     1800cbf4c  MOV qword [RDI+0xc4],0x64     <- UNALIGNED, covers +0xc4/+0xc8
//     1800cbf59  MOV qword [RDI+0xcc],0        <- UNALIGNED, covers +0xcc/+0xd0
//     1800cbf60  MOV dword [RDI+0xd4],0
// A field-by-field port with a header could not have expressed the two
// overlapping qwords; they are written here as memcpy at the exact offsets.
//
// THE NULL-ALLOCATION ARM CLEARS ONLY THE LOW 32 BITS (`XOR EAX,EAX ;
// MOV EDI,EAX` at 0x1800cbf68) — which zeroes all 64 on x64, so it is the same
// null. Noted because the asymmetry with the other members of this batch
// (`MOV RBX,R12`) is real in the bytes and is not a difference in behaviour.
extern "C" void** RtUi_ClipRegionDescription_CreateInstance(const void* selfv,
                                                            void** ret,
                                                            const void* dialog) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    void* raw = MemAlloc(0xd8, 8);                          // 1800cbf02

    uint8_t* elem = nullptr;
    if (raw != nullptr) {                                   // 1800cbf2d
        ElementCtor(raw);                                   // 1800cbf32
        uint8_t* e = static_cast<uint8_t*>(raw);
        *reinterpret_cast<void**>(e) = VtClipRegionElement();        // 1800cbf3f
        *reinterpret_cast<int32_t*>(e + 0xc0) = 0x64;                // 1800cbf42
        const uint64_t k64 = 0x64, kZero = 0;
        __builtin_memcpy(e + 0xc4, &k64, 8);                         // 1800cbf4c
        __builtin_memcpy(e + 0xcc, &kZero, 8);                       // 1800cbf59
        *reinterpret_cast<int32_t*>(e + 0xd4) = 0;                   // 1800cbf60
        elem = e;
    }

    void* slot = elem;                                      // 1800cbf6c
    ElementDescriptionApply(self, &slot);                   // 1800cbf79

    // The two extents are copied AFTER the base applier, so they win over
    // anything it wrote.
    *reinterpret_cast<int32_t*>(elem + 0xc0) =
        *reinterpret_cast<const int32_t*>(self + 0x68);     // 1800cbf81
    *reinterpret_cast<int32_t*>(elem + 0xc4) =
        *reinterpret_cast<const int32_t*>(self + 0x6c);     // 1800cbf8a

    void** made = DialogCreateElement(dialog, &slot,
                                      *reinterpret_cast<void* const*>(self + 0x70)); // 1800cbf9c
    AttachAndPushChild(elem, made);                         // 1800cbfa9 .. 1800cbfe4

    *ret = elem;                                            // 1800cbfe6
    return ret;
}

int RtUiClipRegionDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@ClipRegionDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_ClipRegionDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180037970ull, (void*)&RtUi_ClipRegionDescription_Type);

    RegisterRuntimeExport("?TypeName@ClipRegionDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ClipRegionDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180037990ull, (void*)&RtUi_ClipRegionDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@ClipRegionDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_ClipRegionDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800cc0c0ull, (void*)&RtUi_ClipRegionDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@ClipRegionDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ClipRegionDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180037ce0ull, (void*)&RtUi_ClipRegionDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@ClipRegionDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ClipRegionDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180037c30ull, (void*)&RtUi_ClipRegionDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@ClipRegionDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_ClipRegionDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180037d90ull, (void*)&RtUi_ClipRegionDescription_Destruct);

    RegisterRuntimeExport("??1ClipRegionDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_ClipRegionDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800cc0d0ull, (void*)&RtUi_ClipRegionDescription_Dtor);

    RegisterRuntimeExport("?Serialize@ClipRegionDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_ClipRegionDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800cc010ull, (void*)&RtUi_ClipRegionDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@ClipRegionDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_ClipRegionDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800cbec0ull, (void*)&RtUi_ClipRegionDescription_CreateInstance);

    return n;
}
