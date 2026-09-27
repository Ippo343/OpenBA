// ============================================================================
// runtime/src/ui/editdescription.cpp — UI::EditDescription, the whole class
// ============================================================================
//
// ??_7EditDescription@UI@@6B@ (retail 0x180156880), ten slots:
//   0  deleting dtor      0x18003fa20  EditDescription.h (the class's own)
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
// 0x1800d12c0). ENDGAME B.3.9 UI-A: it is the real ctor (EditDescription.h);
// the class is concrete, its factory pair its own, the doors retired.
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

// The refcounted System::String RELEASE (retail's asymmetry: the freeing
// branch leaves the field at `data - 8`, the sharing branch nulls it), through
// the atomic MemFree slot so a member body keeps no first-call static.
void ReleaseStringFieldAtomic(uint8_t* p, size_t off) {
    char*& s = *reinterpret_cast<char**>(p + off);
    if (s != nullptr) {
        --*reinterpret_cast<int32_t*>(s - 4);
        if (*reinterpret_cast<int32_t*>(s - 4) == 0) {
            s = s - 8;
            FreeStringBlock(s);
        } else {
            s = nullptr;
        }
    }
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

uint32_t EditDescription::Type() const { return TypeId(); }
// ?TypeId@EditDescription@UI@@SAIXZ 0x18003f6c0 — the record's first dword.
uint32_t EditDescription::TypeId() {
    uint32_t v = 0; __builtin_memcpy(&v, EditTypeInfo(), sizeof v); return v;
}
const char* EditDescription::TypeName() const {
    const char* v = nullptr; __builtin_memcpy(&v, EditTypeInfo() + 8, sizeof v); return v;
}
int32_t EditDescription::GetFileVersion() const { return 0x204; }

// 0x18003f870: null test (it sat in the retired door), then a JMP to the ctor.
Object::External* EditDescription::Construct() {
    return ::new (this) EditDescription();                   // 18003f879 JMP
}
Object::External* EditDescription::Reconstruct() {
    DeletingDtor(0);                                         // 18003f85e
    return ::new (this) EditDescription();                   // 18003f868 JMP
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
// (`??0EditElement@UI@@QEAA@XZ`, 0x1800d0f10; the real ctor since B.3.9);
// retail takes its RETURN value as the element. The base applier is the
// non-virtual `ElementDescription::CreateInstance(InternalPtr<Element>)`
// (0x180115ad0) — a QUALIFIED member call since fragment pass F2.
//
// The field copies are a QWORD, two refcounted String assignments, a DWORD and
// a BYTE — and the BYTE copy is LAST, after both strings (0x1800d10d7).
Object::InternalPtr<Element>* EditDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* /*dialog*/) const {
    void* raw = MemAlloc(0xf8, 8);                           // 1800d0fd1

    EditElement* elem = nullptr;
    if (raw != nullptr)                                      // 1800d0ffc
        elem = ::new (raw) EditElement();                    // 1800d1001 ??0EditElement

    void* slot = elem;                                       // 1800d100d
    UI::ElementDescription::CreateInstance(                   // 1800d101a
        reinterpret_cast<Object::InternalPtr<Element>*>(&slot));  // B.3.6 bridge

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
// ??1EditDescription@UI@@UEAA@XZ   0x1800d11f0 — retail's body, moved here
// from the retired door (ENDGAME B.3.9 UI-A)
// ============================================================================
// TWO String releases (+0x80 then +0x70). The restamps are the compiler's, and
// so is the chain into ~ElementDescription (the base's _textName) and
// ~External (retail's tail JMP at 0x1800d12ac). The +0x68 external is NOT
// released.
namespace UI {
EditDescription::~EditDescription() {
    uint8_t* p = reinterpret_cast<uint8_t*>(this);
    ReleaseStringFieldAtomic(p, offsetof(EditDescription, _string80));   // 1800d1210
    ReleaseStringFieldAtomic(p, offsetof(EditDescription, _string70));   // 1800d1246
}

// ?CreateNewObject@EditDescription@UI@@SAPEAVBase@Object@@XZ  0x18003f7c0
// The FACTUI1 row this replaces: MemAlloc(0x88, 8), retail's null branch,
// then the out-of-line ctor.
Object::Base* EditDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(EditDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) EditDescription();
}
// ?CreateInPlaceObject@EditDescription@UI@@SAPEAVBase@Object@@PEAX@Z  0x18003f840
Object::Base* EditDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) EditDescription();
}
}  // namespace UI

// ============================================================================
// Registration: the factory pair only (B39_RULES D2). The slot doors and the
// ??1 door, read only by the retired minted table / RT_DELDTOR row, are gone.
// ============================================================================
int RtUiEditDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?CreateNewObject@EditDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::EditDescription::CreateNewObject);        ++n;
    RegisterRuntimeAddr(0x18003f7c0ull, (void*)&UI::EditDescription::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@EditDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::EditDescription::CreateInPlaceObject);    ++n;
    RegisterRuntimeAddr(0x18003f840ull, (void*)&UI::EditDescription::CreateInPlaceObject);
    return n;
}
