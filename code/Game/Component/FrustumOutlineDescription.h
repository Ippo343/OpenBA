#pragma once
// ============================================================================
// code/Game/Component/FrustumOutlineDescription.h — ENDGAME B.3.8 block VIS
// (handed over by block K1)
//
// `FrustumOutlineDescription` as a real C++ class with a compiler-generated
// vtable (B38_RULES.md D1-D5, §2.2), a subclass of the Runtime's real
// `UI::ElementDescription` (block RB).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "FrustumOutlineDescription", parent "ElementDescription", ti 0x1401c5168
//   create 0x1400122c0: MemAlloc(0x80, 8) + the ctor (0x14006c520):
//                       `??0ElementDescription@UI@@`, stamp 0x14015c300,
//                       +0x68 = 0, +0x70 = String(), +0x78 (qword) = -1
//   createInPlace 0x140012330 (= slot 9, Construct)
//   table 0x14015c300, 10 slots:
//     [0] 0x140012500 scalar deleting dtor (~String +0x70, ~ElementDescription, MemFree on &1)
//     [1] 0x1400122b0 TypeName   [2] 0x1400122a0 Type
//     [3] 0x14006c500 GetFileVersion = ElementDescription's | 0x100
//     [4] External::Compile (import, inherited)
//     [5] 0x14006c460 Serialize (base, ExternalPtr +0x68, String +0x70, int +0x78, int +0x7c; returns 1)
//     [6] 0x140012390 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14006c340 CreateInstance — builds the FrustumOutlineElement (RETSLOT)
//     [9] 0x140012330 Construct (folded with createInPlace)
//   sizeof 0x80.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "system/String.h"
#include "ui/ElementDescription.h"
#include "Object/exe_type.h"

class FrustumOutlineDescription final : public UI::ElementDescription {
    DECLARE_EXE_TYPE(FrustumOutlineDescription)

public:
    FrustumOutlineDescription();                                         // 0x14006c520

    // ~String(+0x70), ~ElementDescription
    ~FrustumOutlineDescription() override = default;                     // [0] 0x140012500
    int32_t           GetFileVersion() const override;                   // [3] 0x14006c500
    bool              Serialize(void* stream) override;                  // [5] 0x14006c460
    Object::External* Reconstruct() override;                            // [6] 0x140012390
    void              Destruct() override;                               // [7] 0x140001640
    Object::InternalPtr<UI::Element>* CreateInstance(
        Object::InternalPtr<UI::Element>* ret,
        const UI::Dialog* dialog) const override;                        // [8] 0x14006c340 RETSLOT
    virtual Object::External* Construct();                               // [9] 0x140012330

    // The non-virtual applier the slot-8 override hides (ElementDescription.h).
    using UI::ElementDescription::CreateInstance;

    // PUBLIC while the ported bodies read them (B38 decision 1).
    void*          _spriteSheet;   // +0x68 ExternalPtr<UI::SpriteSheet>
    System::String _spriteName;    // +0x70
    int32_t        _width;         // +0x78 (ctor -1)
    int32_t        _height;        // +0x7c (ctor -1)
};

static_assert(sizeof(FrustumOutlineDescription) == 0x80, "FrustumOutlineDescription is 0x80 (create 0x1400122c0)");
static_assert(alignof(FrustumOutlineDescription) == 8, "FrustumOutlineDescription alignment");
static_assert(offsetof(FrustumOutlineDescription, _spriteSheet) == 0x68, "+0x68 ExternalPtr");
static_assert(offsetof(FrustumOutlineDescription, _spriteName) == 0x70, "+0x70 String");
static_assert(offsetof(FrustumOutlineDescription, _width) == 0x78, "+0x78 int");
static_assert(offsetof(FrustumOutlineDescription, _height) == 0x7c, "+0x7c int");
