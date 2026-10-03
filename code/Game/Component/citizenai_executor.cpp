// code/Game/Component/citizenai_executor.cpp
//
// Brick 4b — the ACTION-STACK EXECUTOR that runs the Action/Work objects the
// CitizenAI handlers push onto the citizen action stack (CharacterComponent+0x48),
// plus the CharacterComponent per-tick "move-tick" that drives it.
//
// Faithful 1:1 ports of three exe functions (lead-verified addresses this session):
//   0x1400c4c50  CitizenAI_ActionExecutor_c4c50   — the action-stack executor
//   0x1400c4bb0  CitizenAI_ActionFlush_c4bb0      — the executor's flush/cleanup tail
//   0x1401094c0  CharacterComponent_MoveTick_1094c0 — runs the executor IF the stack is
//                non-empty, then (only when char-index>=0) updates the ModelComponent
//                transform/animation. Ghidra MISLABELS this "CharacterComponent_OnAttach".
//
// CRITICAL ABI (transcribed from disassembly at 0x1400c4c50):
//   * Stack object layout (param_1 = CharacterComponent+0x48):
//       [param_1+0x00] = backing array base (slots of InternalBasePtr, 8 bytes each)
//       [param_1+0x18] = top index (int)   (== CharacterComponent+0x60)
//       [param_1+0x10] = the "flush" list head (used by c4bb0)
//   * Each stack entry is an InternalBasePtr; the wrapped node:
//       node+0x18 = 1-byte "entered/in-use" flag
//       node+0x10 = next-node link (the do/while advances via this, NOT node+0x18)
//       node->vtable+0x20 = precheck slot, +0x28 = main slot, +0x38 = exit slot
//   * vcall ABI (bug #6 — explicit forwarding):
//       precheck:  CALL [vt+0x20] with RCX=node, RDX=stack            (asm 0x1400c4ced)
//       main:      CALL [vt+0x28] with RCX=node, RDX=stack, R8=time   (asm 0x1400c4d57)
//       exit:      CALL [vt+0x38] with RCX=node, RDX=stack            (asm 0x1400c4ddc)
//   * RETURN WIDTH (bug #7): each slot return is read as INT (EAX), compared ==1 / ==0 / ==2.
//     Keep `int` returns through the function pointer typedefs — never bool.
//
// Crash-safety for the move-tick (0x1401094c0): in standalone the citizen's
// ModelComponent (CharacterComponent+0x68), its MeshInstance(+0x28)/Geometry/Picking,
// and the char render-index (self+0x40) are INERT (zeroed/negative). The disasm shape is:
//     RCX = self+0x48
//     if (*(int*)(RCX+0x18) /* self+0x60 top index */ >= 0) FUN_1400c4c50(RCX, time);
//     if (*(int*)(self+0x40) < 0) return;        // <-- early-return SKIPS the whole tail
//     ...big transform/animation/rendering block...
// Because self+0x40 < 0 in standalone, the early-return is the intended crash-safety: the
// transform tail (which touches the inert ModelComponent/MeshInstance/Geometry/Picking
// rendering layer) is NEVER reached. It is ported faithfully below but all its engine
// dispatch-slots / exe sub-callees are null-guarded; it stays dormant until ModelComponent
// is brought live. See the report for the rendering-coupled sub-callee table.
//


// graphics/*.h FIRST: they pull runtime/types/volume/Volume.h, whose global-scope
// `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once <windows.h> has
// been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Mesh.h"
#include "Component/PickingDescription.h"   // B.3.8 CB1a (TypeId)
#include <new>
#include "math/Math.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "options.h"
#include "perf_terrain.h"
#include "perf_experimental.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "picking/Scene.h"     // ENDGAME B.3.7 callers (K2)
#include "Action/Action.h"     // B.3.x W0: the executor's Action slots by name
#include "Component/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "Game/exe_constants.h"
#include "Game/Map.h"                          // Map::size / cells (GameUpdateLoop +0xF8)
#include "Component/CharacterComponent.h"
#include "Component/CharacterGrid.h"           // GameUpdateLoop +0x88
#include "Component/CharacterDescription.h"
#include "Component/ModelComponent.h"
#include "Component/PickingComponent.h"

namespace {

// ---------------------------------------------------------------------------
// Move-tick movement diagnostic (BANISHED_MOVETICK_TRACE). Default-off; proves
// the move-tick TAIL runs (char-index>=0) and that the per-frame transform write
// actually changes the citizen's world position at ModelComponent+0x158.
// Tracks the first few citizens whose tail executes and logs position deltas.
// ---------------------------------------------------------------------------
static int  s_mtTrace = -1;            // -1 = unread, 0 = off, 1 = on
static inline bool MoveTickTraceOn() {
    if (s_mtTrace < 0) {
        const char* e = Opt().movetickTrace;
        s_mtTrace = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s_mtTrace == 1;
}

// ---------------------------------------------------------------------------
// The Action slots the executor drives, by name (Action.h; return INT — bug #7):
//   precheck [4] AbstractImpl(stack), main [5] Execute(stack, time),
//   exit [7] Exit(stack).
// ---------------------------------------------------------------------------
static inline int CallPre(Action* node, ActionStack* stack) {
    return node->AbstractImpl(stack);   // slot 4
}
static inline int CallMain(Action* node, ActionStack* stack, void* time) {
    return node->Execute(stack, time);   // slot 5
}
static inline void CallExit(Action* node, ActionStack* stack) {
    node->Exit(stack);   // slot 7
}


// FUN_1400c5400 — TaskListCleanup, called when a main-slot returns 2 (push-more / requeue):
// WorkBase_TaskListCleanup_c5400, called by name.

} // namespace

// ===========================================================================
// FUN_1400c4bb0 — flush/cleanup tail. Walk the flush list at (stack+0x10):
//   for each node: CheckPtr, set node+0x18 = 1, call precheck(+0x20)(node) — break if
//   ret != 1; CheckPtr; advance via node+0x10. After the walk: DeletePtr(stack+0x10) +
//   re-Ctor the slot (InternalBasePtr default ctor, null internal).
// param_1 = stack object (== CharacterComponent+0x48).
// The precheck is Action slot 4 with RDX = the stack: 0x1400c4be0 `MOV RDX,RBX`
// (RBX = RCX = param_1 from 0x1400c4bbe) before `CALL [RAX+0x20]` at 0x1400c4bea.
// (An older note here read it as RCX-only; the disassembly loads RDX.)
// ===========================================================================
// B4 P2 CAI-2: no declaration outside this file; the stack is typed (Action.h ActionStack).
extern "C" void CitizenAI_ActionFlush_c4bb0(ActionStack* param_1) {
    Action* node = param_1->field_0x10;
    if (node != nullptr) {
        do {
            node->_status = true;                                // node+0x18 = 1
            int iVar1 = CallPre(node, param_1);                  // slot 4 (node, stack)
            if (iVar1 != 1) break;
            node = node->_next.Get();                            // advance via node+0x10
        } while (node != nullptr);
        reinterpret_cast< ::Object::InternalBasePtr*>(&param_1->field_0x10)->DeletePtr();   // B.3.3 bridge  // LEFT: Action.h (S2) ActionStack::field_0x10 is Action*, retail an InternalPtr<Action>
        // re-Ctor a default InternalBasePtr (null internal) and store into [stack+0x10]
        param_1->field_0x10 = nullptr;
    }
}

// ===========================================================================
// FUN_1400c4c50 — the action-stack executor. do/while over the stack top index.
// param_1 = stack object (== CharacterComponent+0x48); param_2 = Time* (forwarded to main).
// ===========================================================================
extern "C" void CitizenAI_ActionExecutor_c4c50(int64_t* param_1, void* param_2) {
    ActionStack* stack = reinterpret_cast<ActionStack*>(param_1);   // LEFT: SIG CitizenAI_ActionExecutor_c4c50
    bool bVar2 = false;            // R14: "stay" sticky flag → break the outer loop
    do {
        int iVar1 = stack->current;                                     // top index (stack+0x18)
        ::Object::InternalPtr<Action>* self = &stack->actions[iVar1];
        if (stack->actions[iVar1].Get() == nullptr) {
            stack->current = iVar1 - 1;                                 // empty slot → pop
        } else {
            int iVar3 = 1;
            if (-1 < iVar1) {
                Action* local_res8;
                if (!self->Get()->_status) {
                    // PRECHECK phase: walk the node list, set +0x18=1, call precheck(node,stack).
                    for (local_res8 = stack->actions[stack->current].Get();
                         local_res8 != nullptr;
                         local_res8 = local_res8->_next.Get()) {
                        local_res8->_status = true;                     // node+0x18 = 1
                        iVar3 = CallPre(local_res8, stack);
                        if (iVar3 != 1) goto LAB_d17;
                    }
                    iVar3 = 1;
                }
LAB_d17:
                if (-1 < stack->current) {
                    if (iVar3 == 1) {
                        // MAIN phase: walk the node list, call main(node,stack,time).
                        for (local_res8 = stack->actions[stack->current].Get();
                             local_res8 != nullptr;
                             local_res8 = local_res8->_next.Get()) {
                            iVar3 = CallMain(local_res8, stack, param_2);
                            if (iVar3 != 1) goto LAB_d85;
                        }
                        iVar3 = 1;
                        bVar2 = true;
                    }
LAB_d85:
                    if ((-1 < stack->current) && (iVar3 == 0)) {
                        // POP phase: walk the node list, for entered nodes clear +0x18 and
                        // call exit(node,stack); then DeletePtr the slot, re-Ctor, dec top.
                        for (local_res8 = stack->actions[stack->current].Get();
                             local_res8 != nullptr;
                             local_res8 = local_res8->_next.Get()) {
                            if (local_res8->_status) {                  // node+0x18
                                local_res8->_status = false;
                                CallExit(local_res8, stack);
                            }
                        }
                        stack->actions[stack->current].DeletePtr();
                        ::Object::InternalPtr<Action> local_res18;      // re-Ctor (null internal)
                        stack->actions[stack->current] = local_res18;
                        stack->current = stack->current - 1;
                        goto LAB_e38;
                    }
                }
                if (iVar3 == 2) {
                    WorkBase_TaskListCleanup_c5400(param_1);   // 0x1400c5400
                }
            }
LAB_e38:
            if (bVar2) break;
        }
    } while (-1 < stack->current);
    if (stack->current < 0) {
        CitizenAI_ActionFlush_c4bb0(stack);
    }
}

// ===========================================================================
// 0x1401094c0  CharacterComponent move-tick (Ghidra: "CharacterComponent_OnAttach").
// HEAD (exercised in standalone): run the executor if the stack is non-empty, then
// early-return when the char render-index (self+0x40) is < 0 — the crash-safety gate.
// TAIL (DORMANT in standalone, char-index < 0): the ModelComponent transform/animation
// update. Ported faithfully but every engine dispatch-slot / exe sub-callee is resolved
// lazily + null-guarded. The tail touches the inert rendering layer and must NOT run
// until ModelComponent is brought live. See report for the full sub-callee table.
// ===========================================================================
namespace {

// --- exe .rdata read by the tail: exe_constants.h (kRdata_140172220 180,
//     kRdata_140172238 360, kRdata_1401720e4 0.1, kRdata_1401720cc pi/180,
//     kRdata_140172114 0.5, kRdata_14017214c 1.0, kRdata_1401722a0 -180). The
//     0x1401722b0 abs mask / 0x1401722c0 sign-flip mask are expressed as fabs_f and a
//     negation below. A .rdata float stored as its bit pattern is read through RdataF.
static inline float RdataF(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

// --- exe sub-callees used by the tail, called by name ---
//   0x14009f3e0 GridToWorld = Reimpl_MapGetTerrainPos; 0x140018660 = CharMgr_sub_8660.
//   sinf / cosf (0x140131c90 / 0x140131090) come from CharacterComponent_SinCosf and
//   sqrtf (0x140132010) is __builtin_sqrtf, see the tail.

// --- engine calls the tail makes through exe import slots (all rendering-layer) ---
// Value-return ABIs (CLAUDE.md gotcha): MeshInstance::RootAnimationOffset returns
// Vector3 by value (RCX=this, RDX=hidden retbuf); Vector3::Dot returns Scalar by value
// (RCX=this, RDX=retbuf, R8=other); Scalar::Max is a const member returning Scalar by
// value (RCX=this, RDX=retbuf, R8=other). Verified vs disasm at 0x14010967b/0x14010968b/
// 0x140109802. Transform::operator= [0x140151280] and the Transform copy ctor
// [0x140151418] are Math::Transform's own.

// PERF (2026-09-04): bit-identical inline copies of the trivial Runtime math exports
// the hot paths called through dispatch slots (a DLL call per 3-float store). Bodies
// transcribed from the Runtime decompiles — note Dot's operand ORDER (y, x, z), kept
// so the float result is the same to the bit. Non-trivial helpers (Normalize, Heading,
// RootAnimationOffset) still go through the Runtime.
//   ?Dot@Vector3@Math@@ (0x18001dff0)                   -> InlV3Dot(a,&ret,b)
//   ?Max@Scalar@Math@@ (0x18005ff60)                    -> InlScalarMax(&a,&ret,&b)
// (B4 P2 CAI-2: the unused InlV3 / InlV3Splat / InlV2Dot / InlBoxIsect copies are gone.)
static inline float* InlV3Dot(const float* p, float* ret, const float* q) {
    *ret = q[1] * p[1] + q[0] * p[0] + q[2] * p[2];
    return ret;
}
static inline float* InlScalarMax(const float* a, float* ret, const float* b) {
    const float x = *a, y = *b;
    *ret = (y < x) ? x : y;
    return ret;
}

// PerfX's deferred pick push (perf-only, PICK_CULL) builds its transform copy through
// this: the Math::Transform copy constructor, which the exe slot 0x140151418 names.
static void PickXfCtorMember(void* xformOut, void* transformIn) {
    ::new (xformOut) Math::Transform(*static_cast<const Math::Transform*>(transformIn));   // B.3.1 bridge  // LEFT: SIG PerfX::PickCullSkip (standalone/perf_experimental.h, SA)
}
// PerfX's deferred pick push (perf-only, PICK_CULL) replays through this: the
// member Picking::Scene::SetTransform, which the exe slot 0x1401529a8 used to
// be resolved for (the slot's registered target is the same member's door).
static void PickSetXfMember(void* scene, uint32_t id, void* xform) {
    reinterpret_cast<Picking::Scene*>(scene)->SetTransform(id, *static_cast<const Math::Transform*>(xform));   // B.3.7 bridge  // LEFT: SIG PerfX::PickCullSkip (standalone/perf_experimental.h, SA)
}

// Picking::Scene::AddMesh (Runtime 0x1801252e0) registers a pick mesh and returns its
// id (stored at PickingComponent+0x28); ABI from Entity_impl_8f50 (exe 0x140128f50):
// (scene, meshHandle, &transform, ownerIBP, 0, 0, 0). ENDGAME B.3.7: called as a
// member (RegisterCitizenPickMesh); its file-local cache went with the last reader.
// IsDerivedFrom is called through the import slot 0x140152a98 = Object::Base's member.
static uint32_t         s_tidPickingDesc = 0;

// Resolves no door: it only caches the PickingDescription type id.
static bool s_tailInit = false;
static void EnsureTailInit() {
    if (s_tailInit) return;
    // Picking-registration closure (port of Entity_impl_8f50 = PickingComponent OnAdded).
    s_tidPickingDesc = PickingDescription::TypeId();
    s_tailInit = true;
}

// Register the citizen's mesh in the Picking::Scene so it becomes clickable.
// Faithful port of Entity_impl_8f50 (PickingComponent::OnAdded, exe 0x140128f50),
// specialised to the move-tick (we already hold the sibling ModelComponent at
// self+0x68 and the PickingComponent at self+0x70 — no Entity::FindComponent walk
// needed). Runs once per citizen when its pick-id (PickingComponent+0x28) is still
// unregistered (<0). Returns the new pick-id, or -1 if it could not register
// (description not a PickingDescription / no pick-mesh table — standalone-safe).
// Emit a one-shot reason string explaining why RegisterCitizenPickMesh bailed, so a
// live run pinpoints the remaining gap without a debugger. Cheap (first 8 only).
static void PickRegFail(const char* why) {
    if (!MoveTickTraceOn()) return;
    static int n = 0;
    if (n < 8) { ++n; fprintf(stderr, "[PickReg] FAIL: %s\n", why); fflush(stderr); }
}

static int RegisterCitizenPickMesh(CharacterComponent* self) {
    ModelComponent*   model    = self->modelComponent;     // +0x68
    PickingComponent* pickComp = self->pickingComponent;   // +0x70
    if (model == nullptr || pickComp == nullptr) { PickRegFail("model/pickComp null"); return -1; }

    int16_t variantIdx   = model->meshGroup;   // sVar1 (+0x164)
    int16_t modelVariant = model->meshIndex;   // sVar2 (+0x166)
    if (modelVariant < 0) { PickRegFail("modelVariant<0"); return -1; } // (-1 < sVar2) gate

    // The pick mesh comes from the PickingComponent's description (+0x18), which must
    // be a PickingDescription. Its pick-mesh table is at desc+0x20, indexed exactly
    // like the render mesh: *(*(desc+0x20) + variantIdx*0x10) + modelVariant*8.
    Description* desc = pickComp->_description;
    if (desc == nullptr || s_tidPickingDesc == 0) {
        PickRegFail("desc null / tidPickingDesc 0"); return -1;
    }
    if (!desc->IsDerivedFrom(s_tidPickingDesc)) {
        PickRegFail("desc !PickingDescription"); return -1;
    }
    PickingDescription::Group* meshTable = static_cast<PickingDescription*>(desc)->_groups;
    if (meshTable == nullptr) { PickRegFail("meshTable(desc+0x20) null"); return -1; }
    Picking::PickingMesh** group = meshTable[variantIdx].meshes;
    if (group == nullptr) { PickRegFail("mesh group null"); return -1; }
    Picking::PickingMesh* meshHandle = group[modelVariant];
    if (meshHandle == nullptr) {
        PickRegFail("meshHandle null"); return -1;
    }

    // Transform snapshot (0x1C bytes, model+0x148..+0x163) — the move-tick wrote it
    // this frame; the original copies it into a stack Transform passed by pointer.
    Math::Transform xform = model->transform;

    Picking::Scene* scene = GameUpdateLoop::_singleton->m_pickingScene;   // +0x70
    void* owner = pickComp->userData;   // +0x30 entity IBP value
    uint32_t id = scene->AddMesh(
        Object::ExternalPtr<Picking::PickingMesh>(meshHandle),
        xform, owner, 0, 0, nullptr);
    pickComp->pickingId = static_cast<int32_t>(id);
    return static_cast<int>(id);
}

static inline float fabs_f(float v) {
    // matches the disasm's CVTPS2PD; ANDPD abs-mask; CVTPD2PS — plain magnitude.
    return v < 0.0f ? -v : v;
}

} // namespace

extern "C" void CharacterComponent_SinCosf(float x, float* s, float* c);   // character_trig.cpp

extern "C" void CharacterComponent_MoveTick_1094c0(uint8_t* selfBytes, void* time) {
    CharacterComponent* self = reinterpret_cast<CharacterComponent*>(selfBytes);   // LEFT: SIG CharacterComponent_MoveTick_1094c0 (charactercomponent.cpp's extern)
    // ---- HEAD (exercised) -------------------------------------------------
    // if (-1 < *(int*)(self+0x60)) run the executor on the stack at self+0x48.
    if (-1 < self->currentAction) {
        CitizenAI_ActionExecutor_c4c50(reinterpret_cast<int64_t*>(&self->actions), time);   // LEFT: SIG CitizenAI_ActionExecutor_c4c50
    }
    // Crash-safety gate: char render-index < 0 (always in standalone) → skip the tail.
    if (self->characterIndex < 0) {
        return;
    }

    // ---- TAIL (DORMANT in standalone: only reached when char-index >= 0) --
    // Faithful transcription of the ModelComponent transform/animation update. Every
    // engine slot / exe sub-callee below is null-guarded; the block touches the inert
    // ModelComponent(+0x68)/MeshInstance/Geometry/Picking rendering layer.
    EnsureTailInit();

    // Retail reads Graphics::MeshInstance::_rootOnlyUpdates through its import
    // slot 0x140151f30; the class static is that byte (ENDGAME B.3.9 SS2).
    uint8_t rootOnly = ::Graphics::MeshInstance::_rootOnlyUpdates ? 1 : 0;

    ModelComponent* model = self->modelComponent;
    float fx = model->transform.translation.x;   // +0x158
    float fy = model->transform.translation.y;   // +0x15c
    float fz = model->transform.translation.z;   // +0x160
    float pos[3] = { fx, fy, fz };
    const float mtEntryX = fx, mtEntryY = fy, mtEntryZ = fz;  // for movement trace

    float fVar14 = self->field_0x3C;
    float fVar10 = self->modelComponent->yaw - self->characterDescription->_f24;
    float fVar13 = fVar14 - fVar10;
    // Exe .rdata math constants (exe_constants.h; values verified against the exe):
    // 0x140172220=180, 0x140172238=360, 0x1401720e4=0.1 (turn epsilon),
    // 0x1401722a0=-180, 0x14017214c=1.0 (g_fUVScale).
    float c172220 = kRdata_140172220;
    float c172238 = RdataF(kRdata_140172238);
    float c1720e4 = RdataF(kRdata_1401720e4);
    float c1722a0 = RdataF(kRdata_1401722a0);
    // g_fUVScale is loaded from 0x14017214c (=1.0) per disasm at 0x14010961f — NOT the
    // 0x140172148 sibling (=0.99897).
    float uvScale = kRdata_14017214c;

    if (c172220 <= fabs_f(fVar13)) {
        if (fVar13 <= 0.0f) {
            fVar13 = fVar13 + c172238;
        } else {
            fVar13 = -(c172238 - fVar13);   // XOR sign-flip
        }
    }
    if (c1720e4 < fabs_f(fVar13)) {
        float t = self->characterDescription->_slowSpeed;   // +0x28
        fVar14 = (fVar13 + fVar10) * (uvScale - t) + t * fVar10;
        if (fVar14 <= c172220) {
            if (fVar14 < c1722a0) fVar14 = fVar14 + c172238;
        } else {
            fVar14 = fVar14 - c172238;
        }
    }

    // this = MeshInstance::RootAnimationOffset(model+0x28) (Vector3 by-value into retbuf);
    // speed = sqrt(this.Dot(this)) * self+0x38 — magnitude of the root-motion offset.
    float rootAnimVec[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (model)
        // Retail retslot door; its body moves to the call site as exactly one
        // construction into the SAME storage the door wrote (the 4th float stays 0).
        ::new (static_cast<void*>(rootAnimVec)) Math::Vector3(
            self->modelComponent->meshInstance.RootAnimationOffset());
    float dotOut = 0.0f;
    InlV3Dot(rootAnimVec, &dotOut, rootAnimVec);   // was s_slot_dot (Runtime call)
    // PERF (2026-09-05): 0x140132010 is our own WorkImpl_Sqrtf_132010 (= sqrtf); the
    // builtin compiles to SQRTSS, which is correctly rounded, so this is bit-identical
    // and saves an indirect call per character per tick (1.4% of the phase).
    float speed = __builtin_sqrtf(dotOut);
    speed = speed * self->field_0x38;
    if (0.0f < speed) {
        float ax = speed * self->field_0x2C.x;
        float ay = speed * self->field_0x2C.y;
        float az = speed * self->field_0x2C.z;
        // two Vector3 ctors in the original (locals), result folded into pos.
        pos[0] = fx + ax; pos[1] = fy + ay; pos[2] = fz + az;
        fx = pos[0]; fy = pos[1];
    }

    Map* gameWorld = GameUpdateLoop::_singleton->m_game;   // +0xF8
    bool blockedHandled = false;
    if (rootOnly == 0) {
        int dim = gameWorld->size;
        int ix = static_cast<int>(fx + static_cast<float>(dim >> 1));
        int iy = static_cast<int>(fy + static_cast<float>(dim >> 1));
        if (ix < 0) ix = 0;
        if (dim <= ix) ix = dim - 1;
        if (iy < 0) iy = 0;
        if (dim <= iy) iy = dim - 1;
        MapCell* cellBase = gameWorld->cells;   // +0x80
        // [PerfX] TERRAIN_SIDECAR: cells[dim*iy+ix].type via the mirror (perf_terrain.h).
        if (PerfX::TerrainGet(reinterpret_cast<const uint8_t*>(cellBase), gameWorld->cellCount,   // LEFT: SIG PerfX::TerrainGet (standalone/perf_terrain.h, SA)
                              static_cast<int>(static_cast<int64_t>(dim) * iy + ix)) == 0x0b) {
            pos[2] = 0.0f; pos[0] = fx; pos[1] = fy;
            blockedHandled = true;     // goto LAB_14010981e
        }
    }

    if (!blockedHandled) {
        if (!self->characterDescription->_b3c) {
            if (rootOnly == 0) {
                float triOut[4] = { 0, 0, 0, 0 };
                float* pf = nullptr;
                pf = Reimpl_MapGetTerrainPos(gameWorld, triOut, pos, '\0', nullptr);   // 0x14009f3e0
                // pos.z = Scalar(gridZ).Max(desc+0x44)  — clamp terrain height to the
                // description's min. Const-member by-value ABI: (this=gridZ, retbuf, other).
                float maxThis  = pf ? pf[2] : 0.0f;
                float maxOther = self->characterDescription->_f44;
                float maxRet   = maxThis;
                if (pf) { fx = pf[0]; fy = pf[1]; }
                InlScalarMax(&maxThis, &maxRet, &maxOther);   // was s_slot_max (Runtime call)
                pos[2] = maxRet;
                pos[0] = fx; pos[1] = fy;
            }
        } else {
            pos[2] = self->characterDescription->_f40;
            pos[0] = fx; pos[1] = fy;
        }
    }

    // LAB_14010981e:
    fVar14 = fVar14 + self->characterDescription->_f24;
    // deg->half-angle: 0x1401720cc=0.0174533 (pi/180) * 0x140172114=0.5 (half angle for
    // the (-sin,cos) heading pair).
    float c1720cc = RdataF(kRdata_1401720cc);
    float c172114 = kRdata_140172114;
    float ang = fVar14 * c1720cc * c172114;
    // Retail calls sinf (0x140131c90) then cosf (0x140131090) on the same angle; both
    // are ours (character_trig.cpp), so take the pair from one reduction.
    float s_ang = 0.0f, c_ang = 0.0f;
    CharacterComponent_SinCosf(ang, &s_ang, &c_ang);
    float negSin = -s_ang;

    // Build the source Transform (0x1C bytes). Layout VERIFIED vs disasm at
    // 0x140109856-0x14010987c (source built at RSP+0x68, copied into model+0x148):
    //   +0x00=0  +0x04=0  +0x08=negSin  +0x0c=cos  +0x10=pos.x  +0x14=pos.y  +0x18=pos.z
    // The first 8 bytes are a zeroed 2D translation; +0x08/+0x0c are the heading
    // (-sin, cos) pair; +0x10.. is the 3D world position. (Earlier port had pos.xy
    // and the sin/cos pair SWAPPED — corrupts the citizen render transform. The
    // original locals are, in ascending address: local_d0=0, local_cc=0,
    // local_c8=negSin, local_c4=cos, local_c0=CONCAT44(pos.y,pos.x), local_b8=pos.z.)
    // (B4 P2 CAI-2: was a float[8] whose eighth float nothing read; Transform::operator=
    //  copies the seven dwords.)
    Math::Transform xform;
    xform.rotation.x    = 0.0f;   xform.rotation.y    = 0.0f;
    xform.rotation.z    = negSin; xform.rotation.w    = c_ang;
    xform.translation.x = pos[0]; xform.translation.y = pos[1];
    xform.translation.z = pos[2];

    model = self->modelComponent;
    model->transform = xform;   // Transform::operator=
    model->yaw = fVar14;
    // [PerfX GEOMXF_LASTONLY -- NON-RETAIL] retail pushes the transform into the
    // Geometry (bounds, attached children, scene quadtree) on EVERY speed iteration; only
    // the last one is ever drawn, and the render-side interpolation is snapped from that
    // same last update. Intermediate iterations skip the push. model+0x148 (the gameplay
    // position, read by StatusIcon/Particle/pick code) is still written every tick above.
    // [PerfX GEOM_CULL -- NON-RETAIL] on the last iteration an OFF-SCREEN citizen also
    // skips the push: nothing draws it, the frustum margin (6 units, ~12 frames of pan)
    // updates it before it can enter the view, and PerfX pushes model+0x148 for every
    // culled model when the sim pauses.
    bool pushGeom = true;   // was a Geometry::SetTransform door-readiness test
    if (pushGeom) {
        if (rootOnly != 0) {
            if (PerfX::GeomXfLastOnly()) { pushGeom = false; PerfX::Count(PerfX::kCntGeomXfSkipped); }
        } else if (PerfX::GeomCull() && PerfX::ModelCulled(model)) {
            pushGeom = false; PerfX::Count(PerfX::kCntGeomCulled);
        }
    }
    if (pushGeom)
        model->meshInstance.SetTransform(model->transform);   // Graphics::Geometry::SetTransform

    // ---- movement trace (proves the transform write moves the mesh) --------
    if (MoveTickTraceOn()) {
        static void* s_tracked[4] = { nullptr, nullptr, nullptr, nullptr };
        static int   s_nTracked = 0;
        static long  s_calls = 0;
        int slot = -1;
        for (int i = 0; i < s_nTracked; ++i)
            if (s_tracked[i] == self) { slot = i; break; }
        if (slot < 0 && s_nTracked < 4) { slot = s_nTracked; s_tracked[s_nTracked++] = self; }
        if (slot >= 0) {
            float dx = pos[0] - mtEntryX, dy = pos[1] - mtEntryY, dz = pos[2] - mtEntryZ;
            bool moved = (dx*dx + dy*dy + dz*dz) > 1e-10f;
            // Facing diagnostic (TODO #3-B): desiredFacing = self+0x3c (set by the
            // MoveTo action's UpdateFacing toward the next waypoint); appliedYaw =
            // model+0x168 + heading pair just written into model+0x148. If the
            // citizen moves but appliedYaw never changes, the facing pipeline is
            // not turning the mesh; if desiredFacing changes but appliedYaw lags,
            // it is the smoothing (desc+0x28). model is the ModelComponent (self+0x68).
            float desiredFacing = self->field_0x3C;
            float appliedYaw    = model ? model->yaw : 0.0f;
            // Log every move, plus a heartbeat every 240 calls even when static.
            if (moved || (s_calls % 240) == 0) {
                fprintf(stderr,
                    "[MoveTick] cit#%d self=%p chIdx=%d pos=(%.4f,%.4f,%.4f) "
                    "delta=(%.5f,%.5f,%.5f) speed=%.4f vel38=%.4f stackTop=%d "
                    "facing(want=%.3f applied=%.3f)\n",
                    slot, (void*)self, self->characterIndex,
                    pos[0], pos[1], pos[2], dx, dy, dz, speed,
                    self->field_0x38,
                    self->currentAction,
                    desiredFacing, appliedYaw);
                fflush(stderr);
            }
        }
        ++s_calls;
    }

    if ((self->pickingComponent != nullptr) && (rootOnly == 0)) {
        uint8_t localXform[136];
        Math::Transform* localXf = ::new (static_cast<void*>(localXform)) Math::Transform(
            self->modelComponent->transform);   // Transform copy ctor
        uint32_t pickId = static_cast<uint32_t>(self->pickingComponent->pickingId);
        // First time through (pick-id still unregistered) register the citizen mesh in
        // the Picking::Scene so it becomes clickable.
        if (static_cast<int>(pickId) < 0) {
            int newId = RegisterCitizenPickMesh(self);
            pickId = static_cast<uint32_t>(newId);
            if (MoveTickTraceOn()) {
                static int s_pickLog = 0;
                if (s_pickLog < 8) { ++s_pickLog;
                    fprintf(stderr, "[PickReg] self=%p pickId=%d\n", (void*)self, newId);
                    fflush(stderr);
                }
            }
        }
        if (-1 < static_cast<int>(pickId)) {
            Picking::Scene* pickScene = GameUpdateLoop::_singleton->m_pickingScene;   // +0x70
            // [PerfX PICK_CULL -- NON-RETAIL] an off-screen citizen cannot be clicked, so
            // its pick-scene transform is deferred; PerfX replays it if the sim pauses.
            if (!PerfX::PickCullSkip(model, pickScene, pickId,
                                     &PickXfCtorMember,
                                     &PickSetXfMember)) {
                pickScene->SetTransform(pickId, *localXf);
            }
        }
    }

    // 0x140018660 (was its CharMgr_sub_8660 forwarder, charmanager.cpp)
    GameUpdateLoop::_singleton->m_characterGrid->Place(self->characterIndex);
}

