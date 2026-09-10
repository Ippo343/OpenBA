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
#include "../../funcs/rt_diff.h"
#include "../../types/object/TypeInfo.h"   // BEFORE External.h -- see external_savetext_win.cpp
#include "../../types/object/External.h"
#include "../../types/io/Stream.h"
#include "../../types/io/FileStream.h"
#include "../../types/io/FileSystem.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

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

// ---- doors ----------------------------------------------------------------
// All names except the four addresses called out below.
using CtorFn        = void* (*)(void*);
using StreamOpenFn  = bool  (*)(void* self, const Filename* path);   // FileStream vtbl +0x38
using CompOpenFn    = bool  (*)(RtIo::CompressedStream* self, RtIo::Stream* src, int flag);
using VoidSelfFn    = void  (*)(void* self);
using CloseFileFn   = void  (*)(RtIO::FileSystem*, RtIO::FileInfo*);
using IsDerivedFn   = bool  (*)(const void* self, uint32_t typeId);
using StrSerFn      = void  (*)(String* self, Serializer* ser);
using SerFactoryFn  = void* (*)(void* encoding, Serializer* ser, int flag);
using SerU32Fn      = void  (*)(void* impl, uint32_t* value, const char* name);
using SerI32Fn      = void  (*)(void* impl, int32_t*  value, const char* name);
using FactoryFn     = void* (*)();
using GetVersionFn  = int32_t (*)(void* self);
using DeleteFn      = void* (*)(void* self, int flags);

// `Collection::Array<ExternalArrayElement>::SetSize`, 0x18002c6a0. NOT exported
// in either build, so this is a genuine ADDRESS door and the only one this file
// adds to the cutover surface (reference_porting_can_grow_the_cutover_surface.md).
// Nothing in the tree registers it today; at cutover it needs a body or this
// call site needs to be revisited. Recorded rather than hidden behind a helper.
void ArraySetSize(void* arr, int32_t n) {
    using Fn = void* (*)(void*, int32_t);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18002c6a0));
    fn(arr, n);
}

// The three `Collection::Array<T>` destructors ~CompressedStream's caller runs
// on the stream's own members, reached by address for the same reason
// compressed_stream_release.cpp reaches them that way: none has an exported
// name, and all three are already registered by lane Q / lane PKG2, so these
// resolve to OUR bodies rather than to the DLL's.
void ArrayDtor(uintptr_t addr, void* arr) {
    using Fn = void* (*)(void*);
    Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, addr));
    fn(arr);
}

// The Encoding's deleting destructor, 0x180006a10 — the same door
// external_add.cpp and external_savetext_win.cpp open.
void EncodingDelete(void* p) {
    static VoidSelfFn fn = nullptr;
    if (!fn) fn = reinterpret_cast<VoidSelfFn>(ResolveAddr(Module::Runtime, 0x180006a10));
    fn(p);
}

void CloseIfOpen(RtIO::FileInfo* fi) {
    if (fi->handle == -1) return;
    RtIO::FileSystem* fs = *RT_DATA_SLOT(RtIO::FileSystem**,
                                         "?_singleton@FileSystem@IO@@0PEAV12@EA");
    RT_EXPORT_ONCE(CloseFileFn,
        "?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z")(fs, fi);
}

// A slot of an object's own vtable, by BYTE offset, so a call site reads the
// same way the disassembly does (`CALL qword ptr [RAX+0x38]`).
void* VSlot(const void* obj, size_t byteOffset) {
    void** vtbl = *reinterpret_cast<void** const*>(obj);
    return vtbl[byteOffset / sizeof(void*)];
}

// The two Serializer-implementation entry points this body drives, named once.
// +0xa0 takes a uint32 and +0x98 an int32; both take a third argument retail
// always passes as zero.
void SerU32(void* impl, uint32_t* v) {
    reinterpret_cast<SerU32Fn>(VSlot(impl, 0xa0))(impl, v, nullptr);
}
void SerI32(void* impl, int32_t* v) {
    reinterpret_cast<SerI32Fn>(VSlot(impl, 0x98))(impl, v, nullptr);
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
int      g_lcCtl = 0;
uint64_t g_lcAtt = 0;

// external_load_probe.cpp -- the ours-vs-theirs replay, armed from the tail of
// the body. Declared here rather than in a header because the probe file
// exposes none of its own, which is how every other probe in this directory is
// wired.
void RtObjectLoadProbeStep(const Filename* path, External* ours);

// ============================================================================
// Object::External::Load — the body.                     retail 0x1800bc620
// ============================================================================
extern "C" External* RtObject_External_Load(const Filename* path, External* existing) {
    RT_DIFF_BEGIN("Object::External::Load");
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
    RtIo::CompressedFileStream cfs;
    RT_EXPORT_ONCE(CtorFn, "??0CompressedStream@IO@@QEAA@XZ")(&cfs.base);
    cfs.base.base.vptr = RT_DATA_SLOT(const void*, "??_7CompressedFileStream@IO@@6B@");
    RT_EXPORT_ONCE(CtorFn, "??0BinaryFileStream@IO@@QEAA@XZ")(&cfs.file);

    // ---- 1800bc669..1800bc695 -------------------------------------------
    // Slot 7 of a FileStream is `Open`. On the MemoryStream/CompressedStream
    // branch of this hierarchy the same slot is `Close`; a port that assumed
    // one shape for both would call the wrong virtual here and it would not
    // crash (runtime/types/io/FileStream.h states the trap in full).
    if (!reinterpret_cast<StreamOpenFn>(VSlot(&cfs.file, 0x38))(&cfs.file, path)) {
        ++g_lc.openFileFailed;
    } else if (!RT_EXPORT_ONCE(CompOpenFn,
                   "?Open@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z")(
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
                const Factory* f =
                    *RT_DATA_SLOT(Factory**, "?_singleton@Factory@Object@@0PEAV12@EA");
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

                const BaseTypeInfo* extInfo = RT_DATA_SLOT(const BaseTypeInfo*,
                    "?_typeInfo@External@Object@@0VBaseTypeInfo@2@A");
                const bool isExternal = RT_EXPORT_ONCE(IsDerivedFn,
                    "?IsDerivedFrom@Base@Object@@QEBA_NI@Z")(obj, extInfo->typeId);
                if (!isExternal || obj == nullptr) {              // 1800bc758/761
                    ++g_lc.notAnExternal;
                    bad = true;
                }
            }

            if (!bad) {
                // ---- 1800bc767..1800bc78a -----------------------------------
                uint32_t version;
                SerU32(ser.impl, &version);
                const int32_t objVersion =
                    reinterpret_cast<GetVersionFn>(VSlot(obj, 0x18))(obj);
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
                if (g_lcCtl == 2) { ++g_lcAtt; SerI32(ser.impl, &depCount); }
                else SerU32(ser.impl, reinterpret_cast<uint32_t*>(&depCount));

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
                    RT_EXPORT_ONCE(StrSerFn,
                        "?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z")(
                            reinterpret_cast<String*>(elem), &ser);      // 1800bc7e9
                    if (g_lcCtl == 1) {
                        ++g_lcAtt;
                        SerI32(ser.impl, reinterpret_cast<int32_t*>(elem + 0x0c));
                        SerI32(ser.impl, reinterpret_cast<int32_t*>(elem + 0x08));
                    } else {
                        SerI32(ser.impl, reinterpret_cast<int32_t*>(elem + 0x08));  // 1800bc7fc
                        SerI32(ser.impl, reinterpret_cast<int32_t*>(elem + 0x0c));  // 1800bc811
                    }
                }

                // ---- 1800bc820..1800bc82b -------------------------------
                // The object reads (or writes) ITSELF, through the same slot
                // ExternalFactory::Add and External::SaveText drive.
                ++g_lc.serialized;
                using SerializeFn = bool (*)(External*, void*);
                reinterpret_cast<SerializeFn>(VSlot(obj, 0x28))(obj, &cfs);
            }
        }

        // ---- 1800bc82f / 1800bc845 --------------------------------------
        // The Serializer implementation's DELETING destructor, `vtbl[0](1)`.
        // Both the good and the bad exits run it; they differ only in where
        // they go next, which is what decides the return value.
        if (ser.impl != nullptr) {
            reinterpret_cast<DeleteFn>(VSlot(ser.impl, 0))(ser.impl, 1);
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
            RT_EXPORT_ONCE(VoidSelfFn, "?Commit@CompressedStream@IO@@QEAAXXZ")(&cfs.base);
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
    cfs.base.base.vptr = RT_DATA_SLOT(const void*, "??_7CompressedFileStream@IO@@6B@");
    if (cfs.base.headerSize > 0 && cfs.base.base.direction == 1)          // 1800bc890/896
        RT_EXPORT_ONCE(VoidSelfFn, "?Commit@CompressedStream@IO@@QEAAXXZ")(&cfs.base);
    CloseIfOpen(&cfs.file.base.file);                                     // 1800bc8a7

    // ~BinaryFileStream: retail calls the exported ??1FileStream DIRECTLY on
    // the member at +0x68 (1800bc8c6). BinaryFileStream adds no fields and its
    // own destructor only re-stamps a vtable nothing reads afterwards, so MSVC
    // elided it -- transcribed as written rather than "corrected" to the
    // derived destructor.
    RT_EXPORT_ONCE(CtorFn, "??1FileStream@IO@@UEAA@XZ")(&cfs.file);

    cfs.base.base.vptr = RT_DATA_SLOT(const void*, "??_7CompressedStream@IO@@6B@");  // 1800bc8d0
    RT_EXPORT_ONCE(VoidSelfFn, "?Release@CompressedStream@IO@@QEAAXXZ")(&cfs.base);  // 1800bc8e1
    ArrayDtor(0x180024aa0, &cfs.base.blockData);                          // 1800bc8eb, +0x50
    ArrayDtor(0x1800249c0, &cfs.base.headerData);                         // 1800bc8f5, +0x40
    ArrayDtor(0x1800248e0, &cfs.base.windowData);                         // 1800bc900, +0x30

    cfs.base.base.vptr = RT_DATA_SLOT(const void*, "??_7Stream@IO@@6B@");  // 1800bc906
    if (cfs.base.base.encoding) EncodingDelete(cfs.base.base.encoding);    // 1800bc91c

    // The ours-vs-theirs replay. It must run AFTER the stream is fully torn
    // down: retail's second Load re-opens the same file, and doing that while
    // our own FileInfo still held a handle would measure a different situation
    // than the one under test. Opt-in, and it only compares the case the two
    // sides build comparably -- `existing == nullptr`.
    {
        static bool armed = (std::getenv("BANISHED_RT_EXTLOAD_PROBE") != nullptr);
        if (armed && existing == nullptr) RtObjectLoadProbeStep(path, result);
    }

    RT_DIFF_WORK(true);
    return result;                                                         // 1800bc921
}

void DumpLoadCounters() {
    std::printf("[ExtLoad] calls=%llu openFileFail=%llu openCompFail=%llu nullSer=%llu "
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

// ============================================================================
// Registration
// ============================================================================
int RtObjectExternalLoadRegister() {
    int n = 0;
    RegisterRuntimeExport("?Load@External@Object@@SAPEAV12@AEBVFilename@System@@PEAV12@@Z",
                          (void*)&RtObject::RtObject_External_Load); ++n;
    RegisterRuntimeAddr(0x1800bc620, (void*)&RtObject::RtObject_External_Load);

    std::printf("[RtPort]   Object::External::Load "
                "(wave 11f lane IOSTREAM): %d export registered\n", n);
    std::atexit(&RtObject::DumpLoadCounters);
    return n;
}
