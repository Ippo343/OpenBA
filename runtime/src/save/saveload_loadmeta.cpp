// ============================================================================
// runtime/src/save/saveload_loadmeta.cpp — Save::SaveLoad::LoadMetaData
//                                          (Wave 14, lane LOADMETA)
// ============================================================================
//
// ONE FUNCTION:
//   ?LoadMetaData@SaveLoad@Save@@QEAAXAEAVString@System@@PEAPEAXAEAH22AEAV?$Array@UModInfo@SaveLoad@Save@@@Collection@@@Z
//   retail 0x1800c1b30, ordinal 3208 (name, ordinal and address all read out of
//   the shipped export directory with pefile before this file was written —
//   reference_resolveexport_phantom_names.md).
//
// It is the READ half of the save-file metadata record: given the compressed
// byte image the async loader has already dropped into `this->_embeddedState`,
// it decompresses it, runs one IO::Serializer over it and hands the caller SIX
// out-parameters — description, thumbnail pixels, width, height, the format
// version, and the mod list the save was made with. Its single caller is
// ?UpdateList@SaveLoad state 4 (runtime/src/save/saveload_update.cpp, Wave 13),
// which copies five of the six into the SaveInfo row the save/load dialog draws.
//
// ----------------------------------------------------------------------------
// SEVEN ARGUMENTS, AND WHY THAT IS THE FIRST THING SAID HERE
// ----------------------------------------------------------------------------
// `this` plus six, so THREE arrive on the stack (MS x64 puts args 5..7 at
// [entry_RSP+0x28], +0x30, +0x38). Retail loads them as [RBP+0x6f], [RBP+0x77]
// and [RBP+0x7f] with RBP = entry_RSP - 0x47, which is how each was identified:
//
//   RCX          SaveLoad*                     this
//   RDX          System::String&               description   (out)
//   R8           void**                        pixels        (out)
//   R9           int&                          width         (out)
//   [RSP+0x28]   int&                          height        (out)
//   [RSP+0x30]   int&                          version       (out)   -> R15
//   [RSP+0x38]   Collection::Array<ModInfo>&   mods          (out)
//
// This matters twice over. First, the caller's own comment in
// saveload_update.cpp already pinned the stack triple from the CALL side
// (0x1800c22a0/0x22a9/0x22b2) and the two readings agree, which is the only
// cross-check available for arguments a decompiler renders as names. Second,
// **a register-based screen cannot see arguments 5, 6 and 7 at all** — this
// project scored a positive control 3/3 on the UNION of two screens and only
// later found that one of them caught 1/3, precisely because it was blind to
// every 5th-or-later argument (reference_union_control_hides_a_blind_screen).
// The oracle at the bottom of this file therefore compares OUT-PARAMETER VALUES,
// not registers, and gives height, version and the mod array a screen each.
//
// ----------------------------------------------------------------------------
// GHIDRA'S C IS WRONG ABOUT THIS BODY IN TWO PLACES. THE DISASSEMBLY WINS.
// ----------------------------------------------------------------------------
// Both are the documented failure modes of reference_ghidra_c_output_vs_disassembly:
//
//  1. IT DROPS ARGUMENTS AT EVERY VTABLE CALL. The decompiler renders the
//     encoding factory call as `(**(code **)(*local_b8 + 0x18))()` — no
//     arguments at all — and the mod-element loop as
//     `(**(code **)(*local_d0 + 0x98))(local_d0, lVar5 + 4)`, dropping the
//     third. The bytes say three arguments at every one of them
//     (`XOR R8D,R8D` / `XOR R9D,R9D` immediately before each CALL). Every
//     indirect call below was counted off the disassembly.
//
//  2. IT DROPS A DEREFERENCE ON THE ONE BRANCH THAT DECIDES A WRITE.
//     `if (*(int *)(local_d8 + 1) == 0)` reads, in C, as "the serializer's
//     second qword". The bytes are `MOV RAX,[RSP+0x20]` (LOAD the serializer's
//     _stream) then `CMP dword ptr [RAX+0x8],0x0` — i.e. `ser._stream->direction`,
//     one level deeper. That branch is what gates Array<ModInfo>::SetSize, so
//     taking the decompiler's word for it would have tested the WRONG WORD and
//     either skipped the resize or run it on the writing path.
//
// ----------------------------------------------------------------------------
// WHAT THE BODY IS, STEP BY STEP (retail addresses in the code below)
// ----------------------------------------------------------------------------
// Every step maps onto an export this tree has ALREADY ported, because retail
// inlines the two-line wrappers rather than the work:
//
//   IO::CompressedMemoryStream cms;              // ??0CompressedMemoryStream
//   cms.GetMemoryStream().Open(_embeddedState._size);
//   memmove(cms._memoryStream._data, _embeddedState._data, _embeddedState._size);
//   cms.Open();                                  // == CompressedStream::Open(*this,_mem,1)
//   {   IO::Serializer ser(cms, 0);              // ctor INLINED, see below
//       version     = ser.GetVersion();
//       description.Serialize(ser);
//       ser << width; ser << height;
//       pixels = MemAlloc(width*height*4, 4);
//       ser.SerializeBytes(width*height*4, pixels);
//       if (version > 2) { ser << version; <the mod list>; }
//   }                                            // ~Serializer INLINED
//   cms.Close();                                 // ?Close@CompressedMemoryStream
//   <free _embeddedState>
//   cms.~CompressedMemoryStream();               // ??1CompressedMemoryStream
//
// The three wrapper-level calls (`Open`, `Close`, the ctor/dtor) are made
// through the CompressedMemoryStream exports rather than re-inlined here.
// That is not a shortcut: ?Open@CompressedMemoryStream (0x1800a9500) IS
// `CompressedStream::Open(*this, _memoryStream, 1)` and nothing else, and
// ?Close@ / ??1 are transcribed in runtime/src/io/compressedstream.cpp from
// the same bytes retail inlined here — so the call reaches OUR transcription of
// the identical instructions, and it does not grow the cutover surface with a
// by-address door (reference_porting_can_grow_the_cutover_surface).
//
// THE SERIALIZER CTOR AND DTOR *ARE* RE-INLINED, deliberately. Constructing an
// IO::Serializer consumes a 12-byte header off the stream (see the big note in
// runtime/types/io/Serializer.h), so it is not a wrapper — it is work, and the
// exact factory slot it picks (+0x18 reading, +0x20 writing) is chosen from
// `cms.direction`, a field this body must be seen to read. Both are written out
// instruction for instruction.
//
// ----------------------------------------------------------------------------
// FOUR THINGS A READER WOULD OTHERWISE "FIX"
// ----------------------------------------------------------------------------
//
// 1. `version` IS WRITTEN TWICE, AND THE SECOND WRITE IS NOT A BUG. The
//    out-param first receives ReaderWriter::GetVersion() — the format version
//    the BinaryReader read out of the stream header — and then, when that value
//    is > 2, the same reference is handed to Serialize(int&) and receives a
//    SECOND number read from the record body. Retail uses one variable for both
//    (R15 is the arg-6 pointer throughout) and so does this port. Splitting them
//    would change what the caller stores in SaveInfo+0x44.
//
// 2. THE MOD COUNT IS SEEDED FROM THE ARRAY, THEN OVERWRITTEN. `count` is
//    initialised from `mods._size` and immediately serialized — which on the
//    READ path replaces it with the stored count. That looks like a dead store
//    and is not: this is a shared read/write body, and on the writing path the
//    seed is the value that goes into the file.
//
// 3. THE ELEMENT BASE POINTER IS LOADED ONCE, BEFORE THE LOOP. Retail does
//    `MOV RBX,[RBX]` at 0x1800c1ced and then walks with `ADD RBX,0x20`; it never
//    re-reads `mods._data`. Re-reading it per iteration would be the "safer"
//    spelling and would be a different function.
//
// 4. THE THREE ModInfo VERSION FIELDS GO THROUGH THE **UNSIGNED** SLOT.
//    +0x10, +0x14 and +0x18 are serialized with ReaderWriter vtable +0x98
//    (kScalarUInt), not +0xa0 (kScalarInt). runtime/types/save/SaveLoad.h names
//    two of them `int32_t` after CheckMod's SIGNED comparisons; both readings
//    are right about their own instruction, and the wire format is the unsigned
//    one. Nothing is reinterpreted here — the slot is chosen off the bytes.
//
// ----------------------------------------------------------------------------
// *** A WAVE 13 OPEN FINDING, CLOSED BY THIS LANE ***
// ----------------------------------------------------------------------------
// saveload_update.cpp's UpdateList carries an explicitly-open note: every
// predicting run reports `UNPREDICTED WRITE Save::SaveLoad::UpdateList 9 byte(s)
// ... first at Save::SaveLoad [state 4 meta] +44`, i.e. nine bytes at
// SaveLoad+0x2c — the embedded SavedState's `_size` and `_data` — change during
// a state-4 round and nothing predicts or excludes them. That note names
// LoadMetaData as having been checked and cleared: "only READS +0x2c/+0x30 ...
// and its tail stores to neither."
//
// **Its tail stores to both.** 0x1800c1d98..0x1800c1db5:
//     MOV RCX,[R14+0x30] ; TEST RCX,RCX ; JZ ; CALL 0x1800c7ad0   ; MemFree
//     MOV qword ptr [R14+0x30],0x0
//     MOV dword ptr [R14+0x2c],0x0
// LoadMetaData FREES the compressed image it just consumed and zeroes the
// SavedState in place. That is the writer, it is inside the suppressed callee
// on exactly the state-4 arm the finding names, and the byte count fits (the
// _size dword plus the low bytes of the _data pointer). The stale claim is
// corrected at its source in saveload_update.cpp by this commit, per PLAN §6
// step 8 — a comment that was believed for a wave is worse than none.
//
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "../../types/save/SaveLoad.h"
#include "../../types/io/Stream.h"
#include "../../types/io/Serializer.h"
#include "../../types/collection/Collection.h"
#include "../system/string.h"
#include "saveload_internal.h"
#include "resolve.h"   // ResolveAddr / Module — the port-aware address door

namespace {

using RtSaveDetail::MemAlloc;
using RtSaveDetail::MemFree;
using RtSaveDetail::ReleaseStringSlot;

using RtSave::SaveLoad;
using RtSave::ModInfo;
using RtIo::CompressedMemoryStream;
using RtIo::MemoryStream;

using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
namespace rw_vtbl = rt::io::rw_vtbl;

using ModArray = Collection::Array<ModInfo>;

// ---- callee shapes ---------------------------------------------------------
//
// EVERY ONE of these was counted off the disassembly, not off Ghidra's C, and
// none of them returns a class by value — so not one has a hidden return slot
// (tools/abi_retslot_audit.py agrees; a typedef that omits one is a wild write
// that still returns the right answer, reference_hidden_retslot_wild_write).
using CmsCtorFn   = CompressedMemoryStream* (*)(CompressedMemoryStream*);
using CmsVoidFn   = void (*)(CompressedMemoryStream*);
using CmsBoolFn   = bool (*)(CompressedMemoryStream*);
using MsOpenFn    = bool (*)(MemoryStream*, int);
using StrSerialFn = void (*)(const char** self, Serializer* s);
using SetSizeFn   = void (*)(ModArray* arr, int n);

// IO::Encoding vtable +0x18 / +0x20: (this, Serializer*, int version)
// -> ReaderWriter*. Arity 3 is pinned in runtime/types/io/Serializer.h off
// ??0Serializer's own R8D liveness, and independently here: retail leaves
// R8D zeroed by `XOR R8D,R8D` at 0x1800c1bd1 and never reloads it.
using CreateCodecFn = ReaderWriter* (*)(Encoding*, Serializer*, int);

// IO::ReaderWriter vtable slots. Displacements from runtime/types/io/Serializer.h
// (RETAIL bytes — the profile build's differ and must never be copied).
using RwGetVersionFn = int  (*)(ReaderWriter*);                              // +0x08
using RwScalarUIntFn = void (*)(ReaderWriter*, uint32_t*, const char*);      // +0x98
using RwScalarIntFn  = void (*)(ReaderWriter*, int*,      const char*);      // +0xa0
using RwBytesFn      = void (*)(ReaderWriter*, int, void*, const char*);     // +0xc8
using RwDeleteFn     = void (*)(ReaderWriter*, unsigned int);                // +0x00

using LoadMetaDataFn = void (*)(SaveLoad*, const char**, void**,
                                int*, int*, int*, ModArray*);

// The exported name, spelled ONCE and reused by the port, the oracle and the
// registration below. tools/runtime_census.py can only see it where it is a
// STRING LITERAL at the RegisterRuntimeExport call site, so the registration at
// the bottom of this file repeats it in full rather than referring to this
// constant (reference_census_cannot_see_nonliteral_registrations).
constexpr const char* kLoadMetaData =
    "?LoadMetaData@SaveLoad@Save@@QEAAXAEAVString@System@@PEAPEAXAEAH22AEAV?$Array@UModInfo@SaveLoad@Save@@@Collection@@@Z";

template <typename Fn>
inline Fn VSlot(const void* obj, int byteOffset) {
    const void* const* vtbl = *reinterpret_cast<const void* const* const*>(obj);
    return reinterpret_cast<Fn>(
        const_cast<void*>(vtbl[byteOffset / static_cast<int>(sizeof(void*))]));
}

// FUN_1800331f0 — Collection::Array<ModInfo>::~Array. saveload.cpp owns the
// transcription and registers it at this address; ResolveAddr consults the PORT
// REGISTRY FIRST (reference_resolveaddr_consults_port_registry_first), so this
// reaches our body, not the DLL's. Only the oracle uses it, to tear down the
// array the shipped body filled in our private scratch.
void DestroyScratchModArray(ModArray* arr) {
    using Fn = void (*)(ModArray*);
    static Fn f = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800331f0));
    f(arr);
}

// ---- the negative control ---------------------------------------------------
// BANISHED_RT_LOADMETA_NEGCTL=<1..6> corrupts, on the THEIRS side only, exactly
// one quantity that one screen compares. Theirs is a private scratch that is
// torn down two dozen lines later, so nothing the game can observe is touched.
// Six separate values because a union score cannot tell "both screens work"
// from "one screen is blind" — every screen is scored on its own.
int NegControl() {
    static int v = [] {
        const char* s = std::getenv("BANISHED_RT_LOADMETA_NEGCTL");
        return (s != nullptr) ? std::atoi(s) : 0;
    }();
    return v;
}

// System::String equality by CONTENT. The two sides are two live allocations, so
// the POINTERS differ by construction and comparing them would fail 100% of
// calls on a correct port (reference_oracle_must_not_compare_unwritten_bytes,
// one level over). Length is compared too: String stores it at data[-8], and a
// content compare alone cannot see an embedded NUL.
bool StringContentEqual(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return a == b;
    if (RtSystem::RtString_GetLength(reinterpret_cast<const RtSystem::String*>(&a)) !=
        RtSystem::RtString_GetLength(reinterpret_cast<const RtSystem::String*>(&b)))
        return false;
    return std::strcmp(a, b) == 0;
}

void ReportScreen(RtDiffSite* site, bool equal, const char* ours, const char* theirs) {
    RtDiffRegister(site);
    RtDiffReport(site, equal, ours, theirs);
}

}  // namespace

// ============================================================================
// Save::SaveLoad::LoadMetaData   retail 0x1800c1b30   ordinal 3208
// ============================================================================
extern "C" void RtSave_SaveLoad_LoadMetaData(SaveLoad* self,
                                             const char** description,
                                             void**       pixels,
                                             int*         width,
                                             int*         height,
                                             int*         version,
                                             ModArray*    mods) {
    RT_DIFF_BEGIN("Save::SaveLoad::LoadMetaData [all six out-params]");

    // ---- the oracle's input snapshot, taken BEFORE the body consumes it -----
    // The body FREES `_embeddedState._data` and zeroes the SavedState, so by the
    // time theirs could run the input no longer exists. Copy it first; the copy
    // is the input both halves are given.
    const uint32_t inSize = self->_embeddedState._size;
    void*          inCopy = nullptr;
    if (rt_diff_on__ && inSize != 0 && self->_embeddedState._data != nullptr) {
        inCopy = MemAlloc(inSize, 4);
        std::memcpy(inCopy, self->_embeddedState._data, inSize);
    }

    // ---- 0x1800c1b6c : IO::CompressedMemoryStream cms; ---------------------
    // 0x98 bytes of uninitialised stack, exactly as retail: the ctor writes the
    // fields it writes and leaves the documented gaps alone. Zeroing them first
    // would be a normalisation of retail's bytes.
    alignas(16) unsigned char cmsStorage[sizeof(CompressedMemoryStream)];
    CompressedMemoryStream* cms =
        reinterpret_cast<CompressedMemoryStream*>(cmsStorage);
    RT_EXPORT_ONCE(CmsCtorFn, "??0CompressedMemoryStream@IO@@QEAA@XZ")(cms);

    // ---- 0x1800c1b90 / 0x1800c1ba1 : fill the embedded MemoryStream --------
    // Open allocates the buffer and sets _size/_ownsData; the copy that follows
    // reads `_embeddedState._size` a SECOND time from memory (`MOV R8D,[R14+0x2c]`
    // at 0x1800c1b95, after the Open call), and is transcribed with the same
    // second read. 0x18008a290 is memmove, not memcpy (runtime/src/system/error.cpp).
    RT_EXPORT_ONCE(MsOpenFn, "?Open@MemoryStream@IO@@QEAA_NH@Z")(
        &cms->memoryStream, static_cast<int>(self->_embeddedState._size));
    std::memmove(cms->memoryStream.data,
                 self->_embeddedState._data,
                 self->_embeddedState._size);

    // ---- 0x1800c1bb5 : CompressedStream::Open(cms, cms._memoryStream, 1) ---
    // Retail calls the base directly with the embedded stream and a window count
    // of 1; ?Open@CompressedMemoryStream is that call and nothing else. Its
    // bool result is DISCARDED here, exactly as retail discards it — a save whose
    // window table does not check out produces garbage metadata rather than an
    // error, and that is the shipped behaviour.
    RT_EXPORT_ONCE(CmsBoolFn, "?Open@CompressedMemoryStream@IO@@QEAA_NXZ")(cms);

    // ---- 0x1800c1bba..0x1800c1bea : IO::Serializer ser(cms, 0), INLINED ----
    // Field for field with ??0Serializer@IO@@ (0x1800b9960): store the stream,
    // null the codec, then ask the stream's Encoding for a reader or a writer
    // depending on the stream's own direction. `version` here is the literal 0
    // retail passes (`XOR R8D,R8D`), not the out-parameter of the same name.
    Serializer ser;
    ser.stream       = reinterpret_cast<rt::io::Stream*>(cms);
    ser.readerWriter = nullptr;
    {
        Encoding* enc = reinterpret_cast<Encoding*>(cms->base.base.encoding);
        const int slot = (cms->base.base.direction == 0) ? rt::io::encoding_vtbl::kCreateReader
                                                         : rt::io::encoding_vtbl::kCreateWriter;
        ser.readerWriter = VSlot<CreateCodecFn>(enc, slot)(enc, &ser, 0);
    }
    ReaderWriter* rw = ser.readerWriter;

    // ---- 0x1800c1bf7 : out-param 6 = the format version --------------------
    *version = VSlot<RwGetVersionFn>(rw, rw_vtbl::kGetVersion)(rw);

    // ---- 0x1800c1c09 : out-param 1 = the description String ----------------
    RT_EXPORT_ONCE(StrSerialFn, "?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z")(
        description, &ser);

    // ---- 0x1800c1c1c / 0x1800c1c34 : out-params 3 and 4 --------------------
    // Both through the SIGNED scalar slot (+0xa0) with a null field name.
    VSlot<RwScalarIntFn>(rw, rw_vtbl::kScalarInt)(rw, width,  nullptr);
    VSlot<RwScalarIntFn>(rw, rw_vtbl::kScalarInt)(rw, height, nullptr);

    // ---- 0x1800c1c3a..0x1800c1c93 : out-param 2 = the thumbnail ------------
    // `MOV ESI,[RDI] ; IMUL ESI,[RBX] ; SHL ESI,0x2` — a 32-bit product that is
    // then SIGN-extended by `MOVSXD RDX,ESI` for the allocator. Transcribed with
    // the 32-bit arithmetic and the sign extension intact rather than promoted to
    // 64-bit, because promoting it changes the size request on any w*h that
    // overflows. The locked Allocate retail inlines around
    // MemoryAllocator::_singleton+0x640 is byte-identical to the exported
    // MemAlloc (established by Wave 5 on SavedState::Store, same call shape).
    const int32_t byteCount = (*width) * (*height) * 4;
    *pixels = MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(byteCount)), 4);
    VSlot<RwBytesFn>(rw, rw_vtbl::kSerializeBytes)(rw, byteCount, *pixels, nullptr);

    // ---- 0x1800c1c9a : the mod list, format version 3 and up ---------------
    if (*version > 2) {
        // The SECOND write to the version out-param. See note 1 in the header.
        VSlot<RwScalarIntFn>(rw, rw_vtbl::kScalarInt)(rw, version, nullptr);

        // Seeded from the array (the writing path's value), then replaced by the
        // stored count on the reading path. See note 2.
        int count = mods->_size;
        VSlot<RwScalarIntFn>(rw, rw_vtbl::kScalarInt)(rw, &count, nullptr);

        // `MOV RAX,[RSP+0x20] ; CMP dword ptr [RAX+0x8],0x0` — the stream the
        // SERIALIZER holds, dereferenced, then its direction. This is the
        // dereference Ghidra's C drops; see the header.
        if (ser.stream->direction == 0)
            reinterpret_cast<SetSizeFn>(ResolveAddr(Module::Runtime, 0x180034010))(mods, count);

        // 0x1800c1ced: the base pointer is read ONCE. See note 3.
        ModInfo* e = mods->_data;
        for (int i = 0; i < count; ++i, ++e) {
            RT_EXPORT_ONCE(StrSerialFn, "?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z")(
                &e->name, &ser);
            RT_EXPORT_ONCE(StrSerialFn, "?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z")(
                &e->path, &ser);
            // +0x98, the UNSIGNED slot, for all three. See note 4.
            VSlot<RwScalarUIntFn>(rw, rw_vtbl::kScalarUInt)(
                rw, reinterpret_cast<uint32_t*>(&e->versionMajor), nullptr);
            VSlot<RwScalarUIntFn>(rw, rw_vtbl::kScalarUInt)(rw, &e->gameVersion, nullptr);
            VSlot<RwScalarUIntFn>(rw, rw_vtbl::kScalarUInt)(
                rw, reinterpret_cast<uint32_t*>(&e->versionMinor), nullptr);
        }
    }

    // ---- 0x1800c1d63 : ~Serializer, inlined --------------------------------
    // The scalar-DELETING destructor with flags = 1, i.e. the codec is freed.
    if (ser.readerWriter != nullptr) {
        ReaderWriter* dying = ser.readerWriter;
        VSlot<RwDeleteFn>(dying, rw_vtbl::kDtor)(dying, 1u);
    }

    // ---- 0x1800c1d77 : cms.Close() -----------------------------------------
    // Retail inlines CompressedMemoryStream::Close (guard + Commit + the
    // embedded stream's virtual Close); runtime/src/io/compressedstream.cpp
    // holds that transcription and this reaches it.
    RT_EXPORT_ONCE(CmsVoidFn, "?Close@CompressedMemoryStream@IO@@UEAAXXZ")(cms);

    // ---- 0x1800c1d98 : release the consumed image --------------------------
    // THE WRITER Wave 13's UpdateList note went looking for. See the header.
    if (self->_embeddedState._data != nullptr) MemFree(self->_embeddedState._data);
    self->_embeddedState._data = nullptr;
    self->_embeddedState._size = 0;

    // ---- 0x1800c1db6 : ~CompressedMemoryStream, inlined by retail ----------
    RT_EXPORT_ONCE(CmsVoidFn, "??1CompressedMemoryStream@IO@@UEAA@XZ")(cms);

    // Every entry of this body does the full amount of work: there is no
    // early-out anywhere in it, so `worked` tracks `calls` by construction. It
    // is declared rather than left blank so the summary prints a number instead
    // of `worked=?`, which would read as an unknown that is not one.
    RT_DIFF_WORK(true);

    if (!rt_diff_on__) return;

    // ========================================================================
    // ORACLE — REPLAY ON A PRIVATE SCRATCH, SIX SCREENS SCORED SEPARATELY
    // ------------------------------------------------------------------------
    // OURS ran for real, above, on the live SaveLoad. THEIRS now runs on a
    // ZEROED 0x90-byte scratch whose only populated fields are the embedded
    // SavedState's `_size` and a fresh COPY of the input bytes — which is
    // sufficient because the shipped body touches `this` at EXACTLY five
    // instructions, all of them +0x2c or +0x30 (0x1800c1b88, 0x1800c1b95,
    // 0x1800c1b99, 0x1800c1da6, 0x1800c1dae) and nowhere else. That was read off
    // the disassembly, not assumed.
    //
    // So this is a real oracle in the sense rt_predict.h insists on: the
    // authority is the shipped DLL, running for real on the same input, and the
    // comparison is over values it produced independently of anything we did.
    // It is NOT an RT_DIFF_SHADOW self-check.
    //
    // WHAT IS COMPARED IS BOUNDED TO WHAT IS WRITTEN. The pixel block is
    // compared over exactly w*h*4 bytes; the mod elements over the five fields
    // the record actually contains and NOT over ModInfo+0x1c, which neither side
    // writes (an oracle that memcmps bytes neither side writes fails every call
    // and reads like a catastrophic port defect).
    //
    // SIX SCREENS, SIX SITES, SIX SEPARATE SCORES. A single aggregate verdict
    // cannot distinguish "all six screens work" from "five work and one is
    // structurally incapable of failing", which is exactly the union-control
    // trap this project has already paid for. The RT_DIFF_BEGIN site above
    // carries the aggregate; the six below carry the parts.
    // ========================================================================
    static RtDiffSite sVersion  = { "Save::SaveLoad::LoadMetaData [S1 version, arg 6 = STACK]" };
    static RtDiffSite sExtent   = { "Save::SaveLoad::LoadMetaData [S2 width/height, args 4 REG + 5 STACK]" };
    static RtDiffSite sDesc     = { "Save::SaveLoad::LoadMetaData [S3 description, arg 2]" };
    static RtDiffSite sPixels   = { "Save::SaveLoad::LoadMetaData [S4 pixel block, arg 3]" };
    static RtDiffSite sModCount = { "Save::SaveLoad::LoadMetaData [S5 mod count, arg 7 = STACK]" };
    static RtDiffSite sModElems = { "Save::SaveLoad::LoadMetaData [S6 mod elements, arg 7 = STACK]" };

    alignas(16) unsigned char scratchSelf[sizeof(SaveLoad)];
    std::memset(scratchSelf, 0, sizeof(scratchSelf));
    SaveLoad* theirSelf = reinterpret_cast<SaveLoad*>(scratchSelf);
    theirSelf->_embeddedState._size = inSize;
    theirSelf->_embeddedState._data = (inSize != 0) ? MemAlloc(inSize, 4) : nullptr;
    if (inSize != 0 && inCopy != nullptr)
        std::memcpy(theirSelf->_embeddedState._data, inCopy, inSize);

    const char* tDesc   = nullptr;
    void*       tPixels = nullptr;
    int         tW = 0, tH = 0, tVersion = 0;
    ModArray    tMods{ nullptr, 0, 0 };

    RT_ORIGINAL_ONCE(LoadMetaDataFn, kLoadMetaData)(
        theirSelf, &tDesc, &tPixels, &tW, &tH, &tVersion, &tMods);

    // The real element count, kept aside because negative control 5 deliberately
    // falsifies `tMods._size` and the teardown below walks the array with it.
    // A control that corrupted the destructor's trip count would be a control
    // that crashes the process instead of failing a comparison.
    const int32_t tModsSizeReal = tMods._size;

    // ---- the negative control, applied to THEIRS only ----------------------
    switch (NegControl()) {
        case 1:  // S1: flip a bit of the version
            tVersion ^= 1;
            break;
        case 2:  // S2: move the height, which is argument FIVE (first stack slot)
            tH += 1;
            break;
        case 3:  // S3: change one character of the description
            if (tDesc != nullptr) const_cast<char*>(tDesc)[0] ^= 0x20;
            break;
        case 4:  // S4: flip one byte inside the compared pixel extent
            if (tPixels != nullptr && byteCount > 0)
                reinterpret_cast<unsigned char*>(tPixels)[0] ^= 0xffu;
            break;
        case 5:  // S5: move the mod count, argument SEVEN
            tMods._size += 1;
            break;
        case 6:  // S6: change a field of mod element 0 that S6 actually compares
            if (tMods._data != nullptr && tMods._size > 0)
                tMods._data[0].gameVersion ^= 1u;
            break;
        default:
            break;
    }

    char a[96], b[96];

    // S1 — version. Argument SIX, delivered on the stack; a register screen
    // could not see it at all.
    const bool eqVersion = (*version == tVersion);
    RtDiffFormat(a, sizeof(a), *version);
    RtDiffFormat(b, sizeof(b), tVersion);
    ReportScreen(&sVersion, eqVersion, a, b);

    // S2 — width and height. Argument FOUR (last register) and argument FIVE
    // (first stack slot); the pair is deliberately one screen so a mismatch
    // names the boundary.
    const bool eqExtent = (*width == tW) && (*height == tH);
    std::snprintf(a, sizeof(a), "%dx%d", *width, *height);
    std::snprintf(b, sizeof(b), "%dx%d", tW, tH);
    ReportScreen(&sExtent, eqExtent, a, b);

    // S3 — the description String, by CONTENT and LENGTH.
    const bool eqDesc = StringContentEqual(*description, tDesc);
    std::snprintf(a, sizeof(a), "\"%.60s\"", *description ? *description : "(null)");
    std::snprintf(b, sizeof(b), "\"%.60s\"", tDesc ? tDesc : "(null)");
    ReportScreen(&sDesc, eqDesc, a, b);

    // S4 — the pixel block, over exactly the bytes both sides serialized.
    const int32_t tBytes = tW * tH * 4;
    bool eqPixels = (byteCount == tBytes) && (*pixels != nullptr) == (tPixels != nullptr);
    if (eqPixels && byteCount > 0)
        eqPixels = std::memcmp(*pixels, tPixels, static_cast<size_t>(byteCount)) == 0;
    std::snprintf(a, sizeof(a), "%d bytes", byteCount);
    std::snprintf(b, sizeof(b), "%d bytes%s", tBytes, eqPixels ? "" : " (content differs)");
    ReportScreen(&sPixels, eqPixels, a, b);

    // S5 — the mod count. Argument SEVEN, on the stack.
    const bool eqCount = (mods->_size == tMods._size);
    RtDiffFormat(a, sizeof(a), mods->_size);
    RtDiffFormat(b, sizeof(b), tMods._size);
    ReportScreen(&sModCount, eqCount, a, b);

    // S6 — the mod elements: two Strings by content, three uints by value, and
    // NOT ModInfo+0x1c, which neither body writes.
    bool eqElems = eqCount;
    int  firstBad = -1;
    if (eqElems) {
        for (int i = 0; i < tMods._size; ++i) {
            const ModInfo& o = mods->_data[i];
            const ModInfo& t = tMods._data[i];
            if (!StringContentEqual(o.name, t.name) ||
                !StringContentEqual(o.path, t.path) ||
                o.versionMajor != t.versionMajor ||
                o.gameVersion  != t.gameVersion  ||
                o.versionMinor != t.versionMinor) {
                eqElems = false;
                firstBad = i;
                break;
            }
        }
    }
    std::snprintf(a, sizeof(a), "%d element(s)", eqCount ? mods->_size : -1);
    if (firstBad >= 0)
        std::snprintf(b, sizeof(b), "first mismatch at element %d", firstBad);
    else
        std::snprintf(b, sizeof(b), "%d element(s)%s", tMods._size,
                      eqCount ? "" : " (count already differs)");
    ReportScreen(&sModElems, eqElems, a, b);

    // The aggregate, on this function's own site.
    const bool allEqual = eqVersion && eqExtent && eqDesc && eqPixels &&
                          eqCount && eqElems;
    std::snprintf(a, sizeof(a), "v=%d %dx%d %d mods", *version, *width, *height, mods->_size);
    std::snprintf(b, sizeof(b), "v=%d %dx%d %d mods", tVersion, tW, tH, tMods._size);
    RtDiffReport(&rt_diff_site__, allEqual, a, b);

    // ---- tear the scratch down ---------------------------------------------
    // Every one of these is OUR extra allocation, made only because the oracle
    // ran; none is a deviation from retail. Theirs already freed the input copy
    // it was handed and zeroed the scratch SavedState — the same tail this port
    // reproduces — so that block must NOT be freed a second time here.
    ReleaseStringSlot(&tDesc);
    if (tPixels != nullptr) MemFree(tPixels);
    tMods._size = tModsSizeReal;
    DestroyScratchModArray(&tMods);
    if (inCopy != nullptr) MemFree(inCopy);
}

// ============================================================================
// Registration — BOTH doors.
// ----------------------------------------------------------------------------
// The mangled name is a SINGLE STRING LITERAL on ONE line: tools/runtime_census.py
// matches `RegisterRuntimeExport("<literal>"` and can see through neither a
// named constant nor a line break, and both blind spots have already produced a
// silent under-count of live ports in this project.
//
// The paired RegisterRuntimeAddr is not decoration. A name path that is later
// mistyped or removed sends the call straight back to the DLL with no
// diagnostic whatsoever — this project's single most repeated failure mode — and
// a second door costs one line. It does NOT get its own `++n`: `n` counts ported
// FUNCTIONS, and this is a second door onto one function.
// ============================================================================
int RtSaveLoadMetaRegister() {
    int n = 0;
    RegisterRuntimeExport("?LoadMetaData@SaveLoad@Save@@QEAAXAEAVString@System@@PEAPEAXAEAH22AEAV?$Array@UModInfo@SaveLoad@Save@@@Collection@@@Z", (void*)&RtSave_SaveLoad_LoadMetaData); ++n;
    RegisterRuntimeAddr(0x1800c1b30, (void*)&RtSave_SaveLoad_LoadMetaData);
    return n;
}
