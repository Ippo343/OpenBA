// code/Game/Action/sendcomponentmessage.cpp
// Clean C++ reimplementation of SendComponentMessage action methods.
// Source: Game/Action/SendComponentMessage.cpp (from profile build debug strings)
//
// SendComponentMessage — 0x40 bytes. Sends a message to a specific component
// type on the target entity.
// Inherits: Object::Internal -> Action (0x20) -> SendComponentMessage
// Ctor: 0x140023070 (factory), Vtable: 0x1401600c0
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — it is portable for standalone exe.

#include "SendComponentMessage.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h — SendComponentMessage-specific helpers)
// ============================================================

// Object::InternalBasePtr::Serialize(this, serializer) — via IAT at 0x140152aa0
static void (*s_fn_IBP_Serialize)(void* ibp, void* serializer) = nullptr;

// Object::Type::Serialize(this, serializer) — via dispatch slot at 0x1401516b0
static void (*s_fn_Type_Serialize)(void* typeField, void* serializer) = nullptr;

// IO::Serializer::Serialize(uint&, const char*) — via dispatch slot at 0x140152a50
static void (*s_fn_Serialize_UInt)(void* serializer, void* field, const char* name) = nullptr;

// TypeInfo for ComponentEntity — at 0x1401c5d70
// (Note: SendComponentMessage uses 0x1401c5d70, not 0x1401c4218)
static uint32_t* s_TypeId_Entity = nullptr;

static bool s_funcs_resolved = false;

// ============================================================
// Resolution — called once from Install
// ============================================================
bool SendComponentMessage_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    // IBP::Serialize via IAT
    uintptr_t iatIBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatIBPSer) s_fn_IBP_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iatIBPSer);

    // Object::Type::Serialize via dispatch slot
    uintptr_t slotTypeSer = ResolveAddr(Module::Exe, 0x1401516b0);
    if (slotTypeSer) s_fn_Type_Serialize = *reinterpret_cast<void(**)(void*, void*)>(slotTypeSer);

    // Serialize(uint&) via dispatch slot
    uintptr_t slotSerUInt = ResolveAddr(Module::Exe, 0x140152a50);
    if (slotSerUInt) s_fn_Serialize_UInt = *reinterpret_cast<void(**)(void*, void*, const char*)>(slotSerUInt);

    // TypeInfo
    s_TypeId_Entity = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5d70));

    bool ok = s_fn_IBP_Serialize && s_fn_Type_Serialize && s_fn_Serialize_UInt &&
              s_TypeId_Entity &&
              fn_InternalBasePtr_CheckPtr && fn_IsDerivedFrom &&
              fn_InternalBasePtr_DeletePtr && fn_InternalBasePtr_Ctor &&
              fn_ObjectInternal_Dtor && fn_MemFreeStatic &&
              g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
              g_fn_Serialize_Bool && g_fn_Serialize_Int;

#ifdef _DEBUG
    printf("[SendComponentMessage] ResolveFuncs: IBPSer=%p TypeSer=%p SerUInt=%p TypeId=%p\n",
           (void*)s_fn_IBP_Serialize, (void*)s_fn_Type_Serialize,
           (void*)s_fn_Serialize_UInt, (void*)s_TypeId_Entity);
    if (!ok) printf("[SendComponentMessage] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// ScalarDtor (vtable slot 0)
// Sets vtables, calls DeletePtr on entity ref, ~Internal, optionally free.
// ============================================================
void* SendComponentMessage_ScalarDtor_Impl(SendComponentMessage* self, uint32_t flags) {
    // Set SendComponentMessage vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, SendComponentMessageAddr::Vtable);
    // Set Action base vtable (destructor chain)
    uintptr_t actionVtable = ResolveAddr(Module::Exe, 0x14015faa0);
    self->vtable = (void*)actionVtable;

    // DeletePtr on entity IBP at +0x10 if non-null
    if (self->entity != nullptr) {
        fn_InternalBasePtr_DeletePtr(&self->entity);
    }

    fn_ObjectInternal_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// Serialize (0x1400cb000, vtable slot 3)
// Serializes status, two entity refs with type checks, component type,
// message ID, and two uint params.
// ============================================================
void SendComponentMessage_Serialize_Impl(SendComponentMessage* self, void* stream) {
    uint8_t ser1[24];
    uint8_t ser2[24];

    // Serialize status (version 0)
    g_fn_Serializer_Ctor(ser1, stream, 0);
    g_fn_Serialize_Bool(ser1, &self->status, nullptr);
    g_fn_Serializer_Dtor(ser1);

    // Serialize entity refs + message fields (version 0)
    g_fn_Serializer_Ctor(ser2, stream, 0);

    // Target entity ref (+0x20) with type validation
    s_fn_IBP_Serialize(&self->_field20, ser2);
    if (self->_field20 != nullptr) {
        if (!fn_IsDerivedFrom(self->_field20, *s_TypeId_Entity)) {
            self->_field20 = nullptr;
        }
    }

    // Source entity ref (+0x28) with type validation
    s_fn_IBP_Serialize(&self->_field28, ser2);
    if (self->_field28 != nullptr) {
        if (!fn_IsDerivedFrom(self->_field28, *s_TypeId_Entity)) {
            self->_field28 = nullptr;
        }
    }

    // Component type (+0x30) — Object::Type::Serialize
    s_fn_Type_Serialize(&self->_field30, ser2);

    // Message ID (+0x34) — int
    g_fn_Serialize_Int(ser2, &self->_field34, nullptr);

    // Message params (+0x38, +0x3c) — uint
    // Note: _field38 is void* in struct but actually two uint32s packed
    s_fn_Serialize_UInt(ser2, (void*)((uint8_t*)self + 0x38), nullptr);
    s_fn_Serialize_UInt(ser2, (void*)((uint8_t*)self + 0x3c), nullptr);

    g_fn_Serializer_Dtor(ser2);
}

// ============================================================
// AbstractImpl (0x1400caef0, vtable slot 4)
// Finds component by type ID on target entity, then calls its
// message handler virtual method (vtable+0x78).
// Returns 0 on success, 2 if component not found.
// ============================================================
int SendComponentMessage_AbstractImpl_Impl(SendComponentMessage* self, void* actionStack) {
    (void)actionStack;

    uint32_t componentTypeId = self->_field30;
    uint8_t* s = (uint8_t*)self;

    // CheckPtr on target entity
    fn_InternalBasePtr_CheckPtr(&self->_field20);
    uintptr_t entity = (uintptr_t)self->_field20;

    // Search entity's component array for matching type
    int32_t compCount = *(int32_t*)(entity + 0x38);
    uintptr_t compArray = *(uintptr_t*)(entity + 0x30);
    uintptr_t foundComp = 0;

    for (int i = 0; i < compCount; i++) {
        uintptr_t compIBP = compArray + (uintptr_t)i * 8;
        fn_InternalBasePtr_CheckPtr((void*)compIBP);
        uintptr_t comp = *(uintptr_t*)compIBP;

        // Get component type ID via vtable+0x10
        uintptr_t compVtable = *(uintptr_t*)comp;
        typedef int (*GetTypeId_t)(void*);
        GetTypeId_t getTypeId = *(GetTypeId_t*)(compVtable + 0x10);
        int typeId = getTypeId((void*)comp);

        if (typeId == (int)componentTypeId) {
            // Found matching component
            foundComp = *(uintptr_t*)(compArray + (uintptr_t)i * 8);
            break;
        }
    }

    // If not found, create null IBP
    if (foundComp == 0) {
        fn_InternalBasePtr_Ctor(&foundComp, nullptr);
    }

    if (foundComp == 0) {
        return 2;
    }

    // Call component's message handler (vtable+0x78)
    fn_InternalBasePtr_CheckPtr(&foundComp);
    uintptr_t compObj = foundComp;

    // Build call args: component, msgId, &sourceEntity, param1, param2
    uintptr_t sourceEntityRef = *(uintptr_t*)(s + 0x28);
    uintptr_t compVtable = *(uintptr_t*)compObj;
    typedef void (*MsgHandler_t)(void* comp, int32_t msgId, void* sourceEntityRef, uint32_t param1, uint32_t param2);
    MsgHandler_t handler = *(MsgHandler_t*)(compVtable + 0x78);
    handler((void*)compObj, *(int32_t*)(s + 0x34), &sourceEntityRef, *(uint32_t*)(s + 0x38), *(uint32_t*)(s + 0x3c));

    return 0;
}

// ============================================================
// GetTarget (0x1400cae80, vtable slot 9)
// Returns true if param matches entity at +0x20 or +0x28.
// ============================================================
bool SendComponentMessage_GetTarget_Impl(SendComponentMessage* self, void* param) {
    // CheckPtr on +0x20, then compare [param] directly (no CheckPtr on param)
    fn_InternalBasePtr_CheckPtr(&self->_field20);
    if (*(void**)param == self->_field20) {
        return true;
    }

    // CheckPtr on +0x28, then compare [param]
    fn_InternalBasePtr_CheckPtr(&self->_field28);
    return *(void**)param == self->_field28;
}

// ============================================================
// Standalone factory + vtable — Action family (notes/FACTORY_CASCADE_145_BRIEF.md).
// Vtable @ 0x1401600c0 (10 slots), size 0x40, parent "Action".
// Shared slot helpers live in action_base_standalone.{h,cpp}.
// ============================================================
#ifdef STANDALONE_EXE
#include "action_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t SendComponentMessage_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("SendComponentMessage");
    return c;
}

static void* g_SendComponentMessage_Vtable[10] = {
    (void*)&Action_ScalarDtor,        // +0x00 ScalarDtor
    (void*)&Action_Slot1,             // +0x08 slot1 (returns null)
    (void*)&SendComponentMessage_GetTypeId,           // +0x10 GetTypeId
    (void*)&SendComponentMessage_Serialize_Impl,      // +0x18 Serialize
    (void*)&SendComponentMessage_AbstractImpl_Impl,   // +0x20 AbstractImpl
    (void*)&Action_BaseExecute,   // +0x28 Execute
    (void*)&Action_Enter,  // +0x30 Enter
    (void*)&Action_Exit,   // +0x38 Exit
    (void*)&Action_BaseCheckEntity,  // +0x40 CheckEntity
    (void*)&SendComponentMessage_GetTarget_Impl, // +0x48 GetTarget
};

extern "C" void RegisterSendComponentMessageVtable() {
    RegisterExeAddr(0x1401600c0, (uintptr_t)g_SendComponentMessage_Vtable);
}
extern "C" void* SendComponentMessage_CreateInPlace(void* mem) {
    SendComponentMessage_ResolveFuncs();
    return Action_InitInPlace(mem, 0x40, g_SendComponentMessage_Vtable);
}
extern "C" void* SendComponentMessage_Factory(void) {
    SendComponentMessage_ResolveFuncs();
    return Action_Alloc(0x40, g_SendComponentMessage_Vtable);
}
REGISTER_TYPE(SendComponentMessage, "Action", &SendComponentMessage_Factory, &SendComponentMessage_CreateInPlace);
#endif // STANDALONE_EXE
