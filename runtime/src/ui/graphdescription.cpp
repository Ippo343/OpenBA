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
// ENDGAME B.3.6 block C5: every slot body MOVED into the members of the real
// `UI::GraphDescription` (runtime/types/ui/GraphDescription.h, cross-asserted
// against DescriptionsBatch1.h's POD); each registered door is a one-line
// QUALIFIED delegation. The dtor stays a door (B4b-2).
// ============================================================================
#include <atomic>
#include <new>

#include "description_common.h"
#include "../../types/ui/GraphDescription.h"
#include "../../types/ui/GraphElement.h"

namespace {

using namespace rtdesc;

// RULE B6-5: the members below run on the loader thread. Every export they
// resolve lives in a namespace-scope atomic, never a function-local static.
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
std::atomic<void*> g_vtable{nullptr};
std::atomic<void*> g_ctor{nullptr};

void* VtGraphDescription() { return AtomicExport(g_vtable, RtUI::kVtblGraphDescription); }
const uint8_t* GraphTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@GraphDescription@UI@@0VBaseTypeInfo@Object@@A"));
}

// ??0GraphDescription@UI@@QEAA@XZ (0x1800d1bd0) — the ctor DOOR (B4b-2).
void* GraphCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return reinterpret_cast<CtorFn>(AtomicExport(g_ctor, "??0GraphDescription@UI@@QEAA@XZ"))(self);
}

}  // namespace

namespace UI {

// [1] 0x180040090 / [2] 0x180040070 — the BaseTypeInfo's +0x08 name and +0x00 id.
const char* GraphDescription::TypeName() const {
    const char* v = nullptr; __builtin_memcpy(&v, GraphTypeInfo() + 8, sizeof v); return v;
}
uint32_t GraphDescription::Type() const {
    uint32_t v = 0; __builtin_memcpy(&v, GraphTypeInfo(), sizeof v); return v;
}
// [3] 0x1800d1b20
int32_t GraphDescription::GetFileVersion() const { return 0x104; }

// [9] 0x180040230 — `TEST RCX,RCX ; JZ` then a JMP to the ctor. The null-`this`
// test stays in the door; the member is the JMP.
Object::External* GraphDescription::Construct() {
    return reinterpret_cast<Object::External*>(GraphCtor(this));      // 180040239 JMP
}
// [6] 0x180040210 — destroy in place through slot 0 (flag 0), re-run the ctor.
Object::External* GraphDescription::Reconstruct() {
    DeletingDtor(0);                                                    // 18004021e slot 0
    return reinterpret_cast<Object::External*>(GraphCtor(this));      // 180040228 JMP
}
// [7] 0x180040240
void GraphDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// [5] ?Serialize@GraphDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d1a30
// ============================================================================
// THREE ints then ONE UNSIGNED. +0x74 goes through the kScalarUInt slot (+0x98)
// while +0x68/+0x6c/+0x70 go through kScalarInt (+0xa0); the split is read off
// 0x1800d1acd versus 0x1800d1a8e/0x1800d1aa3/0x1800d1ab8, not assumed.
bool GraphDescription::Serialize(void* streamArg) {
    Stream* stream = static_cast<Stream*>(streamArg);

    // QUALIFIED: this object wears the LEAF's table, whose slot 5 is this body.
    ElementDescription::Serialize(stream);                              // 1800d1a4a

    Serializer ser = { stream, nullptr };
    void* rw = MakeReaderWriter(stream, &ser);

    RwScalar(rw, kRwScalarInt,  &_width);                               // 1800d1a8e
    RwScalar(rw, kRwScalarInt,  &_height);                              // 1800d1aa3
    RwScalar(rw, kRwScalarInt,  &_tickSize);                            // 1800d1ab8
    RwScalar(rw, kRwScalarUInt, &_unknown74);                           // 1800d1acd

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x180004590ull)(&_spriteSheet, &ser);            // 1800d1adc ExternalPtr<SpriteSheet>
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_spriteName, &ser);             // 1800d1aed System::String

    DestroyReaderWriter(rw);                                            // 1800d1b05
    return true;
}

// ============================================================================
// [8] ?CreateInstance@GraphDescription@UI@@...   retail 0x1800d1920
// ============================================================================
// GraphElement is 0x100 and its constructor is OUT OF LINE
// (`??0GraphElement@UI@@QEAA@XZ`, 0x1800d1880, already ported by
// runtime/src/ui/factory_ui1.cpp).
//
// THE FOUR DWORD COPIES ARE NOT IN OFFSET ORDER: +0x68 -> +0xc0, +0x6c -> +0xc4,
// then +0x74 -> +0xcc, and only THEN +0x70 -> +0xc8. Read off
// 0x1800d19ab..0x1800d19c9; reordering them would be a different function under
// any interruption.
Object::InternalPtr<Element>* GraphDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog*) const {
    void* raw = MemAlloc(0x100, 8);                                     // 1800d1961

    GraphElement* elem = static_cast<GraphElement*>(raw);
    if (raw != nullptr) {                                               // 1800d198c
        using GraphElementCtorFn = void* (*)(void*);
        elem = static_cast<GraphElement*>(
            RtAddr<GraphElementCtorFn>(0x1800d1880ull)(raw));           // 1800d1991
    }

    void* slot = elem;                                                  // 1800d1999
    // The NON-virtual ElementDescription::CreateInstance(InternalPtr<Element>)
    // (0x180115ad0): a QUALIFIED member call since fragment pass F2.
    UI::ElementDescription::CreateInstance(                   // 1800d19a6
        reinterpret_cast<Object::InternalPtr<Element>*>(&slot));  // B.3.6 bridge

    elem->_width     = _width;                                          // 1800d19ae
    elem->_height    = _height;                                         // 1800d19b7
    elem->_unknownCC = static_cast<int32_t>(_unknown74);                // 1800d19c0
    elem->_tickSize  = _tickSize;                                       // 1800d19c9

    {
        void* sheet = _spriteSheet;                                     // 1800d19cf
        const uint64_t idxSlot = SpriteIndexSlot(sheet, &_spriteName);  // 1800d19e2
        uint8_t* sprite = reinterpret_cast<uint8_t*>(&elem->_lineSprite);
        *reinterpret_cast<void**>(sprite + 0x00)    = sheet;            // 1800d19f5
        *reinterpret_cast<uint64_t*>(sprite + 0x08) = idxSlot;          // 1800d1a01 QWORD store
    }

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1800d1a08
    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, QUALIFIED delegations.
// ============================================================================
extern "C" uint32_t RtUi_GraphDescription_Type(const void* self) {
    return reinterpret_cast<const UI::GraphDescription*>(self)->UI::GraphDescription::Type();
}
extern "C" const char* RtUi_GraphDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::GraphDescription*>(self)->UI::GraphDescription::TypeName();
}
extern "C" int32_t RtUi_GraphDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::GraphDescription*>(self)->UI::GraphDescription::GetFileVersion();
}
extern "C" void* RtUi_GraphDescription_Construct(void* self) {
    if (self == nullptr) return nullptr;                                // 180040233 (retail null test)
    return reinterpret_cast<UI::GraphDescription*>(self)->UI::GraphDescription::Construct();
}
extern "C" void* RtUi_GraphDescription_Reconstruct(void* self) {
    return reinterpret_cast<UI::GraphDescription*>(self)->UI::GraphDescription::Reconstruct();
}
extern "C" void RtUi_GraphDescription_Destruct(void* self) {
    reinterpret_cast<UI::GraphDescription*>(self)->UI::GraphDescription::Destruct();
}
extern "C" bool RtUi_GraphDescription_Serialize(void* self, void* stream) {
    return reinterpret_cast<UI::GraphDescription*>(self)->UI::GraphDescription::Serialize(stream);
}
extern "C" void** RtUi_GraphDescription_CreateInstance(const void* self, void** ret,
                                                       const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::GraphDescription*>(self)->UI::GraphDescription::CreateInstance(
            reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
            static_cast<const UI::Dialog*>(dialog)));
}

// ============================================================================
// ??1GraphDescription@UI@@UEAA@XZ   0x1800d1b30 — a DOOR (B4b-2), unchanged.
// ============================================================================
extern "C" void RtUi_GraphDescription_Dtor(void* selfv) {
    using namespace rtdesc;
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtGraphDescription();                               // 1800d1b4d
    ReleaseStringField(p, 0x80);                             // 1800d1b50
    *v = VtElementDescription();                             // 1800d1b8d
    ReleaseStringField(p, 0x60);                             // 1800d1b90

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800d1bc2 JMP
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
