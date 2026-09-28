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

// ============================================================================
// ENDGAME B.3.1 step 1 — the real class, `System::Filename`
// ============================================================================
//
// The POD above stays trivially destructible until its last user is converted,
// for the reason String.h gives. The class below is the same eight bytes under
// retail's name, carrying the bodies runtime/src/system/filename.cpp already
// had (plus GetPathDirectory, moved out of runtime/src/system/setupdir_helpers.cpp).
//
// THE CAST TO String IS EXPLICIT AND IT IS THE ONLY ONE. Retail passes a
// `Filename*` straight into String's members (Filename.h's three pins above),
// so the class exposes exactly that as `AsString()`. It is not a downcast and
// not a type pun of convenience: it is the conversion the retail bodies
// perform, spelled once, in one place, so the bodies can keep calling String's
// members the way retail's do.
// ============================================================================

namespace System {

class Filename {
public:
    // ---- lifecycle ---------------------------------------------------------
    Filename();                                      // ??0Filename@System@@QEAA@XZ            0x180006220
    Filename(const char* src);                       // ??0Filename@System@@QEAA@PEBD@Z        0x180026cf0
    Filename(const String& src);                     // ??0Filename@System@@QEAA@AEBVString@1@@Z 0x18000c780
    Filename(const Filename& other);                 // ??0Filename@System@@QEAA@AEBV01@@Z     0x1800067d0
    // Not retail's; a pure pointer steal, as String's is. The source was
    // already normalised by its own construction, so the AdjustDelimiters the
    // copy ctor runs would be a no-op on it.
    Filename(Filename&& other) noexcept;
    ~Filename();                                     // ??1Filename@System@@QEAA@XZ            0x180006980

    Filename& operator=(const Filename& rhs);        // ??4Filename@System@@QEAAAEAV01@AEBV01@@Z     0x180006770
    Filename& operator=(const String& rhs);          // ??4Filename@System@@QEAAAEAV01@AEBVString@1@@Z 0x180026d40
    Filename& operator=(Filename&& rhs) noexcept;    // ours, as above

    // ---- comparison --------------------------------------------------------
    // Case-INSENSITIVE, unlike String::operator<. That is the whole reason the
    // class has its own overload.
    bool operator<(const Filename& other) const;     // ??MFilename@System@@QEBA_NAEBV01@@Z    0x180026cb0

    // ---- path decomposition ------------------------------------------------
    Filename GetPathFilename(bool stripExt) const;   // ?GetPathFilename@..@QEBA?AV12@_N@Z     0x1800c6740
    Filename GetPathIdentifer() const;               // ?GetPathIdentifer@..@QEBA?AV12@XZ      0x1800c6610  (retail's typo)
    Filename GetPathDirectory() const;               // ?GetPathDirectory@..@QEBA?AV12@XZ      0x1800c6880
    Filename GetPathExtension() const;               // ?GetPathExtension@..@QEBA?AV12@XZ      0x1800c64f0

    int GetLength() const { return AsString().GetLength(); }

    // The conversion retail performs by pointer cast. See the header note.
    String&       AsString()       { return reinterpret_cast<String&>(*this); }
    const String& AsString() const { return reinterpret_cast<const String&>(*this); }

private:
    // PRIVATE in retail too (AEAA, not QEAA — the QEAA spelling in circulation
    // is not an export). Every mutating entry point ends in it.
    void AdjustDelimiters();                         // ?AdjustDelimiters@..@AEAAXXZ           0x1800c6970

    friend struct FilenamePrivateAccess;

public:
    char* _data;   // +0x00 — identical to String::_data; public for the reason String.h gives
};
static_assert(sizeof(Filename) == 8, "System::Filename must be 8 bytes");
static_assert(sizeof(Filename) == sizeof(String),
              "Filename and String must be layout-identical: retail casts between them");

// TRANSITIONAL: dies with the step-1 shims.
struct FilenamePrivateAccess {
    static void AdjustDelimiters(Filename& f) { f.AdjustDelimiters(); }
};

}  // namespace System

// ==== B3R W0-P0c: Runtime bodies reached by retail address ====
// ENDGAME B.3R (B3R_RULES R5): each body below is registered at the retail
// address in its trailing comment and read through the registry today. It is
// declared here, copied from its definition (linkage, namespace, types), so the
// reader can call it by name. Nothing calls it through this declaration yet.

namespace RtSystem {
// ---- system/filename.cpp
bool RtSystem_Filename_Less(const Filename* self, const Filename* other);  // 0x180026cb0
}  // namespace RtSystem
// ==== end B3R W0-P0c ====
