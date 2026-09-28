// ============================================================================
// runtime/src/object/external_load.cpp — Wave 11 follow-up 2, lane IOSTREAM
//   `Object::External::Load`                                 retail 0x1800bc620
// ============================================================================
//
//   ?Load@External@Object@@SAPEAV12@AEBVFilename@System@@PEAV12@@Z
//
// `SA` IS STATIC. There is no `this`: the arguments are (const Filename& path,
// External* existing) in RCX and RDX, and the return is `PEAV12@` — a plain
// pointer in RAX with NO hidden return slot. Both halves of that matter, and
// both have shipped wrong in this tree before
// (reference_hidden_retslot_wild_write.md). The call site settles it:
// 0x1800bc642 `MOV RDI,RDX` and 0x1800bc645 `MOV RBX,RCX`, and the epilogue is
// 0x1800bc921 `MOV RAX,RBX`.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES — AND IT IS NOT "construct a stream and call Serialize"
// ---------------------------------------------------------------------------
// Read a COMPILED resource file back into an object:
//
//   1. build an `IO::CompressedFileStream` ON THE STACK, inlined: the
//      `CompressedStream` base ctor, its vtable, then the `BinaryFileStream`
//      member at +0x68;
//   2. open the file through the BinaryFileStream's OWN slot 7 (`Open`, NOT
//      `Close` — see the FileStream note in runtime/types/io/FileStream.h),
//      then layer `CompressedStream::Open` over it;
//   3. ask the stream's `IO::Encoding` for a Serializer implementation
//      (`+0x18` when reading, `+0x20` when writing);
//   4. check the magic number 0x5412a91f;
//   5. read a TYPE ID and, when the caller passed no object, CREATE ONE
//      through `BaseTypeInfo::factoryFn` (+0x20) and check it really is an
//      `Object::External`;
//   6. check the object's VERSION against its own virtual at +0x18;
//   7. replay the DEPENDENCY ARRAY — a count, then per element a `String` and
//      two ints;
//   8. finally dispatch the object's own `Serialize` at vtable +0x28;
//   9. unwind: delete the Serializer, `Commit` if this was a write, close the
//      file, and run the inlined three-level destructor chain.
//
// Steps 3-7 are the part the lane brief did not have: this is a full
// deserialisation driver, and step 5 is a live use of the OBJECT CREATION PATH
// (`BaseTypeInfo::factoryFn`) that Wave 11 is about. Nothing new is ported for
// it — the factory pointer belongs to the DLL's own typeinfo and is called
// through it — but the row is not the two-liner "construct, Open, Serialize"
// that `SaveText` is.
//
// ---------------------------------------------------------------------------
// `IO::Serializer` IS A 16-BYTE FACADE, NOT THE OBJECT WITH THE VTABLE
// ---------------------------------------------------------------------------
// This is the one thing in the function that cannot be read off any single
// instruction, and getting it wrong would mis-shape every call below. Retail
// keeps TWO adjacent stack slots and uses them differently:
//
//     [rsp+0x20]  the Stream*        — 1800bc69b writes &cfs into it, and
//                                      1800bc7ab reads `[[rsp+0x20]+8]`, i.e.
//                                      Stream::direction, back out of it
//     [rsp+0x28]  the implementation — 1800bc6ca stores the Encoding factory's
//                                      result, every `[rax+0x98]`/`[rax+0xa0]`
//                                      call dispatches through it, and
//                                      1800bc841 deletes it with `vtbl[0](1)`
//
// and it passes the ADDRESS OF THE PAIR (`LEA RDX,[RSP+0x20]`) to
// `?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z`, whose mangled name
// says its argument is an `IO::Serializer&`. So `IO::Serializer` IS that pair:
// a stream plus an implementation. The Encoding factory is handed the pair
// with only the stream filled in and returns the implementation.
//
// ---------------------------------------------------------------------------
// NO ADDED NULL GUARDS, AND ONE OF THEM IS DELIBERATE
// ---------------------------------------------------------------------------
// When the type id is not in `Object::Factory`'s tree, retail loads a ZERO into
// RCX (0x1800bc73b `MOV RCX,RBX`, RBX having been zeroed at 0x1800bc6a5) and
// immediately dereferences it at 0x1800bc73e. That is the same load-bearing
// fault `Factory::CreateObject` and `Base::IsDerivedFrom` take, for the same
// reason type_system.cpp records: it is how a missing type registration
// announces itself instead of silently producing a wrong object. Transcribed,
// not guarded (the project rule: find the upstream null, do not paper over a
// faithful crash).
//
// ---------------------------------------------------------------------------
// ORACLE
// ---------------------------------------------------------------------------
// Unlike `SaveText`, this row IS reachable in play — it is the read side of
// resource loading and the Encoding it uses is the BINARY one, whose Serializer
// factories are real code in the shipped DLL (the TEXT ones are stubs; see
// external_savetext_probe.cpp). Verification is therefore live: the counters
// below name every branch, and `RT_DIFF` reports entry counts. See
// WAVE11F_LANE_IOSTREAM.md for what was and was not exercised.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "../system/string.h"
#include "../../funcs/rt_port.h"
#include "../../types/object/TypeInfo.h"   // BEFORE External.h -- see external_savetext_win.cpp
#include "../../types/object/External.h"
#include "../../types/io/Stream.h"
#include "../../types/io/FileStream.h"
#include "../../types/io/FileSystem.h"
#include "../collection/array_internal.h"  // ENDGAME B.3.2 step 2: kSpec_ExtArrayElem bridge

namespace RtObject {

using RtSystem::Filename;
using RtSystem::String;

namespace {

// ---------------------------------------------------------------------------
// `IO::Serializer` — the 16-byte facade described in the header above.
// ---------------------------------------------------------------------------
struct Serializer {
    RtIo::Stream* stream;  // +0x00
    void*         impl;    // +0x08
};
static_assert(sizeof(Serializer) == 0x10, "the pair retail keeps at [rsp+0x20]");

// The magic that opens every compiled resource. 0x1800bc6e5 compares against it
// verbatim; it is not derived from anything.
constexpr uint32_t kCompiledMagic = 0x5412a91fu;

// ---- the two raw function-pointer shapes this body calls through ----------
// B3R RT-Core: CompressedStream::Open / Commit, FileSystem::CloseFile,
// Base::IsDerivedFrom and String::Serialize are called by name.
using SerFactoryFn  = void* (*)(void* encoding, Serializer* ser, int flag);
using FactoryFn     = void* (*)();

// `Collection::Array<ExternalArrayElement>::SetSize`, 0x18002c6a0. NOT exported
// in either build, so this is a genuine ADDRESS door and the only one this file
// adds to the cutover surface (reference_porting_can_grow_the_cutover_surface.md).
//
// *** THIS PREDICTION CAME TRUE AND IS NOW CLOSED. *** The sentence that stood
// here - "nothing in the tree registers it today; at cutover it needs a body or
// this call site needs to be revisited" - named its SYMBOL, its CONDITION and
// its CONSEQUENCE, and cutover ATTEMPT 29 died exactly here, EXEC-of-0 at
// ArraySetSize+0x4d.
//
// WAVE 27 LANE FRONTIER supplied the body. 0x18002c6a0 is registered by
// runtime/src/collection/array.cpp (`kSpec_ExtArrayElem`, two-argument form),
// and its two per-element helpers - 0x1800067d0 (Filename copy ctor, already
// reached by kSpec_PkgEntry) and 0x180006ac0 (the range destroy, ported as
// `RtColl_DestroyStrings_180006ac0` under kD_Str_10) - are registered too. So
// ResolveAddr below is registry-first and lands on our body with the DLL
// present or absent.
//
// THE DOOR IS STILL A DOOR. It resolves through the port registry, which means
// it is only as good as those three registrations: they are all inside
// `RtCollectionPortInit` / `RtCollectionDestroyPortInit`, gated on
// the old DLL-era switch (and the old DLL-era switch for the second). With
// either gate off at cutover this call site is an EXEC-of-0 again, at the same
// instruction, and the crash will look identical to attempt 29's.
// ENDGAME B.3.2 step 2 — `External::_refs` (runtime/types/object/External.h)
// is already `Collection::Array<ExternalArrayElement>`, and `kSpec_ExtArrayElem`
// is NON-TRIVIAL (a Filename-copy-ctor assign and a two-argument range destroy,
// not a byte trait), so the one call site below is a hand-derived
// `rtcoll::AsArray<&kSpec_ExtArrayElem>` bridge rather than a plain
// `.SetSize(n)`. The 0x18002c6a0 row stays registered in array.cpp.
void ArraySetSize(void* arr, int32_t n) {
    rtcoll::AsArray<&rtcoll::kSpec_ExtArrayElem>(arr)->SetSize(n);
}

void CloseIfOpen(RtIO::FileInfo* fi) {
    if (fi->handle == -1) return;
    // ENDGAME B.3.9 SS1 (B39_RULES D7): the class static now, read fresh.
    ::IO::FileSystem::_singleton->CloseFile(*fi);                 // 0x1800accc0
}

// The two Serializer-implementation entry points this body drives, named once.
// B3X W0: called by name, FOLLOWING THE RETAIL OFFSET, not these helpers' old
// names. `SerU32` calls +0xa0, which is `SerializeInt`; `SerI32` calls +0x98,
// which is `SerializeUInt` (Serializer.h:504-505). The helper names were
// swapped against their slots; the offsets are retail's and are what stays.
// Both pass the third argument (the name) as zero, as retail does.
void SerU32(void* impl, uint32_t* v) {
    reinterpret_cast<::IO::ReaderWriter*>(impl)->SerializeInt(     // +0xa0 (B.3.x bridge)
        *reinterpret_cast<int32_t*>(v), nullptr);
}
void SerI32(void* impl, int32_t* v) {
    reinterpret_cast<::IO::ReaderWriter*>(impl)->SerializeUInt(    // +0x98 (B.3.x bridge)
        *reinterpret_cast<uint32_t*>(v), nullptr);
}

}  // namespace

// ---------------------------------------------------------------------------
// EFFECTIVENESS COUNTERS — branches taken INSIDE this body.
//
// `calls` alone cannot tell a load that did the interesting thing from one that
// bailed at the magic check
// (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md). The ones
// that matter are `created` (step 5, the object-creation path) against
// `reusedExisting`, and `deps` — a run in which every resource had zero
// dependencies would leave the whole replay loop unentered while reporting a
// large `calls`.
// ---------------------------------------------------------------------------
struct LoadCounters {
    uint64_t calls;
    uint64_t openFileFailed;
    uint64_t openCompFailed;
    uint64_t nullSerializer;
    uint64_t badMagic;
    uint64_t created;         // existing == null -> factoryFn ran
    uint64_t reusedExisting;
    uint64_t notAnExternal;   // IsDerivedFrom said no
    uint64_t badVersion;
    uint64_t sizedArray;      // the read-side SetSize ran
    uint64_t deps;            // dependency elements replayed
    uint64_t serialized;      // reached the object's own Serialize
    uint64_t committed;       // the write-side Commit ran
    uint64_t returnedNull;
};
LoadCounters g_lc{};

// ---------------------------------------------------------------------------
// NEGATIVE-CONTROL PERTURBATIONS, read by the body below and set only by
// external_load_probe.cpp. Each is consumed at exactly one site in the single
// implementation under test, each increments `g_lcAtt` there, and neither
// changes a pointer, a size or a stride -- so neither can write out of bounds.
//   1  the two per-element int32s read in the OPPOSITE ORDER
//   2  the dependency COUNT through the int32 overload instead of the uint32 one
// ---------------------------------------------------------------------------
// ============================================================================
// Object::External::Load — the body.                     retail 0x1800bc620
// ============================================================================
extern "C" External* RtObject_External_Load(const Filename* path, External* existing) {
    ++g_lc.calls;

    External* obj = existing;      // RDI, and it is REASSIGNED at 1800bc745

    // `keep` IS THE CHOICE BETWEEN RETAIL'S TWO JOIN POINTS, and it is the only
    // thing that decides what this function returns.
    //   1800bc85b  runs `MOV RBX,RDI` -> the result is the object
    //   1800bc88b  is entered PAST that -> the result is the null RBX was given
    //              at 1800bc6a5
    // Both Open failures jump to 85b, so a file that will not open returns
    // `existing` (usually null) and NOT an error; only a bad magic, a bad type
    // or a bad version reach 88b. Defaulting to true is what makes that read.
    bool keep = true;

    // ---- 1800bc648..1800bc668 -------------------------------------------
    // The inlined `CompressedFileStream` constructor. DELIBERATELY NOT
    // zero-initialised: the two ctors write every field retail reads, and
    // CompressedStream +0x18/+0x24 are ones nothing writes -- a `= {}` would be
    // a behavioural difference, which is the position saveload_meta.cpp already
    // takes on this very class.
    // ENDGAME B.3.9 IO (foreign site, IO's class): the class's real constructor
    // (??0CompressedStream, the vptr, ??0BinaryFileStream at +0x68 -- the same
    // three steps in the same order) on raw stack storage. The body below reads
    // the object through the RtIo POD view, unchanged.
    alignas(IO::CompressedFileStream) unsigned char cfsMem[sizeof(IO::CompressedFileStream)];
    IO::CompressedFileStream* const cfsObj =
        ::new (static_cast<void*>(cfsMem)) IO::CompressedFileStream();
    RtIo::CompressedFileStream& cfs = *reinterpret_cast<RtIo::CompressedFileStream*>(cfsObj);

    // ---- 1800bc669..1800bc695 -------------------------------------------
    // Slot 7 of a FileStream is `Open`. On the MemoryStream/CompressedStream
    // branch of this hierarchy the same slot is `Close`; a port that assumed
    // one shape for both would call the wrong virtual here and it would not
    // crash (runtime/types/io/FileStream.h states the trap in full).
    // B3X W0: `IO::FileStream::Open`, slot 7, by name on the embedded
    // BinaryFileStream (+0x68) the real constructor above built.
    if (!static_cast<::IO::FileStream&>(cfsObj->_file).Open(
            *reinterpret_cast<const ::System::Filename*>(path))) {   // B.3.x bridge
        ++g_lc.openFileFailed;
    } else if (!RtIo::RtIo_CompressedStream_Open(                   // 0x1800aa560
                   &cfs.base, &cfs.file.base.base, 1)) {
        ++g_lc.openCompFailed;
    } else {
        // ---- 1800bc69b..1800bc6ca ---------------------------------------
        // Build the Serializer facade and ask the Encoding for the half it
        // does not have. READING takes +0x18, WRITING takes +0x20 -- and this
        // one branch is the whole difference between a load and a save here.
        Serializer ser;
        ser.stream = &cfs.base.base;
        ser.impl   = nullptr;

        void*  enc   = cfs.base.base.encoding;
        void** encVt = *reinterpret_cast<void***>(enc);
        const size_t slot = (cfs.base.base.direction == 0) ? 0x18 : 0x20;   // 1800bc6bc
        ser.impl = reinterpret_cast<SerFactoryFn>(encVt[slot / sizeof(void*)])(
                       enc, &ser, 0);

        bool bad = false;

        // ---- 1800bc6cf..1800bc6ed -----------------------------------------
        // Retail does NOT initialise this slot before the call: on a read the
        // serializer fills it, on a write it emits whatever is there. Left
        // uninitialised for the same reason the stream is.
        uint32_t magic;
        SerU32(ser.impl, &magic);
        if (magic != kCompiledMagic) {
            ++g_lc.badMagic;
            bad = true;
        } else {
            // ---- 1800bc6f3..1800bc70e -------------------------------------
            int32_t typeId = 0;                                   // 1800bc6f3
            SerI32(ser.impl, &typeId);

            if (obj != nullptr) {
                ++g_lc.reusedExisting;                            // 1800bc70e
            } else {
                // ---- 1800bc710..1800bc761 -------------------------------
                // Look the type up and BUILD one. `Factory::_singleton`'s tree,
                // then `BaseTypeInfo::factoryFn` at +0x20 -- the object-creation
                // path, reached here through the DLL's own typeinfo.
                const Factory* f = reinterpret_cast<const Factory*>(   // B.3.3 bridge
                    ::Object::Factory::_singleton);   // class static (B.3.9 OBJ, D7)
                const TypeNode* n = f->root;
                const uint32_t want = static_cast<uint32_t>(typeId);
                while (n != nullptr) {
                    if (want < n->key)      n = n->left;          // 1800bc732, +0x38
                    else if (n->key < want) n = n->right;         // 1800bc72c, +0x40
                    else break;                                   // 1800bc72a
                }

                // FAITHFUL FAULT ON A MISS. 1800bc73b puts ZERO in RCX and
                // 1800bc73e dereferences it. Read through a volatile pointer so
                // the compiler cannot decide the null path is unreachable and
                // delete the surrounding code -- the deref has to actually
                // happen, because that fault IS how a missing registration
                // reports itself (type_system.cpp, "NO ADDED NULL GUARDS").
                const BaseTypeInfo* const volatile* infoSlot =
                    reinterpret_cast<const BaseTypeInfo* const volatile*>(
                        reinterpret_cast<const uint8_t*>(n) + 0x10);
                const BaseTypeInfo* info = *infoSlot;

                obj = static_cast<External*>(
                          reinterpret_cast<FactoryFn>(info->factoryFn)());   // 1800bc742
                ++g_lc.created;

                // `External::TypeId()` (0x180029350): the record's +0x00,
                // through the Object accessor slot (external_class.cpp,
                // ENDGAME B.3.9 OBJ D6 step 1) -- no lazy RT_DATA_SLOT cache
                // on the loader thread.
                const bool isExternal =                           // 0x1800c0390
                    reinterpret_cast<const ::Object::Base*>(obj)->IsDerivedFrom(
                        ::Object::External::TypeId());
                if (!isExternal || obj == nullptr) {              // 1800bc758/761
                    ++g_lc.notAnExternal;
                    bad = true;
                }
            }

            if (!bad) {
                // ---- 1800bc767..1800bc78a -----------------------------------
                uint32_t version;
                SerU32(ser.impl, &version);
                const int32_t objVersion =          // slot 3 +0x18 (B.3.x bridge)
                    reinterpret_cast<::Object::External*>(obj)->GetFileVersion();
                if (static_cast<int32_t>(version) != objVersion) {
                    ++g_lc.badVersion;
                    bad = true;
                }
            }

            if (!bad) {
                // ---- 1800bc790..1800bc81e -------------------------------
                // The dependency array. Retail SEEDS the local with the
                // object's CURRENT size and then serializes it, so the same
                // instruction reads on a load and writes on a save.
                int32_t depCount = obj->_refs._size;              // 1800bc790
                SerU32(ser.impl, reinterpret_cast<uint32_t*>(&depCount));

                if (ser.stream->direction == 0) {                 // 1800bc7b0
                    ++g_lc.sizedArray;
                    ArraySetSize(&obj->_refs, depCount);          // 1800bc7bd
                }

                // Each element is 0x10 bytes: a String at +0x00 and TWO int32s
                // at +0x08 and +0x0c. External.h calls the tail one opaque
                // `uint64_t`; this body is the writer that splits it, and it
                // splits it in two, not one.
                uint8_t* elem = reinterpret_cast<uint8_t*>(obj->_refs._data);
                for (int32_t i = 0; i < depCount; ++i, elem += 0x10) {
                    ++g_lc.deps;
                    reinterpret_cast<::System::String*>(elem)->Serialize(&ser);   // 1800bc7e9
                    SerI32(ser.impl, reinterpret_cast<int32_t*>(elem + 0x08));  // 1800bc7fc
                    SerI32(ser.impl, reinterpret_cast<int32_t*>(elem + 0x0c));  // 1800bc811
                }

                // ---- 1800bc820..1800bc82b -------------------------------
                // The object reads (or writes) ITSELF, through the same slot
                // ExternalFactory::Add and External::SaveText drive.
                ++g_lc.serialized;
                reinterpret_cast<::Object::External*>(obj)->Serialize(&cfs);   // slot 5 +0x28 (B.3.x bridge)
            }
        }

        // ---- 1800bc82f / 1800bc845 --------------------------------------
        // The Serializer implementation's DELETING destructor, `vtbl[0](1)`.
        // Both the good and the bad exits run it; they differ only in where
        // they go next, which is what decides the return value.
        if (ser.impl != nullptr) {
            delete reinterpret_cast<::IO::ReaderWriter*>(ser.impl);   // slot 0 (B.3.x bridge)
        } else {
            ++g_lc.nullSerializer;
        }

        keep = !bad;
    }

    // ---- 1800bc85b..1800bc888 -------------------------------------------
    // The 85b join: flush a write, close the file, and adopt the object as the
    // result. Reached from the two Open failures AND from the successful exit;
    // the bad-magic / bad-type / bad-version exits jump PAST it, which is
    // exactly why they return null and an Open failure returns `existing`.
    if (keep) {
        if (cfs.base.headerSize > 0 && cfs.base.base.direction == 1) {   // 1800bc85b/861
            ++g_lc.committed;
            RtIo::RtIo_CompressedStream_Commit(&cfs.base);                 // 0x1800a9ee0
        }
        CloseIfOpen(&cfs.file.base.file);                                // 1800bc872
    }
    External* result = keep ? obj : nullptr;                             // 1800bc888
    if (result == nullptr) ++g_lc.returnedNull;

    // ---- 1800bc88b..1800bc921 -------------------------------------------
    // The inlined ~CompressedFileStream chain. The Commit/CloseFile pair
    // appears a SECOND time here -- once as the stream's Close and once inside
    // the destructor -- and both are transcribed rather than collapsed: they
    // are two different statements in retail, and CloseFile is idempotent on a
    // closed FileInfo.
    // ENDGAME B.3.9 IO: that chain is the class's own destructors -- the
    // ~CompressedFileStream body (Commit guard, CloseFile), the member's
    // ~BinaryFileStream (~FileStream, ~Stream), ~CompressedStream (Release, the
    // three Array dtors +0x50/+0x40/+0x30, ~Stream frees the encoding) -- same
    // order, same frees, at the same position; no free of the storage.
    cfsObj->IO::CompressedFileStream::~CompressedFileStream();                    // 1800bc88b..91c

    return result;                                                         // 1800bc921
}

void DumpLoadCounters() {
    printf("[ExtLoad] calls=%llu openFileFail=%llu openCompFail=%llu nullSer=%llu "
                "badMagic=%llu created=%llu reused=%llu notExternal=%llu badVer=%llu "
                "sized=%llu deps=%llu serialized=%llu committed=%llu retNull=%llu\n",
                (unsigned long long)g_lc.calls,
                (unsigned long long)g_lc.openFileFailed,
                (unsigned long long)g_lc.openCompFailed,
                (unsigned long long)g_lc.nullSerializer,
                (unsigned long long)g_lc.badMagic,
                (unsigned long long)g_lc.created,
                (unsigned long long)g_lc.reusedExisting,
                (unsigned long long)g_lc.notAnExternal,
                (unsigned long long)g_lc.badVersion,
                (unsigned long long)g_lc.sizedArray,
                (unsigned long long)g_lc.deps,
                (unsigned long long)g_lc.serialized,
                (unsigned long long)g_lc.committed,
                (unsigned long long)g_lc.returnedNull);
}

}  // namespace RtObject
