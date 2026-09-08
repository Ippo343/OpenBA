// ============================================================================
// runtime/src/object/internal.cpp — Object::Internal / InternalBasePtr /
//                                   InternalFactory   (Wave 3, lane B)
// ============================================================================
//
// Byte-faithful port of the decompilation at the retail address named above
// each function (Runtime-x64.dll, image base 0x180000000, read 2026-09-06).
// Layouts, evidence and the vtable dump are in
// runtime/types/object/Internal.h — read that first.
// The full write-up, including the GO/NO-GO verdict, is
// docs/runtime_port/WAVE3_LANE_B.md.
//
// ORIGINAL FILE NAME. The profile build's source-tree strings name
// `Object\InternalFactory.cpp` and the headers `Object/InternalFactory.h` and
// `Object/InternalPtr.h`. There is no `Object\Internal.cpp`. This file's name
// is the wave scaffolding's, not the engine's.
//
// ----------------------------------------------------------------------------
// THE ONE THING THAT MAKES THIS LANE SAFE
// ----------------------------------------------------------------------------
// PLAN.md §4.3 risk 3 says a half-ported class with two vtables in play is the
// swap-remove UAF family again. That failure mode DOES NOT EXIST here, for the
// same reason it did not exist for the Wave 2 allocator, and for one extra
// reason of its own:
//
//   * There is ONE table. Every function below reaches the LIVE
//     `?_singleton@InternalFactory@Object@@0PEAV12@EA` and mutates the DLL's
//     own arrays in place. No replica, no second table, no second heap — the
//     buffers are allocated and freed through the DLL's own
//     System::MemoryAllocator statics.
//   * There is ONE vtable, and it is THEIRS. `??_7Internal@Object@@6B@`
//     (0x180154720) and `??_7Base@Object@@6B@` (0x180154788) are exported data
//     symbols, so the ported constructor stamps the DLL's vtable by name. An
//     object built by our ctor is byte-for-byte indistinguishable from one
//     built by the DLL's, including its virtual dispatch.
//
// What is left is therefore an ordinary faithfulness question — does our body
// compute the same indices and the same array growth as theirs — and that is
// exactly what the shadow mode below measures.
//
// ----------------------------------------------------------------------------
// THREE ARMING TIERS
// ----------------------------------------------------------------------------
//   BANISHED_RT_INTERNAL=0            nothing in this file registers.
//
//   (default)                         TIER 1 only — the five functions with no
//                                     shared state: both InternalBasePtr
//                                     constructors, Copy, DeletePtr and
//                                     Internal::Persist.
//
//   BANISHED_RT_INTERNAL_SHADOW=1     adds observe-only wrappers around
//                                     Internal::Internal, ~Internal,
//                                     ClearPersistState and ResetPersistState.
//                                     OUR CODE NEVER WRITES THE TABLE in this
//                                     mode: it predicts the post-state, calls
//                                     the DLL to do the real work, and asserts
//                                     the prediction held. This is the oracle
//                                     (PLAN.md §4.3 risk 2's RT_DIFF_SHADOW
//                                     contract, verbatim). Requires
//                                     BANISHED_RT_DIFF=1 to report.
//
//   BANISHED_RT_INTERNAL_TABLE=1      TIER 2 — the real ports of the four
//                                     table mutators plus
//                                     InternalBasePtr::Serialize take over.
//                                     *** VERDICT: NO-GO until a SHADOW run is
//                                     green with calls= in the thousands. ***
//                                     TABLE and SHADOW are mutually exclusive;
//                                     asking for both is refused.
//
// CheckPtr is DELIBERATELY NOT REGISTERED in any tier — see the long comment on
// RtObject_InternalBasePtr_CheckPtr. `BANISHED_RT_INTERNAL_CHECKPTR=1` arms it
// for anyone who wants to measure that decision rather than take it on trust.
//
// No C++ exceptions anywhere in this file (PLAN.md §4.3 risk 5).
// ============================================================================

#include <cstdio>
#include <cstring>

#include "../../types/object/Internal.h"
#include "../../funcs/rt_port.h"
#include "../io/save_trace.h"   // Wave 9 lane S

namespace RtObject {
namespace {

// ============================================================
// The DLL's allocator and PRNG
// ============================================================
// Same rule as runtime/src/system/string.cpp: the handle table's buffers cross
// the DLL boundary in both directions (the DLL's own Internal ctors grow the
// very same array), so allocation MUST stay on the DLL's heap. Resolved with
// RtOriginal<> rather than ResolveExport so that an armed MemoryAllocator port
// cannot silently split it.
//
// ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z  0x1800c7a50
// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z     0x1800c7ad0
// Retail's Array bodies inline `EnterCS(_singleton+0x640); Allocate(...);
// LeaveCS`, which is precisely what these two statics are.
using MemAllocFn = void* (*)(uint64_t, int);
using MemFreeFn  = void  (*)(void*);
using RandFn     = uint32_t (*)(void*);

MemAllocFn g_memAlloc = nullptr;
MemFreeFn  g_memFree  = nullptr;
RandFn     g_rand     = nullptr;

void* MemAlloc(uint64_t bytes, int align) {
    if (!g_memAlloc)
        g_memAlloc = RtOriginal<MemAllocFn>("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z");
    return g_memAlloc(bytes, align);
}

void MemFree(void* p) {
    if (!g_memFree)
        g_memFree = RtOriginal<MemFreeFn>("?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
    g_memFree(p);
}

// ?Rand@Random@System@@QEAAIXZ  0x1800c7f90.
// Resolved through the ORDINARY seam, not RtOriginal: Wave 2 ported this and
// proved it bit-exact over 16,035,064 calls, so whichever implementation is
// armed is the right one to be in this path.
uint32_t RandomRand(void* random) {
    if (!g_rand) {
        g_rand = reinterpret_cast<RandFn>(ResolveExportRaw("?Rand@Random@System@@QEAAIXZ"));
    }
    return g_rand(random);
}

// ============================================================
// The live singleton and the two exported vtables
// ============================================================
InternalFactory** g_ppFactory     = nullptr;
void*             g_vtblInternal  = nullptr;
void*             g_vtblBase      = nullptr;

InternalFactory* Factory() {
    if (!g_ppFactory)
        g_ppFactory = reinterpret_cast<InternalFactory**>(
            ResolveExportRaw("?_singleton@InternalFactory@Object@@0PEAV12@EA"));
    return g_ppFactory ? *g_ppFactory : nullptr;
}

// An exported ??_7 symbol's ADDRESS *is* the vtable (slot 0 first), so these
// are used as values, not dereferenced. Verified against the .rdata dump in
// Internal.h: 0x180154720 holds 0x180006440, the scalar deleting destructor.
void* VtblInternal() {
    if (!g_vtblInternal) g_vtblInternal = ResolveExportRaw("??_7Internal@Object@@6B@");
    return g_vtblInternal;
}
void* VtblBase() {
    if (!g_vtblBase) g_vtblBase = ResolveExportRaw("??_7Base@Object@@6B@");
    return g_vtblBase;
}

// ============================================================
// Collection::Array<T> bodies, as retail instantiates them
// ============================================================
// WAVE2's warning is live here: node/element offsets are a function of
// sizeof(T), so MSVC emits a SEPARATE body per instantiation and one ported
// body cannot serve two element widths. Retail has four distinct ones in this
// class alone, and all four are ported separately below with their own
// addresses. The growth policy is identical in all four and matches
// runtime/types/collection/Collection.h exactly.
//
// The `if (slot != nullptr)` tests are RETAIL's, not ours. They are the
// template's per-element address check surviving into codegen; they can never
// fire for a non-null buffer. Kept because this file is byte-faithful first.

// FUN_18002ffa0 — Array<Internal*>::SetSize(int newSize, const Internal*& fill)
void ArrayPtrSetSizeFill(Collection::Array<Internal*>* a, int newSize, Internal* const* fill) {
    const int oldSize = a->_size;
    if (newSize == oldSize)
        return;
    Internal** old = a->_data;
    a->_size = newSize;
    if (a->_capacity < newSize) {
        a->_capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0)
            a->_capacity = newSize * 3;
        a->_data = static_cast<Internal**>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity) << 3), 8));
    }
    int i = (old == a->_data) ? oldSize : 0;
    for (; i < newSize; ++i) {
        Internal** slot = a->_data + i;
        if (slot != nullptr)
            *slot = (i < oldSize) ? old[i] : *fill;
    }
    if (old != a->_data && old != nullptr)
        MemFree(old);
}

// FUN_18002fca0 — Array<Internal*>::SetSize(int newSize), no fill value.
// Only the surviving prefix is copied; a grown tail is left UNINITIALISED,
// which is why retail only ever calls this one to SHRINK.
void ArrayPtrSetSize(Collection::Array<Internal*>* a, int newSize) {
    const int oldSize = a->_size;
    if (newSize == oldSize)
        return;
    Internal** old = a->_data;
    a->_size = newSize;
    if (a->_capacity < newSize) {
        a->_capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0)
            a->_capacity = newSize * 3;
        a->_data = static_cast<Internal**>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity) << 3), 8));
    }
    int i = (old == a->_data) ? oldSize : 0;
    for (; i < newSize; ++i) {
        if (i < oldSize) {
            Internal** slot = a->_data + i;
            if (slot != nullptr)
                *slot = old[i];
        }
    }
    if (old != a->_data && old != nullptr)
        MemFree(old);
}

// FUN_18002fd90 — Array<Internal*>::Reserve(int n). Grows CAPACITY only: it
// sets _size to n while it reallocates and restores the old _size on the way
// out, and it returns immediately unless n is strictly greater than _size.
void ArrayPtrReserve(Collection::Array<Internal*>* a, int n) {
    const int oldSize = a->_size;
    if (oldSize >= n)
        return;
    Internal** old = a->_data;
    a->_size = n;
    if (a->_capacity < n) {
        a->_capacity = n;
        if (n == oldSize + 1 && oldSize != 0)
            a->_capacity = n * 3;
        a->_data = static_cast<Internal**>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity) << 3), 8));
    }
    if (old != a->_data) {
        for (int i = 0; i < oldSize; ++i) {
            Internal** slot = a->_data + i;
            if (slot != nullptr)
                *slot = old[i];
        }
        if (old != nullptr)
            MemFree(old);
    }
    a->_size = oldSize;
}

// FUN_18002fea0 — Array<int32_t>::SetSize(int newSize). Element width 4,
// alignment 4 (Collection.h: the alignment argument is alignof(T), not
// sizeof(T)); otherwise identical to ArrayPtrSetSize.
void ArrayIntSetSize(Collection::Array<int32_t>* a, int newSize) {
    const int oldSize = a->_size;
    if (newSize == oldSize)
        return;
    int32_t* old = a->_data;
    a->_size = newSize;
    if (a->_capacity < newSize) {
        a->_capacity = newSize;
        if (newSize == oldSize + 1 && oldSize != 0)
            a->_capacity = newSize * 3;
        a->_data = static_cast<int32_t*>(
            MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(a->_capacity) << 2), 4));
    }
    int i = (old == a->_data) ? oldSize : 0;
    for (; i < newSize; ++i) {
        if (i < oldSize) {
            int32_t* slot = a->_data + i;
            if (slot != nullptr)
                *slot = old[i];
        }
    }
    if (old != a->_data && old != nullptr)
        MemFree(old);
}

// The capacity ArrayPtrSetSizeFill/SetSize will end up with, given a starting
// (size, capacity) and a target size. Used by the shadow oracle so that the
// growth policy — the only non-trivial arithmetic in the whole class — is
// actually checked and not just assumed.
int PredictCapacity(int oldSize, int oldCapacity, int newSize) {
    if (newSize == oldSize)
        return oldCapacity;
    if (oldCapacity >= newSize)
        return oldCapacity;
    if (newSize == oldSize + 1 && oldSize != 0)
        return newSize * 3;
    return newSize;
}

// Opt-IN switch. RtWaveEnabled() is opt-OUT (true unless "0"), which is wrong
// for a tier that must stay off until someone measures it.
bool RtOptIn(const char* name) {
    const char* e = std::getenv(name);
    return e && e[0] && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0;
}

}  // namespace

// ============================================================================
// TIER 1 — no shared state
// ============================================================================

// ??0InternalBasePtr@Object@@QEAA@XZ   retail 0x1800062c0, profile 0x180023900
// One instruction: `mov qword ptr [rcx], 0`.
void* RtObject_InternalBasePtr_Ctor(InternalBasePtr* self) {
    RT_DIFF_BEGIN("Object::InternalBasePtr::InternalBasePtr()");
    InternalBasePtr ours;
    ours.ptr = nullptr;
    if (rt_diff_on__) {
        // Scratch copies only: the original never touches `self`.
        InternalBasePtr theirs;
        std::memset(&theirs, 0xCD, sizeof(theirs));
        RtOriginal<void* (*)(InternalBasePtr*)>("??0InternalBasePtr@Object@@QEAA@XZ")(&theirs);
        RT_DIFF_BYTES(&ours, &theirs, sizeof(InternalBasePtr));
    }
    *self = ours;
    return self;   // MSVC constructors return `this` in RAX.
}

// ??0InternalBasePtr@Object@@QEAA@PEAVInternal@1@@Z
//                                      retail 0x1800062a0, profile 0x1800238e0
// One instruction: `mov qword ptr [rcx], rdx`.
void* RtObject_InternalBasePtr_CtorPtr(InternalBasePtr* self, Internal* p) {
    RT_DIFF_BEGIN("Object::InternalBasePtr::InternalBasePtr(Internal*)");
    InternalBasePtr ours;
    ours.ptr = p;
    if (rt_diff_on__) {
        InternalBasePtr theirs;
        std::memset(&theirs, 0xCD, sizeof(theirs));
        RtOriginal<void* (*)(InternalBasePtr*, Internal*)>(
            "??0InternalBasePtr@Object@@QEAA@PEAVInternal@1@@Z")(&theirs, p);
        RT_DIFF_BYTES(&ours, &theirs, sizeof(InternalBasePtr));
    }
    *self = ours;
    return self;
}

// ?Copy@InternalBasePtr@Object@@IEAAXAEBV12@@Z
//                                      retail 0x1800062b0, profile 0x1800257d0
// One instruction pair: `mov rax,[rdx]; mov [rcx],rax`. NOT refcounted — this
// really is a raw 8-byte copy, and the two InternalBasePtrs are then aliases.
void RtObject_InternalBasePtr_Copy(InternalBasePtr* self, const InternalBasePtr* other) {
    RT_DIFF_BEGIN("Object::InternalBasePtr::Copy");
    InternalBasePtr ours;
    ours.ptr = other->ptr;
    if (rt_diff_on__) {
        InternalBasePtr theirs;
        std::memset(&theirs, 0xCD, sizeof(theirs));
        RtOriginal<void (*)(InternalBasePtr*, const InternalBasePtr*)>(
            "?Copy@InternalBasePtr@Object@@IEAAXAEBV12@@Z")(&theirs, other);
        RT_DIFF_BYTES(&ours, &theirs, sizeof(InternalBasePtr));
    }
    *self = ours;
}

// ?CheckPtr@InternalBasePtr@Object@@IEBAXXZ
//                                      retail 0x1800c0290, profile 0x18008c5d0
//
// *** A BARE `RET 0` IN BOTH BUILDS. *** It validates nothing. The profile
// build — asserts compiled in, 5,182 exports — has the identical empty body, so
// this was compiled out at the SOURCE level in every shipped configuration, not
// optimised out of retail. `?AssignId@InternalBasePtr@Object@@IEAAXXZ`
// (0x1800c0280) is the same three bytes.
//
// NOT REGISTERED BY DEFAULT, and the reason is not squeamishness — registering
// it would be an active regression:
//
//   engine/funcs/engine_funcs.cpp:3154-3172 inspects the FIRST BYTE of whatever
//   `?CheckPtr@…@IEBAXXZ` resolves to, and sets `g_checkPtrIsNop` only when it
//   is `C3` or `C2 00 00`. That flag makes ~60,000 calls per frame at 800x10
//   disappear. Register this port and the resolved body becomes a GCC-compiled
//   function whose first byte is neither — so the flag goes false, the calls
//   come back, and the `[CheckPtr]` line in standalone_error.log flips to
//   "real call". We would have replaced a no-op with a no-op and paid 60k
//   indirect calls a frame for it.
//
//   Worse in the perf build: with BANISHED_CHECKPTR_STATIC_NOP the call sites
//   are compiled out entirely, so a registered port would never execute at all
//   while the census counted it as ported — Wave 1 defect #4 exactly
//   (reference_uncalled_port_is_not_verified.md).
//
// BANISHED_RT_INTERNAL_CHECKPTR=1 arms it anyway, for measuring that claim.
void RtObject_InternalBasePtr_CheckPtr(const InternalBasePtr*) {
}

// ?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z
//                                      retail 0x1800bfda0, profile 0x18008b490
// Empty in both builds. It is slot 3 of ??_7Internal@Object@@6B@ and exists so
// that a subclass has something to override; InternalFactory::PersistObject
// calls it on every object it creates from a stream.
void RtObject_Internal_Persist(Internal*, void* /*IO::Stream&*/) {
}

// ?DeletePtr@InternalBasePtr@Object@@QEAAXXZ
//                                      retail 0x1800c0150, profile 0x18008c5a0
//
//     p = this->_ptr;
//     if (p) { p->vtbl[0](p, 1); this->_ptr = 0; }
//
// Slot 0 with flag 1 is the scalar deleting destructor: it runs ~Internal (and
// therefore the swap-remove) and then frees the block. The dispatch is virtual,
// so it lands in whatever vtable the object carries — which, because our ctor
// stamps the DLL's, is the same code either way.
//
// NOT DIFFABLE: running the original as an oracle would destroy the object
// twice. The evidence for this one is code inspection of five instructions, not
// a measurement, and the write-up says so.
void RtObject_InternalBasePtr_DeletePtr(InternalBasePtr* self) {
    Internal* p = self->ptr;
    if (p != nullptr) {
        void** vtbl = *reinterpret_cast<void***>(p);
        reinterpret_cast<void (*)(Internal*, int)>(vtbl[0])(p, 1);
        self->ptr = nullptr;
    }
}

// ============================================================================
// TIER 2 — the table mutators.  NO-GO by default; see the file header.
// ============================================================================

// ??0Internal@Object@@QEAA@XZ          retail 0x1800bfd10, profile 0x18008b370
//
// Retail inlines Base::Base and InternalFactory::AddObject; the profile build
// calls both out of line and proves what the inline is:
//     Base::Base(this);                    // vptr = &Base::_vftable_
//     this->vptr = &Internal::_vftable_;
//     this->_factoryIndex = -1;
//     this->_generation   = 0;
//     AddObject(_singleton, this);         // 0x1800c00c0
// and AddObject is
//     obj->_factoryIndex = _objects._size;
//     obj->_generation   = _random.Rand();
//     _objects.SetSize(_objects._size + 1, obj);
//
// The `-1` / `0` pair is a dead store in both builds — AddObject overwrites
// both before anything can observe them — and is reproduced anyway.
Internal* RtObject_Internal_Ctor(Internal* self) {
    self->vptr = VtblBase();
    self->vptr = VtblInternal();
    self->factoryIndex = -1;
    self->generation   = 0;

    InternalFactory* f = Factory();
    self->factoryIndex = f->objects._size;
    Internal* elem = self;
    self->generation = RandomRand(&f->random);
    ArrayPtrSetSizeFill(&f->objects, f->objects._size + 1, &elem);
    return self;
}

// ??1Internal@Object@@UEAA@XZ          retail 0x1800bfdb0, profile 0x18008b400
//
//     this->vptr = &Internal::_vftable_;
//     this->_generation = 0;
//     RemoveObject(_singleton, this);          // 0x1800c0110, inlined
//     Base::~Base(this);                       // vptr = &Base::_vftable_
//
// RemoveObject is the swap-remove that
// reference_internalfactory_swapremove_wild_write.md is about:
//     last = _objects._size - 1;  idx = obj->_factoryIndex;
//     if (last != idx) { _objects[idx] = _objects[last];
//                        _objects[idx]->_factoryIndex = idx; }
//     _objects.SetSize(last);
// There is NO validation of `idx` — not here and not in the profile build. If
// `idx` is stale the second store lands wherever the allocator has since put
// that block. That is retail's behaviour and this port keeps it; the fix for
// the family is upstream (unregister before freeing), never a guard here.
void RtObject_Internal_Dtor(Internal* self) {
    self->vptr = VtblInternal();
    self->generation = 0;

    InternalFactory* f = Factory();
    const int last = f->objects._size - 1;
    const int idx  = self->factoryIndex;
    if (last != idx) {
        f->objects._data[idx] = f->objects._data[last];
        f->objects._data[idx]->factoryIndex = idx;
    }
    ArrayPtrSetSize(&f->objects, last);

    self->vptr = VtblBase();
}

// ?ClearPersistState@InternalFactory@Object@@QEAAXXZ
//                                      retail 0x1800bfe30, profile 0x18008c0b0
// Frees the persist-id array and zeroes all three of its fields. Retail inlines
// the free (FUN_18001c750); the profile build calls MemFree — and it calls it
// UNCONDITIONALLY, where retail's inline null-checks first. Same behaviour:
// MemoryAllocator::Free opens with `test rdx,rdx; jz`.
void RtObject_InternalFactory_ClearPersistState(InternalFactory* self) {
    if (self->persistIds._data != nullptr)
        MemFree(self->persistIds._data);
    self->persistIds._data     = nullptr;
    self->persistIds._size     = 0;
    self->persistIds._capacity = 0;
}

// ?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z
//                                      retail 0x1800bfe40, profile 0x18008bfc0
//
//     Serializer ser(stream, 0);              // inlined; see below
//     int count = _objects._size;
//     ser.readerWriter->vtbl[0xa0](encoding, &count, 0);   // WRITE on save, READ on load
//     _persistIds.SetSize(count);
//     _objects.Reserve(count);
//     for (i = 0; i < _persistIds._size; ++i) _persistIds[i] = -1;
//     ~Serializer(ser);                       // encoding->vtbl[0](encoding, 1)
//
// The Serializer construction is reproduced from ??0Serializer@IO@@QEAA@AEAVStream@1@H@Z
// (0x1800b9960), which retail inlines here verbatim: stream+0x08 selects the
// direction (0 == reading) and stream+0x10 is the encoding factory whose vtable
// slot 3 (+0x18) makes a reader and slot 4 (+0x20) makes a writer.
//
// DEVIATION, stated because it is a real one: both factory calls are made here
// with (factory, &serializer). Retail's read branch passes only RCX in the
// decompile, but Ghidra also invents two junk arguments for the write branch,
// so its argument analysis at these two sites is not trustworthy. Passing an
// extra register argument to a callee that ignores it is a no-op in the
// Microsoft x64 ABI, so this is safe in either direction; it is not, however,
// byte-faithful, and if this function is ever taken live the two slots should
// be disassembled and the exact arity written down.
void RtObject_InternalFactory_ResetPersistState(InternalFactory* self, void* stream) {
    SerializerLite ser;
    ser.stream   = stream;
    ser.readerWriter = nullptr;

    char*  s       = static_cast<char*>(stream);
    void*  factory = *reinterpret_cast<void**>(s + 0x10);
    void** fvtbl   = *reinterpret_cast<void***>(factory);
    const int mode = *reinterpret_cast<int*>(s + 0x08);
    ser.readerWriter = reinterpret_cast<void* (*)(void*, void*)>(
        fvtbl[(mode == 0 ? 0x18 : 0x20) / 8])(factory, &ser);

    void*  enc     = ser.readerWriter;
    void** encVtbl = *reinterpret_cast<void***>(enc);

    int count = self->objects._size;
    reinterpret_cast<void (*)(void*, int*, const char*)>(encVtbl[kEncSerializeInt / 8])(
        enc, &count, nullptr);

    ArrayIntSetSize(&self->persistIds, count);
    ArrayPtrReserve(&self->objects, count);

    // The bound is re-read from _persistIds._size on every iteration, as retail
    // does; it cannot change, but this file is byte-faithful first.
    for (int i = 0; i < self->persistIds._size; ++i)
        self->persistIds._data[i] = -1;

    if (enc != nullptr)
        reinterpret_cast<void (*)(void*, int)>(encVtbl[0])(enc, 1);
}

// ?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z
//                                      retail 0x1800c0180, profile 0x18008c460
//
//     bool present = (_ptr != 0);
//     enc->Serialize(present);                     // enc vtbl +0x78
//     if (!present) { _ptr = 0; return; }
//     index = _ptr ? _ptr->_factoryIndex : 0;
//     type  = _ptr ? _ptr->Type() : 0xffffffff;    // Internal vtbl slot 2
//     gen   = _ptr ? _ptr->_generation : 0;
//     enc->Serialize(index);                       // +0xa0, int
//     enc->Serialize(gen);                         // +0x98, uint
//     enc->Serialize(type);                        // +0x98, uint
//     _ptr = InternalFactory::PersistObject(_singleton, ser.stream, gen, index, type);
//
// The redundant `_ptr ? … : …` guards are retail's: `present` already proved
// the pointer non-null, and the compiler kept the tests anyway.
//
// *** THE ENCODING VTABLE OFFSETS ARE BUILD-SPECIFIC. *** These are the RETAIL
// numbers (0x78 / 0x98 / 0xa0, agreeing with the forwarder table in
// runtime/src/io/serializer.cpp). The PROFILE build uses 0x90 / 0xb0 / 0xb8 for
// the same three calls. Transcribing the profile numbers writes the right
// values through the wrong slots and desyncs every save.
//
// NOT DIFFABLE and NOT SHADOWED: the call advances a stream, so a second run
// would consume or emit the record twice, and there is no way to observe what
// the DLL wrote without reproducing the codec (Wave 4). It is registered only
// under BANISHED_RT_INTERNAL_TABLE=1 and is UNVERIFIED.
void RtObject_InternalBasePtr_Serialize(InternalBasePtr* self, SerializerLite* ser) {
    void*  enc     = ser->readerWriter;
    void** encVtbl = *reinterpret_cast<void***>(enc);

    char present = (self->ptr != nullptr) ? 1 : 0;
    reinterpret_cast<void (*)(void*, char*, const char*)>(encVtbl[kEncSerializeBool / 8])(
        enc, &present, nullptr);
    if (present == 0) {
        self->ptr = nullptr;
        return;
    }

    int      index;
    uint32_t type;
    Internal* p = self->ptr;
    if (p == nullptr) {
        index = 0;
        type  = 0xffffffffu;
    } else {
        index = p->factoryIndex;
        void** pv = *reinterpret_cast<void***>(p);
        type = reinterpret_cast<uint32_t (*)(Internal*)>(pv[kInternalVtblType / 8])(p);
    }
    const uint32_t gen = (self->ptr != nullptr) ? self->ptr->generation : 0u;

    reinterpret_cast<void (*)(void*, int*, const char*)>(encVtbl[kEncSerializeInt / 8])(
        enc, &index, nullptr);
    reinterpret_cast<void (*)(void*, uint32_t*, const char*)>(encVtbl[kEncSerializeUint / 8])(
        enc, const_cast<uint32_t*>(&gen), nullptr);
    reinterpret_cast<void (*)(void*, uint32_t*, const char*)>(encVtbl[kEncSerializeUint / 8])(
        enc, &type, nullptr);

    using PersistObjectFn = Internal* (*)(InternalFactory*, void*, uint32_t, int, uint32_t);
    static PersistObjectFn persist = nullptr;
    if (!persist)
        persist = RtOriginal<PersistObjectFn>(
            "?PersistObject@InternalFactory@Object@@QEAAPEAVInternal@2@AEAVStream@IO@@IHI@Z");
    self->ptr = persist(Factory(), ser->stream, gen, index, type);
}

// ============================================================================
// SHADOW MODE — the oracle for the table mutators
// ============================================================================
// The contract from rt_diff.h, followed literally: OURS computes a prediction
// into locals, THEIRS does the real work on the live table, and the wrapper
// asserts the prediction held. Our code performs no store into the factory in
// this mode, so arming it cannot corrupt anything a plain run would not.
//
// What is checked, and it is the whole of the arithmetic in this class:
//   ctor  — the new object's index, the new element count, that the table slot
//           really points at the object, and the CAPACITY the growth policy
//           should have produced (the `*3` rule).
//   dtor  — the swap-remove: which element moves, where it lands, that its
//           _factoryIndex was rewritten, and the new count.
//   Clear — all three persist-array fields zeroed.
//   Reset — the post-state is self-consistent with our model given the count
//           the stream actually carried: _persistIds sized to it and filled
//           with -1, _objects._size unchanged, and both capacities exactly what
//           PredictCapacity says.
//
// NOT checked by the ctor shadow: the generation. It comes from
// _random.Rand(), and predicting it would require drawing from the same
// generator, which advances it. Wave 2 proved Rand bit-exact over 16M calls
// separately.
namespace {

char g_desc[2][160];

template <typename Fn>
Fn Orig(const char* mangled) { return RtOriginal<Fn>(mangled); }

}  // namespace

Internal* RtObject_Internal_Ctor_Shadow(Internal* self) {
    RT_DIFF_BEGIN("Object::Internal::Internal [shadow]");
    InternalFactory* f = Factory();
    const int oldSize = f ? f->objects._size     : 0;
    const int oldCap  = f ? f->objects._capacity : 0;

    Internal* r = Orig<Internal* (*)(Internal*)>("??0Internal@Object@@QEAA@XZ")(self);

    if (rt_diff_on__ && f) {
        const int wantSize = oldSize + 1;
        const int wantCap  = PredictCapacity(oldSize, oldCap, wantSize);
        const bool ok = self->factoryIndex == oldSize &&
                        f->objects._size == wantSize &&
                        f->objects._capacity == wantCap &&
                        f->objects._data[oldSize] == self;
        std::snprintf(g_desc[0], sizeof(g_desc[0]),
                      "predict idx=%d size=%d cap=%d slot=%p",
                      oldSize, wantSize, wantCap, (void*)self);
        std::snprintf(g_desc[1], sizeof(g_desc[1]),
                      "actual  idx=%d size=%d cap=%d slot=%p",
                      self->factoryIndex, f->objects._size, f->objects._capacity,
                      (void*)f->objects._data[oldSize]);
        RT_DIFF_SHADOW(ok, g_desc[0], g_desc[1]);
    }
    return r;
}

void RtObject_Internal_Dtor_Shadow(Internal* self) {
    RT_DIFF_BEGIN("Object::Internal::~Internal [shadow]");
    InternalFactory* f = Factory();
    const int oldSize = f ? f->objects._size : 0;
    const int last    = oldSize - 1;
    const int idx     = self->factoryIndex;
    Internal* moved   = nullptr;
    if (f && last != idx && last >= 0)
        moved = f->objects._data[last];

    Orig<void (*)(Internal*)>("??1Internal@Object@@UEAA@XZ")(self);

    if (rt_diff_on__ && f) {
        bool ok = f->objects._size == last && self->generation == 0;
        if (ok && moved != nullptr)
            ok = f->objects._data[idx] == moved && moved->factoryIndex == idx;
        std::snprintf(g_desc[0], sizeof(g_desc[0]),
                      "predict size=%d idx=%d moved=%p->idx %d", last, idx, (void*)moved, idx);
        std::snprintf(g_desc[1], sizeof(g_desc[1]),
                      "actual  size=%d slot=%p gen=%u", f->objects._size,
                      (moved && idx >= 0 && idx < f->objects._size)
                          ? (void*)f->objects._data[idx] : nullptr,
                      self->generation);
        RT_DIFF_SHADOW(ok, g_desc[0], g_desc[1]);
    }
}

void RtObject_InternalFactory_ClearPersistState_Shadow(InternalFactory* self) {
    RT_DIFF_BEGIN("Object::InternalFactory::ClearPersistState [shadow]");
    Orig<void (*)(InternalFactory*)>("?ClearPersistState@InternalFactory@Object@@QEAAXXZ")(self);
    if (rt_diff_on__) {
        const bool ok = self->persistIds._data == nullptr &&
                        self->persistIds._size == 0 &&
                        self->persistIds._capacity == 0;
        std::snprintf(g_desc[0], sizeof(g_desc[0]), "predict data=null size=0 cap=0");
        std::snprintf(g_desc[1], sizeof(g_desc[1]), "actual  data=%p size=%d cap=%d",
                      (void*)self->persistIds._data, self->persistIds._size,
                      self->persistIds._capacity);
        RT_DIFF_SHADOW(ok, g_desc[0], g_desc[1]);
    }
}

void RtObject_InternalFactory_ResetPersistState_Shadow(InternalFactory* self, void* stream) {
    RT_DIFF_BEGIN("Object::InternalFactory::ResetPersistState [shadow]");
    const int objSize = self->objects._size;
    const int objCap  = self->objects._capacity;
    const int pSize   = self->persistIds._size;
    const int pCap    = self->persistIds._capacity;

    Orig<void (*)(InternalFactory*, void*)>(
        "?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z")(self, stream);

    if (rt_diff_on__) {
        // The count came off the stream, so it is an OUTPUT here, not an input:
        // read it back from the array the DLL just sized and check everything
        // else against our model of it.
        const int count = self->persistIds._size;
        bool ok = self->objects._size == objSize &&
                  self->objects._capacity == PredictCapacity(objSize, objCap,
                                                             count > objSize ? count : objSize) &&
                  self->persistIds._capacity == PredictCapacity(pSize, pCap, count);
        for (int i = 0; ok && i < count; ++i)
            ok = self->persistIds._data[i] == -1;
        std::snprintf(g_desc[0], sizeof(g_desc[0]),
                      "predict count=%d objSize=%d objCap=%d pCap=%d all -1", count, objSize,
                      PredictCapacity(objSize, objCap, count > objSize ? count : objSize),
                      PredictCapacity(pSize, pCap, count));
        std::snprintf(g_desc[1], sizeof(g_desc[1]),
                      "actual  count=%d objSize=%d objCap=%d pCap=%d", count,
                      self->objects._size, self->objects._capacity, self->persistIds._capacity);
        RT_DIFF_SHADOW(ok, g_desc[0], g_desc[1]);
    }
}

}  // namespace RtObject

// ============================================================================
// Registration
// ============================================================================
int RtObjectInternalRegister() {
    using namespace RtObject;
    int n = 0;

    const bool shadow   = RtOptIn("BANISHED_RT_INTERNAL_SHADOW");
    const bool table    = RtOptIn("BANISHED_RT_INTERNAL_TABLE");
    const bool checkPtr = RtOptIn("BANISHED_RT_INTERNAL_CHECKPTR");

    if (shadow && table) {
        printf("[RtPort]   Object::Internal: REFUSED — BANISHED_RT_INTERNAL_SHADOW and "
               "BANISHED_RT_INTERNAL_TABLE are mutually exclusive (shadow needs the DLL to "
               "own the table). Nothing registered.\n");
        return 0;
    }

    // ---- Tier 1: no shared state, on by default -----------------------------
    // Both a name and a Ghidra address for every one: half our call sites
    // resolve by name and half by ResolveAddr(Module::Runtime, …) with the
    // other as a fallback (see the same note in runtime/src/system/string.cpp).
    RegisterRuntimeExport("??0InternalBasePtr@Object@@QEAA@XZ",
                          (void*)&RtObject_InternalBasePtr_Ctor); ++n;
    RegisterRuntimeAddr(0x1800062c0, (void*)&RtObject_InternalBasePtr_Ctor);

    RegisterRuntimeExport("??0InternalBasePtr@Object@@QEAA@PEAVInternal@1@@Z",
                          (void*)&RtObject_InternalBasePtr_CtorPtr); ++n;
    RegisterRuntimeAddr(0x1800062a0, (void*)&RtObject_InternalBasePtr_CtorPtr);

    RegisterRuntimeExport("?Copy@InternalBasePtr@Object@@IEAAXAEBV12@@Z",
                          (void*)&RtObject_InternalBasePtr_Copy); ++n;
    RegisterRuntimeAddr(0x1800062b0, (void*)&RtObject_InternalBasePtr_Copy);

    RegisterRuntimeExport("?DeletePtr@InternalBasePtr@Object@@QEAAXXZ",
                          (void*)&RtObject_InternalBasePtr_DeletePtr); ++n;
    RegisterRuntimeAddr(0x1800c0150, (void*)&RtObject_InternalBasePtr_DeletePtr);

    RegisterRuntimeExport("?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z",
                          (void*)&RtObject_Internal_Persist); ++n;
    RegisterRuntimeAddr(0x1800bfda0, (void*)&RtObject_Internal_Persist);

    // ---- CheckPtr: opt-in only. See the comment on the function. ------------
    if (checkPtr) {
        RegisterRuntimeExport("?CheckPtr@InternalBasePtr@Object@@IEBAXXZ",
                              (void*)&RtObject_InternalBasePtr_CheckPtr); ++n;
        RegisterRuntimeAddr(0x1800c0290, (void*)&RtObject_InternalBasePtr_CheckPtr);
        printf("[RtPort]   Object::InternalBasePtr::CheckPtr ARMED — expect the [CheckPtr] "
               "line to say \"real call\" and ~60k extra indirect calls per frame.\n");
    }

    // ---- Shadow oracle ------------------------------------------------------
    if (shadow) {
        RegisterRuntimeExport("??0Internal@Object@@QEAA@XZ",
                              (void*)&RtObject_Internal_Ctor_Shadow);
        RegisterRuntimeAddr(0x1800bfd10, (void*)&RtObject_Internal_Ctor_Shadow);
        RegisterRuntimeExport("??1Internal@Object@@UEAA@XZ",
                              (void*)&RtObject_Internal_Dtor_Shadow);
        RegisterRuntimeAddr(0x1800bfdb0, (void*)&RtObject_Internal_Dtor_Shadow);
        RegisterRuntimeExport("?ClearPersistState@InternalFactory@Object@@QEAAXXZ",
                              (void*)&RtObject_InternalFactory_ClearPersistState_Shadow);
        RegisterRuntimeAddr(0x1800bfe30,
                            (void*)&RtObject_InternalFactory_ClearPersistState_Shadow);
        RegisterRuntimeExport("?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z",
                              (void*)&RtObject_InternalFactory_ResetPersistState_Shadow);
        RegisterRuntimeAddr(0x1800bfe40,
                            (void*)&RtObject_InternalFactory_ResetPersistState_Shadow);
        printf("[RtPort]   Object::Internal: SHADOW mode — the DLL still owns the handle "
               "table; 4 wrappers predict-and-verify. Needs BANISHED_RT_DIFF=1 to report. "
               "NOT counted as ported.\n");
    }

    // ---- InternalBasePtr::Serialize — UNGATED 2026-09-07 (Wave 9) -----------
    // It sat in the TABLE tier below, but not because it touches the handle
    // table: it does not. It was gated because WAVE3.md could not verify it —
    // "has no oracle at all and lands in the save/load path". The lane S prefix
    // oracle now verifies it on BOTH branches against the shipped body:
    //   non-null  4 emissions (bool present, int index, uint gen, uint type)
    //             identical — and those four ARE the whole emission sequence,
    //             since the only thing after them is PersistObject, which both
    //             sides would call identically
    //   null      1 emission identical, full compare, no stop needed
    // Gated-because-unverified is not the same as gated-because-dangerous, and
    // keeping the two in one switch made a verified port look like a NO-GO.
    RegisterRuntimeExport("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtObject_InternalBasePtr_Serialize); ++n;
    RegisterRuntimeAddr(0x1800c0180, (void*)&RtObject_InternalBasePtr_Serialize);

    // ---- Tier 2: the real table ports. NO-GO until shadow is green. ---------
    if (table) {
        RegisterRuntimeExport("??0Internal@Object@@QEAA@XZ",
                              (void*)&RtObject_Internal_Ctor); ++n;
        RegisterRuntimeAddr(0x1800bfd10, (void*)&RtObject_Internal_Ctor);

        RegisterRuntimeExport("??1Internal@Object@@UEAA@XZ",
                              (void*)&RtObject_Internal_Dtor); ++n;
        RegisterRuntimeAddr(0x1800bfdb0, (void*)&RtObject_Internal_Dtor);

        RegisterRuntimeExport("?ClearPersistState@InternalFactory@Object@@QEAAXXZ",
                              (void*)&RtObject_InternalFactory_ClearPersistState); ++n;
        RegisterRuntimeAddr(0x1800bfe30, (void*)&RtObject_InternalFactory_ClearPersistState);

        RegisterRuntimeExport("?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z",
                              (void*)&RtObject_InternalFactory_ResetPersistState); ++n;
        RegisterRuntimeAddr(0x1800bfe40, (void*)&RtObject_InternalFactory_ResetPersistState);


        printf("[RtPort]   Object::Internal: TABLE mode ARMED — our code now drives the live "
               "handle table. Verdict on this tier is NO-GO unless a SHADOW run has been "
               "green with calls= in the thousands (docs/runtime_port/WAVE3_LANE_B.md §5).\n");
    }

    printf("[RtPort]   Object::Internal: %d exports registered%s\n", n,
           table ? " (INCLUDING the unverified table tier)" : "");
    return n;
}

namespace RtObject {

// ============================================================================
// Wave 9 lane S probe — InternalBasePtr::Serialize, the "no oracle at all" one
// ============================================================================
// WAVE3.md listed this as the residual issue that survives even a fully green
// shadow: "`InternalBasePtr::Serialize` has no oracle at all and lands in the
// save/load path". That was true when it was written. It is not any more.
//
// Why the full A/B still cannot be used: the body ends in
// `self->ptr = InternalFactory::PersistObject(...)`, which mutates the pointer
// AND the global factory, so replaying the whole thing would persist twice.
//
// Why the PREFIX oracle is nonetheless the WHOLE answer here: every byte this
// function contributes to the stream is emitted by its four calls -- bool
// `present`, int `index`, uint `gen`, uint `type` -- and all four happen BEFORE
// the mutation. Stopping after 4 skips only `PersistObject`, which is shared
// code both sides would call identically. So the prefix is not a partial view
// of the emission sequence; it IS the emission sequence.
//
// The subject is a real live object taken from the factory's own table
// (`InternalFactory::objects`), not a synthetic one: the port reads
// `_factoryIndex`, `_generation` and dispatches `Type()` through the object's
// vtable, none of which a hand-built stand-in would exercise honestly.
extern "C" void RtObject_InternalBasePtr_SerializeTraceProbe() {
    if (!RtSaveTrace::Enabled()) return;
    static bool done = false;
    if (done) return;

    InternalFactory* f = Factory();
    if (!f || !f->objects._data || f->objects._size <= 0) return;

    Internal* live = nullptr;
    for (int i = 0; i < f->objects._size && !live; ++i)
        live = f->objects._data[i];
    if (!live) return;                      // table exists but is all holes
    done = true;

    auto theirs = reinterpret_cast<void (*)(void*, void*)>(
        ResolveExportRaw("?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z"));
    if (!theirs) {
        printf("[RtSaveTrace] SKIP InternalBasePtr::Serialize: shipped body unresolved\n");
        return;
    }
    auto ours = reinterpret_cast<void (*)(void*, void*)>(&RtObject_InternalBasePtr_Serialize);

    // Two separate InternalBasePtrs, one per side, so neither call's trailing
    // store can be seen by the other. Both start at the same live object.
    static InternalBasePtr subject;
    subject.ptr = live;
    RtSaveTrace::Compare("Object::InternalBasePtr::Serialize", &subject, ours, theirs, 4);

    // The null branch too: `present == 0` returns after ONE emission and stores
    // only a null it already held, so it needs no stop and is a full compare.
    static InternalBasePtr nullSubject;
    nullSubject.ptr = nullptr;
    RtSaveTrace::Compare("Object::InternalBasePtr::Serialize[null]", &nullSubject, ours, theirs);
}

}  // namespace RtObject
