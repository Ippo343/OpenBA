#pragma once
// ============================================================================
// code/Game/Component/CaptureBoxDescription.h — ENDGAME B.3.8 block VIS
// (handed over by block K1)
//
// `CaptureBoxDescription` (the sprite-capture box of the debug SpriteCaptureUI)
// as a real C++ class with a compiler-generated vtable (B38_RULES.md D1-D5,
// §2.2), a subclass of the Runtime's real `UI::ElementDescription` (block RB).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "CaptureBoxDescription", parent "ElementDescription", ti 0x1401c5600
//   create 0x140013d90: MemAlloc(0x78, 8) + the ctor (0x140079230):
//                       `??0ElementDescription@UI@@`, stamp 0x14015d4a0,
//                       +0x68 = 0, +0x70 = String()
//   createInPlace 0x140013df0 (= slot 9, Construct)
//   table 0x14015d4a0, 10 slots:
//     [0] 0x140014010 scalar deleting dtor (~String +0x70, ~ElementDescription, MemFree on &1)
//     [1] 0x140013d80 TypeName   [2] 0x140013d70 Type
//     [3] 0x140079210 GetFileVersion = ElementDescription's | 0x100
//     [4] External::Compile (import, inherited)
//     [5] 0x1400791a0 Serialize (base, ExternalPtr +0x68, String +0x70; returns 1)
//     [6] 0x140013e40 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x1400790a0 CreateInstance — builds the CaptureBoxElement (RETSLOT)
//     [9] 0x140013df0 Construct (folded with createInPlace)
//   sizeof 0x78.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "system/String.h"
#include "ui/ElementDescription.h"
#include "Object/exe_type.h"

namespace UI { class SpriteSheet; }

class CaptureBoxDescription final : public UI::ElementDescription {
    DECLARE_EXE_TYPE(CaptureBoxDescription)

public:
    CaptureBoxDescription();                                             // 0x140079230

    // ~String(+0x70), ~ElementDescription
    ~CaptureBoxDescription() override = default;                         // [0] 0x140014010
    int32_t           GetFileVersion() const override;                   // [3] 0x140079210
    bool              Serialize(IO::Stream& stream) override;                  // [5] 0x1400791a0
    Object::External* Reconstruct() override;                            // [6] 0x140013e40
    void              Destruct() override;                               // [7] 0x140001640
    Object::InternalPtr<UI::Element>* CreateInstance(
        Object::InternalPtr<UI::Element>* ret,
        const UI::Dialog* dialog) const override;                        // [8] 0x1400790a0 RETSLOT
    virtual Object::External* Construct();                               // [9] 0x140013df0

    // The non-virtual applier the slot-8 override hides (ElementDescription.h).
    using UI::ElementDescription::CreateInstance;

    // PUBLIC while the ported bodies read them (B38 decision 1).
    UI::SpriteSheet* _spriteSheet; // +0x68 ExternalPtr<UI::SpriteSheet>
    System::String _spriteName;    // +0x70
};

static_assert(sizeof(CaptureBoxDescription) == 0x78, "CaptureBoxDescription is 0x78 (create 0x140013d90)");
static_assert(alignof(CaptureBoxDescription) == 8, "CaptureBoxDescription alignment");
static_assert(offsetof(CaptureBoxDescription, _spriteSheet) == 0x68, "+0x68 ExternalPtr");
static_assert(offsetof(CaptureBoxDescription, _spriteName) == 0x70, "+0x70 String");
