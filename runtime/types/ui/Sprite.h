#pragma once
// UI::Sprite — binary-compatible struct definition.
// Runtime-x64.dll value type, 0x10 bytes.
//
// A Sprite is a lightweight reference into a SpriteSheet: it holds an
// ExternalPtr<SpriteSheet> (8 bytes) and an int32 sprite index (4 bytes),
// plus 4 bytes of padding to reach 0x10 alignment.
//
// Decompiled from:
//   Ctor (name):    0x18011b9c0 (??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@AEBVString@System@@@Z)
//   Ctor (default): 0x18011ba10 (??0Sprite@UI@@QEAA@XZ)
//   Ctor (index):   0x18011ba30 (??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@H@Z)
//   IsValid:        0x180030c40 (?IsValid@Sprite@UI@@QEBA_NXZ)
//   Width:          0x180030bf0 (?Width@Sprite@UI@@QEBAHXZ)
//   Height:         0x180030c10 (?Height@Sprite@UI@@QEBAHXZ)
//
// RE-VERIFIED (Wave 7 lane F, 2026-09-07): every fact already written in this
// file was checked against a fresh decompile of Runtime-x64.dll and is
// correct, including the pad0C claim below -- confirmed independently by
// SpriteSheet.h's evidence for the entry width/height offsets, which needed
// Width/Height's own bodies to be read anyway.
//
// pad0C IS NEVER WRITTEN by any of the three ctors (only the qword at +0x00
// and the dword at +0x08 are stored). A diff oracle that memcmp's a freshly
// poisoned scratch object against a live one must stop at 0xC bytes, not
// 0x10, or it reports a false divergence on whatever garbage was already in
// the caller's storage -- reference_oracle_must_not_compare_unwritten_bytes.md.
// See RtUi_Sprite_* in runtime/src/ui/sprite.cpp.

#include <cstdint>
#include <cstddef>

// ============================================================
// UI::Sprite (0x10 bytes)
// ============================================================
//
// Construction patterns (from decompilation):
//
// Default ctor (0x18011ba10):
//   *(uint64_t*)(this + 0x00) = 0;       // null sheet
//   *(int32_t*)(this + 0x08)  = -1;      // invalid index
//
// Ctor by name (0x18011b9c0):
//   *(SpriteSheet**)(this + 0x00) = sheet;
//   if (sheet != nullptr)
//       *(int32_t*)(this + 0x08) = SpriteSheet::GetSpriteIndexFromName(sheet, name);
//   else
//       *(int32_t*)(this + 0x08) = -1;
//
// Ctor by index (0x18011ba30):
//   *(uint64_t*)(this + 0x00) = sheet;   // ExternalPtr value
//   *(int32_t*)(this + 0x08)  = index;
//
// IsValid (0x180030c40):
//   return (index >= 0) && (sheet != nullptr);
//
struct Sprite {
    void*       spriteSheet;    // +0x00  ExternalPtr<UI::SpriteSheet> (pointer to SpriteSheet, null = no sheet)
    int32_t     spriteIndex;    // +0x08  index into the SpriteSheet (-1 = invalid/unset)
    int32_t     pad0C;          // +0x0C  padding (not explicitly initialized by ctors)

    // --- Inline accessors matching original logic ---

    // IsValid @ 0x180030c40: checks index >= 0 AND sheet != null
    bool IsValid() const {
        return (spriteIndex >= 0) && (spriteSheet != nullptr);
    }
};

// Bytes a diff oracle may compare between two Sprite objects. pad0C (the last
// 4) is never written by any ctor -- see the note above.
inline constexpr size_t kSpriteWrittenBytes = 0x0C;

static_assert(sizeof(Sprite) == 0x10, "Sprite must be 0x10 bytes");
static_assert(offsetof(Sprite, spriteSheet) == 0x00, "spriteSheet at +0x00");
static_assert(offsetof(Sprite, spriteIndex) == 0x08, "spriteIndex at +0x08");
static_assert(offsetof(Sprite, pad0C)       == 0x0C, "pad0C at +0x0C");
