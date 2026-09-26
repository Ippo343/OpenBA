// ============================================================================
// runtime/src/ui/labeldescription.cpp — UI::LabelDescription, the whole class
// ============================================================================
//
// ??_7LabelDescription@UI@@6B@ (retail 0x180156b30), ten slots:
//   0  deleting dtor      0x180042730  runtime/src/object/deleting_dtor.cpp row
//   1  TypeName           0x1800423b0  here
//   2  Type               0x180042390  here
//   3  GetFileVersion     0x1800d2950  here  (returns 0x2304)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d2820  here
//   6  Reconstruct        0x180042530  here
//   7  Destruct           0x180042560  here
//   8  CreateInstance     0x1800d26b0  here
//   9  Construct          0x180042550  here
// plus ??1LabelDescription@UI@@UEAA@XZ (0x1800d2960).
//
// ENDGAME B.3.6 block C3: every slot body is a `UI::LabelDescription` member
// (runtime/types/ui/LabelDescription.h) and the doors delegate with a
// QUALIFIED call. It DERIVES FROM UI::TextDescription, a real class in the same
// block now, so Serialize's base call is the qualified member
// `UI::TextDescription::Serialize`. Two TextDescription bodies stay DOORS and
// are still reached by address:
//     0x18011eae0   ~TextDescription   (the dtor's TAIL JUMP — a door body)
//     0x18011ef70   the TextDescription applier (element_bodies_b5.cpp's door)
//
// The constructor is OUT OF LINE (`??0LabelDescription@UI@@QEAA@XZ`,
// 0x1800d29d0) and is ported by runtime/src/ui/ui_factory_wave28.cpp.
//
// GetFileVersion is 0x2304 — the largest constant in this batch, and read from
// this class's own body. It is NOT its base's.
// ============================================================================
#include <atomic>

#include "description_common.h"
#include "../../types/ui/LabelDescription.h"
#include "../../types/ui/LabelElement.h"
#include "../../types/ui/SpriteSheet.h"
#include "../../types/system/String.h"

namespace {

using namespace rtdesc;

void* VtLabelDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblLabelDescription); }

// RULE B6-5: member bodies resolve through namespace-scope atomics (the same
// never-cache-a-null contract as element_common.h's rtuielem::AtomicExport,
// which cannot be included beside description_common.h — both declare Module).
void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}
std::atomic<void*> g_typeInfoSlot{nullptr};
std::atomic<void*> g_ctorSlot{nullptr};

const uint8_t* LabelTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfoSlot, "?_typeInfo@LabelDescription@UI@@0VBaseTypeInfo@Object@@A"));
}

// ??0LabelDescription@UI@@QEAA@XZ — a ctor DOOR (ui_factory_wave28.cpp).
void* LabelCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return reinterpret_cast<CtorFn>(AtomicExport(
        g_ctorSlot, "??0LabelDescription@UI@@QEAA@XZ"))(self);
}

}  // namespace

namespace UI {

uint32_t LabelDescription::Type() const {
    uint32_t v = 0; __builtin_memcpy(&v, LabelTypeInfo(), sizeof v); return v;
}
const char* LabelDescription::TypeName() const {
    const char* v = nullptr; __builtin_memcpy(&v, LabelTypeInfo() + 8, sizeof v); return v;
}
int32_t LabelDescription::GetFileVersion() const { return 0x2304; }

// 0x180042550: null test (in the DOOR), then a JMP to the ctor.
Object::External* LabelDescription::Construct() {
    LabelCtor(this);                                         // 180042559 JMP
    return this;
}
Object::External* LabelDescription::Reconstruct() {
    DeletingDtor(0);                                         // 18004253e
    LabelCtor(this);                                         // 180042548 JMP
    return this;
}
void LabelDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// ?Serialize@LabelDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d2820
// ============================================================================
// Base is TextDescription::Serialize (0x18011ee30), QUALIFIED. THE SPRITE PAIR
// COMES FIRST, BEFORE THE SIX DWORDS — offset order would put +0x90..+0xa4
// first, and the byte stream a .rsc parses is this order, not that one.
bool LabelDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    UI::TextDescription::Serialize(stream);                  // 1800d283a

    Serializer ser = { stream, nullptr };
    void* rw = MakeReaderWriter(stream, &ser);

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x180004590ull)(&_spriteSheet, &ser); // 1800d287b ExternalPtr<SpriteSheet>
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_spriteName, &ser);  // 1800d288c System::String

    RwScalar(rw, kRwScalarUInt, &_word90);                   // 1800d28a3
    RwScalar(rw, kRwScalarUInt, &_word94);                   // 1800d28bb
    RwScalar(rw, kRwScalarInt,  &_word98);                   // 1800d28d3
    RwScalar(rw, kRwScalarInt,  &_word9c);                   // 1800d28eb
    RwScalar(rw, kRwScalarInt,  &_worda0);                   // 1800d2903
    RwScalar(rw, kRwScalarInt,  &_worda4);                   // 1800d291b

    DestroyReaderWriter(rw);                                 // 1800d2934
    return true;
}

// ============================================================================
// ?CreateInstance@LabelDescription@UI@@...   retail 0x1800d26b0  (slot 8)
// ============================================================================
// RETSLOT (B6-2). LabelElement is 0x130 and its constructor is OUT OF LINE
// (0x1800d2620, a door). The base applier is TextDescription's (0x18011ef70,
// element_bodies_b5.cpp's door, reached by address) and it DOES take the Dialog.
//
// THE SPRITE ASSIGNMENT IS GUARDED BY A CHANGE TEST, and the two halves of the
// pair are compared SEPARATELY (`CMP qword [RDI+0x100],RBX` then
// `CMP dword [RDI+0x108],EAX`) before the mark:
//     idx = sheet ? sheet->GetSpriteIndexFromName(_spriteName) : -1
//     if (elem->sheet != sheet || elem->index != idx) mark the chain
//     elem->sheet = sheet ; elem->index = idx     <- STORED UNCONDITIONALLY
// and the index goes in as a QWORD read from a slot only a DWORD was written to
// (0x1800d275f writes `dword [RSP+0x30]`, 0x1800d279d reads `qword`).
//
// The last copy is `ElementDescription::_id` (+0x3c) into +0x128 (_iconColor).
Object::InternalPtr<Element>* LabelDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    void* raw = MemAlloc(0x130, 8);                          // 1800d26f2

    LabelElement* elem = static_cast<LabelElement*>(raw);
    if (raw != nullptr) {                                    // 1800d271d
        using LabelElementCtorFn = void* (*)(void*);
        elem = static_cast<LabelElement*>(
            RtAddr<LabelElementCtorFn>(0x1800d2620ull)(raw));  // 1800d2722
    }

    void* slot = elem;                                       // 1800d272a
    using TextApplyFn = void (*)(const void*, void**, const void*);
    RtAddr<TextApplyFn>(0x18011ef70ull)(this, &slot, dialog);  // 1800d273a

    {
        SpriteSheet* sheet = _spriteSheet;                   // 1800d273f
        int32_t idx;
        if (sheet != nullptr) {                              // 1800d2749
            idx = sheet->GetSpriteIndexFromName(               // 1800d2755
                *reinterpret_cast<const System::String*>(&_spriteName));
        } else {
            idx = -1;                                        // 1800d275c OR EAX,-1
        }
        uint64_t idxSlot;                                    // [RSP+0x30]
        __builtin_memcpy(&idxSlot, &idx, sizeof idx);        // 1800d275f DWORD store

        if (elem->_sprite.spriteSheet != sheet ||                         // 1800d2763
            elem->_sprite.spriteIndex != idx) {                           // 1800d276c
            elem->SetNeedsSizeCalc();                                     // 1800d2780
        }
        elem->_sprite.spriteSheet = sheet;                                // 1800d2796
        __builtin_memcpy(&elem->_sprite.spriteIndex, &idxSlot, 8);        // 1800d27a2 QWORD
    }

    elem->_align        = static_cast<int32_t>(_word90);     // 1800d27a9 .. 1800d27eb
    elem->_mode         = _word94;
    elem->_minTextWidth = _word98;
    elem->_gap          = _word9c;
    elem->_iconWidth    = _worda0;
    elem->_iconHeight   = _worda4;

    elem->_iconColor = static_cast<uint32_t>(_id);           // 1800d27f1

    *reinterpret_cast<void**>(ret) = elem;                   // 1800d27fa
    return ret;
}

}  // namespace UI

// ============================================================================
// ??1LabelDescription@UI@@UEAA@XZ   0x1800d2960 — a DOOR BODY (no real dtor)
// ============================================================================
// Stamp own vtable, release the sprite name at +0xb0, TAIL-JUMP to
// ~TextDescription (0x18011eae0). It does NOT stamp a base vtable itself and it
// does NOT touch +0x60 — its base does both. The +0xa8 ExternalPtr is not
// released.
extern "C" void RtUi_LabelDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);

    *reinterpret_cast<void**>(p) = VtLabelDescription();     // 1800d297d
    rtdesc::ReleaseStringField(p, offsetof(UI::LabelDescription, _spriteName));  // 1800d2980

    using BaseDtorFn = void (*)(void*);
    rtdesc::RtAddr<BaseDtorFn>(0x18011eae0ull)(p);           // 1800d29be JMP
}

// ============================================================================
// The registered doors — one QUALIFIED delegation each.
// ============================================================================
extern "C" uint32_t RtUi_LabelDescription_Type(const void* self) {
    return reinterpret_cast<const UI::LabelDescription*>(self)->UI::LabelDescription::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_LabelDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::LabelDescription*>(self)->UI::LabelDescription::TypeName();   // B.3.6 bridge
}
extern "C" int32_t RtUi_LabelDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::LabelDescription*>(self)->UI::LabelDescription::GetFileVersion();   // B.3.6 bridge
}
extern "C" void* RtUi_LabelDescription_Construct(void* self) {
    if (self == nullptr) return nullptr;                     // 180042553
    return reinterpret_cast<UI::LabelDescription*>(self)->UI::LabelDescription::Construct();   // B.3.6 bridge
}
extern "C" void* RtUi_LabelDescription_Reconstruct(void* self) {
    return reinterpret_cast<UI::LabelDescription*>(self)->UI::LabelDescription::Reconstruct();   // B.3.6 bridge
}
extern "C" void RtUi_LabelDescription_Destruct(void* self) {
    reinterpret_cast<UI::LabelDescription*>(self)->UI::LabelDescription::Destruct();   // B.3.6 bridge
}
extern "C" bool RtUi_LabelDescription_Serialize(void* self, void* stream) {
    return reinterpret_cast<UI::LabelDescription*>(self)->UI::LabelDescription::Serialize(stream);   // B.3.6 bridge
}
// RDX is the caller's slot; the member writes it and hands it back (B6-2).
extern "C" void** RtUi_LabelDescription_CreateInstance(const void* self, void** ret,
                                                       const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::LabelDescription*>(self)->UI::LabelDescription::CreateInstance(   // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
            static_cast<const UI::Dialog*>(dialog)));
}

int RtUiLabelDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@LabelDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_LabelDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180042390ull, (void*)&RtUi_LabelDescription_Type);

    RegisterRuntimeExport("?TypeName@LabelDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_LabelDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x1800423b0ull, (void*)&RtUi_LabelDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@LabelDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_LabelDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800d2950ull, (void*)&RtUi_LabelDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@LabelDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_LabelDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180042550ull, (void*)&RtUi_LabelDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@LabelDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_LabelDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180042530ull, (void*)&RtUi_LabelDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@LabelDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_LabelDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180042560ull, (void*)&RtUi_LabelDescription_Destruct);

    RegisterRuntimeExport("??1LabelDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_LabelDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d2960ull, (void*)&RtUi_LabelDescription_Dtor);

    RegisterRuntimeExport("?Serialize@LabelDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_LabelDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800d2820ull, (void*)&RtUi_LabelDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@LabelDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_LabelDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800d26b0ull, (void*)&RtUi_LabelDescription_CreateInstance);

    return n;
}
