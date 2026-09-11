// ============================================================================
// runtime/src/io/encoding.cpp — the IO::Encoding factory pair and the two
// IO::BinaryReader / IO::BinaryWriter constructors they call.  Wave 9 lane ENC.
// ============================================================================
//
//   0x1800a8120  IO::Encoding::CreateReader   (UNEXPORTED — vtable slot +0x18)
//   0x1800a8080  IO::Encoding::CreateWriter   (UNEXPORTED — vtable slot +0x20)
//   0x1800a8340  ??0BinaryReader@IO@@QEAA@AEAVSerializer@1@H@Z   (EXPORTED)
//   0x1800a8a30  ??0BinaryWriter@IO@@QEAA@AEAVSerializer@1@H@Z   (EXPORTED)
//
// ----------------------------------------------------------------------------
// WHY ALL FOUR, AND NOT JUST THE TWO THE ASSIGNMENT NAMED
// ----------------------------------------------------------------------------
// The row this lane was given is the two FACTORIES: they are slots 3 and 4 of
// the shared IO::Encoding vtable 0x1801554a8, which lane C2 mints BY ADDRESS
// with all five slots as RT_MINT_UNNAMED traps. Registering the two factories
// alone does replace those two traps — but each factory's whole body is
// "allocate 0x30, then CALL the constructor", and those constructors
// (0x1800a8340 / 0x1800a8a30) are reached by a DIRECT call, not through a
// minted slot. Porting only the factories therefore moves the stop two
// instructions later instead of removing it: at cutover the CALL lands on an
// address nothing registers. That is the "porting can grow the cutover surface"
// failure, so the unit ported here is the FACTORY AND ITS CONSTRUCTOR, twice.
//
// A CORRECTION TO THE INHERITED NOTES. Both constructors are EXPORTED, checked
// against the DLL's export directory by address, not assumed:
//     0x1800a8340 -> ??0BinaryReader@IO@@QEAA@AEAVSerializer@1@H@Z
//     0x1800a8a30 -> ??0BinaryWriter@IO@@QEAA@AEAVSerializer@1@H@Z
// "AEAVSerializer@1@H" is (Serializer&, int) and pins the signature that
// runtime/types/io/Serializer.h had only inferred from the call site. The two
// FACTORIES are genuinely unexported (the same check returns nothing for either
// address), which is why they are registered by ADDRESS ONLY and the two
// constructors are registered by BOTH name and address.
//
// ----------------------------------------------------------------------------
// THE FACTORIES — transcribed from 0x1800a8120..0x1800a81b3 and
// 0x1800a8080..0x1800a8113, which are the SAME 52 bytes except for the CALL
// target. Details the decompiled C loses:
//
//   * "MOV EDX,0x30" / "LEA R8D,[RDX + -0x2c]" — the alignment argument is 4,
//     computed off the size register rather than written as a literal. So the
//     request is MemAlloc(0x30, 4), and 0x30 is where sizeof(BinaryCodec) comes
//     from.
//   * The allocation is EnterCriticalSection(_singleton+0x640) / CALL
//     0x1800c6e60 (?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z) /
//     LeaveCriticalSection. That trio is
//     ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z (0x1800c7a50) INLINED —
//     the same identification runtime/src/io/compressedstream.cpp makes for the
//     identical trio in the two stream constructors, and it is why calling the
//     exported MemAlloc here is byte-equivalent rather than merely similar.
//   * RCX — the IO::Encoding "this" — is NEVER READ. Both factories overwrite
//     it before first use. The encoding object is a pure policy tag with no
//     state; "self" is accepted and ignored here for exactly that reason, and
//     that is faithful, not an omission.
//   * On allocation failure the body does XOR EAX,EAX and returns NULL
//     WITHOUT constructing. Reproduced. Retail's callers do not check it; that
//     is retail's problem to have, and a guard here would hide it rather than
//     fix it.
//
// ----------------------------------------------------------------------------
// THE CONSTRUCTORS — and the reason this row was worth taking
// ----------------------------------------------------------------------------
// These are NOT plain field-stampers. Each one performs three STREAM
// OPERATIONS through serializer->stream's vtable before it returns, so
// constructing a BinaryReader CONSUMES 12 bytes of the stream and constructing
// a BinaryWriter EMITS 12 bytes. That is the stream header, and it is why
// SaveSettings' first dispatch after building its stream is this factory.
//
// Stream vtable slots used (RETAIL offsets, from runtime/types/io/Stream.h,
// where they are read byte-wise out of ??_7Stream@IO@@6B@ at 0x1801549a0):
//     +0x10  Write(int size, const void* src) -> bool
//     +0x18  Read (int size, void* dst)       -> bool
//     +0x28  GetCurrentPos()                  -> int
//
// WRITER (0x1800a8a30), in order:
//     write 4 bytes of 0xAAAAAAAA            <- a fixed MAGIC, built on the
//                                               stack (MOV [RSP+0x50],
//                                               0xaaaaaaaa), never a field
//     _word20 = GetCurrentPos()              <- remembers where the next field
//                                               lands, for a later back-patch
//     write 4 bytes of _word24 (== -1)       <- the PLACEHOLDER, emitted from
//                                               the field just set to -1
//     _word24 = GetCurrentPos()
//     write 4 bytes of _version
//
// READER (0x1800a8340) is the mirror image, and the asymmetry is real:
//     read 4 bytes into a STACK SLOT that is never used   <- the magic is
//                                                            consumed and
//                                                            DISCARDED, not
//                                                            checked
//     read 4 bytes into _word20              <- the back-patched value
//     _word24 = GetCurrentPos()
//     read 4 bytes into _version             <- OVERWRITES the "version"
//                                               argument the caller passed
//
// THE READER'S "version" ARGUMENT IS DEAD ON EVERY SUCCESSFUL CALL. It is
// stored to +0x28 by MOV [RCX+0x28],R8D and then overwritten by the third
// Read. It survives only if that Read fails and leaves the destination alone —
// which is precisely why the store is still reproduced here instead of being
// "cleaned up" into a single assignment from the stream.
//
// TWO STORE-WIDTH DETAILS that the decompiled C prints identically and that are
// not identical:
//   * READER zeroes +0x20 and +0x24 with ONE 8-byte store
//     (MOV qword ptr [RCX+0x20],RAX, RAX just XORed). Writing the two u32
//     fields separately is byte-equivalent and is what is done here.
//   * WRITER writes them as TWO SEPARATE DWORDS (MOV dword [RCX+0x20],0 then
//     MOV dword [RCX+0x24],0xffffffff), because the values differ.
//
// Both constructors stamp ??_7ReaderWriter@IO@@6B@ FIRST and then overwrite it
// with the concrete table — the inlined ReaderWriter base constructor. Both
// stores are reproduced: they are two instructions in the shipped bytes and
// collapsing them would be a divergence for no gain.
//
// MOV RAX,RSI before the epilogue: MSVC constructors return "this", which is
// what makes the factory's "return Ctor(...)" produce the object pointer.
//
// ----------------------------------------------------------------------------
// ORACLE — see encoding_probe.cpp, and read its HONEST LIMITS section.
// ----------------------------------------------------------------------------
// There is deliberately NO per-call RT_DIFF site in this file, and that is a
// decision rather than an omission. A per-call oracle would have to run THEIRS
// alongside OURS, and both constructors MUTATE THE STREAM — theirs would
// consume or emit a second copy of the 12-byte header on the live save/load
// stream. The lockstep probe in encoding_probe.cpp instead gives each side its
// own MemoryStream over its own buffer, which is the only shape that can
// compare these two bodies without corrupting the thing they are called on.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"

// ResolveExport / ResolveExportRaw already come from rt_port.h; only the
// address door needs declaring here, and it carries Module with it.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module module, uintptr_t ghidraAddr);

namespace RtIo {

using rt::io::BinaryCodec;
using rt::io::Serializer;

namespace {

using MemAllocFn = void* (*)(uint64_t, int);

// The three vtables, all EXPORTED (verified against the export directory by
// address: 0x1801553b0 / 0x1801552e0 / 0x180155210 each resolve to exactly the
// name used here). Resolved once through the port-aware door so a later port of
// the tables themselves is picked up with no change here.
void** VtblReaderWriter() {
    static void** v = reinterpret_cast<void**>(ResolveExport("??_7ReaderWriter@IO@@6B@"));
    return v;
}
void** VtblBinaryReader() {
    static void** v = reinterpret_cast<void**>(ResolveExport("??_7BinaryReader@IO@@6B@"));
    return v;
}
void** VtblBinaryWriter() {
    static void** v = reinterpret_cast<void**>(ResolveExport("??_7BinaryWriter@IO@@6B@"));
    return v;
}

// The three IO::Stream virtuals these constructors dispatch through. Offsets
// are RETAIL's; the profile build's Encoding/ReaderWriter tables disagree and
// must NOT be used to check these (its NAMES win, its OFFSETS do not).
using StreamRwFn  = bool (*)(void* self, int32_t size, void* buf);
using StreamPosFn = int32_t (*)(void* self);

constexpr int kStreamWrite  = 0x10;
constexpr int kStreamRead   = 0x18;
constexpr int kStreamGetPos = 0x28;

inline void* VSlot(void* obj, int off) {
    return (*reinterpret_cast<void***>(obj))[off / 8];
}

inline bool StreamRead(void* s, int32_t n, void* dst) {
    return reinterpret_cast<StreamRwFn>(VSlot(s, kStreamRead))(s, n, dst);
}
inline bool StreamWrite(void* s, int32_t n, void* src) {
    return reinterpret_cast<StreamRwFn>(VSlot(s, kStreamWrite))(s, n, src);
}
inline int32_t StreamGetPos(void* s) {
    return reinterpret_cast<StreamPosFn>(VSlot(s, kStreamGetPos))(s);
}

// ---------------------------------------------------------------------------
// ATTENDANCE COUNTERS — NOT A VERIFICATION
// ---------------------------------------------------------------------------
// These count how many times each of the four bodies was ENTERED. They compare
// nothing and they prove nothing about correctness; encoding_probe.cpp is the
// only thing in this lane that compares behaviour.
//
// They exist because "the probe is green" and "the port runs" are DIFFERENT
// claims and a green probe over a port nothing calls is worthless. The two
// numbers are deliberately produced by different machinery and printed under
// different labels so that neither can be read as the other.
//
// They are also NOT RtDiff sites on purpose. An RtDiff site only increments its
// `calls` counter from inside a COMPARISON, so a site that declared work but
// never compared would print `calls=0 worked=N` — which reads as "never ran"
// and means the opposite.
struct EncCounts {
    unsigned long long createReader = 0, createWriter = 0;
    unsigned long long ctorReader   = 0, ctorWriter   = 0;
    ~EncCounts() {
        if ((createReader | createWriter | ctorReader | ctorWriter) == 0) {
            printf("[Encoding] ATTENDANCE: none of the four bodies was "
                        "entered in this run.\n");
            return;
        }
        printf("[Encoding] ATTENDANCE (entries, not comparisons): "
                    "CreateReader=%llu CreateWriter=%llu "
                    "BinaryReader::ctor=%llu BinaryWriter::ctor=%llu\n",
                    createReader, createWriter, ctorReader, ctorWriter);
    }
};
EncCounts g_encCounts;

}  // namespace

// ---------------------------------------------------------------------------
// ??0BinaryReader@IO@@QEAA@AEAVSerializer@1@H@Z   0x1800a8340
// ---------------------------------------------------------------------------
extern "C" BinaryCodec* RtIo_BinaryReader_Ctor(BinaryCodec* self, Serializer* ser,
                                               int32_t version) {
    self->base.vtable = VtblReaderWriter();   // the inlined ReaderWriter base ctor
    self->base.allowCustomSerialize = 0;      // BYTE store
    self->base.serializer = ser;
    self->base.stream     = ser->stream;      // *(void**)ser
    ++g_encCounts.ctorReader;
    self->base.vtable = VtblBinaryReader();   // concrete table, overwriting it

    self->word20  = 0;        // retail: ONE qword zeroing +0x20..+0x27
    self->word24  = 0;
    self->version = version;  // dead on every successful call — see the header

    void* s = self->base.stream;
    int32_t magic;                     // retail's RSP+0x50: written by the Read,
    StreamRead(s, 4, &magic);          // never inspected. Consumed, not checked.
    (void)magic;
    StreamRead(s, 4, &self->word20);
    self->word24 = StreamGetPos(s);
    StreamRead(s, 4, &self->version);
    return self;
}

// ---------------------------------------------------------------------------
// ??0BinaryWriter@IO@@QEAA@AEAVSerializer@1@H@Z   0x1800a8a30
// ---------------------------------------------------------------------------
extern "C" BinaryCodec* RtIo_BinaryWriter_Ctor(BinaryCodec* self, Serializer* ser,
                                               int32_t version) {
    self->base.vtable = VtblReaderWriter();   // the inlined ReaderWriter base ctor
    self->base.allowCustomSerialize = 0;      // BYTE store
    self->base.serializer = ser;
    self->base.stream     = ser->stream;
    ++g_encCounts.ctorWriter;
    self->base.vtable = VtblBinaryWriter();   // concrete table, overwriting it

    self->word20  = 0;        // retail: two SEPARATE dword stores here
    self->word24  = -1;       // the placeholder value that is about to be emitted
    self->version = version;

    void* s = self->base.stream;
    int32_t magic = static_cast<int32_t>(0xaaaaaaaau);   // built on the stack
    StreamWrite(s, 4, &magic);
    self->word20 = StreamGetPos(s);      // where the placeholder lands
    StreamWrite(s, 4, &self->word24);    // emit the -1 placeholder
    self->word24 = StreamGetPos(s);
    StreamWrite(s, 4, &self->version);
    return self;
}

// ---------------------------------------------------------------------------
// Slots 0, 1 and 2 of the shared BINARY encoding vtable 0x1801554a8 — Wave 21
// lane ENC, item 1. NONE of the three is exported; no debug string names them,
// so the names below are DESCRIPTIVE (behaviour-derived), not attested.
//
// Transcribed straight from the shipped bytes (read via pefile against
// Runtime-x64.dll, RETAIL, not the profile build — this table has no
// counterpart in the profile export list to cross-check against, and the
// project's own trap says profile OFFSETS must never be trusted for this
// vtable family):
//
//   0x1800a81e0  slot 0   B0 01 C3          MOV AL,1  ; RET   -> true
//   0x1800a81d0  slot 1   32 C0 C3          XOR AL,AL ; RET   -> false
//   0x1800a81c0  slot 2   B8 02 00 00 00 C3 MOV EAX,2 ; RET   -> 2
//
// All three ignore `self` completely (never read RCX) — the same "self
// accepted and ignored" shape as CreateReader/CreateWriter below, consistent
// with Encoding being a stateless policy tag.
//
// THE SEMANTICS ARE INFERRED FROM A SIBLING TABLE, NOT GUESSED FROM NOTHING.
// The unexported TEXT encoding vtable at 0x1801559d0 (see file_stream.cpp)
// has the SAME SHAPE at the SAME three slots, with slot 0/1 EXACTLY INVERTED
// and slot 2 holding a DIFFERENT small constant:
//
//   0x1801559d0  slot 0   0x1800b9ab0   32 C0 C3          -> false
//   0x1801559d0  slot 1   0x1800b9aa0   B0 01 C3          -> true
//   0x1801559d0  slot 2   0x1800b9a90   B8 01 00 00 00 C3 -> 1
//
// A predicate pair that flips between the binary and text tables and a third
// slot that varies 2-vs-1 reads as {IsBinary, IsText, EncodingKind} — but that
// reading is NOT proven by a string or an export; it is this lane's naming
// choice, called out so a later lane does not mistake the names for an
// attested fact.
// ---------------------------------------------------------------------------
extern "C" bool RtIo_Encoding_IsBinary(void* self) {
    (void)self;
    return true;
}
extern "C" bool RtIo_Encoding_IsText(void* self) {
    (void)self;
    return false;
}
extern "C" int32_t RtIo_Encoding_GetKind(void* self) {
    (void)self;
    return 2;
}

// ---------------------------------------------------------------------------
// IO::Encoding::CreateReader   0x1800a8120   (vtable 0x1801554a8 slot +0x18)
// IO::Encoding::CreateWriter   0x1800a8080   (vtable 0x1801554a8 slot +0x20)
// ---------------------------------------------------------------------------
// "self" is the IO::Encoding and is deliberately unused — retail never reads it.
extern "C" BinaryCodec* RtIo_Encoding_CreateReader(void* self, Serializer* ser,
                                                   int32_t version) {
    (void)self;
    ++g_encCounts.createReader;
    void* p = RT_EXPORT_ONCE(MemAllocFn,
        "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(sizeof(BinaryCodec), 4);
    if (p == nullptr) return nullptr;    // retail's XOR EAX,EAX path
    return RtIo_BinaryReader_Ctor(static_cast<BinaryCodec*>(p), ser, version);
}

extern "C" BinaryCodec* RtIo_Encoding_CreateWriter(void* self, Serializer* ser,
                                                   int32_t version) {
    (void)self;
    ++g_encCounts.createWriter;
    void* p = RT_EXPORT_ONCE(MemAllocFn,
        "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(sizeof(BinaryCodec), 4);
    if (p == nullptr) return nullptr;    // retail's XOR EAX,EAX path
    return RtIo_BinaryWriter_Ctor(static_cast<BinaryCodec*>(p), ser, version);
}

}  // namespace RtIo

// ============================================================================
// Registration
// ============================================================================
// The two CONSTRUCTORS are exported, so they get both doors: the name flips
// every ResolveExport call site, the address covers anything reaching them by
// Ghidra address (which is how the factories in the DLL reach them today).
//
// The two FACTORIES are NOT exported and get the ADDRESS door only. That is
// also the door lane C2's mint uses: an RT_MINT_UNNAMED slot resolves through
// RtPortLookupAddr on its dllVa, so registering these two addresses is exactly
// what turns slots 3 and 4 of the minted 0x1801554a8 table from named traps
// into our bodies. Wave 21 lane ENC closes the same door for the remaining
// three: slots 0, 1 and 2 (0x1800a81e0, 0x1800a81d0, 0x1800a81c0) are now
// registered too, so all FIVE slots of the shared BINARY encoding vtable are
// our bodies. The table itself stays permanently address-keyed (unexported),
// so it can never move into the "promoted" bucket a `??_7` export can — see
// the mint-vtable census comment in tools/runtime_census.py — but every
// dispatch that reaches it now runs OUR code, which is the thing that
// actually matters. Our own ctors are the stampers (RtIo_CompressedMemoryStream_Ctor,
// RtIo_MemoryStream_Ctor, RtIo_CompressedStream_Ctor, RtIo_TextFileStream_Ctor
// et al. all read this exact pointer through `ResolveAddr(Module::Runtime,
// 0x1801554a8ull)` before storing it), so we own the route, not merely the
// slots.
int RtIoEncodingRegister() {
    using namespace RtIo;
    int n = 0;

    RegisterRuntimeExport("??0BinaryReader@IO@@QEAA@AEAVSerializer@1@H@Z",
                          (void*)&RtIo_BinaryReader_Ctor); ++n;
    RegisterRuntimeAddr(0x1800a8340, (void*)&RtIo_BinaryReader_Ctor);

    RegisterRuntimeExport("??0BinaryWriter@IO@@QEAA@AEAVSerializer@1@H@Z",
                          (void*)&RtIo_BinaryWriter_Ctor); ++n;
    RegisterRuntimeAddr(0x1800a8a30, (void*)&RtIo_BinaryWriter_Ctor);

    RegisterRuntimeAddr(0x1800a8120, (void*)&RtIo_Encoding_CreateReader); ++n;
    RegisterRuntimeAddr(0x1800a8080, (void*)&RtIo_Encoding_CreateWriter); ++n;

    RegisterRuntimeAddr(0x1800a81e0, (void*)&RtIo_Encoding_IsBinary); ++n;
    RegisterRuntimeAddr(0x1800a81d0, (void*)&RtIo_Encoding_IsText);   ++n;
    RegisterRuntimeAddr(0x1800a81c0, (void*)&RtIo_Encoding_GetKind);  ++n;

    return n;
}
