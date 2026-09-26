// ============================================================================
// runtime/src/ui/numbereditelement.cpp — UI::NumberEditElement
// ============================================================================
//
// `??_7NumberEditElement@UI@@6B@` (retail 0x180156f70). Only four of its slots
// are the class's own; slots 4..7 are `UI::EditElement`'s own bodies
// (0x1800d0c10 / 0x1800d0ab0 / 0x1800d0820 / 0x1800d0220), INHERITED in C++
// exactly as in the table:
//
//   0  vector deleting dtor  0x180046540  -> deleting_dtor.cpp row
//   1  TypeName              0x180046250  here
//   2  Type                  0x180046230  here
//   4..7                     EditElement's (editelement.cpp)
//   12 FinalizeText          0x1800d4a60  here  (overrides EditElement's
//                                               0x1800d0da0)
//
// plus ??0NumberEditElement (0x1800d4c70) and ??1NumberEditElement
// (0x1800d4c40), both DOOR bodies (no real ctor/dtor).
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

std::atomic<void*> g_typeInfoSlot{nullptr};   // RULE B6-5

const uint8_t* TypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfoSlot, "?_typeInfo@NumberEditElement@UI@@0VBaseTypeInfo@Object@@A"));
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7NumberEditElement@UI@@6B@"); }

using CtorFn    = void* (*)(void* self);
using DtorFn    = void  (*)(void* self);
using ToIntegerFn = int32_t (*)(const char* s);
using MemFreeFn = void (*)(void* p);
// `?Format@String@System@@SA?AV12@PEBDZZ` (0x1800c9730) returns a
// System::String BY VALUE, so the first argument is the HIDDEN RETURN SLOT
// (reference_hidden_retslot_wild_write.md).
using FormatIntFn = void* (*)(void* ret, const char* fmt, int64_t a);

void MemFree(void* p) { RtAddr<MemFreeFn>(0x1800c7ad0ull)(p); }

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

uint32_t NumberEditElement::Type() const { return TypeIdOf(TypeInfo()); }
const char* NumberEditElement::TypeName() const { return TypeNameOf(TypeInfo()); }

// Non-virtual, EMPTY: the dtor door keeps retail's body.
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
    RtAddr<FormatIntFn>(0x1800c9730ull)(&tmp, "%i", clamped);
    AssignString(_text, static_cast<char*>(tmp));
    ReleaseStringLocal(&tmp);
}

}  // namespace UI

// ??0NumberEditElement@UI@@QEAA@XZ   0x1800d4c70 — a DOOR BODY (no real ctor).
extern "C" void* RtUi_NumberEditElement_Ctor(void* selfv) {   // returns this in RAX
    RtAddr<CtorFn>(0x1800d0f10ull)(selfv);                     // ??0EditElement@UI@@QEAA@XZ
    UI::NumberEditElement* self = reinterpret_cast<UI::NumberEditElement*>(selfv);   // B.3.6 bridge
    *reinterpret_cast<void**>(selfv) = Vtable();
    self->_min = static_cast<int32_t>(0xffff8001u);            // -32767
    self->_max = 0x8000;                                       //  32768
    return selfv;
}

// ??1NumberEditElement@UI@@UEAA@XZ   0x1800d4c40 (tail-jumps ~EditElement) — a DOOR BODY.
extern "C" void RtUi_NumberEditElement_Dtor(void* self) {
    *reinterpret_cast<void**>(self) = Vtable();
    RtAddr<DtorFn>(0x1800d0e50ull)(self);                      // ??1EditElement@UI@@UEAA@XZ
}

// ============================================================================
// The registered doors — one QUALIFIED delegation each.
// ============================================================================
extern "C" uint32_t RtUi_NumberEditElement_Type(const void* self) {
    return reinterpret_cast<const UI::NumberEditElement*>(self)->UI::NumberEditElement::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_NumberEditElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::NumberEditElement*>(self)->UI::NumberEditElement::TypeName();   // B.3.6 bridge
}
extern "C" void RtUi_NumberEditElement_FinalizeText(void* self) {
    reinterpret_cast<UI::NumberEditElement*>(self)->UI::NumberEditElement::FinalizeText();   // B.3.6 bridge
}

// ============================================================================
int RtUiNumberEditElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@NumberEditElement@UI@@UEBAIXZ",
                          (void*)&RtUi_NumberEditElement_Type);        ++n;
    RegisterRuntimeAddr(0x180046230ull, (void*)&RtUi_NumberEditElement_Type);

    RegisterRuntimeExport("?TypeName@NumberEditElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_NumberEditElement_TypeName);    ++n;
    RegisterRuntimeAddr(0x180046250ull, (void*)&RtUi_NumberEditElement_TypeName);

    RegisterRuntimeExport("??0NumberEditElement@UI@@QEAA@XZ",
                          (void*)&RtUi_NumberEditElement_Ctor);        ++n;
    RegisterRuntimeAddr(0x1800d4c70ull, (void*)&RtUi_NumberEditElement_Ctor);

    RegisterRuntimeExport("??1NumberEditElement@UI@@UEAA@XZ",
                          (void*)&RtUi_NumberEditElement_Dtor);        ++n;
    RegisterRuntimeAddr(0x1800d4c40ull, (void*)&RtUi_NumberEditElement_Dtor);

    RegisterRuntimeExport("?FinalizeText@NumberEditElement@UI@@UEAAXXZ",
                          (void*)&RtUi_NumberEditElement_FinalizeText); ++n;
    RegisterRuntimeAddr(0x1800d4a60ull, (void*)&RtUi_NumberEditElement_FinalizeText);

    return n;
}
