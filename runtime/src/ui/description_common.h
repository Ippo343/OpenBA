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

#include "../../types/ui/ElementDescription.h"   // UI::ElementDescription (B.3.6)
#include "../../types/ui/Dialog.h"               // UI::Dialog::CreateElement (fragment pass F2)
#include "../../types/ui/SpriteSheet.h"          // UI::SpriteSheet::GetSpriteIndexFromName (F2)
#include "../../types/system/MemoryAllocator.h"  // B3R RT-UI: MemAlloc / MemFree, direct
#include "../../types/system/String.h"           // B3R RT-UI: String::Serialize, direct
#include "../../types/collection/Collection.h"   // B3R RT-UI: the rtcoll bodies, direct
#include "../../types/io/Serializer.h"           // B3R RT-UI: RtIo_SerializeStringArray
#include <new>

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

// RETAIL IO::Encoding factory offsets (Encoding has no destructor slot, so
// these stay byte offsets; B3X_RULES X1). The ReaderWriter the factory returns
// is dispatched BY NAME (IO::ReaderWriter, runtime/types/io/Serializer.h).
constexpr int kEncCreateReader = 0x18;
constexpr int kEncCreateWriter = 0x20;

// Element layout constants these bodies reach through. Element.h pins them.
constexpr int kElemFlags    = 0x34;   // OR dword [RCX+0x34],0x8000000
constexpr int kElemParent   = 0x98;   // MOV [RCX+0x98],<parent>
constexpr int kElemChildren = 0xa0;   // Array<Element*>: _data +0xa0, _size +0xa8
constexpr uint32_t kElementFlagNeedsSizeCalc = 0x08000000u;

inline void** VTableOf(const void* o) {
    return *const_cast<void***>(reinterpret_cast<void** const*>(o));
}

// B3R RT-UI: `?MemFree@MemoryAllocator@System@@` (0x1800c7ad0), called
// directly (B3R_RULES R9, correction 1).
inline void MemFree(void* p) {
    System::MemoryAllocator::MemFree(p);
}
// The locked Enter/Allocate/Leave triple every CreateInstance inlines
// (`?_singleton@MemoryAllocator@System@@` + 0x640, EnterCriticalSection,
// 0x1800c6e60, LeaveCriticalSection) has one out-of-line twin, 0x1800c7a50,
// checked instruction by instruction by runtime/src/ui/factory_ui1.cpp.
inline void* MemAlloc(uint64_t bytes, int align) {
    return System::MemoryAllocator::MemAlloc(bytes, align);
}

// ---- B3R RT-UI: the argument bridges of the direct field-serializer calls ---
// The `Object::ExternalPtr<T>::Serialize` bodies (runtime/types/object/External.h)
// take the field as `RtObject::External**` and the PAIR as `rt::io::Serializer*`;
// every caller here holds them as the raw field and this file's `Serializer`.
inline RtObject::External** ExtField(void* field) {
    return reinterpret_cast<RtObject::External**>(field);                  // B.3.x bridge
}
inline RtObject::External** ExtArray(void* data) {
    return reinterpret_cast<RtObject::External**>(data);                   // B.3.x bridge
}
inline rt::io::Serializer* IoSer(Serializer* ser) {
    return reinterpret_cast<rt::io::Serializer*>(ser);                     // B.3.x bridge
}
// `?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z` (0x1800c9930) on a
// raw System::String field.
inline void StringSerialize(void* field, Serializer* ser) {
    static_cast<System::String*>(field)->Serialize(ser);                   // B.3.x bridge
}

// ---- fragment 1/2: the reader-writer the encoding factory hands back --------
// Returned as the class it is; each caller dispatches the ReaderWriter's
// members by name (`rw->SerializeInt(v, nullptr)`, and the teardown
// `if (rw) rw->`delete` (slot 0, flags 1)`). The encoding factory call itself stays a
// byte-offset read: IO::Encoding has no destructor slot (B3X_RULES X1).
inline IO::ReaderWriter* MakeReaderWriter(Stream* stream, Serializer* ser) {
    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? kEncCreateReader : kEncCreateWriter;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int zero);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, ser, 0);
    ser->readerWriter = rw;
    return reinterpret_cast<IO::ReaderWriter*>(rw);                        // B.3.x bridge
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
    rtcoll::RtColl_ArraySetSizeFill_180076900(
        parent + kElemChildren,
        *reinterpret_cast<int32_t*>(parent + kElemChildren + 8) + 1,
        made);
    MarkNeedsSizeCalc(parent);
}

// ---- the base-class entry point every CreateInstance goes through ---------
// ?CreateElement@Dialog@UI@@ (0x180112ed0). Fragment pass F2: block B defined
// the member; the RETSLOT is kept for the callers (placement-new of the
// member's prvalue, exactly the door's shape in dialog.cpp).
inline void** DialogCreateElement(const void* dialog, void** retSlot, void* desc) {
    new (retSlot) Object::InternalPtr<UI::Element>(
        reinterpret_cast<const UI::Dialog*>(dialog)                         // B.3.6 bridge
            ->UI::Dialog::CreateElement(Object::ExternalPtr<UI::ElementDescription>(
                static_cast<UI::ElementDescription*>(desc))));
    return retSlot;
}
// ?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z, reached
// by rel32 from every one of these appliers. Fragment pass F2: block D's
// member (sprite.cpp), called directly instead of by address.
inline int32_t GetSpriteIndexFromName(const void* sheet, const void* nameStr) {
    return reinterpret_cast<const UI::SpriteSheet*>(sheet)                  // B.3.6 bridge
        ->UI::SpriteSheet::GetSpriteIndexFromName(
            *static_cast<const System::String*>(nameStr));
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
