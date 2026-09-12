// ============================================================================
// runtime/src/ui/dialogcontrollerconfig.cpp — VTABLE BATCH 9
//   UI::DialogControllerConfig, 3 of 8 vtable slots (0, 1, 2 -- the rest are
//   purecall traps or the shared External::Compile, neither this batch's job).
// ============================================================================
//
// ??_7DialogControllerConfig@UI@@6B@ (retail 0x180156650), 8 slots:
//   0  deleting dtor  0x18003b600  deleting_dtor.cpp row (sizeof 0x20)
//   1  TypeName       0x18003b470  here
//   2  Type           0x18003b450  here
//   3  purecall trap  0x18007ec28  minted, not this batch's job
//   4  External::Compile 0x1800066e0  already ported
//   5,6,7 purecall traps 0x18007ec28  minted, not this batch's job
// plus ??1DialogControllerConfig@UI@@UEAA@XZ (0x18003b4f0), the row's dtorVa.
//
// LAYOUT: this class adds NOTHING over `Object::External` -- sizeof 0x20 is
// exactly `External`'s own size (runtime/types/object/External.h), which is
// also what the deleting destructor's `MOV EDX,0x20` at 0x18003b632 says.
// `DialogController::_config` (runtime/types/ui/DialogInstance.h) holds an
// `ExternalPtr<DialogControllerConfig>`, so this class exists purely to be
// pointed at -- it carries no fields of its own for AddController to fill.
//
// ??1DialogControllerConfig@UI@@UEAA@XZ   0x18003b4f0, two instructions:
//     *(void**)this = &??_7DialogControllerConfig@UI@@6B@;   (dead store,
//         transcribed for the same reason ext_compilers.cpp gives)
//     Object::External::~External(this);   -- already ported, ordinary call
//         here where retail tail-jumps.
// ============================================================================

#include <cstdint>

#include "../../funcs/rt_port.h"

namespace RtUI {

namespace {

void* VtDialogControllerConfig() {
    return RT_DATA_SLOT(void*, "??_7DialogControllerConfig@UI@@6B@");
}
const uint8_t* DialogControllerConfigTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@DialogControllerConfig@UI@@0VBaseTypeInfo@Object@@A");
}

}  // namespace

// ---- ?TypeName@DialogControllerConfig@UI@@UEBAPEBDXZ   0x18003b470 -------
extern "C" const char* RtUi_DialogControllerConfig_TypeName(const void*) {
    const char* v = nullptr;
    __builtin_memcpy(&v, DialogControllerConfigTypeInfo() + 8, sizeof v);
    return v;
}
// ---- ?Type@DialogControllerConfig@UI@@UEBAIXZ          0x18003b450 -------
extern "C" uint32_t RtUi_DialogControllerConfig_Type(const void*) {
    uint32_t v = 0;
    __builtin_memcpy(&v, DialogControllerConfigTypeInfo(), sizeof v);
    return v;
}

// ---- ??1DialogControllerConfig@UI@@UEAA@XZ   0x18003b4f0 ------------------
extern "C" void RtUi_DialogControllerConfig_Dtor(void* selfv) {
    void** v = reinterpret_cast<void**>(selfv);
    *v = VtDialogControllerConfig();                               // 18003b4f7 (dead store)
    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(selfv);  // 0x1800bc950
}

}  // namespace RtUI

// ============================================================================
// Registration
// ============================================================================
int RtUiDialogControllerConfigRegister() {
    using namespace RtUI;
    int n = 0;

    RegisterRuntimeExport("?TypeName@DialogControllerConfig@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_DialogControllerConfig_TypeName); ++n;
    RegisterRuntimeAddr(0x18003b470ull, (void*)&RtUi_DialogControllerConfig_TypeName);

    RegisterRuntimeExport("?Type@DialogControllerConfig@UI@@UEBAIXZ",
                          (void*)&RtUi_DialogControllerConfig_Type); ++n;
    RegisterRuntimeAddr(0x18003b450ull, (void*)&RtUi_DialogControllerConfig_Type);

    RegisterRuntimeExport("??1DialogControllerConfig@UI@@UEAA@XZ",
                          (void*)&RtUi_DialogControllerConfig_Dtor); ++n;
    RegisterRuntimeAddr(0x18003b4f0ull, (void*)&RtUi_DialogControllerConfig_Dtor);

    return n;
}
