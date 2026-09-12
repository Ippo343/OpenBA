// ============================================================================
// runtime/src/ui/graphdescription.cpp — UI::GraphDescription, the whole class
// ============================================================================
//
// ??_7GraphDescription@UI@@6B@ (retail 0x180156a30), ten slots:
//   0  deleting dtor      0x1800403a0  runtime/src/object/deleting_dtor.cpp row
//   1  TypeName           0x180040090  here
//   2  Type               0x180040070  here
//   3  GetFileVersion     0x1800d1b20  here  (returns 0x104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d1a30  here
//   6  Reconstruct        0x180040210  here
//   7  Destruct           0x180040240  here
//   8  CreateInstance     0x1800d1920  here
//   9  Construct          0x180040230  here
// plus ??1GraphDescription@UI@@UEAA@XZ (0x1800d1b30).
//
// The constructor is OUT OF LINE (`??0GraphDescription@UI@@QEAA@XZ`,
// 0x1800d1bd0) and is ALREADY ported by runtime/src/ui/factory_ui1.cpp.
//
// Layout: runtime/types/ui/DescriptionsBatch1.h. sizeof 0x88.
// ============================================================================
#include "description_common.h"
#include "../../types/ui/DescriptionsBatch1.h"

namespace {

using namespace rtdesc;

void* VtGraphDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblGraphDescription); }
const uint8_t* GraphTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@GraphDescription@UI@@0VBaseTypeInfo@Object@@A");
}

void* GraphCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return RT_EXPORT_ONCE(CtorFn, "??0GraphDescription@UI@@QEAA@XZ")(self);
}

}  // namespace

extern "C" uint32_t RtUi_GraphDescription_Type(const void*) {
    const uint8_t* ti = GraphTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_GraphDescription_TypeName(const void*) {
    const uint8_t* ti = GraphTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_GraphDescription_GetFileVersion(const void*) { return 0x104; }

extern "C" void* RtUi_GraphDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                    // 180040233
    return GraphCtor(selfv);                                 // 180040239 JMP
}
extern "C" void* RtUi_GraphDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                     // 18004021e
    return GraphCtor(selfv);                                 // 180040228 JMP
}
extern "C" void RtUi_GraphDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1GraphDescription@UI@@UEAA@XZ   0x1800d1b30
// ============================================================================
extern "C" void RtUi_GraphDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtGraphDescription();                               // 1800d1b4d
    ReleaseStringField(p, 0x80);                             // 1800d1b50
    *v = VtElementDescription();                             // 1800d1b8d
    ReleaseStringField(p, 0x60);                             // 1800d1b90

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800d1bc2 JMP
}

// ============================================================================
// ?Serialize@GraphDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d1a30
// ============================================================================
// THREE ints then ONE UNSIGNED. +0x74 goes through the kScalarUInt slot (+0x98)
// while +0x68/+0x6c/+0x70 go through kScalarInt (+0xa0); the split is read off
// 0x1800d1acd versus 0x1800d1a8e/0x1800d1aa3/0x1800d1ab8, not assumed.
extern "C" bool RtUi_GraphDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    ElementDescriptionSerialize(p, stream);                  // 1800d1a4a

    Serializer ser = { stream, nullptr };
    void* rw = MakeReaderWriter(stream, &ser);

    RwScalar(rw, kRwScalarInt,  p + 0x68);                   // 1800d1a8e
    RwScalar(rw, kRwScalarInt,  p + 0x6c);                   // 1800d1aa3
    RwScalar(rw, kRwScalarInt,  p + 0x70);                   // 1800d1ab8
    RwScalar(rw, kRwScalarUInt, p + 0x74);                   // 1800d1acd

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x180004590ull)(p + 0x78, &ser);      // 1800d1adc ExternalPtr<SpriteSheet>
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x80, &ser);      // 1800d1aed System::String

    DestroyReaderWriter(rw);                                 // 1800d1b05
    return true;
}

// ============================================================================
// ?CreateInstance@GraphDescription@UI@@...   retail 0x1800d1920
// ============================================================================
// GraphElement is 0x100 and its constructor is OUT OF LINE
// (`??0GraphElement@UI@@QEAA@XZ`, 0x1800d1880, already ported by
// runtime/src/ui/factory_ui1.cpp).
//
// THE FOUR DWORD COPIES ARE NOT IN OFFSET ORDER: +0x68 -> +0xc0, +0x6c -> +0xc4,
// then +0x74 -> +0xcc, and only THEN +0x70 -> +0xc8. Read off
// 0x1800d19ab..0x1800d19c9; reordering them would be a different function under
// any interruption.
extern "C" void** RtUi_GraphDescription_CreateInstance(const void* selfv, void** ret) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    void* raw = MemAlloc(0x100, 8);                          // 1800d1961

    uint8_t* elem = static_cast<uint8_t*>(raw);
    if (raw != nullptr) {                                    // 1800d198c
        using GraphElementCtorFn = void* (*)(void*);
        elem = static_cast<uint8_t*>(
            RtAddr<GraphElementCtorFn>(0x1800d1880ull)(raw));  // 1800d1991
    }

    void* slot = elem;                                       // 1800d1999
    ElementDescriptionApply(self, &slot);                    // 1800d19a6

    *reinterpret_cast<int32_t*>(elem + 0xc0) =
        *reinterpret_cast<const int32_t*>(self + 0x68);      // 1800d19ae
    *reinterpret_cast<int32_t*>(elem + 0xc4) =
        *reinterpret_cast<const int32_t*>(self + 0x6c);      // 1800d19b7
    *reinterpret_cast<int32_t*>(elem + 0xcc) =
        *reinterpret_cast<const int32_t*>(self + 0x74);      // 1800d19c0
    *reinterpret_cast<int32_t*>(elem + 0xc8) =
        *reinterpret_cast<const int32_t*>(self + 0x70);      // 1800d19c9

    {
        void* sheet = *reinterpret_cast<void* const*>(self + 0x78);   // 1800d19cf
        const uint64_t idxSlot = SpriteIndexSlot(sheet, self + 0x80); // 1800d19e2
        *reinterpret_cast<void**>(elem + 0xd0)    = sheet;   // 1800d19f5
        *reinterpret_cast<uint64_t*>(elem + 0xd8) = idxSlot; // 1800d1a01 QWORD store
    }

    *ret = elem;                                             // 1800d1a08
    return ret;
}

int RtUiGraphDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@GraphDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_GraphDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180040070ull, (void*)&RtUi_GraphDescription_Type);

    RegisterRuntimeExport("?TypeName@GraphDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_GraphDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180040090ull, (void*)&RtUi_GraphDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@GraphDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_GraphDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800d1b20ull, (void*)&RtUi_GraphDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@GraphDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_GraphDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180040230ull, (void*)&RtUi_GraphDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@GraphDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_GraphDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180040210ull, (void*)&RtUi_GraphDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@GraphDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_GraphDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180040240ull, (void*)&RtUi_GraphDescription_Destruct);

    RegisterRuntimeExport("??1GraphDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_GraphDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d1b30ull, (void*)&RtUi_GraphDescription_Dtor);

    RegisterRuntimeExport("?Serialize@GraphDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_GraphDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800d1a30ull, (void*)&RtUi_GraphDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@GraphDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_GraphDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800d1920ull, (void*)&RtUi_GraphDescription_CreateInstance);

    return n;
}
