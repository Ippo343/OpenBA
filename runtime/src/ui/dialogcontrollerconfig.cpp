// ============================================================================
// runtime/src/ui/dialogcontrollerconfig.cpp — VTABLE BATCH 9
//   UI::DialogControllerConfig, 3 of 8 vtable slots (0, 1, 2 -- the rest are
//   purecall traps or the shared External::Compile, neither this batch's job).
// ============================================================================
//
// ??_7DialogControllerConfig@UI@@6B@ (retail 0x180156650), 8 slots:
//   0  deleting dtor  0x18003b600  DialogControllerConfig.h (the class's own, B.3.9)
//   1  TypeName       0x18003b470  here
//   2  Type           0x18003b450  here
//   3  purecall trap  0x18007ec28  minted, not this batch's job
//   4  External::Compile 0x1800066e0  already ported
//   5,6,7 purecall traps 0x18007ec28  minted, not this batch's job
// plus ??1DialogControllerConfig@UI@@UEAA@XZ (0x18003b4f0) and the factory pair.
//
// ENDGAME B.3.9 block UI-D: the dtor door and the two slot doors, whose only
// reader was the minted `??_7DialogControllerConfig@UI@@6B@` (and its
// RT_DELDTOR row), are retired with it; the FACTUI1 row is replaced by the
// class's own factory pair below.
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
#include "../../types/ui/DialogControllerConfig.h"

namespace RtUI {

namespace {

const uint8_t* DialogControllerConfigTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@DialogControllerConfig@UI@@0VBaseTypeInfo@Object@@A");
}

}  // namespace

}  // namespace RtUI

// ============================================================================
// ENDGAME B.3.6 block B — UI::DialogControllerConfig's two own slots are
// members now (runtime/types/ui/DialogControllerConfig.h). Neither reads
// `this`: both read one exported BaseTypeInfo (+0x08 name, +0x00 type id).
// ============================================================================
namespace UI {

// ?TypeName@DialogControllerConfig@UI@@UEBAPEBDXZ   0x18003b470
const char* DialogControllerConfig::TypeName() const {
    const char* v = nullptr;
    __builtin_memcpy(&v, RtUI::DialogControllerConfigTypeInfo() + 8, sizeof v);
    return v;
}

// ?Type@DialogControllerConfig@UI@@UEBAIXZ          0x18003b450
uint32_t DialogControllerConfig::Type() const { return TypeId(); }
// ?TypeId@DialogControllerConfig@UI@@SAIXZ          0x18003b460 — the same dword.
uint32_t DialogControllerConfig::TypeId() {
    uint32_t v = 0;
    __builtin_memcpy(&v, RtUI::DialogControllerConfigTypeInfo(), sizeof v);
    return v;
}

// ?CreateNewObject@ / ?CreateInPlaceObject@DialogControllerConfig@UI@@
// 0x18003b480 / 0x18003b490: each is `XOR EAX,EAX ; RET` — the class is
// abstract, so its reflection record builds nothing (the FACTUI1 row this
// replaces returned null for the same reason).
Object::Base* DialogControllerConfig::CreateNewObject() { return nullptr; }
Object::Base* DialogControllerConfig::CreateInPlaceObject(void*) { return nullptr; }

// ??1DialogControllerConfig@UI@@UEAA@XZ 0x18003b4f0: restamp + tail-jump
// ~External, both the compiler's — the empty body is retail's.
DialogControllerConfig::~DialogControllerConfig() {}

}  // namespace UI

// ============================================================================
// Registration
// ============================================================================
int RtUiDialogControllerConfigRegister() {
    int n = 0;
    RegisterRuntimeExport("?CreateNewObject@DialogControllerConfig@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::DialogControllerConfig::CreateNewObject);      ++n;
    RegisterRuntimeAddr(0x18003b480ull, (void*)&UI::DialogControllerConfig::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@DialogControllerConfig@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::DialogControllerConfig::CreateInPlaceObject);  ++n;
    RegisterRuntimeAddr(0x18003b490ull, (void*)&UI::DialogControllerConfig::CreateInPlaceObject);
    return n;
}
