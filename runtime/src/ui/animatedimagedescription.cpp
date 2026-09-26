// ============================================================================
// runtime/src/ui/animatedimagedescription.cpp — UI::AnimatedImageDescription
// ============================================================================
//
// The cutover boot's wall after UI::RibbonDescription:
//
//   [RtMint] FATAL: call through a NOT-YET-PORTED minted vtable slot
//   [RtMint]   vtable : ??_7AnimatedImageDescription@UI@@6B@
//   [RtMint]   slot   : 2
//   [RtMint]   target : ?Type@AnimatedImageDescription@UI@@UEBAIXZ
//
// ??_7AnimatedImageDescription@UI@@6B@ (retail 0x180156060), ten slots:
//   0  deleting dtor      0x1800369e0  deleting_dtor.cpp row (sizeof 0x98)
//   1  TypeName           0x1800366d0  here
//   2  Type               0x1800366b0  here
//   3  GetFileVersion     0x1800cb6a0  here  (returns 0x10204)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800cb5c0  here
//   6  Reconstruct        0x180036850  here
//   7  Destruct           0x180036880  here
//   8  CreateInstance     0x1800cb480  here
//   9  Construct          0x180036870  here
// plus ??1AnimatedImageDescription@UI@@UEAA@XZ 0x1800cb6b0 (a DOOR, B4b-2).
//
// THIS CLASS DERIVES FROM UI::ImageDescription (the ctor stamps
// ??_7ImageDescription@UI@@6B@ first; ~AnimatedImageDescription TAIL-JUMPS to
// ~ImageDescription; Serialize CALLS ImageDescription::Serialize).
//
// LAYOUT: runtime/types/ui/AnimatedImageDescription.h — ImageDescription (0x80)
// plus `_frameNames` (Array<System::String>, +0x80) and `_framesPerSecond`
// (+0x90, ctor 30); sizeof 0x98.
//
// THE CONSTRUCTOR IS OUT OF LINE HERE, UNLIKE THE REST OF THIS FAMILY:
// `??0AnimatedImageDescription@UI@@QEAA@XZ` (0x1800cb6f0) is a real exported
// function, and `Construct` (0x180036870) is a four-instruction thunk:
//     TEST RCX,RCX ; JNZ 0x1800cb6f0 ; XOR EAX,EAX ; RET
// It is ported and registered, table-driven, by runtime/src/ui/factory_ui1.cpp
// (a ctor door, not retired in step 6), and reached here through `ResolveAddr`,
// which is registry-first.
//
// ENDGAME B.3.6 block C2: every slot body is a MEMBER of
// `UI::AnimatedImageDescription`; the registered doors are one-line QUALIFIED
// delegations. Converted: the base Serialize (by address 0x1801184b0) and the
// applier (a direct call of the extern "C" door `RtUi_ImageDescription_
// ApplyToElement`) are qualified `ImageDescription::` member calls — the
// applier is name-hidden by this class's slot-8 override; the per-frame
// `GetSpriteIndexFromName` (RT_EXPORT_ONCE) is the real `UI::SpriteSheet`
// member.
// ============================================================================
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>

#include "description_common.h"
#include "../../types/ui/AnimatedImageDescription.h"
#include "../../types/ui/AnimatedImageElement.h"
#include "../../types/ui/SpriteSheet.h"

namespace {

using namespace rtdesc;

// RULE B6-5: namespace-scope atomic door caches for the member bodies.
std::atomic<void*> g_animatedImageDescriptionTypeInfo{nullptr};
const uint8_t* AnimatedImageDescriptionTypeInfo() {
    void* p = g_animatedImageDescriptionTypeInfo.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached("?_typeInfo@AnimatedImageDescription@UI@@0VBaseTypeInfo@Object@@A",
                            &scratch);
        if (p != nullptr) g_animatedImageDescriptionTypeInfo.store(p, std::memory_order_release);
    }
    return static_cast<const uint8_t*>(p);
}

// Door-only (the dtor door is not a member).
void* VtAnimatedImageDescription() {
    return RT_DATA_SLOT(void*, "??_7AnimatedImageDescription@UI@@6B@");
}

// ??0AnimatedImageDescription@UI@@QEAA@XZ, 0x1800cb6f0 — see the banner.
void* CallCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return RtAddr<CtorFn>(0x1800cb6f0ull)(self);
}

}  // namespace

// ============================================================================
// The member bodies
// ============================================================================
namespace UI {

uint32_t AnimatedImageDescription::Type() const {              // 0x1800366b0
    const uint8_t* ti = AnimatedImageDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
// The key function: GCC emits `vtable for UI::AnimatedImageDescription` here.
const char* AnimatedImageDescription::TypeName() const {       // 0x1800366d0
    const uint8_t* ti = AnimatedImageDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// 0x1800cb6a0: `MOV EAX,0x10204 ; RET` — a THIRD distinct value in this family.
int32_t AnimatedImageDescription::GetFileVersion() const { return 0x10204; }

// 0x180036870: the null test is the DOOR's; then a tail jump to the ctor,
// whose return value (this) IS this function's.
Object::External* AnimatedImageDescription::Construct() {
    return static_cast<Object::External*>(CallCtor(this));     // 180036873 JNZ
}
// 0x180036850: slot0(this, 0) through the object's own table, then a TAIL JUMP
// to the same ctor.
Object::External* AnimatedImageDescription::Reconstruct() {
    DeletingDtor(0);                                           // 18003685e
    return static_cast<Object::External*>(CallCtor(this));     // 180036868 JMP
}
// 0x180036880: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
void AnimatedImageDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// ?Serialize@AnimatedImageDescription@UI@@UEAA_NAEAVStream@IO@@@Z  0x1800cb5c0
// ============================================================================
//   ImageDescription::Serialize(this, stream)          0x1801184b0, QUALIFIED
//   rw = encoding factory, read or write side by stream->mode
//   n = _frameNames._size ; rw->vt[0xa0](rw, &n, 0)
//   if (pair.stream->mode == 0) Array<String>::SetSize(&_frameNames, n)  0x180029a80
//   Array<String> element walk (&pair, n, _frameNames._data)            0x180029bb0
//   rw->vt[0xa0](rw, &_framesPerSecond, 0)
//   if (rw) rw->vt[0](rw, 1)
//
// The int at +0x90 is serialized AFTER the array, not before. 0x180029a80 is
// array.cpp's kSpec_StringRefZ and 0x180029bb0 the text-codec string-array
// walker (runtime/src/io/text_codec.cpp).
bool AnimatedImageDescription::Serialize(void* streamv) {
    rtdesc::Stream* stream = static_cast<rtdesc::Stream*>(streamv);

    ImageDescription::Serialize(stream);                       // 1800cb5da

    rtdesc::Serializer ser = { stream, nullptr };              // [RSP+0x28], [RSP+0x30]
    void* rw = rtdesc::MakeReaderWriter(stream, &ser);         // 1800cb60a

    int32_t n = _frameNames._size;                             // 1800cb60f
    rtdesc::RwScalar(rw, rtdesc::kRwScalarInt, &n);

    if (ser.stream->mode == 0) {                               // 1800cb62f (re-read)
        using SetSizeFn = void (*)(void* array, int n);
        rtdesc::RtAddr<SetSizeFn>(0x180029a80ull)(&_frameNames, n);   // 1800cb645
    }

    using StrArrayFn = void (*)(rtdesc::Serializer* ser, int count, void* data);
    rtdesc::RtAddr<StrArrayFn>(0x180029bb0ull)(&ser, n, _frameNames._data);

    rtdesc::RwScalar(rw, rtdesc::kRwScalarInt, &_framesPerSecond);

    rtdesc::DestroyReaderWriter(rw);                           // 1800cb67d
    return true;
}

// ============================================================================
// ?CreateInstance@AnimatedImageDescription@UI@@...   retail 0x1800cb480   (slot 8)
// ============================================================================
// RCX = this, RDX = the hidden InternalPtr<Element> return slot. R8 (the
// Dialog*) is never read: this class creates no child elements.
//
// AnimatedImageElement is 0x100 bytes (`MOV EDX,0x100` at 0x1800cb4c1) and its
// constructor is CALLED, not inlined: `??0AnimatedImageElement@UI@@QEAA@XZ`
// (0x1800cb3f0), a factory_ui1.cpp door. The ctor RETURNS `this` and retail
// keeps the returned RAX (0x1800cb4f8).
//
// Then the applier (ImageDescription::CreateInstance, 0x180118570), then the
// per-frame table:
//     Array<Sprite>::SetSize(&elem->_frames, _frameNames._size)   0x1800370c0
//     for i in [0, _frameNames._size):
//         sheet = _spriteSheet                           ; RE-READ each iteration
//         idx   = sheet ? sheet->GetSpriteIndexFromName(_frameNames[i]) : -1
//         elem->_frames[i] = { sheet, idx }              ; idx as a QWORD, see below
//     elem->_framesPerSecond = _framesPerSecond
//
// THE SAME QWORD-FROM-A-DWORD-SLOT AS THE APPLIER. 0x1800cb54f and 0x1800cb555
// write `dword [RSP+0x30]`; 0x1800cb568 reads `qword [RSP+0x30]` and 0x1800cb56d
// stores all eight bytes at entry+0x08. The WIDTH is the faithful part.
Object::InternalPtr<Element>* AnimatedImageDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog*) const {
    void* raw = rtdesc::MemAlloc(0x100, 8);                    // 1800cb4c1

    AnimatedImageElement* elem = nullptr;
    if (raw != nullptr) {                                      // 1800cb4eb
        using ElemCtorFn = void* (*)(void*);
        elem = reinterpret_cast<AnimatedImageElement*>(        // B.3.6 bridge
            rtdesc::RtAddr<ElemCtorFn>(0x1800cb3f0ull)(raw));  // 1800cb4f3, keeps RAX
    }

    Object::InternalPtr<ImageElement> slot(elem);              // 1800cb500
    ImageDescription::CreateInstance(&slot);                   // 1800cb50d  (name-hidden: qualified)

    const int32_t frames = _frameNames._size;

    using SetSizeFn = void (*)(void* array, int n);
    rtdesc::RtAddr<SetSizeFn>(0x1800370c0ull)(&elem->_frames, frames);   // 1800cb51f

    for (int32_t i = 0; i < frames; ++i) {                     // 1800cb524 / 1800cb578
        void* const sheet = _spriteSheet;                      // 1800cb530, re-read

        uint64_t idxSlot;                                      // [RSP+0x30], see the note
        if (sheet != nullptr) {                                // 1800cb534
            const int32_t idx = static_cast<const SpriteSheet*>(sheet)->GetSpriteIndexFromName(
                *reinterpret_cast<const System::String*>(&_frameNames._data[i]));   // B.3.6 bridge — String's one pointer
            __builtin_memcpy(&idxSlot, &idx, sizeof idx);      // 1800cb54f  DWORD store
        } else {
            const int32_t minusOne = -1;
            __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);   // 1800cb555
        }

        Sprite& entry = elem->_frames._data[i];                // 1800cb55d
        entry.spriteSheet = sheet;                             // 1800cb564
        __builtin_memcpy(reinterpret_cast<uint8_t*>(&entry) + offsetof(Sprite, spriteIndex),
                         &idxSlot, sizeof idxSlot);            // 1800cb56d  QWORD store
    }

    elem->_framesPerSecond = _framesPerSecond;                 // 1800cb586

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1800cb58c
    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS — each a one-line QUALIFIED delegation.
// ============================================================================
extern "C" uint32_t RtUi_AnimatedImageDescription_Type(const void* self) {
    return reinterpret_cast<const UI::AnimatedImageDescription*>(self)          // B.3.6 bridge
        ->UI::AnimatedImageDescription::Type();
}
extern "C" const char* RtUi_AnimatedImageDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::AnimatedImageDescription*>(self)          // B.3.6 bridge
        ->UI::AnimatedImageDescription::TypeName();
}
extern "C" int32_t RtUi_AnimatedImageDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::AnimatedImageDescription*>(self)          // B.3.6 bridge
        ->UI::AnimatedImageDescription::GetFileVersion();
}
extern "C" void* RtUi_AnimatedImageDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 180036870
    return reinterpret_cast<UI::AnimatedImageDescription*>(selfv)               // B.3.6 bridge
        ->UI::AnimatedImageDescription::Construct();
}
extern "C" void* RtUi_AnimatedImageDescription_Reconstruct(void* selfv) {
    return reinterpret_cast<UI::AnimatedImageDescription*>(selfv)               // B.3.6 bridge
        ->UI::AnimatedImageDescription::Reconstruct();
}
extern "C" void RtUi_AnimatedImageDescription_Destruct(void* selfv) {
    reinterpret_cast<UI::AnimatedImageDescription*>(selfv)                      // B.3.6 bridge
        ->UI::AnimatedImageDescription::Destruct();
}
extern "C" bool RtUi_AnimatedImageDescription_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<UI::AnimatedImageDescription*>(selfv)               // B.3.6 bridge
        ->UI::AnimatedImageDescription::Serialize(streamv);
}
extern "C" void** RtUi_AnimatedImageDescription_CreateInstance(const void* selfv, void** ret,
                                                               const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::AnimatedImageDescription*>(selfv)            // B.3.6 bridge
            ->UI::AnimatedImageDescription::CreateInstance(
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
                static_cast<const UI::Dialog*>(dialog)));
}

// ============================================================================
// ??1AnimatedImageDescription@UI@@UEAA@XZ   0x1800cb6b0   — a DTOR DOOR (B4b-2)
// ============================================================================
//     stamp ??_7AnimatedImageDescription@UI@@6B@             ; 1800cb6cd
//     Array<String>::~Array(this + 0x80)                     ; 1800cb6d4
//     JMP ~ImageDescription                                  ; 1800cb6e2
// 0x180010ab0 is the Array destructor that ALSO destroys its elements
// (array_destroy.cpp's J2 entry point); the plain Array::~Array the other
// classes use is 0x180039440. Using the wrong one here would leak a
// System::String per frame name.
extern "C" void RtUi_AnimatedImageDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);

    *reinterpret_cast<void**>(p) = VtAnimatedImageDescription();   // 1800cb6cd

    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x180010ab0ull)(p + 0x80);                 // 1800cb6d4

    using BaseDtorFn = void (*)(void*);
    RtAddr<BaseDtorFn>(0x180118370ull)(p);                         // 1800cb6e2 JMP
}

int RtUiAnimatedImageDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@AnimatedImageDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_AnimatedImageDescription_Type);            ++n;
    RegisterRuntimeAddr(0x1800366b0ull, (void*)&RtUi_AnimatedImageDescription_Type);

    RegisterRuntimeExport("?TypeName@AnimatedImageDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_AnimatedImageDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x1800366d0ull, (void*)&RtUi_AnimatedImageDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@AnimatedImageDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_AnimatedImageDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800cb6a0ull, (void*)&RtUi_AnimatedImageDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@AnimatedImageDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_AnimatedImageDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180036870ull, (void*)&RtUi_AnimatedImageDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@AnimatedImageDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_AnimatedImageDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180036850ull, (void*)&RtUi_AnimatedImageDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@AnimatedImageDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_AnimatedImageDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180036880ull, (void*)&RtUi_AnimatedImageDescription_Destruct);

    RegisterRuntimeExport("??1AnimatedImageDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_AnimatedImageDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800cb6b0ull, (void*)&RtUi_AnimatedImageDescription_Dtor);

    RegisterRuntimeExport("?Serialize@AnimatedImageDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_AnimatedImageDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800cb5c0ull, (void*)&RtUi_AnimatedImageDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@AnimatedImageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_AnimatedImageDescription_CreateInstance);            ++n;
    RegisterRuntimeAddr(0x1800cb480ull, (void*)&RtUi_AnimatedImageDescription_CreateInstance);

    return n;
}
