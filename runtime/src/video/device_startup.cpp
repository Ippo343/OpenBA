// ============================================================================
// runtime/src/video/device_startup.cpp — Wave 9 lane VID2
//   Video::Device::{Startup, PlatformStartup, Shutdown, PlatformShutdown,
//   ValidateConfiguration, CreateVideoInterface} and the seven unexported
//   Collection::Array helpers ValidateConfiguration reaches.
// ============================================================================
//
// THE POINT OF THIS FILE
// ----------------------
// `?PlatformStartup@Device@Video@@AEAA_NXZ` is the next cutover boot wall after
// the IO chain. Our ported `Engine::Startup` calls it at
// runtime/src/engine/engine_lifecycle.cpp:719 and a null there returns 0 with
// `g_startupFailAt == 11` — the boot ends, quietly, without a crash to
// diagnose.
//
// `?Startup@` (0x1800f7270) and `?Shutdown@` (0x1800f7280) are ONE-INSTRUCTION
// JMP THUNKS onto `PlatformStartup` (0x1800f8260) and `PlatformShutdown`
// (0x1800f83f0). Verified in the disassembly, not taken on trust:
//
//     1800f7270: JMP 0x1800f8260
//     1800f7280: JMP 0x1800f83f0
//
// so one body closes two rows each. Their live callers are
// code/Game/Dialog/optionsdialog.cpp:1138/1168 (applying video settings).
//
// THREE RECORDED BLOCKERS FOR THIS CLUSTER WERE FALSE, AND ALL THREE WERE
// RE-CHECKED AGAINST THE DISASSEMBLY RATHER THAN INHERITED
// ---------------------------------------------------------------------------
//  1. WAVE9_LANE_VID.md: "loops the renderer interfaces through
//     CreateVideoInterface (which LoadLibrary's a plugin DLL and calls its
//     CreateDeviceData)". **False.** `?CreateVideoInterface@Device@Video@@QEAA_NH@Z`
//     (0x1800f7930) is thirty instructions and contains no `LoadLibrary`. It
//     tests three ALREADY-POPULATED pointers in `Device::dllSlots[idx]` and
//     calls the slot's `createDeviceData`. The `LoadLibraryW` lives in
//     `InitializeInterfaces` (0x1800f79f0), driven by `EnumerateVideoModes`,
//     which `standalone/video_interface.cpp` already detours.
//  2. Same doc: "calls the PRIVATE no-argument ValidateConfiguration". **False.**
//     It is exported — `?ValidateConfiguration@Device@Video@@QEAA_NXZ`, ordinal
//     4533. (Wave 6 lane B ported the EIGHT-ARGUMENT overload, ordinal 4532;
//     that part was right, and the two are different functions.)
//  3. Same doc: "with the DLL gone, Engine::Startup no longer runs; nothing in
//     our tree starts the video device". **Stale.** `Engine::Startup` is ported
//     (Wave 9 lane ENG) and line 719 IS that call.
//
// TRANSITIVE CLOSURE, COUNTED BEFORE ANY BODY WAS WRITTEN (lane PICK's rule)
// ---------------------------------------------------------------------------
//   PlatformStartup 0x1800f8260
//     -> Application::_singleton [0x1801fc678]        (DATA, minted)
//     -> ValidateConfiguration   0x1800f8830   PORTED HERE
//          -> GetDefaultResolution 0x1800f7540  PORTED (device_startup_win.cpp)
//          -> memset               0x1800881b0  = the CRT memset; std::memset
//          -> Array<DisplayMode>::operator=  0x18000fcd0  PORTED HERE
//               -> Array<DisplayMode>::~Array 0x180010830 PORTED HERE
//                    -> element destroy       0x180010cf0 PORTED HERE
//               -> Array<RefreshRate>::operator= 0x18000fbf0 PORTED HERE
//                    -> Array<RefreshRate>::~Array 0x180010760 PORTED HERE
//          -> Array<bool>::operator=          0x180008b00  PORTED HERE
//               -> Array<bool>::~Array        0x180009560  PORTED HERE
//     -> Error::SetText          0x1800c5fa0   ALREADY PORTED (Wave 2)
//     -> CreateVideoInterface    0x1800f7930   PORTED HERE
//     -> Application::RegisterMessageCallback 0x18009fa60 PORTED
//                                   (runtime/src/application/message_callbacks.cpp)
//     -> &SizeWindow 0x1800f7d20 / &MoveWindow 0x1800f7d90  PORTED
//                                   (device_startup_win.cpp) — STORED, so a null
//                                   here is a null Application later calls
//   PlatformShutdown 0x1800f83f0
//     -> Collection::Set erase   0x18000aba0   PORTED (collection/set_avl.cpp)
//     -> the DeviceData vtable   (ours, dx11/)
//
//   MemAlloc 0x1800c7a50 / MemFree 0x1800c7ad0 / Free 0x1800c6c00 — registered
//   by attempt 14's RegisterAllocatorAddresses().
//
// **Closure size: 13 new bodies here + 3 in the _win twin + 5 in the Set
// cluster = 21, and it CLOSES.** Porting only the four assigned rows would have
// created 17 new unregistered address doors where there are none today, which
// is exactly the trade lane PICK refused.
//
// ORACLE
// ------
// `ValidateConfiguration` and the seven Array helpers get a lockstep synthetic
// probe (device_startup_probe.cpp). `PlatformStartup` / `PlatformShutdown` do
// NOT and cannot: they create and destroy the one D3D11 device, so running ours
// and then theirs makes a second swap chain against a window that already has
// one. What they get instead is **live exercise on the DLL-present boot** —
// with this port registered, `Engine::Startup`'s `RT_EXPORT_ONCE` resolves to
// OUR `PlatformStartup`, so every DLL-present run boots the video device
// through this file. A wrong transcription does not report a divergence; it
// fails to render.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/video/Device.h"
#include "../../types/video/CompanionData.h"   // ENDGAME B.3.5: Video::DeviceData
#include "../collection/set_avl.h"

using RtVideo::Adapter;
using RtVideo::Configuration;
using RtVideo::Device;
using RtVideo::DisplayMode;
using RtVideo::GraphicsInterface;
using RtVideo::InterfaceSlot;
using RtVideo::RefreshRate;

// Declared locally rather than by including standalone/resolve.h (PLAN §4.1
// decision 1). Byte-identical to array.cpp:133.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
void*     ResolveExport(const char* mangled_name);

// The Win32 half (device_startup_win.cpp): the two message callbacks whose
// ADDRESSES PlatformStartup stores, and the EnumDisplaySettingsW probe.
extern "C" int  RtVideo_Device_SizeWindow(void* hwnd, unsigned msg, uint64_t w, int64_t l);
extern "C" int  RtVideo_Device_MoveWindow(void* hwnd, unsigned msg, uint64_t w, int64_t l);
// Deliberately typed with void* on both sides so this declaration and the
// definition in the _win twin are textually identical — the twin cannot include
// Device.h (see its banner), so a typed declaration here could drift from the
// definition there and C linkage would not catch it.
extern "C" int  RtVideo_Device_GetDefaultResolution(void* self, const void* adapter);

// The Application message-callback Set (runtime/src/application/message_callbacks.cpp).
extern "C" void RtApp_RegisterMessageCallback(void* app, unsigned id, void* cb);

namespace {

// ---------------------------------------------------------------------------
// The allocator boundary. Cache a hit, RETRY a miss.
// ---------------------------------------------------------------------------
void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(bytes, align) : nullptr;
}
void MemFree(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    if (fn) fn(p);
}

// The two exported DATA singletons these bodies read. Both minted, so both keep
// working with the DLL absent.
// NOTE there is no `Device::_singleton` accessor here, deliberately. Every body
// in this file takes its Device as `this` — retail reads the singleton in the
// SizeWindow/MoveWindow callbacks (which are in the _win twin) and nowhere
// else in this cluster. An unused accessor would invite a later edit to reach
// for the singleton where retail uses the parameter, which is the difference
// between working on the device you were handed and working on whichever one is
// current.
Configuration** ConfigSlot() {
    return RT_DATA_SLOT(Configuration**, "?_singleton@Configuration@Video@@0PEAV12@EA");
}
void** AppSlot() {
    return RT_DATA_SLOT(void**, "?_singleton@Application@1@0PEAV11@EA");
}

// `?SetText@Error@System@@SAXPEBD_N@Z` — ported in Wave 2
// (runtime/src/system/error.cpp), reached by NAME so the port registry wins.
void ErrorSetText(const char* text, bool append) {
    using Fn = void (*)(const char*, bool);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveExport("?SetText@Error@System@@SAXPEBD_N@Z"));
    if (fn) fn(text, append);
}

// ---------------------------------------------------------------------------
// The 16-byte Collection::Array header, {data, size, capacity}.
// Identical to runtime/src/collection/array.cpp's ArrayHdr; declared here
// rather than shared because that one is inside `namespace rtcoll` behind
// array_internal.h, which is documented as being for array.cpp and its probe
// only.
// ---------------------------------------------------------------------------
struct ArrayHdr {
    void*   data;      // +0x00
    int32_t size;      // +0x08
    int32_t capacity;  // +0x0c
};
static_assert(sizeof(ArrayHdr) == 0x10, "Collection::Array is 16 bytes");

// ---------------------------------------------------------------------------
// THE GROWTH RULE, which is identical in all four `operator=` bodies and is
// transcribed once here because getting it wrong is invisible until a resize:
//
//   n = src->size ; old = dst->size ; dst->size = n
//   if (n > dst->capacity) {
//       dst->capacity = n;
//       if (n == old + 1 && old != 0) dst->capacity = n * 3;   // amortise
//       dst->data = MemAlloc(capacity * stride, align);
//   }
//
// The `n == old + 1 && old != 0` guard is the whole rule: growth by exactly one
// from a non-empty array triples, everything else takes the exact size. A
// "simplification" to always triple, or to always take n, changes the
// allocation pattern and therefore the heap layout — which this project has
// already paid for once through a boundary-tag defect.
// ---------------------------------------------------------------------------
inline void GrowFor(ArrayHdr* dst, const ArrayHdr* src, int stride, int align) {
    const int n   = src->size;
    const int old = dst->size;
    dst->size = n;
    if (n > dst->capacity) {
        dst->capacity = n;
        if (n == old + 1 && old != 0) dst->capacity = n * 3;
        dst->data = MemAlloc(static_cast<uint64_t>(
                        static_cast<int64_t>(dst->capacity) * stride), align);
    }
}

}  // namespace

// ============================================================================
// 0x180010760 — Collection::Array<Device::RefreshRate>::~Array
// 0x180009560 — Collection::Array<bool>::~Array
//
// Byte-identical bodies (the element types are trivially destructible, so MSVC
// emitted the same code twice and did NOT fold them). Both:
//     if (data) MemFree(data);   // the null test is retail's, OUTSIDE the call
//     data = nullptr; *(uint64*)&size = 0;   // one qword clears size AND capacity
// ============================================================================
extern "C" void RtVideo_ArrayRefreshDtor(void* arr) {
    ArrayHdr* a = static_cast<ArrayHdr*>(arr);
    if (a->data) MemFree(a->data);
    a->data = nullptr;
    *reinterpret_cast<uint64_t*>(&a->size) = 0;
}
extern "C" void RtVideo_ArrayBoolDtor(void* arr) {
    ArrayHdr* a = static_cast<ArrayHdr*>(arr);
    if (a->data) MemFree(a->data);
    a->data = nullptr;
    *reinterpret_cast<uint64_t*>(&a->size) = 0;
}

// ============================================================================
// 0x180010cf0 — the per-element destroy for Array<DisplayMode>.
//
// Each 0x18-byte DisplayMode carries an Array<RefreshRate> at +0x08. Retail
// walks with a cursor at element+8 (`LEA RBX,[RDX+8]`) and frees each one
// inline rather than calling 0x180010760.
//
// The first parameter is the array object; the callee spills it at [RSP+8] and
// NEVER READS IT. Unnamed here for that reason.
// ============================================================================
extern "C" void RtVideo_ArrayModeElemDestroy(void* /*unused*/, void* data, int32_t count) {
    if (count <= 0) return;
    uint8_t* e = static_cast<uint8_t*>(data) + 8;
    for (int64_t i = count; i != 0; --i) {
        void* p = *reinterpret_cast<void**>(e);
        if (p) MemFree(p);
        *reinterpret_cast<uint64_t*>(e)     = 0;
        *reinterpret_cast<uint64_t*>(e + 8) = 0;
        e += 0x18;
    }
}

// ============================================================================
// 0x180010830 — Collection::Array<DisplayMode>::~Array
// ============================================================================
extern "C" void RtVideo_ArrayModeDtor(void* arr) {
    ArrayHdr* a = static_cast<ArrayHdr*>(arr);
    RtVideo_ArrayModeElemDestroy(arr, a->data, a->size);
    if (a->data) MemFree(a->data);
    a->data = nullptr;
    *reinterpret_cast<uint64_t*>(&a->size) = 0;
}

// ============================================================================
// 0x18000fbf0 — Collection::Array<RefreshRate>::operator=   (stride 8, align 4)
//
// THE PER-ELEMENT NULL TEST IS ON THE COMPUTED ADDRESS, NOT ON `data`:
//     MOV R9,[RBX] / ADD R9,RDX / JZ skip
// the ZF comes from the ADD, so the element is skipped when `data + offset`
// is zero. With a null `data` and a non-zero size — which the growth rule can
// produce if MemAlloc fails — that skips element 0 only and copies through null
// for the rest. Retail's; reproduced rather than "fixed", because a guard here
// would change which of the two implementations faults first.
// ============================================================================
extern "C" void* RtVideo_ArrayRefreshAssign(void* dstv, const void* srcv) {
    ArrayHdr* dst = static_cast<ArrayHdr*>(dstv);
    const ArrayHdr* src = static_cast<const ArrayHdr*>(srcv);
    if (dst == src) return dst;

    RtVideo_ArrayRefreshDtor(dst);
    GrowFor(dst, src, 8, 4);

    for (int32_t i = 0, off = 0; i < dst->size; ++i, off += 8) {
        uint8_t* d = static_cast<uint8_t*>(dst->data) + off;
        if (d == nullptr) continue;
        *reinterpret_cast<uint64_t*>(d) =
            *reinterpret_cast<const uint64_t*>(static_cast<const uint8_t*>(src->data) + off);
    }
    return dst;
}

// ============================================================================
// 0x180008b00 — Collection::Array<bool>::operator=          (stride 1, align 1)
// ============================================================================
extern "C" void* RtVideo_ArrayBoolAssign(void* dstv, const void* srcv) {
    ArrayHdr* dst = static_cast<ArrayHdr*>(dstv);
    const ArrayHdr* src = static_cast<const ArrayHdr*>(srcv);
    if (dst == src) return dst;

    RtVideo_ArrayBoolDtor(dst);
    GrowFor(dst, src, 1, 1);

    for (int32_t i = 0, off = 0; i < dst->size; ++i, ++off) {
        uint8_t* d = static_cast<uint8_t*>(dst->data) + off;
        if (d == nullptr) continue;
        *d = static_cast<const uint8_t*>(src->data)[off];
    }
    return dst;
}

// ============================================================================
// 0x18000fcd0 — Collection::Array<DisplayMode>::operator=  (stride 0x18, align 8)
//
// Per element: copy width and height as two DWORDs, ZERO the embedded
// Array<RefreshRate> (so the assignment below sees an empty destination rather
// than the freed pointer the dtor left), then run the Array<RefreshRate>
// assignment. The zeroing is what makes the nested assign safe and it is
// retail's, at 0x18000fd71/0x18000fd74.
// ============================================================================
extern "C" void* RtVideo_ArrayModeAssign(void* dstv, const void* srcv) {
    ArrayHdr* dst = static_cast<ArrayHdr*>(dstv);
    const ArrayHdr* src = static_cast<const ArrayHdr*>(srcv);
    if (dst == src) return dst;

    RtVideo_ArrayModeDtor(dst);
    GrowFor(dst, src, 0x18, 8);

    for (int32_t i = 0, off = 0; i < dst->size; ++i, off += 0x18) {
        uint8_t* d = static_cast<uint8_t*>(dst->data) + off;
        if (d == nullptr) continue;
        const uint8_t* s = static_cast<const uint8_t*>(src->data) + off;

        *reinterpret_cast<int32_t*>(d)     = *reinterpret_cast<const int32_t*>(s);
        *reinterpret_cast<int32_t*>(d + 4) = *reinterpret_cast<const int32_t*>(s + 4);

        uint8_t* dArr = d + 8;
        *reinterpret_cast<uint64_t*>(dArr)     = 0;
        *reinterpret_cast<uint64_t*>(dArr + 8) = 0;
        RtVideo_ArrayRefreshAssign(dArr, s + 8);
    }
    return dst;
}

// ============================================================================
// ?ValidateConfiguration@Device@Video@@QEAA_NXZ   retail 0x1800f8830, ord 4533
// ============================================================================
// Clamps `Video::Configuration` against the adapter the configuration selects,
// then copies that adapter's mode and multisample lists into the Device.
//
// NOT the eight-argument `?ValidateConfiguration@Device@Video@@QEAAXAEAH0000000@Z`
// (0x1800f7370, ordinal 4532) that Wave 6 lane B ported. Different function,
// adjacent ordinal — exactly the kind of pair that
// reference_ghidra_mcp_address_param_silently_ignored.md exists for.
//
// EARLY OUT: `interfaceCount == 0` returns with AL holding that count, i.e.
// FALSE, and no field is touched (0x1800f8852 jumps straight to the epilogue).
// The return value on that path is not an explicit `XOR AL,AL` — it is the
// count still sitting in EAX. Written as `return false` because the count is
// zero on that path by construction.
//
// SECOND PASS IS NOT REDUNDANT. The function walks the adapter's mode list
// TWICE: once against the requested width/height to pick the mode and clamp the
// refresh rate, then again against the CLAMPED values to find the final indices.
// A single pass would give the same answer whenever the first lookup hit, and a
// different one whenever `GetDefaultResolution` had to substitute — which is
// exactly the case a probe on a well-formed adapter never reaches. Both passes
// are written out.
//
// UNGUARDED INDEXING, twice, both retail's:
//   * `adapter->multisampleData[msIdx]` at 0x1800f89ff is not bounds-checked
//     against `multisampleCount`; the clamp that follows only maps a ZERO
//     ENTRY to index 0, it does not constrain the index itself.
//   * the second pass indexes `modeData[modeIdx]` with the result of a search
//     that can return -1 if the clamped size is somehow still absent.
// Left unguarded per CLAUDE.md. If either faults, something upstream built an
// adapter list our `EnumerateVideoModes` path should have built differently,
// and that is the thing to find.
// ============================================================================
// ---------------------------------------------------------------------------
// ENDGAME B.3.5 block D2 — the three members whose real bodies live in a `_win`
// twin that cannot include this header (PLAN §4.1 decision 1: no <windows.h>
// under runtime/src/ outside a `_win` file). Each member is a one-line forward
// into the `extern "C"` Win32 body, and the REGISTERED DOOR stays the `_win`
// function: that is the one place in this block where the door is not a
// delegation, and it is a platform-seam constraint rather than a choice. C++
// callers — `Device::ValidateConfiguration` below is one — now reach them as
// members.
// ---------------------------------------------------------------------------
int Video::Device::GetDefaultResolution(const Adapter& adapter) const {
    return RtVideo_Device_GetDefaultResolution(const_cast<Video::Device*>(this), &adapter);
}

int Video::Device::SizeWindow(void* hwnd, unsigned msg, uint64_t wparam, int64_t lparam) {
    return RtVideo_Device_SizeWindow(hwnd, msg, wparam, lparam);
}

int Video::Device::MoveWindow(void* hwnd, unsigned msg, uint64_t wparam, int64_t lparam) {
    return RtVideo_Device_MoveWindow(hwnd, msg, wparam, lparam);
}

bool Video::Device::ValidateConfiguration() {
    const int32_t ifaceTotal = interfaceCount;
    if (ifaceTotal == 0) return false;              // 0x1800f8852

    Configuration* cfg = *ConfigSlot();

    int32_t ifaceIdx   = cfg->interfaceIndex;
    int32_t adapterIdx = cfg->adapterIndex;
    int32_t width      = cfg->width;
    int32_t height     = cfg->height;
    int32_t refreshN   = cfg->refreshNumerator;
    int32_t refreshD   = cfg->refreshDenominator;
    int32_t msIdx      = cfg->multisample;
    int32_t filter     = cfg->textureFilter;

    // 0x1800f8893: clamp the interface index into range.
    if (ifaceIdx < 0 || ifaceIdx >= ifaceTotal) ifaceIdx = 0;

    GraphicsInterface* iface = interfaceData + ifaceIdx;

    // 0x1800f88b8: clamp the adapter index against THAT interface's list.
    if (adapterIdx < 0 || adapterIdx >= iface->adapterCount) adapterIdx = 0;

    Adapter* adapter = iface->adapterData + adapterIdx;

    // ---- pass 1: find the requested mode ---------------------------------
    // The stack temp retail builds here is a DisplayMode{width, height, {}}
    // whose only purpose is the ~Array call at 0x1800f8928 — a side-effect-free
    // destruction of an array that was never filled. It is kept because the
    // call is observable to the allocator (it is a no-op on a null pointer, so
    // in practice it is not), and because deleting a written-then-released
    // local is how lane FIND's `dirAlias` finding says a side-effect carrier
    // gets lost. Here it provably carries nothing: MemFree(nullptr).
    int32_t modeIdx = -1;
    {
        const int32_t n = adapter->modeCount;
        for (int32_t i = 0; i < n; ++i) {
            if (adapter->modeData[i].width == width &&
                adapter->modeData[i].height == height) { modeIdx = i; break; }
        }
        ArrayHdr tmp = { nullptr, 0, 0 };
        RtVideo_ArrayRefreshDtor(&tmp);          // 0x1800f8928
    }

    if (modeIdx == -1) {
        // 0x1800f8945: no match — ask the desktop what it is running and take
        // that mode's dimensions.
        modeIdx = Video::Device::GetDefaultResolution(*adapter);
        // `this` is passed because retail passes it (RCX = this at
        // 0x1800f8942); the callee never reads it — 0x1800f7540's first act is
        // `MOV RDI,RDX`, and RCX is destroyed by the memset call two
        // instructions later.
        width  = adapter->modeData[modeIdx].width;
        height = adapter->modeData[modeIdx].height;
    }

    // ---- clamp the refresh rate against that mode -------------------------
    {
        const DisplayMode* mode = adapter->modeData + modeIdx;
        const int32_t n = mode->refreshCount;

        int32_t found = -1;
        for (int32_t i = 0; i < n; ++i) {
            if (mode->refreshData[i].numerator == refreshN &&
                mode->refreshData[i].denominator == refreshD) { found = i; break; }
        }

        if (found == -1) {
            // 0x1800f89a4: no exact match — fall back to the first entry whose
            // integer RATIO matches. Note this is INTEGER division on both
            // sides (`CDQ; IDIV`), so 59.94Hz (60000/1001) and 60Hz (60/1) both
            // reduce to 59 and 60 respectively and do NOT match, while
            // 120000/1001 and 119/1 both reduce to 119 and DO. Retail's
            // arithmetic; the point of a faithful port is that ours picks the
            // same surprising entry.
            int32_t pick = 0;
            if (n > 0) {
                const int32_t want = refreshN / refreshD;
                for (int32_t i = 0; i < n; ++i) {
                    if (mode->refreshData[i].numerator /
                        mode->refreshData[i].denominator == want) { pick = i; break; }
                }
            }
            refreshN = mode->refreshData[pick].numerator;
            refreshD = mode->refreshData[pick].denominator;
        }
    }

    // 0x1800f89f3: a multisample index whose adapter entry is FALSE falls back
    // to 0. The index itself is NOT range-checked — see the banner.
    if (adapter->multisampleData[msIdx] == 0) msIdx = 0;

    // 0x1800f8a07: clamp the texture filter to the adapter's maximum.
    if (filter > adapter->maxFilterLevel) filter = adapter->maxFilterLevel;

    cfg->interfaceIndex     = ifaceIdx;
    cfg->adapterIndex       = adapterIdx;
    cfg->width              = width;
    cfg->height             = height;
    cfg->refreshNumerator   = refreshN;
    cfg->refreshDenominator = refreshD;
    cfg->multisample        = msIdx;
    cfg->textureFilter      = filter;

    // ---- pass 2: re-look-up with the clamped values ------------------------
    // Retail RECOMPUTES the adapter pointer from the interface array here
    // (0x1800f8a38) rather than reusing the one above. Nothing can have moved
    // it, but "nothing can have moved it" is a claim about code this lane does
    // not own, so the re-read stands (lane PICK §5).
    adapter = (interfaceData + ifaceIdx)->adapterData + adapterIdx;

    int32_t finalModeIdx = -1;
    {
        const int32_t n = adapter->modeCount;
        for (int32_t i = 0; i < n; ++i) {
            if (adapter->modeData[i].width == width &&
                adapter->modeData[i].height == height) { finalModeIdx = i; break; }
        }
        ArrayHdr tmp = { nullptr, 0, 0 };
        RtVideo_ArrayRefreshDtor(&tmp);          // 0x1800f8a90
    }

    int32_t finalRefreshIdx = -1;
    {
        const DisplayMode* mode = adapter->modeData + finalModeIdx;
        const int32_t n = mode->refreshCount;
        const int32_t wantN = cfg->refreshNumerator;    // re-read from the cfg
        const int32_t wantD = cfg->refreshDenominator;  // (0x1800f8aa4)
        for (int32_t i = 0; i < n; ++i) {
            if (mode->refreshData[i].numerator == wantN &&
                mode->refreshData[i].denominator == wantD) { finalRefreshIdx = i; break; }
        }
    }

    // ---- clamp the three detail levels ------------------------------------
    // Each is "negative -> 0, above the cap -> the cap, otherwise untouched".
    // The caps differ per field and are literals in the code: 2, 3, 2.
    if (cfg->shadowScalar     < 0) cfg->shadowScalar     = 0;
    else if (cfg->shadowScalar     > 2) cfg->shadowScalar     = 2;
    if (cfg->shaderDetail     < 0) cfg->shaderDetail     = 0;
    else if (cfg->shaderDetail     > 3) cfg->shaderDetail     = 3;
    if (cfg->reflectionDetail < 0) cfg->reflectionDetail = 0;
    else if (cfg->reflectionDetail > 2) cfg->reflectionDetail = 2;

    adapterField2C   = adapter->field2C;      // 0x1800f8b26
    adapterField30   = adapter->field30;      // 0x1800f8b2c
    displayModeIndex = finalModeIdx;          // 0x1800f8b2f
    refreshRateIndex = finalRefreshIdx;       // 0x1800f8b32

    RtVideo_ArrayModeAssign(&modeData, &adapter->modeData);                // 0x1800f8b3d
    RtVideo_ArrayBoolAssign(&multisampleData, &adapter->multisampleData);  // 0x1800f8b4a

    maxAnisotropy = adapter->maxFilterLevel;  // 0x1800f8b52
    return true;
}

extern "C" bool RtVideo_Device_ValidateConfiguration(Device* self) {
    return self->Video::Device::ValidateConfiguration();
}

// ============================================================================
// ?CreateVideoInterface@Device@Video@@QEAA_NH@Z   retail 0x1800f7930, ord 2140
// ============================================================================
// Swap the active renderer backend to `index`.
//
// NO `LoadLibrary` ANYWHERE IN IT — see the banner. It requires all THREE
// pointers in the slot to be non-null (module, getDeviceName, createDeviceData;
// tested in the order +0xb0, +0xc0, +0xb8) and fails with an Error::SetText if
// any is missing.
//
// The teardown of the previous `DeviceData` is vtable slot 0x18 (Destroy) then
// vtable slot 0 with flag 1 (the deleting dtor) — the same two-step every other
// companion release in this tree uses — and the field is nulled between them so
// a re-entrant read cannot see a half-destroyed object.
//
// It returns TRUE even when `createDeviceData` returns null (0x1800f79b7 sets
// AL unconditionally). PlatformStartup catches that separately at 0x1800f838f
// by re-reading the field. Faithful.
// ============================================================================
bool Video::Device::CreateVideoInterface(int index) {
    InterfaceSlot* slot = dllSlots + index;

    if (slot->module == nullptr ||
        slot->createDeviceData == nullptr ||
        slot->getDeviceName == nullptr) {
        // 0x1800f79c9 — the shipped string at 0x180158920. The message text is
        // not reproduced: it lives in the DLL's .rdata and at cutover there is
        // nothing at that address. Passing our own literal would change what
        // the player sees; passing the DLL's pointer would fault. This is the
        // one place in the file where our standalone genuinely differs from
        // retail, and it is stated rather than hidden.
        ErrorSetText("Video::Device::CreateVideoInterface: the selected renderer "
                     "plugin did not export the expected entry points.", false);
        return false;
    }

    // ENDGAME B.3.5: the two hand-inlined vtable dispatches became virtual calls
    // on block A's real `Video::DeviceData` — slot 3 `Shutdown` (+0x18) and
    // slot 0 `DeletingDtor` with the delete flag (+0x00). The receiver keeps its
    // marked bridge cast because `deviceData` is a pinned `void*` field.
    if (void* dd = deviceData) {
        reinterpret_cast<Video::DeviceData*>(dd)->Shutdown();          // B.3.5 bridge
        dd = deviceData;                                     // 0x1800f7980 re-read
        if (dd)
            (void)reinterpret_cast<Video::DeviceData*>(dd)->DeletingDtor(1);  // B.3.5 bridge
        deviceData = nullptr;
    }

    activeInterface = index;                                 // 0x1800f79a4

    using CreateFn = void* (*)(Device*);
    deviceData = reinterpret_cast<CreateFn>(slot->createDeviceData)(this);
    return true;                                             // 0x1800f79b7
}

extern "C" bool RtVideo_Device_CreateVideoInterface(Device* self, int32_t index) {
    return self->Video::Device::CreateVideoInterface(index);
}

// ============================================================================
// ?PlatformStartup@Device@Video@@AEAA_NXZ   retail 0x1800f8260, ord 3405
// ?Startup@Device@Video@@QEAA_NXZ           retail 0x1800f7270, ord 4091 (thunk)
// ============================================================================
// Bring the video device up, trying every renderer interface in turn.
//
// THE RETRY LOOP IS THE INTERESTING PART, and it is not a plain for-loop:
//
//   * it starts at the CONFIGURED interface index, clamped to 0..2 by an
//     UNSIGNED compare (`CMP EBX,2 / CMOVA EBX,1` — a NEGATIVE configured index
//     is above 2 unsigned, so it becomes 1, not 0);
//   * it walks DOWNWARD and WRAPS to 2 at -1, so from index 1 the order is
//     1, 0, 2 — not 1, 2, 0;
//   * a three-byte stack array records which indices have been tried, and the
//     loop stops when it reaches one already marked. The initial `tried` test
//     happens BEFORE the first iteration, so a pre-marked entry would exit
//     immediately — it cannot, because the array is zeroed two instructions
//     earlier, but the test is where retail put it.
//
// `ValidateConfiguration`'s result is CHECKED the first time (failure is fatal)
// and IGNORED inside the loop (0x1800f82f9). Both kept.
//
// The two message callbacks are registered only on the success path, and their
// ids are 5 (WM_SIZE) and 0x232 (WM_EXITSIZEMOVE).
// ============================================================================
bool Video::Device::PlatformStartup() {
    // 0x1800f8266: the HWND comes from Application::_singleton[0].
    windowHandle = *reinterpret_cast<void**>(*AppSlot());

    if (!Video::Device::ValidateConfiguration()) {
        // 0x1800f8283 — the shipped string at 0x1801589b0; see
        // CreateVideoInterface for why the text is ours.
        ErrorSetText("Video::Device::PlatformStartup: no usable video "
                     "configuration.", false);
        return false;
    }

    Configuration* cfg = *ConfigSlot();

    int32_t idx = cfg->interfaceIndex;
    if (static_cast<uint32_t>(idx) > 2u) idx = 1;    // 0x1800f82c5 CMOVA

    uint8_t tried[3] = { 0, 0, 0 };
    int64_t cursor = idx;

    if (tried[cursor] != 0) {                        // 0x1800f82e2, before the loop
        ErrorSetText("Video::Device::PlatformStartup: no renderer interface "
                     "could be started.", false);
        return false;
    }

    for (;;) {
        tried[cursor] = 1;                           // 0x1800f82f3
        cfg->interfaceIndex = idx;                   // 0x1800f82f6

        (void)Video::Device::ValidateConfiguration();      // result IGNORED

        if (Video::Device::CreateVideoInterface(idx)) {
            // ENDGAME B.3.5: three hand-inlined `DeviceData` vtable dispatches
            // became virtual calls on block A's real class — slot 2 `CreateDevice`
            // (+0x10), slot 3 `Shutdown` (+0x18) and slot 0 `DeletingDtor`.
            void* dd = deviceData;

            if (reinterpret_cast<Video::DeviceData*>(dd)->CreateDevice()) {  // B.3.5 bridge
                // ---- success path (0x1800f838f) --------------------------
                if (deviceData == nullptr) break;         // re-read, then fail
                const int32_t active = activeInterface;
                if (active < 0) break;
                cfg->interfaceIndex = active;

                void* app = *AppSlot();
                RtApp_RegisterMessageCallback(app, 5,
                    reinterpret_cast<void*>(&Video::Device::SizeWindow));
                RtApp_RegisterMessageCallback(app, 0x232,
                    reinterpret_cast<void*>(&Video::Device::MoveWindow));
                return true;
            }

            // ---- Create failed: tear the companion down and try the next ---
            reinterpret_cast<Video::DeviceData*>(deviceData)->Shutdown();     // B.3.5 bridge
            if (void* d2 = deviceData)
                (void)reinterpret_cast<Video::DeviceData*>(d2)->DeletingDtor(1);  // B.3.5 bridge
            deviceData = nullptr;
        }

        // 0x1800f8345: step DOWN, wrapping to 2.
        --cursor;
        --idx;
        if (idx < 0) { idx = 2; cursor = 2; }
        if (tried[cursor] != 0) break;
    }

    // 0x1800f8360 — every interface tried, or the success path bailed.
    ErrorSetText("Video::Device::PlatformStartup: no renderer interface could "
                 "be started.", false);
    return false;
}

bool Video::Device::Startup() {
    // 0x1800f7270: JMP 0x1800f8260. One instruction, no frame, no argument
    // shuffle — so this really is the same function under a second name.
    return Video::Device::PlatformStartup();
}

extern "C" bool RtVideo_Device_PlatformStartup(Device* self) {
    return self->Video::Device::PlatformStartup();
}

extern "C" bool RtVideo_Device_Startup(Device* self) {
    return self->Video::Device::Startup();
}

// ============================================================================
// ?PlatformShutdown@Device@Video@@AEAAXXZ   retail 0x1800f83f0, ord 3396
// ?Shutdown@Device@Video@@QEAAXXZ           retail 0x1800f7280, ord 4044 (thunk)
// ============================================================================
// Remove the two message callbacks, then release the DeviceData.
//
// The two removals are `UnregisterMessageCallback` INLINED — retail open-codes
// the descent and tail-calls the erase (0x18000aba0) rather than calling
// 0x18009fa20. Written out here the same way, against the same ported erase, so
// the shape matches; calling our own UnregisterMessageCallback would be
// equivalent today and would hide a future divergence between the two copies.
//
// It re-reads `Application::_singleton` between the two removals (0x1800f842f),
// which is preserved.
// ============================================================================
namespace {
// The inlined descent-and-erase, spelled once because retail spells it twice.
//
// The compare is UNSIGNED and the operand order is INVERTED relative to
// `UnregisterMessageCallback`: retail loads the node's key into EAX and does
// `CMP EAX,<id> / JA left` (0x1800f8412), where `UnregisterMessageCallback`
// does `CMP <id>,EAX / JC left` (0x18009fa31). The two are the same ordering
// written from opposite sides, and both are transcribed as written rather than
// normalised into one helper shared with the Application file.
void UnregisterInline(rtsetavl::Set* set, unsigned id) {
    for (rtsetavl::Node* n = set->root; n; ) {
        const unsigned key = *reinterpret_cast<const unsigned*>(n);
        if (key > id)       n = n->left;                       // JA
        else if (key >= id) { rtsetavl::Erase(set, n); return; }  // JNC -> equal
        else                n = n->right;
    }
}
}  // namespace

void Video::Device::PlatformShutdown() {
    // 0x1800f83f6 / 0x1800f842f: the singleton is read, used, and RE-READ
    // between the two removals. Preserved — see buffers_win.cpp's note on the
    // same pattern in Device::Unlock.
    {
        void* app = *AppSlot();
        UnregisterInline(reinterpret_cast<rtsetavl::Set*>(
            static_cast<uint8_t*>(app) + 0x18), 5);            // WM_SIZE
    }
    {
        void* app = *AppSlot();                                // 0x1800f842f
        UnregisterInline(reinterpret_cast<rtsetavl::Set*>(
            static_cast<uint8_t*>(app) + 0x18), 0x232);        // WM_EXITSIZEMOVE
    }

    // 0x1800f8464: release the DeviceData — Shutdown (vtable +0x18, slot 3), then
    // the deleting dtor (vtable +0x00, flag 1), with the field re-read between and
    // nulled after. The same two-step CreateVideoInterface uses. ENDGAME B.3.5:
    // both are virtual calls on block A's real `Video::DeviceData` now.
    if (void* dd = deviceData) {
        reinterpret_cast<Video::DeviceData*>(dd)->Shutdown();              // B.3.5 bridge
        dd = deviceData;                                       // 0x1800f8476
        if (dd)
            (void)reinterpret_cast<Video::DeviceData*>(dd)->DeletingDtor(1);  // B.3.5 bridge
        deviceData = nullptr;                                  // 0x1800f848c
    }
}

void Video::Device::Shutdown() {
    // 0x1800f7280: JMP 0x1800f83f0.
    Video::Device::PlatformShutdown();
}

extern "C" void RtVideo_Device_PlatformShutdown(Device* self) {
    self->Video::Device::PlatformShutdown();
}

extern "C" void RtVideo_Device_Shutdown(Device* self) {
    self->Video::Device::Shutdown();
}

// ============================================================================
// Registration
//
// Every mangled name was checked against the shipped export directory with
// pefile BEFORE any body was written:
//
//   ?PlatformStartup@Device@Video@@AEAA_NXZ          0x1800f8260  ord 3405
//   ?Startup@Device@Video@@QEAA_NXZ                  0x1800f7270  ord 4091
//   ?PlatformShutdown@Device@Video@@AEAAXXZ          0x1800f83f0  ord 3396
//   ?Shutdown@Device@Video@@QEAAXXZ                  0x1800f7280  ord 4044
//   ?ValidateConfiguration@Device@Video@@QEAA_NXZ    0x1800f8830  ord 4533
//   ?CreateVideoInterface@Device@Video@@QEAA_NH@Z    0x1800f7930  ord 2140
//   ?SizeWindow@Device@Video@@SAHPEAUHWND__@@I_K_J@Z 0x1800f7d20  ord 4064
//   ?MoveWindow@Device@Video@@SAHPEAUHWND__@@I_K_J@Z 0x1800f7d90  ord 3235
//   ?GetDefaultResolution@Device@Video@@QEBAHAEBUAdapter@12@@Z
//                                                    0x1800f7540  ord 2559
//
// Note `?PlatformShutdown@…AEAAXXZ`, NOT `_NXZ`: it returns void. The `_N`
// spelling does not exist in the DLL and would resolve NULL in silence
// (reference_resolveexport_phantom_names.md).
//
// BOTH DOORS for all nine. The address doors matter for the two thunks in
// particular: anything that resolved 0x1800f7270 by address would otherwise
// reach the DLL while the name door reached us, which is the split this lane
// found in the Video::Context lifecycle.
//
// The seven Array helpers get ADDRESS DOORS ONLY — none is exported (checked
// against the retail directory and against Runtime-x64-profile.dll's 5,182
// names), so `RegisterRuntimeExport` has nothing to register under.
// ============================================================================
int RtVideoDeviceStartupRegister() {
    int n = 0;

    // ---- RUN IDENTITY -----------------------------------------------------
    // Four lanes independently found that the on-disk md5 says which exe is ON
    // DISK, not which one RAN: six lanes deploy to one game directory and a
    // concurrent deploy between the copy and the launch produces a complete,
    // plausible log from somebody else's binary. One lane got a whole summary
    // that way.
    //
    // So this line is emitted by THIS FILE, which no other lane has. Its
    // timestamp is this translation unit's compile time, so it changes whenever
    // this lane rebuilds. Every measurement in WAVE9_LANE_VID2.md is gated on
    // finding this exact string in the run's own log — an md5 check before and
    // after is necessary but not sufficient, because it cannot see a swap that
    // happened and was undone.
    printf("[LaneVID2] identity: device_startup.cpp built " __DATE__ " " __TIME__ "\n");

    RegisterRuntimeExport("?PlatformStartup@Device@Video@@AEAA_NXZ",
                          (void*)&RtVideo_Device_PlatformStartup); ++n;
    RegisterRuntimeExport("?Startup@Device@Video@@QEAA_NXZ",
                          (void*)&RtVideo_Device_Startup); ++n;
    RegisterRuntimeExport("?PlatformShutdown@Device@Video@@AEAAXXZ",
                          (void*)&RtVideo_Device_PlatformShutdown); ++n;
    RegisterRuntimeExport("?Shutdown@Device@Video@@QEAAXXZ",
                          (void*)&RtVideo_Device_Shutdown); ++n;
    RegisterRuntimeExport("?ValidateConfiguration@Device@Video@@QEAA_NXZ",
                          (void*)&RtVideo_Device_ValidateConfiguration); ++n;
    RegisterRuntimeExport("?CreateVideoInterface@Device@Video@@QEAA_NH@Z",
                          (void*)&RtVideo_Device_CreateVideoInterface); ++n;
    RegisterRuntimeExport("?SizeWindow@Device@Video@@SAHPEAUHWND__@@I_K_J@Z",
                          (void*)&RtVideo_Device_SizeWindow); ++n;
    RegisterRuntimeExport("?MoveWindow@Device@Video@@SAHPEAUHWND__@@I_K_J@Z",
                          (void*)&RtVideo_Device_MoveWindow); ++n;
    RegisterRuntimeExport("?GetDefaultResolution@Device@Video@@QEBAHAEBUAdapter@12@@Z",
                          (void*)&RtVideo_Device_GetDefaultResolution); ++n;

    RegisterRuntimeAddr(0x1800f8260, (void*)&RtVideo_Device_PlatformStartup);
    RegisterRuntimeAddr(0x1800f7270, (void*)&RtVideo_Device_Startup);
    RegisterRuntimeAddr(0x1800f83f0, (void*)&RtVideo_Device_PlatformShutdown);
    RegisterRuntimeAddr(0x1800f7280, (void*)&RtVideo_Device_Shutdown);
    RegisterRuntimeAddr(0x1800f8830, (void*)&RtVideo_Device_ValidateConfiguration);
    RegisterRuntimeAddr(0x1800f7930, (void*)&RtVideo_Device_CreateVideoInterface);
    RegisterRuntimeAddr(0x1800f7d20, (void*)&RtVideo_Device_SizeWindow);
    RegisterRuntimeAddr(0x1800f7d90, (void*)&RtVideo_Device_MoveWindow);
    RegisterRuntimeAddr(0x1800f7540, (void*)&RtVideo_Device_GetDefaultResolution);

    RegisterRuntimeAddr(0x180010760, (void*)&RtVideo_ArrayRefreshDtor);
    RegisterRuntimeAddr(0x180009560, (void*)&RtVideo_ArrayBoolDtor);
    RegisterRuntimeAddr(0x180010cf0, (void*)&RtVideo_ArrayModeElemDestroy);
    RegisterRuntimeAddr(0x180010830, (void*)&RtVideo_ArrayModeDtor);
    RegisterRuntimeAddr(0x18000fbf0, (void*)&RtVideo_ArrayRefreshAssign);
    RegisterRuntimeAddr(0x180008b00, (void*)&RtVideo_ArrayBoolAssign);
    RegisterRuntimeAddr(0x18000fcd0, (void*)&RtVideo_ArrayModeAssign);

    return n;
}
