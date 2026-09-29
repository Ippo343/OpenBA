// ============================================================================
// runtime/src/video/video_resources.cpp — Wave 18 lane VRES
//   The Video resource lifecycle: RenderBuffer ctor/dtor/Bind,
//   ImageBuffer::Recreate, and the Configuration / Device destructors.
// ============================================================================
//
// Six of lane VRES's seven rows. The seventh
// (?SaveImageToFile@ImageBuffer@Video@@) is in buffers.cpp's holdback note and
// stays there; docs/runtime_port/WAVE18_LANE_VRES.md carries its verdict.
//
//   | Export                                   | Retail      | Ord  |
//   |------------------------------------------|-------------|------|
//   | ??0RenderBuffer@Video@@QEAA@XZ           | 0x1800f3ee0 |  346 |
//   | ??1RenderBuffer@Video@@QEAA@XZ           | 0x1800f4020 |  659 |
//   | ?Bind@RenderBuffer@Video@@QEBAX...       | 0x1800f4300 | 1656 |
//   | ?Recreate@ImageBuffer@Video@@QEAAXXZ     | 0x1800f33f0 | 3520 |
//   | ??1Configuration@Video@@QEAA@XZ          | 0x1800f6f80 |  557 |
//   | ??1Device@Video@@QEAA@XZ                 | 0x1800f7290 |  569 |
//
// Every address and ordinal above is the shipped export directory's own answer
// (pefile over C:\GOG Games\Banished\Runtime-x64.dll), read BEFORE any body was
// written — reference_resolveexport_phantom_names.md. Every body below was
// transcribed from `disassemble_function` at that address, not from Ghidra's
// decompiled C (reference_ghidra_c_output_vs_disassembly.md).
//
// A CORRECTION TO THIS LANE'S OWN BRIEF, recorded here because it changed the
// shape of the work: `dx11/src/contextdata.cpp` names `?Bind@RenderBuffer@` at
// `0x180013448`, which is NOT this function — 0x180013448 is a displacement
// inside an unrelated copy-assign body in Runtime-x64.dll. It is not a defect:
// both DLLs prefer image base 0x180000000 and that constant is VideoDX11's own
// IAT SLOT for the import, which is exactly what `Dx11ResolveRuntimeImport`
// wants. The Runtime function is 0x1800f4300, and that is what is registered.
//
// ---------------------------------------------------------------------------
// THE TWO DESTRUCTORS, AND WHAT EACH ONE DESTROYS VS MERELY FREES
// ---------------------------------------------------------------------------
// A dtor is the highest-risk shape in this project: a prior lane's written
// recipe would have swapped a body that destroys every element for one that
// only frees the block (reference_a_written_recipe_can_be_wrong.md). So the
// distinction is drawn per array, from the disassembly, before the code:
//
// ??1Device@Video@@ owns THREE arrays and destroys them through three DIFFERENT
// retail helpers, which are NOT interchangeable:
//
//   +0x40 Array<GraphicsInterface>  -> 0x1800109d0  DESTROYS EVERY ELEMENT
//         Its element helper 0x180010f00 walks stride 0x18 and, per element,
//         (a) recurses into 0x180010da0 over the element's Adapter array,
//         (b) frees the Adapter buffer and clears its 16-byte header,
//         (c) releases the System::String at +0x00.
//         0x180010da0 in turn, per Adapter (stride 0x38), frees the
//         multisample buffer at +0x18, recurses into 0x180010cf0 over the
//         DisplayMode array to free each mode's RefreshRate buffer, frees the
//         DisplayMode buffer, and releases the Adapter's name String.
//         Writing this one as "free the block" would leak, per Device, one
//         String and one Array per interface, plus one String and three
//         Arrays per adapter, plus one Array per display mode. That is the
//         exact defect this comment exists to prevent.
//
//   +0x30 Array<bool>              -> 0x180009560  FREES THE BLOCK ONLY
//         No element loop at all, and that is CORRECT: `bool` has no
//         destructor. It is instruction-for-instruction lane Q's
//         `rtcoll::ArrayDtorImpl` (0x180009560 is one of the eleven addresses
//         that body already covers).
//
//   +0x20 Array<DisplayMode>       -> 0x180010830  DESTROYS EVERY ELEMENT
//         Element helper 0x180010cf0 = lane D2's BODY A with offs {0x08},
//         stride 0x18: free each mode's RefreshRate buffer, clear the header.
//
// ??1Configuration@Video@@ destroys NOTHING and frees NOTHING. Its entire body
// is two instructions: store 0 over `?_singleton@Configuration@Video@@`. It
// does not even read `this`. Engine::Shutdown does the MemFree separately
// (runtime/src/engine/engine_lifecycle.cpp:969).
//
// ---------------------------------------------------------------------------
// THREE HELPER ADDRESSES lane D2 DID NOT ENUMERATE
// ---------------------------------------------------------------------------
// array_destroy.cpp's banner says the destroy-helper cluster is ELEVEN
// addresses and THREE bodies. Reached from ??1Device, it is fourteen:
// 0x180010cf0, 0x180010da0 and 0x180010f00 are three more, and all three are
// covered by the bodies and specs that file already exports:
//
//   0x180010cf0 == BODY A, offs {0x08}, stride 0x18   (== kD_InnerArr_18's row,
//                  a SECOND address for that spec; the banner lists only
//                  0x1800419b0)
//   0x180010da0 == a nested per-Adapter destroy: A{0x18}, recurse, A{0x08},
//                  B{0x00}
//   0x180010f00 == a nested per-GraphicsInterface destroy: recurse, A{0x08},
//                  B{0x00}
//
// The two nested ones are the shape lane D2 called "the eleventh, 0x18005c550,
// a RECURSIVE destroy that calls one of each body before freeing". They are
// composed here out of `rtcoll::FreeInnerArraysCore` / `ReleaseStringsCore` /
// `ArrayDtorImpl` rather than re-derived, so this lane inherits lane D2's
// verification of those bodies instead of writing a fourth copy of them. They
// are deliberately NOT registered at their own addresses, for the reason
// array_destroy.cpp gives for the other eight: nothing in this tree resolves
// them, and a registration with no caller dilutes the census's dead-port
// signal (reference_uncalled_port_is_not_verified.md).
//
// PLATFORM. `??1Device` calls DeleteCriticalSection on the lock at Device+0x60.
// PLAN §4.1 decision 1 keeps runtime/src/ free of <windows.h>, so that one call
// is `RtVideoDeleteDeviceLock` in video_resources_win.cpp.
// ============================================================================

// B.4 (R-VIDEO): the three nested destroys are the typed helpers now. 0x180010cf0
// is `RtVideo_ArrayModeElemDestroy` (device_startup.cpp) and 0x180010da0 is
// `RtVideo_ArrayAdapterElemDestroy` (device_enum_types.cpp) -- the same retail
// addresses, ported once each on the named fields, spelling B of the String
// release included -- and 0x180010f00 is `RtVideo_ArrayInterfaceElemDestroy`,
// MOVED from this file into device_enum_types.cpp beside its two siblings. The
// by-offset `HdrAt` view and the `DestroySpec` rows it fed are gone.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../types/video/Device.h"
#include "../../types/video/ImageBuffer.h"
#include "../../types/video/CompanionData.h"   // ENDGAME B.3.5: Video::{Device,RenderBuffer}Data
#include "../collection/array_internal.h"

using Video::Configuration;
using Video::Device;
using Video::DeviceData;
using Video::RenderBuffer;

namespace {

// One `{data, size, capacity}` triple's `ArrayDtorImpl` (array.cpp:221):
// MemFree the block (the allocator's own null test), then the three zero stores.
template <class T>
void FreeTriple(T*& data, int32_t& size, int32_t& capacity) {
    rtcoll::MemFree(data);
    data     = nullptr;
    size     = 0;
    capacity = 0;
}

}  // namespace

// ---------------------------------------------------------------------------

// ============================================================================
// ??0RenderBuffer@Video@@QEAA@XZ — 0x1800f3ee0, ordinal 346
// ============================================================================
//
// Twelve instructions. Three qword zero stores covering +0x00..+0x17, a CALL to
// PlatformConstruct, and `MOV RAX,RBX` — the ctor returns `this`, which MSVC
// requires and which minimap_paint.cpp's `fn_RenderBuffer_Ctor_t` already
// expects.
//
// `MOV qword ptr [RCX+0x10],RAX` is ONE eight-byte store covering `width` AND
// `height`. Writing them as two ints is the same bytes; it is spelled as two
// named fields because that is what Device.h pins.
//
// THIS CTOR STAYS A DOOR (no real `RenderBuffer::RenderBuffer()`): it publishes
// `this` into the `_firstRenderBuffer` intrusive list, whose head is an
// `RT_DATA_SLOT` read rather than a C++ static — see Device.h's banner.
RenderBuffer* Video_RenderBuffer_Ctor(RenderBuffer* self) {
    self->colorBuffer = nullptr;                       // MOV [RCX],RAX
    self->depthBuffer = nullptr;                       // MOV [RCX+8],RAX
    self->width  = 0;                                  // MOV [RCX+0x10],RAX,
    self->height = 0;                                  //   one store, two fields
    self->Video::RenderBuffer::PlatformConstruct();
    return self;                                       // MOV RAX,RBX
}

// ============================================================================
// ??1RenderBuffer@Video@@QEAA@XZ — 0x1800f4020, ordinal 659
// ============================================================================
//
// ONE instruction: `JMP 0x1800f4400`. The destructor IS PlatformDestruct — the
// linker folded them, so retail's ~RenderBuffer is a tail-call alias and
// nothing else. What the destination does is unlink `this` from the circular
// `_firstRenderBuffer` list and release the RenderBufferData companion at +0x28
// — and it does NOT null +0x28 afterwards (buffers.cpp's "ImageBuffer
// asymmetry"). Still a DOOR and not a real `~RenderBuffer()` — the unlink it
// delegates to is what a real destructor could not publish.
void Video_RenderBuffer_Dtor(RenderBuffer* self) {
    self->Video::RenderBuffer::PlatformDestruct();
}

// ============================================================================
// ?Bind@RenderBuffer@Video@@QEBAXAEAVContext@2@@Z — 0x1800f4300, ordinal 1656
// ============================================================================
//
//     MOV RCX,[RCX+0x28]   ; this->platformData
//     MOV RAX,[RCX]        ; its vptr
//     JMP [RAX+0x10]       ; slot 2, with RDX (the Context&) untouched
//
// The deref of `platformData` is unguarded in retail and is left unguarded
// here. A null there means PlatformConstruct never ran on this object, which is
// an upstream defect to find, not one to paper over.
void Video::RenderBuffer::Bind(Context& context) const {
    platformData->Bind(&context);   // Video::RenderBufferData slot 2
}

// ============================================================================
// ?Recreate@ImageBuffer@Video@@QEAAXXZ — 0x1800f33f0, ordinal 3520
// ============================================================================
//
// Device-lost / resize recovery for one ImageBuffer. Called from
// runtime/src/save/saveload_update.cpp:279.
//
//     dd = Video::Device::_singleton->deviceData        ; +0x100
//     pd = dd ? dd->CreateImageBufferData(this) : null  ; slot 10, +0x50
//     this->_data = pd                                  ; +0x60
//     f = this->_flags                                  ; +0x34, read BEFORE
//     if (f & 0x10) tail  pd->AttachColorBuffer()
//     else if (f & 0x20) tail  pd->AttachDepthBuffer()
//     else               tail  pd->Create(nullptr)
//
// Note the two tests are `SHR ECX,4 / TEST CL,1` and `SHR EAX,5 / TEST AL,1` —
// bits 4 and 5, which ImageBuffer.h already documents as "device-reset-tracked
// (set by DX11 color/depth attach)". They are read from the SAME loaded value,
// so a flags write by the factory call could not affect the second test;
// reproduced by loading `_flags` once.
//
// FAITHFUL UNGUARDED DEREF. When `dd` is null, `pd` is null and retail
// dereferences it immediately. That is not a bug to guard
// (CLAUDE.md "Don't fix a faithful crash with a null check") — it is retail
// relying on Recreate never being called with no device, and a guard would skip
// the recreate rather than restore it.
void Video::ImageBuffer::Recreate() {
    Device* const dev = Device::_singleton;
    DeviceData* const dd = dev ? dev->deviceData : nullptr;

    ImageBufferData* pd = nullptr;
    if (dd) pd = dd->CreateImageBufferData(this);
    _data = pd;

    const uint32_t f = _flags;
    if ((f >> 4) & 1u) { pd->AttachColorBuffer(); return; }
    if ((f >> 5) & 1u) { pd->AttachDepthBuffer(); return; }
    (void)pd->Create(nullptr);
}

// ============================================================================
// ??1Configuration@Video@@QEAA@XZ — 0x1800f6f80, ordinal 557
// ============================================================================
//
//     MOV qword ptr [0x1801fe7a0],0x0
//     RET
//
// That is the whole function. It does not read `this`, destroys nothing, frees
// nothing, and takes no branch: it clears the exported singleton pointer
// unconditionally, even for a Configuration that was never the singleton.
// Engine::Shutdown does the MemFree afterwards.
void Video_Configuration_Dtor(void* self) {
    (void)self;                        // retail never touches RCX
    Configuration::_singleton = nullptr;
}

// ============================================================================
// ??1Device@Video@@QEAA@XZ — 0x1800f7290, ordinal 569
// ============================================================================
//
// Six steps, in retail's order:
//
//   1. dd = this->deviceData (+0x100); if (dd) dd->DeletingDtor(1) — MSVC's
//      scalar deleting destructor with the delete flag, destroy AND free.
//   2. this->deviceData = nullptr.
//   3. if (this->type == 0) `?_singleton@Device@Video@@` = nullptr.
//      `CMP dword [RBX],ECX / CMOVZ RAX,RCX` — a CMOV, so the singleton slot is
//      REWRITTEN EITHER WAY: with null when this is the type-0 device, with its
//      own current value when it is not. Reproduced as an unconditional store.
//   4. DeleteCriticalSection(&this->lock) — +0x60, via the _win twin.
//   5..6. The three arrays, in retail's order 0x40, 0x30, 0x20. NOTE the order
//      is NOT the offset order and is NOT reverse-declaration order; it is
//      transcribed, not tidied.
//
// WHAT IT DOES NOT TOUCH: `dllSlots` at +0xB0 (the three LOADED renderer plugin
// modules standalone/video_interface.cpp fills) is never read by this body — no
// FreeLibrary, no clear.
//
// THIS STAYS A DOOR and not a real `~Device()` (B4_RULES B12: until the dx11
// seam) — RULE B5-6, because `InitializeCriticalSection` is retail's
// `Device::Startup`.
void Video_Device_Dtor(Device* self) {
    // ---- 1 & 2: the DeviceData companion -----------------------------------
    DeviceData* const dd = self->deviceData;
    if (dd) (void)dd->DeletingDtor(1);
    self->deviceData = nullptr;

    // ---- 3: the singleton slot (CMOV, so always stored) --------------------
    Device* keep = Device::_singleton;
    if (self->type == 0) keep = nullptr;
    Device::_singleton = keep;

    // ---- 4: the lock -------------------------------------------------------
    RtVideoDeleteDeviceLock(self);

    // ---- 5: Array<GraphicsInterface> @+0x40 — DESTROY, then free -----------
    RtVideo_ArrayInterfaceElemDestroy(self->interfaceData, self->interfaceCount);   // 0x180010f00
    FreeTriple(self->interfaceData, self->interfaceCount, self->interfaceCapacity);

    // ---- 5b: Array<bool> @+0x30 — FREE ONLY, bool has no destructor --------
    FreeTriple(self->multisampleData, self->multisampleCount, self->multisampleCap);

    // ---- 6: Array<DisplayMode> @+0x20 — DESTROY, then free -----------------
    RtVideo_ArrayModeElemDestroy(self->modeData, self->modeCount);   // 0x180010cf0
    FreeTriple(self->modeData, self->modeCount, self->modeCapacity);
}
