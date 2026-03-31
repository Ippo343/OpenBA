// engine/reimpl/animal.cpp
// Hook wrapper for AnimalComponent::Update.
// Delegates to clean reimplementation in code/Game/Component/animalcomponent.cpp.
//
// AnimalComponent (0x58 bytes) manages individual wild/domesticated animals.
// Vtable at 0x140162110. Update = slot 9 (+0x48).

#include "AnimalComponent.h"
#include "Component/animalcomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookUpdate;

// ============================================================
// Reimplementation wrapper (delegates to clean code)
// ============================================================

static void Reimpl_Update(AnimalComponent* self, int64_t* deltaTime) {
    g_lastHook = "AnimalComponent::Update";
    AnimalComponent_Update(self, deltaTime);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallAnimalComponentUpdate() {
    AnimalComponent_InitFuncs();
    // VTable hook: MOV RAX,RSP in prologue breaks with RSP compensation.
    // vtable at 0x140162110, Update = slot 9 (+0x48)
    InstallHookVTable(0x140162110, 9, (void*)Reimpl_Update, &g_hookUpdate);
    RegisterHookInfo("AnimalComponent::Update", &g_hookUpdate);
#ifdef _DEBUG
    printf("[AnimalComponent] Update vtable-hooked (original=%p)\n", g_hookUpdate.trampoline);
#endif
}

void RemoveAnimalComponentUpdate() {
    RemoveHookVTable(&g_hookUpdate);
}
