// code/Game/Object/internal_factory_audit.cpp
// See internal_factory_audit.h for the table layout and the two corruption modes.

#include "Object/internal_factory_audit.h"
#include "Game/force_work.h"
#include "resolve.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// Object::InternalFactory::_singleton — Runtime-x64.dll global holding the
// InternalFactory*. (Ghidra: ?_singleton@InternalFactory@Object@@0PEAV12@EA)
constexpr uintptr_t kInternalFactorySingleton = 0x1801fc6d8;

constexpr int kIdxOff = 0x08;   // Internal::_factoryIndex
constexpr int kGenOff = 0x0c;   // Internal::_generation

bool EnvOn(const char* name) {
    const char* v = std::getenv(name);
    return v && v[0] && std::strcmp(v, "0") != 0 && std::strcmp(v, "false") != 0;
}

// Per-vtable tally of "freed while still registered" objects.
struct LeakBucket {
    const void* vtable;
    const char* what;
    uint64_t    count;
};
constexpr int kMaxBuckets = 32;
LeakBucket g_leaks[kMaxBuckets];
int        g_leakBuckets = 0;
uint64_t   g_leakTotal   = 0;

}  // namespace

namespace ObjAudit {

bool Enabled() {
    static int cached = -1;
    if (cached < 0) cached = EnvOn("BANISHED_OBJAUDIT") ? 1 : 0;
    return cached != 0;
}

void* Factory() {
    // The singleton is created once at engine init and never moves, so the
    // VirtualQuery probes only run until the first successful resolve. This keeps
    // IsRegistered() cheap enough to call from a free path.
    static void* s_cached = nullptr;
    if (s_cached) return s_cached;

    uintptr_t slot = ResolveAddr(Module::Runtime, kInternalFactorySingleton);
    if (!slot || !ForceWork::Readable(reinterpret_cast<void*>(slot), 8)) return nullptr;
    void* f = *reinterpret_cast<void**>(slot);
    // factory header is {ptr@+0, count@+8, cap@+0xc} plus a Random at +0x20.
    if (!f || !ForceWork::Readable(f, 0x24)) return nullptr;
    s_cached = f;
    return f;
}

// Fetch table base + count, sanity-checked. Returns false if unusable.
static bool Table(void*** outTable, int* outCount) {
    void* f = Factory();
    if (!f) return false;
    uint8_t* fb = reinterpret_cast<uint8_t*>(f);
    void** tbl  = *reinterpret_cast<void***>(fb);
    int    cnt  = *reinterpret_cast<int*>(fb + 8);
    if (cnt < 0 || cnt > (1 << 24)) return false;              // absurd → don't walk it
    if (cnt > 0 && !ForceWork::Readable(tbl, (size_t)cnt * 8)) return false;
    *outTable = tbl;
    *outCount = cnt;
    return true;
}

int Count() {
    void** tbl = nullptr; int cnt = 0;
    if (!Table(&tbl, &cnt)) return -1;
    return cnt;
}

// Cheap variant used on the free path: assumes `obj` is a live, readable object
// (the caller is about to free it, so it must be), and only touches the factory
// header + one table slot. No VirtualQuery once the factory is cached.
bool IsRegistered(const void* obj) {
    if (!obj) return false;
    void* f = Factory();
    if (!f) return false;
    uint8_t* fb = reinterpret_cast<uint8_t*>(f);
    void** tbl  = *reinterpret_cast<void***>(fb);
    int    cnt  = *reinterpret_cast<int*>(fb + 8);
    if (!tbl || cnt <= 0 || cnt > (1 << 24)) return false;
    int idx = *reinterpret_cast<const int*>(reinterpret_cast<const uint8_t*>(obj) + kIdxOff);
    if (idx < 0 || idx >= cnt) return false;
    return tbl[idx] == obj;
}

// Last-known-GOOD identity per table slot. A stale entry's pointer never changes
// (nothing owns its index any more, so no swap-remove ever overwrites the slot),
// but its *memory* gets recycled — so by the time a scan sees it, the vtable is
// already garbage. Remembering the vtable from when the slot was still consistent
// is what actually names the leaked type.
struct SlotSnap { void* ptr; void* vtable; };
SlotSnap* g_snap    = nullptr;
int       g_snapCap = 0;
// Slots already reported, so each leak is announced once rather than every scan.
bool*     g_reported = nullptr;

// ForceWork::Readable is a VirtualQuery per call — 50k of those per scan is far too
// slow to run often. Heap objects cluster heavily, so caching the last accepted
// 64KB region (the Windows allocation granularity) collapses almost all of them.
static bool ReadableCached(const void* p, size_t n) {
    static uintptr_t s_okRegion = 0;
    uintptr_t region = reinterpret_cast<uintptr_t>(p) & ~(uintptr_t)0xFFFF;
    if (region != 0 && region == s_okRegion) return true;
    if (!ForceWork::Readable(p, n)) return false;
    s_okRegion = region;
    return true;
}

static void EnsureSnap(int cnt) {
    if (cnt <= g_snapCap) return;
    int newCap = cnt + 4096;
    g_snap = static_cast<SlotSnap*>(std::realloc(g_snap, (size_t)newCap * sizeof(SlotSnap)));
    g_reported = static_cast<bool*>(std::realloc(g_reported, (size_t)newCap));
    if (!g_snap || !g_reported) { g_snapCap = 0; return; }
    std::memset(g_snap + g_snapCap, 0, (size_t)(newCap - g_snapCap) * sizeof(SlotSnap));
    std::memset(g_reported + g_snapCap, 0, (size_t)(newCap - g_snapCap));
    g_snapCap = newCap;
}

int Scan(const char* tag, int maxLog) {
    void** tbl = nullptr; int cnt = 0;
    if (!Table(&tbl, &cnt)) {
        std::fprintf(stderr, "[ObjAudit] %s: factory unreachable\n", tag);
        std::fflush(stderr);
        return -1;
    }
    EnsureSnap(cnt);

    int bad = 0, logged = 0;
    for (int i = 0; i < cnt; ++i) {
        void* p = tbl[i];
        const char* why = nullptr;
        int selfIdx = -1;

        if (!p) {
            why = "NULL entry";
        } else if (!ReadableCached(p, 0x10)) {
            why = "UNREADABLE (freed/unmapped)";
        } else {
            selfIdx = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(p) + kIdxOff);
            if (selfIdx != i) why = "self-index mismatch";
        }

        if (!why) {
            // Consistent — remember who lives here, so a later failure can be named.
            if (i < g_snapCap) g_snap[i] = SlotSnap{p, *reinterpret_cast<void**>(p)};
            continue;
        }

        ++bad;
        // Announce each newly-bad slot exactly once, with the identity it had while
        // it was still healthy — that vtable is the one to symbolize with nm.
        if (i < g_snapCap && !g_reported[i]) {
            g_reported[i] = true;
            std::fprintf(stderr,
                "[ObjAudit] %s: table[%d] WENT BAD (%s) ptr=%p selfIdx=%d nowVtable=%p "
                "| WAS ptr=%p vtable=%p%s\n",
                tag, i, why, p, selfIdx,
                (p && ForceWork::Readable(p, 8)) ? *reinterpret_cast<void**>(p) : nullptr,
                g_snap[i].ptr, g_snap[i].vtable,
                (g_snap[i].ptr == p) ? "  <-- SAME PTR: stale entry, block recycled" : "");
            std::fflush(stderr);
        }
        if (logged < maxLog) {
            ++logged;
            void* vt = (p && ForceWork::Readable(p, 8)) ? *reinterpret_cast<void**>(p) : nullptr;
            std::fprintf(stderr,
                "[ObjAudit] %s: table[%d]=%p BAD (%s) selfIdx=%d vtable=%p (was vtable=%p)\n",
                tag, i, p, why, selfIdx, vt, i < g_snapCap ? g_snap[i].vtable : nullptr);
        }
    }

    std::fprintf(stderr, "[ObjAudit] %s: count=%d bad=%d (freed-while-registered so far: %llu)\n",
                 tag, cnt, bad, (unsigned long long)g_leakTotal);
    std::fflush(stderr);
    return bad;
}

void NoteUnregisteredFree(const void* obj, const char* what) {
    if (!Enabled() || !obj) return;
    if (!IsRegistered(obj)) return;      // properly unregistered (or never registered) — fine

    ++g_leakTotal;
    const void* vt = ForceWork::Readable(obj, 8) ? *reinterpret_cast<void* const*>(obj) : nullptr;

    for (int i = 0; i < g_leakBuckets; ++i) {
        if (g_leaks[i].vtable == vt && g_leaks[i].what == what) { ++g_leaks[i].count; return; }
    }
    if (g_leakBuckets < kMaxBuckets) {
        g_leaks[g_leakBuckets++] = LeakBucket{vt, what, 1};
        // First sighting of this type — log it with the factory index so the
        // stale entry can be correlated with a later Scan().
        int idx = *reinterpret_cast<const int*>(reinterpret_cast<const uint8_t*>(obj) + kIdxOff);
        uint32_t gen = *reinterpret_cast<const uint32_t*>(
            reinterpret_cast<const uint8_t*>(obj) + kGenOff);
        std::fprintf(stderr,
            "[ObjAudit] LEAK(first): %s obj=%p vtable=%p factIdx=%d gen=0x%08x "
            "— freed while STILL in the InternalFactory table\n",
            what, obj, vt, idx, gen);
        std::fflush(stderr);
    }
}

void ReportLeaks(const char* tag) {
    if (!Enabled()) return;
    std::fprintf(stderr, "[ObjAudit] %s: freed-while-registered total=%llu across %d type(s)\n",
                 tag, (unsigned long long)g_leakTotal, g_leakBuckets);
    for (int i = 0; i < g_leakBuckets; ++i) {
        std::fprintf(stderr, "[ObjAudit]   %-24s vtable=%p  x%llu\n",
                     g_leaks[i].what, g_leaks[i].vtable,
                     (unsigned long long)g_leaks[i].count);
    }
    std::fflush(stderr);
}

}  // namespace ObjAudit
