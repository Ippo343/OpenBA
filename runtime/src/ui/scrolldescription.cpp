// ============================================================================
// runtime/src/ui/scrolldescription.cpp — UI::ScrollDescription, whole class
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription.
//
// ??_7ScrollDescription@UI@@6B@ (retail 0x180156460), ten slots:
//   0  deleting dtor      0x18003a790  deleting_dtor.cpp row (sizeof 0xc8)
//   1  TypeName           0x18003a120  here
//   2  Type               0x18003a100  here
//   3  GetFileVersion     0x18011a930  here  (returns 0x104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x18011a940  here
//   6  Reconstruct        0x18003a2a0  here
//   7  Destruct           0x18003a2d0  here
//   8  CreateInstance     0x18011aac0  here
//   9  Construct          0x18003a2c0  here
// plus ??0ScrollDescription@UI@@QEAA@XZ 0x18011a4e0 (out of line) and
// ??1ScrollDescription@UI@@UEAA@XZ 0x18011a5d0.
//
// LAYOUT, sizeof 0xc8 (`MOV EDX,0xc8` at 0x18003a7af). ElementDescription
// (0x68) plus:
//     +0x68  ExternalPtr<UI::SpriteSheet>      0x180004590
//     +0x70 .. +0xb0  NINE System::Strings — the batch's largest sprite-name
//                     block; each is looked up against the one sheet at +0x68
//     +0xb8  bool    ctor 1   kScalarBool
//     +0xbc  int32   ctor 1   kScalarInt
//     +0xc0  uint32  \  ctor writes ONE QWORD of -1 at +0xc0 (0x18011a5ab),
//     +0xc4  uint32  /  so BOTH start at 0xffffffff
// and both are kScalarUInt (vt+0x98), not kScalarInt.
//
// Every helper is already ported, including ??0ScrollElement@UI@@ (0x18011a7f0,
// ui_factory_wave28.cpp), so this file adds none.
// ============================================================================
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/ui/ScrollDescription.h"
#include "../../types/ui/ScrollElement.h"
#include "../../types/io/Serializer.h"

// ENDGAME B.3.6 block C4: the layout moved to runtime/types/ui/
// ScrollDescription.h (the real class). Every slot body below is a MEMBER; each
// registered door is a one-line QUALIFIED delegation. The ctor and dtor stay
// DOORS (B4b-2). RULE B6-5: every export the members reach is cached in a
// namespace-scope std::atomic slot, never in a function-local static.

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}

std::atomic<void*> g_vtBase{nullptr};
std::atomic<void*> g_vtExternal{nullptr};
std::atomic<void*> g_vtElementDescription{nullptr};
std::atomic<void*> g_vtScrollDescription{nullptr};
std::atomic<void*> g_typeInfo{nullptr};
std::atomic<void*> g_getSpriteIndex{nullptr};
std::atomic<void*> g_memFree{nullptr};
std::atomic<void*> g_externalDtor{nullptr};

void* VtBase()              { return AtomicExport(g_vtBase, "??_7Base@Object@@6B@"); }
void* VtExternal()          { return AtomicExport(g_vtExternal, "??_7External@Object@@6B@"); }
void* VtElementDescription(){ return AtomicExport(g_vtElementDescription, "??_7ElementDescription@UI@@6B@"); }
void* VtScrollDescription() { return AtomicExport(g_vtScrollDescription, "??_7ScrollDescription@UI@@6B@"); }
const uint8_t* ScrollDescriptionTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@ScrollDescription@UI@@0VBaseTypeInfo@Object@@A"));
}

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

void ReleaseStringField(char** field) {
    char* s = *field;
    if (s == nullptr) return;
    if (--*reinterpret_cast<int32_t*>(s - 4) == 0) {
        *field = s - 8;
        reinterpret_cast<MemFreeFn>(AtomicExport(
            g_memFree, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z"))(*field);
    } else {
        *field = nullptr;
    }
}

}  // namespace

// ??0ScrollDescription@UI@@QEAA@XZ  retail 0x18011a4e0 — a DOOR (B4b-2)
extern "C" void* RtUi_ScrollDescription_Ctor(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 18011a4f1
    *v = VtExternal();                                          // 18011a4fb
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 18011a510
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 18011a513
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 18011a517
    *v = VtElementDescription();                                // 18011a51b
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 18011a525
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 18011a529
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 18011a52d
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 18011a531  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 18011a534  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 18011a53b  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 18011a543
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 18011a547
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 18011a54b
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 18011a54f
    *v = VtScrollDescription();                                 // 18011a553

    UI::ScrollDescription* self = reinterpret_cast<UI::ScrollDescription*>(p);   // B.3.6 bridge
    self->_spriteSheet = nullptr;                               // 18011a55d
    for (int i = 0; i < 9; ++i) self->_spriteNames[i] = nullptr;   // 18011a561 .. 18011a593
    self->_vertical = 1;                                        // 18011a59a  BYTE
    self->_inset = 1;                                           // 18011a5a1  DWORD
    // ONE QWORD of -1 at +0xc0 (18011a5ab): both halves start at 0xffffffff.
    self->_barColor  = 0xffffffffu;
    self->_unknownC4 = 0xffffffffu;
    return p;                                                   // 18011a5b6
}

// ============================================================================
// The member bodies (ENDGAME B.3.6 block C4)
// ============================================================================
namespace UI {

uint32_t ScrollDescription::Type() const {
    const uint8_t* ti = ScrollDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
const char* ScrollDescription::TypeName() const {
    const uint8_t* ti = ScrollDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
int32_t ScrollDescription::GetFileVersion() const { return 0x104; }

// 0x18003a2c0 — the null test stays in the door.
Object::External* ScrollDescription::Construct() {
    return static_cast<Object::External*>(RtUi_ScrollDescription_Ctor(this));
}
// 0x18003a2a0 — slot 0 (VIRTUAL, the minted row) with flag 0, then the ctor.
Object::External* ScrollDescription::Reconstruct() {
    DeletingDtor(0);
    return static_cast<Object::External*>(RtUi_ScrollDescription_Ctor(this));
}
// 0x18003a2d0
void ScrollDescription::Destruct() { DeletingDtor(0); }

// ?Serialize@ScrollDescription@UI@@UEAA_NAEAVStream@IO@@@Z  0x18011a940
bool ScrollDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    UI::ElementDescription::Serialize(stream);                 // 18011a95a (qualified)

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 18011a98a

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x180004590ull)(&_spriteSheet, &ser);   // 18011a998
    for (int i = 0; i < 9; ++i)                                // 18011a9a6 .. 18011aa2b
        RtAddr<FieldSerFn>(0x1800c9930ull)(&_spriteNames[i], &ser);

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool / 8])(rw, &_vertical, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt  / 8])(rw, &_inset, nullptr);
    // BOTH of the last two are kScalarUInt (vt+0x98), not kScalarInt.
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarUInt / 8])(rw, &_barColor, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarUInt / 8])(rw, &_unknownC4, nullptr);

    if (rw != nullptr) {                                       // 18011aa96
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@ScrollDescription@UI@@...   retail 0x18011aac0
// ============================================================================
// ScrollElement is 0x178 bytes (`MOV EDX,0x178` at 0x18011ab00); its ctor is
// CALLED out of line (0x18011a7f0, the scrollelement.cpp door). R8 (the
// Dialog) is never read — this class creates no children.
//
// NINE {sheet, index} pairs into +0xc0/+0xc8 .. +0x140/+0x148, with the same
// dword-written/qword-read stack slot the other appliers in this batch have,
// and the sheet RE-READ from the description before each of the nine.
//
// THE LAST THREE COPIES ARE NOT IN ORDER: +0xbc -> element +0x15c, +0xc0 ->
// +0x154, +0xc4 -> +0x158 (0x18011ad34, 0x18011ad40, 0x18011ad4c). Copying them
// in destination order would swap two element fields.
Object::InternalPtr<Element>* ScrollDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* /*dialog — never read*/) const {
    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0x178, 8);  // 18011ab00 (inlined)

    void* obj = raw;
    if (raw != nullptr) {                                      // 18011ab2a
        using ScrollElementCtorFn = void* (*)(void*);
        obj = RtAddr<ScrollElementCtorFn>(0x18011a7f0ull)(raw);    // 18011ab2f
    }
    ScrollElement* elem = reinterpret_cast<ScrollElement*>(obj);   // B.3.6 bridge

    void* slot = elem;                                         // 18011ab37
    UI::ElementDescription::CreateInstance(                   // 18011ab42
        reinterpret_cast<Object::InternalPtr<Element>*>(&slot));  // B.3.6 bridge

    ::Sprite* const sprites[9] = {
        &elem->_trackStart, &elem->_trackMiddle, &elem->_trackEnd,
        &elem->_thumbStart, &elem->_thumbMiddle, &elem->_thumbEnd,
        &elem->_hotStart,   &elem->_hotMiddle,   &elem->_hotEnd };

    using GetIdxFn = int32_t (*)(const void* sheet, const void* nameStr);
    uint64_t idxSlot;                                          // [RBP-0x10]
    for (int i = 0; i < 9; ++i) {
        void* sheet = _spriteSheet;                            // RE-READ each time
        if (sheet != nullptr) {
            const int32_t idx = reinterpret_cast<GetIdxFn>(AtomicExport(
                g_getSpriteIndex,
                "?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z"))(
                    sheet, &_spriteNames[i]);
            __builtin_memcpy(&idxSlot, &idx, sizeof idx);      // DWORD store
        } else {
            const int32_t minusOne = -1;
            __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);
        }
        sprites[i]->spriteSheet = sheet;
        __builtin_memcpy(&sprites[i]->spriteIndex, &idxSlot, 8);   // QWORD store (+pad)
    }

    elem->_vertical   = _vertical;                             // 18011ad2e
    elem->_inset      = _inset;                                // 18011ad3a
    elem->_barColor   = _barColor;                             // 18011ad46
    elem->_unknown158 = _unknownC4;                            // 18011ad52

    *reinterpret_cast<void**>(ret) = elem;                     // 18011ad58
    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, each a one-line
// QUALIFIED delegation onto the member above.
// ============================================================================
using UI::ScrollDescription;

extern "C" uint32_t RtUi_ScrollDescription_Type(const ScrollDescription* self) {
    return self->UI::ScrollDescription::Type();
}
extern "C" const char* RtUi_ScrollDescription_TypeName(const ScrollDescription* self) {
    return self->UI::ScrollDescription::TypeName();
}
extern "C" int32_t RtUi_ScrollDescription_GetFileVersion(const ScrollDescription* self) {
    return self->UI::ScrollDescription::GetFileVersion();
}
extern "C" void* RtUi_ScrollDescription_Construct(ScrollDescription* self) {
    if (self == nullptr) return nullptr;                       // 18003a2c0
    return self->UI::ScrollDescription::Construct();
}
extern "C" void* RtUi_ScrollDescription_Reconstruct(ScrollDescription* self) {
    return self->UI::ScrollDescription::Reconstruct();
}
extern "C" void RtUi_ScrollDescription_Destruct(ScrollDescription* self) {
    self->UI::ScrollDescription::Destruct();
}
extern "C" bool RtUi_ScrollDescription_Serialize(ScrollDescription* self, void* stream) {
    return self->UI::ScrollDescription::Serialize(stream);
}
// RDX is the caller's slot; R8 (the Dialog) is forwarded and never read (B6-2).
extern "C" void** RtUi_ScrollDescription_CreateInstance(const ScrollDescription* self,
                                                        void** ret, const void* dialog) {
    return reinterpret_cast<void**>(self->UI::ScrollDescription::CreateInstance(
        reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),        // B.3.6 bridge
        static_cast<const UI::Dialog*>(dialog)));
}

// ??1ScrollDescription@UI@@UEAA@XZ  0x18011a5d0 — a DOOR (B4b-2). The nine
// Strings in REVERSE order (+0xb0 down to +0x70) under this class's vtable,
// then the base's _textName at +0x60. The ExternalPtr at +0x68 is not released.
extern "C" void RtUi_ScrollDescription_Dtor(void* selfv) {
    ScrollDescription* self = reinterpret_cast<ScrollDescription*>(selfv);   // B.3.6 bridge
    void** v = reinterpret_cast<void**>(selfv);

    *v = VtScrollDescription();                                // 18011a5eb
    for (int i = 8; i >= 0; --i)                               // 18011a5f5 .. 18011a77a
        ReleaseStringField(&self->_spriteNames[i]);

    *v = VtElementDescription();                               // 18011a7a0
    ReleaseStringField(&self->_textName);                      // 18011a7aa

    using ExtDtorFn = void (*)(void*);
    reinterpret_cast<ExtDtorFn>(AtomicExport(
        g_externalDtor, "??1External@Object@@UEAA@XZ"))(selfv);   // 18011a7dd JMP
}

int RtUiScrollDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@ScrollDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_ScrollDescription_Type);            ++n;
    RegisterRuntimeAddr(0x18003a100ull, (void*)&RtUi_ScrollDescription_Type);

    RegisterRuntimeExport("?TypeName@ScrollDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ScrollDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x18003a120ull, (void*)&RtUi_ScrollDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@ScrollDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_ScrollDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x18011a930ull, (void*)&RtUi_ScrollDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@ScrollDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ScrollDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x18003a2c0ull, (void*)&RtUi_ScrollDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@ScrollDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ScrollDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x18003a2a0ull, (void*)&RtUi_ScrollDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@ScrollDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_ScrollDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x18003a2d0ull, (void*)&RtUi_ScrollDescription_Destruct);

    RegisterRuntimeExport("??0ScrollDescription@UI@@QEAA@XZ",
                          (void*)&RtUi_ScrollDescription_Ctor);            ++n;
    RegisterRuntimeAddr(0x18011a4e0ull, (void*)&RtUi_ScrollDescription_Ctor);

    RegisterRuntimeExport("??1ScrollDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_ScrollDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x18011a5d0ull, (void*)&RtUi_ScrollDescription_Dtor);

    RegisterRuntimeExport("?Serialize@ScrollDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_ScrollDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x18011a940ull, (void*)&RtUi_ScrollDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@ScrollDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_ScrollDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x18011aac0ull, (void*)&RtUi_ScrollDescription_CreateInstance);

    return n;
}
