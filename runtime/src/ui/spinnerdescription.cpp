// ============================================================================
// runtime/src/ui/spinnerdescription.cpp — UI::SpinnerDescription, whole class
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription.
//
// ??_7SpinnerDescription@UI@@6B@ (retail 0x1801576c0), ten slots:
//   0  deleting dtor      0x18004b050  deleting_dtor.cpp row (sizeof 0x88)
//   1  TypeName           0x18004ad60  here
//   2  Type               0x18004ad40  here
//   3  GetFileVersion     0x1800d8940  here  (returns 0x204)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d8880  here
//   6  Reconstruct        0x18004aee0  here
//   7  Destruct           0x18004af10  here
//   8  CreateInstance     0x1800d86b0  here
//   9  Construct          0x18004af00  here
// plus ??0SpinnerDescription@UI@@QEAA@XZ 0x1800d89c0 (out of line, both
// Construct and Reconstruct tail-jump to it) and
// ??1SpinnerDescription@UI@@UEAA@XZ 0x1800d8950.
//
// LAYOUT, sizeof 0x88 (`MOV EDX,0x88` at 0x18004b06f). ElementDescription
// (0x68) plus THREE child descriptions and one int:
//     +0x68  ExternalPtr<UI::ElementDescription>   0x180038f90
//     +0x70  ExternalPtr<UI::ElementDescription>   0x180038f90
//     +0x78  ExternalPtr<UI::ElementDescription>   0x180038f90
//     +0x80  int32  _i80   ctor 1, kScalarInt, copied to the element's +0xc0
//     +0x84..+0x87 never written
//
// The three children are NOT an Array — they are three separate fields, each
// serialized by its own call and each turned into one child element by its own
// copy of the create-parent-push-dirty sequence in CreateInstance. Three
// literal repetitions, not a loop, and transcribed as three.
//
// Every helper is already ported: 0x180038f90 (ExternalPtr<ElementDescription>
// ::Serialize, external_ptr_serialize.cpp), 0x180116b00, 0x180115ad0,
// 0x180112ed0, 0x180115e50, 0x180076900.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/ui/ElementDescription.h"
#include "../../types/ui/Element.h"
#include "../../types/io/Serializer.h"
#include "../../types/ui/SpinnerDescription.h"
#include "../../types/ui/SpinnerElement.h"

#include <atomic>
#include <new>

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

struct Stream {
    uint8_t _head00[0x08];
    int32_t mode;
    int32_t _pad0c;
    void*   encodingFactory;
};
struct Serializer {
    Stream* stream;
    void*   readerWriter;
};
static_assert(offsetof(Stream, mode) == 0x08, "IO::Stream mode @ +0x08");
static_assert(offsetof(Stream, encodingFactory) == 0x10, "IO::Stream factory @ +0x10");
static_assert(offsetof(Serializer, readerWriter) == 0x08, "IO::Serializer rw @ +0x08");

void** VTableOf(void* o) { return *reinterpret_cast<void***>(o); }

// One of CreateInstance's three identical child blocks (0x1800d874e,
// 0x1800d87a6, 0x1800d87f6 — the same instructions with a different source
// field each time).
void AddChild(const void* dialog, void** slot, UI::Element* elem, void* childDesc) {
    // UI::Dialog::CreateElement (block B's class) — a door, by address.
    using CreateElementFn = void** (*)(const void* dlg, void** ret, void* desc);
    void** made = RtAddr<CreateElementFn>(0x180112ed0ull)(dialog, slot, childDesc);

    UI::Element* child = *reinterpret_cast<UI::Element**>(made);
    if (child != nullptr)
        child->_parent = elem;

    using PushFn = void (*)(void* array, int n, const void* fill);
    RtAddr<PushFn>(0x180076900ull)(&elem->_children, elem->_children.GetSize() + 1, made);

    for (UI::Element* n = elem; n != nullptr; n = n->_parent)
        n->_flags |= UI::kElementFlagNeedsSizeCalc;
}

}  // namespace



// ============================================================================
// The MEMBERS
// ============================================================================
namespace {
// RULE B6-5: namespace-scope atomics, never a function-local first-call cache.
void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}
std::atomic<void*> g_typeInfo{nullptr};
const uint8_t* SpinnerTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@SpinnerDescription@UI@@0VBaseTypeInfo@Object@@A"));
}
}  // namespace

namespace UI {

// [1] 0x18004ad60 / [2] 0x18004ad40 / [3] 0x1800d8940
const char* SpinnerDescription::TypeName() const {
    const char* v = nullptr; __builtin_memcpy(&v, SpinnerTypeInfo() + 8, sizeof v); return v;
}
uint32_t SpinnerDescription::Type() const {
    uint32_t v = 0; __builtin_memcpy(&v, SpinnerTypeInfo(), sizeof v); return v;
}
// static -- the same read as slot 2 (the minted record until W3).
uint32_t SpinnerDescription::TypeId() {
    uint32_t v = 0; __builtin_memcpy(&v, SpinnerTypeInfo(), sizeof v); return v;
}
int32_t SpinnerDescription::GetFileVersion() const { return 0x204; }

// [9] 0x18004af00 — the ctor, placement (the null test sat in the retired door).
Object::External* SpinnerDescription::Construct() {
    return ::new (this) SpinnerDescription();
}
// [6] 0x18004aee0 — destroy in place through slot 0 (flag 0), re-run the ctor.
Object::External* SpinnerDescription::Reconstruct() {
    DeletingDtor(0);
    return ::new (this) SpinnerDescription();
}
// [7] 0x18004af10
void SpinnerDescription::Destruct() { DeletingDtor(0); }

// [5] ?Serialize@SpinnerDescription@UI@@UEAA_NAEAVStream@IO@@@Z  0x1800d8880
bool SpinnerDescription::Serialize(void* streamArg) {
    Stream* stream = static_cast<Stream*>(streamArg);

    // QUALIFIED: this object wears the LEAF's table, whose slot 5 is this body.
    ElementDescription::Serialize(stream);                              // 1800d889a

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                              // 1800d88ca

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x180038f90ull)(&_editDescription, &ser);        // 1800d88d8
    RtAddr<FieldSerFn>(0x180038f90ull)(&_upDescription, &ser);          // 1800d88e6
    RtAddr<FieldSerFn>(0x180038f90ull)(&_downDescription, &ser);        // 1800d88f4

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &_increment, nullptr);

    if (rw != nullptr) {                                                // 1800d891a
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// [8] ?CreateInstance@SpinnerDescription@UI@@...   retail 0x1800d86b0
// ============================================================================
// SpinnerElement is 0xc8 bytes (`MOV EDX,0xc8` at 0x1800d86f2); construction
// calls ??0Element, stamps the SpinnerElement vtable and writes ONE dword,
// +0xc0 = 1 — which the tail then overwrites with the description's +0x80. Both
// stores are retail's and both are here.
Object::InternalPtr<Element>* SpinnerDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0xc8, 8);           // 1800d86f2 (inlined)

    SpinnerElement* elem = static_cast<SpinnerElement*>(raw);
    if (raw != nullptr)                                                 // 1800d871d
        // The SpinnerElement ctor, INLINED by retail (0x1800d8722 .. 0x1800d8732):
        // the real ctor.
        elem = ::new (raw) SpinnerElement();

    void* slot = elem;                                                  // 1800d873c
    // The NON-virtual ElementDescription::CreateInstance(InternalPtr<Element>)
    // (0x180115ad0): a QUALIFIED member call since fragment pass F2.
    UI::ElementDescription::CreateInstance(                   // 1800d8749
        reinterpret_cast<Object::InternalPtr<Element>*>(&slot));  // B.3.6 bridge

    AddChild(dialog, &slot, elem, _editDescription);                    // 1800d874e
    AddChild(dialog, &slot, elem, _upDescription);                      // 1800d87a6
    AddChild(dialog, &slot, elem, _downDescription);                    // 1800d87f6

    elem->_increment = _increment;                                      // 1800d884c

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1800d8852
    return ret;
}

// ??1SpinnerDescription@UI@@UEAA@XZ  0x1800d8950 — retail's body is ONE String
// release (the base's _textName at +0x60, under the base vtable) and the tail
// JMP into ~External: the compiler's chain is all of it. The three
// ExternalPtrs are not released.
SpinnerDescription::~SpinnerDescription() {}

// ?CreateNewObject@SpinnerDescription@UI@@SAPEAVBase@Object@@XZ 0x18004ae50 --
// `?MemAlloc@MemoryAllocator` (0x1800c7a50) of 0x88, align 8; retail's null
// branch; the ctor. Replaces the factory interpreter row (B39_RULES D2).
Object::Base* SpinnerDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(SpinnerDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) SpinnerDescription();
}
// ?CreateInPlaceObject@SpinnerDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x18004aed0
Object::Base* SpinnerDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) SpinnerDescription();
}

}  // namespace UI

// ============================================================================
// ENDGAME B.3.9 UI-B: the slot doors, the ctor / dtor doors and the
// factory-interpreter row are gone. Their only readers were the minted
// `??_7SpinnerDescription@UI@@6B@`, its RT_DELDTOR row and FACTUI2: every live
// SpinnerDescription now wears the compiler's table.
// ============================================================================

int RtUiSpinnerDescriptionRegister() {
    int n = 0;
    // The factory pair (B39_RULES D2), the class's own statics, by name AND
    // address; the minted `?_typeInfo@SpinnerDescription` record resolves them.
    RegisterRuntimeExport("?CreateNewObject@SpinnerDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::SpinnerDescription::CreateNewObject);      ++n;
    RegisterRuntimeAddr(0x18004ae50ull, (void*)&UI::SpinnerDescription::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@SpinnerDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::SpinnerDescription::CreateInPlaceObject);  ++n;
    RegisterRuntimeAddr(0x18004aed0ull, (void*)&UI::SpinnerDescription::CreateInPlaceObject);
    return n;
}
