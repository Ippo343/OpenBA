// standalone/reg_orchard_growth.cpp
// OrchardComponent::GetGrowth (exe 0x140051640) -> code/Game/Component/orchardcomponent.cpp
//
// engine_funcs.cpp binds fn_OrchardGetGrowth via ResolveAddr(Exe, 0x140051640). With no
// reimplementation registered that resolved to 0, so the pointer was null. OrchardUI::Slot7
// calls it every frame the orchard panel is open — MEASURED as an EXEC-of-0 the moment the
// OrchardComponent type-id slot was filled (reg_orchard_typeid.cpp) and the finder finally
// returned a component:
//   Fault: EXEC of 0000000000000000, RCX=<OrchardComponent>
//   #1 OrchardUI_Slot7  #2 orchardui_sa::W_Slot7  #3-5 Runtime UI::UserInterface::Update
//     -> DialogInstance::ComputeLayout -> OnUpdateLayout (the per-frame controller walk)
// This is the documented "VCALL-of-0 may be a null fn ptr" shape: the crash dumper printed a
// vtable= guess, but RIP=0 with the component in RCX means a null static function pointer.
#include "resolve.h"
#include "Component/orchardcomponent_funcs.h"
#include <cstdint>

// Trampoline: the registry stores plain uintptr_t code addresses, and GetGrowth's
// signature (float return, one pointer arg) matches the exe function exactly.
void RegisterOrchardGrowth() {
    RegisterExeAddr(0x140051640, reinterpret_cast<uintptr_t>(
        static_cast<float (*)(void*)>(&OrchardComponent_GetGrowth)));
}
