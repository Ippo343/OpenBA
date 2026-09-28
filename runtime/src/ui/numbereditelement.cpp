// ============================================================================
// runtime/src/ui/numbereditelement.cpp — UI::NumberEditElement
// ============================================================================
//
// `??_7NumberEditElement@UI@@6B@` (retail 0x180156f70). Only four of its slots
// are the class's own; slots 4..7 are `UI::EditElement`'s own bodies
// (0x1800d0c10 / 0x1800d0ab0 / 0x1800d0820 / 0x1800d0220), INHERITED in C++
// exactly as in the table:
//
//   0  vector deleting dtor  0x180046540  -> NumberEditElement.h (the class's own)
//   1  TypeName              0x180046250  here
//   2  Type                  0x180046230  here
//   4..7                     EditElement's (editelement.cpp)
//   12 FinalizeText          0x1800d4a60  here  (overrides EditElement's
//                                               0x1800d0da0)
//
// plus ??1NumberEditElement (0x1800d4c40) and the factory pair. ENDGAME B.3.9
// UI-A: concrete, the real ctor ??0NumberEditElement (0x1800d4c70) is in the
// header; the doors are retired.
//
// ENDGAME B.3.6 block C3: the three slot bodies are `UI::NumberEditElement`
// members (runtime/types/ui/NumberEditElement.h). LAYOUT: sizeof 0x100 over
// EditElement's RETAIL 0xF8 — `_min` @ +0xF8 and `_max` @ +0xFC begin exactly
// at EditElement's end (the old 0xE8 + pad arithmetic is gone). The text field
// this class reads is EditElement's `_text` at +0xC8.
// ============================================================================
#include <atomic>

#include "element_common.h"
#include "../../types/ui/NumberEditElement.h"
#include "../../types/system/String.h"

using namespace rtuielem;

namespace {

using ToIntegerFn = int32_t (*)(const char* s);

void MemFree(void* p) { System::MemoryAllocator::MemFree(p); }

// The inlined String::operator= (release old with retail's asymmetry, store,
// addref) and the release of the caller's temporary.
void AssignString(char*& field, char* nv) {
    char* old = field;
    if (old == nv) return;
    if (old != nullptr) {
        int32_t* rc = reinterpret_cast<int32_t*>(old - 4);
        if (--*rc == 0) {
            field = field - 8;
            MemFree(field);
        } else {
            field = nullptr;
        }
    }
    field = nv;
    if (nv != nullptr) ++*reinterpret_cast<int32_t*>(nv - 4);
}
void ReleaseStringLocal(void** slot) {
    uint8_t* p = static_cast<uint8_t*>(*slot);
    if (p == nullptr) return;
    int32_t* rc = reinterpret_cast<int32_t*>(p - 4);
    if (--*rc == 0) { *slot = p - 8; MemFree(*slot); }
}

}  // namespace

namespace UI {

// ??1NumberEditElement@UI@@UEAA@XZ 0x1800d4c40 — restamp, then the tail into
// ~EditElement: EMPTY here, the compiler chains ~EditElement (ENDGAME B.3.9
// UI-A; the retired door called it by address).
NumberEditElement::~NumberEditElement() {}

// ---------------------------------------------------------------------------
// slot 12 — ?FinalizeText@NumberEditElement@UI@@UEAAXXZ   0x1800d4a60
//
// Clamp the edited text to [_min, _max] and write it back formatted. The
// clamp is retail's, and it is NOT symmetric: the value is taken only when it
// is >= _min, and only then compared against _max, so a value below _min
// leaves `_min` itself as the result. It does NOT call EditElement's (empty)
// FinalizeText.
// ---------------------------------------------------------------------------
void NumberEditElement::FinalizeText() {
    const int32_t v = RtAddr<ToIntegerFn>(0x18003f620ull)(_text);

    int32_t clamped = _min;
    if (_min <= v) {
        clamped = v;
        if (_max < v) clamped = _max;
    }

    void* tmp = nullptr;
    // `?Format@String@System@@SA?AV12@PEBDZZ` (0x1800c9730) returns BY VALUE:
    // its hidden return slot is `tmp`.
    new (static_cast<void*>(&tmp)) System::String(System::String::Format("%i", clamped));
    AssignString(_text, static_cast<char*>(tmp));
    ReleaseStringLocal(&tmp);
}

}  // namespace UI

namespace UI {
// ?CreateNewObject@NumberEditElement@UI@@SAPEAVBase@Object@@XZ  0x180046340 —
// the FACTUI2 row this replaces: MemAlloc(0x100, 8), retail's null branch,
// then the out-of-line ctor ??0NumberEditElement (0x1800d4c70).
Object::Base* NumberEditElement::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(NumberEditElement), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) NumberEditElement();
}
// ?CreateInPlaceObject@NumberEditElement@UI@@SAPEAVBase@Object@@PEAX@Z  0x1800463e0
Object::Base* NumberEditElement::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) NumberEditElement();
}
}  // namespace UI

// ============================================================================
// Registration. ENDGAME B.3.9 UI-A: the slot doors, the ??0 and ??1 doors,
// read only by the retired minted table / RT_DELDTOR row / FACTUI2 row, are
// gone. The factory pair (B39_RULES D2) is the class's own.
// ============================================================================
int RtUiNumberEditElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?CreateNewObject@NumberEditElement@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::NumberEditElement::CreateNewObject);        ++n;
    RegisterRuntimeAddr(0x180046340ull, (void*)&UI::NumberEditElement::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@NumberEditElement@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::NumberEditElement::CreateInPlaceObject);    ++n;
    RegisterRuntimeAddr(0x1800463e0ull, (void*)&UI::NumberEditElement::CreateInPlaceObject);
    return n;
}

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::NumberEditElement's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@NumberEditElement@UI@@ (0x1801fa190): name "NumberEditElement", parent
// "EditElement" (retail ?_ParentTypeName@NumberEditElement@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::NumberEditElement, "EditElement", nullptr)
