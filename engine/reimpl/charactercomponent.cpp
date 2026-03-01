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

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;

// ============================================================
// Helper function pointers (resolved at install time)
// ============================================================

// Character_UpdateAnimation(arrayBase*, InternalBasePtr*, Time)
// Located at 0x1400c4f80 in Application-x64.exe.
// Iterates a linked list of character objects from the InternalBasePtr,
// calling each object's vtable[6] (Draw/UpdateAnimation) method.
// RCX = arrayBase (this+0x48), RDX = &localPtr (InternalBasePtr*), R8 = Time (int64)
typedef void (*fn_Character_UpdateAnimation_t)(void* arrayBase, void* ptrRef, int64_t deltaTime);
static fn_Character_UpdateAnimation_t fn_Character_UpdateAnimation = nullptr;

// ============================================================
// Update (0x140109480)
// If data60 >= 0, look up object from *(data48)[data60] and call
// Character_UpdateAnimation with (this+0x48, &obj, deltaTime).
//
// Parameters: RCX = this, RDX = Time (int64)
// ============================================================
static void Reimpl_Update(CharacterComponent* self, int64_t deltaTime) {
    g_lastHook = "CharacterComponent::Update";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[CharacterComponent::Update] self=%p dt=%lld idx=%d\n",
               (void*)self, deltaTime, self->data60);
        callCount++;
    }
#endif

    if (self->data60 >= 0) {
        // data48 is a pointer to an array of object pointers
        // data60 is the index into that array
        void** objArray = *reinterpret_cast<void***>(&self->data48);
        void* obj = objArray[self->data60];

        // Character_UpdateAnimation expects a pointer to the object pointer
        // (InternalBasePtr is just a pointer-sized wrapper)
        void* localPtr = obj;
        fn_Character_UpdateAnimation(
            reinterpret_cast<void*>(&self->data48),
            &localPtr,
            deltaTime
        );
    }
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCharacterComponentUpdate() {
    // Resolve Character_UpdateAnimation in the exe
    fn_Character_UpdateAnimation = reinterpret_cast<fn_Character_UpdateAnimation_t>(
        ResolveAddr(Module::Exe, 0x1400c4f80));

#ifdef _DEBUG
    printf("[CharacterComponent] Character_UpdateAnimation = %p\n",
           (void*)fn_Character_UpdateAnimation);
#endif

    if (!fn_Character_UpdateAnimation) {
#ifdef _DEBUG
        printf("[CharacterComponent] ERROR: failed to resolve Character_UpdateAnimation, skipping hook\n");
#endif
        return;
    }

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
