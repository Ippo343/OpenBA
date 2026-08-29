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

#include "SendComponentMessage.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Game/force_work.h"   // ForceWork::Readable — stale action-target guard
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (diagnostic filter)
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
    // Reached through the registered vtable, which the factory path never
    // has to touch — an exe-ctor-built action arrives here with the file
    // statics still null. Lazy-resolve like the other entry points do.
    SendComponentMessage_ResolveFuncs();
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
    SendComponentMessage_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;

    uint32_t componentTypeId = self->_field30;
    uint8_t* s = (uint8_t*)self;

    // CheckPtr on target entity
    fn_InternalBasePtr_CheckPtr(&self->_field20);
    uintptr_t entity = (uintptr_t)self->_field20;

    // SA-guard: the target at +0x20 is a WEAK ref, and nothing invalidates it when the
    // entity is freed (CheckPtr is an empty function in the shipping Runtime). A citizen
    // whose in-flight labor message targets a building that got demolished/upgraded
    // mid-action therefore reads freed memory here. Observed live on the first upgrade
    // that spawned a replacement: the old building was freed the same frame, and the
    // next builder message read compArray out of the dead entity, giving compIBP =
    // 0x346e6f (ASCII from a reused block) -> ACCESS_VIOLATION at the loop below.
    //
    // Retail reaches the same "nothing to deliver the message to" outcome through its
    // own paths (the serialize side nulls +0x20 when it is not an Entity, and the
    // no-component branch below returns 2), so failing the action is faithful in effect.
    // Same guard shape as PlayAnimation's PlayAnim_TargetLooksLive.
    // A non-canonical pointer is the torn-down sentinel, not an address (edittool.cpp:328
    // and tool_base_render.cpp use the same range test).
    const bool canonical = entity >= 0x10000 && entity <= 0x00007FFFFFFFFFFFULL;
    // MARKED-FOR-REMOVAL (entity+0x40 bit 0x400) is the missing test. The readability
    // probes below only prove the memory is mapped — a dying entity is still perfectly
    // readable in the window between MarkForRemoval (which broadcasts +0x38 and lets each
    // component release its world registrations) and the EntityList drain that frees it.
    // Deliver a labor message in that window and the component scan below walks slots that
    // teardown has already reset to the -1 sentinel, and `*(uintptr_t*)comp` faults on
    // address 0xffffffffffffffff. That is the crash after 3-4 burn/rebuild cycles
    // (2026-08-23): repeated demolitions keep citizens holding in-flight messages against
    // buildings that are mid-teardown. Same aliveness test the dialog controllers already
    // use (SA_DialogEntityAlive / destroyui.cpp DataSourceAlive).
    const bool dying = canonical &&
        ForceWork::Readable(reinterpret_cast<void*>(entity), 0x44) &&
        ((*reinterpret_cast<uint32_t*>(entity + 0x40) >> 10) & 1) != 0;
    if (!entity || !canonical || dying ||
        !ForceWork::Readable(reinterpret_cast<void*>(entity), 0x40) ||
        !ForceWork::Readable(*reinterpret_cast<void**>(entity + 0x30), 8)) {
        static int s_reported = 0;
        if (s_reported++ < 20) {
            std::fprintf(stderr, "[SendCompMsg] ABORT: target entity=%p is not live "
                         "(canonical=%d dying=%d msgId=%d compType=%u) — action dropped "
                         "instead of reading freed memory\n",
                         reinterpret_cast<void*>(entity), (int)canonical, (int)dying,
                         *(int32_t*)(s + 0x34), componentTypeId);
            std::fflush(stderr);
        }
        return 2;
    }

    // Search entity's component array for matching type
    int32_t compCount = *(int32_t*)(entity + 0x38);
    uintptr_t compArray = *(uintptr_t*)(entity + 0x30);
    uintptr_t foundComp = 0;

    for (int i = 0; i < compCount; i++) {
        uintptr_t compIBP = compArray + (uintptr_t)i * 8;
        fn_InternalBasePtr_CheckPtr((void*)compIBP);
        uintptr_t comp = *(uintptr_t*)compIBP;

        // Backstop for the same sentinel one level down: an entity that is NOT marked for
        // removal can still carry an individually released slot. Retail derefs this
        // unguarded and is right to — it never puts a sentinel in a live entity's array —
        // so skip the slot rather than clamping it, and keep the scan going.
        if (comp < 0x10000 || comp > 0x00007FFFFFFFFFFFULL) continue;

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

    {   // First few sends only. When a message never reaches its handler there are three
        // candidate stages — the action never ran, it ran against the wrong entity, or the
        // type-id scan missed — and only a report at the scan can tell them apart.
        // ⚠ The budget is PER MESSAGE ID. It used to be one shared counter, which the 12
        // planting sends (msg 0) exhausted before the first harvest send (msg 1) ever
        // happened — so "zero msg=1 sends" read as a missing message when the message was
        // simply unloggable. A saturating diagnostic reports its own budget, not the
        // system. See reference_diagnostics_must_report_outcomes.
        static uint32_t s_tidOrchard = ::Object::GetTypeIdByName("OrchardComponent");
        static int s_seen[8] = {0};
        const int msgId = *(int32_t*)(s + 0x34);
        const int bucket = (msgId >= 0 && msgId < 8) ? msgId : 7;
        if (componentTypeId == s_tidOrchard && s_seen[bucket]++ < 8) {
            std::fprintf(stderr, "[SendCompMsg] entity=%p compType=%u msg=%d p1=%u -> "
                         "scanned %d components, found=%p\n",
                         reinterpret_cast<void*>(entity), componentTypeId,
                         *(int32_t*)(s + 0x34), *(uint32_t*)(s + 0x38), compCount,
                         reinterpret_cast<void*>(foundComp));
            std::fflush(stderr);
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
    SendComponentMessage_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
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
