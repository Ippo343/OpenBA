// engine/reimpl/interactcomponent.cpp
// Reimplementation of InteractComponent lifecycle methods.
//
// InteractComponent (0x38 bytes) — direct Component subclass.
// Manages interactive object selection (clickable entities).
// Holds a dynamic array of entity references at +0x28 (ptr) / +0x30 (count).
//
// Vtable at 0x1401638b0:
//   [3] Shutdown = 0x140123340 (passthrough — serialization with entity array + IsDerivedFrom loop)
//   [6] Draw     = 0x140123110 (passthrough — 140-instruction rebuild of interact list from scene)
//   All other slots are base no-ops.

#include "InteractComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Shutdown (0x140123340) — passthrough
// 52-instruction function. Serializes Component base + entity array:
//   1. Reads count from +0x30, serializes it
//   2. On load: resizes array at +0x28 via FUN_1400032e0
//   3. Loops through array elements, calls InternalBasePtr::Serialize per element
//   4. Validates each with IsDerivedFrom(DAT_1401c7900), nulls invalid refs
// Heavy use of engine serialization infrastructure.
// ============================================================
typedef void (*InteractShutdown_t)(void* self, void* stream);
static HookInfo g_hookShutdown;

static void Reimpl_Shutdown(void* self, void* stream) {
    ((InteractShutdown_t)g_hookShutdown.trampoline)(self, stream);
}

void InstallInteractComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140123340);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[InteractComponent] Shutdown hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveInteractComponentShutdown() { RemoveHook(&g_hookShutdown); }

// ============================================================
// Draw (0x140123110) — passthrough
// 140-instruction function. Rebuilds the interact entity list from the scene:
//   1. Gets component list from owner (+0x10)
//   2. Frees old array at +0x28, resets count at +0x30
//   3. Iterates through owner's sub-objects, for each:
//      - Iterates sub-object's component list
//      - Finds components matching type DAT_1401c7900 via vtable call
//      - Validates with IsDerivedFrom
//      - Adds to dynamic array with MemAlloc/MemFree for growth
// Very complex scene traversal — must use passthrough.
// ============================================================
typedef void (*InteractDraw_t)(void* self);
static HookInfo g_hookDraw;

static void Reimpl_Draw(void* self) {
    ((InteractDraw_t)g_hookDraw.trampoline)(self);
}

void InstallInteractComponentDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140123110);
    // Prologue: PUSH RBX(1) + PUSH RBP(1) + SUB RSP,0x48(4) + MOV RBP,RCX(3) + ADD RCX,0x10(4) = 13
    // 0x14012311a - 0x140123110 = 0xA = 10. Need more: next is ADD(4)→0x11e = 14
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 14);
#ifdef _DEBUG
    printf("[InteractComponent] Draw hooked at %p (passthrough, prologue=14)\n", (void*)target);
#endif
}
void RemoveInteractComponentDraw() { RemoveHook(&g_hookDraw); }
