// ============================================================================
// runtime/src/ui/labeldescription.cpp — UI::LabelDescription, the whole class
// ============================================================================
//
// ??_7LabelDescription@UI@@6B@ (retail 0x180156b30), ten slots:
//   0  deleting dtor      0x180042730  LabelDescription.h (the class's own)
//   1  TypeName           0x1800423b0  here
//   2  Type               0x180042390  here
//   3  GetFileVersion     0x1800d2950  here  (returns 0x2304)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d2820  here
//   6  Reconstruct        0x180042530  here
//   7  Destruct           0x180042560  here
//   8  CreateInstance     0x1800d26b0  here
//   9  Construct          0x180042550  here
// plus ??1LabelDescription@UI@@UEAA@XZ (0x1800d2960) and the factory pair.
//
// ENDGAME B.3.6 block C3: every slot body is a `UI::LabelDescription` member
// (runtime/types/ui/LabelDescription.h) and the doors delegate with a
// QUALIFIED call. It DERIVES FROM UI::TextDescription, a real class in the same
// block now, so Serialize's base call is the qualified member
// `UI::TextDescription::Serialize`. The TextDescription applier
// (0x18011ef70, element_bodies_b5.cpp's non-slot door) is still reached by
// address.
//
// ENDGAME B.3.9 block UI-A (B39_RULES D1/D2): the class is CONCRETE. The
// out-of-line ctor (`??0LabelDescription@UI@@QEAA@XZ`, 0x1800d29d0) is the
// real one in the header; ~LabelDescription chains ~TextDescription as a
// compiler base call; the factory pair is the class's own. The slot doors, the
// ??1 door and ui_factory_wave28.cpp's ??0 row, read only by the retired
// minted table, its RT_DELDTOR row and the interpreter, are gone.
//
// GetFileVersion is 0x2304 — the largest constant in this batch, and read from
// this class's own body. It is NOT its base's.
// ============================================================================

#include "description_common.h"
#include "../../types/ui/LabelDescription.h"
#include "../../types/ui/LabelElement.h"
#include "../../types/ui/SpriteSheet.h"
#include "../../types/system/String.h"

namespace {

using namespace rtdesc;

}  // namespace

namespace UI {

int32_t LabelDescription::GetFileVersion() const { return 0x2304; }

// 0x180042550: null test (it sat in the retired door), then a JMP to the ctor.
Object::External* LabelDescription::Construct() {
    return ::new (this) LabelDescription();                  // 180042559 JMP
}
Object::External* LabelDescription::Reconstruct() {
    this->~LabelDescription();                                         // 18004253e
    return ::new (this) LabelDescription();                  // 180042548 JMP
}
void LabelDescription::Destruct() { this->~LabelDescription(); }

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
    IO::ReaderWriter* rw = MakeReaderWriter(stream, &ser);

    RtObject_ExternalPtrSerialize_SpriteSheet(ExtField(&_spriteSheet), IoSer(&ser)); // 1800d287b ExternalPtr<SpriteSheet>
    StringSerialize(&_spriteName, &ser);  // 1800d288c System::String

    rw->SerializeUInt(_word90, nullptr);                     // 1800d28a3
    rw->SerializeUInt(_word94, nullptr);                     // 1800d28bb
    rw->SerializeInt(_word98, nullptr);                      // 1800d28d3
    rw->SerializeInt(_word9c, nullptr);                      // 1800d28eb
    rw->SerializeInt(_worda0, nullptr);                      // 1800d2903
    rw->SerializeInt(_worda4, nullptr);                      // 1800d291b

    if (rw != nullptr) delete rw;                  // 1800d2934
    return true;
}

// ============================================================================
// ?CreateInstance@LabelDescription@UI@@...   retail 0x1800d26b0  (slot 8)
// ============================================================================
// RETSLOT (B6-2). LabelElement is 0x130 and its constructor is OUT OF LINE
// (0x1800d2620, the real ctor since B.3.9). The base applier is TextDescription's (0x18011ef70,
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
    if (raw != nullptr)                                      // 1800d271d
        elem = ::new (raw) LabelElement();                   // 1800d2722 ??0LabelElement

    void* slot = elem;                                       // 1800d272a
    TextDescription::CreateInstance(                         // 1800d273a  0x18011ef70
        reinterpret_cast<Object::InternalPtr<TextElement>*>(&slot), dialog);   // B.3.x bridge

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
// ??1LabelDescription@UI@@UEAA@XZ   0x1800d2960 -- retail's body, moved here
// from the retired door (ENDGAME B.3.9 UI-A)
// ============================================================================
// Stamp own vtable (the compiler's), release the sprite name at +0xb0 through
// System::String's own Release, TAIL-JUMP to ~TextDescription (0x18011eae0) --
// the compiler's base call, which restamps and releases +0x78 and +0x60. The
// +0xa8 ExternalPtr is not released.
namespace UI {
LabelDescription::~LabelDescription() {
    System::StringPrivateAccess::Release(
        *reinterpret_cast<System::String*>(&_spriteName));   // 1800d2980
}
}  // namespace UI

namespace UI {
// ?CreateNewObject@LabelDescription@UI@@SAPEAVBase@Object@@XZ  0x1800424a0
// The factory-interpreter row this replaces: MemAlloc(0xb8, 8), retail's
// null branch, then the out-of-line ctor ??0LabelDescription (0x1800d29d0).
Object::Base* LabelDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(LabelDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) LabelDescription();
}
// ?CreateInPlaceObject@LabelDescription@UI@@SAPEAVBase@Object@@PEAX@Z  0x180042520
Object::Base* LabelDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) LabelDescription();
}
}  // namespace UI

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::LabelDescription's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@LabelDescription@UI@@ (0x1801fa008): name "LabelDescription", parent
// "TextDescription" (retail ?_ParentTypeName@LabelDescription@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::LabelDescription, "TextDescription", nullptr)
