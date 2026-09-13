// ============================================================================
// runtime/src/save/saveload_meta.cpp — Save::SaveLoad::SaveMetaData (Wave 14)
// ============================================================================
//
//   ?SaveMetaData@SaveLoad@Save@@QEAAXPEAXHHVString@System@@H@Z
//   retail 0x1800c1490, ordinal 3611
//   void SaveMetaData(void* pixels, int width, int height,
//                     System::String name /*BY VALUE*/, int nameMode)
//
// WHAT IT IS. The writer half of the save-file metadata record: the small
// header block that the save/load dialog reads to draw a row (thumbnail, name,
// mod list) WITHOUT loading the multi-megabyte game state behind it. It
// serialises that record into a compressed in-memory stream and parks the
// result in `this->_embeddedState` (SaveLoad+0x28), from which
// ?UpdateSave@SaveLoad builds SaveBlock entry 0.
//
// Its reader is ?LoadMetaData@SaveLoad@Save@@ (0x1800c1b30, a parallel Wave 14
// lane). Its ONE caller is ?UpdateSave@SaveLoad@Save@@ (0x1800c2a80), ported by
// Wave 13 in saveload_update.cpp, which resolves it BY NAME — so registering
// the mangled name below is what flips that call site onto this body.
//
// ----------------------------------------------------------------------------
// THE RECORD FORMAT, which falls out of the body in full
// ----------------------------------------------------------------------------
//     String  name            the save's display name (the by-value argument)
//     int32   width           thumbnail width
//     int32   height          thumbnail height
//     bytes   width*height*4  the BGRA thumbnail pixels
//     int32   nameMode        SetSaveName's second argument, carried through
//     int32   modCount        the number of ENABLED mods
//     modCount x {
//         String  name        Mod+0x00, the display name
//         String  path        Mod+0x58, RELATIVE to the data directory
//         uint32  versionMajor  <- Mod+0x2c
//         uint32  gameVersion   <- Mod+0x18
//         uint32  versionMinor  <- Mod+0x1c
//     }
//
// all of it through an IO::Serializer at VERSION 3 (`MOV R8D,0x3` at
// 0x1800c1570) over an IO::CompressedMemoryStream with TWO windows.
//
// *** THE FIELD MAPPING IS CONFIRMED BY ITS OWN READER. *** Wave 5 pinned
// Save::SaveLoad::ModInfo from ?CheckMod@SaveLoad@Save@@ (0x1800c1090), which
// compares ModInfo+0x10 against Mod+0x2c, ModInfo+0x18 against Mod+0x1c, and
// range-checks ModInfo+0x14. This body is the WRITER of exactly those three
// fields and stores exactly Mod+0x2c, Mod+0x18, Mod+0x1c into them, in that
// order. Writer and reader agree field for field; neither was derived from the
// other. (runtime/types/save/SaveLoad.h carries the layout and needs no change.)
//
// *** AND THE PATH FIELD IS THE INVERSE OF UpdateLoad'S. *** The per-mod `path`
// is NOT Mod+0x58 verbatim. Retail takes the length of IO::FileSystem's data
// directory and ADVANCES the mod's Filename pointer by it:
//
//     1800c166d  MOV  RCX,[RAX]           ; dataDir.data (a hidden-retslot return)
//     1800c1675  MOV  EAX,[RCX + -0x8]    ; its LENGTH, from the String header
//     1800c1681  MOVSXD RDX,EAX           ; SIGN-extended
//     1800c1684  ADD  RDX,[RBX + 0x58]    ; + Mod::file.data
//     1800c168c  CALL 0x1800c95d0         ; String::Set(&work, that suffix)
//
// i.e. it STRIPS the data-directory prefix. ?UpdateLoad@SaveLoad (Wave 13,
// saveload_update.cpp:797) does `GetDataDirectory() + modInfo.path` and matches
// the result against Mod+0x58 — putting the prefix back. The two are exact
// inverses, which is the strongest available confirmation that this reading of
// an otherwise anonymous pointer add is right.
//
// ----------------------------------------------------------------------------
// FOUR THINGS THE DECOMPILER GETS WRONG HERE
// ----------------------------------------------------------------------------
// Every call this body makes was counted register by register out of the
// disassembly, per reference_ghidra_c_output_vs_disassembly.md. Four disagree
// with Ghidra's C, and three of the four would have been silent defects:
//
// 1. THE ELEMENT DESTROYER'S FIRST ARGUMENT IS DEAD. Ghidra prints
//    `FUN_1800341e0(pplVar8, plVar9, (int)local_120)` and has to invent
//    `pplVar8` to do it, because RCX genuinely holds different things on the
//    two paths that reach the call (the ReaderWriter, or &array). The callee
//    (0x1800341e0) spills RCX to its home slot at 0x1800341e9 and NEVER READS
//    IT AGAIN — its real signature is (ignored, ModInfo* data, int count).
//    Transcribed here as a two-argument helper, which is what it is.
//
// 2. THREE OF THE FIVE `SerializeUInt` CALLS LOSE THEIR THIRD ARGUMENT.
//    Ghidra prints `(*(code *)(*local_130)[0x13])(local_130, plVar7 + 1)` for
//    two of the three per-mod field writes and a three-argument form for the
//    first. All three are identical in the bytes: RCX, RDX, and `XOR R8D,R8D`
//    at 0x1800c17e4 / 0x1800c17fc / 0x1800c1811. The third argument is the
//    debug NAME (nullptr in retail), which every IO::ReaderWriter slot takes.
//
// 3. `MOV EAX,[RCX + -0x8]` IS A SIGNED LENGTH THAT IS THEN `MOVSXD`-EXTENDED
//    before the pointer add (0x1800c1681, quoted above). Ghidra's C renders it
//    as `(longlong)iVar5`, which happens to be right — but the same pattern is
//    what reference_ghidra_c_output_vs_disassembly.md records being printed as
//    a ZERO extension elsewhere, so it was read from the bytes.
//
// 4. `?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ` RETURNS A
//    CLASS BY VALUE. It is a MEMBER function, so the hidden return slot is RDX
//    and there are no other arguments (`LEA RDX,[RSP+0x40]` at 0x1800c165b).
//    A typedef that omits the slot is a wild write that still returns the right
//    answer (reference_hidden_retslot_wild_write.md);
//    RtSaveDetail::GetDataDirectoryFn already spells it correctly and is reused
//    rather than re-declared.
//
// ----------------------------------------------------------------------------
// THE BY-VALUE `System::String` — WHO DESTROYS IT
// ----------------------------------------------------------------------------
// `VString@System@@` in the mangled name is by VALUE, and a class with a
// non-trivial destructor is passed by hidden reference in the MS x64 ABI. So
// argument 5 arrives as a `String*` in the 5th stack slot (`MOV R15,[RBP+0x80]`
// at 0x1800c1597, where RBP+0x80 is entry-RSP+0x28) and argument 6, the int, is
// at entry-RSP+0x30.
//
// THE CALLEE DESTROYS IT. The last thing this function does before returning is
// the standard String release on `*name` (0x1800c1ab7..0x1800c1b0b) — decrement,
// and on reaching zero rewind the pointer by 8 and free that. Wave 13's
// UpdateSave comment already states the caller's half of this contract: it
// retains the name and does NOT release it, precisely because the callee will.
// The two halves now exist in the tree and agree; releasing here is required,
// not optional, and NOT releasing would leak the save name on every save.
//
// ----------------------------------------------------------------------------
// WHY THERE IS NO RtSink PREDICT BODY IN THIS FILE
// ----------------------------------------------------------------------------
// The five Wave 13 state machines next door are written as predicting bodies
// against runtime/funcs/rt_predict.h. That shape does not fit here and pretending
// otherwise would produce a shadow marker, not an oracle
// (reference_shadow_marker_is_not_an_oracle.md): this body's entire observable
// output is a heap block produced by running a real compressor over a real
// stream, and there is nothing to "predict" short of reimplementing deflate.
//
// It does not need one. This is a WRITE path, and a write path is REPLAYABLE
// (reference_write_path_serialize_is_replayable.md): the function is a pure
// read of its arguments and of two singletons, so it can be run TWICE into TWO
// SINKS and the bytes compared. See RtSaveMetaOracle below — ours runs into the
// caller's real SaveLoad, the shipped DLL runs into a scratch one, and the two
// SavedStates are compared. That is a predictive oracle in the sense that
// matters: THEIRS actually runs, and it never sees our answer.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saveload_internal.h"
#include "../../funcs/rt_port.h"
#include "../../types/io/Stream.h"
#include "../../types/io/Serializer.h"
#include "../../types/io/Modification.h"

using RtSave::ModInfo;
using RtSave::SaveLoad;
using RtSaveDetail::MemAlloc;
using RtSaveDetail::MemFree;
using RtSaveDetail::ReleaseStringSlot;
using RtSystem::String;

namespace {

// ---------------------------------------------------------------------------
// Collection::Array<Save::SaveLoad::ModInfo> — the local this body builds.
// ---------------------------------------------------------------------------
// The standard Wave 2 Collection triple. Spelled locally because the three
// helpers below are the ModInfo INSTANTIATION of Array's members and are
// unexported internals of the DLL; nothing else in the tree reaches them.
struct ModInfoArray {
    ModInfo* data;       // +0x00
    int32_t  size;       // +0x08
    int32_t  capacity;   // +0x0c
};
static_assert(sizeof(ModInfoArray) == 0x10, "Collection::Array header is 16 bytes");

// ---------------------------------------------------------------------------
// 0x1800341e0 — release every element's two Strings over [data, data+count).
// ---------------------------------------------------------------------------
// UNEXPORTED (checked against the shipped export table with pefile, not
// inferred from the `AEAA`-vs-`QEAA` spelling of anything —
// reference_aeaa_is_private_not_unexported.md). It is a template instantiation
// the linker folded in, so there is no name to register; ported as a file-local
// static rather than reached through an address door, which keeps this file
// free of by-address calls that would go null at cutover
// (reference_by_address_door_dies_at_cutover.md).
//
// ORDER MATTERS AND IS TRANSCRIBED: +0x08 (path) is released BEFORE +0x00
// (name) — 0x180034217 reads [RBX+8], 0x180034270 reads [RBX]. The refcounts
// are independent so the order is invisible today; it is written the way the
// bytes have it so that a future reader diffing against retail sees a match.
//
// The `if (count <= 0) return` is retail's own first instruction
// (`TEST R8D,R8D ; JLE`), not a defensive guard added here.
void ModInfoDestroyRange(ModInfo* data, int32_t count) {
    if (count <= 0) return;
    for (int32_t i = 0; i < count; ++i) {
        ReleaseStringSlot(&data[i].path);
        ReleaseStringSlot(&data[i].name);
    }
}

// ---------------------------------------------------------------------------
// 0x180033080 — Array<ModInfo>::SetSize(int newSize, const ModInfo& fill)
// ---------------------------------------------------------------------------
// Grow/shrink, copy-constructing any NEW element from `fill`. This is the
// push_back the mod loop uses: SetSize(size + 1, &tmp).
//
// THE CAPACITY RULE IS NOT `capacity = newSize`. When the request is exactly
// one more than the current size AND the array is not empty, retail jumps the
// capacity to newSize*3 (`LEA EAX,[RDI + RDI*0x2]` at 0x1800330ce) — the
// amortised-growth arm. A plain `capacity = newSize` would reallocate on every
// single push and still be functionally correct, which is exactly why it has to
// be transcribed rather than simplified: the difference is invisible in output
// and visible in every allocation counter.
//
// The `dst == nullptr` test inside the loop is retail's (`ADD RCX,R9` at
// 0x180033113 setting ZF, then `JZ` at 0x18003311b). It can only fire when the
// allocation failed and i == 0; it is retail's behaviour, not a null guard this
// port invented (CLAUDE.md).
//
// NOTE WHAT IS *NOT* COPIED: ModInfo+0x1c. The element copy writes +0x00, +0x08,
// +0x10, +0x14 and +0x18 and stops (0x18003314c..0x18003315b). ModInfo::pad1c
// therefore holds allocator garbage in every element of this array, in retail
// as here — and it is never serialised, which is why that is harmless.
void ModInfoArraySetSize(ModInfoArray* a, int32_t newSize, const ModInfo* fill) {
    const int32_t oldSize = a->size;
    if (newSize == oldSize) return;

    ModInfo* oldData = a->data;
    a->size = newSize;

    if (newSize > a->capacity) {
        a->capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0) a->capacity = newSize * 3;
        a->data = static_cast<ModInfo*>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->capacity)) * 0x20, 8));
    }

    // `CMOVZ ECX,ESI` at 0x1800330f2: when the buffer did NOT move, the
    // elements below oldSize are already in place and the copy starts at
    // oldSize; when it moved, everything is copied from index 0.
    const int32_t start = (oldData == a->data) ? oldSize : 0;
    for (int32_t i = start; i < newSize; ++i) {
        ModInfo* dst = a->data + i;
        if (dst == nullptr) continue;
        const ModInfo* src = (i < oldSize) ? (oldData + i) : fill;

        dst->name = nullptr;
        if (src->name != nullptr) {
            dst->name = src->name;
            RtSystem::StringRefCount(const_cast<char*>(src->name)) += 1;
        }
        dst->path = nullptr;
        if (src->path != nullptr) {
            dst->path = src->path;
            RtSystem::StringRefCount(const_cast<char*>(src->path)) += 1;
        }
        dst->versionMajor = src->versionMajor;
        dst->gameVersion  = src->gameVersion;
        dst->versionMinor = src->versionMinor;
    }

    if (oldData != a->data) {
        ModInfoDestroyRange(oldData, oldSize);
        if (oldData != nullptr) MemFree(oldData);
    } else if (oldSize > newSize) {
        ModInfoDestroyRange(oldData + newSize, oldSize - newSize);
    }
}

// ---------------------------------------------------------------------------
// 0x180034010 — Array<ModInfo>::SetSize(int newSize)
// ---------------------------------------------------------------------------
// The same function with no `fill`: new elements get two NULL Strings and their
// three ints are left untouched (0x1800340ff/0x180034102 are the only stores on
// that arm).
//
// *** THIS IS PROVABLY DEAD ON THIS PATH AND IS PORTED ANYWAY. *** Its only
// call site is guarded by `serializer.stream->direction == 0`, i.e. READING,
// and this body hard-codes direction = 1 twenty instructions after constructing
// the stream. It is the read arm of a Serialize routine that retail compiles
// once and uses in both directions. Transcribed rather than dropped, because
// "the branch cannot be taken" is a claim about today's single caller, and a
// deleted branch is not recoverable by the next reader.
void ModInfoArraySetSize(ModInfoArray* a, int32_t newSize) {
    const int32_t oldSize = a->size;
    if (newSize == oldSize) return;

    ModInfo* oldData = a->data;
    a->size = newSize;

    if (newSize > a->capacity) {
        a->capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0) a->capacity = newSize * 3;
        a->data = static_cast<ModInfo*>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->capacity)) * 0x20, 8));
    }

    const int32_t start = (oldData == a->data) ? oldSize : 0;
    for (int32_t i = start; i < newSize; ++i) {
        ModInfo* dst = a->data + i;
        if (dst == nullptr) continue;
        if (i < oldSize) {
            const ModInfo* src = oldData + i;
            dst->name = nullptr;
            if (src->name != nullptr) {
                dst->name = src->name;
                RtSystem::StringRefCount(const_cast<char*>(src->name)) += 1;
            }
            dst->path = nullptr;
            if (src->path != nullptr) {
                dst->path = src->path;
                RtSystem::StringRefCount(const_cast<char*>(src->path)) += 1;
            }
            dst->versionMajor = src->versionMajor;
            dst->gameVersion  = src->gameVersion;
            dst->versionMinor = src->versionMinor;
        } else {
            dst->name = nullptr;
            dst->path = nullptr;
        }
    }

    if (oldData != a->data) {
        ModInfoDestroyRange(oldData, oldSize);
        if (oldData != nullptr) MemFree(oldData);
    } else if (oldSize > newSize) {
        ModInfoDestroyRange(oldData + newSize, oldSize - newSize);
    }
}

// ---------------------------------------------------------------------------
// The DLL entry points this body calls, all by MANGLED NAME.
// ---------------------------------------------------------------------------
// Every one of these is a real export (verified against the shipped export
// table before it was written down — a name the DLL does not contain resolves
// NULL in silence, reference_resolveexport_phantom_names.md). There is not one
// address door in this file.
using CtorFn        = void* (*)(void* self);
using SetWindowsFn  = void  (*)(void* self, int count);
using CommitFn      = void  (*)(void* self);
using ReleaseFn     = void  (*)(void* self);
using StrSerialFn   = void  (*)(String* self, rt::io::Serializer* ser);
using StrSetFn      = void  (*)(String* self, const char* src);
using StoreFn       = void  (*)(void* savedState, RtIo::MemoryStream* stream);

// The IO::Encoding factory slots. THREE arguments — the version stays live in
// R8D across the whole inlined Serializer ctor; runtime/types/io/Serializer.h
// records the pin.
using MakeCodecFn   = rt::io::ReaderWriter* (*)(void* encoding,
                                                rt::io::Serializer* ser, int version);
// IO::ReaderWriter scalar slot: (this, T& value, const char* debugName).
using RwScalarFn    = void (*)(rt::io::ReaderWriter* rw, void* value, const char* name);
// IO::ReaderWriter +0xC8: (this, int byteCount, void* data, const char* debugName).
using RwBytesFn     = void (*)(rt::io::ReaderWriter* rw, int size, void* data, const char* name);
// Slot 0 of any polymorphic class here: the scalar-deleting destructor.
using DeletingDtorFn = void (*)(void* self, int flags);
// IO::Stream vtable slot 7 (+0x38) is Close() on every concrete Stream.
using StreamCloseFn = void (*)(void* stream);

// The four vtables retail stamps by RIP-relative LEA. All four are EXPORTED
// symbols (??_7...@6B@), so they are resolved by name like everything else.
// Resolve-and-cache, NEVER caching a null — the same contract RT_EXPORT_ONCE
// has, spelled for a DATA symbol. ResolveExport is GetProcAddress: an
// export-table binary search under the loader lock, every call
// (reference_rtoriginal_is_getprocaddress.md). Six of them per save would not
// be measurable, but the house rule is to resolve once and the cheap version is
// the same length as the expensive one.
#define RT_VTABLE_ONCE(MANGLED)                                                \
    ([]() -> const void* {                                                     \
        static void* rt_vtable_once__ = nullptr;                               \
        return reinterpret_cast<const void*>(                                  \
            RtResolveCached((MANGLED), &rt_vtable_once__));                    \
    }())

inline void* ReadVtableSlot(const void* obj, int byteOffset) {
    void* const* vtbl = *reinterpret_cast<void* const* const*>(obj);
    return *reinterpret_cast<void* const*>(
        reinterpret_cast<const uint8_t*>(vtbl) + byteOffset);
}

// ---------------------------------------------------------------------------
} // namespace

// ============================================================================
// Save::SaveLoad::SaveMetaData — the body.        retail 0x1800c1490
// ============================================================================
static void RtSave_SaveLoad_SaveMetaData_Body(SaveLoad* self, void* pixels,
                                              int width, int height,
                                              String* name, int nameMode) {
    // ---- inlined ??0CompressedMemoryStream + ?Create(memoryStream, 2) ------
    // Left DELIBERATELY uninitialised: retail's ctor does not touch
    // CompressedStream+0x18 or +0x24 (runtime/types/io/Stream.h records both as
    // never-initialised), so a `= {}` here would be a behavioural difference,
    // not tidiness.
    RtIo::CompressedMemoryStream cs;

    RT_EXPORT_ONCE(CtorFn, "??0CompressedStream@IO@@QEAA@XZ")(&cs.base);
    cs.base.base.vptr = RT_VTABLE_ONCE("??_7CompressedMemoryStream@IO@@6B@");
    RT_EXPORT_ONCE(CtorFn, "??0MemoryStream@IO@@QEAA@XZ")(&cs.memoryStream);

    cs.base.base.direction = 1;                 // 1 == writing
    cs.base.source         = &cs.memoryStream.base;

    // `Allocate(8, 4)` under the allocator lock, then System::Compression's
    // ctor on it — which is exactly the inlined body of MemAlloc(8, 4).
    // Retail stores the CTOR'S RETURN (`this`), or zero when the allocation
    // failed; transcribed, including the failure arm.
    void* compression = MemAlloc(8, 4);
    cs.base.compression = (compression != nullptr)
        ? RT_EXPORT_ONCE(CtorFn, "??0Compression@System@@QEAA@XZ")(compression)
        : nullptr;

    RT_EXPORT_ONCE(SetWindowsFn, "?SetWindowCount@CompressedStream@IO@@QEAAXH@Z")(&cs.base, 2);

    // ---- inlined ??0Serializer@IO@@(stream, 3) ----------------------------
    // The stream's own Encoding decides which ReaderWriter gets built; the
    // direction chooses the slot. Version 3 is a literal in the bytes.
    rt::io::Serializer ser;
    ser.stream       = reinterpret_cast<rt::io::Stream*>(&cs);
    ser.readerWriter = nullptr;
    {
        void* encoding = cs.base.base.encoding;
        const int slot = (cs.base.base.direction == 0)
            ? rt::io::encoding_vtbl::kCreateReader
            : rt::io::encoding_vtbl::kCreateWriter;
        ser.readerWriter = reinterpret_cast<MakeCodecFn>(ReadVtableSlot(encoding, slot))(
            encoding, &ser, 3);
    }
    rt::io::ReaderWriter* rw = ser.readerWriter;

    // Resolved once per call rather than once per use: retail reloads
    // `[RSP+0x28]` and re-walks the vtable before EVERY one of these calls, but
    // the pointer it reloads cannot change (nothing writes it), so hoisting is
    // an equivalence, not a guess.
    auto scalarInt   = reinterpret_cast<RwScalarFn>(
                           ReadVtableSlot(rw, rt::io::rw_vtbl::kScalarInt));
    auto scalarUInt  = reinterpret_cast<RwScalarFn>(
                           ReadVtableSlot(rw, rt::io::rw_vtbl::kScalarUInt));
    auto writeBytes  = reinterpret_cast<RwBytesFn>(
                           ReadVtableSlot(rw, rt::io::rw_vtbl::kSerializeBytes));
    auto strSerialize = RT_EXPORT_ONCE(StrSerialFn,
                            "?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z");

    // ---- the record -------------------------------------------------------
    strSerialize(name, &ser);

    // Retail serialises the ARGUMENT HOME SLOTS: it spills R8D/R9D at entry
    // (0x1800c1493/0x1800c1497) and hands their addresses to the scalar slot,
    // which takes an `int&`. Our parameters occupy the same role. On the write
    // path the slot only reads them.
    // The slot takes an `int&` and writes back through it on the read path, so
    // the argument itself is handed over, exactly as retail hands it the home
    // slot.
    scalarInt(rw, &width,  nullptr);
    scalarInt(rw, &height, nullptr);

    writeBytes(rw, width * height * 4, pixels, nullptr);

    scalarInt(rw, &nameMode, nullptr);

    // ---- collect the enabled mods -----------------------------------------
    // The Modification singleton pointer is hoisted (`MOV R14,[...]` once at
    // 0x1800c1606) but `_mods` and `_count` are RE-READ every iteration
    // (0x1800c1630, 0x1800c1766). Transcribed as re-reads: String::Set and the
    // array grow both allocate, and hoisting would be a guess about what the
    // allocator cannot do.
    RtIO::Modification* mods =
        reinterpret_cast<RtIO::Modification*>(RtSaveDetail::ModificationSingleton());

    ModInfoArray arr;
    arr.data     = nullptr;
    arr.size     = 0;
    arr.capacity = 0;

    auto strSet = RT_EXPORT_ONCE(StrSetFn, "?Set@String@System@@QEAAXPEBD@Z");
    auto getDataDir = RT_EXPORT_ONCE(RtSaveDetail::GetDataDirectoryFn,
        "?GetDataDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ");

    int64_t byteOffset = 0;
    for (int32_t i = 0; i < mods->count; ++i, byteOffset += 0x80) {
        RtIO::Mod* entry = reinterpret_cast<RtIO::Mod*>(
            reinterpret_cast<uint8_t*>(mods->mods) + byteOffset);
        if (entry->enabled == 0) continue;

        ModInfo tmp;
        tmp.name = nullptr;
        tmp.path = nullptr;

        // The display name, adopted straight off the Mod entry with an AddRef.
        if (entry->name != nullptr) {
            tmp.name = entry->name;
            RtSystem::StringRefCount(entry->name) += 1;
        }

        // The path, with the data-directory prefix stripped. See the banner.
        RtSystem::Filename dataDir;
        dataDir.data = nullptr;
        getDataDir(RtSaveDetail::FileSystemSingleton(),
                   reinterpret_cast<String*>(&dataDir));

        const int32_t dirLen =
            (dataDir.data != nullptr) ? RtSystem::StringLength(dataDir.data) : 0;

        String work;
        work.data = nullptr;
        strSet(&work, entry->file.data + static_cast<int64_t>(dirLen));

        if (work.data != nullptr) {
            tmp.path = work.data;
            RtSystem::StringRefCount(work.data) += 1;
        }
        ReleaseStringSlot(const_cast<const char**>(&work.data));
        ReleaseStringSlot(const_cast<const char**>(&dataDir.data));

        tmp.versionMajor = entry->i2c;   // -> CheckMod status 4
        tmp.gameVersion  = entry->i18;   // -> CheckMod status 2 (< 0x1000400)
        tmp.versionMinor = entry->i1c;   // -> CheckMod status 3
        // tmp.pad1c is deliberately NOT written; retail does not write it and
        // the array copy above does not copy it.

        ModInfoArraySetSize(&arr, arr.size + 1, &tmp);

        ReleaseStringSlot(&tmp.path);
        ReleaseStringSlot(&tmp.name);
    }

    // ---- the mod block ----------------------------------------------------
    // `count` is a SEPARATE local from arr.size (retail copies it into a stack
    // slot at 0x1800c1775 and serialises THAT). It matters: the element loop
    // below is bounded by `count`, the destroy at the end by `arr.size`, and on
    // the read path SetSize is what makes them agree. Collapsing them into one
    // variable would be right today and wrong the moment this runs backwards.
    int32_t count = arr.size;
    scalarInt(rw, &count, nullptr);

    if (ser.stream->direction == 0) {          // read path; dead here, see above
        ModInfoArraySetSize(&arr, count);
    }

    for (int32_t i = 0; i < count; ++i) {
        ModInfo* e = arr.data + i;
        strSerialize(reinterpret_cast<String*>(&e->name), &ser);
        strSerialize(reinterpret_cast<String*>(&e->path), &ser);
        scalarUInt(rw, &e->versionMajor, nullptr);
        scalarUInt(rw, &e->gameVersion,  nullptr);
        scalarUInt(rw, &e->versionMinor, nullptr);
    }

    ModInfoDestroyRange(arr.data, arr.size);
    if (arr.data != nullptr) MemFree(arr.data);
    arr.data     = nullptr;
    arr.size     = 0;
    arr.capacity = 0;

    // ---- inlined ~Serializer ----------------------------------------------
    if (rw != nullptr) {
        reinterpret_cast<DeletingDtorFn>(ReadVtableSlot(rw, rt::io::rw_vtbl::kDtor))(rw, 1);
    }

    // ---- inlined ?Close@CompressedMemoryStream, then the handoff ----------
    // `_headers._size > 0` is "has anything been written yet"; the direction
    // test keeps a read-mode stream from committing.
    auto commit = RT_EXPORT_ONCE(CommitFn, "?Commit@CompressedStream@IO@@QEAAXXZ");
    if (cs.base.headerSize > 0 && cs.base.base.direction == 1) commit(&cs.base);
    reinterpret_cast<StreamCloseFn>(
        ReadVtableSlot(&cs.memoryStream, RtIo::kStreamVtblCloseSlot * 8))(&cs.memoryStream);

    // THE ONE SIDE EFFECT THAT LEAVES THIS FUNCTION: the finished record is
    // copied into the EMBEDDED SavedState at SaveLoad+0x28, which is what
    // ?UpdateSave builds SaveBlock entry 0 out of. A port that computed the
    // right bytes and did not publish them here would be invisible until a save
    // was loaded back (reference_port_must_reproduce_side_effects.md).
    RT_EXPORT_ONCE(StoreFn, "?Store@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z")(
        &self->_embeddedState, &cs.memoryStream);

    // ---- inlined ~CompressedMemoryStream / ~CompressedStream / ~MemoryStream
    // Every one of these vtable re-stamps is a real store in retail (an object
    // under destruction reverts to each base's table in turn). They are not
    // decoration: ?Close@CompressedMemoryStream is virtual, and the second
    // Close below dispatches through the table stamped on the line above it.
    cs.base.base.vptr = RT_VTABLE_ONCE("??_7CompressedMemoryStream@IO@@6B@");
    if (cs.base.headerSize > 0 && cs.base.base.direction == 1) commit(&cs.base);
    reinterpret_cast<StreamCloseFn>(
        ReadVtableSlot(&cs.memoryStream, RtIo::kStreamVtblCloseSlot * 8))(&cs.memoryStream);

    cs.memoryStream.base.vptr = RT_VTABLE_ONCE("??_7MemoryStream@IO@@6B@");
    if (cs.memoryStream.ownsData != 0 && cs.memoryStream.data != nullptr) {
        MemFree(cs.memoryStream.data);
    }
    cs.memoryStream.base.vptr = RT_VTABLE_ONCE("??_7Stream@IO@@6B@");
    if (cs.memoryStream.base.encoding != nullptr) MemFree(cs.memoryStream.base.encoding);

    cs.base.base.vptr = RT_VTABLE_ONCE("??_7CompressedStream@IO@@6B@");
    RT_EXPORT_ONCE(ReleaseFn, "?Release@CompressedStream@IO@@QEAAXXZ")(&cs.base);

    // *** EVERY FREE BELOW THIS LINE IS DEAD, AND THAT IS RETAIL'S DOING. ***
    // ?Release@CompressedStream (0x1800aa100) already freed all three arrays
    // AND the Compression at +0x60, and ZEROED every one of those pointers
    // before returning. What follows is the inlined ~CompressedStream running
    // straight afterwards over the corpse: each `if (ptr != nullptr)` sees the
    // null Release just wrote and does nothing.
    //
    // It is transcribed rather than deleted for the reason PLAN §6 step 8 gives
    // about comments, applied to code: "these frees cannot fire" is a claim
    // about ?Release's CURRENT body, not about this function. Deleting them
    // would silently make this port depend on that, and nothing would notice if
    // Release ever stopped zeroing.
    //
    // NOTE THE ONE POINTER NOT FREED ANYWHERE IN THIS EPILOGUE: the Compression
    // at CompressedStream+0x60. It is not leaked — Release owns it (it frees
    // the object's first qword, then the object). A reader auditing this
    // epilogue for balance will not find its free here and should not add one.
    if (cs.base.blockData != nullptr) MemFree(cs.base.blockData);
    cs.base.blockData = nullptr;
    cs.base.blockSize = 0;
    cs.base.blockCap  = 0;

    if (cs.base.headerData != nullptr) MemFree(cs.base.headerData);
    cs.base.headerData = nullptr;
    cs.base.headerSize = 0;
    cs.base.headerCap  = 0;

    if (cs.base.windowData != nullptr) MemFree(cs.base.windowData);
    cs.base.windowData = nullptr;
    cs.base.windowSize = 0;
    cs.base.windowCap  = 0;

    cs.base.base.vptr = RT_VTABLE_ONCE("??_7Stream@IO@@6B@");
    if (cs.base.base.encoding != nullptr) MemFree(cs.base.base.encoding);

    // ---- destroy the BY-VALUE String argument ------------------------------
    // See the banner. This is the callee's half of a contract whose caller half
    // is already written down in saveload_update.cpp's UpdateSave.
    ReleaseStringSlot(const_cast<const char**>(&name->data));
}

// ============================================================================
// THE ORACLE — two sinks, and THEIRS actually runs
// ============================================================================
//
// Armed by the old DLL-era switch. Ours writes into the caller's real
// SaveLoad (so the live save is produced by the port, not by the DLL); the
// shipped DLL writes into a scratch SaveLoad-shaped block; the two resulting
// Save::SavedState records are compared.
//
// WHY THIS IS SOUND. The function is a pure read of (pixels, width, height,
// name, nameMode) and of two singletons that it does not mutate, so running it
// twice produces the same record twice. It is NOT a self-check
// (reference_shadow_marker_is_not_an_oracle.md): the second run is
// the raw export door'd out of the DLL and never sees our verdict, our buffer, or
// our SaveLoad.
//
// THE COMPARED REGION IS BOUNDED to what both sides actually write — `_size`
// and the `_size` bytes at `_data`. `_id` is deliberately NOT compared: nothing
// in this call path writes it (?SetId and the 1-arg ctor are its only writers),
// so comparing it would be comparing a byte neither side wrote, which fails
// 100% of calls and reads like a catastrophic defect
// (reference_oracle_must_not_compare_unwritten_bytes.md).
//
// TWO SCREENS, SCORED SEPARATELY (reference_union_control_hides_a_blind_screen.md):
//   SIZE  — do the two records have the same length?
//   BYTES — are the bytes identical?
// A control scored on the union of the two could not tell "both work" from "one
// is blind", so each is counted and reported on its own line.
//
// THE BY-VALUE STRING IS THE TRAP HERE. Each call DESTROYS its String argument,
// so handing the same one to both would double-release the save name. The
// oracle takes two fresh references and consumes the caller's own, leaving the
// net refcount exactly as a single normal call would.

namespace {

} // namespace

extern "C" void RtSave_SaveLoad_SaveMetaData(SaveLoad* self, void* pixels,
                                             int width, int height,
                                             String* name, int nameMode) {
    RtSave_SaveLoad_SaveMetaData_Body(self, pixels, width, height, name, nameMode);
}

// A3-HOLD: the oracle this summarised compared us with the shipped DLL and is
// gone. The entry point stays as a no-op only because standalone/main.cpp still
// calls it.
extern "C" void RtSaveMetaSummary() {}

// ============================================================================
// Registration — BOTH doors.
// ============================================================================
// The mangled name is a STRING LITERAL because tools/runtime_census.py only
// sees literals (reference_census_cannot_see_nonliteral_registrations.md), and
// it was checked byte for byte against the shipped export table: a phantom name
// resolves NULL in silence.
//
// The address door is NOT redundant with it. UpdateSave reaches this function
// by name, but the DLL's own internal callers and any exe dispatch slot reach
// it by address, and a by-address door with nothing registered behind it is a
// call through null at cutover.
//
// GREPPED FIRST: neither "?SaveMetaData@SaveLoad@Save@@" nor 0x1800c1490 is
// registered anywhere else in runtime/, code/, engine/ or standalone/ — a
// duplicate RegisterRuntimeAddr is last-binding-wins and would silently make
// one of the two bodies dead code
// (reference_grep_for_the_port_before_writing_it.md).
int RtSaveSaveMetaDataRegister() {
    int n = 0;
    RegisterRuntimeExport("?SaveMetaData@SaveLoad@Save@@QEAAXPEAXHHVString@System@@H@Z",
                          (void*)&RtSave_SaveLoad_SaveMetaData); ++n;
    RegisterRuntimeAddr(0x1800c1490, (void*)&RtSave_SaveLoad_SaveMetaData);
    return n;
}
