// ============================================================================
// runtime/src/ui/numbereditelement.cpp — UI::NumberEditElement
// ============================================================================
//
// `??_7NumberEditElement@UI@@6B@` (retail 0x180156f70). Only four of its slots
// are the class's own; slots 4..7 are `UI::EditElement`'s own bodies
// (0x1800d0c10 / 0x1800d0ab0 / 0x1800d0820 / 0x1800d0220), which belong to the
// elem_A batch and are NOT duplicated here:
//
//   0  vector deleting dtor  0x180046540  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x180046250  here
//   2  Type                  0x180046230  here
//   4..7                     EditElement's, elem_A
//   12 FinalizeText          0x1800d4a60  here  (overrides EditElement's
//                                               0x1800d0da0)
//
// plus ??0NumberEditElement (0x1800d4c70) and ??1NumberEditElement
// (0x1800d4c40).
//
// LAYOUT: sizeof == 0x100 (the deleting destructor's `MOV EDX,0x100`), over
// EditElement's 0xE8 (runtime/types/ui/NumberEditElement.h, which already
// pins _min @ +0xF8 and _max @ +0xFC from ?SetMinMax@). The text field this
// class reads is EditElement's own System::String at +0xC8 — the same field
// ?SetValue@NumberEditElement@ (0x1800d4bb0) writes.
// ============================================================================

#include "widget_common.h"
#include "../../types/ui/NumberEditElement.h"

using namespace rtui;

namespace {

constexpr int kText = 0xC8;   // EditElement's System::String body pointer
constexpr int kMin  = 0xF8;
constexpr int kMax  = 0xFC;

static_assert(kMin == offsetof(UI::NumberEditElement, _min), "");
static_assert(kMax == offsetof(UI::NumberEditElement, _max), "");

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@NumberEditElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7NumberEditElement@UI@@6B@"); }

// `?Format@String@System@@SA?AV12@PEBDZZ` (0x1800c9730) returns a
// System::String BY VALUE, so the first argument is the HIDDEN RETURN SLOT
// (reference_hidden_retslot_wild_write.md: getting this wrong is a wild write,
// not a wrong value).
using FormatIntFn = void* (*)(void* ret, const char* fmt, int64_t a);

}  // namespace

extern "C" uint32_t RtUi_NumberEditElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_NumberEditElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ??0NumberEditElement@UI@@QEAA@XZ   0x1800d4c70
extern "C" void RtUi_NumberEditElement_Ctor(void* self) {
    RtAddr<ElementCtorFn>(0x1800d0f10ull)(self);    // ??0EditElement@UI@@QEAA@XZ
    PTR(self, 0x00) = Vtable();
    I32(self, kMin) = static_cast<int32_t>(0xffff8001u);   // -32767
    I32(self, kMax) = 0x8000;                              //  32768
}

// ??1NumberEditElement@UI@@UEAA@XZ   0x1800d4c40 (tail-jumps ~EditElement)
extern "C" void RtUi_NumberEditElement_Dtor(void* self) {
    PTR(self, 0x00) = Vtable();
    RtAddr<ElementDtorFn>(0x1800d0e50ull)(self);    // ??1EditElement@UI@@UEAA@XZ
}

// ---------------------------------------------------------------------------
// slot 12 — ?FinalizeText@NumberEditElement@UI@@UEAAXXZ   0x1800d4a60
//
// Clamp the edited text to [_min, _max] and write it back formatted. The
// clamp is retail's, and it is NOT symmetric: the value is taken only when it
// is >= _min, and only then compared against _max, so a value below _min
// leaves `_min` itself as the result.
// ---------------------------------------------------------------------------
extern "C" void RtUi_NumberEditElement_FinalizeText(void* self) {
    const int32_t v = RtAddr<ToIntegerFn>(0x18003f620ull)(
        static_cast<const char*>(PTR(self, kText)));

    int32_t clamped = I32(self, kMin);
    if (I32(self, kMin) <= v) {
        clamped = v;
        if (I32(self, kMax) < v) clamped = I32(self, kMax);
    }

    void* tmp = nullptr;
    RtAddr<FormatIntFn>(0x1800c9730ull)(&tmp, "%i", clamped);
    AssignStringField(self, kText, tmp);
    ReleaseStringLocal(&tmp);
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
