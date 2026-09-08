// ============================================================================
// runtime/src/video/buffers.cpp — Wave 6 lane C — Video::ImageBuffer +
// Video::ImageBufferData + Video::IndexBuffer + Video::VertexBuffer
// ============================================================================
//
// Scope: the 22 functions in docs/runtime_port/wave6_lanes.txt under
// "=== LANE C ===". Every mangled name below was checked against the real
// export table of C:\GOG Games\Banished\Runtime-x64.dll with
// tools/pe_exports.py BEFORE this file was written (all 22 present — see the
// per-function table in docs/runtime_port/WAVE6_LANE_C.md).
//
// SHAPE OF THIS LANE, IN ONE PARAGRAPH: `IndexBuffer`/`VertexBuffer` carry no
// vtable of their own — `Create`/`Set` are two-instruction tail-call
// trampolines into a platform buffer object living at `this+0x08`
// (dx11::IndexBufferData / VertexBufferData — dx11/types/VideoDX11Data.h,
// NOT this lane's file). `ImageBuffer::CopyToTexture`/`CopyFromTexture`/
// `Create` are the same shape, dispatching through `this+0x60`
// (dx11::ImageBufferData, dx11/src/imagebufferdata.cpp). Since VideoDX11 is
// ALREADY OURS (PLAN.md's "Why this wave is different" note), every one of
// those trampolines reaches a vtable this project already owns on both sides
// of the port — see the per-function oracle notes below for what that means
// for diffing.
//
// The remaining functions are the true leaves: nine pure accessors with a
// full differential oracle, three constructors, two destructors, and one
// held-back function (SaveImageToFile — see its own comment).

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"   // RtDiffSite/RtDiffReport for the [branch:] counters
#include "../../types/video/ImageBuffer.h"
#include "../../types/video/Buffers.h"
#include "../../types/video/Device.h"   // Wave 9 lane W: the lock and +0x100

#include <cstdio>
#include <cstddef>
#include <cstdint>

// ============================================================================
// Resolving the DLL's own helpers this lane's bodies call into but does not
// reimplement (out of scope — see each call site). RT_ORIGINAL_ONCE resolves
// once per call site and caches (rt_port.h); these are never on a hot path
// except the buffer/image trampolines, which is exactly why they are cached
// rather than resolved per call.
// ============================================================================

// runtime/src/video/buffers_win.cpp — see the note at their use site below.
void RtVideoDeviceLock(void** deviceSingleton);
void RtVideoDeviceUnlock(void** deviceSingleton);

namespace {

using PlatformConstructFn = void (*)(void*);
using PlatformDestructFn  = void (*)(void*);
using DtorFn              = void (*)(void*);
using MemFreeFn           = void (*)(void*);

// Video::ImageBuffer::PlatformConstruct/PlatformDestruct (0x1800f2f40 /
// 0x1800f3020) — allocate/release the dx11::ImageBufferData at +0x60 through
// Video::Device's factory. Not in this lane's 22-function scope (that factory
// call belongs to Video::Device, lane B), so the retail implementation is
// called through rather than duplicated.
PlatformConstructFn ImageBufferPlatformConstruct() {
    return RT_ORIGINAL_ONCE(PlatformConstructFn, "?PlatformConstruct@ImageBuffer@Video@@AEAAXXZ");
}
PlatformDestructFn ImageBufferPlatformDestruct() {
    return RT_ORIGINAL_ONCE(PlatformDestructFn, "?PlatformDestruct@ImageBuffer@Video@@AEAAXXZ");
}
PlatformConstructFn IndexBufferPlatformConstruct() {
    return RT_ORIGINAL_ONCE(PlatformConstructFn, "?PlatformConstruct@IndexBuffer@Video@@QEAAXXZ");
}
PlatformDestructFn IndexBufferPlatformDestruct() {
    return RT_ORIGINAL_ONCE(PlatformDestructFn, "?PlatformDestruct@IndexBuffer@Video@@QEAAXXZ");
}
PlatformConstructFn VertexBufferPlatformConstruct() {
    return RT_ORIGINAL_ONCE(PlatformConstructFn, "?PlatformConstruct@VertexBuffer@Video@@QEAAXXZ");
}
PlatformDestructFn VertexBufferPlatformDestruct() {
    return RT_ORIGINAL_ONCE(PlatformDestructFn, "?PlatformDestruct@VertexBuffer@Video@@QEAAXXZ");
}

// Object::External::~External — the base class ImageBuffer's dtor tail-calls
// after this lane's own body finishes. Wave 3 (Object) territory, not this
// lane's; called through.
DtorFn ObjectExternalDtor() {
    // RT_EXPORT_ONCE (Wave 9 lane R): ~External is ported (object/external.cpp:680)
    // and this is a dependency call, not an oracle.
    return RT_EXPORT_ONCE(DtorFn, "??1External@Object@@UEAA@XZ");
}

MemFreeFn MemFree() {
    return RT_ORIGINAL_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
}

// This class's own vtable, restored at dtor entry (the standard MSVC "run the
// dtor body under this class's own vtable, not a derived override" idiom —
// reference_phantom_dtor_mangling_qeaa_vs_ueaa.md is the same family of bug
// this guards against getting backwards).
void* ImageBufferVtable() {
    static void* v = ResolveExport("??_7ImageBuffer@Video@@6B@");
    return v;
}
void* ImageBufferDataVtable() {
    static void* v = ResolveExport("??_7ImageBufferData@Video@@6B@");
    return v;
}

// Video::Device::_singleton / Video::Configuration::_singleton — data
// exports, resolved once and cached exactly like a function pointer. Both are
// read verbatim by Width()/Height(); lane B owns Video::Device's own body,
// not these two field reads.
void** DeviceSingletonPtr() {
    void** p = RT_DATA_SLOT(void**,
        "?_singleton@Device@Video@@0PEAV12@EA");
    return p;
}
void** ConfigurationSingletonPtr() {
    void** p = RT_DATA_SLOT(void**,
        "?_singleton@Configuration@Video@@0PEAV12@EA");
    return p;
}

// ----------------------------------------------------------------------------
// Wave 9 lane W — the three class-static list heads Platform{Construct,Destruct}
// maintain, and the Device lock they take. See the block comment above
// Video_ImageBuffer_PlatformConstruct.
// ----------------------------------------------------------------------------

// PORT-AWARE (ResolveExport), and it must be. These are exported DATA in the
// shipped DLL; while it is loaded and the mint is disarmed this resolves to the
// DLL's own storage, so our linkage and the DLL's still-unported
// ImageBuffer::RecreateDynamic / ReleaseDynamic (0x1800f2d90 / 0x1800f2e80,
// which walk the same list) agree on ONE list. At cutover the mint owns the
// bytes and the walkers are ours. ResolveExportRaw would be null at cutover and
// every link write would land at offset 0 of nothing.
void** ImageBufferFirstSurface() {
    static void** p = reinterpret_cast<void**>(
        ResolveExport("?_firstSurface@ImageBuffer@Video@@0PEAV12@EA"));
    return p;
}
void** IndexBufferFirstBuffer() {
    static void** p = reinterpret_cast<void**>(
        ResolveExport("?_firstBuffer@IndexBuffer@Video@@0PEAV12@EA"));
    return p;
}
void** VertexBufferFirstBuffer() {
    static void** p = reinterpret_cast<void**>(
        ResolveExport("?_firstBuffer@VertexBuffer@Video@@0PEAV12@EA"));
    return p;
}

// Video::Device's recursive render lock, in runtime/src/video/buffers_win.cpp
// (PLAN §4.1 decision 1: no <windows.h> in this file). It takes the ADDRESS of
// the singleton slot, not a loaded Device*, because retail's unlock RE-READS
// the singleton — see that file's header for why that is reproduced.
//
// Declared at NAMESPACE SCOPE, not inside the anonymous namespace above: an
// anonymous-namespace declaration gives internal linkage, which turns a
// cross-TU call into an undefined symbol at link time.

// The three offsets buffers_win.cpp hardcodes, bound to the struct HERE so a
// Device.h layout change breaks the build instead of silently corrupting the
// lock. buffers_win.cpp cannot include Device.h itself (see its header).
static_assert(offsetof(RtVideo::Device, lock) == 0x60, "buffers_win.cpp kDeviceLockOffset");
static_assert(offsetof(RtVideo::Device, lockOwnerThreadId) == 0x88, "buffers_win.cpp kDeviceOwnerOffset");
static_assert(offsetof(RtVideo::Device, lockRecursionCount) == 0x8C, "buffers_win.cpp kDeviceDepthOffset");

// The intrusive circular doubly-linked list every Platform{Construct,Destruct}
// pair maintains, at the two link offsets that class uses (+0x50/+0x58 for
// ImageBuffer, +0x10/+0x18 for Index/VertexBuffer). `next` is the field written
// with the list head; `prev` is the one written with the head's own prev.
//
// TRANSCRIBED, not reasoned about: all four decompiles agree instruction for
// instruction, including the two oddities. In Link, the last write is spelled
// `(*(this->next))->prev = this` rather than `head->prev = this` — the same
// address by construction, kept because retail reloads `this->next` to get it.
// In Unlink, the `if (*first == self) *first = nullptr` AFTER `*first` was
// already advanced to `self->next` is not dead: for a one-element list
// `self->next == self`, so the advance puts `self` back and this clears it.
struct ListLinks { size_t next; size_t prev; };

inline void** LinkField(void* self, size_t off) {
    return reinterpret_cast<void**>(static_cast<uint8_t*>(self) + off);
}

// Returns the branch taken, so the caller can COUNT it. `calls=` on the port
// itself only says the body was entered; a green summary over six calls that
// all took the same branch leaves the other one unexercised, which is the
// SetValueInt trap (12,896 calls, ten of which did any work). See the
// [branch:...] sites in the RtDiff summary.
enum class LinkBranch { FirstInList, Appended };

LinkBranch ListLink(void* self, void** first, ListLinks L) {
    if (*first == nullptr) {
        *first = self;
        *LinkField(self, L.next) = self;
        *LinkField(self, L.prev) = self;
        return LinkBranch::FirstInList;
    } else {
        void* tail = *LinkField(*first, L.prev);
        *LinkField(self, L.prev) = tail;
        *LinkField(self, L.next) = *first;
        *LinkField(tail, L.next) = self;
        *LinkField(*LinkField(self, L.next), L.prev) = self;
        return LinkBranch::Appended;
    }
}

// NotLinked is a genuine EARLY-OUT: both bodies do nothing at all, so the
// comparison that follows is vacuous and a call that took it must not be
// counted as a call that verified anything.
enum class UnlinkBranch { NotLinked, Unlinked, ListEmptied };

UnlinkBranch ListUnlink(void* self, void** first, ListLinks L) {
    void* prev = *LinkField(self, L.prev);
    void* next = *LinkField(self, L.next);
    if (prev == nullptr || next == nullptr)
        return UnlinkBranch::NotLinked;
    if (*first == self)
        *first = next;
    *LinkField(prev, L.next) = next;
    *LinkField(next, L.prev) = prev;
    *LinkField(self, L.prev) = nullptr;
    *LinkField(self, L.next) = nullptr;
    if (*first == self) {
        *first = nullptr;
        return UnlinkBranch::ListEmptied;
    }
    return UnlinkBranch::Unlinked;
}

// Count one branch as a zero-divergence "call" on its own RtDiffSite, so it
// appears in the ordinary summary next to the port it belongs to. Using the
// existing machinery rather than a private counter means the number is printed
// by the same code that prints every other number in that report.
// REGISTER UNCONDITIONALLY, then report. The first version of this was
//     if (taken) RtDiffReport(...); else RtDiffRegister(...);
// on the theory that the report would register the site for us. `RtDiffReport`
// does NOT register (rt_diff.cpp:82 — it only bumps `calls`), so an
// ALWAYS-taken branch never entered the site list and never printed, while a
// NEVER-taken one printed `calls=0`. The output was exactly inverted: the
// branches that did all the work were the invisible ones. Recorded rather than
// silently fixed, because it is the probe-defect shape
// reference_probe_needs_an_effectiveness_control.md warns about — a
// coverage instrument that is itself wrong reads as coverage.
void CountBranch(RtDiffSite* site, bool taken) {
    RtDiffRegister(site);
    if (taken) RtDiffReport(site, true, "taken", "taken");
}

constexpr ListLinks kImageLinks  = { 0x50, 0x58 };
constexpr ListLinks kBufferLinks = { 0x10, 0x18 };

// The five words a Link or an Unlink can write, captured so the diff oracle can
// restore the pre-state and replay the retail body over it. Everything outside
// this set is untouched by both bodies, and comparing bytes neither side writes
// is the trap in reference_oracle_must_not_compare_unwritten_bytes.md.
struct LinkState {
    void* first;
    void* selfNext;
    void* selfPrev;
    void* neighbourA;   // *LinkField(prevAtEntry, next)  — 0 when there is no neighbour
    void* neighbourB;   // *LinkField(headAtEntry, prev)
    void* prevAtEntry;
    void* headAtEntry;
};

LinkState CaptureLinks(void* self, void** first, ListLinks L) {
    LinkState s{};
    s.first       = *first;
    s.selfNext    = *LinkField(self, L.next);
    s.selfPrev    = *LinkField(self, L.prev);
    s.headAtEntry = *first;
    s.prevAtEntry = (s.headAtEntry != nullptr) ? *LinkField(s.headAtEntry, L.prev) : nullptr;
    s.neighbourA  = (s.prevAtEntry != nullptr) ? *LinkField(s.prevAtEntry, L.next) : nullptr;
    s.neighbourB  = (s.headAtEntry != nullptr) ? *LinkField(s.headAtEntry, L.prev) : nullptr;
    return s;
}

// Re-read the same five words the capture named, using the ENTRY-TIME
// neighbours (not the current ones), so ours and theirs are read from the same
// addresses even though the body moved the head.
LinkState ReadLinksAt(void* self, void** first, ListLinks L, const LinkState& at) {
    LinkState s = at;
    s.first    = *first;
    s.selfNext = *LinkField(self, L.next);
    s.selfPrev = *LinkField(self, L.prev);
    s.neighbourA = (at.prevAtEntry != nullptr) ? *LinkField(at.prevAtEntry, L.next) : nullptr;
    s.neighbourB = (at.headAtEntry != nullptr) ? *LinkField(at.headAtEntry, L.prev) : nullptr;
    return s;
}

void RestoreLinks(void* self, void** first, ListLinks L, const LinkState& s) {
    *first = s.first;
    *LinkField(self, L.next) = s.selfNext;
    *LinkField(self, L.prev) = s.selfPrev;
    if (s.prevAtEntry != nullptr) *LinkField(s.prevAtEntry, L.next) = s.neighbourA;
    if (s.headAtEntry != nullptr) *LinkField(s.headAtEntry, L.prev) = s.neighbourB;
}

bool LinksEqual(const LinkState& a, const LinkState& b) {
    return a.first == b.first && a.selfNext == b.selfNext && a.selfPrev == b.selfPrev &&
           a.neighbourA == b.neighbourA && a.neighbourB == b.neighbourB;
}

// The companion object's slot 0 is its VECTOR DELETING DESTRUCTOR: retail's
// PlatformDestruct calls `(**pc)(pc, 1)`. The oracle uses the same call to
// dispose of the companion OUR body created before letting retail create its
// own — see reference_phantom_dtor_mangling_qeaa_vs_ueaa.md for why slot 0 is
// ??_E and not ??_G.
void ReleaseCompanion(void* companion) {
    if (companion == nullptr)
        return;
    using DeletingDtorFn = void (*)(void*, int);
    DeletingDtorFn slot0 = *reinterpret_cast<DeletingDtorFn*>(*reinterpret_cast<void**>(companion));
    slot0(companion, 1);
}

// The Device-side factory call at the head of every PlatformConstruct:
// `dd = Device::_singleton->deviceData; companion = dd ? dd->vt[SLOT](dd, this) : nullptr`.
// SLOT is 0x50 for ImageBuffer, 0x60 for IndexBuffer, 0x68 for VertexBuffer —
// the three companion factories on dx11::DeviceData, which is OUR code
// (dx11/src/devicedata.cpp), so this dispatch lands in the project on both
// sides of the port.
void* MakeCompanion(void* self, size_t vtSlot) {
    RtVideo::Device* dev = static_cast<RtVideo::Device*>(*DeviceSingletonPtr());
    void* dd = dev->deviceData;
    if (dd == nullptr)
        return nullptr;
    using FactoryFn = void* (*)(void*, void*);
    FactoryFn make = *reinterpret_cast<FactoryFn*>(
        static_cast<uint8_t*>(*reinterpret_cast<void**>(dd)) + vtSlot);
    return make(dd, self);
}

}  // namespace

// ----------------------------------------------------------------------------
// Wave 9 lane W ports, defined further down. Forward-declared so the three
// ctors and ~ImageBuffer above reach OUR bodies rather than the DLL's: they
// used to call ImageBufferPlatformConstruct() &co, which is ResolveExportRaw
// and therefore a call through null the moment Runtime-x64.dll is gone. Those
// four resolvers survive as the diff oracle's door and nothing else.
// ----------------------------------------------------------------------------
void Video_ImageBuffer_PlatformConstruct(ImageBuffer* self);
void Video_ImageBuffer_PlatformDestruct(ImageBuffer* self);
void Video_IndexBuffer_PlatformConstruct(IndexBuffer* self);
void Video_IndexBuffer_PlatformDestruct(IndexBuffer* self);
void Video_VertexBuffer_PlatformConstruct(VertexBuffer* self);
void Video_VertexBuffer_PlatformDestruct(VertexBuffer* self);

// ============================================================================
// Video::ImageBuffer::ImageBuffer() — 0x1800f2580, ordinal 226
//
// Field values are fixed constants (no inputs), so there is nothing to run
// the original ctor against and compare — see WAVE6_LANE_C.md for why this
// lane does not call the real ctor for diffing (PlatformConstruct's factory
// call links into Video::Device's live buffer/tracking state; invoking it a
// second time on a throwaway scratch object would corrupt shared state, not
// verify anything). RT_DIFF_SHADOW instead asserts the constants we just
// wrote are exactly what retail writes.
// ============================================================================
void Video_ImageBuffer_Ctor(ImageBuffer* self) {
    RT_DIFF_BEGIN("Video::ImageBuffer::ImageBuffer");

    self->vtable = ImageBufferVtable();       // Base->External->CompiledExternal->ImageBuffer chain,
                                               // collapsed: only the final vtable is observable.
    self->externalData08 = 0;
    self->externalData10 = 0;
    self->externalData18 = 0;

    self->width = 0;
    self->height = 0;
    self->dimension = 0;
    self->kind = 2;         // default/none
    self->format = 0;
    self->flags = 0;
    self->addressU = 0;
    self->addressV = 0;
    self->addressW = 0;
    self->filter = 1;       // LINEAR
    self->pixelData = nullptr;
    // field50/field58: not written by the ctor (undefined scope for this lane).

    Video_ImageBuffer_PlatformConstruct(self);

    RT_DIFF_SHADOW(self->width == 0 && self->height == 0 && self->dimension == 0 &&
                       self->kind == 2 && self->format == 0 && self->flags == 0 &&
                       self->filter == 1 && self->pixelData == nullptr,
                   "ctor fields set", "retail constants (0,0,0,2,0,0,1,null)");
}

// ============================================================================
// Video::ImageBuffer::~ImageBuffer() — 0x1800f2610, ordinal 608
//
// POST-STATE only: a second destructor run on the same object (or a scratch
// copy) would double-decrement the refcount at pixelData-4 and potentially
// double-free — there is no safe way to run "theirs" without mutating shared
// allocator state. Reproduced field-for-field from the decompile.
// ============================================================================
void Video_ImageBuffer_Dtor(ImageBuffer* self) {
    self->vtable = ImageBufferVtable();
    Video_ImageBuffer_PlatformDestruct(self);

    if (self->pixelData != nullptr) {
        int32_t* refcount = reinterpret_cast<int32_t*>(static_cast<uint8_t*>(self->pixelData) - 4);
        *refcount -= 1;
        if (*refcount == 0) {
            void* block = static_cast<uint8_t*>(self->pixelData) - 8;
            self->pixelData = block;
            MemFree()(self->pixelData);
        } else {
            self->pixelData = nullptr;
        }
    }

    // CompiledExternal has no dtor of its own (no export named
    // ~CompiledExternal takes a body beyond the vtable stamp retail performs
    // here); the observable effect is entirely the vtable write plus the
    // External base dtor below.
    static void* compiledExternalVtable = ResolveExport("??_7CompiledExternal@Object@@6B@");
    self->vtable = compiledExternalVtable;
    ObjectExternalDtor()(self);
}

// ============================================================================
// Video::ImageBuffer accessors — pure reads, FULL oracle (call both, compare).
// ============================================================================

int Video_ImageBuffer_Width(ImageBuffer* self) {
    RT_DIFF_BEGIN("Video::ImageBuffer::Width");
    void** device = DeviceSingletonPtr();
    void** config = ConfigurationSingletonPtr();
    int ours;
    if ((self->flags >> 8) & 1) {
        ours = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(*device) + 4);
    } else if ((self->flags >> 9) & 1) {
        ours = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(*device) + 4) / 2;
    } else {
        ours = self->width;
        if ((self->flags >> 6) & 1) {
            uint32_t shift = *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(*config) + 0x2c) & 0x1f;
            ours >>= shift;
        }
    }
    using Fn = int (*)(void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?Width@ImageBuffer@Video@@QEBAHXZ")(self));
    return ours;
}

int Video_ImageBuffer_Height(ImageBuffer* self) {
    RT_DIFF_BEGIN("Video::ImageBuffer::Height");
    void** device = DeviceSingletonPtr();
    void** config = ConfigurationSingletonPtr();
    int ours;
    if ((self->flags >> 8) & 1) {
        ours = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(*device) + 8);
    } else if ((self->flags >> 9) & 1) {
        ours = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(*device) + 8) / 2;
    } else {
        ours = self->height;
        if ((self->flags >> 6) & 1) {
            uint32_t shift = *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(*config) + 0x2c) & 0x1f;
            ours >>= shift;
        }
    }
    using Fn = int (*)(void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?Height@ImageBuffer@Video@@QEBAHXZ")(self));
    return ours;
}

int Video_ImageBuffer_GetLevelCount(ImageBuffer* self) {
    RT_DIFF_BEGIN("Video::ImageBuffer::GetLevelCount");
    int ours = self->GetLevelCount();
    using Fn = int (*)(void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?GetLevelCount@ImageBuffer@Video@@QEBAHXZ")(self));
    return ours;
}

bool Video_ImageBuffer_IsCompressed(ImageBuffer* self) {
    RT_DIFF_BEGIN("Video::ImageBuffer::IsCompressed");
    bool ours = self->IsCompressed();
    using Fn = bool (*)(void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?IsCompressed@ImageBuffer@Video@@QEBA_NXZ")(self));
    return ours;
}

bool Video_ImageBuffer_IsDepthBuffer(ImageBuffer* self) {
    RT_DIFF_BEGIN("Video::ImageBuffer::IsDepthBuffer");
    bool ours = self->IsDepthBuffer();
    using Fn = bool (*)(void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?IsDepthBuffer@ImageBuffer@Video@@QEBA_NXZ")(self));
    return ours;
}

bool Video_ImageBuffer_IsDynamic(ImageBuffer* self) {
    RT_DIFF_BEGIN("Video::ImageBuffer::IsDynamic");
    bool ours = self->IsDynamic();
    using Fn = bool (*)(void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?IsDynamic@ImageBuffer@Video@@QEBA_NXZ")(self));
    return ours;
}

bool Video_ImageBuffer_IsGenerated(ImageBuffer* self) {
    RT_DIFF_BEGIN("Video::ImageBuffer::IsGenerated");
    bool ours = self->IsGenerated();
    using Fn = bool (*)(void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?IsGenerated@ImageBuffer@Video@@QEBA_NXZ")(self));
    return ours;
}

bool Video_ImageBuffer_IsRenderBuffer(ImageBuffer* self) {
    RT_DIFF_BEGIN("Video::ImageBuffer::IsRenderBuffer");
    bool ours = self->IsRenderBuffer();
    using Fn = bool (*)(void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?IsRenderBuffer@ImageBuffer@Video@@QEBA_NXZ")(self));
    return ours;
}

bool Video_ImageBuffer_IsRendered(ImageBuffer* self) {
    RT_DIFF_BEGIN("Video::ImageBuffer::IsRendered");
    bool ours = self->IsRendered();
    using Fn = bool (*)(void*);
    RT_DIFF_SCALAR(ours, RT_ORIGINAL_ONCE(Fn, "?IsRendered@ImageBuffer@Video@@QEBA_NXZ")(self));
    return ours;
}

// ============================================================================
// Video::ImageBuffer::CopyToTexture / CopyFromTexture — 0x1800f3380 (slot 5,
// Update/upload) / 0x1800f33a0 (slot 6, Read/download). Both are two-
// instruction tail-call trampolines in retail: `this = this->platformData;
// jmp [this->vtable + N]`. No new behaviour to invent, and no separate
// oracle is meaningful here: the vtable this dispatches into is
// dx11::ImageBufferData's, which is ALREADY OURS (dx11/src/imagebufferdata.cpp,
// slots 5 and 6). Calling "the DLL's original CopyToTexture" and "our
// CopyToTexture" reach the *same* dx11 vtable slot either way — there is no
// second implementation on the other side of this trampoline to diverge
// from. RT_DIFF_SHADOW registers the site (so it is visible in the census
// and the diff summary) without invoking the original a second time, which
// would upload/download the texture twice.
// ============================================================================
bool Video_ImageBuffer_CopyToTexture(ImageBuffer* self, void* data, int width, int height) {
    RT_DIFF_BEGIN("Video::ImageBuffer::CopyToTexture");
    using Fn = bool (*)(void*, void*, int, int);
    ImageBufferData* pd = static_cast<ImageBufferData*>(self->platformData);
    Fn slot = *reinterpret_cast<Fn*>(static_cast<uint8_t*>(pd->vtable) + 0x28);
    bool ours = slot(pd, data, width, height);
    RT_DIFF_SHADOW(true, "trampoline into dx11::ImageBufferData slot 5 (ours)",
                   "same dx11::ImageBufferData slot 5 (retail's own trampoline)");
    return ours;
}

bool Video_ImageBuffer_CopyFromTexture(ImageBuffer* self, void* data, int width, int height) {
    RT_DIFF_BEGIN("Video::ImageBuffer::CopyFromTexture");
    using Fn = bool (*)(void*, void*, int, int);
    ImageBufferData* pd = static_cast<ImageBufferData*>(self->platformData);
    Fn slot = *reinterpret_cast<Fn*>(static_cast<uint8_t*>(pd->vtable) + 0x30);
    bool ours = slot(pd, data, width, height);
    RT_DIFF_SHADOW(true, "trampoline into dx11::ImageBufferData slot 6 (ours)",
                   "same dx11::ImageBufferData slot 6 (retail's own trampoline)");
    return ours;
}

// ============================================================================
// Video::ImageBuffer::Create — 0x1800f2680, ordinal 1877. Stamps the ten
// descriptor fields from its parameters (an ordinary field-store, in scope
// and fully reproduced), then a genuine two-argument call — not a raw
// register-preserving trampoline like the pair above — into
// dx11::ImageBufferData's slot 3 (ImageBufferData_Create in
// dx11/src/imagebufferdata.cpp). Same "already our own vtable on the far
// side" situation as CopyToTexture/CopyFromTexture, so RT_DIFF_SHADOW rather
// than a second live call.
//
// Enum parameters (ImageType/Usage/Format/FilterMode/AddressMode) are plain
// `enum` in the retail signature, i.e. 4-byte int underlying type in the
// MSVC ABI with no special calling-convention handling — passed here as
// int32_t so this file does not need to invent enumerator names Runtime
// never exports.
// ============================================================================
struct ImageLevelArray { void* data; int32_t count; int32_t capacity; };

bool Video_ImageBuffer_Create(ImageBuffer* self, int32_t imageType, int32_t usage, int32_t format,
                               uint32_t flags, int width, int height, int32_t filter,
                               int32_t addressU, int32_t addressV, int32_t addressW,
                               ImageLevelArray* levels) {
    RT_DIFF_BEGIN("Video::ImageBuffer::Create");
    self->dimension = imageType;
    self->width = width;
    self->kind = usage;
    self->height = height;
    self->format = format;
    self->flags = flags;
    self->filter = filter;
    self->addressU = addressU;
    self->addressV = addressV;
    self->addressW = addressW;

    using Fn = bool (*)(void*, ImageLevelArray*);
    ImageBufferData* pd = static_cast<ImageBufferData*>(self->platformData);
    Fn slot = *reinterpret_cast<Fn*>(static_cast<uint8_t*>(pd->vtable) + 0x18);
    bool ours = slot(pd, levels);
    RT_DIFF_SHADOW(true, "fields stamped + dx11::ImageBufferData slot 3 (ours)",
                   "same fields + same dx11::ImageBufferData slot 3 (retail's own dispatch)");
    return ours;
}

// ============================================================================
// Video::ImageBufferData::~ImageBufferData() — 0x1800f8e10, ordinal 609.
// This is Runtime's OWN abstract base (runtime/types/video/ImageBuffer.h),
// distinct from dx11::ImageBufferData. Trivial: reset the vptr to this base
// class's own vtable. dx11's derived dtor (ImageBufferData_ScalarDeletingDtor,
// dx11/src/imagebufferdata.cpp) tail-calls this exact export as its
// `im->BaseDtor(self)` — porting it here is what makes that tail call land on
// us instead of the DLL, with no change to dx11's own file.
// ============================================================================
void Video_ImageBufferData_Dtor(ImageBufferData* self) {
    self->vtable = ImageBufferDataVtable();
}

// ============================================================================
// Wave 9 lane W — Video::{Image,Index,Vertex}Buffer::Platform{Construct,Destruct}
//
//   0x1800f2f40  ?PlatformConstruct@ImageBuffer@Video@@AEAAXXZ    ord 3337
//   0x1800f3020  ?PlatformDestruct@ImageBuffer@Video@@AEAAXXZ     ord 3365
//   0x1800f3680  ?PlatformConstruct@IndexBuffer@Video@@QEAAXXZ    ord 3338
//   0x1800f3760  ?PlatformDestruct@IndexBuffer@Video@@QEAAXXZ     ord 3366
//   0x1800f57e0  ?PlatformConstruct@VertexBuffer@Video@@QEAAXXZ   ord 3345
//   0x1800f58c0  ?PlatformDestruct@VertexBuffer@Video@@QEAAXXZ    ord 3373
//
// WHY THEY ARE HERE NOW. Wave 6 lane C left these "out of scope" and called the
// DLL through RT_ORIGINAL_ONCE from the three ctors and ~ImageBuffer above.
// Wave 9 lane V's residue census reads four of the six as REAL DEPENDENCY on
// the RAW door: at cutover `ResolveExportRaw` is `GetProcAddress` on a module
// that is not there, so every one of those becomes a call through null on the
// first buffer the renderer builds. (The Index/Vertex PlatformDestruct pair is
// not in lane V's 25 for the only reason that its resolver is DEFINED and never
// CALLED — the two dead helpers at the top of this file. They are ported here
// anyway, because the moment ~IndexBuffer or ~VertexBuffer is ported they stop
// being dead and become two more rows.)
//
// ALL SIX ARE THE SAME FUNCTION with three parameters — the companion factory
// slot on Video::Device's DeviceData vtable, the two link offsets, and which
// class-static list head. Construct: make the dx11 companion, store it, take
// Device's lock, splice `this` into the class's circular list, drop the lock.
// Destruct: release the companion, take the lock, unsplice, drop the lock.
//
// THE COMPANION FACTORY IS ALREADY OURS. Device+0x100 is dx11::DeviceData
// (dx11/src/devicedata.cpp) and slots 0x50/0x60/0x68 are its Image/Index/Vertex
// companion factories, so that virtual call lands in this project on both sides
// of the port and there is nothing on the far side to diverge from.
//
// ---------------------------------------------------------------------------
// THE ORACLE: restore-and-replay, on the LINKAGE only.
// ---------------------------------------------------------------------------
// A plain "call ours, call theirs, compare" is wrong twice over here: theirs
// would splice the same object into the list a second time (a self-loop, and a
// corrupt list for every later walker) and would create a second companion
// object nothing frees. So the diff path does this instead:
//
//   1. capture the five words a Link/Unlink can write, at the ENTRY-TIME
//      addresses (the head, `this`'s two links, and the two neighbour slots);
//   2. run OURS in full — the real body, not a prediction;
//   3. read those same five words back;
//   4. rewind: give the companion we just made back to dx11 through its own
//      slot-0 deleting destructor, restore `this`'s companion field, and write
//      the five words back to their entry values;
//   5. run THEIRS in full over the restored state;
//   6. read the five words again and compare against step 3.
//
// The final state is retail's, by construction, which is the safe way round: if
// our linkage is wrong, the run continues on the DLL's list and the divergence
// is a log line rather than a crash three frames later.
//
// WHAT THIS ORACLE DOES **NOT** COVER, stated rather than buried. The companion
// create/release itself is not differenced — in the Construct path ours' object
// is destroyed before theirs runs, and in the Destruct path `this`'s companion
// field is nulled before theirs runs so that retail's own `if (pc != nullptr)`
// guard skips a second release of memory we already freed. Both are a single
// virtual dispatch to code this project owns (dx11::DeviceData's factory slot,
// and the companion's slot-0 vector deleting destructor), and both are exercised
// on the ordinary non-diff path where our body is the only one that runs. The
// LINKAGE — the part that is retail logic we transcribed, with the two branch
// oddities transcribed with it — is what is compared, on every call.
//
// A note on reading the result: RT_DIFF_SHADOW here reports on every call that
// reaches it, so `calls=` counts real work rather than entry
// (reference_probe_needs_an_effectiveness_control.md). A `calls=0` on these six
// means the renderer built no buffers, not that the port agreed with retail.
// ============================================================================

namespace {

// One body, three instantiations. `vtSlot == kNoCompanion` is not used; every
// caller passes a real slot.
LinkBranch PlatformConstructBody(void* self, size_t vtSlot, size_t companionOff,
                                 void** first, ListLinks L) {
    *LinkField(self, companionOff) = MakeCompanion(self, vtSlot);
    RtVideoDeviceLock(DeviceSingletonPtr());
    const LinkBranch br = ListLink(self, first, L);
    RtVideoDeviceUnlock(DeviceSingletonPtr());
    return br;
}

// `nullCompanionAfterRelease` is the ONE place the three classes differ:
// Index/VertexBuffer write `this+8 = 0` after releasing, ImageBuffer leaves the
// freed pointer at +0x60 in place. That asymmetry is in the shipped bytes of
// all three and is reproduced rather than tidied.
UnlinkBranch PlatformDestructBody(void* self, size_t companionOff,
                                  bool nullCompanionAfterRelease,
                                  void** first, ListLinks L) {
    void* companion = *LinkField(self, companionOff);
    if (companion != nullptr)
        ReleaseCompanion(companion);
    if (nullCompanionAfterRelease)
        *LinkField(self, companionOff) = nullptr;
    RtVideoDeviceLock(DeviceSingletonPtr());
    const UnlinkBranch br = ListUnlink(self, first, L);
    RtVideoDeviceUnlock(DeviceSingletonPtr());
    return br;
}

void DescribeLinks(char* buf, size_t n, const LinkState& s) {
    std::snprintf(buf, n, "first=%p next=%p prev=%p nA=%p nB=%p",
                  s.first, s.selfNext, s.selfPrev, s.neighbourA, s.neighbourB);
}

// ============================================================================
// THE PER-CLASS DESCRIPTOR — one literal, used by the port AND by the probe
// ============================================================================
//
// The six ports differ only in four values: the companion-factory slot on
// DeviceData's vtable, the offset the companion is stored at, the two link
// offsets, and whether the destructor nulls the companion field afterwards.
// They were originally spelled out at each of the six call sites.
//
// They are now a single `BufferClass` literal per class, because Wave 9 lane W's
// execution report left `Video::{Index,Vertex}Buffer::PlatformDestruct`
// UNVERIFIED with the note "the shared body is verified 10x through ImageBuffer
// and the link offsets 10x each through the construct siblings, but THE
// COMPOSITION is unproved" — the composition being exactly this argument tuple.
// A tuple that appears once, and that the empty-list probe below runs, cannot
// disagree with itself. That is what turns "unproved composition" into "proved
// by the probe" rather than leaving two ports registered and untested.
struct BufferClass {
    size_t      vtSlot;            // DeviceData vtable slot of the companion factory
    size_t      companionOff;      // where the companion pointer is stored
    bool        nullAfterRelease;  // Index/Vertex null it; ImageBuffer does not
    ListLinks   links;
    void**    (*head)();           // the exported class-static list head
    PlatformConstructFn (*theirCtor)();
    PlatformDestructFn  (*theirDtor)();
    RtDiffSite* siteLink;          // [PROBE:first-in-list]
    RtDiffSite* siteUnlink;        // [PROBE:list-emptied]
};

RtDiffSite g_probeImgLink   = { "Video::ImageBuffer::PlatformConstruct[PROBE:first-in-list]",  0, 0, false, false };
RtDiffSite g_probeImgUnlink = { "Video::ImageBuffer::PlatformDestruct[PROBE:list-emptied]",    0, 0, false, false };
RtDiffSite g_probeIdxLink   = { "Video::IndexBuffer::PlatformConstruct[PROBE:first-in-list]",  0, 0, false, false };
RtDiffSite g_probeIdxUnlink = { "Video::IndexBuffer::PlatformDestruct[PROBE:list-emptied, and the ONLY execution this port gets]", 0, 0, false, false };
RtDiffSite g_probeVtxLink   = { "Video::VertexBuffer::PlatformConstruct[PROBE:first-in-list]", 0, 0, false, false };
RtDiffSite g_probeVtxUnlink = { "Video::VertexBuffer::PlatformDestruct[PROBE:list-emptied, and the ONLY execution this port gets]", 0, 0, false, false };

const BufferClass kImageClass  = { 0x50, 0x60, false, kImageLinks,  &ImageBufferFirstSurface,
                                   &ImageBufferPlatformConstruct,  &ImageBufferPlatformDestruct,
                                   &g_probeImgLink, &g_probeImgUnlink };
const BufferClass kIndexClass  = { 0x60, 0x08, true,  kBufferLinks, &IndexBufferFirstBuffer,
                                   &IndexBufferPlatformConstruct,  &IndexBufferPlatformDestruct,
                                   &g_probeIdxLink, &g_probeIdxUnlink };
const BufferClass kVertexClass = { 0x68, 0x08, true,  kBufferLinks, &VertexBufferFirstBuffer,
                                   &VertexBufferPlatformConstruct, &VertexBufferPlatformDestruct,
                                   &g_probeVtxLink, &g_probeVtxUnlink };

// The descriptors' offsets are the structs', bound here so a header change
// breaks the build rather than silently mis-linking.
static_assert(offsetof(ImageBuffer, platformData)  == 0x60, "kImageClass.companionOff");
static_assert(offsetof(IndexBuffer, platformData)  == 0x08, "kIndexClass.companionOff");
static_assert(offsetof(VertexBuffer, platformData) == 0x08, "kVertexClass.companionOff");

// ============================================================================
// THE EMPTY-LIST PROBE — the branch cutover takes FIRST
// ============================================================================
//
// WHAT IT CLOSES. Lane W's execution report (WAVE9_LANE_W.md §9 finding 2)
// measured `ListLink`'s **first-in-list** branch and `ListUnlink`'s
// **list-emptied** branch at calls=0 in all four harness cells, for all three
// classes: 31 construct calls all took `appended`, 10 destruct calls all took
// `unlinked`. The cause is structural, not luck — no detour is installed on
// these six, so the DLL's own constructors never reach our ports, and by the
// time ours runs the shipped `Video::Device` startup has already put objects on
// those lists. **At cutover that inverts**: the mint owns the three heads, they
// start NULL, and every construct in the process is ours, so the FIRST buffer
// the renderer builds takes the branch that has never been compared against
// retail — on the first frame, with no retail body left to compare against.
//
// WHY A PRIVATE HEAD DOES NOT WORK, checked rather than assumed. Our side takes
// its head by pointer (`void** first`), so a private head would cost us nothing.
// The SHIPPED body does not: `?PlatformConstruct@ImageBuffer@Video@@` reads
// `_firstSurface` RIP-relative, hardcoded. A private head therefore gives
// ours-against-ours, which is not an oracle. The synthetic-subject lesson still
// applies and IS applied — the SUBJECT is synthetic, a scratch node, and no live
// object's fields are ever written — but the HEAD has to be the real one,
// briefly.
//
// WHY BORROWING THE REAL HEAD IS SAFE. This is the fact that made the probe
// buildable rather than impossible, and it was read out of the shipped bodies
// rather than assumed: every other reader of these lists inside the DLL takes
// the SAME Device lock, with the same recursion idiom, before touching the head:
//
//   ?RecreateDynamic@ImageBuffer@Video@@SAXXZ   0x1800f2d90
//   ?ReleaseDynamic@ImageBuffer@Video@@SAXXZ    0x1800f2e80
//   ... and the Index / Vertex / RenderBuffer twins at 0x1800f3500 / 0x1800f35d0,
//       0x1800f5660 / 0x1800f5730, 0x1800f4030 / 0x1800f40c0
//
// each opening with `EnterCriticalSection(Device::_singleton + 0x60)`. Holding
// that lock for the whole probe therefore excludes every DLL walker. Better:
// both walkers open with `if (_firstSurface != nullptr)` and skip their loop
// entirely, so a NULL head is a state the shipped code is already written for
// rather than one this probe invents.
//
// The window is bounded — head nulled and restored inside one lock acquisition,
// no live object touched, saved head put back before the lock is released.
//
// WHAT IS COMPARED, per class, on an EMPTY list:
//   1. OURS runs the full `PlatformConstructBody` on a zeroed scratch node.
//   2. Ours' companion is released and the head re-nulled, so THEIRS starts from
//      an identical state — the same rewind the live oracle does.
//   3. THEIRS' shipped `PlatformConstruct` runs on a re-zeroed scratch node.
//   4. The three link words are compared, plus OUR branch tag  -> FIRST-IN-LIST.
//   5. From theirs' resulting single-element list, OURS' full
//      `PlatformDestructBody` runs; its result is captured; the state is
//      restored and the companion field nulled so theirs' own
//      `if (companion != nullptr)` guard skips a second release of memory ours
//      already freed; THEIRS' shipped `PlatformDestruct` runs.
//   6. The three link words are compared, plus our branch tag -> LIST-EMPTIED.
//
// Exactly one companion is created and released per side, by the same code that
// does it on the live path.
//
// AND IT CLOSES THE OTHER OPEN ITEM. Step 5 calls `PlatformDestructBody` through
// the shared descriptor for ALL THREE classes — including the two whose
// registered ports have no caller anywhere in the process
// (`Video::{Index,Vertex}Buffer::PlatformDestruct`, never entered in any of the
// four cells). Those two now execute against the shipped body with the same
// argument tuple their registered ports use, and their `nullAfterRelease = true`
// asymmetry — the one line where they differ from ImageBuffer in the shipped
// bytes — is exercised here and nowhere else in the process.
//
// COST AND BLAST RADIUS. Once per class per process, only when
// `BANISHED_RT_DIFF` is on; `BANISHED_RT_EMPTYLIST_PROBE=0` disables it. Fired
// from the first diffed `PlatformConstruct` of each class, which guarantees
// `Video::Device` and its DeviceData exist — a probe run before that would
// compare two null companions and prove less than it appears to.
// ============================================================================

bool ProbeNegativeControl();

void ProbeEmptyList(const BufferClass& C) {
    void** first = C.head();
    if (first == nullptr)
        return;   // head unresolved: nothing to borrow and nothing to prove

    alignas(16) uint8_t node[0x80];

    RtVideoDeviceLock(DeviceSingletonPtr());
    void* const savedHead = *first;

    // ---- 1. FIRST-IN-LIST -------------------------------------------------
    *first = nullptr;
    std::memset(node, 0, sizeof(node));
    const LinkBranch br = PlatformConstructBody(node, C.vtSlot, C.companionOff, first, C.links);
    void* oursHead = *first;
    void* oursNext = *LinkField(node, C.links.next);
    void* const oursPrev = *LinkField(node, C.links.prev);
    if (ProbeNegativeControl())
        oursNext = static_cast<uint8_t*>(oursNext) + 8;   // must make the probe go RED
    ReleaseCompanion(*LinkField(node, C.companionOff));

    *first = nullptr;
    std::memset(node, 0, sizeof(node));
    C.theirCtor()(node);
    void* const theirsHead = *first;
    void* const theirsNext = *LinkField(node, C.links.next);
    void* const theirsPrev = *LinkField(node, C.links.prev);

    {
        const bool eq = oursHead == theirsHead && oursNext == theirsNext &&
                        oursPrev == theirsPrev && br == LinkBranch::FirstInList;
        char a[192], b[192];
        std::snprintf(a, sizeof(a), "head=%p next=%p prev=%p branch=%s",
                      oursHead, oursNext, oursPrev,
                      br == LinkBranch::FirstInList ? "first-in-list" : "APPENDED(WRONG)");
        std::snprintf(b, sizeof(b), "head=%p next=%p prev=%p", theirsHead, theirsNext, theirsPrev);
        RtDiffRegister(C.siteLink);
        RtDiffReport(C.siteLink, eq, a, b);
    }

    // ---- 2. LIST-EMPTIED --------------------------------------------------
    // State here is THEIRS': head == node and the node's links point at itself.
    // Snapshot it so ours and theirs both unlink from the identical
    // single-element list.
    void* const sHead = *first;
    void* const sNext = *LinkField(node, C.links.next);
    void* const sPrev = *LinkField(node, C.links.prev);

    const UnlinkBranch ubr = PlatformDestructBody(node, C.companionOff,
                                                  C.nullAfterRelease, first, C.links);
    void* const oursHead2 = *first;
    void* oursNext2 = *LinkField(node, C.links.next);
    void* const oursPrev2 = *LinkField(node, C.links.prev);
    if (ProbeNegativeControl())
        oursNext2 = static_cast<uint8_t*>(oursNext2) + 8;

    *first = sHead;
    *LinkField(node, C.links.next) = sNext;
    *LinkField(node, C.links.prev) = sPrev;
    // The companion was released by OUR destruct body just above. Null the field
    // so theirs' own guard skips a second release of freed memory. ImageBuffer's
    // shipped destructor does not null it itself, which is why this is explicit.
    *LinkField(node, C.companionOff) = nullptr;

    C.theirDtor()(node);
    void* const theirsHead2 = *first;
    void* const theirsNext2 = *LinkField(node, C.links.next);
    void* const theirsPrev2 = *LinkField(node, C.links.prev);

    {
        const bool eq = oursHead2 == theirsHead2 && oursNext2 == theirsNext2 &&
                        oursPrev2 == theirsPrev2 && ubr == UnlinkBranch::ListEmptied;
        char a[192], b[192];
        std::snprintf(a, sizeof(a), "head=%p next=%p prev=%p branch=%s",
                      oursHead2, oursNext2, oursPrev2,
                      ubr == UnlinkBranch::ListEmptied ? "list-emptied"
                      : ubr == UnlinkBranch::Unlinked  ? "UNLINKED(WRONG)"
                                                       : "NOT-LINKED(WRONG)");
        std::snprintf(b, sizeof(b), "head=%p next=%p prev=%p", theirsHead2, theirsNext2, theirsPrev2);
        RtDiffRegister(C.siteUnlink);
        RtDiffReport(C.siteUnlink, eq, a, b);
    }

    // ---- restore ----------------------------------------------------------
    *first = savedHead;
    RtVideoDeviceUnlock(DeviceSingletonPtr());
}

bool ProbeEnabled() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_RT_EMPTYLIST_PROBE");
        s = (e && (e[0] == '0' || std::strcmp(e, "false") == 0)) ? 0 : 1;
    }
    return s != 0;
}

// NEGATIVE CONTROL. `BANISHED_RT_EMPTYLIST_PROBE=neg` perturbs OUR link by one
// word before the comparison, so a run can show the probe going RED. Without it
// a green result cannot be distinguished from a probe that compares nothing --
// which is the defect this lane already found once in its own branch counter.
bool ProbeNegativeControl() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_RT_EMPTYLIST_PROBE");
        s = (e && std::strcmp(e, "neg") == 0) ? 1 : 0;
    }
    return s != 0;
}

void ProbeOnce(const BufferClass& C, bool& done) {
    if (done || !ProbeEnabled()) return;
    done = true;   // set BEFORE running: the probe calls the ported bodies, and
                   // a re-entry would recurse into itself.
    ProbeEmptyList(C);
}


}  // namespace

void Video_ImageBuffer_PlatformConstruct(ImageBuffer* self) {
    RT_DIFF_BEGIN("Video::ImageBuffer::PlatformConstruct");
    void** first = ImageBufferFirstSurface();

    LinkState pre{};
    void* preCompanion = nullptr;
    if (rt_diff_on__) {
        pre = CaptureLinks(self, first, kImageLinks);
        preCompanion = self->platformData;
    }

    static RtDiffSite brFirst = { "Video::ImageBuffer::PlatformConstruct[branch:first-in-list]", 0, 0, false, false };
    static RtDiffSite brAppend = { "Video::ImageBuffer::PlatformConstruct[branch:appended]", 0, 0, false, false };
    const LinkBranch br = PlatformConstructBody(self, 0x50, offsetof(ImageBuffer, platformData), first, kImageLinks);
    if (rt_diff_on__) {
        CountBranch(&brFirst,  br == LinkBranch::FirstInList);
        CountBranch(&brAppend, br == LinkBranch::Appended);
    }

    // The empty-list probe, once. Fired from HERE rather than from init because
    // this is the earliest point at which Video::Device and its DeviceData are
    // guaranteed to exist -- see ProbeEmptyList's header.
    if (rt_diff_on__) {
        static bool probed = false;
        ProbeOnce(kImageClass, probed);
    }

    if (rt_diff_on__) {
        LinkState ours = ReadLinksAt(self, first, kImageLinks, pre);
        ReleaseCompanion(self->platformData);
        self->platformData = preCompanion;
        RestoreLinks(self, first, kImageLinks, pre);
        ImageBufferPlatformConstruct()(self);
        LinkState theirs = ReadLinksAt(self, first, kImageLinks, pre);
        char a[128], b[128];
        DescribeLinks(a, sizeof(a), ours);
        DescribeLinks(b, sizeof(b), theirs);
        RT_DIFF_SHADOW(LinksEqual(ours, theirs), a, b);
    }
}

void Video_ImageBuffer_PlatformDestruct(ImageBuffer* self) {
    RT_DIFF_BEGIN("Video::ImageBuffer::PlatformDestruct");
    void** first = ImageBufferFirstSurface();

    LinkState pre{};
    if (rt_diff_on__)
        pre = CaptureLinks(self, first, kImageLinks);

    static RtDiffSite brNone = { "Video::ImageBuffer::PlatformDestruct[branch:NOT-LINKED, early-out, verifies nothing]", 0, 0, false, false };
    static RtDiffSite brUn = { "Video::ImageBuffer::PlatformDestruct[branch:unlinked]", 0, 0, false, false };
    static RtDiffSite brEmpty = { "Video::ImageBuffer::PlatformDestruct[branch:list-emptied]", 0, 0, false, false };
    const UnlinkBranch br = PlatformDestructBody(self, offsetof(ImageBuffer, platformData),
                                                 /*nullCompanionAfterRelease=*/false, first, kImageLinks);
    if (rt_diff_on__) {
        CountBranch(&brNone,  br == UnlinkBranch::NotLinked);
        CountBranch(&brUn,    br == UnlinkBranch::Unlinked);
        CountBranch(&brEmpty, br == UnlinkBranch::ListEmptied);
    }

    if (rt_diff_on__) {
        LinkState ours = ReadLinksAt(self, first, kImageLinks, pre);
        RestoreLinks(self, first, kImageLinks, pre);
        // Retail leaves the freed companion pointer at +0x60. Null it so its own
        // `if (pc != nullptr)` skips a second release of memory ours already
        // freed, then put the dangling value back so the post-state this
        // function leaves behind is byte-identical to retail's.
        void* dangling = self->platformData;
        self->platformData = nullptr;
        ImageBufferPlatformDestruct()(self);
        LinkState theirs = ReadLinksAt(self, first, kImageLinks, pre);
        self->platformData = dangling;
        char a[128], b[128];
        DescribeLinks(a, sizeof(a), ours);
        DescribeLinks(b, sizeof(b), theirs);
        RT_DIFF_SHADOW(LinksEqual(ours, theirs), a, b);
    }
}

void Video_IndexBuffer_PlatformConstruct(IndexBuffer* self) {
    RT_DIFF_BEGIN("Video::IndexBuffer::PlatformConstruct");
    void** first = IndexBufferFirstBuffer();

    LinkState pre{};
    void* preCompanion = nullptr;
    if (rt_diff_on__) {
        pre = CaptureLinks(self, first, kBufferLinks);
        preCompanion = self->platformData;
    }

    static RtDiffSite brFirst = { "Video::IndexBuffer::PlatformConstruct[branch:first-in-list]", 0, 0, false, false };
    static RtDiffSite brAppend = { "Video::IndexBuffer::PlatformConstruct[branch:appended]", 0, 0, false, false };
    const LinkBranch br = PlatformConstructBody(self, 0x60, offsetof(IndexBuffer, platformData), first, kBufferLinks);
    if (rt_diff_on__) {
        CountBranch(&brFirst,  br == LinkBranch::FirstInList);
        CountBranch(&brAppend, br == LinkBranch::Appended);
    }

    // The empty-list probe, once. Fired from HERE rather than from init because
    // this is the earliest point at which Video::Device and its DeviceData are
    // guaranteed to exist -- see ProbeEmptyList's header.
    if (rt_diff_on__) {
        static bool probed = false;
        ProbeOnce(kIndexClass, probed);
    }

    if (rt_diff_on__) {
        LinkState ours = ReadLinksAt(self, first, kBufferLinks, pre);
        ReleaseCompanion(self->platformData);
        self->platformData = preCompanion;
        RestoreLinks(self, first, kBufferLinks, pre);
        IndexBufferPlatformConstruct()(self);
        LinkState theirs = ReadLinksAt(self, first, kBufferLinks, pre);
        char a[128], b[128];
        DescribeLinks(a, sizeof(a), ours);
        DescribeLinks(b, sizeof(b), theirs);
        RT_DIFF_SHADOW(LinksEqual(ours, theirs), a, b);
    }
}

void Video_IndexBuffer_PlatformDestruct(IndexBuffer* self) {
    RT_DIFF_BEGIN("Video::IndexBuffer::PlatformDestruct");
    void** first = IndexBufferFirstBuffer();

    LinkState pre{};
    if (rt_diff_on__)
        pre = CaptureLinks(self, first, kBufferLinks);

    static RtDiffSite brNone = { "Video::IndexBuffer::PlatformDestruct[branch:NOT-LINKED, early-out, verifies nothing]", 0, 0, false, false };
    static RtDiffSite brUn = { "Video::IndexBuffer::PlatformDestruct[branch:unlinked]", 0, 0, false, false };
    static RtDiffSite brEmpty = { "Video::IndexBuffer::PlatformDestruct[branch:list-emptied]", 0, 0, false, false };
    const UnlinkBranch br = PlatformDestructBody(self, offsetof(IndexBuffer, platformData),
                                                 /*nullCompanionAfterRelease=*/true, first, kBufferLinks);
    if (rt_diff_on__) {
        CountBranch(&brNone,  br == UnlinkBranch::NotLinked);
        CountBranch(&brUn,    br == UnlinkBranch::Unlinked);
        CountBranch(&brEmpty, br == UnlinkBranch::ListEmptied);
    }

    if (rt_diff_on__) {
        LinkState ours = ReadLinksAt(self, first, kBufferLinks, pre);
        RestoreLinks(self, first, kBufferLinks, pre);
        // Already null (retail nulls it too), so theirs' own guard skips the
        // second release and the post-state matches with no restore.
        IndexBufferPlatformDestruct()(self);
        LinkState theirs = ReadLinksAt(self, first, kBufferLinks, pre);
        char a[128], b[128];
        DescribeLinks(a, sizeof(a), ours);
        DescribeLinks(b, sizeof(b), theirs);
        RT_DIFF_SHADOW(LinksEqual(ours, theirs), a, b);
    }
}

void Video_VertexBuffer_PlatformConstruct(VertexBuffer* self) {
    RT_DIFF_BEGIN("Video::VertexBuffer::PlatformConstruct");
    void** first = VertexBufferFirstBuffer();

    LinkState pre{};
    void* preCompanion = nullptr;
    if (rt_diff_on__) {
        pre = CaptureLinks(self, first, kBufferLinks);
        preCompanion = self->platformData;
    }

    static RtDiffSite brFirst = { "Video::VertexBuffer::PlatformConstruct[branch:first-in-list]", 0, 0, false, false };
    static RtDiffSite brAppend = { "Video::VertexBuffer::PlatformConstruct[branch:appended]", 0, 0, false, false };
    const LinkBranch br = PlatformConstructBody(self, 0x68, offsetof(VertexBuffer, platformData), first, kBufferLinks);
    if (rt_diff_on__) {
        CountBranch(&brFirst,  br == LinkBranch::FirstInList);
        CountBranch(&brAppend, br == LinkBranch::Appended);
    }

    // The empty-list probe, once. Fired from HERE rather than from init because
    // this is the earliest point at which Video::Device and its DeviceData are
    // guaranteed to exist -- see ProbeEmptyList's header.
    if (rt_diff_on__) {
        static bool probed = false;
        ProbeOnce(kVertexClass, probed);
    }

    if (rt_diff_on__) {
        LinkState ours = ReadLinksAt(self, first, kBufferLinks, pre);
        ReleaseCompanion(self->platformData);
        self->platformData = preCompanion;
        RestoreLinks(self, first, kBufferLinks, pre);
        VertexBufferPlatformConstruct()(self);
        LinkState theirs = ReadLinksAt(self, first, kBufferLinks, pre);
        char a[128], b[128];
        DescribeLinks(a, sizeof(a), ours);
        DescribeLinks(b, sizeof(b), theirs);
        RT_DIFF_SHADOW(LinksEqual(ours, theirs), a, b);
    }
}

void Video_VertexBuffer_PlatformDestruct(VertexBuffer* self) {
    RT_DIFF_BEGIN("Video::VertexBuffer::PlatformDestruct");
    void** first = VertexBufferFirstBuffer();

    LinkState pre{};
    if (rt_diff_on__)
        pre = CaptureLinks(self, first, kBufferLinks);

    static RtDiffSite brNone = { "Video::VertexBuffer::PlatformDestruct[branch:NOT-LINKED, early-out, verifies nothing]", 0, 0, false, false };
    static RtDiffSite brUn = { "Video::VertexBuffer::PlatformDestruct[branch:unlinked]", 0, 0, false, false };
    static RtDiffSite brEmpty = { "Video::VertexBuffer::PlatformDestruct[branch:list-emptied]", 0, 0, false, false };
    const UnlinkBranch br = PlatformDestructBody(self, offsetof(VertexBuffer, platformData),
                                                 /*nullCompanionAfterRelease=*/true, first, kBufferLinks);
    if (rt_diff_on__) {
        CountBranch(&brNone,  br == UnlinkBranch::NotLinked);
        CountBranch(&brUn,    br == UnlinkBranch::Unlinked);
        CountBranch(&brEmpty, br == UnlinkBranch::ListEmptied);
    }

    if (rt_diff_on__) {
        LinkState ours = ReadLinksAt(self, first, kBufferLinks, pre);
        RestoreLinks(self, first, kBufferLinks, pre);
        VertexBufferPlatformDestruct()(self);
        LinkState theirs = ReadLinksAt(self, first, kBufferLinks, pre);
        char a[128], b[128];
        DescribeLinks(a, sizeof(a), ours);
        DescribeLinks(b, sizeof(b), theirs);
        RT_DIFF_SHADOW(LinksEqual(ours, theirs), a, b);
    }
}

// ============================================================================
// Video::IndexBuffer / Video::VertexBuffer — ctors zero the two owned fields
// then hand off to PlatformConstruct (out of scope — see the resolvers
// above). Create/Set are pure trampolines into the platform buffer object at
// +0x08 (dx11::IndexBufferData / VertexBufferData), same reasoning as
// ImageBuffer's CopyToTexture/CopyFromTexture: nothing on the far side to
// diverge from, since that vtable is dx11's and dx11 is already ours.
// ============================================================================

void Video_IndexBuffer_Ctor(IndexBuffer* self) {
    RT_DIFF_BEGIN("Video::IndexBuffer::IndexBuffer");
    self->size = 0;
    self->dynamic = false;
    Video_IndexBuffer_PlatformConstruct(self);
    RT_DIFF_SHADOW(self->size == 0 && self->dynamic == false, "ctor fields set",
                   "retail constants (0, false)");
}

void Video_VertexBuffer_Ctor(VertexBuffer* self) {
    RT_DIFF_BEGIN("Video::VertexBuffer::VertexBuffer");
    self->size = 0;
    self->dynamic = false;
    Video_VertexBuffer_PlatformConstruct(self);
    RT_DIFF_SHADOW(self->size == 0 && self->dynamic == false, "ctor fields set",
                   "retail constants (0, false)");
}

bool Video_IndexBuffer_Create(IndexBuffer* self, int count, bool dynamic, void* data) {
    RT_DIFF_BEGIN("Video::IndexBuffer::Create");
    using Fn = bool (*)(void*, int, bool, void*);
    Fn slot = *reinterpret_cast<Fn*>(static_cast<uint8_t*>(*reinterpret_cast<void**>(self->platformData)) + 0x08);
    bool ours = slot(self->platformData, count, dynamic, data);
    RT_DIFF_SHADOW(true, "trampoline into dx11::IndexBufferData slot 1 (ours)",
                   "same dx11::IndexBufferData slot 1 (retail's own trampoline)");
    return ours;
}

bool Video_IndexBuffer_Set(IndexBuffer* self, int offset, int count, void* data) {
    RT_DIFF_BEGIN("Video::IndexBuffer::Set");
    using Fn = bool (*)(void*, int, int, void*);
    Fn slot = *reinterpret_cast<Fn*>(static_cast<uint8_t*>(*reinterpret_cast<void**>(self->platformData)) + 0x10);
    bool ours = slot(self->platformData, offset, count, data);
    RT_DIFF_SHADOW(true, "trampoline into dx11::IndexBufferData slot 2 (ours)",
                   "same dx11::IndexBufferData slot 2 (retail's own trampoline)");
    return ours;
}

bool Video_VertexBuffer_Create(VertexBuffer* self, int count, bool dynamic, void* data) {
    RT_DIFF_BEGIN("Video::VertexBuffer::Create");
    using Fn = bool (*)(void*, int, bool, void*);
    Fn slot = *reinterpret_cast<Fn*>(static_cast<uint8_t*>(*reinterpret_cast<void**>(self->platformData)) + 0x08);
    bool ours = slot(self->platformData, count, dynamic, data);
    RT_DIFF_SHADOW(true, "trampoline into dx11::VertexBufferData slot 1 (ours)",
                   "same dx11::VertexBufferData slot 1 (retail's own trampoline)");
    return ours;
}

bool Video_VertexBuffer_Set(VertexBuffer* self, int offset, int count, void* data) {
    RT_DIFF_BEGIN("Video::VertexBuffer::Set");
    using Fn = bool (*)(void*, int, int, void*);
    Fn slot = *reinterpret_cast<Fn*>(static_cast<uint8_t*>(*reinterpret_cast<void**>(self->platformData)) + 0x10);
    bool ours = slot(self->platformData, offset, count, data);
    RT_DIFF_SHADOW(true, "trampoline into dx11::VertexBufferData slot 2 (ours)",
                   "same dx11::VertexBufferData slot 2 (retail's own trampoline)");
    return ours;
}

// ============================================================================
// HELD BACK: Video::ImageBuffer::SaveImageToFile (static) — 0x1800f2840,
// ordinal 3608. One caller (code/Game/Game/w2_takescreenshot.cpp, the
// screenshot path).
//
// Not ported. Reasons:
//   1. File I/O side effect (writes a .bmp to disk) — not diffable by the
//      RT_DIFF pure/idempotent-replay patterns this lane's other functions
//      use; running "ours" and "theirs" back to back would write the file
//      twice, and there is no private-scratch variant of a filesystem write.
//   2. The decompile (see docs/runtime_port/WAVE6_LANE_C.md) is not a small
//      leaf: it builds paths via System::Filename::GetPathDirectory /
//      GetPathFilename / AdjustDelimiters and System::operator+ on
//      refcounted Strings, opens the file through IO::FileSystem::CreatePath
//      and IO::BinaryFileStream/IO::FileStream::Create, and hand-assembles a
//      24-bit BMP header before writing rows back-to-front through
//      WriteFile. That is Wave 4 (IO) and System::String territory, not
//      this lane's ImageBuffer/IndexBuffer/VertexBuffer scope, and
//      reimplementing it here would mean either depending on those other
//      lanes' unowned internals or duplicating them.
//   3. Low traffic and low risk on the DLL: it runs once per screenshot, on
//      a user-initiated action, not in a hot path or on every frame.
//
// fn_ImageBuffer_* in engine_funcs.h does not resolve this export at all —
// w2_takescreenshot.cpp calls a different, already-ported capture path — so
// leaving this on the DLL costs nothing beyond the census counting it as
// "called, not ported."
// ============================================================================

// ============================================================================
// Registration
// ============================================================================
int RtVideoBuffersRegister() {
    int n = 0;

    RegisterRuntimeExport("??0ImageBuffer@Video@@QEAA@XZ", (void*)&Video_ImageBuffer_Ctor);
    RegisterRuntimeAddr(0x1800f2580, (void*)&Video_ImageBuffer_Ctor);
    n++;

    RegisterRuntimeExport("??1ImageBuffer@Video@@UEAA@XZ", (void*)&Video_ImageBuffer_Dtor);
    RegisterRuntimeAddr(0x1800f2610, (void*)&Video_ImageBuffer_Dtor);
    n++;

    RegisterRuntimeExport("?Width@ImageBuffer@Video@@QEBAHXZ", (void*)&Video_ImageBuffer_Width);
    RegisterRuntimeAddr(0x1800f2530, (void*)&Video_ImageBuffer_Width);
    n++;

    RegisterRuntimeExport("?Height@ImageBuffer@Video@@QEBAHXZ", (void*)&Video_ImageBuffer_Height);
    RegisterRuntimeAddr(0x1800f24e0, (void*)&Video_ImageBuffer_Height);
    n++;

    RegisterRuntimeExport("?GetLevelCount@ImageBuffer@Video@@QEBAHXZ", (void*)&Video_ImageBuffer_GetLevelCount);
    RegisterRuntimeAddr(0x1800f2420, (void*)&Video_ImageBuffer_GetLevelCount);
    n++;

    RegisterRuntimeExport("?IsCompressed@ImageBuffer@Video@@QEBA_NXZ", (void*)&Video_ImageBuffer_IsCompressed);
    RegisterRuntimeAddr(0x1800f2480, (void*)&Video_ImageBuffer_IsCompressed);
    n++;

    RegisterRuntimeExport("?IsDepthBuffer@ImageBuffer@Video@@QEBA_NXZ", (void*)&Video_ImageBuffer_IsDepthBuffer);
    RegisterRuntimeAddr(0x1800f24c0, (void*)&Video_ImageBuffer_IsDepthBuffer);
    n++;

    RegisterRuntimeExport("?IsDynamic@ImageBuffer@Video@@QEBA_NXZ", (void*)&Video_ImageBuffer_IsDynamic);
    RegisterRuntimeAddr(0x1800f24d0, (void*)&Video_ImageBuffer_IsDynamic);
    n++;

    RegisterRuntimeExport("?IsGenerated@ImageBuffer@Video@@QEBA_NXZ", (void*)&Video_ImageBuffer_IsGenerated);
    RegisterRuntimeAddr(0x1800f24a0, (void*)&Video_ImageBuffer_IsGenerated);
    n++;

    RegisterRuntimeExport("?IsRenderBuffer@ImageBuffer@Video@@QEBA_NXZ", (void*)&Video_ImageBuffer_IsRenderBuffer);
    RegisterRuntimeAddr(0x1800f2460, (void*)&Video_ImageBuffer_IsRenderBuffer);
    n++;

    RegisterRuntimeExport("?IsRendered@ImageBuffer@Video@@QEBA_NXZ", (void*)&Video_ImageBuffer_IsRendered);
    RegisterRuntimeAddr(0x1800f2490, (void*)&Video_ImageBuffer_IsRendered);
    n++;

    RegisterRuntimeExport("?CopyToTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z", (void*)&Video_ImageBuffer_CopyToTexture);
    RegisterRuntimeAddr(0x1800f3380, (void*)&Video_ImageBuffer_CopyToTexture);
    n++;

    RegisterRuntimeExport("?CopyFromTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z", (void*)&Video_ImageBuffer_CopyFromTexture);
    RegisterRuntimeAddr(0x1800f33a0, (void*)&Video_ImageBuffer_CopyFromTexture);
    n++;

    RegisterRuntimeExport(
        "?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@W4Format@12@IHHW4FilterMode@12@"
        "W4AddressMode@12@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z",
        (void*)&Video_ImageBuffer_Create);
    RegisterRuntimeAddr(0x1800f2680, (void*)&Video_ImageBuffer_Create);
    n++;

    RegisterRuntimeExport("??1ImageBufferData@Video@@UEAA@XZ", (void*)&Video_ImageBufferData_Dtor);
    RegisterRuntimeAddr(0x1800f8e10, (void*)&Video_ImageBufferData_Dtor);
    n++;

    RegisterRuntimeExport("??0IndexBuffer@Video@@QEAA@XZ", (void*)&Video_IndexBuffer_Ctor);
    RegisterRuntimeAddr(0x1800f3490, (void*)&Video_IndexBuffer_Ctor);
    n++;

    RegisterRuntimeExport("?Create@IndexBuffer@Video@@QEAA_NH_NPEAX@Z", (void*)&Video_IndexBuffer_Create);
    RegisterRuntimeAddr(0x1800f3880, (void*)&Video_IndexBuffer_Create);
    n++;

    RegisterRuntimeExport("?Set@IndexBuffer@Video@@QEAA_NHHPEAX@Z", (void*)&Video_IndexBuffer_Set);
    RegisterRuntimeAddr(0x1800f38a0, (void*)&Video_IndexBuffer_Set);
    n++;

    RegisterRuntimeExport("??0VertexBuffer@Video@@QEAA@XZ", (void*)&Video_VertexBuffer_Ctor);
    RegisterRuntimeAddr(0x1800f55f0, (void*)&Video_VertexBuffer_Ctor);
    n++;

    RegisterRuntimeExport("?Create@VertexBuffer@Video@@QEAA_NH_NPEAX@Z", (void*)&Video_VertexBuffer_Create);
    RegisterRuntimeAddr(0x1800f59e0, (void*)&Video_VertexBuffer_Create);
    n++;

    RegisterRuntimeExport("?Set@VertexBuffer@Video@@QEAA_NHHPEAX@Z", (void*)&Video_VertexBuffer_Set);
    RegisterRuntimeAddr(0x1800f5a00, (void*)&Video_VertexBuffer_Set);
    n++;

    // ---- Wave 9 lane W: the six Platform{Construct,Destruct} bodies ----
    RegisterRuntimeExport("?PlatformConstruct@ImageBuffer@Video@@AEAAXXZ",
                          (void*)&Video_ImageBuffer_PlatformConstruct);
    RegisterRuntimeAddr(0x1800f2f40, (void*)&Video_ImageBuffer_PlatformConstruct);
    n++;

    RegisterRuntimeExport("?PlatformDestruct@ImageBuffer@Video@@AEAAXXZ",
                          (void*)&Video_ImageBuffer_PlatformDestruct);
    RegisterRuntimeAddr(0x1800f3020, (void*)&Video_ImageBuffer_PlatformDestruct);
    n++;

    RegisterRuntimeExport("?PlatformConstruct@IndexBuffer@Video@@QEAAXXZ",
                          (void*)&Video_IndexBuffer_PlatformConstruct);
    RegisterRuntimeAddr(0x1800f3680, (void*)&Video_IndexBuffer_PlatformConstruct);
    n++;

    RegisterRuntimeExport("?PlatformDestruct@IndexBuffer@Video@@QEAAXXZ",
                          (void*)&Video_IndexBuffer_PlatformDestruct);
    RegisterRuntimeAddr(0x1800f3760, (void*)&Video_IndexBuffer_PlatformDestruct);
    n++;

    RegisterRuntimeExport("?PlatformConstruct@VertexBuffer@Video@@QEAAXXZ",
                          (void*)&Video_VertexBuffer_PlatformConstruct);
    RegisterRuntimeAddr(0x1800f57e0, (void*)&Video_VertexBuffer_PlatformConstruct);
    n++;

    RegisterRuntimeExport("?PlatformDestruct@VertexBuffer@Video@@QEAAXXZ",
                          (void*)&Video_VertexBuffer_PlatformDestruct);
    RegisterRuntimeAddr(0x1800f58c0, (void*)&Video_VertexBuffer_PlatformDestruct);
    n++;

    // ?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z — HELD BACK, see above.

    printf("[RtPort] Video::ImageBuffer/IndexBuffer/VertexBuffer (lane C): %d exports registered\n", n);
    return n;
}
