// Phase 4 (lane L1) — the three GPU-buffer resource classes of VideoDX11-x64.dll:
// VertexBufferData, IndexBufferData and ConstantBufferData. 18 vtable slots.
//
//   VertexBufferData    vtable 0x180018830, 7 slots
//   IndexBufferData     vtable 0x1800187c8, 7 slots
//   ConstantBufferData  vtable 0x1800185d0, 4 slots
//
// Why the three live in ONE translation unit
// ------------------------------------------
// Slots 3 and 4 of VertexBufferData and IndexBufferData are *the same two
// addresses* — 0x180003290 (Lock) and 0x180003340 (Unlock) appear verbatim in
// both vtables. The original has one copy of the code; so does this file. The
// two structs are byte-identical in layout (both 0x20, ID3D11Buffer* at +0x10,
// write cursor at +0x18, locked flag at +0x1c), which is what makes a single
// implementation legal — VideoDX11Data.h carries static_asserts that pin that
// equivalence so a future edit to one struct cannot silently break the other.
//
// ConstantBufferData rides along because it is the third member of the
// "buffer upload path" and shares the private array helper at 0x180001550.
//
// How every function here reaches D3D11
// -------------------------------------
// None of these classes caches the device or the context (only ContextData
// does). Each call walks the same chain the original walks:
//
//     Video::Device::_singleton   (Runtime 0x1801fe7a8, a Video::Device*)
//       -> [+0x100]  active Video::DeviceData*   (our DeviceData struct)
//       -> [+0x20]   ID3D11Device*
//       -> [+0x28]   ID3D11DeviceContext*
//
// In the shipped DLL this reads the import slot at VideoDX11 0x1800134b8,
// which the loader fills with the address of that same Runtime variable — see
// "The one global" in decomp/videodx11.md. We resolve the Runtime variable
// directly, which is the same object with one less indirection to get wrong.
//
// ConstantBufferData::Create is the one exception: it takes the Video::Device*
// from its *owner* (Video::ConstantBuffer+0x38) rather than from the singleton.
// Reproduced as written; the two are the same object in practice.
//
// COM calls
// ---------
// Written against <d3d11.h> rather than hand-rolled vtable offsets, but every
// method used was checked against the offset the disassembly calls through,
// using the declaration order of ID3D11DeviceContextVtbl / ID3D11DeviceVtbl:
//
//     [ctx+0x38] = index  7 = VSSetConstantBuffers
//     [ctx+0x70] = index 14 = Map
//     [ctx+0x78] = index 15 = Unmap
//     [ctx+0x80] = index 16 = PSSetConstantBuffers
//     [ctx+0x90] = index 18 = IASetVertexBuffers
//     [ctx+0x98] = index 19 = IASetIndexBuffer
//     [ctx+0x178]= index 47 = CopyResource
//     [ctx+0x180]= index 48 = UpdateSubresource
//     [dev+0x18] = index  3 = ID3D11Device::CreateBuffer
//     [obj+0x10] = index  2 = IUnknown::Release
//
// HRESULT checks are reproduced literally: the original compares `(int)hr == 0`
// (S_OK exactly), not SUCCEEDED(hr). So does this file.
//
// Structured exception handling
// -----------------------------
// The two SetData slots (0x1800031c0 / 0x180003c60) carry an MSVC SEH state
// cookie (`mov [rsp-0x38], -2`) whose unwind funclet tail-calls the
// Video::Device::Unlock thunk at 0x180003060 — i.e. the original guarantees the
// device lock is released if the D3D11 call unwinds. GCC cannot reproduce MSVC
// SEH, and none of the calls between Lock and Unlock can throw (they are COM
// vtable calls into d3d11.dll), so this file uses a plain Lock/…/Unlock pair.
// That is the one deliberate behavioural difference in the file, and it is only
// observable on a path that cannot occur.
//
// Registration
// ------------
// All 18 slots are live and unconditional. They were verified in-game and
// RenderDoc-diffed against the shipped DLL — see dx11/README.md §"Per-draw
// pipeline-state equality".

#include "dx11_seam.h"
#include "VideoDX11Data.h"
#include "dx11_backend_access.h"
// ENDGAME B.3.5 block D2, RULE B5-3: the three real Runtime abstract bases whose
// destructors the three scalar deleting dtors below call by qualified name.
#include "video/CompanionData.h"

#include "resolve.h"

#include <windows.h>
#include <d3d11.h>

#include <cstdio>
#include <mutex>
#include <vector>

namespace VideoDX11 {
namespace {

// ============================================================
// Runtime-x64.dll dependencies
//
// Resolved locally (not in engine/funcs/engine_funcs.h) so this lane cannot
// collide with the other five. Every mangled name below was verified against
// the export-name table of the shipped Runtime-x64.dll.
// ============================================================

struct RuntimeDeps {
    // Video::Device::_singleton — the ADDRESS of the variable, not its value.
    void**  videoDeviceSingleton;

    // ?Lock@Device@Video@@QEAAXXZ   (ordinal 3214)
    void  (*DeviceLock)(void* device);
    // ?Unlock@Device@Video@@QEAAXXZ (ordinal 4462)
    void  (*DeviceUnlock)(void* device);

    // The three Runtime-side base destructors chained by our slot-0 dtors.
    void  (*VertexBufferDataDtor)(void* self);    // ??1VertexBufferData@Video@@UEAA@XZ
    void  (*IndexBufferDataDtor)(void* self);     // ??1IndexBufferData@Video@@UEAA@XZ
    void  (*ConstantBufferDataDtor)(void* self);  // ??1ConstantBufferData@Video@@UEAA@XZ

    // ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z  (static: size, alignment)
    void* (*MemAlloc)(unsigned long long size, int alignment);
    // ?MemFree@MemoryAllocator@System@@SAXPEAX@Z     (static)
    void  (*MemFree)(void* ptr);

    // IO::Serializer, used only by the two Serialize slots.
    const void* (*SerializerGetEncoding)(void* serializer);              // ?GetEncoding@Serializer@IO@@QEAAAEBVEncoding@2@XZ
    void  (*SerializeBool)(void* ser, bool* field, const char* name);    // ?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z
    void  (*SerializeInt)(void* ser, int* field, const char* name);      // ?Serialize@Serializer@IO@@QEAAXAEAHPEBD@Z
    void  (*SerializeBytes)(void* ser, int count, void* data, const char* name); // ?SerializeBytes@Serializer@IO@@QEAAXHPEAXPEBD@Z

    // The three vtable bases in the shipped DLL's .rdata. A destructor stores
    // its own class's vptr before chaining to the base dtor; because the seam
    // patches slots *inside* these arrays rather than replacing the arrays,
    // storing the original address is what keeps our overrides installed.
    void*   vtVertexBufferData;    // 0x180018830
    void*   vtIndexBufferData;     // 0x1800187c8
    void*   vtConstantBufferData;  // 0x1800185d0

    bool    ok;
};

RuntimeDeps g_deps;
bool        g_depsTried = false;

template <typename T>
T Exp(const char* mangled) {
    return reinterpret_cast<T>(ResolveExport(mangled));
}

// Resolve once. Returns false if anything is missing, in which case callers
// forward to the original slot instead of dereferencing a null pointer.
bool EnsureDeps() {
    if (g_depsTried) return g_deps.ok;
    g_depsTried = true;

    g_deps.videoDeviceSingleton =
        reinterpret_cast<void**>(ResolveAddr(Module::Runtime, 0x1801fe7a8));

    g_deps.DeviceLock   = Exp<void (*)(void*)>("?Lock@Device@Video@@QEAAXXZ");
    g_deps.DeviceUnlock = Exp<void (*)(void*)>("?Unlock@Device@Video@@QEAAXXZ");

    g_deps.VertexBufferDataDtor   = Exp<void (*)(void*)>("??1VertexBufferData@Video@@UEAA@XZ");
    g_deps.IndexBufferDataDtor    = Exp<void (*)(void*)>("??1IndexBufferData@Video@@UEAA@XZ");
    g_deps.ConstantBufferDataDtor = Exp<void (*)(void*)>("??1ConstantBufferData@Video@@UEAA@XZ");

    g_deps.MemAlloc = Exp<void* (*)(unsigned long long, int)>(
        "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z");
    g_deps.MemFree  = Exp<void (*)(void*)>("?MemFree@MemoryAllocator@System@@SAXPEAX@Z");

    g_deps.SerializerGetEncoding = Exp<const void* (*)(void*)>(
        "?GetEncoding@Serializer@IO@@QEAAAEBVEncoding@2@XZ");
    g_deps.SerializeBool  = Exp<void (*)(void*, bool*, const char*)>(
        "?Serialize@Serializer@IO@@QEAAXAEA_NPEBD@Z");
    g_deps.SerializeInt   = Exp<void (*)(void*, int*, const char*)>(
        "?Serialize@Serializer@IO@@QEAAXAEAHPEBD@Z");
    g_deps.SerializeBytes = Exp<void (*)(void*, int, void*, const char*)>(
        "?SerializeBytes@Serializer@IO@@QEAAXHPEAXPEBD@Z");

    // The LIVE vtables, not the shipped .rdata arrays at 0x1800188xx. These
    // are our exe-owned arrays and VideoDX11-x64.dll is never mapped, so
    // resolving those addresses directly would store a vptr into nothing.
    g_deps.vtVertexBufferData   = Dx11LiveVtable(Vt::VertexBufferData);
    g_deps.vtIndexBufferData    = Dx11LiveVtable(Vt::IndexBufferData);
    g_deps.vtConstantBufferData = Dx11LiveVtable(Vt::ConstantBufferData);

    g_deps.ok = g_deps.videoDeviceSingleton && g_deps.DeviceLock && g_deps.DeviceUnlock &&
                g_deps.VertexBufferDataDtor && g_deps.IndexBufferDataDtor &&
                g_deps.ConstantBufferDataDtor && g_deps.MemAlloc && g_deps.MemFree &&
                g_deps.SerializerGetEncoding && g_deps.SerializeBool &&
                g_deps.SerializeInt && g_deps.SerializeBytes &&
                g_deps.vtVertexBufferData && g_deps.vtIndexBufferData &&
                g_deps.vtConstantBufferData;

    if (!g_deps.ok) {
        fprintf(stderr, "[Dx11Buffers] Runtime dependency resolution FAILED — all 18 "
                        "slots fall back to the original DLL code\n");
    }
    return g_deps.ok;
}

// ============================================================
// Device walk
// ============================================================

inline DeviceData* ActiveDeviceData() {
    uint8_t* device = reinterpret_cast<uint8_t*>(*g_deps.videoDeviceSingleton);
    return *reinterpret_cast<DeviceData**>(device + 0x100);
}
inline ID3D11DeviceContext* ActiveContext() {
    return reinterpret_cast<ID3D11DeviceContext*>(ActiveDeviceData()->d3dContext);
}
inline ID3D11Device* ActiveDevice() {
    return reinterpret_cast<ID3D11Device*>(ActiveDeviceData()->d3dDevice);
}
inline void* VideoDeviceSingleton() { return *g_deps.videoDeviceSingleton; }

// ============================================================
// Owner-side views (Runtime-x64.dll classes — prefix only)
//
// These are NOT complete class definitions: only the leading fields the DX11
// side touches are known, so there is no sizeof() assert, only offsetof().
// Adding a field here requires a decompiled read/write that proves it.
// ============================================================

// Video::VertexBuffer / Video::IndexBuffer.
// Proven by VertexBufferData/IndexBufferData slots 1, 2, 3 and 6.
struct GpuBufferOwner {
    int32_t sizeBytes;   // +0x00  total byte size; 0 means "nothing to create"
    uint8_t dynamic;     // +0x04  bool; selects D3D11_USAGE_DYNAMIC + CPU write
};
static_assert(offsetof(GpuBufferOwner, sizeBytes) == 0x00, "GpuBufferOwner::sizeBytes");
static_assert(offsetof(GpuBufferOwner, dynamic)   == 0x04, "GpuBufferOwner::dynamic");

// Video::ConstantBuffer. Proven by ConstantBufferData slots 1, 2 and 3.
struct ConstantBufferOwner {
    uint8_t  pad00[0x14];
    int32_t  slotIndex;       // +0x14  D3D11 constant-buffer register (StartSlot)
    uint8_t  bindToVertex;    // +0x18  0 -> PSSetConstantBuffers, else VSSetConstantBuffers
    uint8_t  banked;          // +0x19  0 -> one buffer, else a ladder of growing buffers
    uint8_t  pad1a[2];
    int32_t  registerCount;   // +0x1c  <<4 == total byte size (float4 registers)
    int32_t  bankStride;      // +0x20  bytes per bank in the ladder
    uint8_t  pad24[4];
    void*    entries;         // +0x28  stride 0x18; [+0x10] size in, [+0x14] offset out
    int32_t  entryCount;      // +0x30
    uint8_t  pad34[4];
    void*    device;          // +0x38  Video::Device*
};
static_assert(offsetof(ConstantBufferOwner, slotIndex)     == 0x14, "ConstantBufferOwner::slotIndex");
static_assert(offsetof(ConstantBufferOwner, bindToVertex)  == 0x18, "ConstantBufferOwner::bindToVertex");
static_assert(offsetof(ConstantBufferOwner, banked)        == 0x19, "ConstantBufferOwner::banked");
static_assert(offsetof(ConstantBufferOwner, registerCount) == 0x1c, "ConstantBufferOwner::registerCount");
static_assert(offsetof(ConstantBufferOwner, bankStride)    == 0x20, "ConstantBufferOwner::bankStride");
static_assert(offsetof(ConstantBufferOwner, entries)       == 0x28, "ConstantBufferOwner::entries");
static_assert(offsetof(ConstantBufferOwner, entryCount)    == 0x30, "ConstantBufferOwner::entryCount");
static_assert(offsetof(ConstantBufferOwner, device)        == 0x38, "ConstantBufferOwner::device");

constexpr int kConstantEntryStride     = 0x18;
constexpr int kConstantEntrySizeField  = 0x10;
constexpr int kConstantEntryOffsetField= 0x14;

// IO::Serializer+0x00 is the IO::Stream* it is attached to; +0x08 on that
// stream is an int the DLL compares against 0 and 1.
//
// NOT PROVEN — the enum's name and its full value set are unknown. All that is
// established, from the two Serialize slots themselves, is:
//   0        -> allocate a scratch block, read the payload into it, and rebuild
//               the GPU buffer from it (a load).
//   1        -> CopyResource the live GPU buffer into a STAGING buffer, then map
//               and hand the payload to the serializer (a save).
//   anything -> same as 1 but WITHOUT the CopyResource, so whatever is mapped is
//   else        undefined memory. Most plausibly a size-counting pass, but that
//               is a guess and is not relied on here.
// ENDGAME B.3.4 callers batch H LEFT THIS ONE ALONE. It is the same
// `IO::Serializer::GetDirection()` (retail 0x180006210) that the batch
// converted 61 times elsewhere, but this translation unit includes no
// runtime/types header at all — only dx11's own and <windows.h>/<d3d11.h> —
// so converting it would mean pulling `runtime/types/io/Serializer.h` into the
// VideoDX11 replacement for one expression. That is a module-boundary
// decision, not a caller batch's. Reported instead.
inline int32_t SerializerStreamMode(void* serializer) {
    uint8_t* stream = *reinterpret_cast<uint8_t**>(serializer);
    return *reinterpret_cast<int32_t*>(stream + 8);
}

// ============================================================
// 0x180001550 — generic Array<T*>::SetLength(int)
//
// A shared utility, not a vtable slot. Only ConstantBufferData slot 1 calls it.
// Reproduced exactly, including the three properties that matter:
//   * growth is x3 but ONLY on a +1 append from a non-empty array; any other
//     resize allocates exactly newLen slots,
//   * capacity never shrinks, so the array is not reallocated when shrinking,
//   * NEW slots are left UNINITIALIZED. ConstantBufferData::Create relies on
//     CreateBuffer filling every one of them; if a CreateBuffer fails partway,
//     the tail of the array holds garbage that the destructor will later
//     Release(). That is an original-code hazard, faithfully preserved.
// ============================================================

struct PtrArray {
    void*   data;      // +0x00
    int32_t count;     // +0x08
    int32_t capacity;  // +0x0c
};
static_assert(sizeof(PtrArray) == 0x10, "PtrArray size");
static_assert(offsetof(PtrArray, count)    == 0x08, "PtrArray::count");
static_assert(offsetof(PtrArray, capacity) == 0x0c, "PtrArray::capacity");

// The {buffers, bufferCount, bufferCapacity} triple of ConstantBufferData IS a
// PtrArray; these asserts are what makes the reinterpret_cast below legal.
static_assert(offsetof(ConstantBufferData, bufferCount) -
              offsetof(ConstantBufferData, buffers) == offsetof(PtrArray, count),
              "ConstantBufferData::bufferCount is PtrArray::count");
static_assert(offsetof(ConstantBufferData, bufferCapacity) -
              offsetof(ConstantBufferData, buffers) == offsetof(PtrArray, capacity),
              "ConstantBufferData::bufferCapacity is PtrArray::capacity");

inline PtrArray* BufferArray(ConstantBufferData* self) {
    return reinterpret_cast<PtrArray*>(&self->buffers);
}

void ArraySetLength(PtrArray* a, int newLen) {
    const int oldLen = a->count;
    if (newLen == oldLen) return;

    void* oldData = a->data;
    a->count = newLen;

    if (a->capacity < newLen) {
        a->capacity = newLen;
        if (newLen == oldLen + 1 && oldLen != 0) a->capacity = newLen * 3;
        a->data = g_deps.MemAlloc(
            static_cast<unsigned long long>(static_cast<int64_t>(a->capacity) << 3), 8);
    }

    // When no reallocation happened this loop is empty by construction
    // (start == oldLen and the body requires i < oldLen).
    // The original also tests `&data[i] != nullptr` here — the address of an
    // array element, so the test is dead. Omitted.
    int i = (oldData == a->data) ? oldLen : 0;
    for (; i < newLen; ++i) {
        if (i < oldLen) {
            reinterpret_cast<void**>(a->data)[i] = reinterpret_cast<void**>(oldData)[i];
        }
    }

    if (oldData != a->data) g_deps.MemFree(oldData);
}

// 0x180001520 — free-and-null the same array shape. Reached ONLY from MSVC EH
// unwind funclets, which GCC has no way to install and which cannot fire here
// (nothing between the allocation and its owner's constructor throws).
// Deliberately not ported; recorded so the roster stays complete.

// ============================================================
// Shared: VertexBufferData / IndexBufferData slots 3 and 4
//
// 0x180003290 and 0x180003340 are the same two functions in BOTH vtables. One
// implementation, registered four times. Typed on IndexBufferData; the asserts
// in VideoDX11Data.h prove VertexBufferData has the identical layout.
// ============================================================

// 0x180003290 — Lock(sizeBytes, &outByteOffset) -> mapped pointer, or null.
//
// A classic dynamic ring buffer: NO_OVERWRITE while the write cursor still has
// room, DISCARD (and rewind to 0) once it does not.
void* BufferData_Lock(IndexBufferData* self, int32_t sizeBytes, int32_t* outByteOffset) {
    const GpuBufferOwner* owner = reinterpret_cast<const GpuBufferOwner*>(self->indexBuffer);

    D3D11_MAP mapType;
    if (self->writeCursor + sizeBytes > owner->sizeBytes) {
        mapType = D3D11_MAP_WRITE_DISCARD;         // 4
        self->writeCursor = 0;
    } else {
        mapType = D3D11_MAP_WRITE_NO_OVERWRITE;    // 5
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = ActiveContext()->Map(
        reinterpret_cast<ID3D11Buffer*>(self->buffer), 0, mapType, 0, &mapped);
    if (hr != 0) return nullptr;

    *outByteOffset = self->writeCursor;
    self->locked   = 1;
    // Advance by the request rounded up to an even byte count. The original
    // emits the signed-division idiom for `(n + 1) / 2 * 2`, not an AND mask;
    // for the non-negative sizes this is ever called with the two agree.
    self->writeCursor += ((sizeBytes + 1) / 2) * 2;

    return reinterpret_cast<uint8_t*>(mapped.pData) + *outByteOffset;
}

// 0x180003340 — Unlock()
void BufferData_Unlock(IndexBufferData* self) {
    ActiveContext()->Unmap(reinterpret_cast<ID3D11Buffer*>(self->buffer), 0);
    self->locked = 0;
}

// ============================================================
// Shared: the Create body behind VertexBufferData slot 1 (0x180003b90) and
// IndexBufferData slot 1 (0x1800030f0).
//
// The two are byte-identical apart from BindFlags, so the shape is factored
// here and the two slots below differ only in that one constant. Both write the
// requested size and dynamic flag back onto the OWNER before doing anything
// else — Create is also the setter.
// ============================================================

// `dynamic` is deliberately uint8_t, not bool: the original stores the raw
// incoming byte on the owner and then compares it against 0. Declaring it bool
// would let the compiler assume 0/1 and test only bit 0.
bool GpuBuffer_Create(void* selfBase, void* ownerPtr, void** outBuffer,
                      uint32_t sizeBytes, uint8_t dynamicByte, const void* initialData,
                      UINT bindFlags) {
    (void)selfBase;
    GpuBufferOwner* owner = reinterpret_cast<GpuBufferOwner*>(ownerPtr);
    owner->sizeBytes = static_cast<int32_t>(sizeBytes);
    owner->dynamic   = dynamicByte;

    if (owner->sizeBytes == 0) return true;

    const bool dynamic = (dynamicByte != 0);

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeBytes;
    if (dynamic) {
        desc.Usage          = D3D11_USAGE_DYNAMIC;                       // 2
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;                    // 0x10000
    } else {
        // IMMUTABLE when seeded with data, DEFAULT when not.
        desc.Usage          = initialData ? D3D11_USAGE_IMMUTABLE        // 1
                                          : D3D11_USAGE_DEFAULT;         // 0
        desc.CPUAccessFlags = 0;
    }
    desc.BindFlags              = bindFlags;
    desc.MiscFlags              = 0;
    desc.StructureByteStride    = 0;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem          = initialData;
    init.SysMemPitch      = 0;
    init.SysMemSlicePitch = 0;

    HRESULT hr = ActiveDevice()->CreateBuffer(
        &desc, initialData ? &init : nullptr,
        reinterpret_cast<ID3D11Buffer**>(outBuffer));

    return hr == 0 && *outBuffer != nullptr;
}

// ============================================================
// Shared: the Serialize body behind VertexBufferData slot 6 (0x180003d90) and
// IndexBufferData slot 6 (0x1800033b0). Identical apart from which Create they
// call back into on the load path.
// ============================================================

using RecreateFn = bool (*)(void* self, uint32_t sizeBytes, uint8_t dynamicByte, const void* data);

void GpuBuffer_Serialize(void* self, void* ownerPtr, void** bufferSlot,
                         void* serializer, RecreateFn recreate) {
    const void* encoding = g_deps.SerializerGetEncoding(serializer);

    // encoding->vtable[0](encoding) -> bool. Which virtual this is has NOT been
    // identified; the DLL only ever uses it as an "is this encoding one I can
    // serialize raw bytes through" gate. Called blind, exactly as the original.
    typedef bool (*EncodingQuery_t)(const void*);
    EncodingQuery_t query = *reinterpret_cast<EncodingQuery_t*>(
        *reinterpret_cast<void* const*>(encoding));
    if (!query(encoding)) return;

    GpuBufferOwner* owner = reinterpret_cast<GpuBufferOwner*>(ownerPtr);

    // Order matters: the dynamic flag first, then the size.
    g_deps.SerializeBool(serializer, reinterpret_cast<bool*>(&owner->dynamic), nullptr);
    g_deps.SerializeInt(serializer, &owner->sizeBytes, nullptr);

    const int32_t sizeBytes = owner->sizeBytes;
    if (sizeBytes == 0) return;

    if (SerializerStreamMode(serializer) == 0) {
        // Load: read the payload into scratch memory and rebuild the buffer.
        void* scratch = g_deps.MemAlloc(static_cast<unsigned long long>(sizeBytes), 4);
        g_deps.SerializeBytes(serializer, owner->sizeBytes, scratch, nullptr);
        recreate(self, static_cast<uint32_t>(owner->sizeBytes), owner->dynamic, scratch);
        g_deps.MemFree(scratch);
        return;
    }

    // Save (and the unidentified third mode): pull the contents back through a
    // STAGING buffer.
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth      = static_cast<UINT>(sizeBytes);
    desc.Usage          = D3D11_USAGE_STAGING;                                   // 3
    desc.BindFlags      = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;        // 0x30000
    desc.MiscFlags      = 0;
    desc.StructureByteStride = 0;

    ID3D11Buffer* staging = nullptr;

    g_deps.DeviceLock(VideoDeviceSingleton());

    ID3D11Device*        dev = ActiveDevice();
    ID3D11DeviceContext* ctx = ActiveContext();

    // The original ignores this HRESULT and then unconditionally maps, unmaps
    // and Releases `staging`. On a failed CreateBuffer that is a null
    // dereference. Reproduced rather than "fixed" — silently diverging here
    // would hide a real failure mode from the RenderDoc/A-B comparison.
    dev->CreateBuffer(&desc, nullptr, &staging);

    if (SerializerStreamMode(serializer) == 1) {
        ctx->CopyResource(staging, reinterpret_cast<ID3D11Buffer*>(*bufferSlot));
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    ctx->Map(staging, 0, D3D11_MAP_READ_WRITE, 0, &mapped);                      // 3
    g_deps.SerializeBytes(serializer, owner->sizeBytes, mapped.pData, nullptr);
    ctx->Unmap(staging, 0);
    staging->Release();

    g_deps.DeviceUnlock(VideoDeviceSingleton());
}

// ============================================================
// Shared: the SetData body behind VertexBufferData slot 2 (0x180003c60) and
// IndexBufferData slot 2 (0x1800031c0).
//
// !! The two are byte-identical EXCEPT for one instruction:
//        VertexBufferData  xor r8d, r8d   -> UpdateSubresource DstSubresource = 0
//        IndexBufferData   mov r8d, edi   -> UpdateSubresource DstSubresource = offsetBytes
//    Passing a byte offset as a subresource index is meaningless for a buffer
//    (buffers have exactly one subresource, index 0) and is almost certainly a
//    copy-paste bug in the shipped DLL. It is reproduced verbatim: the D3D11
//    debug layer will complain about it under the original DLL too, and making
//    ours "correct" would make an A/B capture diverge for the wrong reason.
//    See the dstSubresource parameter at each call site below.
// ============================================================

bool GpuBuffer_SetData(void* ownerPtr, void* buffer,
                       int32_t sizeBytes, int32_t offsetBytes, const void* src,
                       UINT dstSubresource) {
    const GpuBufferOwner* owner = reinterpret_cast<const GpuBufferOwner*>(ownerPtr);
    if (owner->sizeBytes == 0) return true;

    g_deps.DeviceLock(VideoDeviceSingleton());

    ID3D11DeviceContext* ctx = ActiveContext();

    D3D11_BOX box;
    box.left   = static_cast<UINT>(offsetBytes);
    box.top    = 0;
    box.front  = 0;
    box.right  = static_cast<UINT>(sizeBytes + offsetBytes);
    box.bottom = 1;
    box.back   = 1;

    ctx->UpdateSubresource(reinterpret_cast<ID3D11Buffer*>(buffer), dstSubresource, &box,
                           src, static_cast<UINT>(sizeBytes), 0);

    g_deps.DeviceUnlock(VideoDeviceSingleton());
    return true;
}

// ============================================================
// VertexBufferData — vtable 0x180018830
// ============================================================

bool VertexBufferData_Create(void* self, uint32_t sizeBytes, uint8_t dynamicByte, const void* data);

// 0x180003b20 — slot 0: scalar deleting destructor.
VertexBufferData* VertexBufferData_ScalarDeletingDtor(VertexBufferData* self, unsigned flags) {
    if (!EnsureDeps()) {
        typedef VertexBufferData* (*Orig_t)(VertexBufferData*, unsigned);
        return reinterpret_cast<Orig_t>(
            Dx11OriginalSlot(Vt::VertexBufferData, 0))(self, flags);
    }

    self->vptr = g_deps.vtVertexBufferData;
    if (self->buffer) reinterpret_cast<ID3D11Buffer*>(self->buffer)->Release();
    self->buffer      = nullptr;
    self->writeCursor = 0;
    self->locked      = 0;
    // RULE B5-3: a QUALIFIED NON-VIRTUAL call to the real Runtime base's
    // destructor, never `DeletingDtor(0)` — that would re-enter this slot. The
    // cast is the bridge the declared leave (RULE B5-8) requires: this class
    // mirrors the base's two fields rather than deriving from it in C++.
    reinterpret_cast<Video::VertexBufferData*>(self)
        ->Video::VertexBufferData::~VertexBufferData();   // B.3.5 bridge
    if (flags & 1) g_deps.MemFree(self);
    return self;
}

// 0x180003b90 — slot 1: Create(sizeBytes, dynamic, initialData) -> bool.
// BindFlags = D3D11_BIND_VERTEX_BUFFER (1).
bool VertexBufferData_Create(void* selfV, uint32_t sizeBytes, uint8_t dynamicByte, const void* data) {
    VertexBufferData* self = reinterpret_cast<VertexBufferData*>(selfV);
    if (!EnsureDeps()) {
        typedef bool (*Orig_t)(void*, uint32_t, uint8_t, const void*);
        return reinterpret_cast<Orig_t>(
            Dx11OriginalSlot(Vt::VertexBufferData, 1))(selfV, sizeBytes, dynamicByte, data);
    }
    return GpuBuffer_Create(self, self->vertexBuffer, &self->buffer,
                            sizeBytes, dynamicByte, data, D3D11_BIND_VERTEX_BUFFER);
}

// 0x180003c60 — slot 2: SetData(sizeBytes, offsetBytes, src) -> bool.
// DstSubresource is a hard 0 here (contrast IndexBufferData slot 2).
bool VertexBufferData_SetData(VertexBufferData* self, int32_t sizeBytes,
                              int32_t offsetBytes, const void* src) {
    if (!EnsureDeps()) {
        typedef bool (*Orig_t)(VertexBufferData*, int32_t, int32_t, const void*);
        return reinterpret_cast<Orig_t>(
            Dx11OriginalSlot(Vt::VertexBufferData, 2))(self, sizeBytes, offsetBytes, src);
    }
    return GpuBuffer_SetData(self->vertexBuffer, self->buffer,
                             sizeBytes, offsetBytes, src, /*dstSubresource=*/0);
}

// 0x180003d30 — slot 5: Bind(?, startSlot, offsetBytes, strideBytes).
//
// The second parameter (RDX) is never read by the original — it is overwritten
// with the Video::Device pointer three instructions in. Almost certainly the
// Video::Context* every other Bind in the DLL is handed and ignores.
void VertexBufferData_Bind(VertexBufferData* self, void* /*context, unused*/,
                           uint32_t startSlot, uint32_t offsetBytes, uint32_t strideBytes) {
    if (!EnsureDeps()) {
        typedef void (*Orig_t)(VertexBufferData*, void*, uint32_t, uint32_t, uint32_t);
        reinterpret_cast<Orig_t>(Dx11OriginalSlot(Vt::VertexBufferData, 5))(
            self, nullptr, startSlot, offsetBytes, strideBytes);
        return;
    }
    // The original passes the addresses of its own R9 home slot (offset) and of
    // stack argument 5 (stride); taking the address of our parameters is the
    // same observable call.
    UINT stride = strideBytes;
    UINT offset = offsetBytes;
    ActiveContext()->IASetVertexBuffers(
        startSlot, 1, reinterpret_cast<ID3D11Buffer**>(&self->buffer), &stride, &offset);
}

// 0x180003d90 — slot 6: Serialize(IO::Serializer*).
void VertexBufferData_Serialize(VertexBufferData* self, void* serializer) {
    if (!EnsureDeps()) {
        typedef void (*Orig_t)(VertexBufferData*, void*);
        reinterpret_cast<Orig_t>(
            Dx11OriginalSlot(Vt::VertexBufferData, 6))(self, serializer);
        return;
    }
    GpuBuffer_Serialize(self, self->vertexBuffer, &self->buffer, serializer,
                        &VertexBufferData_Create);
}

// ============================================================
// IndexBufferData — vtable 0x1800187c8
// ============================================================

bool IndexBufferData_Create(void* self, uint32_t sizeBytes, uint8_t dynamicByte, const void* data);

// 0x180003080 — slot 0: scalar deleting destructor.
IndexBufferData* IndexBufferData_ScalarDeletingDtor(IndexBufferData* self, unsigned flags) {
    if (!EnsureDeps()) {
        typedef IndexBufferData* (*Orig_t)(IndexBufferData*, unsigned);
        return reinterpret_cast<Orig_t>(
            Dx11OriginalSlot(Vt::IndexBufferData, 0))(self, flags);
    }

    self->vptr = g_deps.vtIndexBufferData;
    if (self->buffer) reinterpret_cast<ID3D11Buffer*>(self->buffer)->Release();
    self->buffer      = nullptr;
    self->writeCursor = 0;
    self->locked      = 0;
    // RULE B5-3, as for VertexBufferData above.
    reinterpret_cast<Video::IndexBufferData*>(self)
        ->Video::IndexBufferData::~IndexBufferData();     // B.3.5 bridge
    if (flags & 1) g_deps.MemFree(self);
    return self;
}

// 0x1800030f0 — slot 1: Create(sizeBytes, dynamic, initialData) -> bool.
// BindFlags = D3D11_BIND_INDEX_BUFFER (2) — the ONLY difference from the
// VertexBufferData version.
bool IndexBufferData_Create(void* selfV, uint32_t sizeBytes, uint8_t dynamicByte, const void* data) {
    IndexBufferData* self = reinterpret_cast<IndexBufferData*>(selfV);
    if (!EnsureDeps()) {
        typedef bool (*Orig_t)(void*, uint32_t, uint8_t, const void*);
        return reinterpret_cast<Orig_t>(
            Dx11OriginalSlot(Vt::IndexBufferData, 1))(selfV, sizeBytes, dynamicByte, data);
    }
    return GpuBuffer_Create(self, self->indexBuffer, &self->buffer,
                            sizeBytes, dynamicByte, data, D3D11_BIND_INDEX_BUFFER);
}

// 0x1800031c0 — slot 2: SetData(sizeBytes, offsetBytes, src) -> bool.
//
// DstSubresource = offsetBytes. See the warning on GpuBuffer_SetData: this is
// the shipped DLL's behaviour, bug and all.
bool IndexBufferData_SetData(IndexBufferData* self, int32_t sizeBytes,
                             int32_t offsetBytes, const void* src) {
    if (!EnsureDeps()) {
        typedef bool (*Orig_t)(IndexBufferData*, int32_t, int32_t, const void*);
        return reinterpret_cast<Orig_t>(
            Dx11OriginalSlot(Vt::IndexBufferData, 2))(self, sizeBytes, offsetBytes, src);
    }
    return GpuBuffer_SetData(self->indexBuffer, self->buffer,
                             sizeBytes, offsetBytes, src,
                             /*dstSubresource=*/static_cast<UINT>(offsetBytes));
}

// 0x180003380 — slot 5: Bind().
//
// Not a Ghidra function: a 45-byte body sitting immediately after Unlock
// (0x180003340) that the analyser folded into its predecessor. Decoded by hand
// from `read_memory(0x180003380, 64)`:
//   48 8b 05 ..   mov  rax, [rip+0x10131]   ; -> Video::Device::_singleton
//   4c 8b d1      mov  r10, rcx             ; this
//   45 33 c9      xor  r9d, r9d             ; Offset = 0
//   48 8b 10      mov  rdx, [rax]
//   45 8d 41 39   lea  r8d, [r9+0x39]       ; Format = 0x39 = DXGI_FORMAT_R16_UINT
//   ...
//   48 ff a0 98.. jmp  [rax+0x98]           ; IASetIndexBuffer
//
// The hard-coded R16_UINT is what proves Banished indices are always 16-bit.
void IndexBufferData_Bind(IndexBufferData* self) {
    if (!EnsureDeps()) {
        typedef void (*Orig_t)(IndexBufferData*);
        reinterpret_cast<Orig_t>(Dx11OriginalSlot(Vt::IndexBufferData, 5))(self);
        return;
    }
    ActiveContext()->IASetIndexBuffer(
        reinterpret_cast<ID3D11Buffer*>(self->buffer), DXGI_FORMAT_R16_UINT, 0);
}

// 0x1800033b0 — slot 6: Serialize(IO::Serializer*).
void IndexBufferData_Serialize(IndexBufferData* self, void* serializer) {
    if (!EnsureDeps()) {
        typedef void (*Orig_t)(IndexBufferData*, void*);
        reinterpret_cast<Orig_t>(
            Dx11OriginalSlot(Vt::IndexBufferData, 6))(self, serializer);
        return;
    }
    GpuBuffer_Serialize(self, self->indexBuffer, &self->buffer, serializer,
                        &IndexBufferData_Create);
}

// ============================================================
// Shared slot 3 / slot 4 entry points
//
// Registered for BOTH vtables. Typed on IndexBufferData purely because a type
// had to be picked; the static_asserts in VideoDX11Data.h make the choice a
// formality.
// ============================================================

void* BufferData_Lock_Entry(IndexBufferData* self, int32_t sizeBytes, int32_t* outByteOffset) {
    if (!EnsureDeps()) {
        typedef void* (*Orig_t)(IndexBufferData*, int32_t, int32_t*);
        return reinterpret_cast<Orig_t>(
            Dx11OriginalSlot(Vt::IndexBufferData, 3))(self, sizeBytes, outByteOffset);
    }
    return BufferData_Lock(self, sizeBytes, outByteOffset);
}

void BufferData_Unlock_Entry(IndexBufferData* self) {
    if (!EnsureDeps()) {
        typedef void (*Orig_t)(IndexBufferData*);
        reinterpret_cast<Orig_t>(Dx11OriginalSlot(Vt::IndexBufferData, 4))(self);
        return;
    }
    BufferData_Unlock(self);
}

// ============================================================
// ConstantBufferData — vtable 0x1800185d0
// ============================================================

// 0x180001270 — ~ConstantBufferData. Not a vtable slot; tail-called by slot 0.
void ConstantBufferData_Destruct(ConstantBufferData* self) {
    Dx11UntrackConstantBuffer(self);
    self->vptr = g_deps.vtConstantBufferData;
    for (int i = 0; i < self->bufferCount; ++i) {
        ID3D11Buffer* b = reinterpret_cast<ID3D11Buffer**>(self->buffers)[i];
        if (b) b->Release();
    }
    // Unconditional — MemFree(nullptr) is the allocator's problem, as in the
    // original, which does not null-check either.
    g_deps.MemFree(self->buffers);
    self->buffers = nullptr;
    // One qword store clears count and capacity together, exactly as compiled.
    self->bufferCount    = 0;
    self->bufferCapacity = 0;
    // RULE B5-3, as for the other two.
    reinterpret_cast<Video::ConstantBufferData*>(self)
        ->Video::ConstantBufferData::~ConstantBufferData();  // B.3.5 bridge
}

// 0x180001240 — slot 0: scalar deleting destructor.
ConstantBufferData* ConstantBufferData_ScalarDeletingDtor(ConstantBufferData* self,
                                                          unsigned flags) {
    if (!EnsureDeps()) {
        typedef ConstantBufferData* (*Orig_t)(ConstantBufferData*, unsigned);
        return reinterpret_cast<Orig_t>(
            Dx11OriginalSlot(Vt::ConstantBufferData, 0))(self, flags);
    }
    ConstantBufferData_Destruct(self);
    if (flags & 1) g_deps.MemFree(self);
    return self;
}

// 0x180001300 — slot 1: Create() -> bool.
//
// Two jobs:
//   1. Walk the owner's entry table and stamp each entry's byte offset
//      (entry[i].[0x14]) as the running sum of the preceding entry sizes
//      (entry[i].[0x10]). This is a layout pass, not a GPU operation.
//   2. Allocate the ID3D11Buffer(s). Un-banked owners get exactly one buffer of
//      the full size. Banked owners get a LADDER: buffer i has size
//      bankStride*(i+1), so a partial update can bind the smallest buffer that
//      still holds the data — that is what slot 2's `size/bankStride - 1` index
//      selects.
//
// Note this slot takes its ID3D11Device from the OWNER (Video::ConstantBuffer
// +0x38 -> Video::Device -> +0x100 -> +0x20), not from the module singleton.
bool ConstantBufferData_Create(ConstantBufferData* self) {
    if (!EnsureDeps()) {
        typedef bool (*Orig_t)(ConstantBufferData*);
        return reinterpret_cast<Orig_t>(
            Dx11OriginalSlot(Vt::ConstantBufferData, 1))(self);
    }

    ConstantBufferOwner* owner =
        reinterpret_cast<ConstantBufferOwner*>(self->constantBuffer);

    // 1. offsets. (The original re-loads owner->entries and owner->entryCount
    // on every iteration; neither can change, so the loop is hoisted here.)
    int32_t running = 0;
    for (int i = 0; i < owner->entryCount; ++i) {
        uint8_t* entry = reinterpret_cast<uint8_t*>(owner->entries) +
                         static_cast<int64_t>(i) * kConstantEntryStride;
        *reinterpret_cast<int32_t*>(entry + kConstantEntryOffsetField) = running;
        running += *reinterpret_cast<int32_t*>(entry + kConstantEntrySizeField);
    }

    const int32_t totalBytes = owner->registerCount << 4;

    uint8_t* ownerDevice = reinterpret_cast<uint8_t*>(owner->device);
    DeviceData* dd = *reinterpret_cast<DeviceData**>(ownerDevice + 0x100);
    ID3D11Device* dev = reinterpret_cast<ID3D11Device*>(dd->d3dDevice);
    // No device yet -> nothing to build, and the original reports success.
    if (!dev) return true;

    HRESULT hr;
    if (!owner->banked) {
        ArraySetLength(BufferArray(self), 1);

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth           = static_cast<UINT>(totalBytes);
        desc.Usage               = D3D11_USAGE_DEFAULT;               // 0
        desc.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;        // 4
        desc.CPUAccessFlags      = 0;
        desc.MiscFlags           = 0;
        desc.StructureByteStride = 0;

        hr = dev->CreateBuffer(&desc, nullptr,
                               reinterpret_cast<ID3D11Buffer**>(self->buffers));
    } else {
        ArraySetLength(BufferArray(self), totalBytes / owner->bankStride);

        // Seeded to a failure value so an empty ladder reports false, matching
        // the `or eax, 0xffffffff` before the loop.
        hr = -1;
        for (int i = 0; i < self->bufferCount; ++i) {
            D3D11_BUFFER_DESC desc = {};
            desc.ByteWidth           = static_cast<UINT>(owner->bankStride * (i + 1));
            desc.Usage               = D3D11_USAGE_DEFAULT;
            desc.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags      = 0;
            desc.MiscFlags           = 0;
            desc.StructureByteStride = 0;

            hr = dev->CreateBuffer(&desc, nullptr,
                                   reinterpret_cast<ID3D11Buffer**>(self->buffers) + i);
            if (hr != 0) break;
        }
    }
    return hr == 0;
}

// 0x180001460 — slot 2: Bind/Update(?, srcData, ?, sizeBytes).
//
// Parameters 2 (RDX) and 4 (R9) are never read by the original. The payload
// pointer arrives in R8 and the byte count as stack argument 5.
//
// Picks the ladder rung that fits, re-binds only when the rung changed (that
// `activeIndex != idx` guard is the whole point of the class), then pushes the
// payload with a full-resource UpdateSubresource.
void ConstantBufferData_Bind(ConstantBufferData* self, void* /*unused*/,
                             const void* srcData, void* /*unused*/, int32_t sizeBytes) {
    if (!EnsureDeps()) {
        typedef void (*Orig_t)(ConstantBufferData*, void*, const void*, void*, int32_t);
        reinterpret_cast<Orig_t>(Dx11OriginalSlot(Vt::ConstantBufferData, 2))(
            self, nullptr, srcData, nullptr, sizeBytes);
        return;
    }

    ConstantBufferOwner* owner =
        reinterpret_cast<ConstantBufferOwner*>(self->constantBuffer);
    ID3D11DeviceContext* ctx = ActiveContext();

    const int32_t index = owner->banked ? (sizeBytes / owner->bankStride - 1) : 0;

    if (self->activeIndex != index) {
        self->activeIndex = index;
        ID3D11Buffer** slot = reinterpret_cast<ID3D11Buffer**>(self->buffers) + index;
        if (owner->bindToVertex == 0) {
            ctx->PSSetConstantBuffers(static_cast<UINT>(owner->slotIndex), 1, slot);
        } else {
            ctx->VSSetConstantBuffers(static_cast<UINT>(owner->slotIndex), 1, slot);
        }
    }

    ctx->UpdateSubresource(
        reinterpret_cast<ID3D11Buffer**>(self->buffers)[self->activeIndex],
        0, nullptr, srcData, 0, 0);
}

// 0x180001450 — slot 3: `return owner->slotIndex + 1`.
//
// Not a Ghidra function; a 10-byte tail after ~ConstantBufferData. Bytes:
//   48 8b 41 08   mov eax-path: rax = [rcx+8]     ; owner
//   8b 40 14      mov eax, [rax+0x14]             ; owner->slotIndex
//   ff c0         inc eax
//   c3            ret
//
// The Runtime-side name is unknown. Given +0x14 is the StartSlot handed to
// VS/PSSetConstantBuffers, "index + 1" reads as a required-slot-count query,
// but that is inference, not evidence.
int32_t ConstantBufferData_SlotIndexPlusOne(ConstantBufferData* self) {
    // No Runtime dependency and no D3D11 call, so no fallback path is needed.
    const ConstantBufferOwner* owner =
        reinterpret_cast<const ConstantBufferOwner*>(self->constantBuffer);
    return owner->slotIndex + 1;
}

}  // namespace

// ============================================================
// Constant-buffer binding registry — see dx11_backend_access.h.
//
// DELIBERATE DEVIATION from the shipped DLL. ConstantBufferData::Bind's
// `activeIndex != index` guard (0x180001460) caches "this rung is already bound
// to my shader slot". DeviceData::Resize (0x180004340) calls
// ID3D11DeviceContext::ClearState(), which drops every constant-buffer binding
// on the floor while leaving `activeIndex` alone — so the guard skips the
// rebind on every subsequent frame and the shaders read a NULL constant buffer
// from then on. Zeroed transform matrices collapse every vertex, nothing
// rasterises, and the window stays black until the process exits. Measured:
// after a resize the same ConstantBufferData objects come back with
// activeIndex == index == 0 and VS/PSSetConstantBuffers is never called again.
//
// The shipped DLL has the same hole; it only stays hidden while something keeps
// changing the rung (a differently sized update) and re-binding as a side
// effect. Resetting the cache where the bindings are actually dropped is the
// narrowest correct fix, and it costs one rebind per buffer per device reset
// rather than a rebind per update, so the guard keeps doing its job.
//
// Touched on create, on destroy, and once per device reset — never per draw.
namespace {
std::mutex&                       CbRegistryLock() { static std::mutex m; return m; }
std::vector<ConstantBufferData*>& CbRegistry()     { static std::vector<ConstantBufferData*> v; return v; }
}  // namespace

void Dx11TrackConstantBuffer(ConstantBufferData* p) {
    if (!p) return;
    std::lock_guard<std::mutex> lock(CbRegistryLock());
    CbRegistry().push_back(p);
}

void Dx11UntrackConstantBuffer(ConstantBufferData* p) {
    if (!p) return;
    std::lock_guard<std::mutex> lock(CbRegistryLock());
    std::vector<ConstantBufferData*>& v = CbRegistry();
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == p) { v[i] = v.back(); v.pop_back(); return; }
    }
}

void Dx11InvalidateConstantBufferBindings() {
    std::lock_guard<std::mutex> lock(CbRegistryLock());
    for (ConstantBufferData* p : CbRegistry())
        p->activeIndex = -1;   // the factory's "no rung bound yet" sentinel
}

}  // namespace VideoDX11

// ============================================================
// Registration — 18 slots, live.
//
// Slots 3 and 4 register the SAME two functions against both vtables, which is
// exactly what the original does by putting the same address in both.
// ============================================================

DX11_REGISTER_SLOT(VertexBufferData, 0, &::VideoDX11::VertexBufferData_ScalarDeletingDtor,
                   "VertexBufferData::ScalarDeletingDtor");
DX11_REGISTER_SLOT(VertexBufferData, 1, &::VideoDX11::VertexBufferData_Create,
                   "VertexBufferData::Create");
DX11_REGISTER_SLOT(VertexBufferData, 2, &::VideoDX11::VertexBufferData_SetData,
                   "VertexBufferData::SetData");
DX11_REGISTER_SLOT(VertexBufferData, 3, &::VideoDX11::BufferData_Lock_Entry,
                   "VertexBufferData::Lock (shared)");
DX11_REGISTER_SLOT(VertexBufferData, 4, &::VideoDX11::BufferData_Unlock_Entry,
                   "VertexBufferData::Unlock (shared)");
DX11_REGISTER_SLOT(VertexBufferData, 5, &::VideoDX11::VertexBufferData_Bind,
                   "VertexBufferData::Bind");
DX11_REGISTER_SLOT(VertexBufferData, 6, &::VideoDX11::VertexBufferData_Serialize,
                   "VertexBufferData::Serialize");

DX11_REGISTER_SLOT(IndexBufferData, 0, &::VideoDX11::IndexBufferData_ScalarDeletingDtor,
                   "IndexBufferData::ScalarDeletingDtor");
DX11_REGISTER_SLOT(IndexBufferData, 1, &::VideoDX11::IndexBufferData_Create,
                   "IndexBufferData::Create");
DX11_REGISTER_SLOT(IndexBufferData, 2, &::VideoDX11::IndexBufferData_SetData,
                   "IndexBufferData::SetData");
DX11_REGISTER_SLOT(IndexBufferData, 3, &::VideoDX11::BufferData_Lock_Entry,
                   "IndexBufferData::Lock (shared)");
DX11_REGISTER_SLOT(IndexBufferData, 4, &::VideoDX11::BufferData_Unlock_Entry,
                   "IndexBufferData::Unlock (shared)");
DX11_REGISTER_SLOT(IndexBufferData, 5, &::VideoDX11::IndexBufferData_Bind,
                   "IndexBufferData::Bind");
DX11_REGISTER_SLOT(IndexBufferData, 6, &::VideoDX11::IndexBufferData_Serialize,
                   "IndexBufferData::Serialize");

DX11_REGISTER_SLOT(ConstantBufferData, 0, &::VideoDX11::ConstantBufferData_ScalarDeletingDtor,
                   "ConstantBufferData::ScalarDeletingDtor");
DX11_REGISTER_SLOT(ConstantBufferData, 1, &::VideoDX11::ConstantBufferData_Create,
                   "ConstantBufferData::Create");
DX11_REGISTER_SLOT(ConstantBufferData, 2, &::VideoDX11::ConstantBufferData_Bind,
                   "ConstantBufferData::Bind");
DX11_REGISTER_SLOT(ConstantBufferData, 3, &::VideoDX11::ConstantBufferData_SlotIndexPlusOne,
                   "ConstantBufferData::SlotIndex+1");
