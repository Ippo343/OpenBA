// getenv cache — see env_cache.h. This TU must see the REAL CRT functions.
#undef getenv
#undef _putenv_s
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <windows.h>

namespace {

struct Entry { char* name; char* value; unsigned gen; };
constexpr int kCap = 1024;               // power of two; ~370 distinct names in the port
Entry g_tbl[kCap];
unsigned g_gen = 1;                      // bumped by BanishedPutenvS -> every entry stale
CRITICAL_SECTION g_lock;
bool g_lockInit = false;

inline uint32_t Hash(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}

void EnsureLock() {
    if (!g_lockInit) { InitializeCriticalSection(&g_lock); g_lockInit = true; }
}

} // namespace

extern "C" char* BanishedGetenvCached(const char* name) {
    if (!name) return nullptr;
    EnsureLock();
    EnterCriticalSection(&g_lock);
    uint32_t h = Hash(name) & (kCap - 1);
    for (int probe = 0; probe < kCap; ++probe) {
        Entry& e = g_tbl[(h + probe) & (kCap - 1)];
        if (!e.name) {
            e.name = _strdup(name);
            e.value = std::getenv(name);
            e.gen = g_gen;
            char* v = e.value;
            LeaveCriticalSection(&g_lock);
            return v;
        }
        if (std::strcmp(e.name, name) == 0) {
            if (e.gen != g_gen) { e.value = std::getenv(name); e.gen = g_gen; }
            char* v = e.value;
            LeaveCriticalSection(&g_lock);
            return v;
        }
    }
    LeaveCriticalSection(&g_lock);
    return std::getenv(name);            // table full (cannot happen at kCap = 1024)
}

extern "C" int BanishedPutenvS(const char* name, const char* value) {
    EnsureLock();
    EnterCriticalSection(&g_lock);
    const int r = _putenv_s(name, value);
    ++g_gen;                             // invalidate every cached value
    LeaveCriticalSection(&g_lock);
    return r;
}
