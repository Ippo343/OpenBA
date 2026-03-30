// engine/reimpl/fruitcomponent.cpp
// Reimplementation of FruitComponent::Update from Application-x64.exe.
//
// Fruit ripeness simulation with temperature-dependent growth.
// Original: 0x140049de0 (105 instructions)

#include "FruitComponent.h"
#include "Component/fruitcomponent_funcs.h"
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
// Update (0x140049de0)
// ============================================================
static void Reimpl_Update(FruitComponent* self, int64_t* pDeltaTime) {
    g_lastHook = "FruitComponent::Update";
    FruitComponent_Update(self, pDeltaTime);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallFruitComponentUpdate() {
    FruitComponent_InitFuncs();

    if (!fn_IsDerivedFrom || !g_Timer_secondsPerTick) {
#ifdef _DEBUG
        printf("[FruitComponent] ERROR: missing critical deps, skipping hook\n");
#endif
        return;
    }

    // VTable hook: MOV RAX,RSP in prologue breaks with RSP compensation.
    // vtable at 0x140159450, Update = slot 9 (+0x48)
    InstallHookVTable(0x140159450, 9, (void*)Reimpl_Update, &g_hookUpdate);
    RegisterHookInfo("FruitComponent::Update", &g_hookUpdate);
#ifdef _DEBUG
    printf("[FruitComponent] Update vtable-hooked (original=%p)\n", g_hookUpdate.trampoline);
#endif
}

void RemoveFruitComponentUpdate() { RemoveHookVTable(&g_hookUpdate); }
