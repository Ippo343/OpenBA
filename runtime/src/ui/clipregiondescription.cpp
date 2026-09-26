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
// ENDGAME B.3.6 block C1: the bodies are MEMBERS of
// `UI::ClipRegionDescription` (runtime/types/ui/ClipRegionDescription.h); the
// doors are one-line QUALIFIED delegations.
//
// ----------------------------------------------------------------------------
// THE CONSTRUCTOR IS INLINED INTO Construct / Reconstruct / both factory steps
// ----------------------------------------------------------------------------
// Each open-codes the same store sequence; the kClipRegionDescription row in
// runtime/src/ui/factory_ui1_tables.inc has a null ctor for that reason.
// (`??0ClipRegionDescription@UI@@QEAA@XZ` DOES exist and IS exported —
// 0x1800cc140, B36_SLOTS.md sections 6/7 — the old banner here said it did
// not. Nothing calls it from the tree.) `ConstructBody` below is that sequence
// written once, transcribed from 0x180037cfa..0x180037d78. Two details
// Ghidra's C loses:
//   * `+0x40` is written as ONE QWORD holding 3 (`MOV qword [RCX+0x40],3` at
//     0x180037d44), covering both dwords there;
//   * `+0x38` and `+0x3c` are two SEPARATE DWORDS (0 and -1).
// The three dead base-vtable stamps (Base, then External over it unread, then
// ElementDescription) are reproduced.
//
// Layout: runtime/types/ui/ClipRegionDescription.h over DescriptionsBatch1.h.
// sizeof 0x78. Both ints start at 0x64 — 100, the default clip extent.
// ============================================================================
#include <atomic>
#include <new>

#include "description_common.h"
#include "../../types/ui/ClipRegionDescription.h"
#include "../../types/ui/ClipRegionElement.h"

namespace {

using namespace rtdesc;

// RULE B6-5: namespace-scope atomic door caches, never a function-local static
// inside a member (see buttondescription.cpp).
void* AtomicResolve(std::atomic<void*>& slot, const char* mangled) {
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
std::atomic<void*> g_vtClipRegionDescription{nullptr};
std::atomic<void*> g_vtClipRegionElement{nullptr};
std::atomic<void*> g_clipTypeInfo{nullptr};

void* VtBaseAtomic()     { return AtomicResolve(g_vtBase, "??_7Base@Object@@6B@"); }
void* VtExternalAtomic() { return AtomicResolve(g_vtExternal, "??_7External@Object@@6B@"); }
void* VtElementDescriptionAtomic() {
    return AtomicResolve(g_vtElementDescription, "??_7ElementDescription@UI@@6B@");
}
void* VtClipRegionDescription() {
    return AtomicResolve(g_vtClipRegionDescription, RtUI::kVtblClipRegionDescription);
}
void* VtClipRegionElement() {
    return AtomicResolve(g_vtClipRegionElement, RtUI::kVtblClipRegionElement);
}
const uint8_t* ClipRegionTypeInfo() {
    return static_cast<const uint8_t*>(AtomicResolve(
        g_clipTypeInfo, "?_typeInfo@ClipRegionDescription@UI@@0VBaseTypeInfo@Object@@A"));
}

// The inlined constructor, over raw bytes: it writes the vptr three times and
// every byte of the base, which no field-by-field member spelling can express.
void ConstructBody(uint8_t* p) {
    void** v = reinterpret_cast<void**>(p);

    *v = VtBaseAtomic();                                    // 180037d01
    *v = VtExternalAtomic();                                // 180037d0b
    // `LEA RAX,[RCX+8]` then `MOV [RAX],RDX` — retail writes +0x08 through an
    // intermediate pointer (the SEH frame's visible temporary) and +0x10/+0x18
    // directly. Byte-identical; the temporary costs nothing to drop.
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;             // 180037d19
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;             // 180037d1c
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;             // 180037d20
    *v = VtElementDescriptionAtomic();                      // 180037d2b
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
static_assert(offsetof(UI::ClipRegionDescription, _width) == 0x68 &&
              offsetof(UI::ClipRegionDescription, _childDesc) == 0x70,
              "ConstructBody's tail stores are the class's fields");

// The dtor door only.
void* VtClipRegionDescriptionDoor() { return RT_DATA_SLOT(void*, RtUI::kVtblClipRegionDescription); }

}  // namespace

namespace UI {

uint32_t ClipRegionDescription::Type() const {
    const uint8_t* ti = ClipRegionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
const char* ClipRegionDescription::TypeName() const {
    const uint8_t* ti = ClipRegionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
int32_t ClipRegionDescription::GetFileVersion() const { return 0x104; }

// 0x180037ce0 (the null test at 0x180037cf4 is the DOOR's).
Object::External* ClipRegionDescription::Construct() {
    ConstructBody(reinterpret_cast<uint8_t*>(this));
    return this;                                            // 180037d78
}
// 0x180037c30: slot 0 with flag 0, then the same body. No null test.
Object::External* ClipRegionDescription::Reconstruct() {
    DeletingDtor(0);                                        // 180037c47
    ConstructBody(reinterpret_cast<uint8_t*>(this));
    return this;                                            // 180037ccc
}
void ClipRegionDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// ?Serialize@ClipRegionDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800cc010
// ============================================================================
// The base half is QUALIFIED; then the two extents through the INT slot and
// the one child description. No array arm (unlike its siblings).
bool ClipRegionDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    ElementDescription::Serialize(stream);                  // 1800cc02a

    Serializer ser = { stream, nullptr };
    void* rw = MakeReaderWriter(stream, &ser);

    RwScalar(rw, kRwScalarInt, &_width);                    // 1800cc06e
    RwScalar(rw, kRwScalarInt, &_height);                   // 1800cc083

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x180038f90ull)(&_childDesc, &ser);  // 1800cc092

    DestroyReaderWriter(rw);                                // 1800cc0aa
    return true;
}

// ============================================================================
// ?CreateInstance@ClipRegionDescription@UI@@...   retail 0x1800cbec0   (slot 8)
// ============================================================================
// RCX = this, RDX = the hidden return slot, R8 = Dialog* (RULE B6-2).
//
// ClipRegionElement is 0xd8 and its constructor is INLINED here, with THREE
// stores whose widths are not uniform and one of which is UNALIGNED:
//     1800cbf42  MOV dword [RDI+0xc0],0x64
//     1800cbf4c  MOV qword [RDI+0xc4],0x64     <- UNALIGNED, covers +0xc4/+0xc8
//     1800cbf59  MOV qword [RDI+0xcc],0        <- UNALIGNED, covers +0xcc/+0xd0
//     1800cbf60  MOV dword [RDI+0xd4],0
// The two overlapping qwords are written as memcpy at the exact offsets.
//
// THE NULL-ALLOCATION ARM CLEARS ONLY THE LOW 32 BITS (`XOR EAX,EAX ;
// MOV EDI,EAX` at 0x1800cbf68) — which zeroes all 64 on x64, so it is the same
// null. Noted because the asymmetry with the other members of this batch
// (`MOV RBX,R12`) is real in the bytes and is not a difference in behaviour.
Object::InternalPtr<Element>* ClipRegionDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    void* raw = MemAlloc(0xd8, 8);                          // 1800cbf02

    ClipRegionElement* elem = nullptr;
    if (raw != nullptr) {                                   // 1800cbf2d
        ElementCtor(raw);                                   // 1800cbf32
        ClipRegionElement* e = reinterpret_cast<ClipRegionElement*>(raw);   // B.3.6 bridge
        uint8_t* eb = static_cast<uint8_t*>(raw);
        *reinterpret_cast<void**>(e) = VtClipRegionElement();       // 1800cbf3f
        e->_reqWidth = 0x64;                                        // 1800cbf42
        const uint64_t k64 = 0x64, kZero = 0;
        __builtin_memcpy(eb + offsetof(ClipRegionElement, _reqHeight), &k64, 8);   // 1800cbf4c
        __builtin_memcpy(eb + offsetof(ClipRegionElement, _innerHeight), &kZero, 8); // 1800cbf59
        e->_offsetY = 0;                                            // 1800cbf60
        elem = e;
    }

    void* slot = static_cast<Element*>(elem);               // 1800cbf6c
    ElementDescriptionApply(this, &slot);                   // 1800cbf79

    // The two extents are copied AFTER the base applier, so they win over
    // anything it wrote.
    elem->_reqWidth  = _width;                              // 1800cbf81
    elem->_reqHeight = _height;                             // 1800cbf8a

    void** made = DialogCreateElement(dialog, &slot, _childDesc);   // 1800cbf9c
    AttachAndPushChild(reinterpret_cast<uint8_t*>(elem), made);     // 1800cbfa9 .. 1800cbfe4

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1800cbfe6
    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS
// ============================================================================
extern "C" uint32_t RtUi_ClipRegionDescription_Type(const void* self) {
    return reinterpret_cast<const UI::ClipRegionDescription*>(self)      // B.3.6 bridge
        ->UI::ClipRegionDescription::Type();
}
extern "C" const char* RtUi_ClipRegionDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::ClipRegionDescription*>(self)      // B.3.6 bridge
        ->UI::ClipRegionDescription::TypeName();
}
extern "C" int32_t RtUi_ClipRegionDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::ClipRegionDescription*>(self)      // B.3.6 bridge
        ->UI::ClipRegionDescription::GetFileVersion();
}

extern "C" void* RtUi_ClipRegionDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                   // 180037cf4
    return reinterpret_cast<UI::ClipRegionDescription*>(selfv)           // B.3.6 bridge
        ->UI::ClipRegionDescription::Construct();
}
extern "C" void* RtUi_ClipRegionDescription_Reconstruct(void* selfv) {
    return reinterpret_cast<UI::ClipRegionDescription*>(selfv)           // B.3.6 bridge
        ->UI::ClipRegionDescription::Reconstruct();
}
extern "C" void RtUi_ClipRegionDescription_Destruct(void* selfv) {
    reinterpret_cast<UI::ClipRegionDescription*>(selfv)                  // B.3.6 bridge
        ->UI::ClipRegionDescription::Destruct();
}

// ============================================================================
// ??1ClipRegionDescription@UI@@UEAA@XZ   0x1800cc0d0   — a DTOR DOOR (B4b-2)
// ============================================================================
extern "C" void RtUi_ClipRegionDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtClipRegionDescriptionDoor();                     // 1800cc0ed
    *v = VtElementDescription();                            // 1800cc0f7
    ReleaseStringField(p, 0x60);                            // 1800cc0fa

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800cc12c JMP
}

extern "C" bool RtUi_ClipRegionDescription_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<UI::ClipRegionDescription*>(selfv)           // B.3.6 bridge
        ->UI::ClipRegionDescription::Serialize(streamv);
}

extern "C" void** RtUi_ClipRegionDescription_CreateInstance(const void* selfv,
                                                            void** ret,
                                                            const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::ClipRegionDescription*>(selfv)        // B.3.6 bridge
            ->UI::ClipRegionDescription::CreateInstance(
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
                static_cast<const UI::Dialog*>(dialog)));
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
