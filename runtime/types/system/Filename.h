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
// RULING (B.4 Z MIRROR): in retail, Filename IS-A String (public derivation).
// The export table shows exactly what a derived class must redeclare and
// nothing else: ctors (default 0x180006220, const char* 0x180026cf0, const
// String& 0x18000c780 = the inlined String copy ctor on `this` then
// AdjustDelimiters, copy 0x1800067d0 = String() then String::operator= inlined
// then AdjustDelimiters twice), ~Filename 0x180006980 (the inlined ~String,
// under an EH frame), both operator= overloads, and the comparisons it HIDES
// (==, !=, < case-insensitive). It exports NO accessor for its characters:
// every reader goes through String's own exports with the Filename's address,
// e.g. PlatformExists 0x1800adc70 passes its `const Filename&` straight to
// WideString(const String&) 0x1800cab50 — a derived-to-base conversion.
// IMPLEMENTED by B.4 Z MIRROR2 (see the class below). Filename's own ==, !=
// (retail exports them) are not ported: nothing in the tree compares two
// Filenames for equality (it did not compile before the derivation), and a
// new `f == g` would now bind String's case-sensitive ==, not retail's.
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

// (B.4 acceptance MOD: the `RtSystem::Filename` POD mirror is gone with its
// last user, `IO::Modification::Mod::file`; System::Filename below is the one
// layout.)

// ============================================================================
// ENDGAME B.3.1 step 1 — the real class, `System::Filename`
// ============================================================================
//
// The class below is the eight bytes under retail's name, carrying the bodies
// runtime/src/system/filename.cpp already
// had (plus GetPathDirectory, moved out of runtime/src/system/setupdir_helpers.cpp).
//
// Retail passes a `Filename*` straight into String's members (the three pins
// above): the derived-to-base conversion. `AsString()` names it (no cast since
// B.4 Z MIRROR2) so the bodies can keep calling String's members the way
// retail's do.
// ============================================================================

namespace System {

// B.4 Z MIRROR2 (`behaviour:`): the ruling above, IMPLEMENTED. Filename is a
// String by public derivation, as in retail. Every ctor constructs the String
// base first (each retail ctor opens with the inlined String ctor under an EH
// frame of its own: 0x180006220 / 0x180026cf0 / 0x1800067d0 store the null,
// 0x18000c780 is String's copy ctor inlined on `this`), and ~Filename has no
// body of its own: the release at 0x180006980 (the inlined ~String under an EH
// frame) is ~String run on the base subobject. `AsString()` is now the
// derived-to-base conversion, no cast; GetLength is String's, as retail's
// export table (no Filename::GetLength) says.
// OVERLOAD SURVEY (code runtime standalone dx11 engine tests): the only
// String / Filename overload pairs are Filename's own ctor, operator= and
// operator< (each hides or pairs with String's, binding as retail's exports
// do: a Filename argument takes the Filename overload by exact match, a String
// argument the String one) and the test helper RefCount; no by-value Filename parameter or return changes its
// ABI (Filename was already non-trivial: hidden pointer / hidden return slot).
class Filename : public String {
public:
    // ---- lifecycle ---------------------------------------------------------
    [[gnu::always_inline]] Filename();               // ??0Filename@System@@QEAA@XZ            0x180006220
    Filename(const char* src);                       // ??0Filename@System@@QEAA@PEBD@Z        0x180026cf0
    [[gnu::always_inline]] Filename(const String& src); // ??0Filename@System@@QEAA@AEBVString@1@@Z 0x18000c780
    Filename(const Filename& other);                 // ??0Filename@System@@QEAA@AEBV01@@Z     0x1800067d0
    // Not retail's; a pure pointer steal, as String's is. The source was
    // already normalised by its own construction, so the AdjustDelimiters the
    // copy ctor runs would be a no-op on it.
    Filename(Filename&& other) noexcept;
    [[gnu::always_inline]] ~Filename();              // ??1Filename@System@@QEAA@XZ            0x180006980

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

    // The base subobject: retail hands the Filename's own address to String's
    // members (PlatformExists 0x1800adc70 -> WideString(const String&)).
    String&       AsString()       { return *this; }
    const String& AsString() const { return *this; }

private:
    // PRIVATE in retail too (AEAA, not QEAA — the QEAA spelling in circulation
    // is not an export). Every mutating entry point ends in it.
    void AdjustDelimiters();                         // ?AdjustDelimiters@..@AEAAXXZ           0x1800c6970

    friend struct FilenamePrivateAccess;
    // +0x00: String::_data, the base's one field.
};
// ---- the inline lifecycle (B7 D4) ------------------------------------------
// Retail inlines these at its callers exactly as it inlines String's (GetNextItem
// 0x1800cdf40 builds `wanted` at 1800ce932..1800ce94c: null store, INC, CALL
// AdjustDelimiters; releases it open-coded, no ~Filename call), so they are
// header-defined over String's inline bodies. Bodies unchanged from filename.cpp.

// ??0Filename@System@@QEAA@XZ  retail 0x180006220: String() (one null store).
inline Filename::Filename() : String() {}

// ??0Filename@System@@QEAA@AEBVString@1@@Z  retail 0x18000c780
// 0x18000c780..7a8 is String's copy ctor (null store, AddRef) on `this`, then
// the AdjustDelimiters call. AdjustDelimiters writes through the shared buffer
// (filename.cpp's note): the source String's characters change too.
inline Filename::Filename(const String& src) : String(src) {
    AdjustDelimiters();
}

// Not retail's: a pure pointer steal, as String's is (see the declaration).
inline Filename::Filename(Filename&& other) noexcept : String(static_cast<String&&>(other)) {}

// ??1Filename@System@@QEAA@XZ  retail 0x180006980: no body of its own; the
// release is the base's ~String (0x180006c80, inlined there).
inline Filename::~Filename() {}

static_assert(sizeof(Filename) == 8, "System::Filename must be 8 bytes");
static_assert(sizeof(Filename) == sizeof(String),
              "Filename and String must be layout-identical: retail casts between them");

// TRANSITIONAL: dies with the step-1 shims.
struct FilenamePrivateAccess {
    static void AdjustDelimiters(Filename& f) { f.AdjustDelimiters(); }
};

}  // namespace System

// (B.4 Z LAST IO: `RtSystem_Filename_CtorCopy`, the POD spelling of the copy
// ctor 0x1800067d0, is gone with its last caller.)
