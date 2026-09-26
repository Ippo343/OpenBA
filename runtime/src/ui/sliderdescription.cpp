// ============================================================================
// runtime/src/ui/sliderdescription.cpp — UI::SliderDescription, whole class
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription.
//
// ??_7SliderDescription@UI@@6B@ (retail 0x1801575e0), ten slots:
//   0  deleting dtor      0x180049c60  deleting_dtor.cpp row (sizeof 0xb8)
//   1  TypeName           0x180049750  here
//   2  Type               0x180049730  here
//   3  GetFileVersion     0x1800d79f0  here  (returns 0x304)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d7890  here
//   6  Reconstruct        0x1800498d0  here
//   7  Destruct           0x180049900  here
//   8  CreateInstance     0x1800d7630  here
//   9  Construct          0x1800498f0  here
// plus ??0SliderDescription@UI@@QEAA@XZ 0x1800d7b60 (out of line) and
// ??1SliderDescription@UI@@UEAA@XZ 0x1800d7a00.
//
// LAYOUT: runtime/types/ui/SliderDescription.h (sizeof 0xb8; RULE B6-8 moved
// this file's offsets there under the retail name).
//
// ENDGAME B.3.6 block C5: every slot body MOVED into the members of the real
// `UI::SliderDescription`; each registered door is a one-line QUALIFIED
// delegation. The ctor / dtor stay doors (B4b-2).
//
// THE DESTRUCTOR RELEASES THE FIVE STRINGS IN REVERSE ORDER (+0x98, +0x90,
// +0x88, +0x80, +0x78) under THIS class's vtable, then restamps the base vtable
// and releases ElementDescription's _textName at +0x60. Neither ExternalPtr is
// released. All of that is retail's.
//
// Every helper is already ported: 0x180038f90 and 0x180004590
// (external_ptr_serialize.cpp), 0x1800c9930 (string.cpp), 0x180116b00,
// 0x180115ad0, 0x180112ed0, 0x180076900, and
// ?GetSpriteIndexFromName@SpriteSheet@UI@@ (0x1800031f0), reached by its export
// name the way imagedescription.cpp reaches it.
// ??0SliderElement@UI@@QEAA@XZ (0x1800d7570) is NOT ported here — the *Element
// constructors belong to the Element files; it is reached through ResolveAddr.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/ui/ElementDescription.h"
#include "../../types/ui/Element.h"
#include "../../types/io/Serializer.h"
#include "../../types/ui/SliderDescription.h"
#include "../../types/ui/SliderElement.h"

#include <atomic>
#include <new>

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

void* VtBase()               { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal()           { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtElementDescription() { return RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@"); }
void* VtSliderDescription()  { return RT_DATA_SLOT(void*, "??_7SliderDescription@UI@@6B@"); }
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


using MemFreeFn = void (*)(void*);

// The refcounted-String release retail inlines six times in ~SliderDescription,
// with its usual asymmetry: on the last reference the FIELD is moved to
// `data - 8` and that is what is freed; otherwise the field is NULLED.
void ReleaseStringField(char** field) {
    char* s = *field;
    if (s == nullptr) return;
    if (--*reinterpret_cast<int32_t*>(s - 4) == 0) {
        *field = s - 8;
        RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(*field);
    } else {
        *field = nullptr;
    }
}

}  // namespace

// ============================================================================
// ??0SliderDescription@UI@@QEAA@XZ   retail 0x1800d7b60
// ============================================================================
extern "C" void* RtUi_SliderDescription_Ctor(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 1800d7b71
    *v = VtExternal();                                          // 1800d7b7b
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 1800d7b90
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 1800d7b93
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 1800d7b97
    *v = VtElementDescription();                                // 1800d7b9b
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 1800d7ba5
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 1800d7ba9
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 1800d7bad
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 1800d7bb1  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 1800d7bb4  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 1800d7bbb  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 1800d7bc3
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 1800d7bc7
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 1800d7bcb
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 1800d7bcf
    *v = VtSliderDescription();                                 // 1800d7bd3
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 1800d7bdd
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 1800d7be1
    *reinterpret_cast<uint64_t*>(p + 0x78) = 0;                 // 1800d7be5
    *reinterpret_cast<uint64_t*>(p + 0x80) = 0;                 // 1800d7be9
    *reinterpret_cast<uint64_t*>(p + 0x88) = 0;                 // 1800d7bf0
    *reinterpret_cast<uint64_t*>(p + 0x90) = 0;                 // 1800d7bf7
    *reinterpret_cast<uint64_t*>(p + 0x98) = 0;                 // 1800d7bfe
    *reinterpret_cast<uint64_t*>(p + 0xa0) = 0;                 // 1800d7c05  _i a0 + a4
    *reinterpret_cast<int32_t*>(p + 0xa8)  = 0;                 // 1800d7c0c  DWORD (0.0f)
    *reinterpret_cast<int32_t*>(p + 0xac)  = 0x3f800000;        // 1800d7c12  1.0f, stored
                                                                //            as an integer
    *reinterpret_cast<int32_t*>(p + 0xb0)  = 0xa;               // 1800d7c1c
    return p;                                                   // 1800d7c26
}

// ??1SliderDescription@UI@@UEAA@XZ  0x1800d7a00
extern "C" void RtUi_SliderDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtSliderDescription();                                // 1800d7a1b

    // REVERSE ORDER, 0x98 down to 0x78 — retail's, and the order is visible in
    // the addresses below.
    ReleaseStringField(reinterpret_cast<char**>(p + 0x98));    // 1800d7a25
    ReleaseStringField(reinterpret_cast<char**>(p + 0x90));    // 1800d7a58
    ReleaseStringField(reinterpret_cast<char**>(p + 0x88));    // 1800d7a8a
    ReleaseStringField(reinterpret_cast<char**>(p + 0x80));    // 1800d7abc
    ReleaseStringField(reinterpret_cast<char**>(p + 0x78));    // 1800d7aee

    *v = VtElementDescription();                               // 1800d7b14
    ReleaseStringField(reinterpret_cast<char**>(p + 0x60));    // 1800d7b1e

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800d7b51 JMP
}

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
std::atomic<void*> g_getSpriteIndex{nullptr};
const uint8_t* SliderTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@SliderDescription@UI@@0VBaseTypeInfo@Object@@A"));
}
// ?GetSpriteIndexFromName@SpriteSheet@UI@@ — block D's class, a door.
int32_t GetSpriteIndexFromName(const void* sheet, const void* nameStr) {
    using GetIdxFn = int32_t (*)(const void* sheet, const void* nameStr);
    return reinterpret_cast<GetIdxFn>(AtomicExport(
        g_getSpriteIndex,
        "?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z"))(sheet, nameStr);
}
// A raw 4-byte copy: retail moves the two floats through EAX, not an XMM register.
template <typename D, typename S>
void Copy4(D& dst, const S& src) {
    static_assert(sizeof(D) == 4 && sizeof(S) == 4, "four bytes");
    __builtin_memcpy(&dst, &src, 4);
}
}  // namespace

namespace UI {

// [1] 0x180049750 / [2] 0x180049730 / [3] 0x1800d79f0
const char* SliderDescription::TypeName() const {
    const char* v = nullptr; __builtin_memcpy(&v, SliderTypeInfo() + 8, sizeof v); return v;
}
uint32_t SliderDescription::Type() const {
    uint32_t v = 0; __builtin_memcpy(&v, SliderTypeInfo(), sizeof v); return v;
}
int32_t SliderDescription::GetFileVersion() const { return 0x304; }

// [9] 0x1800498f0 — the null-`this` test stays in the door.
Object::External* SliderDescription::Construct() {
    return reinterpret_cast<Object::External*>(RtUi_SliderDescription_Ctor(this));
}
// [6] 0x1800498d0 — destroy in place through slot 0 (flag 0), re-run the ctor.
Object::External* SliderDescription::Reconstruct() {
    DeletingDtor(0);
    return reinterpret_cast<Object::External*>(RtUi_SliderDescription_Ctor(this));
}
// [7] 0x180049900
void SliderDescription::Destruct() { DeletingDtor(0); }

// [5] ?Serialize@SliderDescription@UI@@UEAA_NAEAVStream@IO@@@Z  0x1800d7890
bool SliderDescription::Serialize(void* streamArg) {
    Stream* stream = static_cast<Stream*>(streamArg);

    // QUALIFIED: this object wears the LEAF's table, whose slot 5 is this body.
    ElementDescription::Serialize(stream);                              // 1800d78aa

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                              // 1800d78da

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x180038f90ull)(&_childDescription, &ser);       // 1800d78e8 ExternalPtr<ElementDescription>
    RtAddr<FieldSerFn>(0x180004590ull)(&_spriteSheet, &ser);            // 1800d78f6 ExternalPtr<SpriteSheet>
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_trackStartName, &ser);         // 1800d7904
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_trackMiddleName, &ser);        // 1800d7915
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_trackEndName, &ser);           // 1800d7926
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_thumbName, &ser);              // 1800d7937
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_thumbHotName, &ser);           // 1800d7948

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt   / 8])(rw, &_trackOffsetY, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt   / 8])(rw, &_thumbMargin, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarFloat / 8])(rw, &_minimum, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarFloat / 8])(rw, &_maximum, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt   / 8])(rw, &_steps, nullptr);

    if (rw != nullptr) {                                                // 1800d79ce
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// [8] ?CreateInstance@SliderDescription@UI@@...   retail 0x1800d7630
// ============================================================================
// SliderElement is 0x130 bytes (`MOV EDX,0x130` at 0x1800d7679) and its ctor is
// CALLED out of line (0x1800d7570). ONE child element from +0x68, then FIVE
// sprite lookups.
//
// *** THE FIVE LOOKUPS REPEAT ImageDescription's WIDTH BUG, five times. ***
// Each `MOV dword [RBP-0x10], EAX` writes FOUR bytes and the following
// `MOV RAX,[RBP-0x10]` reads EIGHT, so each of the element's index slots
// (+0xc8, +0xd8, +0xe8, +0xf8, +0x108) receives four bytes of the looked-up
// index and four bytes of whatever was on retail's stack. All five share ONE
// stack slot, so after the first iteration the upper half is the PREVIOUS
// lookup's leftover — deterministic in retail, and not reproducible here. What
// is faithful is the WIDTH, so the upper half is left indeterminate.
//
// The sheet pointer is RE-READ from the description before every lookup
// (0x1800d7716, 0x1800d7749, ...), not hoisted; retail does not assume it is
// stable across the calls and neither does this.
Object::InternalPtr<Element>* SliderDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0x130, 8);          // 1800d7679 (inlined)

    SliderElement* elem = static_cast<SliderElement*>(raw);
    if (raw != nullptr) {                                               // 1800d76a3
        using SliderElementCtorFn = void* (*)(void*);
        elem = static_cast<SliderElement*>(
            RtAddr<SliderElementCtorFn>(0x1800d7570ull)(raw));          // 1800d76a8
    }

    void* slot = elem;                                                  // 1800d76b0
    // The NON-virtual ElementDescription::CreateInstance(InternalPtr<Element>)
    // (0x180115ad0): its body is still dialog.cpp's door (fragment pass).
    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(this, &slot);            // 1800d76bb

    {   // the single child, from +0x68. UI::Dialog::CreateElement — block B's door.
        using CreateElementFn = void** (*)(const void* dlg, void** ret, void* desc);
        void** made = RtAddr<CreateElementFn>(0x180112ed0ull)(
            dialog, &slot, _childDescription);                          // 1800d76cb

        Element* child = *reinterpret_cast<Element**>(made);            // 1800d76d0
        if (child != nullptr)
            child->_parent = elem;                                      // 1800d76d8

        using PushFn = void (*)(void* array, int n, const void* fill);
        RtAddr<PushFn>(0x180076900ull)(&elem->_children,
                                       elem->_children.GetSize() + 1,
                                       made);                           // 1800d76ee

        for (Element* n = elem; n != nullptr; n = n->_parent)           // 1800d7700..1800d7714
            n->_flags |= kElementFlagNeedsSizeCalc;
    }

    // The five {sheet, index} pairs. ONE stack slot, written 4 bytes wide and
    // read 8 — see the banner.
    uint64_t idxSlot;                                                   // [RBP-0x10]
    char* const* const names[5] = { &_trackStartName, &_trackMiddleName, &_trackEndName,
                                    &_thumbName, &_thumbHotName };
    ::Sprite* const sprites[5] = { &elem->_trackStart, &elem->_trackMiddle, &elem->_trackEnd,
                                   &elem->_thumb, &elem->_thumbHot };
    for (int i = 0; i < 5; ++i) {
        void* sheet = _spriteSheet;                                     // RE-READ each time
        if (sheet != nullptr) {
            const int32_t idx = GetSpriteIndexFromName(sheet, names[i]);
            __builtin_memcpy(&idxSlot, &idx, sizeof idx);               // DWORD store
        } else {
            const int32_t minusOne = -1;
            __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);
        }
        uint8_t* s = reinterpret_cast<uint8_t*>(sprites[i]);
        *reinterpret_cast<void**>(s)        = sheet;
        *reinterpret_cast<uint64_t*>(s + 8) = idxSlot;                  // QWORD store
    }

    elem->_trackOffsetY = _trackOffsetY;                                // 1800d7827
    elem->_thumbMargin  = _thumbMargin;                                 // 1800d7833
    Copy4(elem->_minimum, _minimum);                                    // 1800d783f  (4 raw bytes)
    Copy4(elem->_maximum, _maximum);                                    // 1800d784b  (4 raw bytes)
    elem->_steps        = _steps;                                       // 1800d7857

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1800d785d
    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, QUALIFIED delegations.
// ============================================================================
extern "C" uint32_t RtUi_SliderDescription_Type(const void* self) {
    return reinterpret_cast<const UI::SliderDescription*>(self)->UI::SliderDescription::Type();
}
extern "C" const char* RtUi_SliderDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::SliderDescription*>(self)->UI::SliderDescription::TypeName();
}
extern "C" int32_t RtUi_SliderDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::SliderDescription*>(self)
        ->UI::SliderDescription::GetFileVersion();
}
extern "C" void* RtUi_SliderDescription_Construct(void* self) {
    if (self == nullptr) return nullptr;                                // 1800498f0 (retail null test)
    return reinterpret_cast<UI::SliderDescription*>(self)->UI::SliderDescription::Construct();
}
extern "C" void* RtUi_SliderDescription_Reconstruct(void* self) {
    return reinterpret_cast<UI::SliderDescription*>(self)->UI::SliderDescription::Reconstruct();
}
extern "C" void RtUi_SliderDescription_Destruct(void* self) {
    reinterpret_cast<UI::SliderDescription*>(self)->UI::SliderDescription::Destruct();
}
extern "C" bool RtUi_SliderDescription_Serialize(void* self, void* stream) {
    return reinterpret_cast<UI::SliderDescription*>(self)->UI::SliderDescription::Serialize(stream);
}
extern "C" void** RtUi_SliderDescription_CreateInstance(const void* self, void** ret,
                                                        const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::SliderDescription*>(self)
            ->UI::SliderDescription::CreateInstance(
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
                static_cast<const UI::Dialog*>(dialog)));
}

int RtUiSliderDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@SliderDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_SliderDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180049730ull, (void*)&RtUi_SliderDescription_Type);

    RegisterRuntimeExport("?TypeName@SliderDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_SliderDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180049750ull, (void*)&RtUi_SliderDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@SliderDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_SliderDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800d79f0ull, (void*)&RtUi_SliderDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@SliderDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_SliderDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x1800498f0ull, (void*)&RtUi_SliderDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@SliderDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_SliderDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x1800498d0ull, (void*)&RtUi_SliderDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@SliderDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_SliderDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180049900ull, (void*)&RtUi_SliderDescription_Destruct);

    RegisterRuntimeExport("??0SliderDescription@UI@@QEAA@XZ",
                          (void*)&RtUi_SliderDescription_Ctor);            ++n;
    RegisterRuntimeAddr(0x1800d7b60ull, (void*)&RtUi_SliderDescription_Ctor);

    RegisterRuntimeExport("??1SliderDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_SliderDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d7a00ull, (void*)&RtUi_SliderDescription_Dtor);

    RegisterRuntimeExport("?Serialize@SliderDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_SliderDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800d7890ull, (void*)&RtUi_SliderDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@SliderDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_SliderDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800d7630ull, (void*)&RtUi_SliderDescription_CreateInstance);

    return n;
}
