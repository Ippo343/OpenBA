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
#include "object/TypeInfo.h"
#include "object/Type.h"   // RtObject::RtObject_Type_Serialize
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Game/force_work.h"   // ForceWork::Readable — stale action-target guard
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (diagnostic filter)
#include <cstdio>
#include <cstdint>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/Component.h"          // ENDGAME B.3.8 CB3: slot 15 OnMessage
#include "Component/OrchardComponent.h"   // ENDGAME B.3.8 CB3: OrchardComponent::TypeId()
#include "Entity/ComponentEntity.h"       // ENDGAME B.3.9 C4: ComponentEntity::TypeId()
#include "Component/CitizenComponent.h"   // CitizenComponent::TypeId() (moved ctor)

// ============================================================
// Helpers, called by name (B3R)
//   import slot 0x1401516b0 ?Serialize@Type@Object@@QEAAXAEAVSerializer@IO@@@Z ->
//               RtObject::RtObject_Type_Serialize (object/Type.h)
// ============================================================

// ============================================================
// Serialize (0x1400cb000, vtable slot 3)
// Serializes status, two entity refs with type checks, component type,
// message ID, and two uint params.
// ============================================================
void SendComponentMessage::Persist(void* stream) {
    Action::Persist(stream);   // inlined in retail: Serializer(stream, 0), status +0x18

    // Serialize entity refs + message fields (version 0)
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge (Persist's stream is void*)

    // Target entity ref (+0x20) with type validation
    _field20.InternalBasePtr::Serialize(ser);
    if (_field20.Get() != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(_field20.Get(), ComponentEntity::TypeId())) {
            _field20 = nullptr;
        }
    }

    // Source entity ref (+0x28) with type validation
    _field28.InternalBasePtr::Serialize(ser);
    if (_field28.Get() != nullptr) {
        if (!::Object::IsDerivedFromNullSafe(_field28.Get(), ComponentEntity::TypeId())) {
            _field28 = nullptr;
        }
    }

    // Component type (+0x30) — Object::Type::Serialize
    RtObject::RtObject_Type_Serialize(&_field30, &ser);   // slot 0x1401516b0

    ser.Serialize(_field34, nullptr);   // +0x34 message id (int)
    ser.Serialize(_field38, nullptr);   // +0x38 message param (uint)
    ser.Serialize(_field3C, nullptr);   // +0x3C message param (uint)
}

// ============================================================
// AbstractImpl (0x1400caef0, vtable slot 4)
// Finds component by type ID on target entity, then calls its
// message handler virtual method (vtable+0x78).
// Returns 0 on success, 2 if component not found.
// ============================================================
int  SendComponentMessage::AbstractImpl(void* actionStack) {
    (void)actionStack;

    uint32_t componentTypeId = _field30;

    // CheckPtr on target entity
    ComponentEntity* target = _field20.Get();
    uintptr_t entity = reinterpret_cast<uintptr_t>(target);  // CAST-OK: ptr_int

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
        ForceWork::Readable(target, 0x44) &&
        ((target->_entityFlags >> 10) & 1) != 0;
    if (!entity || !canonical || dying ||
        !ForceWork::Readable(target, 0x40) ||
        !ForceWork::Readable(target->_components, 8)) {
        static int s_reported = 0;
        if (s_reported++ < 20) {
            std::fprintf(stderr, "[SendCompMsg] ABORT: target entity=%p is not live "
                         "(canonical=%d dying=%d msgId=%d compType=%u) — action dropped "
                         "instead of reading freed memory\n",
                         static_cast<void*>(target), (int)canonical, (int)dying,
                         _field34, componentTypeId);
            std::fflush(stderr);
        }
        return 2;
    }

    // Search entity's component array for matching type
    int32_t compCount = target->_componentCount;
    Object::InternalPtr<Component>* compArray = target->_components;
    Component* foundComp = nullptr;

    for (int i = 0; i < compCount; i++) {
        Component* comp = compArray[i].Get();

        // Backstop for the same sentinel one level down: an entity that is NOT marked for
        // removal can still carry an individually released slot. Retail derefs this
        // unguarded and is right to — it never puts a sentinel in a live entity's array —
        // so skip the slot rather than clamping it, and keep the scan going.
        uintptr_t compAddr = reinterpret_cast<uintptr_t>(comp);  // CAST-OK: ptr_int
        if (compAddr < 0x10000 || compAddr > 0x00007FFFFFFFFFFFULL) continue;

        // Get component type ID via vtable+0x10
        int typeId = static_cast<int>(comp->Type());   // slot 2

        if (typeId == (int)componentTypeId) {
            // Found matching component
            foundComp = compArray[i].Get();
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
                         static_cast<void*>(target), componentTypeId,
                         _field34, _field38, compCount,
                         static_cast<void*>(foundComp));
            std::fflush(stderr);
        }
    }

    if (foundComp == nullptr) {
        return 2;
    }

    // Call the component's message handler (slot 15, vtable+0x78):
    // component, msgId, &sourceEntity, param1, param2
    ComponentEntity* sourceEntityRef = _field28.Get();
    foundComp->OnMessage(_field34, &sourceEntityRef, _field38, _field3C);

    return 0;
}

// ============================================================
// GetTarget (0x1400cae80, vtable slot 9)
// Returns true if param matches entity at +0x20 or +0x28.
// ============================================================
bool SendComponentMessage::GetTarget(ComponentEntity** param) {
    // CheckPtr on +0x20, then compare [param] directly (no CheckPtr on param)
    if (*param == _field20.Get()) {
        return true;
    }

    // CheckPtr on +0x28, then compare [param]
    return *param == _field28.Get();
}

// ============================================================
// class SendComponentMessage (SendComponentMessage.h) — ENDGAME B.3.8 ACT.
// The hand-built g_SendComponentMessage_Vtable[10], its per-class type-id cache and the
// Action_Alloc / Action_InitInPlace factories are gone: the compiler emits
// the table, DECLARE_EXE_TYPE answers slots 1/2.
// ============================================================
#include "Object/factory_registration.h"

// create 0x140023070. Retail's default ctor (inlined, 0x1400230cc) stores +0x34 = -1;
// the tree's factory zero-filled every field instead. B38 B8-1: the retail stores are kept; the
// fields retail leaves unwritten stay zero-filled (MemAlloc gives no guarantee).
SendComponentMessage::SendComponentMessage()
    : _field20(nullptr),
      _field28(nullptr),
      _field30(0),
      _field34(-1),
      _field38(0),
      _field3C(0) {}

// [3] 0x1400cb000
// [4] 0x1400caef0
// [9] 0x1400cae80

void* SendComponentMessage::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(SendComponentMessage), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) SendComponentMessage());
}
void* SendComponentMessage::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) SendComponentMessage());
}


// ============================================================
// FUN_1400cb1d0 — SendComponentMessage_Action CopyCtor
// Base Internal ctor, Action -> SendComponentMessage vtable; copies IBP +0x20 (param_2),
// IBP +0x28 (param_5), int +0x30 (*param_3), +0x34 (param_4), +0x38 (param_6), +0x3c=0.
// ============================================================
void* Pf_SendComponentMessageCopyCtor_b1d0(void* param_1, uint64_t* param_2, uint32_t* param_3,
                                           uint32_t param_4, uint64_t* param_5, uint32_t param_6) {
    // B4 FORWARD: outside callers (Action/action_pf_ctors.cpp, the
    // Component citizen builders, the Work action builders) hold the refs as uint64_t*.
    return static_cast<Object::Base*>(
        new (param_1) SendComponentMessage(reinterpret_cast<ComponentEntity* const*>(param_2), param_3,
                                           param_4, reinterpret_cast<ComponentEntity* const*>(param_5),
                                           param_6));
}

// 0x1400cb1d0 — the ctor body: +0x20 = *param_2, +0x28 = *param_5, then the retail stores
// (and the tree's typeid heal).
SendComponentMessage::SendComponentMessage(ComponentEntity* const* param_2, const uint32_t* param_3,
                                           uint32_t param_4, ComponentEntity* const* param_5,
                                           uint32_t param_6)
    : _field20(*param_2),
      _field28(*param_5) {
    uint32_t compTypeId = *param_3;
    // [ECONOMY M-D — BANISHED_LIVE_STORAGE typeid heal]
    // The eat push (PerformWork_Action_Ctor6, citizenai_actionctors.cpp) reads the
    // component-typeid from .rdata DWORD_1401c7158 (CitizenComponent) — deliberately
    // NOT registered in standalone (see reg_work_tail_trade.cpp), so it arrives here
    // as 0 and SendComponentMessage::AbstractImpl can never match a component
    // (returns 2, message silently dropped). Under BANISHED_LIVE_STORAGE, heal the
    // msg==2 (EAT) case only — msg 2 with a zero typeid is uniquely the ctor6
    // eat-delivery message — substituting the live GetTypeIdByName CRC that our
    // CitizenComponent live vtable's GetTypeId (slot 2) returns. msg 0 stays
    // unhealed: a zero-typeid msg 0 is ambiguous (CitizenAIComponent 0x1401c70e8
    // completion lead vs CitizenComponent warm-house), deferred.
    if (compTypeId == 0 && param_4 == 2) {
        compTypeId = CitizenComponent::TypeId();
    }
    _field30 = compTypeId;
    _field34 = static_cast<int32_t>(param_4);
    _field38 = param_6;
    _field3C = 0;
}

DEFINE_EXE_TYPE(SendComponentMessage, "Action")
