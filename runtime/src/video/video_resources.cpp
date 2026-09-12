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

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/video/Device.h"
#include "../../types/video/ImageBuffer.h"
#include "../collection/array_internal.h"
#include "../collection/array_destroy_internal.h"

using RtVideo::Adapter;
using RtVideo::Configuration;
using RtVideo::Device;
using RtVideo::DisplayMode;
using RtVideo::GraphicsInterface;
using RtVideo::RenderBuffer;

// Implemented in video_resources_win.cpp. Takes the Device, not the lock, so
// the +0x60 offset is stated once on this side where the static_assert lives.
extern "C" void RtVideoDeleteDeviceLock(void* device);

// video_resources_probe.cpp — armed at init, RUN from the first Bind.
void RtVideoResourcesProbeArm(bool on);
void RtVideoResourcesProbeRun();
// Tranche 2's probe. It has its own env var and its own summary line so the
// sealed report's cases=8 and tranche 1's cases=11 stay reproducible; it only
// shares this gate, which is the one moment in a run known to be safe for a
// probe that allocates.
void RtVideoImageBufferSerializeProbeRun();

namespace {

using VoidThisFn = void (*)(void*);

// ---------------------------------------------------------------------------
// The two exported DATA singletons this file writes.
//
// They are SLOTS, not values: the export address IS the variable, so a body
// that kept its own copy would be invisible to every other reader
// (reference_port_must_reproduce_side_effects.md). Both go through
// RT_DATA_SLOT, which is mint-aware and never caches a null.
// ---------------------------------------------------------------------------
void** ConfigSingletonSlot() {
    return RT_DATA_SLOT(void**, "?_singleton@Configuration@Video@@0PEAV12@EA");
}
void** DeviceSingletonSlot() {
    return RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
}

// ---------------------------------------------------------------------------
// The Array<T> header, addressed by byte offset inside a Device.
//
// Device.h spells the three arrays out as nine separate named fields (that is
// what pins the offsets), so this is the adapter between those names and the
// {data, size, capacity} triple lane Q's bodies take. The static_asserts below
// bind the two spellings together at compile time.
// ---------------------------------------------------------------------------
rtcoll::ArrayHdr* HdrAt(void* base, size_t off) {
    return reinterpret_cast<rtcoll::ArrayHdr*>(static_cast<uint8_t*>(base) + off);
}

static_assert(offsetof(Device, modeData) == 0x20, "Device Array<DisplayMode> @+0x20");
static_assert(offsetof(Device, multisampleData) == 0x30, "Device Array<bool> @+0x30");
static_assert(offsetof(Device, interfaceData) == 0x40, "Device Array<GraphicsInterface> @+0x40");
static_assert(offsetof(Device, lock) == 0x60, "Device lock @+0x60 — RtVideoDeleteDeviceLock");

// ---- the specs the three retail helpers are, one row each ------------------
//
// Each is read out of that address's own listing: `stride` is the `ADD RBX,imm`
// at the bottom of its loop, each offset the `LEA RBX,[RDX+imm]` seed plus the
// loop's displacement. They are `constexpr` locals rather than additions to
// array_destroy.cpp's table because only this file uses them; the BODIES are
// that file's.
constexpr rtcoll::DestroySpec kV_ModeRefresh =   // 0x180010cf0
    { "DisplayMode: inner Array<RefreshRate> @+0x008, stride 0x018", 0x018, 1, { 0x008 } };
constexpr rtcoll::DestroySpec kV_AdapterMulti =  // 0x180010da0, first step
    { "Adapter: inner Array<bool> @+0x018, stride 0x038", 0x038, 1, { 0x018 } };
constexpr rtcoll::DestroySpec kV_AdapterModes =  // 0x180010da0, third step
    { "Adapter: inner Array<DisplayMode> @+0x008, stride 0x038", 0x038, 1, { 0x008 } };
constexpr rtcoll::DestroySpec kV_AdapterName =   // 0x180010da0, fourth step
    { "Adapter: String @+0x000, stride 0x038", 0x038, 1, { 0x000 } };
constexpr rtcoll::DestroySpec kV_IfaceAdapters = // 0x180010f00, second step
    { "GraphicsInterface: inner Array<Adapter> @+0x008, stride 0x018", 0x018, 1, { 0x008 } };
constexpr rtcoll::DestroySpec kV_IfaceName =     // 0x180010f00, third step
    { "GraphicsInterface: String @+0x000, stride 0x018", 0x018, 1, { 0x000 } };

// The same "does this slot fit inside its own stride?" check array_destroy.cpp
// runs on its table. Body A writes 16 bytes at the offset, Body B writes 8; an
// offset that overruns the stride is a wild write into the NEXT element.
#define RTV_FITS(SPEC, W) \
    static_assert((SPEC).nOffs == 1 && (SPEC).offs[0] + (W) <= (SPEC).stride, #SPEC " fits")
RTV_FITS(kV_ModeRefresh,   16u);
RTV_FITS(kV_AdapterMulti,  16u);
RTV_FITS(kV_AdapterModes,  16u);
RTV_FITS(kV_AdapterName,    8u);
RTV_FITS(kV_IfaceAdapters, 16u);
RTV_FITS(kV_IfaceName,      8u);
#undef RTV_FITS

// And that they agree with the layouts Device.h pins, so a future edit to
// either side breaks the build rather than the heap.
static_assert(sizeof(DisplayMode) == kV_ModeRefresh.stride, "DisplayMode stride");
static_assert(offsetof(DisplayMode, refreshData) == kV_ModeRefresh.offs[0], "DisplayMode inner");
static_assert(sizeof(Adapter) == kV_AdapterMulti.stride, "Adapter stride");
static_assert(offsetof(Adapter, multisampleData) == kV_AdapterMulti.offs[0], "Adapter multisample");
static_assert(offsetof(Adapter, modeData) == kV_AdapterModes.offs[0], "Adapter modes");
static_assert(offsetof(Adapter, name) == kV_AdapterName.offs[0], "Adapter name");
static_assert(sizeof(GraphicsInterface) == kV_IfaceAdapters.stride, "GraphicsInterface stride");
static_assert(offsetof(GraphicsInterface, adapterData) == kV_IfaceAdapters.offs[0], "iface adapters");
static_assert(offsetof(GraphicsInterface, name) == kV_IfaceName.offs[0], "iface name");

// ---- 0x180010da0 — destroy a range of Adapters -----------------------------
// Order is retail's, and it is not the field order: multisample buffer first,
// then the nested per-mode destroy, then the mode buffer, then the name.
void DestroyAdapterRange(Adapter* base, int count) {
    if (count <= 0) return;                       // TEST R8D,R8D / JLE
    uint8_t* p = reinterpret_cast<uint8_t*>(base);
    const int64_t n = static_cast<int64_t>(count);  // MOVSXD
    for (int64_t i = 0; i < n; ++i) {
        uint8_t* e = p + i * kV_AdapterMulti.stride;
        rtcoll::FreeInnerArraysCore(e, 1, &kV_AdapterMulti);   // +0x18, header cleared
        Adapter* a = reinterpret_cast<Adapter*>(e);
        // 0x180010e3a: CALL 0x180010cf0 with (modeData, modeCount) — BEFORE
        // the mode buffer itself is freed, which is the whole point.
        rtcoll::FreeInnerArraysCore(reinterpret_cast<uint8_t*>(a->modeData),
                                    a->modeCount, &kV_ModeRefresh);
        rtcoll::FreeInnerArraysCore(e, 1, &kV_AdapterModes);   // +0x08, header cleared
        rtcoll::ReleaseStringsCore(e, 1, &kV_AdapterName);     // +0x00
    }
}

// ---- 0x180010f00 — destroy a range of GraphicsInterfaces -------------------
void DestroyInterfaceRange(GraphicsInterface* base, int count) {
    if (count <= 0) return;
    uint8_t* p = reinterpret_cast<uint8_t*>(base);
    const int64_t n = static_cast<int64_t>(count);
    for (int64_t i = 0; i < n; ++i) {
        uint8_t* e = p + i * kV_IfaceAdapters.stride;
        GraphicsInterface* g = reinterpret_cast<GraphicsInterface*>(e);
        DestroyAdapterRange(g->adapterData, g->adapterCount);  // 0x180010f3f
        rtcoll::FreeInnerArraysCore(e, 1, &kV_IfaceAdapters);  // +0x08
        rtcoll::ReleaseStringsCore(e, 1, &kV_IfaceName);       // +0x00
    }
}

// ---------------------------------------------------------------------------
// Live coverage counters.
//
// One site per REGISTERED body, not per file: `calls=` is the only thing that
// distinguishes "verified clean" from "never ran", and four of the causes of
// calls=0 invert the conclusion
// (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md). Each
// counter reports what the call actually DID, so the column reads as work and
// not merely as entry (reference_probe_needs_an_effectiveness_control.md).
//
// *** Every Site has static storage duration. *** RtDiffRegister stores the
// pointer and re-reads it at summary time; a stack site dangles and the summary
// reports it as harness corruption (array.cpp records 144 of those).
// ---------------------------------------------------------------------------
RtDiffSite g_siteDeviceDtor = { "Video::Device::~Device", 0, 0, false, false };
RtDiffSite g_siteConfigDtor = { "Video::Configuration::~Configuration", 0, 0, false, false };
// One site per registered row, so the report can carry a MEASURED calls= for
// each rather than one number for the file. Four of the six have a live route
// in this tree and two do not; only a per-row counter can say which is which,
// and `calls=0` has four causes that invert the conclusion
// (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md).
RtDiffSite g_siteRbCtor = { "Video::RenderBuffer::RenderBuffer", 0, 0, false, false };
RtDiffSite g_siteRbDtor = { "Video::RenderBuffer::~RenderBuffer", 0, 0, false, false };
RtDiffSite g_siteRbBind = { "Video::RenderBuffer::Bind [trampoline into RenderBufferData slot 2]",
                            0, 0, false, false };
RtDiffSite g_siteRecreate = { "Video::ImageBuffer::Recreate", 0, 0, false, false };

void ReportLine(RtDiffSite* site, const char* text) {
    if (!RtDiffEnabled()) return;
    RtDiffRegister(site);
    RtDiffReport(site, true, text, text);
}

// The probe's self-schedule gate; see Video_RenderBuffer_Bind for why the first
// Bind is the moment and RtVideoPortInit() is not.
bool g_probeRunning = false;
inline void ProbeGate() {
    if (g_probeRunning) return;
    g_probeRunning = true;
    RtVideoResourcesProbeRun();
    RtVideoImageBufferSerializeProbeRun();
}

}  // namespace

// ---------------------------------------------------------------------------
// The probe's door onto the two nested walkers.
//
// video_resources_probe.cpp drives THESE bodies, not a copy of them — the rule
// array_destroy_internal.h states, and the reason that header exists: a probe
// that re-derived the body would be testing a copy of the port rather than the
// port. The wrappers exist only because the bodies are in an anonymous
// namespace; they add nothing.
// ---------------------------------------------------------------------------
void RtVideoProbe_DestroyAdapterRange(Adapter* base, int count) {
    DestroyAdapterRange(base, count);
}
void RtVideoProbe_DestroyInterfaceRange(GraphicsInterface* base, int count) {
    DestroyInterfaceRange(base, count);
}

// ---------------------------------------------------------------------------
// 0x180010f00 — the RETAIL-ABI entry point. WAVE 26 LANE INT33.
//
// The banner above says these nested walkers are "deliberately NOT registered
// at their own addresses ... nothing in this tree resolves them". **That second
// clause stopped being true and nobody noticed**: video_resources_probe.cpp:661
// resolves 0x180010f00, so `tools/runtime_census.py` has carried it as a
// CALLED-but-unported `internal` row ever since that probe landed — Wave 25
// lane INTERNAL42 residual #3 called it "the cheapest row left in the bucket"
// and addressed it to a Wave 26 lane. This is that lane.
//
// The deliberate-non-registration reason is preserved for the OTHER two:
// 0x180010cf0 is still unresolved anywhere and still unregistered here.
// (0x180010da0 has been registered since Wave 18 from a SECOND, open-coded port
// in video/device_enum_types.cpp:632 — see §DUPLICATE in the lane doc; that
// duplication is pre-existing and is NOT resolved by this change.)
//
// SIGNATURE, read from 0x180010f00's own prologue and not from the C++ shape:
//   TEST R8D,R8D / JLE   -> R8D is `count`
//   MOV [RSP+8],RCX      -> RCX is spilled and NEVER READ (the array object)
//   RDX                  -> the element base
// i.e. `void(void* ignoredThis, void* base, int32_t count)`, which is exactly
// the `RangeFn` the probe already casts the shipped body to.
//
// WHY REGISTERING DOES NOT BLIND THE ORACLE. The probe's THEIRS side reaches
// the shipped body through `ResolveRuntimeRaw(0x180010f00)`, which is the RAW
// door and does not consult the port registry, so it keeps calling the DLL
// after this. Had it used `ResolveAddr(Module::Runtime, …)` — registry-first —
// this registration would have converted a working oracle into a self-check.
// That direction of the trap is the one this project has actually been bitten
// by, so it is checked rather than assumed.
extern "C" void RtVideo_ArrayInterfaceElemDestroy(void* /*array, spilled and
                                                  never read*/,
                                                  void* base, int32_t count) {
    DestroyInterfaceRange(static_cast<GraphicsInterface*>(base), count);
}

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
// The CALL goes through RT_EXPORT_ONCE, not a direct C++ call to our own
// `Video_RenderBuffer_PlatformConstruct`: retail's is an in-DLL rel32 that our
// registry cannot intercept, so the port-aware door is what makes
// BANISHED_RT_VIDBUFFERS=0 actually route this arm back to the DLL. A direct
// call would make the kill switch a lie for this path — the shape
// reference_off_by_default_was_not_true.md is about.
RenderBuffer* Video_RenderBuffer_Ctor(RenderBuffer* self) {
    self->colorBuffer = nullptr;                       // MOV [RCX],RAX
    self->depthBuffer = nullptr;                       // MOV [RCX+8],RAX
    self->width  = 0;                                  // MOV [RCX+0x10],RAX,
    self->height = 0;                                  //   one store, two fields
    RT_EXPORT_ONCE(VoidThisFn, "?PlatformConstruct@RenderBuffer@Video@@AEAAXXZ")(self);
    ReportLine(&g_siteRbCtor, "three qwords zeroed, PlatformConstruct called");
    return self;                                       // MOV RAX,RBX
}

// ============================================================================
// ??1RenderBuffer@Video@@QEAA@XZ — 0x1800f4020, ordinal 659
// ============================================================================
//
// ONE instruction: `JMP 0x1800f4400`. The destructor IS PlatformDestruct — the
// linker folded them, so retail's ~RenderBuffer is a tail-call alias and
// nothing else.
//
// WHAT IT DESTROYS VS WHAT IT FREES, since that is the question asked of every
// dtor in this lane: by itself, neither. It delegates 100% of its body. What
// the destination does is unlink `this` from the circular `_firstRenderBuffer`
// list and release the RenderBufferData companion at +0x28 — and it does NOT
// null +0x28 afterwards (buffers.cpp:1330 calls that "the ImageBuffer
// asymmetry" and reproduces it deliberately). That destination has been ported
// and oracle-verified since Wave 9 lane GFX2, with a link-state comparator and
// a three-branch counter; this row adds no new memory operation of its own.
void Video_RenderBuffer_Dtor(RenderBuffer* self) {
    RT_EXPORT_ONCE(VoidThisFn, "?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ")(self);
    ReportLine(&g_siteRbDtor, "tail-call alias for PlatformDestruct");
}

// ============================================================================
// ?Bind@RenderBuffer@Video@@QEBAXAEAVContext@2@@Z — 0x1800f4300, ordinal 1656
// ============================================================================
//
//     MOV RCX,[RCX+0x28]   ; this->platformData
//     MOV RAX,[RCX]        ; its vptr
//     JMP [RAX+0x10]       ; slot 2, with RDX (the Context&) untouched
//
// A three-instruction trampoline into Video::RenderBufferData slot 2, the same
// shape as the six Lock/Unlock/Set trampolines already in buffers.cpp. `self`
// is const in the mangled name (QEBA) and the body honours that: it reads +0x28
// and writes nothing.
//
// The deref of `platformData` is unguarded in retail and is left unguarded
// here. A null there means PlatformConstruct never ran on this object, which is
// an upstream defect to find, not one to paper over.
void Video_RenderBuffer_Bind(const RenderBuffer* self, void* context) {
    // The probe self-schedules here rather than at init, for the reason
    // array_internal.h's "Probe scheduling" note gives: RtVideoPortInit() runs
    // immediately after ResolveInit()'s LoadLibrary, when the DLL's
    // MemoryAllocator is CONSTRUCTED BUT NOT STARTED, and the probe allocates
    // ~40 blocks. Bind is a provably safe moment — it is called once per
    // render target bind, long after Engine::Startup, and the DX11 backend is
    // already up because it is the caller.
    ProbeGate();

    void*  pd  = self->platformData;
    void** vt  = *reinterpret_cast<void***>(pd);
    reinterpret_cast<void (*)(void*, void*)>(vt[2])(pd, context);
    ReportLine(&g_siteRbBind, "dispatched RenderBufferData slot 2");
}

// ============================================================================
// ?Recreate@ImageBuffer@Video@@QEAAXXZ — 0x1800f33f0, ordinal 3520
// ============================================================================
//
// Device-lost / resize recovery for one ImageBuffer. Called from
// runtime/src/save/saveload_update.cpp:279.
//
//     dd = Video::Device::_singleton->deviceData        ; +0x100
//     pd = dd ? dd->vtable[10](dd, this) : nullptr      ; CALL [RAX+0x50]
//     this->platformData = pd                           ; +0x60
//     f = this->flags                                   ; +0x34, read BEFORE
//     if (f & 0x10) tail  pd->vtable[1](pd)
//     else if (f & 0x20) tail  pd->vtable[2](pd)
//     else               tail  pd->vtable[3](pd, 0)
//
// Note the two tests are `SHR ECX,4 / TEST CL,1` and `SHR EAX,5 / TEST AL,1` —
// bits 4 and 5, which ImageBuffer.h already documents as "device-reset-tracked
// (set by DX11 color/depth attach)". They are read from the SAME loaded value,
// so a flags write by the vtable[10] call could not affect the second test;
// reproduced by loading `flags` once.
//
// FAITHFUL UNGUARDED DEREF. When `dd` is null, `pd` is null and retail
// dereferences it immediately. That is not a bug to guard
// (CLAUDE.md "Don't fix a faithful crash with a null check") — it is retail
// relying on Recreate never being called with no device, and a guard would skip
// the recreate rather than restore it.
void Video_ImageBuffer_Recreate(::ImageBuffer* self) {
    void** devSlot = DeviceSingletonSlot();
    uint8_t* dev = devSlot ? static_cast<uint8_t*>(*devSlot) : nullptr;
    void* dd = dev ? *reinterpret_cast<void**>(dev + offsetof(Device, deviceData)) : nullptr;

    void* pd = nullptr;
    if (dd) {
        void** ddvt = *reinterpret_cast<void***>(dd);
        pd = reinterpret_cast<void* (*)(void*, void*)>(ddvt[10])(dd, self);
    }
    self->platformData = pd;

    const uint32_t f = self->flags;
    void** vt = *reinterpret_cast<void***>(pd);
    if ((f >> 4) & 1u) {
        reinterpret_cast<void (*)(void*)>(vt[1])(pd);
        ReportLine(&g_siteRecreate, "flags bit 4 set -> slot 1");
        return;
    }
    if ((f >> 5) & 1u) {
        reinterpret_cast<void (*)(void*)>(vt[2])(pd);
        ReportLine(&g_siteRecreate, "flags bit 5 set -> slot 2");
        return;
    }
    reinterpret_cast<void (*)(void*, void*)>(vt[3])(pd, nullptr);
    ReportLine(&g_siteRecreate, "flags bit 4/5 clear -> slot 3 with a null second argument");
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
//
// THE `if (slot)` IS OURS, NOT RETAIL'S. Retail writes through an absolute
// address that always exists; we write through RT_DATA_SLOT, which returns null
// when the name does not resolve. Guarding the SLOT is a guard on our
// resolution machinery, not on retail's semantics — and if it ever fires, the
// counter below says `slot=MISSING`, which is a defect report and not a silent
// success.
void Video_Configuration_Dtor(void* self) {
    (void)self;                        // retail never touches RCX
    void** slot = ConfigSingletonSlot();
    if (slot) *slot = nullptr;
    ReportLine(&g_siteConfigDtor, slot ? "singleton cleared" : "slot=MISSING");
}

// ============================================================================
// ??1Device@Video@@QEAA@XZ — 0x1800f7290, ordinal 569
// ============================================================================
//
// Six steps, in retail's order:
//
//   1. dd = this->deviceData (+0x100); if (dd) dd->vtable[0](dd, 1).
//      `MOV EDX,1` + slot 0 is MSVC's SCALAR DELETING DESTRUCTOR with the
//      delete flag set, so the companion is destroyed AND freed by that call.
//   2. this->deviceData = nullptr.
//   3. if (this->type == 0) `?_singleton@Device@Video@@` = nullptr.
//      `CMP dword [RBX],ECX / CMOVZ RAX,RCX` — a CMOV, so the singleton slot is
//      REWRITTEN EITHER WAY: with null when this is the type-0 device, with its
//      own current value when it is not. Reproduced as an unconditional store,
//      because a "cleaner" `if (type == 0) *slot = nullptr;` would skip a write
//      retail makes.
//   4. DeleteCriticalSection(&this->lock) — +0x60, via the _win twin.
//   5..6. The three arrays, in retail's order 0x40, 0x30, 0x20, each through
//      the helper the banner at the top of this file tabulates. NOTE the order
//      is NOT the offset order and is NOT reverse-declaration order; it is
//      transcribed, not tidied.
//
// WHAT IT DOES NOT TOUCH: `dllSlots` at +0xB0 (the three LOADED renderer plugin
// modules standalone/video_interface.cpp fills) is never read by this body — no
// FreeLibrary, no clear. Device.h's banner warns that the +0x40 array and the
// +0xB0 array are easy to conflate; the dtor settles it, it destroys the first
// and ignores the second.
void Video_Device_Dtor(Device* self) {
    // ---- 1 & 2: the DeviceData companion -----------------------------------
    void* dd = self->deviceData;
    if (dd) {
        void** vt = *reinterpret_cast<void***>(dd);
        reinterpret_cast<void (*)(void*, int)>(vt[0])(dd, 1);   // scalar deleting dtor
    }
    self->deviceData = nullptr;

    // ---- 3: the singleton slot (CMOV, so always stored) --------------------
    void** slot = DeviceSingletonSlot();
    bool clearedSingleton = false;
    if (slot) {
        void* keep = *slot;
        if (self->type == 0) { keep = nullptr; clearedSingleton = true; }
        *slot = keep;
    }

    // ---- 4: the lock -------------------------------------------------------
    RtVideoDeleteDeviceLock(self);

    // ---- 5: Array<GraphicsInterface> @+0x40 — DESTROY, then free -----------
    rtcoll::ArrayHdr* ifaces = HdrAt(self, offsetof(Device, interfaceData));
    const int ifaceCount = ifaces->_size;
    DestroyInterfaceRange(self->interfaceData, ifaceCount);
    rtcoll::ArrayDtorImpl(ifaces);

    // ---- 5b: Array<bool> @+0x30 — FREE ONLY, bool has no destructor --------
    rtcoll::ArrayHdr* multi = HdrAt(self, offsetof(Device, multisampleData));
    const int multiCount = multi->_size;
    rtcoll::ArrayDtorImpl(multi);

    // ---- 6: Array<DisplayMode> @+0x20 — DESTROY, then free -----------------
    rtcoll::ArrayHdr* modes = HdrAt(self, offsetof(Device, modeData));
    const int modeCount = modes->_size;
    rtcoll::FreeInnerArraysCore(reinterpret_cast<uint8_t*>(self->modeData),
                                modeCount, &kV_ModeRefresh);
    rtcoll::ArrayDtorImpl(modes);

    if (RtDiffEnabled()) {
        char line[192];
        std::snprintf(line, sizeof(line),
                      "type=%d singleton=%s companion=%s ifaces=%d(destroyed) "
                      "multisample=%d(freed-only) modes=%d(destroyed)",
                      self->type, clearedSingleton ? "cleared" : "kept",
                      dd ? "deleted" : "none", ifaceCount, multiCount, modeCount);
        ReportLine(&g_siteDeviceDtor, line);
    }
}

// ============================================================================
// Registration — BY NAME AND BY ADDRESS
// ============================================================================
//
// Both doors, for every row. 13 live sites in this tree reach Runtime symbols
// through `ResolveAddr(Module::Runtime, …)`, which is registry-FIRST — that
// call is not "the DLL" (reference_resolveaddr_consults_port_registry_first.md)
// — and `ResolveExport` falls through to the raw impl in silence for a symbol
// nobody registered.
int RtVideoResourcesRegister() {
    int n = 0;

    RegisterRuntimeExport("??0RenderBuffer@Video@@QEAA@XZ", (void*)&Video_RenderBuffer_Ctor);
    RegisterRuntimeAddr(0x1800f3ee0, (void*)&Video_RenderBuffer_Ctor);
    n++;

    RegisterRuntimeExport("??1RenderBuffer@Video@@QEAA@XZ", (void*)&Video_RenderBuffer_Dtor);
    RegisterRuntimeAddr(0x1800f4020, (void*)&Video_RenderBuffer_Dtor);
    n++;

    RegisterRuntimeExport("?Bind@RenderBuffer@Video@@QEBAXAEAVContext@2@@Z",
                          (void*)&Video_RenderBuffer_Bind);
    RegisterRuntimeAddr(0x1800f4300, (void*)&Video_RenderBuffer_Bind);
    n++;

    RegisterRuntimeExport("?Recreate@ImageBuffer@Video@@QEAAXXZ",
                          (void*)&Video_ImageBuffer_Recreate);
    RegisterRuntimeAddr(0x1800f33f0, (void*)&Video_ImageBuffer_Recreate);
    n++;

    RegisterRuntimeExport("??1Configuration@Video@@QEAA@XZ", (void*)&Video_Configuration_Dtor);
    RegisterRuntimeAddr(0x1800f6f80, (void*)&Video_Configuration_Dtor);
    n++;

    RegisterRuntimeExport("??1Device@Video@@QEAA@XZ", (void*)&Video_Device_Dtor);
    RegisterRuntimeAddr(0x1800f7290, (void*)&Video_Device_Dtor);
    n++;

    // Wave 26 lane INT33 — 0x180010f00, the per-GraphicsInterface element
    // destroy. ADDRESS ONLY: it has no exported name in either shipped build
    // (checked against the export directory, not asserted from the `internal`
    // bucket's label). See the comment above RtVideo_ArrayInterfaceElemDestroy.
    RegisterRuntimeAddr(0x180010f00, (void*)&RtVideo_ArrayInterfaceElemDestroy);
    n++;

    printf("[RtPort] Video resource lifecycle (Wave 18 lane VRES): %d exports registered\n", n);
    return n;
}
