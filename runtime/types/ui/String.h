#pragma once

// <cstdint> for int32_t, <cstddef> for size_t and offsetof. Added 2026-09-07:
// this header used all three without including either, so it compiled only when
// something else in the translation unit had already pulled the standard headers
// in. Its one includer at the time (runtime/src/ui/sprite.cpp) happened to do so,
// which is why the omission was latent. It stopped being latent when Element.h
// began including this file: a TU that includes String.h FIRST failed with
// "'int32_t' does not name a type", after which UIString silently degraded to an
// 8-byte struct with no `index` and no `pad0C` and every static_assert in both
// headers fired. Found by Wave 7 lane A, which depends on this header.
#include <cstdint>
#include <cstddef>
// UI::String -- binary-compatible struct definition.
// Runtime-x64.dll value type, 0x10 bytes.
//
// A UI::String is a lightweight reference into a StringTable: it holds an
// ExternalPtr<StringTable> (8 bytes) and an int32 text index (4 bytes), plus
// 4 bytes of padding. Byte-for-byte the same shape as UI::Sprite
// (runtime/types/ui/Sprite.h) referencing a SpriteSheet -- expected, since
// both are "handle + index into an owning table" value types, and Element.h's
// independently-recovered local `UiString` (lane A, `runtime/types/ui/
// Element.h`) already pins this same shape from a different angle (SetToolTip
// copies exactly two qwords, no refcount inc/dec).
//
// NAMED `UIString` here (not `String`) to avoid colliding with the global
// System::String port's type at runtime/types/system/String.h
// (`RtSystem::String`) if both headers are ever included from the same file.
//
// Decompiled from (Wave 7 lane F, 2026-09-07, Runtime-x64.dll, image base
// 0x180000000):
//   Ctor (default):    0x18011da40 (??0String@UI@@QEAA@XZ)
//   Ctor (name):       0x18011da60 (??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@AEBV0System@@@Z)
//   Ctor (index):      0x18011da20 (??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@H@Z)
//   IsValid:           0x180035060 (?IsValid@String@UI@@QEBA_NXZ)
//   operator System::String: 0x180035020 (??BString@UI@@QEBA?AV0System@@XZ)
//
// Construction patterns, read off the disassembly (all four are `MOV`/`MOV`
// pairs, no branches beyond the null-table check):
//
// Default ctor (0x18011da40):
//   *(uint64_t*)(this + 0x00) = 0;       // null table
//   *(int32_t*)(this + 0x08)  = -1;      // invalid index
//
// Ctor by name (0x18011da60):
//   *(StringTable**)(this + 0x00) = table;
//   if (table != nullptr)
//       *(int32_t*)(this + 0x08) = StringTable::GetIndexFromName(table, name);
//   else
//       *(int32_t*)(this + 0x08) = -1;
//
// Ctor by index (0x18011da20):
//   *(uint64_t*)(this + 0x00) = table;   // ExternalPtr value
//   *(int32_t*)(this + 0x08)  = index;
//
// IsValid (0x180035060): `(table != 0) && (index >= 0)` -- note the order is
// TABLE-FIRST here, the opposite short-circuit order from Sprite::IsValid
// (index-first). Both produce the same boolean; the order is preserved
// anyway because CLAUDE.md's rule is byte-faithful first.
//
// operator System::String (0x180035020) is a ONE-LINE forwarder:
//   return table->GetText(index);   // StringTable::GetText(int), see StringTable.h
// with `table` read unguarded (this + 0x00) -- if `table` is null, retail
// itself would fault here; there is no null check to preserve or add.
struct UIString {
    void*       table;      // +0x00  ExternalPtr<UI::StringTable> (null = no table)
    int32_t     index;      // +0x08  index into the StringTable (-1 = invalid/unset)
    int32_t     pad0C;      // +0x0C  padding; never written by any of the three ctors

    bool IsValid() const {
        return (table != nullptr) && (index >= 0);
    }
};

static_assert(sizeof(UIString) == 0x10, "UI::String must be 0x10 bytes");
static_assert(offsetof(UIString, table) == 0x00, "table at +0x00");
static_assert(offsetof(UIString, index) == 0x08, "index at +0x08");
static_assert(offsetof(UIString, pad0C) == 0x0C, "pad0C at +0x0C");

// Bytes a diff oracle may compare between two UIString objects -- pad0C is
// never written, same trap as Sprite.h's kSpriteWrittenBytes.
inline constexpr size_t kUIStringWrittenBytes = 0x0C;
