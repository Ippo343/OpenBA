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

#include <cstdio>

// ============================================================================
// Resolving the DLL's own helpers this lane's bodies call into but does not
// reimplement (out of scope — see each call site). RT_ORIGINAL_ONCE resolves
// once per call site and caches (rt_port.h); these are never on a hot path
// except the buffer/image trampolines, which is exactly why they are cached
// rather than resolved per call.
// ============================================================================
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
    static void* v = ResolveExportRaw("??_7ImageBuffer@Video@@6B@");
    return v;
}
void* ImageBufferDataVtable() {
    static void* v = ResolveExportRaw("??_7ImageBufferData@Video@@6B@");
    return v;
}

// Video::Device::_singleton / Video::Configuration::_singleton — data
// exports, resolved once and cached exactly like a function pointer. Both are
// read verbatim by Width()/Height(); lane B owns Video::Device's own body,
// not these two field reads.
void** DeviceSingletonPtr() {
    static void** p = reinterpret_cast<void**>(ResolveExportRaw("?_singleton@Device@Video@@0PEAV12@EA"));
    return p;
}
void** ConfigurationSingletonPtr() {
    static void** p = reinterpret_cast<void**>(ResolveExportRaw("?_singleton@Configuration@Video@@0PEAV12@EA"));
    return p;
}

}  // namespace

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

    ImageBufferPlatformConstruct()(self);

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
    ImageBufferPlatformDestruct()(self);

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
    static void* compiledExternalVtable = ResolveExportRaw("??_7CompiledExternal@Object@@6B@");
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
    IndexBufferPlatformConstruct()(self);
    RT_DIFF_SHADOW(self->size == 0 && self->dynamic == false, "ctor fields set",
                   "retail constants (0, false)");
}

void Video_VertexBuffer_Ctor(VertexBuffer* self) {
    RT_DIFF_BEGIN("Video::VertexBuffer::VertexBuffer");
    self->size = 0;
    self->dynamic = false;
    VertexBufferPlatformConstruct()(self);
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

    // ?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z — HELD BACK, see above.

    printf("[RtPort] Video::ImageBuffer/IndexBuffer/VertexBuffer (lane C): %d exports registered\n", n);
    return n;
}
