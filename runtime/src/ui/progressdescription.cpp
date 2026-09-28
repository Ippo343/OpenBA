// ============================================================================
// runtime/src/ui/progressdescription.cpp — UI::ProgressDescription, the class
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription.
//
// ??_7ProgressDescription@UI@@6B@ (retail 0x1801591e0), ten slots:
//   0  deleting dtor      0x180077c80  deleting_dtor.cpp row (sizeof 0xb0)
//   1  TypeName           0x180077770  here
//   2  Type               0x180077750  here
//   3  GetFileVersion     0x18011a070  here  (returns 0x204)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x180119f60  here
//   6  Reconstruct        0x1800778f0  here
//   7  Destruct           0x180077920  here
//   8  CreateInstance     0x180119d00  here
//   9  Construct          0x180077910  here
// plus ??0ProgressDescription@UI@@QEAA@XZ 0x18011a210 (out of line) and
// ??1ProgressDescription@UI@@UEAA@XZ 0x18011a080.
//
// LAYOUT: runtime/types/ui/ProgressDescription.h (sizeof 0xb0; RULE B6-8 moved
// this file's offsets there under the retail name).
//
// ENDGAME B.3.6 block C5: every slot body MOVED into the members of the real
// `UI::ProgressDescription`; each registered door is a one-line QUALIFIED
// delegation. The ctor / dtor stay doors (B4b-2).
//
// Every helper is already ported, including ??0ProgressElement@UI@@
// (0x180119c40, ui_factory_wave28.cpp), so this file adds none.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/ui/ElementDescription.h"
#include "../../types/ui/Element.h"
#include "../../types/io/Serializer.h"
#include "../../types/ui/ProgressDescription.h"
#include "../../types/ui/ProgressElement.h"

#include <atomic>
#include <new>

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

struct Stream {
    uint8_t _head00[0x08];
    int32_t mode;
    int32_t _pad0c;
    void*   encodingFactory;
};
struct Serializer {
    Stream* stream;
    void*   readerWriter;
};
static_assert(offsetof(Stream, mode) == 0x08, "IO::Stream mode @ +0x08");
static_assert(offsetof(Stream, encodingFactory) == 0x10, "IO::Stream factory @ +0x10");
static_assert(offsetof(Serializer, readerWriter) == 0x08, "IO::Serializer rw @ +0x08");

void** VTableOf(void* o) { return *reinterpret_cast<void***>(o); }

using MemFreeFn = void (*)(void*);

void ReleaseStringField(char** field) {
    char* s = *field;
    if (s == nullptr) return;
    if (--*reinterpret_cast<int32_t*>(s - 4) == 0) {
        *field = s - 8;
        RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(*field);
    } else {
        *field = nullptr;
    }
}

}  // namespace

// ============================================================================
// The MEMBERS
// ============================================================================
namespace {
// RULE B6-5: namespace-scope atomics, never a function-local first-call cache.
void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}
std::atomic<void*> g_getSpriteIndex{nullptr};
// ?GetSpriteIndexFromName@SpriteSheet@UI@@ — block D's class, a door.
int32_t GetSpriteIndexFromName(const void* sheet, const void* nameStr) {
    using GetIdxFn = int32_t (*)(const void* sheet, const void* nameStr);
    return reinterpret_cast<GetIdxFn>(AtomicExport(
        g_getSpriteIndex,
        "?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z"))(sheet, nameStr);
}
}  // namespace

namespace UI {

// [3] 0x18011a070
int32_t ProgressDescription::GetFileVersion() const { return 0x204; }

// [9] 0x180077910 — the ctor, placement (the null test sat in the retired door).
Object::External* ProgressDescription::Construct() {
    return ::new (this) ProgressDescription();       // ENDGAME B.3.9: the real ctor
}
// [6] 0x1800778f0 — destroy in place through slot 0 (flag 0), re-run the ctor.
Object::External* ProgressDescription::Reconstruct() {
    DeletingDtor(0);
    return ::new (this) ProgressDescription();
}
// [7] 0x180077920
void ProgressDescription::Destruct() { DeletingDtor(0); }

// [5] ?Serialize@ProgressDescription@UI@@UEAA_NAEAVStream@IO@@@Z  0x180119f60
bool ProgressDescription::Serialize(void* streamArg) {
    Stream* stream = static_cast<Stream*>(streamArg);

    // QUALIFIED: this object wears the LEAF's table, whose slot 5 is this body.
    ElementDescription::Serialize(stream);                              // 180119f7a

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = IO::ReaderWriter* (*)(void* factory, Serializer* ser, int version);   // B.3.x bridge: the factory returns the class
    IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                              // 180119faa

    // kScalarUInt, NOT kScalarInt — vt+0x98 at 0x180119fbe.
    rw->SerializeUInt(_unknown68, nullptr);

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x180004590ull)(&_spriteSheet, &ser);            // 180119fcd ExternalPtr<SpriteSheet>
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_fillLeftName, &ser);           // 180119fdb
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_fillMiddleName, &ser);         // 180119fec
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_fillRightName, &ser);          // 180119ffd
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_trackLeftName, &ser);          // 18011a00e
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_trackMiddleName, &ser);        // 18011a01f
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_trackRightName, &ser);         // 18011a030
    RtAddr<FieldSerFn>(0x180038f90ull)(&_childDescription, &ser);       // 18011a041 ExternalPtr<ElementDescription>

    if (rw != nullptr) {                                                // 18011a04f
        rw->DeletingDtor(1);
    }
    return true;
}

// ============================================================================
// [8] ?CreateInstance@ProgressDescription@UI@@...   retail 0x180119d00
// ============================================================================
// ProgressElement is 0x130 bytes (`MOV EDX,0x130` at 0x180119d49); its ctor is
// CALLED out of line (0x180119c40, already ported).
//
// SIX {sheet, index} pairs written into +0xc0/+0xc8 .. +0x110/+0x118, with the
// same dword-written/qword-read stack slot the Image, Slider and Tab bodies
// have (0x180119da5 writes `dword [RBP-0x10]`, 0x180119db8 reads
// `qword [RBP-0x10]`), and the sheet pointer RE-READ from the description
// before every one of the six.
//
// THE CHILD IS CONDITIONAL: `CMP qword [RSI+0xa8],0 ; JZ` at 0x180119ed1 skips
// the whole create-parent-push-dirty block when the description has no child.
// Every other class in this batch creates its children unconditionally.
Object::InternalPtr<Element>* ProgressDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0x130, 8);          // 180119d49 (inlined)

    ProgressElement* elem = static_cast<ProgressElement*>(raw);
    if (raw != nullptr)                                                 // 180119d73
        elem = ::new (raw) ProgressElement();                           // 180119d78 ??0 (out of line)

    void* slot = elem;                                                  // 180119d80
    // The NON-virtual ElementDescription::CreateInstance(InternalPtr<Element>)
    // (0x180115ad0): a QUALIFIED member call since fragment pass F2.
    UI::ElementDescription::CreateInstance(                   // 180119d8b
        reinterpret_cast<Object::InternalPtr<Element>*>(&slot));  // B.3.6 bridge

    uint64_t idxSlot;                                                   // [RBP-0x10]
    char* const* const names[6] = { &_fillLeftName,  &_fillMiddleName,  &_fillRightName,
                                    &_trackLeftName, &_trackMiddleName, &_trackRightName };
    ::Sprite* const sprites[6] = { &elem->_fillLeft,  &elem->_fillMiddle,  &elem->_fillRight,
                                   &elem->_trackLeft, &elem->_trackMiddle, &elem->_trackRight };
    for (int i = 0; i < 6; ++i) {
        void* sheet = _spriteSheet;                                     // RE-READ each time
        if (sheet != nullptr) {
            const int32_t idx = GetSpriteIndexFromName(sheet, names[i]);
            __builtin_memcpy(&idxSlot, &idx, sizeof idx);               // DWORD store
        } else {
            const int32_t minusOne = -1;
            __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);
        }
        uint8_t* s = reinterpret_cast<uint8_t*>(sprites[i]);
        *reinterpret_cast<void**>(s)        = sheet;
        *reinterpret_cast<uint64_t*>(s + 8) = idxSlot;                  // QWORD store
    }

    if (_childDescription != nullptr) {                                 // 180119ed1
        // UI::Dialog::CreateElement (block B's class) — a door, by address.
        using CreateElementFn = void** (*)(const void* dlg, void** ret, void* desc);
        void** made = RtAddr<CreateElementFn>(0x180112ed0ull)(
            dialog, &slot, _childDescription);                          // 180119ee9

        Element* child = *reinterpret_cast<Element**>(made);            // 180119eee
        if (child != nullptr)
            child->_parent = elem;                                      // 180119ef6

        using PushFn = void (*)(void* array, int n, const void* fill);
        RtAddr<PushFn>(0x180076900ull)(&elem->_children,
                                       elem->_children.GetSize() + 1,
                                       made);                           // 180119f0c

        for (Element* n = elem; n != nullptr; n = n->_parent)           // 180119f20..180119f34
            n->_flags |= kElementFlagNeedsSizeCalc;
    }

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 180119f39
    return ret;
}

// ??1ProgressDescription@UI@@UEAA@XZ  0x18011a080 — retail's body, moved from
// the retired door (ENDGAME B.3.9 UI-B): the six Strings in REVERSE order. The
// restamps, the base's _textName (+0x60) and the tail JMP into ~External are
// the compiler's chain. Neither ExternalPtr is released.
ProgressDescription::~ProgressDescription() {
    uint8_t* p = reinterpret_cast<uint8_t*>(this);
    ReleaseStringField(reinterpret_cast<char**>(p + 0xa0));    // 18011a0a5
    ReleaseStringField(reinterpret_cast<char**>(p + 0x98));    // 18011a0d8
    ReleaseStringField(reinterpret_cast<char**>(p + 0x90));    // 18011a10a
    ReleaseStringField(reinterpret_cast<char**>(p + 0x88));    // 18011a13c
    ReleaseStringField(reinterpret_cast<char**>(p + 0x80));    // 18011a16e
    ReleaseStringField(reinterpret_cast<char**>(p + 0x78));    // 18011a1a0
}

// ?CreateNewObject@ProgressDescription@UI@@SAPEAVBase@Object@@XZ 0x180077860 --
// `?MemAlloc@MemoryAllocator` (0x1800c7a50) of 0xb0, align 8; retail's null
// branch; the ctor. Replaces the factory interpreter row (B39_RULES D2).
Object::Base* ProgressDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ProgressDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) ProgressDescription();
}
// ?CreateInPlaceObject@ProgressDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x1800778e0
Object::Base* ProgressDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) ProgressDescription();
}

}  // namespace UI

// ============================================================================
// ENDGAME B.3.9 UI-B: the slot doors, the ctor / dtor doors and the
// factory-interpreter row are gone. Their only readers were the minted
// `??_7ProgressDescription@UI@@6B@`, its RT_DELDTOR row and FACTUI2: every live
// ProgressDescription now wears the compiler's table.
// ============================================================================

int RtUiProgressDescriptionRegister() {
    int n = 0;
    // The factory pair (B39_RULES D2), the class's own statics, by name AND
    // address; the minted `?_typeInfo@ProgressDescription` record resolves them.
    RegisterRuntimeExport("?CreateNewObject@ProgressDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::ProgressDescription::CreateNewObject);      ++n;
    RegisterRuntimeAddr(0x180077860ull, (void*)&UI::ProgressDescription::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@ProgressDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::ProgressDescription::CreateInPlaceObject);  ++n;
    RegisterRuntimeAddr(0x1800778e0ull, (void*)&UI::ProgressDescription::CreateInPlaceObject);
    return n;
}

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::ProgressDescription's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@ProgressDescription@UI@@ (0x1801facf0): name "ProgressDescription", parent
// "ElementDescription" (retail ?_ParentTypeName@ProgressDescription@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::ProgressDescription, "ElementDescription", nullptr)
