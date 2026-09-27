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
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Game/force_work.h"   // ForceWork::Readable — stale action-target guard
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (diagnostic filter)
#include <cstdio>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/Component.h"          // ENDGAME B.3.8 CB3: slot 15 OnMessage
#include "Component/OrchardComponent.h"   // ENDGAME B.3.8 CB3: OrchardComponent::TypeId()

// ============================================================
// Local function pointers resolved at init time
// (NOT in engine_funcs.h — SendComponentMessage-specific helpers)
// ============================================================

// Object::Type::Serialize(this, serializer) — via dispatch slot at 0x1401516b0
static void (*s_fn_Type_Serialize)(void* typeField, void* serializer) = nullptr;

// TypeInfo for ComponentEntity — at 0x1401c5d70
// (Note: SendComponentMessage uses 0x1401c5d70, not 0x1401c4218)
static uint32_t* s_TypeId_Entity = nullptr;

static bool s_funcs_resolved = false;

// ============================================================
// Resolution — called once from Install
// ============================================================
bool SendComponentMessage_ResolveFuncs() {
    if (s_funcs_resolved) return true;

    // Object::Type::Serialize via dispatch slot
    uintptr_t slotTypeSer = ResolveAddr(Module::Exe, 0x1401516b0);
    if (slotTypeSer) s_fn_Type_Serialize = *reinterpret_cast<void(**)(void*, void*)>(slotTypeSer);

    // TypeInfo
    s_TypeId_Entity = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5d70));

    bool ok = s_fn_Type_Serialize &&
              s_TypeId_Entity &&
              fn_IsDerivedFrom &&
              fn_InternalBasePtr_DeletePtr && fn_InternalBasePtr_Ctor &&
              fn_ObjectInternal_Dtor && fn_MemFreeStatic;

#ifdef _DEBUG
    printf("[SendComponentMessage] ResolveFuncs: TypeSer=%p TypeId=%p\n",
           (void*)s_fn_Type_Serialize,
           (void*)s_TypeId_Entity);
    if (!ok) printf("[SendComponentMessage] ERROR: some function pointers not resolved!\n");
#endif

    s_funcs_resolved = ok;
    return ok;
}

// ============================================================
// Serialize (0x1400cb000, vtable slot 3)
// Serializes status, two entity refs with type checks, component type,
// message ID, and two uint params.
// ============================================================
void SendComponentMessage::Persist(void* stream) {
    // Reached through the registered vtable, which the factory path never
    // has to touch — an exe-ctor-built action arrives here with the file
    // statics still null. Lazy-resolve like the other entry points do.
    SendComponentMessage_ResolveFuncs();
    alignas(8) uint8_t ser1[24];
    alignas(8) uint8_t ser2[24];

    // Serialize status (version 0)
    ::new (static_cast<void*>(ser1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->Serialize(*reinterpret_cast<bool*>(&this->_status), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();   // B.3.4 bridge

    // Serialize entity refs + message fields (version 0)
    ::new (static_cast<void*>(ser2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Target entity ref (+0x20) with type validation
    reinterpret_cast<Object::InternalBasePtr*>(&this->_field20)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (this->_field20 != nullptr) {
        if (!fn_IsDerivedFrom(this->_field20, *s_TypeId_Entity)) {
            this->_field20 = nullptr;
        }
    }

    // Source entity ref (+0x28) with type validation
    reinterpret_cast<Object::InternalBasePtr*>(&this->_field28)->Serialize(*reinterpret_cast<IO::Serializer*>(ser2));   // B.3.4 bridge
    if (this->_field28 != nullptr) {
        if (!fn_IsDerivedFrom(this->_field28, *s_TypeId_Entity)) {
            this->_field28 = nullptr;
        }
    }

    // Component type (+0x30) — Object::Type::Serialize
    s_fn_Type_Serialize(&this->_field30, ser2);

    // Message ID (+0x34) — int
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<int*>(&this->_field34), nullptr);   // B.3.4 bridge

    // Message params (+0x38, +0x3c) — uint
    // Note: _field38 is void* in struct but actually two uint32s packed
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<unsigned int*>((reinterpret_cast<uint8_t*>(this) + 0x38)), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser2)->Serialize(*reinterpret_cast<unsigned int*>((reinterpret_cast<uint8_t*>(this) + 0x3c)), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser2)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// AbstractImpl (0x1400caef0, vtable slot 4)
// Finds component by type ID on target entity, then calls its
// message handler virtual method (vtable+0x78).
// Returns 0 on success, 2 if component not found.
// ============================================================
int  SendComponentMessage::AbstractImpl(void* actionStack) {
    SendComponentMessage_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    (void)actionStack;

    uint32_t componentTypeId = this->_field30;
    uint8_t* s = reinterpret_cast<uint8_t*>(this);

    // CheckPtr on target entity
    uintptr_t entity = reinterpret_cast<uintptr_t>(_field20);

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
                         _field34, componentTypeId);
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
        uintptr_t comp = *(uintptr_t*)compIBP;

        // Backstop for the same sentinel one level down: an entity that is NOT marked for
        // removal can still carry an individually released slot. Retail derefs this
        // unguarded and is right to — it never puts a sentinel in a live entity's array —
        // so skip the slot rather than clamping it, and keep the scan going.
        if (comp < 0x10000 || comp > 0x00007FFFFFFFFFFFULL) continue;

        // Get component type ID via vtable+0x10
        int typeId = static_cast<int>(reinterpret_cast<Object::Base*>(comp)->Type());   // B.3.8 bridge

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
        static int s_seen[8] = {0};
        const int msgId = _field34;
        const int bucket = (msgId >= 0 && msgId < 8) ? msgId : 7;
        if (componentTypeId == OrchardComponent::TypeId() && s_seen[bucket]++ < 8) {
            std::fprintf(stderr, "[SendCompMsg] entity=%p compType=%u msg=%d p1=%u -> "
                         "scanned %d components, found=%p\n",
                         reinterpret_cast<void*>(entity), componentTypeId,
                         _field34, *(uint32_t*)(s + 0x38), compCount,
                         reinterpret_cast<void*>(foundComp));
            std::fflush(stderr);
        }
    }

    // If not found, create null IBP
    if (foundComp == 0) {
        new (&foundComp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    }

    if (foundComp == 0) {
        return 2;
    }

    // Call component's message handler (vtable+0x78)
    uintptr_t compObj = foundComp;

    // Build call args: component, msgId, &sourceEntity, param1, param2
    uintptr_t sourceEntityRef = *(uintptr_t*)(s + 0x28);
    // slot 15 (vtable +0x78) Component::OnMessage on the component the type scan found
    reinterpret_cast<Component*>(compObj)->OnMessage(_field34, &sourceEntityRef,   // B.3.8 bridge
                                                     *(uint32_t*)(s + 0x38), *(uint32_t*)(s + 0x3c));

    return 0;
}

// ============================================================
// GetTarget (0x1400cae80, vtable slot 9)
// Returns true if param matches entity at +0x20 or +0x28.
// ============================================================
bool SendComponentMessage::GetTarget(void* param) {
    SendComponentMessage_ResolveFuncs();   // lazy-resolve (exe-ctor-built action)
    // CheckPtr on +0x20, then compare [param] directly (no CheckPtr on param)
    if (*(void**)param == this->_field20) {
        return true;
    }

    // CheckPtr on +0x28, then compare [param]
    return *(void**)param == this->_field28;
}

// ============================================================
// class SendComponentMessage (SendComponentMessage.h) — ENDGAME B.3.8 ACT.
// The hand-built g_SendComponentMessage_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// create 0x140023070. Retail's default ctor (inlined, 0x1400230cc) stores +0x34 = -1;
// the tree's factory zero-filled every field instead. B38 B8-1: the retail stores are kept; the
// fields retail leaves unwritten stay zero-filled (MemAlloc gives no guarantee).
SendComponentMessage::SendComponentMessage()
    : _field20(nullptr),
      _field28(nullptr),
      _field30(0),
      _field34(-1),
      _field38(nullptr) {}

// [0] 0x1400230f0
Object::Base* SendComponentMessage::DeletingDtor(unsigned flags) {
    this->~SendComponentMessage();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [3] 0x1400cb000
// [4] 0x1400caef0
// [9] 0x1400cae80

void* SendComponentMessage::CreateNewObject() {
    SendComponentMessage_ResolveFuncs();
    void* p = Object::detail::InternalMemAlloc(sizeof(SendComponentMessage), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) SendComponentMessage());
}
void* SendComponentMessage::CreateInPlaceObject(void* storage) {
    SendComponentMessage_ResolveFuncs();
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) SendComponentMessage());
}

DEFINE_EXE_VTABLE(SendComponentMessage, "_ZTV20SendComponentMessage")

DEFINE_EXE_TYPE(SendComponentMessage, "Action")
