// ============================================================================
// runtime/src/video/device_enum_types.cpp — Wave 18 lane GFXIF
//   Video::Device::Adapter and ::GraphicsInterface (the enumeration value
//   types), plus the two Device methods that build and read them.
// ============================================================================
//
// WHY THIS FILE EXISTS: these are the six functions the cutover boot gate has
// died on since attempt 23. `VideoDX11`'s `DeviceData::EnumerateInterfaces`
// resolves a 40-entry Runtime import table at dx11/src/devicedata.cpp:216-277;
// nine of those entries are the symbols below, `GetImports()` sets `g_imOk =
// false` if ANY entry is null, and the null-import path forwards to
// `Orig<...>(1)` — which with the shipped DLL absent is a null pointer.
// That is the VCALL-of-0. Filling these nine names is what makes GetImports
// succeed. (docs/runtime_port/WAVE16_BOOTGATE.md)
//
// `Dx11ResolveRuntimeImport` (dx11/src/dx11_backend_access.cpp:9) tries
// `ResolveExport(mangled)` FIRST, so registration BY NAME is the registration
// that unblocks the wall. Registration by address is done too, for the 13 live
// sites that reach Runtime symbols through `ResolveAddr(Module::Runtime, …)`.
//
//   | Export                                        | Retail      |
//   |-----------------------------------------------|-------------|
//   | ??0GraphicsInterface@Device@Video@@QEAA@XZ    | 0x18000cd90 |
//   | ??0GraphicsInterface@…@AEBU012@@Z             | 0x18000ce10 |
//   | ??1GraphicsInterface@Device@Video@@QEAA@XZ    | 0x18000cdc0 |
//   | ??0Adapter@Device@Video@@QEAA@XZ              | 0x1800f7100 |
//   | ??0Adapter@Device@Video@@QEAA@AEBU012@@Z      | 0x18000cc70 |
//   | ??1Adapter@Device@Video@@QEAA@XZ              | 0x18000cc10 |
//   | ?FindOrAddMode@Adapter@Device@Video@@…        | 0x1800f7020 |
//   | ?GetDisplayMode@Device@Video@@QEBA?AV…@H@Z    | 0x18000d3e0 |
//
// (`?AdjustDisplay@Device@Video@@QEAAXXZ`, 0x1800f7e20, is the ninth. It is
// pure Win32 and lives in the _win twin, device_adjustdisplay_win.cpp.)
//
// Every address above was read out of the shipped export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with tools/pe_exports.py BEFORE any
// body was written, not copied from a document.
//
// THE ONE ADDRESS THE BRIEF FLAGGED. `??0Adapter@Device@Video@@QEAA@XZ` sits at
// 0x1800f7100, far from the other five (0x18000cc10-0x18000ce10), which raised
// the question of whether it is a thunk. It is NOT: the body at 0x1800f7100 is
// fourteen instructions that zero +0x00..+0x27, store -1 as a QWORD across
// +0x28..+0x2f and -1 as a DWORD at +0x30, and return `this`. Disassembled, not
// decompiled. The split is just which translation unit MSVC emitted each into.
//
// A NEAR-MISS NOT TO REPEAT: dx11/src/devicedata.cpp lists a different address
// beside each of these names (0x1800132d0 for the GraphicsInterface ctor, and
// so on). That column is VideoDX11's OWN IAT SLOT, not the Runtime function —
// every value in it falls inside 0x180013218-0x1800134c8. It is not a
// discrepancy and must not be "fixed".
//
// WHAT THIS FILE DOES NOT DUPLICATE
// ---------------------------------
// Four of the six unexported Collection::Array instantiations these bodies call
// were ALREADY ported by Wave 9 lane VID2 in device_startup.cpp and are
// registered by address there:
//     0x18000fcd0  Array<DisplayMode>::operator=   RtVideo_ArrayModeAssign
//     0x180008b00  Array<bool>::operator=          RtVideo_ArrayBoolAssign
//     0x180010830  Array<DisplayMode>::~Array      RtVideo_ArrayModeDtor
//     0x180009560  Array<bool>::~Array             RtVideo_ArrayBoolDtor
//     0x180010cf0  Array<DisplayMode> elem destroy RtVideo_ArrayModeElemDestroy
//     0x18000fbf0  Array<RefreshRate>::operator=   RtVideo_ArrayRefreshAssign
// They are declared here and CALLED, not rewritten. Porting a second copy would
// be last-binding-wins on the address registry and would silently make one of
// the two bodies dead code.
//
// Five instantiations were NOT already ported, and are added below because the
// nine bodies cannot run without them once the DLL is gone:
//     0x180010da0  Array<Adapter> per-element destroy
//     0x180010900  Array<Adapter>::~Array
//     0x18000fe10  Array<Adapter>::operator=
//     0x180057340  Array<RefreshRate>::SetSize(n, fill)
//     0x180057440  Array<DisplayMode>::SetSize(n, fill)
// None of the five is exported (checked against the export directory, not
// assumed from the mangling — `AEAA` means private, not unexported, and this
// DLL does export its private members). They are template instantiations with
// no name of their own, so they can only be registered by address.
//
// PORTABILITY: no <windows.h> here, per PLAN §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <new>       // ENDGAME B.3.5: the hidden-retslot door placement-news (RULE B5-12)

#include "../../types/system/MemoryAllocator.h"
#include "../../types/video/Device.h"

using Video::Device;
using Adapter           = Video::Device::Adapter;
using DisplayMode       = Video::Device::DisplayMode;
using GraphicsInterface = Video::Device::GraphicsInterface;
using RefreshRate       = Video::Device::RefreshRate;

// The Win32 twin (device_adjustdisplay_win.cpp).
extern "C" void RtVideo_Device_AdjustDisplay(void* self);

namespace {

// ---------------------------------------------------------------------------
// THE GROWTH RULE. Identical in shape to device_startup.cpp's GrowFor, but the
// new size arrives as a PARAMETER here rather than from a source array, and the
// old size is read by the caller before `size` is overwritten. Transcribed from
// 0x180057370-0x1800573a1 (RefreshRate) and 0x18005747a-0x1800574af
// (DisplayMode), which are the same instructions with a different stride.
//
//   if (n > capacity) {
//       capacity = n;
//       if (n == old + 1 && old != 0) capacity = n * 3;   // amortise
//       data = MemAlloc(capacity * stride, align);
//   }
//
// The `n == old + 1 && old != 0` guard is the whole rule: growth by exactly one
// from a non-empty array triples, everything else takes the exact size. It
// decides the heap layout, so it is reproduced rather than simplified.
//
// B.4 (R-VIDEO): the array is its owner's named `data` / `capacity` fields.
// ---------------------------------------------------------------------------
template <class T>
void GrowTo(T*& data, int32_t& capacity, int32_t n, int32_t old, int stride, int align) {
    if (n > capacity) {
        capacity = n;
        if (n == old + 1 && old != 0) capacity = n * 3;
        data = static_cast<T*>(System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(
                   static_cast<int64_t>(capacity) * stride), align));
    }
}

// ---------------------------------------------------------------------------
// The inlined System::String release, in its TWO retail spellings.
//
// A System::String is one pointer to a refcounted char buffer: the refcount is
// the DWORD at [-4] and the allocation base is [-8] (dx11/src/devicedata.cpp:518
// says the same, from Video::Configuration::Startup's own inlined dtor). The
// `name` fields stay `void*` (H5: a real `System::String` member would give the
// value types a destructor), so the header is reached through the one marked
// `string_header` view per access.
//
// Both spellings leave the slot in a state that looks wrong and is retail's:
// on the FREE path the slot is left pointing at the freed allocation base, and
// only on the still-referenced path is it nulled. Reproduced, not tidied — the
// object is being destroyed and no retail reader looks at the slot again.
// ---------------------------------------------------------------------------

// Spelling A — the standalone dtors, 0x18000cde0 (~GraphicsInterface) and
// 0x18000cc3a (~Adapter). Byte-identical to each other.
void StringRelease(void*& slot) {
    void* p = slot;
    if (!p) return;
    int32_t* rc = static_cast<int32_t*>(p) - 1;          // CAST-OK: string_header
    if (--*rc == 0) {
        slot = static_cast<uint8_t*>(slot) - 8;          // CAST-OK: string_header
        System::MemoryAllocator::MemFree(slot);
    } else {
        slot = nullptr;
    }
}

// Spelling B — the copy INSIDE the Array<Adapter> element-destroy loop,
// 0x180010e85. Same code plus ONE extra test: at 0x180010e96 retail does
//     ADD qword ptr [RSI],-0x8
//     MOV RSI,qword ptr [RSI]
//     JZ  <skip the free>
// and the JZ reads the flags of the ADD, not of the MOV (a MOV sets no flags).
// So the free is skipped when the decremented pointer is exactly zero. It is a
// difference of one instruction from spelling A and it is invisible in
// decompiled C, which is why this was read from the disassembly.
void StringReleaseInLoop(void*& slot) {
    void* p = slot;
    if (!p) return;
    int32_t* rc = static_cast<int32_t*>(p) - 1;          // CAST-OK: string_header
    if (--*rc == 0) {
        void* base = static_cast<uint8_t*>(slot) - 8;    // CAST-OK: string_header
        slot = base;
        if (base) System::MemoryAllocator::MemFree(base);
    } else {
        slot = nullptr;
    }
}

// The inlined System::String share, from both copy ctors (0x18000ce29 and
// 0x18000cc95): null the destination first, then adopt and bump the refcount.
void StringShare(void*& dst, void* const& src) {
    dst = nullptr;
    void* p = src;
    if (p) {
        dst = p;
        ++*(static_cast<int32_t*>(p) - 1);               // CAST-OK: string_header
    }
}

}  // namespace

// ============================================================================
// 0x180010da0 — the per-element destroy for Array<Adapter>.
//
// Retail walks with a cursor at element+0x10 (`LEA RBX,[RDX+0x10]`, stride
// 0x38) and destroys each Adapter in the order: the msaa Array<bool> at +0x18,
// then the Array<DisplayMode> at +0x08 (through the shared element-destroy at
// 0x180010cf0), then the String at +0x00. Both array frees are open-coded
// Enter/Free/Leave triples, which IS ?MemFree@MemoryAllocator@System@@ at
// 0x1800c7ad0 (runtime/src/collection/array.cpp:191 established this).
//
// Retail's first parameter is the array object; the callee spills it at
// [RSP+8] and NEVER READS IT — same as the DisplayMode twin — so it is not
// passed here.
// ============================================================================
void RtVideo_ArrayAdapterElemDestroy(Adapter* data, int32_t count) {
    if (count <= 0) return;
    Adapter* e = data;
    for (int64_t i = count; i != 0; --i) {
        // +0x18 Array<bool> — data freed, then size AND capacity cleared by one
        // qword store at +0x20 (0x180010e2b/0x180010e2f, relative to elem+0x10),
        // written as the named fields in address order.
        uint8_t* msaa = e->multisampleData;
        if (msaa) System::MemoryAllocator::MemFree(msaa);
        e->multisampleData  = nullptr;
        e->multisampleCount = 0;
        e->multisampleCap   = 0;

        // +0x08 Array<DisplayMode> — per-element destroy, then the block.
        RtVideo_ArrayModeElemDestroy(e->modeData, e->modeCount);
        DisplayMode* modes = e->modeData;
        if (modes) System::MemoryAllocator::MemFree(modes);
        e->modeData     = nullptr;
        e->modeCount    = 0;
        e->modeCapacity = 0;

        // +0x00 System::String — spelling B (see the note on that helper).
        StringReleaseInLoop(e->name);

        ++e;
    }
}

// ============================================================================
// 0x180010900 — Collection::Array<Adapter>::~Array
// ============================================================================
void RtVideo_ArrayAdapterDtor(Adapter*& data, int32_t& size, int32_t& capacity) {
    RtVideo_ArrayAdapterElemDestroy(data, size);
    if (data) System::MemoryAllocator::MemFree(data);
    data = nullptr;
    size = 0;       // one store clears size AND capacity in retail
    capacity = 0;
}

// ============================================================================
// 0x18000fe10 — Collection::Array<Adapter>::operator=   (stride 0x38, align 8)
//
// Destroy the destination, grow it, then COPY-CONSTRUCT each element with the
// Adapter copy ctor (0x18000cc70) — not a memcpy. Retail calls the copy ctor
// per element at 0x18000fe92.
//
// THE PER-ELEMENT NULL TEST IS ON THE COMPUTED ADDRESS, NOT ON `data`:
//     MOV RCX,[RBX] / ADD RCX,RDX / JZ skip
// the ZF comes from the ADD, so the element is skipped when `data + offset` is
// zero — which with a null `data` (a failed MemAlloc) skips element 0 only and
// copies through null for the rest. Retail's, and reproduced rather than
// guarded: a guard here would change which of the two implementations faults
// first, and the fault is the signal. Same shape as
// device_startup.cpp:RtVideo_ArrayRefreshAssign.
// ============================================================================
void RtVideo_ArrayAdapterAssign(Adapter*& data, int32_t& size, int32_t& capacity,
                                Adapter* const& srcData, const int32_t& srcSize) {
    if (&data == &srcData) return;

    RtVideo_ArrayAdapterDtor(data, size, capacity);

    const int32_t n   = srcSize;
    const int32_t old = size;
    size = n;
    GrowTo(data, capacity, n, old, 0x38, 8);

    for (int32_t i = 0; i < size; ++i) {
        Adapter* d = data + i;
        if (d == nullptr) continue;
        RtVideo_Adapter_CopyCtor(d, srcData + i);
    }
}

// ============================================================================
// 0x180057340 — Collection::Array<RefreshRate>::SetSize(int n, const T& fill)
//                                                       (stride 8, align 4)
//
// Grow or shrink to `n`, filling any NEW element from `fill` and copying the
// surviving ones out of the old block. The `i` cursor starts at 0 when the
// block MOVED (everything must be copied across) and at `old` when it did not
// (the survivors are already in place) — that is the `CMOVZ EAX,ESI` at
// 0x1800573a9, and it is the only reason this reads as one loop instead of two.
//
// RefreshRate is trivially destructible, so a shrink needs no element destroy.
// ============================================================================
void RtVideo_ArrayRefreshSetSize(RefreshRate*& data, int32_t& size, int32_t& capacity,
                                 int32_t n, const RefreshRate& fill) {
    const int32_t old = size;
    if (n == old) return;

    RefreshRate* oldData = data;
    size = n;
    GrowTo(data, capacity, n, old, 8, 4);

    int32_t i = (oldData == data) ? old : 0;
    for (; i < n; ++i) {
        RefreshRate* d = data + i;
        if (d == nullptr) continue;
        const RefreshRate* s = (i < old) ? (oldData + i) : &fill;
        *d = *s;
    }

    if (oldData != data && oldData) System::MemoryAllocator::MemFree(oldData);
}

// ============================================================================
// 0x180057440 — Collection::Array<DisplayMode>::SetSize(int n, const T& fill)
//                                                       (stride 0x18, align 8)
//
// Same skeleton as the RefreshRate twin, with three differences, all of them
// consequences of DisplayMode owning a nested Array<RefreshRate> at +0x08:
//   1. the per-element copy zeroes that nested array before assigning into it
//      (0x180057518/0x18005751b), so the assignment sees an empty destination
//      rather than the old block's pointer;
//   2. when the block MOVED, the old elements are destroyed (0x180057559);
//   3. when it did NOT move and the array SHRANK, only the tail beyond `n` is
//      destroyed (0x1800575b2). Retail has both, and they are exclusive.
// ============================================================================
void RtVideo_ArrayModeSetSize(DisplayMode*& data, int32_t& size, int32_t& capacity,
                              int32_t n, const DisplayMode& fill) {
    const int32_t old = size;
    if (n == old) return;

    DisplayMode* oldData = data;
    size = n;
    GrowTo(data, capacity, n, old, 0x18, 8);

    int32_t i = (oldData == data) ? old : 0;
    for (; i < n; ++i) {
        DisplayMode* d = data + i;
        if (d == nullptr) continue;
        const DisplayMode* s = (i < old) ? (oldData + i) : &fill;

        d->width  = s->width;
        d->height = s->height;

        d->refreshData     = nullptr;
        d->refreshCount    = 0;
        d->refreshCapacity = 0;
        RtVideo_ArrayRefreshAssign(d->refreshData, d->refreshCount, d->refreshCapacity,
                                   s->refreshData, s->refreshCount);
    }

    if (oldData != data) {
        RtVideo_ArrayModeElemDestroy(oldData, old);
        if (oldData) System::MemoryAllocator::MemFree(oldData);
    } else if (old > n) {
        RtVideo_ArrayModeElemDestroy(oldData + n, old - n);
    }
}

// ============================================================================
// ??0GraphicsInterface@Device@Video@@QEAA@XZ      retail 0x18000cd90
//
// Zeroes exactly three qwords — name, adapterData, and the {count, capacity}
// pair as one store (written as the two named fields) — and returns `this`.
// That is the whole body.
// ============================================================================
extern "C" GraphicsInterface* RtVideo_GraphicsInterface_Ctor(GraphicsInterface* self) {
    self->name            = nullptr;
    self->adapterData     = nullptr;
    self->adapterCount    = 0;
    self->adapterCapacity = 0;
    return self;
}

// ============================================================================
// ??0GraphicsInterface@Device@Video@@QEAA@AEBU012@@Z   retail 0x18000ce10
// ============================================================================
extern "C" GraphicsInterface* RtVideo_GraphicsInterface_CopyCtor(GraphicsInterface* dst,
                                                                const GraphicsInterface* src) {
    StringShare(dst->name, src->name);

    // Zero the Array<Adapter> before assigning into it: the assignment starts
    // by destroying its destination, and an uninitialised destination would be
    // destroyed through garbage. Retail's, at 0x18000ce3e/0x18000ce41.
    dst->adapterData     = nullptr;
    dst->adapterCount    = 0;
    dst->adapterCapacity = 0;
    RtVideo_ArrayAdapterAssign(dst->adapterData, dst->adapterCount, dst->adapterCapacity,
                               src->adapterData, src->adapterCount);

    return dst;
}

// ============================================================================
// ??1GraphicsInterface@Device@Video@@QEAA@XZ      retail 0x18000cdc0
//
// Array<Adapter>::~Array on +0x08, then the String at +0x00. In that order.
// ============================================================================
extern "C" void RtVideo_GraphicsInterface_Dtor(GraphicsInterface* self) {
    RtVideo_ArrayAdapterDtor(self->adapterData, self->adapterCount, self->adapterCapacity);
    StringRelease(self->name);
}

// ============================================================================
// ??0Adapter@Device@Video@@QEAA@XZ                retail 0x1800f7100
//
// Zeroes +0x00..+0x27 (five qwords), stores -1 as a QWORD over +0x28..+0x2f and
// -1 as a DWORD at +0x30, returns `this`. +0x34 is NOT written — the ctor
// leaves it, so this port leaves it too. The qword stores are written as the
// named fields they cover, in address order.
//
// This is the body the brief asked to be confirmed as not-a-thunk. It is not;
// see the file banner.
// ============================================================================
extern "C" Adapter* RtVideo_Adapter_Ctor(Adapter* self) {
    self->name             = nullptr;   // +0x00
    self->modeData         = nullptr;   // +0x08
    self->modeCount        = 0;         // +0x10
    self->modeCapacity     = 0;         // +0x14
    self->multisampleData  = nullptr;   // +0x18
    self->multisampleCount = 0;         // +0x20
    self->multisampleCap   = 0;         // +0x24
    self->maxFilterLevel   = -1;        // +0x28 \ one qword -1
    self->dxgiAdapterIndex = -1;        // +0x2c /
    self->dxgiOutputIndex  = -1;        // +0x30
    return self;
}

// ============================================================================
// ??0Adapter@Device@Video@@QEAA@AEBU012@@Z        retail 0x18000cc70
//
// String share, then the two owned arrays (each zeroed then assigned), then
// three plain DWORDs. +0x34 is not copied, matching the ctor.
// ============================================================================
extern "C" Adapter* RtVideo_Adapter_CopyCtor(Adapter* dst, const Adapter* src) {
    StringShare(dst->name, src->name);

    dst->modeData     = nullptr;
    dst->modeCount    = 0;
    dst->modeCapacity = 0;
    RtVideo_ArrayModeAssign(dst->modeData, dst->modeCount, dst->modeCapacity,
                            src->modeData, src->modeCount);

    dst->multisampleData  = nullptr;
    dst->multisampleCount = 0;
    dst->multisampleCap   = 0;
    RtVideo_ArrayBoolAssign(dst->multisampleData, dst->multisampleCount, dst->multisampleCap,
                            src->multisampleData, src->multisampleCount);

    dst->maxFilterLevel   = src->maxFilterLevel;   // +0x28
    dst->dxgiAdapterIndex = src->dxgiAdapterIndex; // +0x2c
    dst->dxgiOutputIndex  = src->dxgiOutputIndex;  // +0x30
    return dst;
}

// ============================================================================
// ??1Adapter@Device@Video@@QEAA@XZ                retail 0x18000cc10
//
// Destruction order is +0x18 (msaa), then +0x08 (modes), then +0x00 (name) —
// the REVERSE of construction, and the reverse of the field order. Read from
// 0x18000cc26/0x18000cc30/0x18000cc3a.
// ============================================================================
extern "C" void RtVideo_Adapter_Dtor(Adapter* self) {
    RtVideo_ArrayBoolDtor(self->multisampleData, self->multisampleCount, self->multisampleCap);
    RtVideo_ArrayModeDtor(self->modeData, self->modeCount, self->modeCapacity);
    StringRelease(self->name);
}

// ============================================================================
// ?FindOrAddMode@Adapter@Device@Video@@QEAAXAEBVDisplayMode@23@AEBVRefreshRate@23@@Z
//                                                 retail 0x1800f7020
//
// Idempotent insert into the adapter's mode list, then into that mode's refresh
// list. Two linear searches, each followed by a SetSize(count + 1, value) when
// the search missed.
//
// TWO THINGS THE DISASSEMBLY SETTLES AND THE DECOMPILED C WOULD NOT:
//
//  1. Both SetSize calls take THREE arguments. `R8` still holds the incoming
//     `mode` pointer at 0x1800f7088 (the search loop uses R9/R10/R11 and never
//     touches R8), so 0x180057440 is called as (array, count+1, mode) — the new
//     element is FILLED with the mode, not default-constructed. A two-argument
//     reading would grow the array and leave the new mode's width and height as
//     whatever the allocator returned, and the mode would then never match on
//     any later call. That is the "drops whole arguments" defect class.
//
//  2. The element pointer is recomputed from `*R14` AFTER the SetSize
//     (0x1800f7094), not carried across it. It has to be: SetSize may
//     reallocate.
//
// The `TEST EDI,EDI / JNS` at 0x1800f7075 is retail checking a loop counter
// that started at 0 and only increments, so it always takes the jump. Kept as
// the unconditional it provably is, rather than transcribed as a live branch.
// ============================================================================
void Video::Device::Adapter::FindOrAddMode(const DisplayMode& mode,
                                           const RefreshRate& rate) {
    int32_t idx = 0;
    bool found = false;
    const int64_t n = modeCount;
    if (n > 0) {
        const DisplayMode* p = modeData;
        for (int64_t i = 0; i < n; ++i, ++idx, ++p) {
            if (p->width == mode.width && p->height == mode.height) { found = true; break; }
        }
    }
    if (!found) {
        idx = modeCount;
        RtVideo_ArrayModeSetSize(modeData, modeCount, modeCapacity, modeCount + 1, mode);
    }

    DisplayMode* m = modeData + idx;   // reloaded: SetSize may have moved it
    bool rfound = false;
    const int64_t rn = m->refreshCount;
    if (rn > 0) {
        const RefreshRate* q = m->refreshData;
        for (int64_t i = 0; i < rn; ++i, ++q) {
            if (q->numerator == rate.numerator &&
                q->denominator == rate.denominator) { rfound = true; break; }
        }
    }
    if (!rfound) {
        RtVideo_ArrayRefreshSetSize(m->refreshData, m->refreshCount, m->refreshCapacity,
                                    m->refreshCount + 1, rate);
    }
}

// ============================================================================
// ?GetDisplayMode@Device@Video@@QEBA?AVDisplayMode@12@H@Z   retail 0x18000d3e0
//
// Returns a DisplayMode BY VALUE, which on this ABI means a HIDDEN RETURN SLOT
// in RDX and the index in R8 — the signature below has three parameters for a
// one-parameter method, and that is not a mistake. A typedef that omits the
// hidden slot writes the return value through whatever was in RDX and still
// hands the caller a plausible answer, which is a wild write that verifies
// green.
//
// The returned mode is a DEEP copy: the embedded Array<RefreshRate> is zeroed
// and then assigned, so the caller owns its own refresh list and must destroy
// it. Retail, 0x18000d41b-0x18000d426.
// ============================================================================
// ENDGAME B.3.5 block D2, RULE B5-12: the MEMBER returns the `DisplayMode` BY
// VALUE, which is what retail's `?AVDisplayMode@12@` mangling says, and GCC's
// Microsoft-ABI lowering gives it the same hidden slot in RDX and the same RAX
// return. The DOOR keeps the explicit three-parameter shape its two registry rows
// have always had, placement-constructs the member's prvalue into the caller's
// slot (C++17 guarantees that is built in place, not copied) and returns the slot
// pointer — `MOV RAX,RDI` at 0x18000d42e.
Video::Device::DisplayMode Video::Device::GetDisplayMode(int index) const {
    const DisplayMode* m = modeData + index;

    DisplayMode out;
    out.width  = m->width;
    out.height = m->height;
    out.refreshData     = nullptr;
    out.refreshCount    = 0;
    out.refreshCapacity = 0;
    RtVideo_ArrayRefreshAssign(out.refreshData, out.refreshCount, out.refreshCapacity,
                               m->refreshData, m->refreshCount);
    return out;
}

// ENDGAME B.3.5 block D2: `AdjustDisplay`'s body is `EnumDisplaySettingsW` and
// `ChangeDisplaySettingsExW` in device_adjustdisplay_win.cpp, a TU that cannot
// include Device.h (PLAN §4.1 decision 1). The member is a one-line forward so
// C++ callers exist; the REGISTERED DOOR stays the `_win` function itself.
void Video::Device::AdjustDisplay() {
    RtVideo_Device_AdjustDisplay(this);
}

