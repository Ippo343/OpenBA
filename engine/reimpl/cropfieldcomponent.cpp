// engine/reimpl/cropfieldcomponent.cpp
// Hook wrappers for CropFieldComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/cropfieldcomponent.cpp.
//
// CropFieldComponent (0x68 bytes) extends ControllerComponent with crop flags
// and entity reference arrays for field tiles.
// Vtable at 0x140162ec0. One non-trivial hookable override:
//   Shutdown  (0x14011ae70) — complex entity ref serialization

#include "CropFieldComponent.h"
#include "Component/cropfieldcomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;

// ============================================================
// Reimplementation wrapper (delegates to clean code)
// ============================================================
static void Reimpl_Shutdown(CropFieldComponent* self, void* stream) {
    g_lastHook = "CropFieldComponent::Shutdown";
    CropFieldComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCropFieldComponentShutdown() {
    CropFieldComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14011ae70);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[CropFieldComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveCropFieldComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}
