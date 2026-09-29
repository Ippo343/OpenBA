// ============================================================================
// runtime/src/system/filename.cpp — System::Filename   (Wave 2, lane A)
// ============================================================================
//
// Byte-faithful port of the decompilation at the retail address named above
// each function (Runtime-x64.dll, image base 0x180000000, read 2026-09-06).
//
// Filename is String plus one thing: every mutating entry point ends in
// AdjustDelimiters, which rewrites '/' to '\\' in place and then collapses
// "\..\" segments. runtime/types/system/Filename.h shows the three pins for
// the shared layout; string.h/string.cpp own the refcount idioms this file
// reuses by pointer cast, exactly as retail does.
//
// ABI: same hidden-return-slot rule as String — see the header comment of
// string.cpp. GetPathFilename and GetPathIdentifer both return a Filename BY
// MEMORY, so their signature is (this in RCX, ret in RDX, args from R8).
//
// DIFFABILITY: the ctors, the dtor, the two operator= overloads and
// AdjustDelimiters all mutate a live object and a shared refcount, so running
// the original as an oracle would mutate twice and, at refcount 1, double
// free. They are undiffed. operator<, GetPathFilename and GetPathIdentifer are
// const on `this` and produce independent results, so they are diffed.
// ============================================================================

// ============================================================================
// ENDGAME B.3.1 step 1: the bodies below now live on `System::Filename`
// (runtime/types/system/Filename.h), together with GetPathDirectory, whose body
// moved here out of setupdir_helpers.cpp. They MOVED — they were not rewritten.
// The `RtSystem_Filename_*` free functions are one-line shims at the end of the
// file and die in step 2 (B31_RULES.md).
// ============================================================================

#include <cstdio>
#include <cstring>
#include <new>

#include "../../types/system/Filename.h"
#include "string.h"

namespace RtSystem {

// The transitional bridge from the POD the tree still uses to the class; one
// cast, over two types that ARE the same bytes. It goes with the POD.
static inline System::Filename*       AsSys(Filename* f) {
    return reinterpret_cast<System::Filename*>(f);
}
static inline const System::Filename* AsSys(const Filename* f) {
    return reinterpret_cast<const System::Filename*>(f);
}

}  // namespace RtSystem

// ############################################################################
// System::Filename — the class (ENDGAME B.3.1 step 1)
// ############################################################################
// `Filename::GetPathExtension`'s body lives in external_path_win.cpp; see the
// comment on that member below.
namespace RtObject {
RtSystem::Filename* RtSystem_Filename_GetPathExtension(const RtSystem::Filename* self,
                                                       RtSystem::Filename* ret);
}

namespace System {

using RtSystem::StringLength;

// ============================================================
// Constructors / destructor
// ============================================================

// ??0Filename@System@@QEAA@XZ  retail 0x180006220
Filename::Filename() : _data(nullptr) {}

// ??0Filename@System@@QEAA@PEBD@Z  retail 0x180026cf0
Filename::Filename(const char* src) : _data(nullptr) {
    AsString().Set(src);
    AdjustDelimiters();
}

// ??0Filename@System@@QEAA@AEBVString@1@@Z  retail 0x18000c780
// Adopts the String's buffer (no copy) and then normalises. Note that
// AdjustDelimiters can WRITE THROUGH that shared buffer — the '/'->'\\' pass
// is in place — so constructing a Filename from a String mutates the String's
// characters too when the path contained forward slashes. That is retail's
// behaviour and several call sites depend on the resulting Filename and its
// source String staying identical.
Filename::Filename(const String& src) : _data(nullptr) {
    StringPrivateAccess::Adopt(AsString(), src._data);
    AdjustDelimiters();
}

// ??0Filename@System@@QEAA@AEBV01@@Z  retail 0x1800067d0
// Not in this lane's assigned list, but GetPathIdentifer calls it, so it is
// ported (and registered — it is a Filename ctor, and leaving one of the four
// on the DLL is exactly the half-ported class PLAN.md §4.1.5 warns about).
// Retail zeroes, then inlines operator=(const Filename&) — which already ends
// in AdjustDelimiters — and then calls AdjustDelimiters a SECOND time. The
// second call is idempotent, but it is transcribed rather than folded away.
Filename::Filename(const Filename& other) : _data(nullptr) {
    if (_data != other._data) {
        StringPrivateAccess::Release(AsString());
        _data = other._data;
        if (_data != nullptr)
            ++RtSystem::StringRefCount(_data);
    }
    AdjustDelimiters();
    AdjustDelimiters();
}

// Not retail's: a pure pointer steal (B31_RULES class rule 4). The source was
// normalised when it was built, so the AdjustDelimiters the copy ctor runs
// would find nothing to do.
Filename::Filename(Filename&& other) noexcept : _data(other._data) {
    other._data = nullptr;
}

// ??1Filename@System@@QEAA@XZ  retail 0x180006980
Filename::~Filename() {
    StringPrivateAccess::Release(AsString());
}

// ============================================================
// Assignment
// ============================================================

// ??4Filename@System@@QEAAAEAV01@AEBV01@@Z  retail 0x180006770
Filename& Filename::operator=(const Filename& rhs) {
    if (_data != rhs._data) {
        StringPrivateAccess::Release(AsString());
        _data = rhs._data;
        if (_data != nullptr)
            ++RtSystem::StringRefCount(_data);
    }
    AdjustDelimiters();
    return *this;
}

// ??4Filename@System@@QEAAAEAV01@AEBVString@1@@Z  retail 0x180026d40
// Byte-identical to the Filename& overload — the two are separate exports of
// the same code shape, not a forwarder.
Filename& Filename::operator=(const String& rhs) {
    if (_data != rhs._data) {
        StringPrivateAccess::Release(AsString());
        _data = rhs._data;
        if (_data != nullptr)
            ++RtSystem::StringRefCount(_data);
    }
    AdjustDelimiters();
    return *this;
}

// Not retail's: the pointer steal, self-move short-circuited as above.
Filename& Filename::operator=(Filename&& rhs) noexcept {
    if (this != &rhs) {
        StringPrivateAccess::Release(AsString());
        _data = rhs._data;
        rhs._data = nullptr;
    }
    return *this;
}

// ============================================================
// operator< — diffable
// ============================================================

// ??MFilename@System@@QEBA_NAEBV01@@Z  retail 0x180026cb0
// Case-INSENSITIVE, unlike String::operator< which is case-sensitive; that is
// the whole reason Filename has its own overload, and it is what makes
// Collection::Set<Filename> treat "Terrain\Grass.rsc" and "terrain\grass.rsc"
// as one key.
//
// Retail calls the CRT's _stricmp. That is locale-sensitive in MSVC; the DLL
// never calls setlocale, so it runs in the "C" locale where _stricmp is plain
// ASCII case folding. Spelled out here rather than calling _stricmp so that
// runtime/src stays free of a CRT whose locale behaviour differs between
// toolchains (PLAN.md §4.1: the portability seam). Only the SIGN is used.
static int StriCmpAscii(const char* a, const char* b) {
    for (;;) {
        unsigned char ca = static_cast<unsigned char>(*a++);
        unsigned char cb = static_cast<unsigned char>(*b++);
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<unsigned char>(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<unsigned char>(cb + 32);
        if (ca != cb)
            return static_cast<int>(ca) - static_cast<int>(cb);
        if (ca == 0)
            return 0;
    }
}

bool Filename::operator<(const Filename& other) const {
    bool ours;
    if (other._data == nullptr)
        ours = false;
    else if (_data == nullptr)
        ours = true;
    else
        ours = StriCmpAscii(_data, other._data) < 0;

    return ours;
}

// ============================================================
// AdjustDelimiters — NOT DIFFED (rewrites the shared buffer in place)
// ============================================================

// ?AdjustDelimiters@Filename@System@@AEAAXXZ  retail 0x1800c6970
//
// PRIVATE (AEAA), not QEAA — the `...@QEAAXXZ` spelling in circulation is not
// an export and resolves to null forever (the failure mode of WAVE1 defect 3).
// It is exported anyway, as everything in this DLL is, so it is registered.
//
// Two phases:
//   1. Every '/' becomes '\\', IN PLACE, through the shared buffer.
//   2. Repeatedly find a "\.." whose ".." is not the tail, and splice out the
//      preceding segment: head = [0 .. lastDelim-1], tail = [i+3 .. len-1].
//
// Two details that read as bugs and are not:
//   * `lastDelim` is initialised ONCE, at 0x1800c69b5, ABOVE the outer loop
//     label at 0x1800c69c0. Every re-scan after a splice therefore starts with
//     the PREVIOUS scan's delimiter index, not -1. Transcribed as written.
//   * Each splice both recurses into AdjustDelimiters and then continues the
//     outer loop. The recursion normally finishes the job and the outer
//     re-scan finds nothing, but the loop is what retail wrote.
// GetLength is re-read from the object on every iteration because the splice
// replaces the buffer.
void Filename::AdjustDelimiters() {
    // Phase 1: '/' -> '\\'
    {
        int i = 0;
        int64_t k = 0;
        int len = AsString().GetLength();
        if (len > 0) {
            do {
                if (_data[k] == '/')
                    _data[k] = '\\';
                ++i;
                ++k;
                len = AsString().GetLength();
            } while (i < len);
        }
    }

    int lastDelim = -1;   // see the note above: NOT reset by the outer loop

    for (;;) {
        int i = 0;
        int64_t k = 0;
        if (AsString().GetLength() - 2 <= 0)
            return;

        const char* base = _data;   // cached once, as retail caches RBP
        bool collapse = false;

        for (;;) {
            bool isDelim = false;
            if (base[k] == '\\') {
                if (base[k + 1] == '.' && base[k + 2] == '.' && lastDelim >= 1) {
                    if (i + 3 < AsString().GetLength() - 1) {
                        collapse = true;
                        break;
                    }
                    // ".." runs to the end of the path: nothing to splice, so
                    // this backslash is treated as an ordinary delimiter.
                }
                isDelim = true;
            }
            if (isDelim)
                lastDelim = i;
            ++i;
            ++k;
            if (i >= AsString().GetLength() - 2)
                return;
        }
        if (!collapse)
            return;

        String tail    = AsString().SubString(i + 3, AsString().GetLength() - 1);
        String head    = AsString().SubString(0, lastDelim - 1);
        String spliced = head + tail;
        AsString() = spliced;
        AdjustDelimiters();
        // The three explicit releases retail wrote out are the three locals'
        // destructors, and they run in retail's order (spliced, head, tail)
        // because the declarations are in the reverse of it.
    }
}

// ============================================================
// Path decomposition — diffable
// ============================================================

// Both of these produce a fresh Filename with its own buffer and touch nothing
// shared.

// ?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z  retail 0x1800c6740
// Returns the leaf, with the extension stripped when `stripExt` is true.
//
// The `stripExt` flag is misleadingly named by its own logic: when NO '.' is
// found at index >= 1, a true flag returns the EMPTY filename rather than the
// whole leaf. So `GetPathFilename(true)` on an extension-less path yields
// nothing at all.
Filename Filename::GetPathFilename(bool stripExt) const {
    Filename ret;

    const char* d = _data;
    if (d != nullptr) {
        int end = StringLength(d) - 1;

        // Scan back for the extension dot; i == -1 means none was found.
        int i = end;
        int64_t k = end;
        if (end >= 0) {
            do {
                if (d[k] == '.')
                    break;
                --i;
                --k;
            } while (k >= 0);
        }
        bool done = false;
        if (i < 1) {
            if (stripExt)
                done = true;         // the NULL Filename; see the comment above
        } else {
            end = i - 1;
        }

        if (!done) {
            // Scan back for the last path delimiter; either separator counts,
            // even though phase 1 of AdjustDelimiters should have removed '/'.
            i = end;
            k = end;
            if (end >= 0) {
                do {
                    if (d[k] == '/' || d[k] == '\\')
                        break;
                    --i;
                    --k;
                } while (k >= 0);
            }
            int start = 0;
            if (i >= 0)
                start = i + 1;

            if (end >= start) {
                String tmp = AsString().SubString(start, end);
                StringPrivateAccess::Adopt(ret.AsString(), tmp._data);
                ret.AdjustDelimiters();
            }
        }
    }

    return ret;
}

// ?GetPathIdentifer@Filename@System@@QEBA?AV12@XZ  retail 0x1800c6610
// (the misspelling is retail's own). NOTE the return type: V12 = FILENAME.
// The `...QEBA?AVString@2@XZ` spelling in circulation is not an export.
//
// Returns everything after the last ':' — i.e. the path with its drive or
// package identifier removed. If there is no ':' at all the whole path comes
// back UNLESS a '.', '/' or '\\' was seen on the way, in which case the result
// is empty: an identifier is a bare name, and anything that looks like a path
// or a file with an extension has no identifier.
Filename Filename::GetPathIdentifer() const {
    Filename ret;

    const char* d = _data;
    if (d != nullptr) {
        bool sawPathChar = false;
        int i = StringLength(d) - 1;
        int64_t k = i;
        if (i >= 0) {
            do {
                const char c = d[k];
                if (c == ':')
                    break;
                bool isPathChar;
                if (static_cast<unsigned char>(c - 0x2e) < 2)   // '.' (0x2e) or '/' (0x2f)
                    isPathChar = true;
                else
                    isPathChar = (c == '\\');
                sawPathChar = sawPathChar || isPathChar;
                --i;
                --k;
            } while (k >= 0);
        }

        if (i < 0) {
            if (!sawPathChar)
                ret = Filename(*this);   // retail's copy ctor, twin AdjustDelimiters and all
        } else {
            const int len = d ? StringLength(d) : 0;
            String tmp = AsString().SubString(i + 1, len - 1);
            StringPrivateAccess::Adopt(ret.AsString(), tmp._data);
            ret.AdjustDelimiters();
        }
    }

    return ret;
}

// ============================================================================
// ?GetPathDirectory@Filename@System@@QEBA?AV12@XZ          retail 0x1800c6880
// ============================================================================
// MOVED HERE from runtime/src/system/setupdir_helpers.cpp (ENDGAME B.3.1 step 1
// — that file kept it out of filename.cpp only because Wave 2 lane A owned this
// file and two lanes must not edit one source; that reason is gone). The
// `RtSystem_Filename_GetPathDirectory` name and its registry rows stay there,
// now as a shim.
//
// "Everything up to and INCLUDING the last delimiter", or the null Filename if
// there is no delimiter (and for a null or empty `this`).
//
// The trailing delimiter is kept, and that is load-bearing: SetupDirectories
// immediately does `root + "cmd.txt"` and `Format("%s%s%s\\", root, ...)` with
// no separator of its own, so a GetPathDirectory that stripped the '\' would
// silently produce "C:\GOG Games\Banishedcmd.txt". It is kept because SubString
// takes an INCLUSIVE END, not a count, and retail passes `end = <index of the
// delimiter>`.
//
// The scan itself is a REVERSE walk from `length - 1` testing 0x2F and 0x5C,
// read from the disassembly (0x1800c68d0-0x1800c68e4); the decompiled C for
// this loop prints the MOVSXD of the index as a zero extension
// (reference_ghidra_c_output_vs_disassembly.md), which would have made an
// empty string walk off the front instead of taking the `JS` at 0x1800c68c6.
Filename Filename::GetPathDirectory() const {
    Filename ret;

    const char* d = _data;
    if (d != nullptr) {
        int i = StringLength(d) - 1;          // `MOV R9D,[RDX-8] ; DEC R9D`
        while (i >= 0) {                      // `MOVSXD RAX,R9D ; JS`
            const unsigned char c = static_cast<unsigned char>(d[i]);
            if (c == '/' || c == '\\') break;
            --i;
        }
        if (i >= 0) {
            String tmp = AsString().SubString(0, i);
            StringPrivateAccess::Adopt(ret.AsString(), tmp._data);
            ret.AdjustDelimiters();
        }
    }

    return ret;
}

// ?GetPathExtension@Filename@System@@QEBA?AV12@XZ          retail 0x1800c64f0
//
// THE BODY IS STILL IN runtime/src/object/external_path_win.cpp and this is a
// forward to it, not a second transcription (a duplicate would be a silently
// dead body — reference_grep_for_the_port_before_writing_it.md). That file
// wraps the body in the `g_pc` path-counter instrumentation its own lane
// reports, and it is not this batch's file; the body comes here when
// `Object::ExternalFactory` is converted.
Filename Filename::GetPathExtension() const {
    RtSystem::Filename raw{nullptr};
    RtObject::RtSystem_Filename_GetPathExtension(
        reinterpret_cast<const RtSystem::Filename*>(this), &raw);
    Filename out;
    out._data = raw.data;   // the C body built the value; take it whole, no refcount edit
    return out;
}

}  // namespace System

// ############################################################################
// ENDGAME B.3.1 step 1 — THE SHIMS. THEY DIE IN STEP 2.
// ############################################################################
// Same contract as string.cpp's shim block: the pre-conversion signature is
// kept exactly (hidden return slot included), the registry rows below register
// THESE, and a shim that produces a value constructs into the caller's raw
// `ret` slot with placement new rather than assigning to it.
// ############################################################################
namespace RtSystem {

Filename* RtSystem_Filename_CtorCopy(Filename* self, const Filename* other) {
    new (AsSys(self)) System::Filename(*AsSys(other));
    return self;
}

void RtSystem_Filename_AdjustDelimiters(Filename* self) {
    System::FilenamePrivateAccess::AdjustDelimiters(*AsSys(self));
}

}  // namespace RtSystem
