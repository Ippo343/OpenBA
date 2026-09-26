#pragma once
// ============================================================================
// runtime/src/ui/description_common.h — the shape every UI::*Description body
// repeats, written once.
// ============================================================================
//
// The ten classes ported alongside this header (border/button/check/clipregion/
// combo/container/documenttext/edit/graph/labeldescription.cpp) are ten
// instances of one compiler-generated shape. Five fragments appear verbatim in
// all ten, instruction for instruction:
//
//   1. the IO::Serializer PAIR on the frame and the encoding-factory call that
//      fills its second half;
//   2. the `if (rw) rw->vt[0](rw, 1)` teardown at the bottom of Serialize;
//   3. the refcounted System::String release, WITH retail's asymmetry
//      (on the last reference the FIELD is rewritten to `data - 8` and THAT is
//      freed, so the field is left holding a freed pointer; otherwise the field
//      is NULLED). Normalising the two arms would be a divergence — the same
//      asymmetry runtime/src/ui/dialog_ctor_dtor.cpp documents for ~Dialog;
//   4. the `kElementFlagNeedsSizeCalc` walk up `Element::_parent` (+0x98),
//      which retail emits as a DO-WHILE: the first element is marked before
//      the null test, so a null `Element*` would fault in retail too;
//   5. attach-child-and-push: write the child's `_parent`, then
//      `Array<Element*>::SetSize(&parent->_children, size + 1, made)` — the
//      three-argument fill form, ported as array.cpp's kSpec_Fill8Z
//      (0x180076900).
//
// Writing each of the five once is not a simplification of retail: each helper
// below is a transcription of ONE fragment, and every call site names the
// retail instruction it stands for. What would have been a simplification is
// folding two DIFFERENT fragments together, and none of these does that — where
// a class's copy differs (ClipRegionDescription's Serialize has no array arm;
// ContainerDescription's applier marks TWO different elements) the difference
// stays in that class's own file.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/ui/ElementDescription.h"   // UI::ElementDescription (B.3.6)

// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// — resolve.h drags in <windows.h> and PLAN.md §4.1 keeps it out of
// runtime/src/. This declaration must stay byte-identical to resolve.h's; the
// linker checks the two.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace rtdesc {

// ---- the IO::Serializer pair -----------------------------------------------
// [RSP+0x28] = the Stream*, [RSP+0x30] = the ReaderWriter the encoding factory
// returns, and `LEA RDX,[RSP+0x28]` is what every field serializer receives.
// Keeping the PAIR on the stack is load-bearing: a first draft of
// runtime/src/graphics/wave24_lane_gfxmat.cpp passed the address of a lone
// Stream* and every callee then read its reader from whatever slot happened to
// sit next to it.
struct Stream {
    uint8_t _head00[0x08];
    int32_t mode;              // +0x08  0 == reading
    int32_t _pad0c;
    void*   encodingFactory;   // +0x10
};
struct Serializer {
    Stream* stream;            // +0x00
    void*   readerWriter;      // +0x08
};
static_assert(offsetof(Stream, mode) == 0x08, "IO::Stream mode @ +0x08");
static_assert(offsetof(Stream, encodingFactory) == 0x10, "IO::Stream factory @ +0x10");
static_assert(offsetof(Serializer, readerWriter) == 0x08, "IO::Serializer rw @ +0x08");

// RETAIL ReaderWriter vtable offsets, the ones these ten bodies use. The
// profile build's table disagrees and must NOT be used to check them: its
// NAMES win, its OFFSETS do not. Full table: runtime/types/io/Serializer.h.
constexpr int kEncCreateReader = 0x18;
constexpr int kEncCreateWriter = 0x20;
constexpr int kRwScalarBool    = 0x78;
constexpr int kRwScalarUInt    = 0x98;
constexpr int kRwScalarInt     = 0xa0;
constexpr int kRwDeletingDtor  = 0x00;

// Element layout constants these bodies reach through. Element.h pins them.
constexpr int kElemFlags    = 0x34;   // OR dword [RCX+0x34],0x8000000
constexpr int kElemParent   = 0x98;   // MOV [RCX+0x98],<parent>
constexpr int kElemChildren = 0xa0;   // Array<Element*>: _data +0xa0, _size +0xa8
constexpr uint32_t kElementFlagNeedsSizeCalc = 0x08000000u;

template <typename Fn>
inline Fn RtAddr(uintptr_t ghidra) {
    return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra));
}

inline void** VTableOf(const void* o) {
    return *const_cast<void***>(reinterpret_cast<void** const*>(o));
}

// slot 0 is the VECTOR deleting destructor; flag 0 = destroy WITHOUT freeing.
inline void CallSlot0(void* self, unsigned flag) {
    using Slot0Fn = void* (*)(void*, unsigned);
    reinterpret_cast<Slot0Fn>(VTableOf(self)[0])(self, flag);
}

// ---- the vtables and the allocator, resolved by name ------------------------
// RT_DATA_SLOT caches a HIT only, so a lookup that runs before the mint has
// published the symbol is retried rather than frozen as null.
inline void* VtBase()     { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
inline void* VtExternal() { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
inline void* VtElementDescription() {
    return RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@");
}
// A class's OWN vtable and BaseTypeInfo are resolved in that class's own file,
// through RT_DATA_SLOT. They are deliberately NOT wrapped in a helper here:
// RT_DATA_SLOT's cache is one static per EXPANSION, so a single shared helper
// taking the name as a runtime argument would cache the first class's vtable
// and hand it to all ten.

inline void MemFree(void* p) {
    using MemFreeFn = void (*)(void*);
    RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p);
}
// The locked Enter/Allocate/Leave triple every CreateInstance inlines
// (`?_singleton@MemoryAllocator@System@@` + 0x640, EnterCriticalSection,
// 0x1800c6e60, LeaveCriticalSection) has one out-of-line twin, 0x1800c7a50,
// checked instruction by instruction by runtime/src/ui/factory_ui1.cpp.
inline void* MemAlloc(uint64_t bytes, int align) {
    using MemAllocFn = void* (*)(uint64_t, int);
    return RtAddr<MemAllocFn>(0x1800c7a50ull)(bytes, align);
}

// ---- fragment 1/2: the reader-writer the encoding factory hands back --------
inline void* MakeReaderWriter(Stream* stream, Serializer* ser) {
    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? kEncCreateReader : kEncCreateWriter;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int zero);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, ser, 0);
    ser->readerWriter = rw;
    return rw;
}
inline void DestroyReaderWriter(void* rw) {
    if (rw != nullptr) {
        using DeleteFn = void (*)(void*, int);
        reinterpret_cast<DeleteFn>(VTableOf(rw)[kRwDeletingDtor / 8])(rw, 1);
    }
}
// `CALL qword [RAX + <slot>]` with (rw, &value, nullptr).
inline void RwScalar(void* rw, int slot, void* value) {
    using ScalarFn = void (*)(void*, void*, const char*);
    reinterpret_cast<ScalarFn>(VTableOf(rw)[slot / 8])(rw, value, nullptr);
}

// ---- fragment 3: the refcounted System::String release ---------------------
// See the banner: the asymmetry is retail's and is kept.
inline void ReleaseStringField(uint8_t* p, size_t off) {
    char*& s = *reinterpret_cast<char**>(p + off);
    if (s != nullptr) {
        --*reinterpret_cast<int32_t*>(s - 4);
        if (*reinterpret_cast<int32_t*>(s - 4) == 0) {
            s = s - 8;             // the FIELD moves, then that is what is freed
            MemFree(s);
        } else {
            s = nullptr;
        }
    }
}

// ---- fragment 4: the needs-size-calc walk, as a DO-WHILE -------------------
inline void MarkNeedsSizeCalc(uint8_t* n) {
    do {
        *reinterpret_cast<uint32_t*>(n + kElemFlags) |= kElementFlagNeedsSizeCalc;
        n = *reinterpret_cast<uint8_t**>(n + kElemParent);
    } while (n != nullptr);
}

// ---- fragment 5: attach the new child and push it into the parent ----------
inline void AttachAndPushChild(uint8_t* parent, void** made) {
    uint8_t* child = *reinterpret_cast<uint8_t**>(made);
    if (child != nullptr)
        *reinterpret_cast<void**>(child + kElemParent) = parent;
    using PushFn = void (*)(void* array, int n, const void* fill);
    RtAddr<PushFn>(0x180076900ull)(
        parent + kElemChildren,
        *reinterpret_cast<int32_t*>(parent + kElemChildren + 8) + 1,
        made);
    MarkNeedsSizeCalc(parent);
}

// ---- the two base-class entry points every CreateInstance goes through -----
inline void* ElementCtor(void* raw) {
    using ElementCtorFn = void* (*)(void*);
    return RtAddr<ElementCtorFn>(0x180115e50ull)(raw);   // ??0Element@UI@@QEAA@XZ
}
inline void ElementDescriptionApply(const void* self, void** elemPtr) {
    using EdCreateInstanceFn = void (*)(const void*, void**);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(self, elemPtr);
}
inline void** DialogCreateElement(const void* dialog, void** retSlot, void* desc) {
    using CreateElementFn = void** (*)(const void*, void**, void*);
    return RtAddr<CreateElementFn>(0x180112ed0ull)(dialog, retSlot, desc);
}
// `?Serialize@ElementDescription@UI@@` (0x180116b00), the base half every
// leaf's Serialize runs first. ENDGAME B.3.6 block A: a direct QUALIFIED member
// call instead of the by-address door — qualified because the receiver wears
// the LEAF's table, whose slot 5 is the leaf's own Serialize (the caller).
inline void ElementDescriptionSerialize(void* self, Stream* stream) {
    reinterpret_cast<UI::ElementDescription*>(self)                        // B.3.6 bridge
        ->UI::ElementDescription::Serialize(stream);
}
// ?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z, reached
// by rel32 from every one of these appliers.
inline int32_t GetSpriteIndexFromName(const void* sheet, const void* nameStr) {
    using GetIdxFn = int32_t (*)(const void*, const void*);
    return RtAddr<GetIdxFn>(0x1800031f0ull)(sheet, nameStr);
}

// *** THE SPRITE INDEX IS STORED AS A QWORD READ FROM A SLOT ONLY A DWORD WAS
// WRITTEN TO. *** Every applier in this family writes `dword [RSP+n]` and then
// reads `qword [RSP+n]` and stores all eight bytes into the element. So the
// element's index+4 receives four bytes of whatever was on retail's stack. That
// is reproduced with an uninitialised local rather than "cleaned up" to a
// 4-byte store: a 4-byte store would leave the high dword holding the element
// constructor's own value, and this body's whole observable effect there is
// that it overwrites it with something arbitrary. Our arbitrary value is ours,
// not retail's; what is faithful is the WIDTH.
// (Identical finding, same words, in runtime/src/ui/imagedescription.cpp.)
inline uint64_t SpriteIndexSlot(const void* sheet, const void* nameStr) {
    uint64_t slot;                                 // deliberately uninitialised
    if (sheet != nullptr) {
        const int32_t idx = GetSpriteIndexFromName(sheet, nameStr);
        __builtin_memcpy(&slot, &idx, sizeof idx); // DWORD store
    } else {
        const int32_t minusOne = -1;
        __builtin_memcpy(&slot, &minusOne, sizeof minusOne);  // DWORD store
    }
    return slot;                                   // read back as a QWORD
}

}  // namespace rtdesc
