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
#include "../../types/video/ImageBuffer.h"
#include "../../types/video/Buffers.h"
#include "../../types/video/Device.h"   // Wave 9 lane W: the lock and +0x100
#include "../../types/video/CompanionData.h"  // ENDGAME B.3.5: Video::{Index,Vertex}BufferData

#include <cstdio>
#include <cstddef>
#include <cstdint>

// runtime/src/video/buffers_win.cpp — see the note at their use site below.
void RtVideoDeviceLock(void** deviceSingleton);
void RtVideoDeviceUnlock(void** deviceSingleton);

namespace {

using MemFreeFn           = void (*)(void*);


// ENDGAME B.3.5 block VC — `ObjectExternalDtor()` IS GONE. It was an
// `RT_EXPORT_ONCE(DtorFn, "??1External@Object@@UEAA@XZ")` (binding spelling 3)
// that `~ImageBuffer` used to reach its own base destructor with, next to a
// hand-written stamp of `??_7CompiledExternal@Object@@6B@`. Both are the
// DESTRUCTOR CHAIN now: `Video::ImageBuffer` really derives from
// `Object::CompiledExternal`, so `~CompiledExternal()` and then `~External()`
// run after the member body with no resolver on the path — the same retirement
// `Video::VertexLayout` made in b35-B, and `Object::External`'s own banner
// (`runtime/src/object/external_class.cpp:128-135`) is the settled rule for why
// the two base vptr stores are the compiler's and not hand-written on a
// converted object.

// REGISTRY door, not the raw DLL door. The raw door reads the DLL's export
// table directly and is NULL with the DLL absent: the 2026-09-12 cutover walk
// died here on the first ImageBuffer with a name (Environment\RainTexture.png)
// once the quit path reached ExternalFactory::Clean.
MemFreeFn MemFree() {
    return RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
}

// This class's own vtable, restored at dtor entry (the standard MSVC "run the
// dtor body under this class's own vtable, not a derived override" idiom —
// reference_phantom_dtor_mangling_qeaa_vs_ueaa.md is the same family of bug
// this guards against getting backwards).
void* ImageBufferVtable() {
    static void* v = ResolveExport("??_7ImageBuffer@Video@@6B@");
    return v;
}
// `ImageBufferDataVtable()` used to live here, as the whole body of
// `??1ImageBufferData@Video@@`. ENDGAME B.3.5 block VC: that door delegates to
// `Video::ImageBufferData::~ImageBufferData()`, whose body — the same
// `??_7ImageBufferData@Video@@6B@` re-stamp — is in `companion_data.cpp` with the
// other eleven, so the second copy of the resolver went with the second copy of
// the store.

// Video::Device::_singleton / Video::Configuration::_singleton — data
// exports, resolved once and cached exactly like a function pointer. Both are
// read verbatim by Width()/Height(); lane B owns Video::Device's own body,
// not these two field reads.
void** DeviceSingletonPtr() {
    void** p = reinterpret_cast<void**>(&::Video::Device::_singleton);
    return p;
}
void** ConfigurationSingletonPtr() {
    void** p = reinterpret_cast<void**>(&::Video::Configuration::_singleton);
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
// bytes and the walkers are ours. the raw DLL door would be null at cutover and
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
// Wave 9 lane GFX2. Minted (standalone/rt_data_mint_tables.inc, 0x1801fe790,
// 8 bytes) and already read PORT-AWARE by runtime/src/video/dynamic_resources.cpp
// so our linkage and lane VID's walkers agree on ONE list on both sides of the
// cutover.
void** RenderBufferFirstRenderBuffer() {
    static void** p = reinterpret_cast<void**>(
        ResolveExport("?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA"));
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

// Returns the branch taken.
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

constexpr ListLinks kImageLinks  = { 0x50, 0x58 };
constexpr ListLinks kBufferLinks = { 0x10, 0x18 };
// Wave 9 lane GFX2. Read off 0x1800f4381..0x1800f43bd: retail loads the tail as
// `[head + 0x20]` and writes `this + 0x18 = head`, which is ListLink's
// `prev = 0x20` / `next = 0x18`. Independently cross-checked against the class's
// own list WALKER: dynamic_resources.cpp records that all four walkers step the
// `prev` field (ImageBuffer +0x58, Index/Vertex +0x18, RenderBuffer +0x20), and
// +0x20 is exactly what this assignment calls `prev`.
constexpr ListLinks kRenderLinks = { 0x18, 0x20 };

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
// used to call ImageBufferPlatformConstruct() &co, which is the raw DLL door
// and therefore a call through null the moment Runtime-x64.dll is gone. Those
// four resolvers survive as the diff oracle's door and nothing else.
//
// ENDGAME B.3.5 block VC: `Video::ImageBuffer::Platform{Construct,Destruct}` are
// MEMBERS now, so the two ImageBuffer rows below are the registered DOORS over
// them rather than the bodies, and the two forward declarations are only needed
// because the doors are registered from `RtVideoBuffersRegister` at the bottom.
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
// verify anything). the diff oracle instead asserts the constants we just
// wrote are exactly what retail writes.
// ============================================================================
// ENDGAME B.3.5 block VC: STILL A DOOR, and for B4b-2's reason — it runs on raw
// `operator new` bytes whose first field it then writes with the MINTED vtable,
// `Video::ImageBuffer` is abstract in C++ (slot 0 is a `deleting_dtor.cpp` table
// row), and `context_ctor.cpp` reaches it for the two default render images. What
// converted inside it is the hand-off to `PlatformConstruct`, now a qualified
// member call.
void Video_ImageBuffer_Ctor(ImageBuffer* self) {

    self->vtable = ImageBufferVtable();       // Base->External->CompiledExternal->ImageBuffer chain,
                                               // collapsed: only the final vtable is observable.
    self->externalData08 = 0;
    self->externalData10 = 0;
    self->externalData18 = 0;

    // ENDGAME B.3.8 block RB (RULE B8-1): the field stores and the
    // PlatformConstruct call are ONE body with the real constructor
    // `Video::ImageBuffer()` — `InitFields`, beside ~ImageBuffer below.
    Video::ImageBuffer::DoorBody::Init(reinterpret_cast<Video::ImageBuffer*>(self));   // B.3.8 bridge

}

// ============================================================================
// Video::ImageBuffer::~ImageBuffer() — 0x1800f2610, ordinal 608
//
// POST-STATE only: a second destructor run on the same object (or a scratch
// copy) would double-decrement the refcount at pixelData-4 and potentially
// double-free — there is no safe way to run "theirs" without mutating shared
// allocator state. Reproduced field-for-field from the decompile.
// ============================================================================
// ENDGAME B.3.5 block VC — THE BODY IS `Video::ImageBuffer::~ImageBuffer()`, at
// `namespace Video` scope below. Retail's last two steps are gone from the body
// and are the CHAIN's: the `??_7CompiledExternal@Object@@6B@` stamp is what
// `~CompiledExternal()` emits and the `??1External@Object@@UEAA@XZ` call is
// `~External()`, both of which now run because the class really derives.
void Video_ImageBuffer_Dtor(ImageBuffer* self) {
    // B.3.5 bridge — the registered door's parameter is the POD view.
    reinterpret_cast<Video::ImageBuffer*>(self)
        ->Video::ImageBuffer::~ImageBuffer();
}

// ============================================================================
// Video::ImageBuffer accessors — pure reads. Bodies are the MEMBERS below; each
// door is a one-line QUALIFIED non-virtual delegation through a bridge cast.
// (The `using Fn = …` typedefs that used to sit unused at the bottom of each
// door were the retired differential oracle's, and went with the bodies.)
// ============================================================================

int Video_ImageBuffer_Width(ImageBuffer* self) {
    return reinterpret_cast<const Video::ImageBuffer*>(self)
               ->Video::ImageBuffer::Width();                    // B.3.5 bridge
}

int Video_ImageBuffer_Height(ImageBuffer* self) {
    return reinterpret_cast<const Video::ImageBuffer*>(self)
               ->Video::ImageBuffer::Height();                   // B.3.5 bridge
}

int Video_ImageBuffer_GetLevelCount(ImageBuffer* self) {
    return reinterpret_cast<const Video::ImageBuffer*>(self)
               ->Video::ImageBuffer::GetLevelCount();            // B.3.5 bridge
}

bool Video_ImageBuffer_IsCompressed(ImageBuffer* self) {
    return reinterpret_cast<const Video::ImageBuffer*>(self)
               ->Video::ImageBuffer::IsCompressed();             // B.3.5 bridge
}

bool Video_ImageBuffer_IsDepthBuffer(ImageBuffer* self) {
    return reinterpret_cast<const Video::ImageBuffer*>(self)
               ->Video::ImageBuffer::IsDepthBuffer();            // B.3.5 bridge
}

bool Video_ImageBuffer_IsDynamic(ImageBuffer* self) {
    return reinterpret_cast<const Video::ImageBuffer*>(self)
               ->Video::ImageBuffer::IsDynamic();                // B.3.5 bridge
}

bool Video_ImageBuffer_IsGenerated(ImageBuffer* self) {
    return reinterpret_cast<const Video::ImageBuffer*>(self)
               ->Video::ImageBuffer::IsGenerated();              // B.3.5 bridge
}

bool Video_ImageBuffer_IsRenderBuffer(ImageBuffer* self) {
    return reinterpret_cast<const Video::ImageBuffer*>(self)
               ->Video::ImageBuffer::IsRenderBuffer();           // B.3.5 bridge
}

bool Video_ImageBuffer_IsRendered(ImageBuffer* self) {
    return reinterpret_cast<const Video::ImageBuffer*>(self)
               ->Video::ImageBuffer::IsRendered();               // B.3.5 bridge
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
// from. the diff oracle registers the site (so it is visible in the census
// and the diff summary) without invoking the original a second time, which
// would upload/download the texture twice.
// ============================================================================
// ENDGAME B.3.5 block VC: the two hand-inlined `pd->vtable + 0x28` / `+ 0x30`
// dispatches are VIRTUAL CALLS on block A's real `Video::ImageBufferData` now,
// whose slots 5 and 6 that header names `Update` and `Read` — the same two byte
// offsets (0x28/8 == 5, 0x30/8 == 6), spelled as the slots they are, with the
// index checked by the compiler instead of by a comment.
bool Video_ImageBuffer_CopyToTexture(ImageBuffer* self, void* data, int width, int height) {
    return reinterpret_cast<Video::ImageBuffer*>(self)
               ->Video::ImageBuffer::CopyToTexture(data, width, height);   // B.3.5 bridge
}

bool Video_ImageBuffer_CopyFromTexture(ImageBuffer* self, void* data, int width, int height) {
    return reinterpret_cast<Video::ImageBuffer*>(self)
               ->Video::ImageBuffer::CopyFromTexture(data, width, height); // B.3.5 bridge
}

// ============================================================================
// Video::ImageBuffer::Create — 0x1800f2680, ordinal 1877. Stamps the ten
// descriptor fields from its parameters (an ordinary field-store, in scope
// and fully reproduced), then a genuine two-argument call — not a raw
// register-preserving trampoline like the pair above — into
// dx11::ImageBufferData's slot 3 (ImageBufferData_Create in
// dx11/src/imagebufferdata.cpp). Same "already our own vtable on the far
// side" situation as CopyToTexture/CopyFromTexture, so the diff oracle rather
// than a second live call.
//
// Enum parameters (ImageType/Usage/Format/FilterMode/AddressMode) are plain
// `enum` in the retail signature, i.e. 4-byte int underlying type in the
// MSVC ABI with no special calling-convention handling — passed here as
// int32_t so this file does not need to invent enumerator names Runtime
// never exports.
// ============================================================================
// ENDGAME B.3.5 block VC — SLOT 9 IS A MEMBER, so `ImageBuffer.h`'s `= 0` came
// off this declaration (block C's banner said exactly this would happen once the
// body moved). The local `struct ImageLevelArray` view is gone: the parameter is
// `Video::ImageBuffer::LevelArray*`, i.e. the real
// `Collection::Array<Collection::Array<uint8_t>>` the retail mangling spells, and
// the `pd->vtable + 0x18` dispatch is `Video::ImageBufferData` slot 3, `Create`.
bool Video_ImageBuffer_Create(ImageBuffer* self, int32_t imageType, int32_t usage, int32_t format,
                               uint32_t flags, int width, int height, int32_t filter,
                               int32_t addressU, int32_t addressV, int32_t addressW,
                               Video::ImageBuffer::LevelArray* levels) {
    return reinterpret_cast<Video::ImageBuffer*>(self)
               ->Video::ImageBuffer::Create(imageType, usage, format, flags, width, height,
                                            filter, addressU, addressV, addressW,
                                            levels);                        // B.3.5 bridge
}

// (The `??1ImageBufferData@Video@@UEAA@XZ` door, retail 0x1800f8e10, is GONE:
// ENDGAME B.3.9 VID retired it with the RT_DELDTOR row that was its one caller.
// dx11/src/imagebufferdata.cpp calls `Video::ImageBufferData::~ImageBufferData()`
// directly, RULE B5-3.)

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
// DLL through the raw DLL door from the three ctors and ~ImageBuffer above.
// Wave 9 lane V's residue census reads four of the six as REAL DEPENDENCY on
// the RAW door: at cutover `the raw DLL door` is `GetProcAddress` on a module
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
// A note on reading the result: the diff oracle here reports on every call that
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
};


const BufferClass kImageClass  = { 0x50, 0x60, false, kImageLinks,  &ImageBufferFirstSurface };
const BufferClass kIndexClass  = { 0x60, 0x08, true,  kBufferLinks, &IndexBufferFirstBuffer };
const BufferClass kVertexClass = { 0x68, 0x08, true,  kBufferLinks, &VertexBufferFirstBuffer };
// Wave 9 lane GFX2. Slot 0x98 == dx11::DeviceData vtable index 19, which
// dx11/src/devicedata.cpp registers as Factory_RenderBufferData — so this
// dispatch lands in this project on both sides of the port, exactly as the other
// three do. `nullAfterRelease = false`: 0x1800f4400 releases the companion at
// +0x28 and leaves the freed pointer there, the ImageBuffer asymmetry, not the
// Index/Vertex one.
const BufferClass kRenderClass = { 0x98, 0x28, false, kRenderLinks, &RenderBufferFirstRenderBuffer };

// The descriptors' offsets are the structs', bound here so a header change
// breaks the build rather than silently mis-linking.
static_assert(offsetof(ImageBuffer, platformData)  == 0x60, "kImageClass.companionOff");
static_assert(offsetof(IndexBuffer, platformData)  == 0x08, "kIndexClass.companionOff");
static_assert(offsetof(VertexBuffer, platformData) == 0x08, "kVertexClass.companionOff");
static_assert(offsetof(RtVideo::RenderBuffer, platformData) == 0x28, "kRenderClass.companionOff");
static_assert(offsetof(RtVideo::RenderBuffer, next) == 0x18, "kRenderLinks.next");
static_assert(offsetof(RtVideo::RenderBuffer, prev) == 0x20, "kRenderLinks.prev");


}  // namespace

// ============================================================================
// ENDGAME B.3.5 block VC — THE `Video::ImageBuffer` MEMBER BODIES.
//
// At `namespace Video` scope and AFTER both anonymous namespaces (B34_BRIEFS
// A.2); the helpers above stay reachable by unqualified name because an
// anonymous namespace's members are members of the global namespace's
// declarative region. `Video::ImageBuffer` was made a real class by b35-C, which
// had to leave every one of these bodies where it was because this file is
// b35-D2's; this is the pass that moves them.
//
// THE FIELD READS DID NOT CHANGE, and two of them are deliberately still raw:
// `Width()`/`Height()` read `Device+4`/`+8` and `Configuration+0x2c` through the
// two singleton slots, as retail's inlined ladder does. Retyping those two
// receivers is `Video::Device`'s and `Video::Configuration`'s business, not this
// member's, and both singletons stay `RT_DATA_SLOT` reads (RULE B4b-4).
// ============================================================================
namespace Video {

// ??1ImageBuffer@Video@@UEAA@XZ — 0x1800f2610, ordinal 608.
//
// POST-STATE only (there is no safe way to run "theirs" a second time over the
// same refcount). Retail's four steps, and where each one is now:
//   1. stamp ImageBuffer's own vtable          — written by hand, below, at the
//      MINTED address, because a live object wears the minted table and GCC's
//      emitted store would name GCC's
//   2. PlatformDestruct                        — the qualified member call
//   3. release the `_sourceName` String by hand — below; `_sourceName` keeps the
//      `void*` type precisely so the compiler does NOT release it a second time
//   4. stamp CompiledExternal's vtable, then tail-jump ~External
//                                              — the DESTRUCTOR CHAIN, which is
//      what replaced this file's `ObjectExternalDtor()` resolver
//
// ENDGAME B.3.8 block RB: step 1's hand-written MINTED stamp is GONE. The
// destructor now runs on compiler-built objects too (DLG's ModDialog image,
// the concrete slot 0 above), and a converted object must never wear the
// minted table (B8-5); the compiler's own stamp on entry is retail's step 1.
// For the minted objects the door `Video_ImageBuffer_Dtor` still destroys,
// the vptr during this body is therefore the compiler's ImageBuffer table
// instead of the minted one — nothing in the body dispatches through `this`
// (PlatformDestruct walks the companion list; the String release is by hand).
// Steps 2 and 3 are `ReleaseFields`, the shared body.
ImageBuffer::~ImageBuffer() { ReleaseFields(); }

// ??0ImageBuffer 0x1800f2580 after the inlined External stores and the stamp.
void ImageBuffer::InitFields() {
    _width = 0;
    _height = 0;
    _dimension = 0;
    _kind = 2;         // default/none
    _format = 0;
    _flags = 0;
    _addressU = 0;
    _addressV = 0;
    _addressW = 0;
    _filter = 1;       // LINEAR
    _sourceName = nullptr;
    // +0x50/+0x58 are not written by the ctor.
    PlatformConstruct();
}

// ??1ImageBuffer 0x1800f2610 steps 2 and 3 (see above).
void ImageBuffer::ReleaseFields() {
    PlatformDestruct();

    if (_sourceName != nullptr) {
        int32_t* refcount = reinterpret_cast<int32_t*>(static_cast<uint8_t*>(_sourceName) - 4);
        *refcount -= 1;
        if (*refcount == 0) {
            _sourceName = static_cast<uint8_t*>(_sourceName) - 8;
            MemFree()(_sourceName);
        } else {
            _sourceName = nullptr;
        }
    }
}

// ?Width@ImageBuffer@Video@@QEBAHXZ  0x1800f2530
// Bit 8 = the live device width, bit 9 = half of it, otherwise the stored value
// right-shifted by `Configuration+0x2c & 0x1f` when bit 6 is set.
int32_t ImageBuffer::Width() const {
    void** device = DeviceSingletonPtr();
    void** config = ConfigurationSingletonPtr();
    if ((_flags >> 8) & 1)
        return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(*device) + 4);
    if ((_flags >> 9) & 1)
        return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(*device) + 4) / 2;
    int32_t v = _width;
    if ((_flags >> 6) & 1) {
        const uint32_t shift =
            *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(*config) + 0x2c) & 0x1f;
        v >>= shift;
    }
    return v;
}

// ?Height@ImageBuffer@Video@@QEBAHXZ  0x1800f24e0 — the same ladder on +8.
int32_t ImageBuffer::Height() const {
    void** device = DeviceSingletonPtr();
    void** config = ConfigurationSingletonPtr();
    if ((_flags >> 8) & 1)
        return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(*device) + 8);
    if ((_flags >> 9) & 1)
        return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(*device) + 8) / 2;
    int32_t v = _height;
    if ((_flags >> 6) & 1) {
        const uint32_t shift =
            *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(*config) + 0x2c) & 0x1f;
        v >>= shift;
    }
    return v;
}

// ?GetLevelCount@ImageBuffer@Video@@QEBAHXZ  0x1800f2420 — the mip ladder, which
// halves BOTH dimensions with a floor of 1 and counts until both reach 1.
int32_t ImageBuffer::GetLevelCount() const {
    int32_t count = 1;
    if (HasMipMaps()) {
        int32_t w = _width;
        int32_t h = _height;
        while (w > 1 || h > 1) {
            h >>= 1; if (h == 0) h = 1;
            w >>= 1; if (w == 0) w = 1;
            ++count;
        }
    }
    return count;
}

// The five `_kind` tests and the format test, each one `CMP` against retail's
// own constant. `IsRenderBuffer` is `_kind & ~0b101 == 0`, i.e. `_kind` in
// {0,1,4,5} — deliberately NOT the same set as `dynamic_resources.cpp`'s
// per-element guard, which also admits 3.
bool ImageBuffer::IsCompressed() const   { return _format < 3; }
bool ImageBuffer::HasMipMaps() const     { return (_flags & 1u) != 0; }
bool ImageBuffer::IsDepthBuffer() const  { return _kind == 1; }
bool ImageBuffer::IsDynamic() const      { return _kind == 3; }
bool ImageBuffer::IsGenerated() const    { return _kind == 4; }
bool ImageBuffer::IsRendered() const     { return _kind == 5; }
bool ImageBuffer::IsRenderBuffer() const {
    return (static_cast<uint32_t>(_kind) & 0xfffffffau) == 0;
}

// ?CopyToTexture@ / ?CopyFromTexture@ — 0x1800f3380 / 0x1800f33a0, two-
// instruction tail-call trampolines in retail: `this = this->_data; jmp
// [vtable + N]`. Slots 5 and 6 of `Video::ImageBufferData`, i.e. +0x28 and +0x30.
// Both land in dx11/src/imagebufferdata.cpp, which is ours on both sides of the
// port, so there is nothing on the far side to diverge from.
bool ImageBuffer::CopyToTexture(void* dst, int32_t width, int32_t height) {
    return _data->Update(dst, width, height);
}

bool ImageBuffer::CopyFromTexture(void* src, int32_t width, int32_t height) {
    return _data->Read(src, width, height);
}

// slot 9 — ?Create@ImageBuffer@Video@@UEAA_N…  0x1800f2680.
// Ten descriptor field stores in retail's order (note it interleaves
// dimension/width and kind/height), then a genuine two-argument call into
// `Video::ImageBufferData` slot 3.
bool ImageBuffer::Create(ImageType type, Usage usage, Format format, uint32_t flags,
                         int32_t width, int32_t height, FilterMode filter,
                         AddressMode addressU, AddressMode addressV,
                         AddressMode addressW, LevelArray* levels) {
    _dimension = type;
    _width     = width;
    _kind      = usage;
    _height    = height;
    _format    = format;
    _flags     = flags;
    _filter    = filter;
    _addressU  = addressU;
    _addressV  = addressV;
    _addressW  = addressW;

    return _data->Create(levels);
}

// ?PlatformConstruct@ImageBuffer@Video@@AEAAXXZ  0x1800f2f40, ordinal 3337
// ?PlatformDestruct@ImageBuffer@Video@@AEAAXXZ   0x1800f3020, ordinal 3365
//
// The shared `PlatformConstructBody`/`PlatformDestructBody` pair is NOT
// specialised per class here: it is the ONE argument tuple Wave 9 lane W's
// empty-list probe proved, and it is also `{Index,Vertex,Render}Buffer`'s. The
// ImageBuffer arm is the one that does NOT null the companion field after
// releasing it — that asymmetry is in the shipped bytes of all four and is
// reproduced rather than tidied.
void ImageBuffer::PlatformConstruct() {
    void** first = ImageBufferFirstSurface();
    (void)PlatformConstructBody(this, 0x50, offsetof(ImageBuffer, _data), first, kImageLinks);
}

void ImageBuffer::PlatformDestruct() {
    void** first = ImageBufferFirstSurface();
    (void)PlatformDestructBody(this, offsetof(ImageBuffer, _data),
                               /*nullCompanionAfterRelease=*/false, first, kImageLinks);
}

}  // namespace Video

void Video_ImageBuffer_PlatformConstruct(ImageBuffer* self) {
    reinterpret_cast<Video::ImageBuffer*>(self)
        ->Video::ImageBuffer::PlatformConstruct();                // B.3.5 bridge
}

void Video_ImageBuffer_PlatformDestruct(ImageBuffer* self) {
    reinterpret_cast<Video::ImageBuffer*>(self)
        ->Video::ImageBuffer::PlatformDestruct();                 // B.3.5 bridge
}

// ENDGAME B.3.5 block D2 — the four buffer `Platform*` bodies are members of the
// real `Video::IndexBuffer` / `Video::VertexBuffer` now. The shared
// `PlatformConstructBody`/`PlatformDestructBody` pair stays exactly as it is: it
// is the ONE argument tuple Wave 9 lane W's empty-list probe proved, and it is
// also ImageBuffer's (b35-C's class), so it is not specialised per class here.
void Video::IndexBuffer::PlatformConstruct() {
    void** first = IndexBufferFirstBuffer();
    (void)PlatformConstructBody(this, 0x60, offsetof(IndexBuffer, platformData), first, kBufferLinks);
}

void Video::IndexBuffer::PlatformDestruct() {
    void** first = IndexBufferFirstBuffer();
    (void)PlatformDestructBody(this, offsetof(IndexBuffer, platformData),
                               /*nullCompanionAfterRelease=*/true, first, kBufferLinks);
}

void Video::VertexBuffer::PlatformConstruct() {
    void** first = VertexBufferFirstBuffer();
    (void)PlatformConstructBody(this, 0x68, offsetof(VertexBuffer, platformData), first, kBufferLinks);
}

void Video::VertexBuffer::PlatformDestruct() {
    void** first = VertexBufferFirstBuffer();
    (void)PlatformDestructBody(this, offsetof(VertexBuffer, platformData),
                               /*nullCompanionAfterRelease=*/true, first, kBufferLinks);
}

void Video_IndexBuffer_PlatformConstruct(IndexBuffer* self) {
    self->Video::IndexBuffer::PlatformConstruct();
}

void Video_IndexBuffer_PlatformDestruct(IndexBuffer* self) {
    self->Video::IndexBuffer::PlatformDestruct();
}

void Video_VertexBuffer_PlatformConstruct(VertexBuffer* self) {
    self->Video::VertexBuffer::PlatformConstruct();
}

void Video_VertexBuffer_PlatformDestruct(VertexBuffer* self) {
    self->Video::VertexBuffer::PlatformDestruct();
}

// ============================================================================
// Wave 9 lane GFX2 — Video::RenderBuffer::Platform{Construct,Destruct}
//
//   0x1800f4320  ?PlatformConstruct@RenderBuffer@Video@@AEAAXXZ   ord 3341
//   0x1800f4400  ?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ    ord 3369
//
// The FOURTH instantiation of the family above, and the only one Wave 9 lane W
// left out. Both names were verified present in the shipped export table before
// this was written (tools/pe_exports.py against the shipped
// C:/GOG Games/Banished/Runtime-x64.dll) and both disassemblies were transcribed instruction for
// instruction against `PlatformConstructBody` / `PlatformDestructBody`; the
// bodies agree with the other three with no exception at all, which is why this
// adds a descriptor and two wrappers rather than any new logic.
//
// WHY IT IS HERE AND NOT LEFT ON THE DLL. `Graphics::SunLight`'s constructor
// (0x1801081f0, below in runtime/src/graphics/sunlight.cpp) and its `Set`
// rebuild paths both call these two by ADDRESS. Wave 9 lane GFX declined the
// SunLight pair with exactly this as the blocker — "?PlatformConstruct@
// RenderBuffer@Video@@ and ?PlatformDestruct@ are exported and UNPORTED", and
// its recommended order was to land these two first so that porting SunLight
// closes a row rather than moving it one frame deeper. That is what this is.
//
// The oracle is the SAME restore-and-replay the other three use, plus the
// empty-list probe, whose FIRST-IN-LIST branch matters more here than for any
// of the other three: at cutover the mint owns `_firstRenderBuffer` and it
// starts NULL, and the SunLight ctor builds the very first RenderBuffer in the
// process.
// ============================================================================

void Video::RenderBuffer::PlatformConstruct() {
    void** first = RenderBufferFirstRenderBuffer();
    (void)PlatformConstructBody(this, 0x98, offsetof(Video::RenderBuffer, platformData),
                                first, kRenderLinks);
}

void Video::RenderBuffer::PlatformDestruct() {
    void** first = RenderBufferFirstRenderBuffer();
    (void)PlatformDestructBody(this, offsetof(Video::RenderBuffer, platformData),
                               /*nullCompanionAfterRelease=*/false, first, kRenderLinks);
}

void Video_RenderBuffer_PlatformConstruct(RtVideo::RenderBuffer* self) {
    self->Video::RenderBuffer::PlatformConstruct();
}

void Video_RenderBuffer_PlatformDestruct(RtVideo::RenderBuffer* self) {
    self->Video::RenderBuffer::PlatformDestruct();
}

// ============================================================================
// Video::IndexBuffer / Video::VertexBuffer — ctors zero the two owned fields
// then hand off to PlatformConstruct (out of scope — see the resolvers
// above). Create/Set are pure trampolines into the platform buffer object at
// +0x08 (dx11::IndexBufferData / VertexBufferData), same reasoning as
// ImageBuffer's CopyToTexture/CopyFromTexture: nothing on the far side to
// diverge from, since that vtable is dx11's and dx11 is already ours.
// ============================================================================

// ENDGAME B.3.5 block D2. The two constructors stay DOORS and do NOT become real
// `IndexBuffer()` / `VertexBuffer()`: the `PlatformConstruct` they tail into
// publishes `this` into the `_firstBuffer` intrusive list, whose head is an
// `RT_DATA_SLOT` read rather than a C++ static (RULE B4b-4 / B35_RULES §9). What
// converted inside them is the hand-off, now a qualified member call.
void Video_IndexBuffer_Ctor(IndexBuffer* self) {
    self->size = 0;
    self->dynamic = false;
    self->Video::IndexBuffer::PlatformConstruct();
}

void Video_VertexBuffer_Ctor(VertexBuffer* self) {
    self->size = 0;
    self->dynamic = false;
    self->Video::VertexBuffer::PlatformConstruct();
}

// Create/Set are pure trampolines into the companion at +0x08. ENDGAME B.3.5:
// the hand-inlined `*(vtable + 0x08)` / `+ 0x10` dispatches became VIRTUAL CALLS
// on block A's real `Video::{Index,Vertex}BufferData`, whose slots 1 and 2 are
// `Create` and `SetData` (runtime/types/video/CompanionData.h, +0x08 and +0x10 —
// the same two byte offsets, now spelled as the slots they are). `platformData`
// stays a `void*` field, so each receiver takes one marked bridge cast.
bool Video::IndexBuffer::Create(int count, bool isDynamic, void* data) {
    // B.3.5 bridge — platformData is a pinned void* field.
    return reinterpret_cast<Video::IndexBufferData*>(platformData)
               ->Create(static_cast<uint32_t>(count),
                        static_cast<uint8_t>(isDynamic ? 1 : 0), data);
}

// ENDGAME B.3.5 closing pass: the two ints are RENAMED, not reordered, to the
// `(sizeBytes, offsetBytes)` the retail trampoline and dx11's slot-2 consumer both
// say they are -- Buffers.h carries the evidence. The forwarding was already
// correct: `SetData`'s own parameters are spelled `(sizeBytes, offsetBytes, src)`
// in CompanionData.h, so the old names disagreed with the call one line below them.
bool Video::IndexBuffer::Set(int sizeBytes, int offsetBytes, void* data) {
    // B.3.5 bridge — platformData is a pinned void* field.
    return reinterpret_cast<Video::IndexBufferData*>(platformData)
               ->SetData(sizeBytes, offsetBytes, data);
}

bool Video::VertexBuffer::Create(int count, bool isDynamic, void* data) {
    // B.3.5 bridge — platformData is a pinned void* field.
    return reinterpret_cast<Video::VertexBufferData*>(platformData)
               ->Create(static_cast<uint32_t>(count),
                        static_cast<uint8_t>(isDynamic ? 1 : 0), data);
}

bool Video::VertexBuffer::Set(int sizeBytes, int offsetBytes, void* data) {
    // B.3.5 bridge — platformData is a pinned void* field.
    return reinterpret_cast<Video::VertexBufferData*>(platformData)
               ->SetData(sizeBytes, offsetBytes, data);
}

bool Video_IndexBuffer_Create(IndexBuffer* self, int count, bool dynamic, void* data) {
    return self->Video::IndexBuffer::Create(count, dynamic, data);
}

bool Video_IndexBuffer_Set(IndexBuffer* self, int sizeBytes, int offsetBytes, void* data) {
    return self->Video::IndexBuffer::Set(sizeBytes, offsetBytes, data);
}

bool Video_VertexBuffer_Create(VertexBuffer* self, int count, bool dynamic, void* data) {
    return self->Video::VertexBuffer::Create(count, dynamic, data);
}

bool Video_VertexBuffer_Set(VertexBuffer* self, int sizeBytes, int offsetBytes, void* data) {
    return self->Video::VertexBuffer::Set(sizeBytes, offsetBytes, data);
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

    // ---- Wave 9 lane GFX2: the fourth pair ----
    RegisterRuntimeExport("?PlatformConstruct@RenderBuffer@Video@@AEAAXXZ",
                          (void*)&Video_RenderBuffer_PlatformConstruct);
    RegisterRuntimeAddr(0x1800f4320, (void*)&Video_RenderBuffer_PlatformConstruct);
    n++;

    RegisterRuntimeExport("?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ",
                          (void*)&Video_RenderBuffer_PlatformDestruct);
    RegisterRuntimeAddr(0x1800f4400, (void*)&Video_RenderBuffer_PlatformDestruct);
    n++;

    // ?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z — HELD BACK, see above.

    printf("[RtPort] Video::ImageBuffer/IndexBuffer/VertexBuffer (lane C): %d exports registered\n", n);
    return n;
}
