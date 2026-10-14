// src/diag/diag_threads.cpp -- see diag_threads.h (NOT RETAIL; block LOADSTAMP).
#include "diag_threads.h"

#include <atomic>
#include <cstring>

#include "platform_thread.h"   // Platform::CurrentThreadId

namespace DiagThreads {

namespace {

struct Slot {
    std::atomic<uint32_t> tid;   // published last; 0 = not yet written
    std::atomic<char>     role;
    std::atomic<const volatile uint32_t*> busy;
};

Slot g_slots[kMaxThreads];           // zero-initialised (constant init)
std::atomic<int> g_count{0};
thread_local bool t_registered = false;

int FindSlot(uint32_t tid) {
    const int n = g_count.load(std::memory_order_acquire);
    const int lim = n < kMaxThreads ? n : kMaxThreads;
    for (int i = 0; i < lim; ++i)
        if (g_slots[i].tid.load(std::memory_order_acquire) == tid) return i;
    return -1;
}

}  // namespace

void Register(char role, const volatile uint32_t* busy) {
    if (t_registered) return;
    t_registered = true;
    const uint32_t tid = Platform::CurrentThreadId();
    // The OS reuses ids: a dead thread's slot is taken over by the new owner.
    const int existing = FindSlot(tid);
    if (existing >= 0) {
        g_slots[existing].busy.store(busy, std::memory_order_release);
        g_slots[existing].role.store(role, std::memory_order_release);
        return;
    }
    const int i = g_count.fetch_add(1, std::memory_order_acq_rel);
    if (i >= kMaxThreads) return;
    g_slots[i].role.store(role, std::memory_order_relaxed);
    g_slots[i].busy.store(busy, std::memory_order_relaxed);
    g_slots[i].tid.store(tid, std::memory_order_release);
}

void RegisterNamed(const char* typeName, const volatile uint32_t* busy) {
    if (t_registered) return;
    const char* n = typeName ? typeName : "";
    char role = kOther;
    if      (std::strstr(n, "PathfinderThread"))  role = kPathfinder;
    else if (std::strstr(n, "PoolThread"))        role = kPathWorker;
    else if (std::strstr(n, "RenderThread"))      role = kRender;
    else if (std::strstr(n, "PersistenceWorker")) role = kPersist;
    else if (std::strstr(n, "Stream"))            role = kAudio;    // Audio::Stream
    else if (std::strstr(n, "Loader"))            role = kLoader;   // Engine::Loader
    Register(role, busy);
}

char RoleOf(uint32_t tid) {
    const int i = FindSlot(tid);
    return i >= 0 ? g_slots[i].role.load(std::memory_order_acquire) : 0;
}

const char* RoleName(char role) {
    switch (role) {
    case kMain:       return "main";
    case kLoader:     return "loader";
    case kPersist:    return "persist";
    case kAudio:      return "audio";
    case kPathfinder: return "pathfinder";
    case kPathWorker: return "path";
    case kRender:     return "render";
    case kCull:       return "cull";
    case kOther:      return "thread";
    default:          return "?";
    }
}

int Count() {
    const int n = g_count.load(std::memory_order_acquire);
    return n < kMaxThreads ? n : kMaxThreads;
}

uint32_t TidAt(int i) { return g_slots[i].tid.load(std::memory_order_acquire); }
char RoleAt(int i)    { return g_slots[i].role.load(std::memory_order_acquire); }
const volatile uint32_t* BusyAt(int i) { return g_slots[i].busy.load(std::memory_order_acquire); }

}  // namespace DiagThreads
