// standalone/work_ser_probe.cpp
// BANISHED_WORK_SER_PROBE=1 — diagnostic for the save-leg crash inside
// Work::Serialize (0x1400f7400): Object::InternalBasePtr::Serialize faulted
// reading the target object's vtable (obj+0 held 0x0000148200000000), i.e. the
// Work's task-target IBP pointed at freed/recycled memory.
//
// IBP is a weak ref and the shipping Runtime's CheckPtr is an empty function,
// so nothing nulls it — retail must be removing the Work when its target dies.
// This probe answers WHICH mechanism is missing:
//   * is the dangling target a ComponentEntity we destroyed (dtor ring hit)?
//   * how many Works in the goals tree carry one?
//   * which Work subclass (log its vtable, symbolize offline)?
//
// Diagnostic only, fully env-gated: when the probe is ON it also nulls the bad
// field so the save completes and we get the whole census plus the load leg in
// one run. With the probe OFF nothing changes — the faithful deref stands.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>

#include "engine_funcs.h"   // g_pGameWorldSingleton

namespace {

bool ProbeOn() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_WORK_SER_PROBE");
        s = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s != 0;
}

// Ring of recently destroyed ComponentEntity addresses (dtor order).
constexpr int kRing = 4096;
void*    s_dead[kRing];
uint64_t s_deadSeq[kRing];
uint64_t s_dtorCount = 0;

// A readable, committed address?
bool Readable(const void* p, size_t n) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    const DWORD bad = PAGE_NOACCESS | PAGE_GUARD;
    if (mbi.Protect & bad) return false;
    const uint8_t* end = static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    return static_cast<const uint8_t*>(p) + n <= end;
}

// A plausible vtable pointer: readable AND inside a mapped image (.rdata of a
// module), since every vtable we care about lives in reimpl-x64.exe or a DLL.
bool LooksLikeVtable(const void* v) {
    if (!Readable(v, 8)) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(v, &mbi, sizeof(mbi))) return false;
    return mbi.Type == MEM_IMAGE;
}

const char* ModuleOf(const void* p, uintptr_t* offOut) {
    static char name[MAX_PATH];
    MEMORY_BASIC_INFORMATION mbi;
    if (!p || !VirtualQuery(p, &mbi, sizeof(mbi))) return nullptr;
    if (mbi.Type != MEM_IMAGE) return nullptr;
    HMODULE mod = reinterpret_cast<HMODULE>(mbi.AllocationBase);
    if (!GetModuleFileNameA(mod, name, sizeof(name))) return nullptr;
    const char* slash = strrchr(name, 92);
    if (offOut) *offOut = reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(mod);
    return slash ? slash + 1 : name;
}

} // namespace

// ---------------------------------------------------------------------------
// Catch the dangling ref at the moment it is CREATED, not at save time.
//
// The goals/work manager is *(GameWorld+0x118). Its work trees are the 15-slot
// {root,count} array at +0x30 (stride 0x10) that Goals::Serialize hands to
// 0x14002d060, plus the tree at +0x120. Node layout (from d060's walkers):
//   +0x00 int key (work id)   +0x08 IBP payload (the Work)
//   +0x10 parent  +0x18 left  +0x20 right
// A Work's task target is the IBP at work+0x10. So when an entity dies, any
// Work still holding it is the dangling ref the save leg will later fault on —
// and the dtor is where we can still name the entity.
// ---------------------------------------------------------------------------
namespace {

uint8_t* BstLeftmost(uint8_t* n) {
    if (!n) return nullptr;
    while (Readable(n + 0x18, 8)) {
        uint8_t* l = *reinterpret_cast<uint8_t**>(n + 0x18);
        if (!l) break;
        n = l;
    }
    return n;
}

uint8_t* BstNext(uint8_t* n) {
    if (!n || !Readable(n, 0x28)) return nullptr;
    uint8_t* r = *reinterpret_cast<uint8_t**>(n + 0x20);
    if (r) return BstLeftmost(r);
    uint8_t* p = *reinterpret_cast<uint8_t**>(n + 0x10);
    while (p && Readable(p, 0x28) && *reinterpret_cast<uint8_t**>(p + 0x20) == n) {
        n = p;
        p = *reinterpret_cast<uint8_t**>(p + 0x10);
    }
    return p;
}

// Who is destroying the entity? The dtor is a shared funnel, so the identity of
// the caller chain is the whole question: a Pickup target should only die as a
// CONSEQUENCE of the Pickup completing (which removes the work first). Print the
// return-address chain as module+offset; symbolize offline with nm.
void PrintDeathStack() {
    void* frames[24];
    USHORT n = CaptureStackBackTrace(1, 24, frames, nullptr);
    fprintf(stderr, "[WorkSerProbe]   death stack:");
    for (USHORT i = 0; i < n; ++i) {
        uintptr_t off = 0;
        const char* mod = ModuleOf(frames[i], &off);
        if (!mod) continue;
        fprintf(stderr, " %s+0x%llX", mod, (unsigned long long)off);
    }
    fprintf(stderr, "\n");
}

void ScanTreeForTarget(uint8_t* root, void* dying, int slot, int* hits, int* seen) {
    int guard = 0;
    for (uint8_t* n = BstLeftmost(root); n && guard < 100000; n = BstNext(n), ++guard) {
        if (!Readable(n, 0x28)) break;
        ++*seen;
        uint8_t* work = *reinterpret_cast<uint8_t**>(n + 0x08);
        if (!work || !Readable(work, 0x48)) continue;
        if (*reinterpret_cast<void**>(work + 0x10) != dying &&
            *reinterpret_cast<void**>(work + 0x18) != dying) continue;
        const char* which =
            (*reinterpret_cast<void**>(work + 0x10) == dying) ? "+0x10" : "+0x18";
        uintptr_t off = 0;
        const void* wvt = *reinterpret_cast<void* const*>(work);
        const char* mod = ModuleOf(wvt, &off);
        fprintf(stderr,
                "[WorkSerProbe] DANGLING-CREATED entity=%p still referenced by work=%p "
                "(%s) workVt=%s+0x%llX slot=%d nodeKey=%d taskId=%d/%d\n",
                dying, work, which, mod ? mod : "?", (unsigned long long)off,
                slot, *reinterpret_cast<int*>(n),
                *reinterpret_cast<int*>(work + 0x20),
                *reinterpret_cast<int*>(work + 0x28));
        PrintDeathStack();
        ++*hits;
    }
}

} // namespace

// Called from ComponentEntity::~ComponentEntity (code/Game/Entity/componententity.cpp).
extern "C" void WorkSerProbe_NoteEntityDtor(void* entity) {
    if (!ProbeOn()) return;
    const int i = static_cast<int>(s_dtorCount % kRing);
    s_dead[i]    = entity;
    s_deadSeq[i] = s_dtorCount;
    ++s_dtorCount;

    // Scan the goals trees for Works still pointing at this entity.
    static int s_reported = 0;
    if (s_reported >= 40) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    if (!Readable(gw + 0x118, 8)) return;
    uint8_t* goals = *reinterpret_cast<uint8_t**>(gw + 0x118);
    if (!goals || !Readable(goals, 0x128)) return;

    int hits = 0, seen = 0;
    for (int slot = 0; slot < 15; ++slot)
        ScanTreeForTarget(*reinterpret_cast<uint8_t**>(goals + 0x30 + slot * 0x10),
                          entity, slot, &hits, &seen);
    ScanTreeForTarget(*reinterpret_cast<uint8_t**>(goals + 0x120), entity, 99, &hits, &seen);
    if (hits) { s_reported += hits; fflush(stderr); }

    // Positive control: a silent probe must not be mistaken for a clean world.
    // Print what the scan actually looked at every 64 entity deaths, so "no
    // DANGLING-CREATED lines" is readable as "N works examined, none stale"
    // rather than "the walk found nothing to examine".
    if ((s_dtorCount % 64) == 0) {
        fprintf(stderr, "[WorkSerProbe] scan alive: entityDtors=%llu goals=%p "
                        "worksExamined=%d danglingThisDtor=%d\n",
                (unsigned long long)s_dtorCount, (void*)goals, seen, hits);
        fflush(stderr);
    }
}

// Returns true if the field held a dangling pointer (and, when the probe is on,
// has been nulled so the save can continue).
extern "C" int WorkSerProbe_Check(void* work, void** field, const char* which) {
    if (!ProbeOn()) return 0;
    void* target = *field;
    if (!target) return 0;

    const bool memOk = Readable(target, 0x20);
    const void* vt   = memOk ? *reinterpret_cast<void* const*>(target) : nullptr;
    if (memOk && LooksLikeVtable(vt)) return 0;   // healthy

    long long deadSeq = -1;
    for (int i = 0; i < kRing; ++i)
        if (s_dead[i] == target) { deadSeq = static_cast<long long>(s_deadSeq[i]); break; }

    uintptr_t wvOff = 0;
    const void* workVt = Readable(work, 8) ? *reinterpret_cast<void* const*>(work) : nullptr;
    const char* wmod = ModuleOf(workVt, &wvOff);

    static int s_n = 0;
    if (++s_n <= 40) {
        fprintf(stderr,
                "[WorkSerProbe] STALE %s work=%p workVt=%s+0x%llX target=%p "
                "readable=%d vt=%p deadDtorSeq=%lld (dtorsSoFar=%llu) "
                "taskId=%d/%d\n",
                which, work, wmod ? wmod : "?", (unsigned long long)wvOff,
                target, memOk ? 1 : 0, vt, deadSeq,
                (unsigned long long)s_dtorCount,
                Readable(work, 0x30) ? *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(work) + 0x20) : -1,
                Readable(work, 0x30) ? *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(work) + 0x28) : -1);
        fflush(stderr);
    }

    *field = nullptr;   // diagnostic-only: let the save finish so we see the census
    return 1;
}

// One-shot cross-check of the tree base this probe walks. Goals::Serialize hands
// 0x14002d060 the real {root,count} array; if that pointer is not
// *(GameWorld+0x118)+0x30 then the dtor-time scan is walking the wrong memory and
// its silence means nothing.
extern "C" void WorkSerProbe_NoteGoalsArray(void* arrayBase, int slotCount) {
    if (!ProbeOn()) return;
    static bool s_once = false;
    if (s_once) return;
    s_once = true;
    void* goals = nullptr;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
        if (Readable(gw + 0x118, 8)) goals = *reinterpret_cast<void**>(gw + 0x118);
    }
    void* expect = goals ? reinterpret_cast<uint8_t*>(goals) + 0x30 : nullptr;
    int total = 0;
    if (Readable(arrayBase, static_cast<size_t>(slotCount) * 0x10)) {
        const int64_t* slot = reinterpret_cast<const int64_t*>(arrayBase);
        for (int i = 0; i < slotCount; ++i, slot += 2)
            total += static_cast<int>(slot[1]);
    }
    fprintf(stderr, "[WorkSerProbe] goals array: d060 base=%p expected(goals+0x30)=%p "
                    "%s slots=%d worksInTrees=%d\n",
            arrayBase, expect, (arrayBase == expect) ? "MATCH" : "MISMATCH",
            slotCount, total);
    fflush(stderr);
}
