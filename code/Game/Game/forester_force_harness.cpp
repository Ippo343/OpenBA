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

#ifdef STANDALONE_EXE

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <windows.h>   // VirtualQuery — crash-safe probes for the lodge component scan

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

// Entity component-IBP array layout (matches starttown.cpp ENT_COMP_ARR/CNT).
static constexpr int ENT_COMP_ARR = 0x30;
static constexpr int ENT_COMP_CNT = 0x38;

static bool ForceForesterEnvOn() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_FORCE_FORESTERWORK");
        s = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s == 1;
}

// ---- crash-safe memory probes (same policy as production_harness.cpp) ----
static bool Readable(const void* p, size_t n) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    DWORD prot = mbi.Protect;
    if (prot & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    const DWORD rd = PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                     PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
    if (!(prot & rd)) return false;
    uintptr_t end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return (reinterpret_cast<uintptr_t>(p) + n) <= end;
}
static bool Executable(const void* p) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    DWORD prot = mbi.Protect;
    if (prot & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    const DWORD ex = PAGE_EXECUTE | PAGE_EXECUTE_READ |
                     PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (prot & ex) != 0;
}
static inline uint32_t SafeGetTypeId(void* obj) {
    if (!Readable(obj, 8)) return 0;
    uintptr_t vt = *reinterpret_cast<uintptr_t*>(obj);
    if (!Readable(reinterpret_cast<void*>(vt + 0x10), 8)) return 0;
    void* slot = *reinterpret_cast<void**>(vt + 0x10);
    if (!Executable(slot)) return 0;
    typedef uint32_t (*gt_t)(void*);
    return reinterpret_cast<gt_t>(slot)(obj);
}

// Find a component by type-id on an entity's IBP component list (crash-safe).
static void* FindComp(void* entity, uint32_t tid) {
    if (!entity || !tid || !Readable(entity, 0x40)) return nullptr;
    int count = *reinterpret_cast<int*>((uintptr_t)entity + ENT_COMP_CNT);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>((uintptr_t)entity + ENT_COMP_ARR);
    if (count <= 0 || count > 512 || !Readable(reinterpret_cast<void*>(arr), (size_t)count * 8))
        return nullptr;
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* comp = *slot;
        if (SafeGetTypeId(comp) == tid) return comp;
    }
    return nullptr;
}

// Compute the lodge's work-area CENTER cell exactly as Work_Ctor25 does.
static int LodgeCenterCell(void* lodge) {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return -1;
    static uint32_t tidMap = 0;
    if (!tidMap) tidMap = ::Object::GetTypeIdByName("MapComponent");
    void* mc = FindComp(lodge, tidMap);
    if (!mc || !Readable(mc, 0x30)) return -1;
    uintptr_t game = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
    if (!game) return -1;
    int dim = *reinterpret_cast<int*>(game);
    if (dim <= 0) return -1;
    int16_t tileX = *reinterpret_cast<int16_t*>((uintptr_t)mc + 0x28);
    int16_t tileY = *reinterpret_cast<int16_t*>((uintptr_t)mc + 0x2a);
    int16_t fpW   = *reinterpret_cast<int16_t*>((uintptr_t)mc + 0x2c);
    int16_t fpH   = *reinterpret_cast<int16_t*>((uintptr_t)mc + 0x2e);
    int cell = (((int)fpH >> 1) + (int)tileY) * dim + (((int)fpW >> 1) + (int)tileX);
    if (cell < 0 || cell >= dim * dim) return -1;
    return cell;
}

// Dump the lodge component set + locate a usable ForesterComponent (once). Returns the
// ForesterComponent whose description derives from ForesterDescription, or null.
static void* VerifyLodgeForester(void* lodge, bool dump) {
    static uint32_t tidForesterComp = 0, tidForesterDesc = 0;
    if (!tidForesterComp) tidForesterComp = ::Object::GetTypeIdByName("ForesterComponent");
    if (!tidForesterDesc) tidForesterDesc = ::Object::GetTypeIdByName("ForesterDescription");

    if (dump) {
        int count = Readable(lodge, 0x40) ? *reinterpret_cast<int*>((uintptr_t)lodge + ENT_COMP_CNT) : 0;
        uintptr_t arr = Readable(lodge, 0x40) ? *reinterpret_cast<uintptr_t*>((uintptr_t)lodge + ENT_COMP_ARR) : 0;
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

    // CharacterComponent + action stack.
    uint8_t* cz = reinterpret_cast<uint8_t*>(citizen);
    void* charComp = *reinterpret_cast<void**>(cz + 0x278);
    if (!charComp || !Readable(charComp, 0x68)) return true;   // own it, but not ready yet
    uint8_t* chc = reinterpret_cast<uint8_t*>(charComp);

    // Construct the ForesterWork ONCE and keep it (it carries a per-work cell BST @+0x48 and a
    // fail counter @+0x58 that must survive across drives). Faithful to ForesterWork_CopyCtor:
    // ForesterWork_Factory does the object build (vtable + Object::Internal + +0x20/+0x28 = -1),
    // then we set the Work_Ctor25 work-area-dependent fields (+0x10 work-area IBP, +0x28 cell).
    static void* s_fw = nullptr;
    if (!s_fw) {
        s_fw = ForesterWork_Factory();
        if (!s_fw) return true;
        fn_InternalBasePtr_Ctor(reinterpret_cast<uint8_t*>(s_fw) + 0x10, lodge);  // +0x10 work-area IBP
        int cell = LodgeCenterCell(lodge);
        if (cell >= 0) *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(s_fw) + 0x28) = cell;
        fprintf(stderr, "[ForceForester] constructed ForesterWork=%p work+0x10=lodge "
                "work+0x28(cell)=%d\n", s_fw, cell);
        fflush(stderr);
    }

    // Only DRIVE when the citizen's action stack is IDLE (top-index +0x60 < 0): the prior
    // work sequence has completed. ForesterWork::Execute is a Work DRIVER, NOT an action-stack
    // item: the building's ResourceProduceComponent calls its Work-vtable slot DIRECTLY with the
    // worker's IBP — (self, citizenIBP) ABI, NOT the action executor's (node, stack) ABI — and
    // Execute then pushes a PerformWork (cut) sequence onto the citizen's OWN action stack.
    // (Pushing the ForesterWork onto the action stack instead makes the executor mis-call it
    // with the stack as param_2 -> garbage citizen deref.) So we mirror the building: call
    // Execute directly with the citizen IBP; the executor (MoveTick) then runs the pushed cut.
    if (*reinterpret_cast<int*>(chc + 0x60) >= 0) return true;

    int64_t citizenIBP = 0;
    fn_InternalBasePtr_Ctor(&citizenIBP, citizen);
    int rc = ForesterWork_ForceDrive(s_fw, &citizenIBP);

    static int s_driveCount = 0;
    if (s_driveCount < 8) { ++s_driveCount;
        fprintf(stderr, "[ForceForester] DRIVE ForesterWork::Execute#%d citizen=%p ret=%d "
                "newTop+0x60=%d\n", s_driveCount, citizen, rc, *reinterpret_cast<int*>(chc + 0x60));
        fflush(stderr);
    }
    return true;
}

#endif // STANDALONE_EXE
