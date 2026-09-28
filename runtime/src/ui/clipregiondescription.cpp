// ============================================================================
// runtime/src/ui/clipregiondescription.cpp — UI::ClipRegionDescription, whole
// ============================================================================
//
// ??_7ClipRegionDescription@UI@@6B@ (retail 0x180156350), ten slots:
//   0  deleting dtor      0x180037eb0  ClipRegionDescription.h (the class's own)
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
// doors were one-line QUALIFIED delegations; ENDGAME B.3.9 UI-A retired them
// with the minted table (the class is concrete, ClipRegionDescription.h).
//
// ----------------------------------------------------------------------------
// THE CONSTRUCTOR IS INLINED INTO Construct / Reconstruct / both factory steps
// ----------------------------------------------------------------------------
// Each open-codes the same store sequence. ENDGAME B.3.9 UI-A: that sequence
// is the real ctor now (ClipRegionDescription.h), and all four sites are
// `::new (p) ClipRegionDescription()`.
// (`??0ClipRegionDescription@UI@@QEAA@XZ` DOES exist and IS exported —
// 0x1800cc140, B36_SLOTS.md sections 6/7 — the old banner here said it did
// not. Nothing calls it from the tree.) The sequence, 0x180037cfa..0x180037d78:
// the base stores ElementDescription() makes (`+0x40` as ONE QWORD holding 3,
// `+0x38` / `+0x3c` two separate DWORDs 0 and -1), then the class's own three.
// The dead Base / External / ElementDescription stamps are the compiler's.
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

}  // namespace

namespace UI {

int32_t ClipRegionDescription::GetFileVersion() const { return 0x104; }

// 0x180037ce0: the inlined constructor (the null test at 0x180037cf4 sat in
// the retired door; a virtual call always has a `this`).
Object::External* ClipRegionDescription::Construct() {
    return ::new (this) ClipRegionDescription();            // 180037cfa..180037d78
}
// 0x180037c30: slot 0 with flag 0, then the same inlined ctor. No null test.
Object::External* ClipRegionDescription::Reconstruct() {
    this->~ClipRegionDescription();                                        // 180037c47
    return ::new (this) ClipRegionDescription();
}
void ClipRegionDescription::Destruct() { this->~ClipRegionDescription(); }

// ============================================================================
// ?Serialize@ClipRegionDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800cc010
// ============================================================================
// The base half is QUALIFIED; then the two extents through the INT slot and
// the one child description. No array arm (unlike its siblings).
bool ClipRegionDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    ElementDescription::Serialize(stream);                  // 1800cc02a

    Serializer ser = { stream, nullptr };
    IO::ReaderWriter* rw = MakeReaderWriter(stream, &ser);

    rw->SerializeInt(_width, nullptr);                      // 1800cc06e
    rw->SerializeInt(_height, nullptr);                     // 1800cc083

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x180038f90ull)(&_childDesc, &ser);  // 1800cc092

    if (rw != nullptr) delete rw;                 // 1800cc0aa
    return true;
}

// ============================================================================
// ?CreateInstance@ClipRegionDescription@UI@@...   retail 0x1800cbec0   (slot 8)
// ============================================================================
// RCX = this, RDX = the hidden return slot, R8 = Dialog* (RULE B6-2).
//
// ClipRegionElement is 0xd8 and its constructor is INLINED here (the real
// ctor in ClipRegionElement.h makes the same stores), with THREE
// stores whose widths are not uniform and one of which is UNALIGNED:
//     1800cbf42  MOV dword [RDI+0xc0],0x64
//     1800cbf4c  MOV qword [RDI+0xc4],0x64     <- UNALIGNED, covers +0xc4/+0xc8
//     1800cbf59  MOV qword [RDI+0xcc],0        <- UNALIGNED, covers +0xcc/+0xd0
//     1800cbf60  MOV dword [RDI+0xd4],0
// The two overlapping qwords cover exactly the six int fields the ctor sets.
//
// THE NULL-ALLOCATION ARM CLEARS ONLY THE LOW 32 BITS (`XOR EAX,EAX ;
// MOV EDI,EAX` at 0x1800cbf68) — which zeroes all 64 on x64, so it is the same
// null. Noted because the asymmetry with the other members of this batch
// (`MOV RBX,R12`) is real in the bytes and is not a difference in behaviour.
Object::InternalPtr<Element>* ClipRegionDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    void* raw = MemAlloc(0xd8, 8);                          // 1800cbf02

    ClipRegionElement* elem = nullptr;
    if (raw != nullptr)                                     // 1800cbf2d
        elem = ::new (raw) ClipRegionElement();             // 1800cbf32 .. 1800cbf60 (inlined ctor)

    void* slot = static_cast<Element*>(elem);               // 1800cbf6c
    UI::ElementDescription::CreateInstance(                   // 1800cbf79
        reinterpret_cast<Object::InternalPtr<Element>*>(&slot));  // B.3.6 bridge

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
// ??1ClipRegionDescription@UI@@UEAA@XZ   0x1800cc0d0
// ============================================================================
// Retail's body releases nothing of its own: restamp, restamp the base, the
// base's `_textName` (+0x60), then the tail JMP into ~External (0x1800cc12c).
// ENDGAME B.3.9 UI-A: the body is EMPTY and the compiler chains
// ~ElementDescription (the +0x60 release) and ~External; the retired door did
// the same by hand.
namespace UI {
ClipRegionDescription::~ClipRegionDescription() {}

// ?CreateNewObject@ClipRegionDescription@UI@@SAPEAVBase@Object@@XZ  0x180037a80
// The FACTUI1 row this replaces: `?MemAlloc@MemoryAllocator@System@@`
// (0x1800c7a50) of 0x78 bytes, align 8; retail's null branch; then the
// inlined constructor.
Object::Base* ClipRegionDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ClipRegionDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) ClipRegionDescription();
}
// ?CreateInPlaceObject@ClipRegionDescription@UI@@SAPEAVBase@Object@@PEAX@Z  0x180037b80
Object::Base* ClipRegionDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) ClipRegionDescription();
}
}  // namespace UI

// ============================================================================
// Registration. ENDGAME B.3.9 UI-A: the eight slot doors and the ??1 door are
// gone -- their only reader was the minted `??_7ClipRegionDescription@UI@@6B@`
// (and its RT_DELDTOR row). The factory pair (B39_RULES D2) is the class's own.
// ============================================================================
int RtUiClipRegionDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?CreateNewObject@ClipRegionDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::ClipRegionDescription::CreateNewObject);        ++n;
    RegisterRuntimeAddr(0x180037a80ull, (void*)&UI::ClipRegionDescription::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@ClipRegionDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::ClipRegionDescription::CreateInPlaceObject);    ++n;
    RegisterRuntimeAddr(0x180037b80ull, (void*)&UI::ClipRegionDescription::CreateInPlaceObject);
    return n;
}

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::ClipRegionDescription's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@ClipRegionDescription@UI@@ (0x1801f9d30): name "ClipRegionDescription", parent
// "ElementDescription" (retail ?_ParentTypeName@ClipRegionDescription@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::ClipRegionDescription, "ElementDescription", nullptr)
