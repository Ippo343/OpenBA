// code/Game/Game/production_harness.cpp
// Phase 3 economy WORK-half — Phase B: prove that WORK PRODUCES a resource.
//
// The full faithful producer chain (profession BST @ townMgr+0x100 -> per-profession
// Work driver -> ForesterWork::Slot4 finds a tree, pushes a cut PerformWork action ->
// citizen chops -> the harvested NaturalResource pushes a CreateRawMaterial action ->
// CreateRawMaterial::AbstractImpl (0x1400c6b40, ported in code/Game/Action/createrawmaterial.cpp)
// spawns log entities + calls RegisterRawMaterial -> the town stock count increments) is a
// multi-layer subsystem still gated behind the inert profession/work-management registry.
//
// What IS already ported + registered: the production-ACCOUNTING tail. The exact count
// increment the producer makes is RegisterRawMaterial (0x140088ef0, ActRm_RegisterRawMaterial_88ef0
// in code/Game/Action/action_base_rawmat.cpp): it classifies the produced material's
// RawMaterialDescription category (GetResourceCategory 0x140088f90 — reads the description's
// component list at +0x28/+0x30 for a RawMaterialDescription (DAT_1401c7cb8) and maps its
// category-flag word at comp+0x5c to a 0..0x14 index) and does
//     *(int*)( *(void**)(townMgr+0x170) + catIdx*0x20 + 0x10 ) += amount.
// (townMgr = *(void**)(gameWorld+0xf0); per-category record stride 0x20; the forester's own
// capacity check in ForesterWork::Slot4 reads the SAME record's +0x00 (count) / +0x08 (cap).)
//
// This harness (env BANISHED_LIVE_PRODUCTION, default OFF) drives exactly that real, ported
// count-increment once for the force-worked ForesterLodge: it locates a real
// RawMaterialDescription reachable from the lodge's own component descriptions (the lodge's
// produce material) and calls the REAL RegisterRawMaterial with it, then dumps the town stock
// record before/after to PROVE the count increments — crash-free. It exercises the genuine
// production-accounting path (GetResourceCategory -> ArrayUpsert/bucket write) with the real
// building as owner; it is NOT a fabricated counter.
//
// Default build stays byte-clean: with the env unset the one-shot returns immediately.

#ifdef STANDALONE_EXE

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <windows.h>   // VirtualQuery — crash-safe readability probe for the exploratory scan

// The force-worked ForesterLodge (starttown.cpp, BANISHED_LIVE_WORKPLACE).
extern void* g_pForceWorkBuilding;
// First resolved natural-resource (tree) template — a produced-material description
// container (sub-desc array @+0x28 / count @+0x30). Captured in StartTown_SpawnResources.
extern void* g_pSampleResourceTemplate;

// Real ported production-accounting fns (code/Game/Action/action_base_rawmat.cpp).
extern void ActRm_RegisterRawMaterial_88ef0(int64_t entityList, void* sourceIBP,
                                            char placeNear, int64_t descEntity, int quantity);
extern int  ActRm_GetResourceCategory_88f90(void* unused, uintptr_t entity);

// Entity component-IBP array layout (matches starttown.cpp ENT_COMP_ARR/CNT).
static constexpr int ENT_COMP_ARR = 0x30;
static constexpr int ENT_COMP_CNT = 0x38;

static bool ProductionEnvOn() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_LIVE_PRODUCTION");
        s = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s == 1;
}

// Crash-safe readability probe: is [p, p+n) committed + readable memory? Used so the
// EXPLORATORY scan (which walks description layouts we have not yet proven) can never
// fault on a field that turns out to hold packed ints rather than a pointer.
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

// Crash-safe EXECUTABILITY probe: is `p` committed CODE memory? The exploratory scan
// treats arbitrary fields as objects and reads obj->vtable->slot[+0x10]; for a real object
// that slot points into a code section, but for a non-object field it points at readable
// HEAP data. CALLING such a slot faults with EXEC (0xC0000005) — Readable() alone is NOT
// enough (heap is readable but not executable). Require PAGE_EXECUTE_* before the indirect
// call so a non-object field is rejected instead of executing garbage.
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

// Read a 32-bit GetTypeId() via vtable slot +0x10 on a fully validated object. The vtable
// pointer must be readable AND the slot it names must be executable code, else return 0 —
// so the exploratory scan can never EXEC a heap field that merely looked like a vtable.
static inline uint32_t SafeGetTypeId(void* obj) {
    if (!Readable(obj, 8)) return 0;
    uintptr_t vt = *reinterpret_cast<uintptr_t*>(obj);
    if (!Readable(reinterpret_cast<void*>(vt + 0x10), 8)) return 0;
    void* slot = *reinterpret_cast<void**>(vt + 0x10);
    if (!Executable(slot)) return 0;
    typedef uint32_t (*gt_t)(void*);
    return reinterpret_cast<gt_t>(slot)(obj);
}

// Guarded scan of a DESCRIPTION object's component list (desc+0x28 array / desc+0x30 count,
// the layout GetResourceCategory/FindRawMaterialDesc use) for a RawMaterialDescription
// (tid_rawmat). Returns true if present. Every deref is readability-checked, so a field that
// is NOT actually an {array,count} pair (e.g. packed int16s) is rejected instead of faulting.
static bool DescHasRawMaterial(void* desc, uint32_t tid_rawmat) {
    if (!tid_rawmat || !Readable(desc, 0x38)) return false;
    int count = *reinterpret_cast<int*>((uintptr_t)desc + 0x30);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>((uintptr_t)desc + 0x28);
    if (count <= 0 || count > 512 || !Readable(reinterpret_cast<void*>(arr), (size_t)count * 8))
        return false;
    for (int i = 0; i < count; ++i) {
        void* c = *reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        if (SafeGetTypeId(c) == tid_rawmat) return true;
    }
    return false;
}

// Read the three observed fields of a town stock record for category `cat`.
//   record = *(townMgr+0x170) + cat*0x20 ; fields +0x00, +0x08, +0x10.
static void DumpStock(uintptr_t townMgr, int cat, const char* tag) {
    uintptr_t table = *reinterpret_cast<uintptr_t*>(townMgr + 0x170);
    if (!table || cat < 0) { fprintf(stderr, "[Production]   stock[%d] %s (no table/cat)\n", cat, tag); return; }
    uintptr_t rec = table + (uintptr_t)cat * 0x20;
    fprintf(stderr, "[Production]   stock[cat=%d] %s  +0x00=%d  +0x08=%d  +0x10=%d\n",
            cat, tag,
            *reinterpret_cast<int*>(rec + 0x00),
            *reinterpret_cast<int*>(rec + 0x08),
            *reinterpret_cast<int*>(rec + 0x10));
    fflush(stderr);
}

// One-shot, fires once per process once the world + force-work building are live.
extern "C" void StartTown_DriveProduction() {
    if (!ProductionEnvOn()) return;
    static int s_done = 0;
    if (s_done) return;

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    void* lodge = g_pForceWorkBuilding;
    if (!lodge) return;

    uintptr_t gameWorld = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(gameWorld + 0xf0);
    if (!townMgr) return;
    uintptr_t resTable = *reinterpret_cast<uintptr_t*>(townMgr + 0x170);
    if (!resTable) return;

    s_done = 1;   // commit: we have a live town manager + lodge

    const uint32_t tid_rawmat = ::Object::GetTypeIdByName("RawMaterialDescription");

    fprintf(stderr, "[Production] === Phase B: drive ForesterLodge production ===\n");
    fprintf(stderr, "[Production] lodge=%p townMgr=%p resTable=%p tid(RawMaterialDescription)=%u\n",
            lodge, (void*)townMgr, (void*)resTable, tid_rawmat);
    fflush(stderr);

    // ---- Locate a produce RawMaterialDescription reachable from the lodge. ----
    // Scan the lodge's components; for each, its description (comp+0x18) is the candidate
    // produce-description container. The first whose +0x28/+0x30 list carries a
    // RawMaterialDescription is the produced material (faithful: the same description the
    // producer's CreateRawMaterial action carries at action+0x20).
    uint8_t* e = reinterpret_cast<uint8_t*>(lodge);
    int compCount = *reinterpret_cast<int*>(e + ENT_COMP_CNT);
    uintptr_t compArr = *reinterpret_cast<uintptr_t*>(e + ENT_COMP_ARR);
    if (compCount <= 0 || compCount > 512 || !compArr) {
        fprintf(stderr, "[Production] BAIL: bad lodge component list count=%d arr=%p\n",
                compCount, (void*)compArr);
        fflush(stderr);
        return;
    }

    void* descEntity = nullptr;
    for (int i = 0; i < compCount; ++i) {
        void** slot = reinterpret_cast<void**>(compArr + (uintptr_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* comp = *slot;
        if (!comp || !Readable(comp, 0x20)) continue;
        uint32_t compTid = SafeGetTypeId(comp);
        void* desc = *reinterpret_cast<void**>((uintptr_t)comp + 0x18);
        uint32_t descTid = SafeGetTypeId(desc);
        // Candidate produce-description containers (the layout GetResourceCategory wants —
        // a {array@+0x28, count@+0x30} list holding a RawMaterialDescription), in the order
        // ForesterWork::Slot4 reaches the forester's produced material:
        //   (1) comp itself, (2) the component's description (comp+0x18), (3) the
        //   description's matDescRef field (desc+0x20) — radiusComp+0x20 in Slot4, the
        //   produced-material description ref.
        void* matRef = Readable(desc, 0x28) ? *reinterpret_cast<void**>((uintptr_t)desc + 0x20) : nullptr;
        bool hasRaw     = DescHasRawMaterial(desc, tid_rawmat);
        bool hasRawSelf = DescHasRawMaterial(comp, tid_rawmat);
        bool hasRawMat  = DescHasRawMaterial(matRef, tid_rawmat);
        if (hasRaw || hasRawSelf || hasRawMat) {
            fprintf(stderr, "[Production]   comp[%d] tid=%u desc=%p descTid=%u matRef=%p "
                    "hasRaw(desc=%d self=%d matRef=%d) <== PRODUCE DESC\n",
                    i, compTid, desc, descTid, matRef,
                    (int)hasRaw, (int)hasRawSelf, (int)hasRawMat);
            if (!descEntity) descEntity = hasRaw ? desc : (hasRawSelf ? comp : matRef);
        } else {
            fprintf(stderr, "[Production]   comp[%d] tid=%u desc=%p descTid=%u matRef=%p\n",
                    i, compTid, desc, descTid, matRef);
        }
    }
    fflush(stderr);

    if (!descEntity) {
        // The lodge carries no reachable produce description (faithful: a forester's logs
        // come from harvested TREES, not the lodge). Use the captured natural-resource
        // (tree) TEMPLATE — a real produced-material description container ({sub-desc array
        // @+0x28, count @+0x30}) carrying the RawMaterialDescription the producer clones.
        void* res = g_pSampleResourceTemplate;
        bool resHasRaw = DescHasRawMaterial(res, tid_rawmat);
        fprintf(stderr, "[Production] lodge has no direct produce desc; sampleResourceTemplate=%p hasRaw=%d\n",
                res, (int)resHasRaw);
        // Probe each of the resource template's sub-descriptions for a RawMaterialDescription
        // container at sub-desc itself and at sub-desc+0x20 (the NaturalResourceDescription's
        // produced-material ref), so we can report exactly where the produce desc lives.
        if (Readable(res, 0x38)) {
            int rc = *reinterpret_cast<int*>((uintptr_t)res + 0x30);
            uintptr_t ra = *reinterpret_cast<uintptr_t*>((uintptr_t)res + 0x28);
            if (rc > 0 && rc <= 512 && Readable(reinterpret_cast<void*>(ra), (size_t)rc * 8)) {
                for (int i = 0; i < rc; ++i) {
                    void* sub = *reinterpret_cast<void**>(ra + (uintptr_t)i * 8);
                    uint32_t subTid = SafeGetTypeId(sub);
                    void* sref = Readable(sub, 0x28) ? *reinterpret_cast<void**>((uintptr_t)sub + 0x20) : nullptr;
                    bool sSelf = DescHasRawMaterial(sub, tid_rawmat);
                    bool sRef  = DescHasRawMaterial(sref, tid_rawmat);
                    fprintf(stderr, "[Production]   res.sub[%d] tid=%u self=%d ref=%p refHas=%d\n",
                            i, subTid, (int)sSelf, sref, (int)sRef);
                    if (!descEntity && sSelf) descEntity = sub;
                    else if (!descEntity && sRef) descEntity = sref;
                }
            }
        }
        if (!descEntity && resHasRaw) descEntity = res;
        fflush(stderr);
    }

    if (!descEntity) {
        fprintf(stderr, "[Production] NO produce RawMaterialDescription reachable from the lodge "
                "or the sampled resource template — next domino = the produce desc lives deeper "
                "(workplace MultiObject sub-object / the log raw-material template referenced by "
                "the NaturalResourceDescription). Reporting; no increment attempted.\n");
        fflush(stderr);
        return;
    }

    // ---- Classify + drive the REAL ported count-increment. ----
    int cat = ActRm_GetResourceCategory_88f90(reinterpret_cast<void*>(townMgr),
                                              reinterpret_cast<uintptr_t>(descEntity));
    fprintf(stderr, "[Production] descEntity=%p -> GetResourceCategory=%d\n", descEntity, cat);
    if (cat < 0) {
        fprintf(stderr, "[Production] category < 0 (no category bit at desc RawMaterial+0x5c) — "
                "cannot account; reporting.\n");
        fflush(stderr);
        return;
    }

    DumpStock(townMgr, cat, "BEFORE");

    // Owner IBP = the lodge (faithful: producer passes &ownerIBP). placeNear=0 so the
    // per-owner ArrayUpsert branch is skipped and the global town bucket is incremented.
    int64_t lodgeIBP = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&lodgeIBP, lodge);
    const int amount = 10;
    fprintf(stderr, "[Production] RegisterRawMaterial(townMgr, &lodgeIBP, placeNear=0, descEntity, amount=%d)\n", amount);
    fflush(stderr);

    ActRm_RegisterRawMaterial_88ef0(static_cast<int64_t>(townMgr), &lodgeIBP, 0,
                                    reinterpret_cast<int64_t>(descEntity), amount);

    DumpStock(townMgr, cat, "AFTER ");
    fprintf(stderr, "[Production] === done (expect AFTER +0x10 == BEFORE +0x10 + %d) ===\n", amount);
    fflush(stderr);
}

#endif // STANDALONE_EXE
