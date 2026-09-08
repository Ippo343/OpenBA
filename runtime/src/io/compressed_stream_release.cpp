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

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace RtIo {

namespace {

// `if (p) { Enter(cs); Free(_singleton, p); Leave(cs); }` — retail INLINES this
// at every one of Release's nine free sites, but the null test and the lock are
// exactly what the exported `?MemFree@MemoryAllocator@System@@SAXPEAX@Z`
// (0x1800c7ad0) already is: its first instruction is `TEST RCX,RCX; JZ ret`, so
// it takes no lock for a null pointer either. Ported by lane E, so this resolves
// to our own body and not to the DLL's.
void MemFree(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    fn(p);
}

// The three Array<T> dtors ~CompressedStream calls on its own members. All
// three are lane Q's; 0x180024aa0 was already registered, the other two were
// added by this rung. Reached by address because none has an exported name.
void ArrayDtor(uintptr_t addr, void* arr) {
    using Fn = void* (*)(void*);
    Fn fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, addr));
    fn(arr);
}

// The Encoding's deleting destructor, 0x180006a10 — the same one
// ~MemoryStream uses at its own +0x10.
void EncodingDelete(void* enc) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180006a10));
    fn(enc);
}

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
    RT_DIFF_BEGIN("IO::CompressedStream::Release");

    uint8_t* const base = reinterpret_cast<uint8_t*>(self);

    // 1. the Array<void*> of compressed block buffers, +0x50 / +0x58
    {
        void** const data = *reinterpret_cast<void***>(base + 0x50);
        const int32_t n   = *reinterpret_cast<int32_t*>(base + 0x58);
        for (int32_t i = 0; i < n; ++i)             // 1800aa140..1800aa18c
            if (data[i]) MemFree(data[i]);
    }

    // 2. each Window's 0x8000 buffer, +0x30 / +0x38, stride 0x18, buffer +0x10
    {
        uint8_t* const data = *reinterpret_cast<uint8_t**>(base + 0x30);
        const int32_t  n    = *reinterpret_cast<int32_t*>(base + 0x38);
        for (int32_t i = 0; i < n; ++i) {           // 1800aa1a0..1800aa1ed
            void* const buf = *reinterpret_cast<void**>(data + i * 0x18 + 0x10);
            if (buf) MemFree(buf);
        }
    }

    // 3. the Compression object at +0x60: free ITS first field, NULL that field,
    //    then free the object. The intermediate `*p = 0` at 1800aa236 is not
    //    dead — it is retail's, and it runs between the two frees.
    {
        void** const comp = *reinterpret_cast<void***>(base + 0x60);
        if (comp) {                                  // 1800aa1f3
            if (comp[0]) MemFree(comp[0]);           // 1800aa1fe..1800aa230
            comp[0] = nullptr;                       // 1800aa236
            MemFree(comp);                           // 1800aa256
        }
        *reinterpret_cast<void**>(base + 0x60) = nullptr;   // 1800aa26f
    }

    // 4..6. the three array storages, each followed by zeroing {_data,_size}
    {
        void* const p = *reinterpret_cast<void**>(base + 0x50);
        if (p) MemFree(p);                                   // 1800aa27a
        *reinterpret_cast<void**>(base + 0x50)   = nullptr;  // 1800aa2b2
        *reinterpret_cast<int64_t*>(base + 0x58) = 0;        // 1800aa2b6
    }
    {
        void* const p = *reinterpret_cast<void**>(base + 0x40);
        if (p) MemFree(p);                                   // 1800aa2c1
        *reinterpret_cast<void**>(base + 0x40)   = nullptr;  // 1800aa2f9
        *reinterpret_cast<int64_t*>(base + 0x48) = 0;        // 1800aa2fd
    }
    {
        void* const p = *reinterpret_cast<void**>(base + 0x30);
        if (p) MemFree(p);                                   // 1800aa308
        *reinterpret_cast<void**>(base + 0x30)   = nullptr;  // 1800aa340
        *reinterpret_cast<int64_t*>(base + 0x38) = 0;        // 1800aa344
    }

    // 7. the three scalars. _activeWindow goes back to -1, NOT to 0 — the same
    //    value the constructor writes, which is what makes a Released stream
    //    reusable by a later SetWindowCount.
    *reinterpret_cast<int32_t*>(base + 0x1c) = 0;            // 1800aa348
    *reinterpret_cast<int32_t*>(base + 0x20) = -1;           // 1800aa34c
    *reinterpret_cast<void**> (base + 0x28) = nullptr;       // 1800aa353

    RT_DIFF_WORK(true);
}

// ============================================================================
// ??1CompressedStream@IO@@UEAA@XZ                            retail 0x1800aa650
// ============================================================================
// NOTE THE VTABLE: this stamps **0x180155640**, not the 0x180155560 that
// `ReadPackfile` stamps onto its stack object. That is a third, independent
// confirmation of §48 — 0x180155560 is the CompressedMemoryStream table and
// 0x180155640 is CompressedStream's own. Had they been one object, one of the
// two stamps would have to be wrong.
//
// The tail `if (_encoding) delete _encoding` is a TAIL JUMP in retail
// (0x1800aa6ac `JMP 0x180006a10`), so the Encoding delete happens after this
// frame is gone. Reproduced as a call, which is observationally identical here
// because nothing follows it.
extern "C" void RtIo_CompressedStream_Dtor(CompressedStream* self) {
    RT_DIFF_BEGIN("IO::CompressedStream::~CompressedStream");

    uint8_t* const base = reinterpret_cast<uint8_t*>(self);

    *reinterpret_cast<uintptr_t*>(base) =
        ResolveAddr(Module::Runtime, 0x180155640);           // 1800aa666/66d

    RtIo_CompressedStream_Release(self);                     // 1800aa670

    ArrayDtor(0x180024aa0, base + 0x50);                     // 1800aa67a
    ArrayDtor(0x1800249c0, base + 0x40);                     // 1800aa684
    ArrayDtor(0x1800248e0, base + 0x30);                     // 1800aa68e

    *reinterpret_cast<uintptr_t*>(base) =
        ResolveAddr(Module::Runtime, 0x1801549a0);           // 1800aa694/69b

    void* const enc = *reinterpret_cast<void**>(base + 0x10);
    if (enc) EncodingDelete(enc);                            // 1800aa69e..6ac

    RT_DIFF_WORK(true);
}

}  // namespace RtIo

// ============================================================================
// Registration
// ============================================================================
int RtIoCompressedStreamReleaseRegister() {
    using namespace RtIo;
    int n = 0;

    RegisterRuntimeExport("?Release@CompressedStream@IO@@QEAAXXZ",
                          (void*)&RtIo_CompressedStream_Release); ++n;
    RegisterRuntimeAddr(0x1800aa100, (void*)&RtIo_CompressedStream_Release);

    RegisterRuntimeExport("??1CompressedStream@IO@@UEAA@XZ",
                          (void*)&RtIo_CompressedStream_Dtor); ++n;
    RegisterRuntimeAddr(0x1800aa650, (void*)&RtIo_CompressedStream_Dtor);

    std::printf("[RtPort]   IO::CompressedStream Release/dtor (lane PKG2 rung 4b-iii, "
                "lane W's rows): %d exports registered\n", n);
    return n;
}
