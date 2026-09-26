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
// ENDGAME B.3.6 block C3: every slot body is a `UI::EditDescription` member
// (runtime/types/ui/EditDescription.h) and the doors delegate with a QUALIFIED
// call. CreateInstance writes a real `UI::EditElement` now (EditElement.h, the
// 0xf8 layout).
//
// The constructor is OUT OF LINE (`??0EditDescription@UI@@QEAA@XZ`,
// 0x1800d12c0) and is ported by runtime/src/ui/factory_ui1.cpp.
//
// THIS CLASS HAS NO SEPARATE APPLIER. Its CreateInstance is the only one in the
// batch that open-codes TWO refcounted String ASSIGNMENTS (not releases) — the
// increment side of the protocol, which no other member of this batch does.
// ============================================================================
#include <atomic>

#include "description_common.h"
#include "../../types/ui/EditDescription.h"
#include "../../types/ui/EditElement.h"

namespace {

using namespace rtdesc;

void* VtEditDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblEditDescription); }

// RULE B6-5: member bodies resolve through namespace-scope atomics (same
// contract as element_common.h's rtuielem::AtomicExport, which cannot be
// included beside description_common.h — both declare Module).
void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}
std::atomic<void*> g_typeInfoSlot{nullptr};
std::atomic<void*> g_ctorSlot{nullptr};
std::atomic<void*> g_memFreeSlot{nullptr};

// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z through an atomic slot:
// description_common.h's rtdesc::MemFree keeps a first-call static, which a
// MEMBER body may not reach (RULE B6-5).
void FreeStringBlock(void* p) {
    using MemFreeFn = void (*)(void*);
    reinterpret_cast<MemFreeFn>(AtomicExport(
        g_memFreeSlot, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z"))(p);
}

const uint8_t* EditTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfoSlot, "?_typeInfo@EditDescription@UI@@0VBaseTypeInfo@Object@@A"));
}

// ??0EditDescription@UI@@QEAA@XZ — a ctor DOOR (factory_ui1.cpp).
void* EditCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return reinterpret_cast<CtorFn>(AtomicExport(
        g_ctorSlot, "??0EditDescription@UI@@QEAA@XZ"))(self);
}

// The refcounted System::String ASSIGNMENT retail open-codes twice in
// CreateInstance (0x1800d102a and 0x1800d1082). Not a release: the old value is
// released with the usual asymmetry, the new one is stored and its refcount
// INCREMENTED — and the whole thing is skipped when the two pointers are
// already equal (`CMP RAX,[RDI+0x70] ; JZ`), which is what makes a self-assign
// not drop the last reference.
void AssignStringField(char*& d, char* const s) {
    if (d == s) return;                                      // 1800d1035
    if (d != nullptr) {                                      // 1800d103a
        --*reinterpret_cast<int32_t*>(d - 4);
        if (*reinterpret_cast<int32_t*>(d - 4) == 0) {
            d = d - 8;                                       // 1800d1045
            FreeStringBlock(d);
        } else {
            d = nullptr;                                     // 1800d105b
        }
    }
    d = s;                                                   // 1800d106a
    if (d != nullptr) ++*reinterpret_cast<int32_t*>(d - 4);  // 1800d1076
}

}  // namespace

namespace UI {

uint32_t EditDescription::Type() const {
    uint32_t v = 0; __builtin_memcpy(&v, EditTypeInfo(), sizeof v); return v;
}
const char* EditDescription::TypeName() const {
    const char* v = nullptr; __builtin_memcpy(&v, EditTypeInfo() + 8, sizeof v); return v;
}
int32_t EditDescription::GetFileVersion() const { return 0x204; }

// 0x18003f870: null test (in the DOOR), then a JMP to the ctor.
Object::External* EditDescription::Construct() {
    EditCtor(this);                                          // 18003f879 JMP
    return this;
}
Object::External* EditDescription::Reconstruct() {
    DeletingDtor(0);                                         // 18003f85e
    EditCtor(this);                                          // 18003f868 JMP
    return this;
}
void EditDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// ?Serialize@EditDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d1110
// ============================================================================
// +0x68 goes through 0x18003f1e0, a DIFFERENT ExternalPtr serializer from the
// two this batch otherwise uses — which is how its pointee type (UI::Font) is
// distinguished.
bool EditDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    UI::ElementDescription::Serialize(stream);               // 1800d112a

    Serializer ser = { stream, nullptr };
    void* rw = MakeReaderWriter(stream, &ser);

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x18003f1e0ull)(&_ptr68, &ser);       // 1800d1168
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_string70, &ser);    // 1800d1176

    RwScalar(rw, kRwScalarInt,  &_word78);                   // 1800d118a
    RwScalar(rw, kRwScalarBool, &_flag7c);                   // 1800d119f

    RtAddr<FieldSerFn>(0x1800c9930ull)(&_string80, &ser);    // 1800d11ae

    DestroyReaderWriter(rw);                                 // 1800d11c6
    return true;
}

// ============================================================================
// ?CreateInstance@EditDescription@UI@@...   retail 0x1800d0f90  (slot 8)
// ============================================================================
// RETSLOT (B6-2). R8 (the Dialog) is never read — no children.
//
// EditElement is 0xf8 and its constructor is OUT OF LINE
// (`??0EditElement@UI@@QEAA@XZ`, 0x1800d0f10, a door in factory_ui1.cpp);
// retail takes its RETURN value as the element. The base applier is the
// non-virtual `ElementDescription::CreateInstance(InternalPtr<Element>)`
// (0x180115ad0) — a dialog.cpp door, reached by address.
//
// The field copies are a QWORD, two refcounted String assignments, a DWORD and
// a BYTE — and the BYTE copy is LAST, after both strings (0x1800d10d7).
Object::InternalPtr<Element>* EditDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* /*dialog*/) const {
    void* raw = MemAlloc(0xf8, 8);                           // 1800d0fd1

    EditElement* elem = nullptr;
    if (raw != nullptr) {                                    // 1800d0ffc
        using EditElementCtorFn = void* (*)(void*);
        elem = static_cast<EditElement*>(
            RtAddr<EditElementCtorFn>(0x1800d0f10ull)(raw)); // 1800d1001
    }

    void* slot = elem;                                       // 1800d100d
    ElementDescriptionApply(this, &slot);                    // 1800d101a

    elem->_font = _ptr68;                                    // 1800d1023 QWORD
    AssignStringField(elem->_text, _string70);               // 1800d102a
    elem->_maxLength = _word78;                              // 1800d107c DWORD
    AssignStringField(elem->_filter, _string80);             // 1800d1082
    elem->_password = _flag7c;                               // 1800d10d7 BYTE

    *reinterpret_cast<void**>(ret) = elem;                   // 1800d10e1
    return ret;
}

}  // namespace UI

// ============================================================================
// ??1EditDescription@UI@@UEAA@XZ   0x1800d11f0 — a DOOR BODY (no real dtor)
// ============================================================================
// TWO String releases (+0x80 then +0x70) under this class's own vtable, then
// the base vtable and the base's _textName. The +0x68 external is NOT released.
extern "C" void RtUi_EditDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtEditDescription();                                // 1800d120d
    ReleaseStringField(p, offsetof(UI::EditDescription, _string80));   // 1800d1210
    ReleaseStringField(p, offsetof(UI::EditDescription, _string70));   // 1800d1246
    *v = VtElementDescription();                             // 1800d1277
    ReleaseStringField(p, offsetof(UI::ElementDescription, _textName)); // 1800d127a

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800d12ac JMP
}

// ============================================================================
// The registered doors — one QUALIFIED delegation each.
// ============================================================================
extern "C" uint32_t RtUi_EditDescription_Type(const void* self) {
    return reinterpret_cast<const UI::EditDescription*>(self)->UI::EditDescription::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_EditDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::EditDescription*>(self)->UI::EditDescription::TypeName();   // B.3.6 bridge
}
extern "C" int32_t RtUi_EditDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::EditDescription*>(self)->UI::EditDescription::GetFileVersion();   // B.3.6 bridge
}
extern "C" void* RtUi_EditDescription_Construct(void* self) {
    if (self == nullptr) return nullptr;                     // 18003f873
    return reinterpret_cast<UI::EditDescription*>(self)->UI::EditDescription::Construct();   // B.3.6 bridge
}
extern "C" void* RtUi_EditDescription_Reconstruct(void* self) {
    return reinterpret_cast<UI::EditDescription*>(self)->UI::EditDescription::Reconstruct();   // B.3.6 bridge
}
extern "C" void RtUi_EditDescription_Destruct(void* self) {
    reinterpret_cast<UI::EditDescription*>(self)->UI::EditDescription::Destruct();   // B.3.6 bridge
}
extern "C" bool RtUi_EditDescription_Serialize(void* self, void* stream) {
    return reinterpret_cast<UI::EditDescription*>(self)->UI::EditDescription::Serialize(stream);   // B.3.6 bridge
}
// RDX is the caller's slot; the member writes it and hands it back (B6-2).
// R8 (the Dialog) is forwarded although the body never reads it.
extern "C" void** RtUi_EditDescription_CreateInstance(const void* self, void** ret,
                                                      const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::EditDescription*>(self)->UI::EditDescription::CreateInstance(   // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
            static_cast<const UI::Dialog*>(dialog)));
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
