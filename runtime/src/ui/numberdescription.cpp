// ============================================================================
// runtime/src/ui/numberdescription.cpp — UI::NumberDescription, whole class
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription.
//
// ??_7NumberDescription@UI@@6B@ (retail 0x180157530), ten slots:
//   0  deleting dtor      0x18004a760  deleting_dtor.cpp row (sizeof 0x88)
//   1  TypeName           0x18004a240  here
//   2  Type               0x18004a220  here
//   3  GetFileVersion     0x180118e20  here  (returns 0x104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x180118e30  here
//   6  Reconstruct        0x18004a4c0  here
//   7  Destruct           0x18004a610  here
//   8  CreateInstance     0x180118f10  here
//   9  Construct          0x18004a570  here
// plus ??1NumberDescription@UI@@UEAA@XZ 0x180118c00.
//
// THE CONSTRUCTOR IS INLINED, NOT OUT OF LINE. Unlike Text/Radio/Page/
// NumberEdit, there is no ??0NumberDescription export to register: the body
// appears twice in the shipped DLL, once inside Construct (0x18004a586..) and
// once inside Reconstruct (0x18004a4de..). The two were compared instruction by
// instruction for this port and agree; it is written once here and both slots
// call it.
//
// LAYOUT, sizeof 0x88 (`MOV EDX,0x88` at 0x18004a77f). ElementDescription
// (0x68) plus:
//     +0x68  ExternalPtr<UI::Font>   ctor 0, serialized by 0x18003f1e0
//     +0x70  uint32  _u70   kScalarUInt (vt+0x98) — NOT the int slot
//     +0x74  int32   _i74   kScalarInt
//     +0x78  float   _f78   kScalarFloat (vt+0x80); the ctor writes it as an
//                           integer zero (`MOV dword [RCX+0x78],EDX`), which is
//                           0.0f, so nothing is lost by calling it a float —
//                           but the SERIALIZER is what says it is one.
//     +0x7c  bool    _b7c
//     +0x7d..+0x87 never written by the ctor
// The ctor's +0x68 store is ONE QWORD covering +0x68..+0x6f and the +0x70 store
// is another covering +0x70..+0x77, which is what pins _u70 and _i74 as the two
// halves of that second qword.
//
// NOT PORTED HERE: ??0NumberElement@UI@@QEAA@XZ (0x180118c70), reached through
// ResolveAddr — the *Element constructors belong to the Element files.
//
// ENDGAME B.3.6 block C3: every slot body is a `UI::NumberDescription` member
// (runtime/types/ui/NumberDescription.h, the layout above moved out of this
// file — RULE B6-8). The doors delegate with a QUALIFIED call. CreateInstance
// writes a real `UI::NumberElement`.
// ============================================================================

#include "element_common.h"   // rtuielem::AtomicExport (RULE B6-5), ResolveAddr
#include "../../types/ui/NumberDescription.h"
#include "../../types/ui/NumberElement.h"
#include "../../types/io/Serializer.h"
#include "../../types/system/MemoryAllocator.h"

namespace {

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

}  // namespace

namespace UI {

int32_t NumberDescription::GetFileVersion() const { return 0x104; }

// 0x18004a570: null test (in the DOOR), the INLINED ctor, `MOV RAX,RCX`.
// The inlined ctor (0x18004a586..0x18004a600, Reconstruct's copy at
// 0x18004a4de agrees) is the real ctor now (NumberDescription.h).
Object::External* NumberDescription::Construct() {
    return ::new (this) NumberDescription();                   // 18004a600
}
Object::External* NumberDescription::Reconstruct() {
    this->~NumberDescription();                                           // 18004a4d7
    return ::new (this) NumberDescription();                   // 18004a558
}
void NumberDescription::Destruct() { this->~NumberDescription(); }

// ============================================================================
// ?Serialize@NumberDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x180118e30
// ============================================================================
//   ElementDescription::Serialize                   0x180116b00 (qualified)
//   ExternalPtr<UI::Font>::Serialize(&_font, &pair) 0x18003f1e0
//   kScalarUInt  (vt+0x98)  _u70
//   kScalarInt   (vt+0xa0)  _i74
//   kScalarFloat (vt+0x80)  _f78
//   kScalarBool  (vt+0x78)  _b7c
//
// FOUR DIFFERENT SCALAR SLOTS IN FOUR CONSECUTIVE CALLS. This is the body that
// says what the four fields' types are; nothing else in the class does.
bool NumberDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    UI::ElementDescription::Serialize(stream);                 // 180118e4a

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = IO::ReaderWriter* (*)(void* factory, Serializer* ser, int version);   // B.3.x bridge: the factory returns the class
    IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 180118e7a

    RtObject_ExternalPtrSerialize_Font(reinterpret_cast<RtObject::External**>(&_font), reinterpret_cast<rt::io::Serializer*>(&ser));          // 180118e88

    rw->SerializeUInt(_u70, nullptr);
    rw->SerializeInt(_i74, nullptr);
    rw->SerializeFloat(_f78, nullptr);
    rw->SerializeBool(reinterpret_cast<bool&>(_b7c), nullptr);

    if (rw != nullptr) {                                       // 180118ee7
        delete rw;
    }
    return true;
}

// ============================================================================
// ?CreateInstance@NumberDescription@UI@@...   retail 0x180118f10  (slot 8)
// ============================================================================
// RETSLOT (B6-2); R8 (the Dialog) is never read.
//
// NumberElement is 0xe8 bytes (`MOV EDX,0xe8` at 0x180118f51) and its
// constructor is CALLED out of line (0x180118c70, the real ctor), not inlined.
//
// THE FIVE COPIES ARE NOT AT MATCHING OFFSETS on the two objects, and the gap
// is not uniform: _font -> +0xc0, _u70 -> _mode (+0xc8), _i74 -> _intValue
// (+0xd8), _f78 -> _floatValue (+0xdc), _b7c -> _showSign (+0xe0).
Object::InternalPtr<Element>* NumberDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* /*dialog*/) const {
    void* raw = System::MemoryAllocator::MemAlloc(0xe8, 8);    // 180118f51 (inlined)

    NumberElement* elem = nullptr;
    if (raw != nullptr)                                        // 180118f7c
        elem = ::new (raw) NumberElement();                    // 180118f81 ??0NumberElement

    void* slot = elem;                                         // 180118f8d
    // The non-virtual ElementDescription::CreateInstance(InternalPtr<Element>)
    // (0x180115ad0) — a QUALIFIED member call since fragment pass F2.
    UI::ElementDescription::CreateInstance(                   // 180118f9a
        reinterpret_cast<Object::InternalPtr<Element>*>(&slot));  // B.3.6 bridge

    elem->_font = _font;                                       // 180118fa3
    elem->_mode = static_cast<int32_t>(_u70);                  // 180118fad
    elem->_intValue = _i74;                                    // 180118fb6
    __builtin_memcpy(&elem->_floatValue, &_f78, 4);            // 180118fbf  4 raw bytes
    elem->_showSign = _b7c;                                    // 180118fc9

    *reinterpret_cast<void**>(ret) = elem;                     // 180118fcf
    return ret;
}

}  // namespace UI

// ============================================================================
// ??1NumberDescription@UI@@UEAA@XZ   0x180118c00
// ============================================================================
// ONE String release — ElementDescription's _textName at +0x60, under the BASE
// vtable — then a tail JMP to ??1External. ENDGAME B.3.9 UI-A: EMPTY; the
// compiler chains ~ElementDescription and ~External. The ExternalPtr<Font> at
// +0x68 is not released; that is retail's.
namespace UI {
NumberDescription::~NumberDescription() {}

// ?CreateNewObject@NumberDescription@UI@@SAPEAVBase@Object@@XZ  0x18004a330
// The FACTUI2 row this replaces: MemAlloc(0x88, 8), retail's null branch,
// then the inlined ctor.
Object::Base* NumberDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(NumberDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) NumberDescription();
}
// ?CreateInPlaceObject@NumberDescription@UI@@SAPEAVBase@Object@@PEAX@Z  0x18004a420
Object::Base* NumberDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) NumberDescription();
}
}  // namespace UI

// ============================================================================
// Registration. ENDGAME B.3.9 UI-A: the slot doors and the ??1 door (and the
// FACTUI2 row's ??0 door), read only by the retired minted table / RT_DELDTOR
// row / factory interpreter, are gone. The factory pair (B39_RULES D2) is the
// class's own.
// ============================================================================
int RtUiNumberDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?CreateNewObject@NumberDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::NumberDescription::CreateNewObject);        ++n;
    RegisterRuntimeAddr(0x18004a330ull, (void*)&UI::NumberDescription::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@NumberDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::NumberDescription::CreateInPlaceObject);    ++n;
    RegisterRuntimeAddr(0x18004a420ull, (void*)&UI::NumberDescription::CreateInPlaceObject);
    return n;
}

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::NumberDescription's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@NumberDescription@UI@@ (0x1801fac80): name "NumberDescription", parent
// "ElementDescription" (retail ?_ParentTypeName@NumberDescription@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::NumberDescription, "ElementDescription", nullptr)
