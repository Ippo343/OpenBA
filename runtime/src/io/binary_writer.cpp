// ============================================================================
// runtime/src/io/binary_writer.cpp — the BinaryWriter framing tail.
// Wave 9 lane ENC-2, bodies 1-4.
// ============================================================================
//
//   0x1800a8fb0  ?Serialize@BinaryWriter@IO@@UEAAXAEAIPEBD@Z   slot +0x98 (uint&)
//   0x1800a8fd0  ?Serialize@BinaryWriter@IO@@UEAAXAEAHPEBD@Z   slot +0xa0 (int&)
//   0x1800a8980  ??1BinaryWriter@IO@@UEAA@XZ                   the BACK-PATCH
//   0x180023b50  BinaryWriter scalar deleting dtor             slot 0, UNEXPORTED
//
// These are the four bodies `?SaveSettings@DialogInstance@UI@@` dispatches into
// on the write path. Lane ENC ported the Encoding factories and the two codec
// constructors; without these four, porting SaveSettings would have reached a
// minted trap on the very next call — the same "moves the stop instead of
// removing it" failure that made lane ENC four functions instead of two.
//
// ----------------------------------------------------------------------------
// THE FRAMING, NOW THAT BOTH HALVES ARE VISIBLE
// ----------------------------------------------------------------------------
// ??1BinaryWriter is the MIRROR of ??0BinaryWriter (ported in encoding.cpp), and
// reading the two together is what finally explains the header. The constructor
// lays down:
//
//     offset:   0             4                  8           12
//             [ 0xAAAAAAAA ][ -1 placeholder ][ version ][ ... fields ... ]
//                             ^ _word20 = 4                ^
//                                                _word24 = 8
//
// and the destructor closes the record:
//
//     len = GetCurrentPos() - _word24        // see below: NOT just the fields
//     Write(0xBBBBBBBB)                      // trailer magic
//     resume = GetCurrentPos()
//     SetCurrentPos(_word20)                 // seek BACK to the placeholder
//     Write(len)                             // BACK-PATCH it
//     SetCurrentPos(resume)                  // and forward again
//
// **`_word24` IS CAPTURED BEFORE THE VERSION IS WRITTEN, SO THE BACK-PATCHED
// LENGTH INCLUDES THE VERSION DWORD.** The constructor's order is: write the
// placeholder, THEN read the position into `_word24`, THEN write the version.
// So `_word24` == 8 == the offset of the version field, and
// `len == 4 + 4*fieldCount`, not `4*fieldCount`. Calling `_word24` "payload
// start" and `len` "payload bytes" is off by exactly one dword, and the framing
// assertion in binary_writer_probe.cpp checks the arithmetic that is actually
// there rather than the one that reads more naturally.
//
// So it is a length-prefixed, magic-bracketed record, and **neither half reads
// correctly alone**: `_word20` only makes sense as "where the length goes" once
// the destructor is read, and `_word24` only makes sense once `end - _word24`
// is seen. Porting this file is therefore also the strongest available check on
// the constructor that already merged — if the two halves disagree about which
// field is which, the round-trip probe in binary_writer_probe.cpp fails, and it
// does not.
//
// The vtable is restamped at BOTH ends of the destructor: BinaryWriter's table
// on entry, ReaderWriter's on exit. That is MSVC's standard base-unwind and it
// is reproduced, not collapsed.
//
// ----------------------------------------------------------------------------
// THE TWO Serialize SLOTS — five instructions each, and the argument they DROP
// ----------------------------------------------------------------------------
//     MOV RCX,[RCX+0x18]     ; RCX = this->_stream
//     MOV R8,RDX             ; R8  = &value        <-- OVERWRITES the `name` arg
//     MOV EDX,4
//     MOV RAX,[RCX]
//     JMP [RAX+0x10]         ; TAIL JUMP to Stream::Write(4, &value)
//
// The `const char* name` parameter is not merely unused, it is CLOBBERED before
// the call: R8 held `name` on entry and is overwritten with the value pointer.
// So the name is genuinely absent from the binary format — a fact worth having
// in writing, because it means the stream carries no field names and a reader
// must rely entirely on ORDER. Both slots are the same five instructions; only
// the C++ parameter type differs, which is why they share a body here.
//
// ----------------------------------------------------------------------------
// THE DELETING DESTRUCTOR AND ITS DEAD ARM
// ----------------------------------------------------------------------------
// 0x180023b50 is MSVC's standard scalar/vector deleting destructor:
//
//     if (flags & 2) {                       // delete[] form
//         count = *(int*)((char*)this - 8);  // the array cookie
//         <eh vector dtor>(this, 0x30, count, ~BinaryWriter);   // 0x18007ec98
//         if (flags & 1) MemFree(this - 8);
//         return this - 8;
//     }
//     ~BinaryWriter(this);
//     if (flags & 1) MemFree(this);
//     return this;
//
// **The `flags & 2` arm is unreachable by construction.** A BinaryWriter is only
// ever produced by IO::Encoding::CreateWriter (0x1800a8080), whose allocation is
// a bare `MemAlloc(0x30, 4)` — it lays down NO array cookie, so the 8 bytes the
// arm reads would be heap metadata, not a count. Nothing in the DLL or in our
// tree array-allocates one. Rather than open an address door onto the CRT's
// vector-destructor helper (0x18007ec98, unexported and unported) for a path no
// caller can take, that arm is a NAMED ABORT — the same disposition the mint
// gives a slot retail relies on never being called, and the message says what
// would have to have happened to get there. This is a deliberate, declared
// divergence from the shipped bytes on a dead path, not an oversight; it is the
// one place in this file that is not byte-faithful and it is called out in
// WAVE9_LANE_ENC2.md as such.
//
// `0x180006a10`, the unexported MemFree the shipped arm calls, was compared
// instruction-for-instruction against the exported
// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z (0x1800c7ad0): the two are
// BYTE-IDENTICAL, including the leading null check. Using the exported name is
// exact here, not merely equivalent. (runtime/src/io/compressedstream.cpp
// asserted this; this lane re-derived it rather than inheriting it.)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module module, uintptr_t ghidraAddr);

namespace RtIo {

using rt::io::BinaryCodec;

namespace {

using MemFreeFn = void (*)(void*);

// Deliberately duplicated from encoding.cpp rather than hoisted into a shared
// header: four inline helpers is less coupling than a header edit on a file
// that has already merged, and each file in runtime/src/io is self-contained by
// house style.
using StreamRwFn    = bool (*)(void* self, int32_t size, void* buf);
using StreamPosFn   = int32_t (*)(void* self);
using StreamSeekFn  = bool (*)(void* self, int32_t pos);

constexpr int kStreamWrite  = 0x10;
constexpr int kStreamSetPos = 0x20;
constexpr int kStreamGetPos = 0x28;

inline void* VSlot(void* obj, int off) {
    return (*reinterpret_cast<void***>(obj))[off / 8];
}
inline bool StreamWrite(void* s, int32_t n, void* src) {
    return reinterpret_cast<StreamRwFn>(VSlot(s, kStreamWrite))(s, n, src);
}
inline int32_t StreamGetPos(void* s) {
    return reinterpret_cast<StreamPosFn>(VSlot(s, kStreamGetPos))(s);
}
inline void StreamSetPos(void* s, int32_t pos) {
    reinterpret_cast<StreamSeekFn>(VSlot(s, kStreamSetPos))(s, pos);
}

void** VtblBinaryWriter() {
    static void** v = reinterpret_cast<void**>(ResolveExport("??_7BinaryWriter@IO@@6B@"));
    return v;
}
void** VtblReaderWriter() {
    static void** v = reinterpret_cast<void**>(ResolveExport("??_7ReaderWriter@IO@@6B@"));
    return v;
}

}  // namespace

// ---------------------------------------------------------------------------
// ?Serialize@BinaryWriter@IO@@UEAAXAEAIPEBD@Z   0x1800a8fb0   slot +0x98
// ?Serialize@BinaryWriter@IO@@UEAAXAEAHPEBD@Z   0x1800a8fd0   slot +0xa0
// ---------------------------------------------------------------------------
// One body for both: the shipped bytes are identical and only the C++ parameter
// type differs. `name` is clobbered before use — see the header block.
extern "C" void RtIo_BinaryWriter_SerializeUInt(BinaryCodec* self, uint32_t* value,
                                                const char* name) {
    (void)name;
    StreamWrite(self->base.stream, 4, value);
}

extern "C" void RtIo_BinaryWriter_SerializeInt(BinaryCodec* self, int32_t* value,
                                               const char* name) {
    (void)name;
    StreamWrite(self->base.stream, 4, value);
}

// ---------------------------------------------------------------------------
// ??1BinaryWriter@IO@@UEAA@XZ   0x1800a8980   — closes the record
// ---------------------------------------------------------------------------
extern "C" void RtIo_BinaryWriter_Dtor(BinaryCodec* self) {
    self->base.vtable = VtblBinaryWriter();      // MSVC restamps on dtor entry

    void* s = self->base.stream;
    // Everything written after the placeholder, VERSION DWORD INCLUDED — see
    // the header block for why that is not the same as "payload bytes".
    const int32_t len = StreamGetPos(s) - self->word24;

    int32_t trailer = static_cast<int32_t>(0xbbbbbbbbu);
    StreamWrite(s, 4, &trailer);

    const int32_t resume = StreamGetPos(s);
    StreamSetPos(s, self->word20);               // back to the placeholder
    int32_t lenOut = len;
    StreamWrite(s, 4, &lenOut);                  // BACK-PATCH the length
    StreamSetPos(s, resume);                     // and forward again

    self->base.vtable = VtblReaderWriter();      // base-unwind on dtor exit
}

// ---------------------------------------------------------------------------
// BinaryWriter scalar deleting destructor   0x180023b50   — vtable slot 0
// ---------------------------------------------------------------------------
extern "C" void* RtIo_BinaryWriter_DeletingDtor(BinaryCodec* self, unsigned int flags) {
    if (flags & 2u) {
        // Unreachable by construction — see the header block. The only producer
        // of a BinaryWriter is CreateWriter's bare MemAlloc(0x30, 4), which lays
        // down no array cookie, so nothing can legitimately arrive here.
        std::fprintf(stderr,
            "\n[RtIo] FATAL: BinaryWriter deleting dtor reached with flags&2 "
            "(delete[]).\n"
            "[RtIo] No code path array-allocates a BinaryWriter: the only\n"
            "[RtIo] producer is IO::Encoding::CreateWriter's MemAlloc(0x30,4),\n"
            "[RtIo] which writes no array cookie. Reaching this means either a\n"
            "[RtIo] wrong-typed pointer arrived, or a new producer exists that\n"
            "[RtIo] this lane did not know about -- see WAVE9_LANE_ENC2.md.\n\n");
        std::fflush(stderr);
        std::abort();
    }

    RtIo_BinaryWriter_Dtor(self);
    if (flags & 1u)
        RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(self);
    return self;
}

}  // namespace RtIo

// ============================================================================
// Registration
// ============================================================================
// The three exported bodies get both doors. The deleting dtor is UNEXPORTED and
// gets the address door only -- which is also the door its mint slot uses
// (kVSlots8[0] is RT_MINT_UNNAMED on 0x180023b50), so registering that address
// is what turns BinaryWriter's slot 0 from a trap into this body.
int RtIoBinaryWriterRegister() {
    using namespace RtIo;
    int n = 0;

    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXAEAIPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerializeUInt); ++n;
    RegisterRuntimeAddr(0x1800a8fb0, (void*)&RtIo_BinaryWriter_SerializeUInt);

    RegisterRuntimeExport("?Serialize@BinaryWriter@IO@@UEAAXAEAHPEBD@Z",
                          (void*)&RtIo_BinaryWriter_SerializeInt); ++n;
    RegisterRuntimeAddr(0x1800a8fd0, (void*)&RtIo_BinaryWriter_SerializeInt);

    RegisterRuntimeExport("??1BinaryWriter@IO@@UEAA@XZ",
                          (void*)&RtIo_BinaryWriter_Dtor); ++n;
    RegisterRuntimeAddr(0x1800a8980, (void*)&RtIo_BinaryWriter_Dtor);

    RegisterRuntimeAddr(0x180023b50, (void*)&RtIo_BinaryWriter_DeletingDtor); ++n;

    return n;
}
