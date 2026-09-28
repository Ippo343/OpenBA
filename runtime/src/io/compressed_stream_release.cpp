// ============================================================================
// runtime/src/io/compressed_stream_release.cpp — Wave 9 lane PKG2, rung 4b-iii
// ============================================================================
//
// ?Release@CompressedStream@IO@@   0x1800aa100
// ??1CompressedStream@IO@@UEAA@XZ  0x1800aa650
//
// THESE TWO ARE LANE W'S ROWS, CLOSED BY THIS LANE, and they are billed that
// way: lane W is closed and this lane is the one with the file in working
// memory, so re-deriving them elsewhere would be waste. `~CompressedStream` is
// a LIVE cutover blocker today, not merely at cutover — lane W's *ported*
// `~CompressedMemoryStream` (0x1800a9560) tail-jumps straight into it, so a
// ported body already depends on a name with nothing behind it.
//
// WHAT RELEASE CONFIRMS. Its field accesses are an independent check on the
// CompressedStream layout in runtime/types/io/Stream.h — it walks +0x50/+0x58
// (Array<void*> of block buffers), +0x30/+0x38 (Array<Window>, stride 0x18,
// buffer at +0x10), +0x40/+0x48 (Array<Header>), +0x60 (the Compression*), and
// resets +0x1c, +0x20 and +0x28. Every one agrees with the header, which was
// pinned from the CONSTRUCTOR. Two different functions, same answer.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/io/Stream.h"
#include "../../types/system/MemoryAllocator.h"
#include "../../types/collection/Collection.h"

namespace RtIo {

namespace {

// `if (p) { Enter(cs); Free(_singleton, p); Leave(cs); }` -- retail INLINES this
// at every one of Release's nine free sites, but the null test and the lock are
// exactly what the exported `?MemFree@MemoryAllocator@System@@SAXPEAX@Z`
// (0x1800c7ad0) already is: its first instruction is `TEST RCX,RCX; JZ ret`, so
// it takes no lock for a null pointer either. B3R RT-IO: called by name
// (System::MemoryAllocator::MemFree), as are the three Array<T> dtors
// (0x180024aa0 / 0x1800249c0 / 0x1800248e0) ~CompressedStream calls on its
// own members.

inline uint8_t* U8(void* p) { return static_cast<uint8_t*>(p); }

}  // namespace

// ============================================================================
// ?Release@CompressedStream@IO@@                             retail 0x1800aa100
// ============================================================================
// Nine frees in a fixed order, then a three-field reset. The ORDER is
// transcribed rather than tidied: retail frees the *contents* of all three
// arrays before freeing any of the array storage itself, which is the only
// order in which the element loops can still read their own backing store.
extern "C" void RtIo_CompressedStream_Release(CompressedStream* self) {

    uint8_t* const base = reinterpret_cast<uint8_t*>(self);

    // 1. the Array<void*> of compressed block buffers, +0x50 / +0x58
    {
        void** const data = *reinterpret_cast<void***>(base + 0x50);
        const int32_t n   = *reinterpret_cast<int32_t*>(base + 0x58);
        for (int32_t i = 0; i < n; ++i)             // 1800aa140..1800aa18c
            if (data[i]) ::System::MemoryAllocator::MemFree(data[i]);
    }

    // 2. each Window's 0x8000 buffer, +0x30 / +0x38, stride 0x18, buffer +0x10
    {
        uint8_t* const data = *reinterpret_cast<uint8_t**>(base + 0x30);
        const int32_t  n    = *reinterpret_cast<int32_t*>(base + 0x38);
        for (int32_t i = 0; i < n; ++i) {           // 1800aa1a0..1800aa1ed
            void* const buf = *reinterpret_cast<void**>(data + i * 0x18 + 0x10);
            if (buf) ::System::MemoryAllocator::MemFree(buf);
        }
    }

    // 3. the Compression object at +0x60: free ITS first field, NULL that field,
    //    then free the object. The intermediate `*p = 0` at 1800aa236 is not
    //    dead — it is retail's, and it runs between the two frees.
    {
        void** const comp = *reinterpret_cast<void***>(base + 0x60);
        if (comp) {                                  // 1800aa1f3
            if (comp[0]) ::System::MemoryAllocator::MemFree(comp[0]);           // 1800aa1fe..1800aa230
            comp[0] = nullptr;                       // 1800aa236
            ::System::MemoryAllocator::MemFree(comp);                           // 1800aa256
        }
        *reinterpret_cast<void**>(base + 0x60) = nullptr;   // 1800aa26f
    }

    // 4..6. the three array storages, each followed by zeroing {_data,_size}
    {
        void* const p = *reinterpret_cast<void**>(base + 0x50);
        if (p) ::System::MemoryAllocator::MemFree(p);                                   // 1800aa27a
        *reinterpret_cast<void**>(base + 0x50)   = nullptr;  // 1800aa2b2
        *reinterpret_cast<int64_t*>(base + 0x58) = 0;        // 1800aa2b6
    }
    {
        void* const p = *reinterpret_cast<void**>(base + 0x40);
        if (p) ::System::MemoryAllocator::MemFree(p);                                   // 1800aa2c1
        *reinterpret_cast<void**>(base + 0x40)   = nullptr;  // 1800aa2f9
        *reinterpret_cast<int64_t*>(base + 0x48) = 0;        // 1800aa2fd
    }
    {
        void* const p = *reinterpret_cast<void**>(base + 0x30);
        if (p) ::System::MemoryAllocator::MemFree(p);                                   // 1800aa308
        *reinterpret_cast<void**>(base + 0x30)   = nullptr;  // 1800aa340
        *reinterpret_cast<int64_t*>(base + 0x38) = 0;        // 1800aa344
    }

    // 7. the three scalars. _activeWindow goes back to -1, NOT to 0 — the same
    //    value the constructor writes, which is what makes a Released stream
    //    reusable by a later SetWindowCount.
    *reinterpret_cast<int32_t*>(base + 0x1c) = 0;            // 1800aa348
    *reinterpret_cast<int32_t*>(base + 0x20) = -1;           // 1800aa34c
    *reinterpret_cast<void**> (base + 0x28) = nullptr;       // 1800aa353

}

// ENDGAME B.3.9 IO (W2): the ??1CompressedStream door (0x1800aa650) that
// stood here -- vptr stamps of 0x180155640 and 0x1801549a0 by address around
// the body below -- is gone with the minted CompressedStream row and its
// RT_DELDTOR row, its last callers. The body is the class destructor below
// plus ~Stream (the encoding free), run by the compiler.

}  // namespace RtIo

// ============================================================================
// IO::CompressedStream::~CompressedStream -- ENDGAME B.3.9 IO
// ============================================================================
// The class's own destructor: the middle of 0x1800aa650 (Release, then the
// three Array destructors in retail's order). The compiler's epilogue runs
// ~Stream, which frees the encoding.
IO::CompressedStream::~CompressedStream() {
    auto* const pod = reinterpret_cast<RtIo::CompressedStream*>(this);
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    RtIo::RtIo_CompressedStream_Release(pod);                // 1800aa670
    rtcoll::RtColl_ArrayDtor_180024aa0(base + 0x50);         // 1800aa67a
    rtcoll::RtColl_ArrayDtor_1800249c0(base + 0x40);         // 1800aa684
    rtcoll::RtColl_ArrayDtor_1800248e0(base + 0x30);         // 1800aa68e
}

// ============================================================================
// Registration
// ============================================================================
int RtIoCompressedStreamReleaseRegister() {
    using namespace RtIo;
    int n = 0;

    RegisterRuntimeExport("?Release@CompressedStream@IO@@QEAAXXZ",
                          (void*)&RtIo_CompressedStream_Release); ++n;
    RegisterRuntimeAddr(0x1800aa100, (void*)&RtIo_CompressedStream_Release);

    printf("[RtPort]   IO::CompressedStream Release/dtor (lane PKG2 rung 4b-iii, "
                "lane W's rows): %d exports registered\n", n);
    return n;
}
