// engine/reimpl/charactercomponent.cpp
// Reimplementation of CharacterComponent::Update from Application-x64.exe.
//
// The Update method checks if a character index (data60) is valid (>= 0),
// looks up a character object from an array at data48, and calls
// Character_UpdateAnimation to process the character's animation state.
//
// Original: 0x140109480 (58 bytes, simple conditional + delegate)

#include "CharacterComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration from code/Game/Component/charactercomponent.cpp
extern void CharacterComponent_Update(CharacterComponent* self, int64_t deltaTime);
extern void CharacterComponent_InitFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;

// ============================================================
// Update (0x140109480)
// Delegates to clean reimplementation in code/Game/Component/charactercomponent.cpp.
//
// Parameters: RCX = this, RDX = Time (int64)
// ============================================================
static void Reimpl_Update(CharacterComponent* self, int64_t deltaTime) {
    g_lastHook = "CharacterComponent::Update";
    CharacterComponent_Update(self, deltaTime);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCharacterComponentUpdate() {
    // Initialize clean code function pointers
    CharacterComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, CharacterComponentAddr::Update);
    // Prologue: SUB RSP,0x28 (4) + MOVSXD RAX,[RCX+0x60] (4) + ADD RCX,0x48 (4) = 12 bytes.
    // 12 bytes lands exactly on instruction boundary (TEST EAX,EAX at offset 12).
    // Trampoline has modified RCX (+0x48), not usable, but we don't call it.
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 12);
#ifdef _DEBUG
    printf("[CharacterComponent] Update hooked at %p (prologue=12)\n", (void*)target);
#endif
}

void RemoveCharacterComponentUpdate() { RemoveHook(&g_hookUpdate); }
