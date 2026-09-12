// ============================================================================
// runtime/src/ui/checkdescription.cpp — UI::CheckDescription, the whole class
// ============================================================================
//
// ??_7CheckDescription@UI@@6B@ (retail 0x1801572c0), ten slots:
//   0  deleting dtor      0x1800489c0  runtime/src/object/deleting_dtor.cpp row
//   1  TypeName           0x180048680  here
//   2  Type               0x180048660  here
//   3  GetFileVersion     0x180111920  here  (returns 0x1104 — NOT 0x104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x180111930  here
//   6  Reconstruct        0x180048850  here
//   7  Destruct           0x1800488f0  here
//   8  CreateInstance     0x1801119c0  here
//   9  Construct          0x1800488a0  here
// plus ??1CheckDescription@UI@@UEAA@XZ (0x180111620).
//
// DERIVES FROM UI::ButtonDescription, and the derivation is load-bearing in
// three places, all of them reaching runtime/src/ui/buttondescription.cpp:
//   Construct/Reconstruct call ??0ButtonDescription (0x180110b10);
//   Serialize calls ButtonDescription::Serialize (0x1801113e0);
//   CreateInstance calls the shared applier 0x1801108d0.
//
// THERE IS NO OUT-OF-LINE ??0CheckDescription. The DLL exports none and the
// kCheckDescription row in runtime/src/ui/factory_ui1_tables.inc has a null
// ctor with `??0ButtonDescription@UI@@QEAA@XZ` as its BASE ctor plus two
// stores of its own — exactly the two stores `Construct` open-codes below.
//
// GetFileVersion is 0x1104, read from this class's own body (0x180111920). Its
// base returns 0x104. The constant is per class and was read per class.
//
// Layout: runtime/types/ui/DescriptionsBatch1.h. sizeof 0xa8.
// ============================================================================
#include "description_common.h"
#include "../../types/ui/DescriptionsBatch1.h"

namespace {

using namespace rtdesc;

void* VtCheckDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblCheckDescription); }
void* VtButtonDescription(){ return RT_DATA_SLOT(void*, RtUI::kVtblButtonDescription); }
void* VtButtonElement()    { return RT_DATA_SLOT(void*, RtUI::kVtblButtonElement); }
void* VtCheckElement()     { return RT_DATA_SLOT(void*, RtUI::kVtblCheckElement); }
const uint8_t* CheckTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@CheckDescription@UI@@0VBaseTypeInfo@Object@@A");
}

void* ButtonCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return RT_EXPORT_ONCE(CtorFn, "??0ButtonDescription@UI@@QEAA@XZ")(self);
}

// The two stores CheckDescription adds on top of its base's constructor,
// transcribed from 0x1800488c2..0x1800488cc (and identically from
// 0x180048877..0x180048881 inside Reconstruct).
void ConstructTail(uint8_t* p) {
    *reinterpret_cast<void**>(p)        = VtCheckDescription();   // 1800488c9
    *reinterpret_cast<uint64_t*>(p + 0xa0) = 0;                   // 1800488cc
}

}  // namespace

extern "C" uint32_t RtUi_CheckDescription_Type(const void*) {
    const uint8_t* ti = CheckTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_CheckDescription_TypeName(const void*) {
    const uint8_t* ti = CheckTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_CheckDescription_GetFileVersion(const void*) { return 0x1104; }

// 0x1800488a0: null test, base ctor, own two stores, return this.
extern "C" void* RtUi_CheckDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                  // 1800488ba
    uint8_t* p = static_cast<uint8_t*>(selfv);
    ButtonCtor(p);                                         // 1800488bc
    ConstructTail(p);
    return p;                                              // 1800488d7
}
// 0x180048850: slot 0 with flag 0, then the same construction, NO null test.
extern "C" void* RtUi_CheckDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                   // 180048867
    uint8_t* p = static_cast<uint8_t*>(selfv);
    ButtonCtor(p);                                         // 180048871
    ConstructTail(p);
    return p;                                              // 18004888c
}
extern "C" void RtUi_CheckDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1CheckDescription@UI@@UEAA@XZ   0x180111620
// ============================================================================
// THREE vtable stamps back to back — own, ButtonDescription's, then
// ElementDescription's — with nothing between them, then the base's _textName.
// All three are kept; see buttondescription.cpp's ~ButtonDescription note.
// The +0xa0 child description is NOT released.
extern "C" void RtUi_CheckDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtCheckDescription();                             // 18011163d
    *v = VtButtonDescription();                            // 180111647
    *v = VtElementDescription();                           // 180111651
    ReleaseStringField(p, 0x60);                           // 180111654

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 180111686 JMP
}

// ============================================================================
// ?Serialize@CheckDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x180111930
// ============================================================================
// Note the base call is ButtonDescription::Serialize (0x1801113e0), NOT
// ElementDescription::Serialize — this is the one place in this batch where the
// base serializer is a sibling class's rather than the root's.
extern "C" bool RtUi_CheckDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x1801113e0ull)(p, stream);          // 18011194a

    Serializer ser = { stream, nullptr };
    void* rw = MakeReaderWriter(stream, &ser);

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0xa0, &ser);    // 18011198b

    DestroyReaderWriter(rw);                               // 1801119a3
    return true;
}

// ============================================================================
// ?CreateInstance@CheckDescription@UI@@...   retail 0x1801119c0
// ============================================================================
// CheckElement is 0xe0 bytes, and its constructor is inlined here as the
// BUTTON element's inline constructor followed by two more stores:
//     ??0Element (0x180115e50)
//     ButtonElement vtable, +0xc0 = -1 (dword), +0xc4 = 0 (dword)
//     CheckElement vtable, +0xd8 = 0 (BYTE — `MOV byte`, not dword)
// so the ButtonElement vtable is written and then overwritten. Kept.
//
// Then the shared ButtonDescription applier (0x1801108d0), then ONE more child
// from `self->_checkDesc`, attached to the element retail still holds in RDI —
// not re-read from the slot the way the applier's own loop does it.
extern "C" void** RtUi_CheckDescription_CreateInstance(const void* selfv,
                                                       void** ret,
                                                       const void* dialog) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    void* raw = MemAlloc(0xe0, 8);                         // 180111a02

    uint8_t* elem = nullptr;
    if (raw != nullptr) {                                  // 180111a2d
        ElementCtor(raw);                                  // 180111a32
        uint8_t* e = static_cast<uint8_t*>(raw);
        *reinterpret_cast<void**>(e)          = VtButtonElement();  // 180111a3f
        *reinterpret_cast<int32_t*>(e + 0xc0) = -1;                 // 180111a42
        *reinterpret_cast<int32_t*>(e + 0xc4) = 0;                  // 180111a4c
        *reinterpret_cast<void**>(e)          = VtCheckElement();   // 180111a5d
        *reinterpret_cast<uint8_t*>(e + 0xd8) = 0;                  // 180111a60 BYTE
        elem = e;
    }

    void* slot = elem;                                     // 180111a67
    using ApplyFn = void (*)(const void*, void**, const void*);
    RtAddr<ApplyFn>(0x1801108d0ull)(self, &slot, dialog);  // 180111a77

    void** made = DialogCreateElement(dialog, &slot,
                                      *reinterpret_cast<void* const*>(self + 0xa0)); // 180111a8b
    AttachAndPushChild(elem, made);                        // 180111a98 .. 180111ad4

    *ret = elem;                                           // 180111ad6
    return ret;
}

int RtUiCheckDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@CheckDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_CheckDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180048660ull, (void*)&RtUi_CheckDescription_Type);

    RegisterRuntimeExport("?TypeName@CheckDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_CheckDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180048680ull, (void*)&RtUi_CheckDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@CheckDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_CheckDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x180111920ull, (void*)&RtUi_CheckDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@CheckDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_CheckDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x1800488a0ull, (void*)&RtUi_CheckDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@CheckDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_CheckDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180048850ull, (void*)&RtUi_CheckDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@CheckDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_CheckDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x1800488f0ull, (void*)&RtUi_CheckDescription_Destruct);

    RegisterRuntimeExport("??1CheckDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_CheckDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x180111620ull, (void*)&RtUi_CheckDescription_Dtor);

    RegisterRuntimeExport("?Serialize@CheckDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_CheckDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x180111930ull, (void*)&RtUi_CheckDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@CheckDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_CheckDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1801119c0ull, (void*)&RtUi_CheckDescription_CreateInstance);

    return n;
}
