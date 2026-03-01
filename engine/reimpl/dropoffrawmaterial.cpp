// engine/reimpl/dropoffrawmaterial.cpp
// Reimplementation of DropOffRawMaterial action from Application-x64.exe.
//
// DropOffRawMaterial handles transferring raw materials from a citizen to a
// storage building. The AbstractImpl method is the most complex: it finds
// the target storage's StateMachineComponent, collects resource entries,
// and calls a helper to actually drop off each resource type.
//
// Struct size: 0x40, vtable at 0x14015fd80
// Inherits: Object::Internal -> Action -> DropOffRawMaterial
//
// Vtable overrides vs Action base:
//   Slot 3 (Serialize):    0x1400c7b10 (override) -- PASSTHROUGH
//   Slot 4 (AbstractImpl): 0x1400c7300 (override) -- PASSTHROUGH (complex resource transfer)
//   Slot 9 (GetTarget):    0x1400c7270 (override) -- checks +0x38 and +0x30->+0x10
//   Slots 5,6,7 (Execute, Enter, Exit): BASE (inherited)
//   Slot 8 (CheckEntity): 0x1400c72f0 -- tiny, not a separate function in Ghidra

#include "DropOffRawMaterial.h"
#include "hooks.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookSerialize;
static HookInfo g_hookAbstractImpl;

// ============================================================
// Original function typedefs
// ============================================================

typedef void (*DropOffRawMaterial_Serialize_t)(void* self, void* stream);
typedef uint64_t (*DropOffRawMaterial_AbstractImpl_t)(void* self, void* param2);

// ============================================================
// DropOffRawMaterial::Serialize -- passthrough wrapper
// Serializes status, target storage (+0x30), source entity (+0x38),
// resource type (+0x28), flag (+0x20), count (+0x24)
// ============================================================

static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "DropOffRawMaterial::Serialize";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[DropOffRawMaterial::Serialize] self=%p stream=%p\n", self, stream);
        callCount++;
    }
#endif
    ((DropOffRawMaterial_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

// ============================================================
// DropOffRawMaterial::AbstractImpl -- passthrough wrapper
// Complex resource transfer logic: collects resources from source entity,
// finds storage StateMachineComponent, transfers each resource type via helper
// ============================================================

static uint64_t Reimpl_AbstractImpl(void* self, void* param2) {
    g_lastHook = "DropOffRawMaterial::AbstractImpl";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        uint8_t* act = (uint8_t*)self;
        void* target = *(void**)(act + 0x38);
        printf("[DropOffRawMaterial::AbstractImpl] self=%p target=%p\n", self, target);
        callCount++;
    }
#endif
    return ((DropOffRawMaterial_AbstractImpl_t)g_hookAbstractImpl.trampoline)(self, param2);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallDropOffRawMaterialSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c7b10);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[DropOffRawMaterial] Serialize hooked at %p (prologue=20, passthrough)\n", (void*)target);
#endif
}

void RemoveDropOffRawMaterialSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallDropOffRawMaterialAbstractImpl() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c7300);
    // Prologue: MOV(5)+MOV(5)+PUSH(1)+PUSH(1)+PUSH(1) = 13 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 13);
#ifdef _DEBUG
    printf("[DropOffRawMaterial] AbstractImpl hooked at %p (prologue=13, passthrough)\n", (void*)target);
#endif
}

void RemoveDropOffRawMaterialAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}
