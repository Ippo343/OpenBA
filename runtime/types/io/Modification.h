#pragma once
// ============================================================================
// runtime/types/io/Modification.h — IO::Modification binary layout
// ============================================================================
// Wave 4, lane D. Only the two Modification fields and the one Mod field that
// ?Find@Modification@IO@@QEBAHAEBVFilename@System@@@Z (retail 0x1800aea10)
// touches are modelled. The rest of the 0x80-byte Mod record — display name
// String, enabled flag, registry index — is read (and printed) by
// code/Game/Dialog/mod_dialog_vtables.cpp's ModProbe_DumpModList, which is
// where the 0x80 stride and the +0x58 filename offset were first read; this
// header adds the static_asserts that comment never had.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../system/Filename.h"

namespace RtIO {

// One entry of Modification's mod array. Find() compares `file` against the
// query Filename with a length-checked, case-insensitive string compare.
struct Mod {
    uint8_t           _unknown00[0x58];
    RtSystem::Filename file;   // +0x58 — the mod's .pkm path; Find()'s comparison key
    uint8_t           _unknown60[0x80 - 0x58 - sizeof(RtSystem::Filename)];
};
static_assert(sizeof(Mod) == 0x80,
              "Find() 0x1800aea10: `lVar3 = lVar3 + 0x80` strides the array by this");
static_assert(offsetof(Mod, file) == 0x58,
              "Find() 0x1800aea10: `*(char**)(*(this+8) + 0x58 + lVar3)`");

struct Modification {
    uint8_t  _unknown00[0x08];
    Mod*     mods;    // +0x08 — Find()'s `*(longlong*)(this + 8)`
    int32_t  count;   // +0x10 — Find()'s `*(int*)(this + 0x10)`
};
static_assert(offsetof(Modification, mods)  == 0x08, "see field comment");
static_assert(offsetof(Modification, count) == 0x10, "see field comment");

}  // namespace RtIO
