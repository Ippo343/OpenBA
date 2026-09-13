// code/Game/Game/forester_force_harness.cpp
// Phase 3 economy WORK-half — Phase B-real / G1: drive the REAL ForesterWork producer.
//
// Recon (2026-06-26) corrected the prior G1 premise. ForesterWork::Execute (0x1400eaec0)
// is NOT dispatched by any CitizenAI state and the citizen's profession (+0x1f0) is NOT a
// gate (it only flips a priority-flag bit). ForesterWork is an ACTION on the citizen's
// CharacterComponent action stack (charComp = citizen+0x278, stack @+0x48, top-index @+0x60),
// ticked by the executor FUN_1400c4c50 — which runs every frame from
// CharacterComponent::MoveTick (CharacterComponent_MoveTick_1094c0, env
// BANISHED_LIVE_CHARACTER_MOVETICK) when the top-index is >= 0. Organically the building's
// ResourceProduceComponent controller clones a ForesterWork template onto each present
// worker's stack; that controller is inert in standalone, so nothing ever pushes the work.
//
// This harness (env BANISHED_FORCE_FORESTERWORK, default OFF) replaces that inert controller
// for ONE force-worked citizen: when the citizen's action stack is idle it constructs a
// ForesterWork (faithful to ForesterWork_CopyCtor 0x14011f780 == Work_Ctor25 + vtable + zero
// +0x48/+0x50/+0x58) with the work-area set to the force-worked ForesterLodge, and pushes it
// onto the stack. MoveTick then ticks ForesterWork::Execute, which scans the lodge radius for
// a harvestable tree and pushes a cut PerformWork sequence (-> produces logs ->
// RegisterRawMaterial, the Phase-B-verified accounting tail).
//
// Work_Ctor25 (verified vs disasm) computes the work fields from the work-area entity:
//   +0x10 = work-area IBP (the lodge)
//   +0x18 = work-area material description (FindComp_MatDesc; unused by Execute, skipped)
//   +0x20 = -1 (priority; left at the Work-ctor default, as the clone does)
//   +0x28 = work-area CENTER cell =
//             ((mc.fpH>>1)+mc.tileY)*mapDim + ((mc.fpW>>1)+mc.tileX)
//           where mc = the lodge's MapComponent (+0x28 tileX/+0x2a tileY/+0x2c fpW/+0x2e fpH,
//           written at spawn by starttown.cpp) and mapDim = *(int*)*(gameWorld+0xf8).
//
// CRASH SAFETY: ForesterWork::Execute reads the work-area's ForesterComponent (DAT_1401c7740)
// description as a ForesterDescription (DAT_1401c7778) UNGUARDED (workplaceDesc+0x28 etc.), so
// if the lodge lacks a ForesterComponent the work would fault. We therefore VERIFY the lodge
// carries a ForesterComponent whose description derives from ForesterDescription before ever
// pushing — and dump the component set once so the first run reports exactly what the lodge
// has. If the check fails we never push (report-only), so the default-off path and a
// mis-templated lodge both stay crash-free.


#include "engine_funcs.h"
#include "options.h"
#include "resolve.h"
#include "force_work.h"                     // generic ForceWork core (probes + drive)
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// The force-worked ForesterLodge (starttown.cpp, BANISHED_LIVE_WORKPLACE).
extern void* g_pForceWorkBuilding;

// ForesterWork Execute-readiness (code/Game/Work/foresterwork.cpp): true once every sub-fn /
// type-id global / .rdata float that ForesterWork::Execute actually derefs is resolved
// (excludes the Dtor-only parent vtables + serialize-only ids, which Execute never touches and
// which are unregistered in standalone). We refuse to push until it's true, so Execute can
// never null-deref a dependency.
extern bool ForesterWork_ExecuteReady();

// Standalone ForesterWork factory (code/Game/Work/foresterwork.cpp): Work_Alloc(0x60) +
// Work_GenericCtor (Object::Internal::Internal, zero from +0x10, +0x20/+0x28 = -1, install
// the ForesterWork vtable, resolve the Slot fns). Leaves +0x10 (work-area) zeroed.
extern "C" void* ForesterWork_Factory(void);

// ForesterWork::Execute driver (foresterwork.cpp): runs the forester work tick directly with
// the worker citizen's IBP (the Work-driver ABI). Returns the work result code; on finding a
// harvestable tree it pushes a PerformWork (cut) sequence onto the citizen's action stack.
extern "C" int ForesterWork_ForceDrive(void* self, void* citizenIBP);

static bool ForceForesterEnvOn() {
    static int s = -1;
    if (s < 0) {
        const char* e = Opt().forceForesterWork;
        s = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s == 1;
}

// Crash-safe probes + component scan + Work_Ctor25 center-cell now live in the shared
// ForceWork core (force_work.h). Alias the two used repeatedly for readability.
using ForceWork::Readable;
using ForceWork::SafeGetTypeId;
using ForceWork::FindComp;

// Dump the lodge component set + locate a usable ForesterComponent (once). Returns the
// ForesterComponent whose description derives from ForesterDescription, or null.
static void* VerifyLodgeForester(void* lodge, bool dump) {
    static uint32_t tidForesterComp = 0, tidForesterDesc = 0;
    if (!tidForesterComp) tidForesterComp = ::Object::GetTypeIdByName("ForesterComponent");
    if (!tidForesterDesc) tidForesterDesc = ::Object::GetTypeIdByName("ForesterDescription");

    if (dump) {
        int count = Readable(lodge, 0x40) ? *reinterpret_cast<int*>((uintptr_t)lodge + ForceWork::kEntCompCnt) : 0;
        uintptr_t arr = Readable(lodge, 0x40) ? *reinterpret_cast<uintptr_t*>((uintptr_t)lodge + ForceWork::kEntCompArr) : 0;
        fprintf(stderr, "[ForceForester] lodge=%p compCount=%d tid(ForesterComponent)=%u tid(ForesterDescription)=%u\n",
                lodge, count, tidForesterComp, tidForesterDesc);
        if (count > 0 && count <= 512 && Readable(reinterpret_cast<void*>(arr), (size_t)count * 8)) {
            for (int i = 0; i < count; ++i) {
                void** slot = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
                void* comp = *slot;
                uint32_t ct = SafeGetTypeId(comp);
                void* desc = (comp && Readable(comp, 0x20)) ? *reinterpret_cast<void**>((uintptr_t)comp + 0x18) : nullptr;
                uint32_t dt = SafeGetTypeId(desc);
                const char* mark = (ct == tidForesterComp) ? "  <== ForesterComponent" : "";
                fprintf(stderr, "[ForceForester]   comp[%d] tid=%u desc=%p descTid=%u%s\n",
                        i, ct, desc, dt, mark);
            }
        }
        fflush(stderr);
    }

    void* fc = FindComp(lodge, tidForesterComp);
    if (!fc) return nullptr;
    // ForesterWork::Execute reads fc+0x18 (the description) and treats it as a
    // ForesterDescription via IsDerivedFrom; verify that holds before we let it run.
    void* desc = Readable(fc, 0x20) ? *reinterpret_cast<void**>((uintptr_t)fc + 0x18) : nullptr;
    if (!desc) return nullptr;
    if (fn_IsDerivedFrom && tidForesterDesc && !fn_IsDerivedFrom(desc, tidForesterDesc))
        return nullptr;
    return fc;
}

// Driver. Called from the CitizenAI UpdateAdapter for ONE force-worked citizen. Returns true
// if it "owns" the citizen this tick (caller should skip the competing AI state machine), so
// ForesterWork — not Leisure/Wander — keeps driving the stack.
extern "C" bool StartTown_ForcePushForesterWork(void* citizen) {
    if (!ForceForesterEnvOn()) return false;
    void* lodge = g_pForceWorkBuilding;
    if (!lodge || !citizen) return false;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return false;
    if (!fn_InternalBasePtr_Ctor) return false;

    // Bind to the FIRST force-worked citizen only (single, observable subject).
    static void* s_owner = nullptr;
    if (!s_owner) s_owner = citizen;
    if (citizen != s_owner) return false;

    // One-time lodge verification + dump. If the lodge can't host ForesterWork, report and
    // never push (report-only; keeps a mis-templated lodge crash-free).
    static int s_verified = -1;   // -1 unchecked, 0 unusable, 1 usable
    if (s_verified < 0) {
        void* fc = VerifyLodgeForester(lodge, /*dump*/true);
        // Refuse to push unless EVERY ForesterWork dependency (sub-fns, type-id globals,
        // .rdata floats) resolved — else Execute would null-deref. ResolveFuncs prints the
        // missing sub-fns in _DEBUG, so a FAIL here pinpoints the next domino.
        bool depsOk = ForesterWork_ExecuteReady();
        s_verified = (fc && depsOk) ? 1 : 0;
        // The ForesterComponent's cut/plant toggles (+0x50 plant-enabled, +0x51 cut-enabled)
        // are configured from the lodge UI / ForesterDescription defaults in the real game; on
        // our INERT lodge both default to 0, so ForesterWork::Execute would neither cut nor
        // plant (idle forever). Force CUT-only here (the force harness stands in for that
        // configuration): +0x51 = 1 (cut trees), +0x50 = 0 (no planting). This drives the
        // CUT path -> PerformWork -> CreateRawMaterial -> RegisterRawMaterial production chain.
        if (s_verified == 1 && fc && Readable(fc, 0x52)) {
            *reinterpret_cast<uint8_t*>((uintptr_t)fc + 0x51) = 1;   // cut-trees enabled
            *reinterpret_cast<uint8_t*>((uintptr_t)fc + 0x50) = 0;   // plant-trees disabled
            fprintf(stderr, "[ForceForester] set ForesterComponent cut-flag (+0x51)=1, plant(+0x50)=0\n");
            fflush(stderr);
        }
        if (!fc) {
            fprintf(stderr, "[ForceForester] lodge carries no usable ForesterComponent "
                    "(ForesterDescription) — NOT pushing ForesterWork (report-only). The work-area "
                    "entity likely lives on a MultiObject sub-entity; resolve before driving.\n");
            fflush(stderr);
        }
        if (!depsOk) {
            fprintf(stderr, "[ForceForester] ForesterWork_ResolveFuncs FAILED (a sub-fn / type-id "
                    "/ .rdata float is unresolved) — NOT pushing (report-only). See the "
                    "[ForesterWork] ResolveFuncs MISSING lines above.\n");
            fflush(stderr);
        }
    }
    if (s_verified != 1) return false;

    // Construct the ForesterWork ONCE and keep it (it carries a per-work cell BST @+0x48 and a
    // fail counter @+0x58 that must survive across drives). Faithful to ForesterWork_CopyCtor:
    // ForesterWork_Factory does the object build (vtable + Object::Internal + +0x20/+0x28 = -1),
    // then we set the Work_Ctor25 work-area-dependent fields (+0x10 work-area IBP, +0x28 cell).
    static void* s_fw = nullptr;
    if (!s_fw) {
        s_fw = ForesterWork_Factory();
        if (!s_fw) return true;
        fn_InternalBasePtr_Ctor(reinterpret_cast<uint8_t*>(s_fw) + 0x10, lodge);  // +0x10 work-area IBP
        int cell = ForceWork::WorkAreaCenterCell(lodge);
        if (cell >= 0) *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(s_fw) + 0x28) = cell;
        fprintf(stderr, "[ForceForester] constructed ForesterWork=%p work+0x10=lodge "
                "work+0x28(cell)=%d\n", s_fw, cell);
        fflush(stderr);
    }

    // Drive Execute only when the citizen's action stack is IDLE — ForesterWork::Execute is a
    // Work DRIVER, NOT an action-stack item: the building's ResourceProduceComponent calls its
    // Work-vtable slot DIRECTLY with the worker's IBP ((self, citizenIBP) ABI), and Execute then
    // pushes a PerformWork (cut) sequence onto the citizen's OWN action stack, which MoveTick's
    // executor runs. ForceWork::DriveWhenIdle mirrors that (skips while the stack is busy). We
    // own the citizen every tick so the AI state machine doesn't grab the idle stack first.
    static int s_driveCount = 0;
    ForceWork::DriveWhenIdle(citizen, s_fw, &ForesterWork_ForceDrive,
                             "ForceForester", &s_driveCount, 8);
    return true;
}

