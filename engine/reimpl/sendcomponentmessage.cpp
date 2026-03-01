// engine/reimpl/sendcomponentmessage.cpp
// Reimplementation of SendComponentMessage action from Application-x64.exe.
//
// SendComponentMessage sends a message to a specific component type on the
// target entity. It looks up the component by type ID, then calls its
// message handler virtual method with the stored message parameters.
//
// Struct size: 0x40, vtable at 0x1401600c0
// Inherits: Object::Internal -> Action -> SendComponentMessage
//
// Vtable overrides vs Action base:
//   Slot 3 (Serialize):    0x1400cb000 (override) -- PASSTHROUGH
//   Slot 4 (AbstractImpl): 0x1400caef0 (override) -- PASSTHROUGH (component lookup + vcall)
//   Slot 9 (GetTarget):    0x1400cae80 (override) -- checks +0x20 and +0x28
//   Slots 5,6,7 (Execute, Enter, Exit): BASE (inherited)
//   Slot 8 (CheckEntity): BASE

#include "SendComponentMessage.h"
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

typedef void (*SendComponentMessage_Serialize_t)(void* self, void* stream);
typedef uint64_t (*SendComponentMessage_AbstractImpl_t)(void* self);

// ============================================================
// SendComponentMessage::Serialize -- passthrough wrapper
// Serializes status, target entity (+0x20), source entity (+0x28),
// component type (+0x30), message ID (+0x34), and params (+0x38, +0x3c)
// ============================================================

static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "SendComponentMessage::Serialize";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[SendComponentMessage::Serialize] self=%p stream=%p\n", self, stream);
        callCount++;
    }
#endif
    ((SendComponentMessage_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

// ============================================================
// SendComponentMessage::AbstractImpl -- passthrough wrapper
// Finds component by type ID on target entity, calls its message handler
// virtual method (vtable+0x78) with message params
// ============================================================

static uint64_t Reimpl_AbstractImpl(void* self) {
    g_lastHook = "SendComponentMessage::AbstractImpl";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        uint8_t* act = (uint8_t*)self;
        uint32_t msgType = *(uint32_t*)(act + 0x30);
        int32_t msgId = *(int32_t*)(act + 0x34);
        printf("[SendComponentMessage::AbstractImpl] self=%p msgType=%u msgId=%d\n",
               self, msgType, msgId);
        callCount++;
    }
#endif
    return ((SendComponentMessage_AbstractImpl_t)g_hookAbstractImpl.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallSendComponentMessageSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400cb000);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[SendComponentMessage] Serialize hooked at %p (prologue=20, passthrough)\n", (void*)target);
#endif
}

void RemoveSendComponentMessageSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallSendComponentMessageAbstractImpl() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400caef0);
    // Prologue: PUSH(2)+SUB(4)+MOV(5)+MOV(5) = 16 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 16);
#ifdef _DEBUG
    printf("[SendComponentMessage] AbstractImpl hooked at %p (prologue=16, passthrough)\n", (void*)target);
#endif
}

void RemoveSendComponentMessageAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}
