// ============================================================================
// runtime/src/video/dynamic_resources.cpp — Wave 9 lane VID
//   The device-lost / device-restore resource sweep:
//   Video::Device::{Lock,Unlock,ReleaseDynamic,RecreateDynamic} and the eight
//   class-static list walkers they forward to, plus Video::RenderBuffer::Recreate.
// ============================================================================
//
// WHY THIS FILE EXISTS, AND WHY IT IS NOT "THE DEVICE-LOST PATH"
// -------------------------------------------------------------
// `?ReleaseDynamic@Device@Video@@QEAAXXZ` and `?RecreateDynamic@Device@Video@@QEAAXXZ`
// look like they matter only when the GPU device is lost. They do not. Both
// names sit in the import tables of `dx11/src/contextdata.cpp` and
// `dx11/src/devicedata.cpp`, and BOTH of those tables are ALL-OR-NOTHING:
//
//     const Imports* GetImports() { ... return g_imOk ? &g_im : nullptr; }
//     void ContextData_Finish(ContextData* self) {
//         const Imports* im = GetImports();
//         if (!im) { Orig<void (*)(ContextData*)>(3)(self); return; }
//
// `g_imOk` is false if ANY ONE of the twenty entries resolved null. At cutover
// `ResolveExport` misses the port registry for an unported name and falls
// through to `ResolveExportRawImpl`, which has no module handle and returns
// null — so ONE unported Runtime export in that table disables EVERY
// `ContextData` slot, and there is no shipped body to fall back to:
// VideoDX11-x64.dll is never loaded. That is a dead renderer on the first
// draw, not on device loss.
//
// So these four `Video::Device` members are per-frame infrastructure. Their
// Wave 6 hold-back reason ("running ours and theirs both releases twice and
// recreates twice; there is nothing to compare") is a statement about the
// ORACLE, and it is still true — but it was never a statement about whether
// the port is needed, and at cutover there is no second implementation for the
// double-execution hazard to arise from.
//
// WHAT THE EIGHT STATICS ARE
// --------------------------
//   RecreateDynamic: IndexBuffer::RecreateDynamic(); VertexBuffer::RecreateDynamic();
//                    ImageBuffer::RecreateDynamic();  RenderBuffer::RecreateDynamic();
//   ReleaseDynamic:  RenderBuffer::ReleaseDynamic();  ImageBuffer::ReleaseDynamic();
//                    VertexBuffer::ReleaseDynamic();  IndexBuffer::ReleaseDynamic();
//
// Each walks its class's process-global intrusive list under Video::Device's
// recursive lock and destroys or recreates the dx11 companion object hanging
// off every live element. Wave 6 lane B's secondary reason for holding the two
// forwarders — "three of the four callees belong to lane C, porting the
// forwarder first would hard-code call-by-address into three classes another
// lane is reversing" — is answered by porting the callees too, which is what
// this file does. No address door is used for any of the nine callees.
//
// A STALE BLOCKER, CHECKED AND FALSE
// ----------------------------------
// WAVE6_LANE_B.md §2.4 says the RenderBuffer list head "_firstRenderBuffer" is
// "a DLL-internal variable with no export". It is EXPORTED:
//
//     ord 4787  0x1801fe790  ?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA
//
// read out of the shipped export directory with pefile. Lane C2 minted the
// other three sibling heads (`_firstSurface`, and both `_firstBuffer`s) and
// skipped this one, so the fourth head had no storage after cutover. This lane
// adds the missing mint row rather than working around it.
//
// EVERY MANGLED NAME BELOW WAS CHECKED AGAINST THE SHIPPED EXPORT DIRECTORY OF
// C:\GOG Games\Banished\Runtime-x64.dll WITH pefile BEFORE ANY BODY WAS
// WRITTEN (reference_resolveexport_phantom_names.md). Retail addresses and
// ordinals:
//
//   ?Lock@Device@Video@@QEAAXXZ                    0x18000ca90  ord 3214
//   ?Unlock@Device@Video@@QEAAXXZ                  0x18000cac0  ord 4462
//   ?RecreateDynamic@Device@Video@@QEAAXXZ         0x1800f80b0  ord 3522
//   ?ReleaseDynamic@Device@Video@@QEAAXXZ          0x1800f80e0  ord 3536
//   ?RecreateDynamic@ImageBuffer@Video@@SAXXZ      0x1800f2d90  ord 3523
//   ?ReleaseDynamic@ImageBuffer@Video@@SAXXZ       0x1800f2e80  ord 3537
//   ?RecreateDynamic@IndexBuffer@Video@@SAXXZ      0x1800f3500  ord 3524
//   ?ReleaseDynamic@IndexBuffer@Video@@SAXXZ       0x1800f35d0  ord 3538
//   ?RecreateDynamic@VertexBuffer@Video@@SAXXZ     0x1800f5660  ord 3526
//   ?ReleaseDynamic@VertexBuffer@Video@@SAXXZ      0x1800f5730  ord 3540
//   ?RecreateDynamic@RenderBuffer@Video@@SAXXZ     0x1800f4030  ord 3525
//   ?ReleaseDynamic@RenderBuffer@Video@@SAXXZ      0x1800f40c0  ord 3539
//   ?Recreate@RenderBuffer@Video@@QEAAXXZ          0x1800f4170  ord 3521
//
// THERE IS NO ORACLE FOR ANY OF THE TWELVE WALKERS/FORWARDERS, AND THIS FILE
// SAYS SO RATHER THAN PRETENDING OTHERWISE.
// Wave 6's argument stands unchanged: running ours and then theirs releases
// every live GPU companion twice and recreates it twice, and the second pass
// operates on freed objects. A `RT_DIFF_SHADOW` has nothing to assert either —
// the observable effect is entirely inside `dx11/`. So no `RT_DIFF_BEGIN`
// appears below for those bodies. What IS verified:
//
//   * `Device::Lock` / `Device::Unlock` are byte-identical in shape to the
//     inline copies already living in `buffers_win.cpp`, which Wave 9 lane W
//     validated against retail through the empty-list probe. Their two new
//     `this`-taking twins are transcribed from 0x18000ca90 / 0x18000cac0
//     instruction by instruction (the store ORDER differs from the inline copy
//     and is preserved — see buffers_win.cpp).
//   * `RenderBuffer::Recreate`'s width/height arithmetic is not transcribed a
//     second time. It is EXACTLY `ImageBuffer::Width()` / `Height()`, which are
//     already ported AND carry a FULL scalar oracle
//     (`Video_ImageBuffer_Width` / `_Height`, buffers.cpp:429/450). Calling
//     them rather than re-deriving the bit8/bit9/bit6 ladder means the
//     equivalence is enforced by the compiler instead of asserted in a comment,
//     and it puts the one piece of real arithmetic in this file under an oracle
//     that already runs. Retail inlines that ladder, so under
//     `BANISHED_RT_DIFF` this shows up as extra `Video::ImageBuffer::Width`
//     call counts; that is a measurement artefact, not a behaviour change.
//
// PLATFORM. Nothing here includes <windows.h>. The Device lock lives in
// buffers_win.cpp per PLAN §4.1 decision 1.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/video/Buffers.h"
#include "../../types/video/Device.h"
#include "../../types/video/ImageBuffer.h"

using RtVideo::Device;
using RtVideo::RenderBuffer;
// Video::ImageBuffer, ::IndexBuffer and ::VertexBuffer live in the GLOBAL
// namespace (runtime/types/video/ImageBuffer.h and Buffers.h declare them
// there); only Device.h puts its types in RtVideo. No `using` for those three.

// The Win32 half of Video::Device's recursive render lock. The two
// singleton-reading forms are buffers_win.cpp's originals (Wave 9 lane W); the
// two `This` forms are this lane's, and are the bodies of the exported
// Device::Lock / Device::Unlock.
void RtVideoDeviceLock(void** deviceSingleton);
void RtVideoDeviceUnlock(void** deviceSingleton);
void RtVideoDeviceLockThis(void* device);
void RtVideoDeviceUnlockThis(void* device);

// Ported siblings in buffers.cpp, called directly the way retail's inlined copy
// of the same arithmetic runs. See the banner.
int Video_ImageBuffer_Width(ImageBuffer* self);
int Video_ImageBuffer_Height(ImageBuffer* self);

namespace {

// ---------------------------------------------------------------------------
// The five DATA slots these bodies read.
//
// All five are exported and all five are minted, so they keep working with the
// DLL absent. `_firstRenderBuffer` is the row this lane added to lane C2's mint
// table — see the banner.
// ---------------------------------------------------------------------------
Device** DeviceSlot() {
    Device** p = RT_DATA_SLOT(Device**, "?_singleton@Device@Video@@0PEAV12@EA");
    return p;
}
ImageBuffer** FirstSurfaceSlot() {
    ImageBuffer** p = RT_DATA_SLOT(ImageBuffer**,
        "?_firstSurface@ImageBuffer@Video@@0PEAV12@EA");
    return p;
}
IndexBuffer** FirstIndexBufferSlot() {
    IndexBuffer** p = RT_DATA_SLOT(IndexBuffer**,
        "?_firstBuffer@IndexBuffer@Video@@0PEAV12@EA");
    return p;
}
VertexBuffer** FirstVertexBufferSlot() {
    VertexBuffer** p = RT_DATA_SLOT(VertexBuffer**,
        "?_firstBuffer@VertexBuffer@Video@@0PEAV12@EA");
    return p;
}
RenderBuffer** FirstRenderBufferSlot() {
    RenderBuffer** p = RT_DATA_SLOT(RenderBuffer**,
        "?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA");
    return p;
}

// ---------------------------------------------------------------------------
// The intrusive list link every walker steps through.
//
// The offsets are retail's, read off the four disassemblies, and they are NOT
// the same across the four classes:
//
//   ImageBuffer   +0x58   (0x1800f2e35  MOV RBX,[RBX+0x58])
//   IndexBuffer   +0x18   (0x1800f3584  MOV RBX,[RBX+0x18])
//   VertexBuffer  +0x18   (0x1800f56e4  MOV RBX,[RBX+0x18])
//   RenderBuffer  +0x20   (0x1800f4078  MOV RBX,[RBX+0x20])
//
// Two notes on the headers this contradicts, neither of which is a defect in
// this file:
//   * ImageBuffer.h calls +0x50/+0x58 `field50` / `field58` ("unknown; copied in
//     the copy ctor"). They are the list links; the walkers prove it.
//   * Buffers.h names IndexBuffer/VertexBuffer +0x10 `listPrev` and +0x18
//     `listNext`, and RenderBuffer's +0x18 `next` / +0x20 `prev`. Retail's four
//     walkers step through +0x18 for the buffers and +0x20 for RenderBuffer, so
//     either RenderBuffer's two names are swapped relative to the buffers' or
//     the two families genuinely walk in opposite directions. The list is
//     circular, so both terminate; this lane transcribes the OFFSET retail uses
//     and does not rename anything on the strength of it.
// ---------------------------------------------------------------------------
template <typename T>
T* LinkStep(T* p, size_t offset) {
    return *reinterpret_cast<T**>(reinterpret_cast<uint8_t*>(p) + offset);
}

constexpr size_t kImageLink  = 0x58;
constexpr size_t kBufferLink = 0x18;
constexpr size_t kRenderLink = 0x20;

// A companion object's vtable slot, by BYTE offset — the shipped code dispatches
// through raw offsets and so does this.
template <typename Fn>
Fn CompanionSlot(void* companion, size_t byteOffset) {
    void** vtable = *reinterpret_cast<void***>(companion);
    return reinterpret_cast<Fn>(
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vtable) + byteOffset));
}

// The DeviceData factory slot every RecreateDynamic calls to rebuild a
// companion. `deviceData` NULL yields a NULL companion and the shipped code
// then dispatches through it anyway — see the note at each call site.
void* MakeCompanion(size_t deviceDataSlot, void* subject) {
    void* deviceData = (*DeviceSlot())->deviceData;
    if (deviceData == nullptr)
        return nullptr;                       // XOR EAX,EAX
    using Fn = void* (*)(void*, void*);
    return CompanionSlot<Fn>(deviceData, deviceDataSlot)(deviceData, subject);
}

// The one shape all four ReleaseDynamic walkers share for a single element:
// destroy the companion through its scalar deleting destructor (vtable slot 0,
// flag 1) and null the field. Guarded on the companion, exactly as retail is.
void ReleaseCompanion(void** companionField) {
    void* companion = *companionField;
    if (companion != nullptr) {
        using Fn = void (*)(void*, int);
        CompanionSlot<Fn>(companion, 0)(companion, 1);
    }
    *companionField = nullptr;
}

// ImageBuffer's per-element guard, from 0x1800f2dd0 and 0x1800f2ec8:
//
//     EAX = kind;  CMP EAX,1;  JBE take
//     ADD EAX,-3;  CMP EAX,2;  JA  skip          (unsigned)
//
// i.e. kind in {0, 1, 3, 4, 5}. Note this is NOT ImageBuffer::IsRenderBuffer()
// ({0,1,4,5}) — the dynamic kind 3 is included here and excluded there, so the
// existing accessor deliberately is not reused.
bool ImageBufferIsDynamicResource(const ImageBuffer* p) {
    const int32_t kind = p->kind;
    if (kind <= 1)
        return true;
    return static_cast<uint32_t>(kind - 3) <= 2u;
}

}  // namespace

// ============================================================================
// Video::Device::Lock  —  retail 0x18000ca90, ordinal 3214
// Video::Device::Unlock —  retail 0x18000cac0, ordinal 4462
//
// Thin `this`-taking wrappers over the Win32 half. They are NOT the same as the
// inline copies the four PlatformConstruct/PlatformDestruct bodies carry: those
// load `Device::_singleton` themselves, these operate on the passed `this`, and
// retail's exported Lock stores the depth BEFORE the owner id while the inline
// copy stores the owner first. buffers_win.cpp keeps both orders.
// ============================================================================
extern "C" void Video_Device_Lock(void* self) {
    RtVideoDeviceLockThis(self);
}

extern "C" void Video_Device_Unlock(void* self) {
    RtVideoDeviceUnlockThis(self);
}

// ============================================================================
// Video::ImageBuffer::RecreateDynamic  —  retail 0x1800f2d90, ordinal 3523
//
// Walks _firstSurface under the Device lock. For each element whose `kind` is a
// dynamic-resource kind: build a fresh companion through DeviceData vtable
// +0x50, store it at ImageBuffer+0x60, then poke it once through one of three
// companion slots selected by ImageBuffer::flags bits 4 and 5.
//
// THE UNGUARDED COMPANION DEREF IS RETAIL'S. When `Device::deviceData` is null
// the shipped body stores a null companion at +0x60 and then dispatches through
// it (0x1800f2df3 XOR EAX,EAX -> 0x1800f2e00 store -> 0x1800f2e14 CALL [RDX+8]).
// Per CLAUDE.md that deref stays unguarded: retail relies on the device data
// being there whenever any ImageBuffer is on this list, and a guard here would
// hide an upstream ordering defect rather than fix it.
//
// The head is RE-READ from memory on every iteration (0x1800f2e39 compares
// against `[_firstSurface]`, not a cached register), so the loop below re-reads
// it too.
// ============================================================================
extern "C" void Video_ImageBuffer_RecreateDynamic() {
    RtVideoDeviceLock(reinterpret_cast<void**>(DeviceSlot()));

    ImageBuffer** head = FirstSurfaceSlot();
    ImageBuffer*  p    = *head;
    if (p != nullptr) {
        do {
            if (ImageBufferIsDynamicResource(p)) {
                void* companion = MakeCompanion(0x50, p);
                p->platformData = companion;

                const uint32_t flags = p->flags;
                if ((flags >> 4) & 1u) {
                    using Fn = void (*)(void*);
                    CompanionSlot<Fn>(companion, 0x08)(companion);
                } else if ((flags >> 5) & 1u) {
                    using Fn = void (*)(void*);
                    CompanionSlot<Fn>(companion, 0x10)(companion);
                } else {
                    using Fn = void (*)(void*, int);
                    CompanionSlot<Fn>(companion, 0x18)(companion, 0);
                }
            }
            p = LinkStep(p, kImageLink);
        } while (p != *head);
    }

    RtVideoDeviceUnlock(reinterpret_cast<void**>(DeviceSlot()));
}

// ============================================================================
// Video::ImageBuffer::ReleaseDynamic  —  retail 0x1800f2e80, ordinal 3537
//
// THE STALE HEAD IS DELIBERATE. Retail loads the head into RCX once
// (0x1800f2eb7), reloads it ONLY inside the taken branch (0x1800f2eef), and
// compares the walk against that possibly-stale RCX (0x1800f2efa). An element
// that fails the kind test therefore does not refresh the terminator. That is
// reproduced literally: `terminator` below is updated in exactly the same place.
// ============================================================================
extern "C" void Video_ImageBuffer_ReleaseDynamic() {
    RtVideoDeviceLock(reinterpret_cast<void**>(DeviceSlot()));

    ImageBuffer** head       = FirstSurfaceSlot();
    ImageBuffer*  terminator = *head;
    if (terminator != nullptr) {
        ImageBuffer* p = terminator;
        do {
            if (ImageBufferIsDynamicResource(p)) {
                ReleaseCompanion(&p->platformData);
                terminator = *head;
            }
            p = LinkStep(p, kImageLink);
        } while (p != terminator);
    }

    RtVideoDeviceUnlock(reinterpret_cast<void**>(DeviceSlot()));
}

// ============================================================================
// Video::IndexBuffer::RecreateDynamic   —  retail 0x1800f3500, ordinal 3524
// Video::VertexBuffer::RecreateDynamic  —  retail 0x1800f5660, ordinal 3526
//
// One shape, two heads, two DeviceData factory slots (+0x60 index, +0x68
// vertex). The per-element guard is the `dynamic` bool at +0x04, and after the
// companion is stored at +0x08 retail calls companion vtable +0x08 with
// (companion, self->size, /*bool*/ true, /*void**/ nullptr) — that is the
// buffer's Create, with `dynamic` hardcoded true and no initial data.
//
// The head reload sits inside the taken branch here too (0x1800f357d /
// 0x1800f56dd), so the terminator goes stale for a skipped element exactly as
// in ReleaseDynamic above.
// ============================================================================
namespace {

template <typename Buf>
void BufferRecreateDynamic(Buf** head, size_t deviceDataSlot) {
    RtVideoDeviceLock(reinterpret_cast<void**>(DeviceSlot()));

    Buf* terminator = *head;
    if (terminator != nullptr) {
        Buf* p = terminator;
        do {
            if (p->dynamic) {
                void* companion = MakeCompanion(deviceDataSlot, p);
                p->platformData = companion;
                // Unguarded, as retail is — see the ImageBuffer note.
                using Fn = void (*)(void*, int32_t, bool, void*);
                CompanionSlot<Fn>(companion, 0x08)(companion, p->size, true, nullptr);
                terminator = *head;
            }
            p = LinkStep(p, kBufferLink);
        } while (p != terminator);
    }

    RtVideoDeviceUnlock(reinterpret_cast<void**>(DeviceSlot()));
}

template <typename Buf>
void BufferReleaseDynamic(Buf** head) {
    RtVideoDeviceLock(reinterpret_cast<void**>(DeviceSlot()));

    Buf* terminator = *head;
    if (terminator != nullptr) {
        Buf* p = terminator;
        do {
            if (p->dynamic) {
                ReleaseCompanion(&p->platformData);
                terminator = *head;
            }
            p = LinkStep(p, kBufferLink);
        } while (p != terminator);
    }

    RtVideoDeviceUnlock(reinterpret_cast<void**>(DeviceSlot()));
}

}  // namespace

extern "C" void Video_IndexBuffer_RecreateDynamic() {
    BufferRecreateDynamic(FirstIndexBufferSlot(), 0x60);
}
extern "C" void Video_IndexBuffer_ReleaseDynamic() {
    BufferReleaseDynamic(FirstIndexBufferSlot());
}
extern "C" void Video_VertexBuffer_RecreateDynamic() {
    BufferRecreateDynamic(FirstVertexBufferSlot(), 0x68);
}
extern "C" void Video_VertexBuffer_ReleaseDynamic() {
    BufferReleaseDynamic(FirstVertexBufferSlot());
}

// ============================================================================
// Video::RenderBuffer::Recreate  —  retail 0x1800f4170, ordinal 3521
//
// Rebuild one RenderBuffer's companion (DeviceData vtable +0x98), poke it
// through companion slot +0x08, then recompute the render buffer's cached
// width/height from whichever attachment it has.
//
// The width/height ladder is `ImageBuffer::Width()` / `Height()` verbatim —
// bit8 = device backbuffer size, bit9 = half of it, otherwise the buffer's own
// size shifted right by Configuration::shadowScalar when bit6 is set — applied
// to the colour attachment if there is one and to the depth attachment
// otherwise. Rather than transcribe that ladder a fourth time this calls the
// two ported accessors; see the file banner for why.
//
// `depthBuffer` is dereferenced without a null check when `colorBuffer` is
// null, which is retail (0x1800f41d3 MOV RCX,[RBX+8] then 0x1800f41d7 reads
// [RCX+0x34] unconditionally). A RenderBuffer with neither attachment never
// reaches this function, because nothing puts one on the list.
// ============================================================================
extern "C" void Video_RenderBuffer_Recreate(RenderBuffer* self) {
    void* companion = MakeCompanion(0x98, self);
    self->platformData = companion;
    {
        // Unguarded, as retail is (0x1800f41a0 reads [RAX] with no test).
        using Fn = void (*)(void*);
        CompanionSlot<Fn>(companion, 0x08)(companion);
    }

    ImageBuffer* source = static_cast<ImageBuffer*>(self->colorBuffer);
    if (source == nullptr)
        source = static_cast<ImageBuffer*>(self->depthBuffer);

    self->width  = Video_ImageBuffer_Width(source);
    self->height = Video_ImageBuffer_Height(source);
}

// ============================================================================
// Video::RenderBuffer::RecreateDynamic  —  retail 0x1800f4030, ordinal 3525
// Video::RenderBuffer::ReleaseDynamic   —  retail 0x1800f40c0, ordinal 3539
//
// No per-element guard on either: every RenderBuffer on the list is swept.
// Both re-read the head from memory on every iteration (0x1800f407c and
// 0x1800f4120 compare against `[_firstRenderBuffer]`, not a cached register),
// unlike the other three classes — so both loops below re-read it too.
// ============================================================================
extern "C" void Video_RenderBuffer_RecreateDynamic() {
    RtVideoDeviceLock(reinterpret_cast<void**>(DeviceSlot()));

    RenderBuffer** head = FirstRenderBufferSlot();
    RenderBuffer*  p    = *head;
    if (p != nullptr) {
        do {
            Video_RenderBuffer_Recreate(p);
            p = LinkStep(p, kRenderLink);
        } while (p != *head);
    }

    RtVideoDeviceUnlock(reinterpret_cast<void**>(DeviceSlot()));
}

extern "C" void Video_RenderBuffer_ReleaseDynamic() {
    RtVideoDeviceLock(reinterpret_cast<void**>(DeviceSlot()));

    RenderBuffer** head = FirstRenderBufferSlot();
    RenderBuffer*  p    = *head;
    if (p != nullptr) {
        do {
            ReleaseCompanion(&p->platformData);
            p = LinkStep(p, kRenderLink);
        } while (p != *head);
    }

    RtVideoDeviceUnlock(reinterpret_cast<void**>(DeviceSlot()));
}

// ============================================================================
// Video::Device::RecreateDynamic  —  retail 0x1800f80b0, ordinal 3522
// Video::Device::ReleaseDynamic   —  retail 0x1800f80e0, ordinal 3536
//
// The four-call forwarders. ORDER IS LOAD-BEARING and is retail's: recreate
// index, vertex, image, render; release render, image, vertex, index — the
// exact reverse, because a RenderBuffer's companion refers to the ImageBuffer
// companions it is built from.
//
// Note both take NO `this`. They are `QEAAXXZ` — non-static members with a
// `this` in RCX that neither body reads (0x1800f80b0 goes straight to
// `SUB RSP,0x28` and never touches RCX). Declaring them with a parameter would
// be equally correct on the Microsoft x64 ABI; declaring them without one is
// what the bodies do, and dx11's import table types them as `void (*)(void*)`,
// which passes an argument the callee ignores. Both spellings interoperate.
// ============================================================================
extern "C" void Video_Device_RecreateDynamic() {
    Video_IndexBuffer_RecreateDynamic();
    Video_VertexBuffer_RecreateDynamic();
    Video_ImageBuffer_RecreateDynamic();
    Video_RenderBuffer_RecreateDynamic();
}

extern "C" void Video_Device_ReleaseDynamic() {
    Video_RenderBuffer_ReleaseDynamic();
    Video_ImageBuffer_ReleaseDynamic();
    Video_VertexBuffer_ReleaseDynamic();
    Video_IndexBuffer_ReleaseDynamic();
}

// ============================================================================
// Registration
//
// Every name is spelled as a LITERAL here, a second time, because
// tools/runtime_census.py matches `RegisterRuntimeExport("<literal>"` and
// cannot see through a named constant
// (reference_census_cannot_see_nonliteral_registrations.md).
//
// Address doors are registered alongside the name doors for the four functions
// retail reaches by direct near CALL from a body this lane also ports
// (`Device::RecreateDynamic`/`ReleaseDynamic` -> the eight statics, and
// `RenderBuffer::RecreateDynamic` -> `Recreate`). Our own forwarders call the
// C++ symbols directly, so those doors exist for any OTHER caller that still
// goes by address; they are not counted twice
// (reference_census_three_blind_spots.md).
// ============================================================================
int RtVideoDynamicResourcesRegister() {
    int n = 0;

    RegisterRuntimeExport("?Lock@Device@Video@@QEAAXXZ",
                          (void*)&Video_Device_Lock); ++n;
    RegisterRuntimeExport("?Unlock@Device@Video@@QEAAXXZ",
                          (void*)&Video_Device_Unlock); ++n;
    RegisterRuntimeExport("?RecreateDynamic@Device@Video@@QEAAXXZ",
                          (void*)&Video_Device_RecreateDynamic); ++n;
    RegisterRuntimeExport("?ReleaseDynamic@Device@Video@@QEAAXXZ",
                          (void*)&Video_Device_ReleaseDynamic); ++n;
    RegisterRuntimeExport("?RecreateDynamic@ImageBuffer@Video@@SAXXZ",
                          (void*)&Video_ImageBuffer_RecreateDynamic); ++n;
    RegisterRuntimeExport("?ReleaseDynamic@ImageBuffer@Video@@SAXXZ",
                          (void*)&Video_ImageBuffer_ReleaseDynamic); ++n;
    RegisterRuntimeExport("?RecreateDynamic@IndexBuffer@Video@@SAXXZ",
                          (void*)&Video_IndexBuffer_RecreateDynamic); ++n;
    RegisterRuntimeExport("?ReleaseDynamic@IndexBuffer@Video@@SAXXZ",
                          (void*)&Video_IndexBuffer_ReleaseDynamic); ++n;
    RegisterRuntimeExport("?RecreateDynamic@VertexBuffer@Video@@SAXXZ",
                          (void*)&Video_VertexBuffer_RecreateDynamic); ++n;
    RegisterRuntimeExport("?ReleaseDynamic@VertexBuffer@Video@@SAXXZ",
                          (void*)&Video_VertexBuffer_ReleaseDynamic); ++n;
    RegisterRuntimeExport("?RecreateDynamic@RenderBuffer@Video@@SAXXZ",
                          (void*)&Video_RenderBuffer_RecreateDynamic); ++n;
    RegisterRuntimeExport("?ReleaseDynamic@RenderBuffer@Video@@SAXXZ",
                          (void*)&Video_RenderBuffer_ReleaseDynamic); ++n;
    RegisterRuntimeExport("?Recreate@RenderBuffer@Video@@QEAAXXZ",
                          (void*)&Video_RenderBuffer_Recreate); ++n;

    RegisterRuntimeAddr(0x18000ca90, (void*)&Video_Device_Lock);
    RegisterRuntimeAddr(0x18000cac0, (void*)&Video_Device_Unlock);
    RegisterRuntimeAddr(0x1800f80b0, (void*)&Video_Device_RecreateDynamic);
    RegisterRuntimeAddr(0x1800f80e0, (void*)&Video_Device_ReleaseDynamic);
    RegisterRuntimeAddr(0x1800f2d90, (void*)&Video_ImageBuffer_RecreateDynamic);
    RegisterRuntimeAddr(0x1800f2e80, (void*)&Video_ImageBuffer_ReleaseDynamic);
    RegisterRuntimeAddr(0x1800f3500, (void*)&Video_IndexBuffer_RecreateDynamic);
    RegisterRuntimeAddr(0x1800f35d0, (void*)&Video_IndexBuffer_ReleaseDynamic);
    RegisterRuntimeAddr(0x1800f5660, (void*)&Video_VertexBuffer_RecreateDynamic);
    RegisterRuntimeAddr(0x1800f5730, (void*)&Video_VertexBuffer_ReleaseDynamic);
    RegisterRuntimeAddr(0x1800f4030, (void*)&Video_RenderBuffer_RecreateDynamic);
    RegisterRuntimeAddr(0x1800f40c0, (void*)&Video_RenderBuffer_ReleaseDynamic);
    RegisterRuntimeAddr(0x1800f4170, (void*)&Video_RenderBuffer_Recreate);

    return n;
}
