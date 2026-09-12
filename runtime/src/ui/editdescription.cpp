// ============================================================================
// runtime/src/ui/editdescription.cpp — UI::EditDescription, the whole class
// ============================================================================
//
// ??_7EditDescription@UI@@6B@ (retail 0x180156880), ten slots:
//   0  deleting dtor      0x18003fa20  runtime/src/object/deleting_dtor.cpp row
//   1  TypeName           0x18003f6d0  here
//   2  Type               0x18003f6b0  here
//   3  GetFileVersion     0x1800d11e0  here  (returns 0x204)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d1110  here
//   6  Reconstruct        0x18003f850  here
//   7  Destruct           0x18003f880  here
//   8  CreateInstance     0x1800d0f90  here
//   9  Construct          0x18003f870  here
// plus ??1EditDescription@UI@@UEAA@XZ (0x1800d11f0).
//
// The constructor is OUT OF LINE (`??0EditDescription@UI@@QEAA@XZ`,
// 0x1800d12c0) and is ALREADY ported by runtime/src/ui/factory_ui1.cpp.
//
// THIS CLASS HAS NO SEPARATE APPLIER. Its CreateInstance is the only one in the
// batch that open-codes TWO refcounted String ASSIGNMENTS (not releases) — the
// increment side of the protocol, which no other member of this batch does.
//
// Layout: runtime/types/ui/DescriptionsBatch1.h. sizeof 0x88.
// ============================================================================
#include "description_common.h"
#include "../../types/ui/DescriptionsBatch1.h"

namespace {

using namespace rtdesc;

void* VtEditDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblEditDescription); }
const uint8_t* EditTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@EditDescription@UI@@0VBaseTypeInfo@Object@@A");
}

void* EditCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return RT_EXPORT_ONCE(CtorFn, "??0EditDescription@UI@@QEAA@XZ")(self);
}

// The refcounted System::String ASSIGNMENT retail open-codes twice in
// CreateInstance (0x1800d102a and 0x1800d1082). Not a release: the old value is
// released with the usual asymmetry, the new one is stored and its refcount
// INCREMENTED — and the whole thing is skipped when the two pointers are
// already equal (`CMP RAX,[RDI+0x70] ; JZ`), which is what makes a self-assign
// not drop the last reference.
void AssignStringField(uint8_t* dst, size_t dstOff, const uint8_t* src, size_t srcOff) {
    char*& d = *reinterpret_cast<char**>(dst + dstOff);
    char* const s = *reinterpret_cast<char* const*>(src + srcOff);
    if (d == s) return;                                      // 1800d1035
    if (d != nullptr) {                                      // 1800d103a
        --*reinterpret_cast<int32_t*>(d - 4);
        if (*reinterpret_cast<int32_t*>(d - 4) == 0) {
            d = d - 8;                                       // 1800d1045
            MemFree(d);
        } else {
            d = nullptr;                                     // 1800d105b
        }
    }
    d = s;                                                   // 1800d106a
    if (d != nullptr) ++*reinterpret_cast<int32_t*>(d - 4);  // 1800d1076
}

}  // namespace

extern "C" uint32_t RtUi_EditDescription_Type(const void*) {
    const uint8_t* ti = EditTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_EditDescription_TypeName(const void*) {
    const uint8_t* ti = EditTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_EditDescription_GetFileVersion(const void*) { return 0x204; }

extern "C" void* RtUi_EditDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                    // 18003f873
    return EditCtor(selfv);                                  // 18003f879 JMP
}
extern "C" void* RtUi_EditDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                     // 18003f85e
    return EditCtor(selfv);                                  // 18003f868 JMP
}
extern "C" void RtUi_EditDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1EditDescription@UI@@UEAA@XZ   0x1800d11f0
// ============================================================================
// TWO String releases (+0x80 then +0x70) under this class's own vtable, then
// the base vtable and the base's _textName. The +0x68 external is NOT released.
extern "C" void RtUi_EditDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtEditDescription();                                // 1800d120d
    ReleaseStringField(p, 0x80);                             // 1800d1210
    ReleaseStringField(p, 0x70);                             // 1800d1246
    *v = VtElementDescription();                             // 1800d1277
    ReleaseStringField(p, 0x60);                             // 1800d127a

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800d12ac JMP
}

// ============================================================================
// ?Serialize@EditDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d1110
// ============================================================================
// +0x68 goes through 0x18003f1e0, a DIFFERENT ExternalPtr serializer from the
// two this batch otherwise uses (0x180038f90 for ElementDescription,
// 0x1800042a0 for StringTable, 0x180004590 for SpriteSheet) — which is how its
// pointee type is distinguished. Taken from the call, not guessed.
extern "C" bool RtUi_EditDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    ElementDescriptionSerialize(p, stream);                  // 1800d112a

    Serializer ser = { stream, nullptr };
    void* rw = MakeReaderWriter(stream, &ser);

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x18003f1e0ull)(p + 0x68, &ser);      // 1800d1168
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x70, &ser);      // 1800d1176

    RwScalar(rw, kRwScalarInt,  p + 0x78);                   // 1800d118a
    RwScalar(rw, kRwScalarBool, p + 0x7c);                   // 1800d119f

    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x80, &ser);      // 1800d11ae

    DestroyReaderWriter(rw);                                 // 1800d11c6
    return true;
}

// ============================================================================
// ?CreateInstance@EditDescription@UI@@...   retail 0x1800d0f90
// ============================================================================
// RCX = this, RDX = the hidden return slot. R8 is never read — no children.
//
// EditElement is 0xf8 and its constructor is OUT OF LINE
// (`??0EditElement@UI@@QEAA@XZ`, 0x1800d0f10, already ported by
// runtime/src/ui/factory_ui1.cpp); retail takes its RETURN value as the element.
//
// The field copies are a QWORD, two refcounted String assignments, a DWORD and
// a BYTE — and the BYTE copy is LAST, after both strings (0x1800d10d7).
extern "C" void** RtUi_EditDescription_CreateInstance(const void* selfv, void** ret) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    void* raw = MemAlloc(0xf8, 8);                           // 1800d0fd1

    uint8_t* elem = nullptr;
    if (raw != nullptr) {                                    // 1800d0ffc
        using EditElementCtorFn = void* (*)(void*);
        elem = static_cast<uint8_t*>(
            RtAddr<EditElementCtorFn>(0x1800d0f10ull)(raw));  // 1800d1001
    }

    void* slot = elem;                                       // 1800d100d
    ElementDescriptionApply(self, &slot);                    // 1800d101a

    *reinterpret_cast<void**>(elem + 0xc0) =
        *reinterpret_cast<void* const*>(self + 0x68);        // 1800d1023 QWORD

    AssignStringField(elem, 0xc8, self, 0x70);               // 1800d102a

    *reinterpret_cast<int32_t*>(elem + 0xd0) =
        *reinterpret_cast<const int32_t*>(self + 0x78);      // 1800d107c DWORD

    AssignStringField(elem, 0xd8, self, 0x80);               // 1800d1082

    *reinterpret_cast<uint8_t*>(elem + 0xd4) =
        *reinterpret_cast<const uint8_t*>(self + 0x7c);      // 1800d10d7 BYTE

    *ret = elem;                                             // 1800d10e1
    return ret;
}

int RtUiEditDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@EditDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_EditDescription_Type);            ++n;
    RegisterRuntimeAddr(0x18003f6b0ull, (void*)&RtUi_EditDescription_Type);

    RegisterRuntimeExport("?TypeName@EditDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_EditDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x18003f6d0ull, (void*)&RtUi_EditDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@EditDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_EditDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800d11e0ull, (void*)&RtUi_EditDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@EditDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_EditDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x18003f870ull, (void*)&RtUi_EditDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@EditDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_EditDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x18003f850ull, (void*)&RtUi_EditDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@EditDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_EditDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x18003f880ull, (void*)&RtUi_EditDescription_Destruct);

    RegisterRuntimeExport("??1EditDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_EditDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d11f0ull, (void*)&RtUi_EditDescription_Dtor);

    RegisterRuntimeExport("?Serialize@EditDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_EditDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800d1110ull, (void*)&RtUi_EditDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@EditDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_EditDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800d0f90ull, (void*)&RtUi_EditDescription_CreateInstance);

    return n;
}
