// code/Game/Component/charactercomponent.cpp
// Clean reimplementation of CharacterComponent::Update from Application-x64.exe.
//
// CharacterComponent (0x80 bytes) manages character animation state.
// Update checks if a character index (data60) is valid (>= 0),
// looks up a character object from an array at data48, and calls
// Character_UpdateAnimation to process the character's animation state.
//
// Vtable at 0x140162370. One non-trivial override:
//   Update (0x140109480) — conditional animation delegate
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/charactercomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Character_UpdateAnimation(arrayBase*, InternalBasePtr*, Time)
// Located at 0x1400c4f80 in Application-x64.exe.
// Iterates a linked list of character objects from the InternalBasePtr,
// calling each object's vtable[6] (Draw/UpdateAnimation) method.
// RCX = arrayBase (this+0x48), RDX = &localPtr (InternalBasePtr*), R8 = Time (int64)
typedef void (*fn_Character_UpdateAnimation_t)(void* arrayBase, void* ptrRef, int64_t deltaTime);
static fn_Character_UpdateAnimation_t s_fn_Character_UpdateAnimation = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    s_fn_Character_UpdateAnimation = reinterpret_cast<fn_Character_UpdateAnimation_t>(
        ResolveAddr(Module::Exe, 0x1400c4f80));

    s_helpersResolved = true;
}

// ============================================================
// CharacterComponent::Update (vtable slot [9], 0x140109480)
// ============================================================
// If data60 >= 0, look up object from *(data48)[data60] and call
// Character_UpdateAnimation with (&this->data48, &obj, deltaTime).
//
// Original disassembly:
//   SUB RSP,0x28
//   MOVSXD RAX,[RCX+0x60]    ; sign-extend data60
//   ADD RCX,0x48              ; arrayBase = this+0x48
//   TEST EAX,EAX
//   JS skip                   ; skip if index < 0
//   MOV RDX,[RCX]             ; objArray = *(this+0x48)
//   LEA RDX,[RDX+RAX*8]      ; &objArray[index]
//   MOV R8,R9                 ; deltaTime (forwarded from caller)
//   CALL Character_UpdateAnimation
// skip:
//   ADD RSP,0x28
//   RET

void CharacterComponent_Update(CharacterComponent* self, int64_t deltaTime) {
    EnsureHelpers();

    if (self->data60 >= 0) {
        if (!s_fn_Character_UpdateAnimation) return;

        // data48 is a pointer to an array of object pointers
        // data60 is the index into that array
        void** objArray = *reinterpret_cast<void***>(&self->data48);
        void* obj = objArray[self->data60];

        // Character_UpdateAnimation expects a pointer to the object pointer
        // (InternalBasePtr is just a pointer-sized wrapper)
        void* localPtr = obj;
        s_fn_Character_UpdateAnimation(
            reinterpret_cast<void*>(&self->data48),
            &localPtr,
            deltaTime
        );
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void CharacterComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[CharacterComponent] Funcs resolved: UpdateAnimation=%p\n",
           (void*)s_fn_Character_UpdateAnimation);
#endif
}
