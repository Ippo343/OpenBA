#pragma once
// ============================================================================
// runtime/types/system/Filename.h — System::Filename binary layout
// ============================================================================
//
// Wave 2, lane A. Read out of Runtime-x64.dll in Ghidra (base 0x180000000).
//
// Filename IS a String, layout-identically: one pointer to the same
// refcounted character block described in String.h, sharing the same
// allocation header and the same release idiom. Three independent pins:
//
//   * ??0Filename@System@@QEAA@XZ  (0x180006220) writes exactly one qword of
//     zero and returns — the same body as ??0String@System@@QEAA@XZ.
//   * ??0Filename@System@@QEAA@PEBD@Z (0x180026cf0) zeroes the qword and then
//     calls String::Set(this, str) — passing `this` DIRECTLY as a String*,
//     which is only possible if the two share a prefix layout.
//   * ??1Filename@System@@QEAA@XZ (0x180006980) is byte-for-byte the same
//     release sequence as ??1String@System@@QEAA@XZ (0x180006c80).
//
// Whether the original source expressed this as `class Filename : public
// String` or as a String member is unobservable and irrelevant: retail
// converts between them by pointer cast throughout (Filename::operator=(const
// String&) at 0x180026d40 is byte-identical to the Filename& overload).
//
// The one behavioural difference is that every mutating Filename entry point
// ends in a call to the private `?AdjustDelimiters@Filename@System@@AEAAXXZ`
// (0x1800c6970), which rewrites '/' to '\' in place and then collapses
// "\..\" segments. That is the whole of Filename.
//
// NOTE ON THE MANGLED NAMES: two names in circulation for this class do not
// exist in the DLL's export table.
//   * AdjustDelimiters is PRIVATE — `?AdjustDelimiters@Filename@System@@AEAAXXZ`
//     (AEAA), not QEAA.
//   * GetPathIdentifer (retail's own spelling, with the typo) returns a
//     FILENAME, not a String: `?GetPathIdentifer@Filename@System@@QEBA?AV12@XZ`.
//     `...QEBA?AVString@2@XZ` is not an export.
// ============================================================================

#include <cstdint>
#include "String.h"

namespace RtSystem {

struct Filename {
    char* data;   // +0x00 — identical to String::data
};
static_assert(sizeof(Filename) == 8, "System::Filename must be 8 bytes");
static_assert(sizeof(Filename) == sizeof(String),
              "Filename and String must be layout-identical: retail casts between them");

}  // namespace RtSystem
