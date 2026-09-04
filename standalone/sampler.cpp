// Sampling profiler for the sim thread — see sampler.h.
#include "sampler.h"
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>

#include "hw_watch.h"

static HANDLE g_mainThread = nullptr;   // from HwWatch_Init (SUSPEND_RESUME + GET_CONTEXT)

volatile int g_samplerTag = 0;

namespace {

constexpr int kMaxFrames = 20;

struct Sample { uint8_t tag; uint8_t n; uintptr_t rip[kMaxFrames]; };

struct ModInfo {
    HMODULE base; uintptr_t lo, hi; char prefix;
    // exports (rva,name), sorted by rva — for nearest-symbol attribution
    std::vector<std::pair<uint32_t, const char*>> exports;
};

CRITICAL_SECTION g_lock;
std::vector<Sample> g_samples;       // pending (guarded by g_lock)
std::vector<Sample> g_flushBuf;
HANDLE g_thread = nullptr;
volatile LONG g_stop = 0;
int g_hz = 1000;
long long g_total = 0, g_failed = 0;
ModInfo g_mods[3];                   // exe, Runtime, VideoDX11
int g_nMods = 0;
FILE* g_out = nullptr;

void LoadExports(ModInfo& m) {
    auto* base = reinterpret_cast<const uint8_t*>(m.base);
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;
    m.hi = m.lo + nt->OptionalHeader.SizeOfImage;
    const IMAGE_DATA_DIRECTORY& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dd.VirtualAddress || !dd.Size) return;
    auto* ex = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + dd.VirtualAddress);
    auto* funcs = reinterpret_cast<const uint32_t*>(base + ex->AddressOfFunctions);
    auto* names = reinterpret_cast<const uint32_t*>(base + ex->AddressOfNames);
    auto* ords  = reinterpret_cast<const uint16_t*>(base + ex->AddressOfNameOrdinals);
    m.exports.reserve(ex->NumberOfNames);
    for (uint32_t i = 0; i < ex->NumberOfNames; ++i) {
        const uint16_t o = ords[i];
        if (o >= ex->NumberOfFunctions) continue;
        const uint32_t rva = funcs[o];
        if (rva >= dd.VirtualAddress && rva < dd.VirtualAddress + dd.Size) continue;  // forwarder
        m.exports.push_back({ rva, reinterpret_cast<const char*>(base + names[i]) });
    }
    std::sort(m.exports.begin(), m.exports.end());
}

void AddMod(const char* name, char prefix, bool withExports) {
    HMODULE h = GetModuleHandleA(name);
    if (!h || g_nMods >= 3) return;
    ModInfo& m = g_mods[g_nMods++];
    m.base = h; m.lo = reinterpret_cast<uintptr_t>(h); m.hi = m.lo + 0x10000000; m.prefix = prefix;
    if (withExports) LoadExports(m); else {
        auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(h);
        auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(reinterpret_cast<const uint8_t*>(h) + dos->e_lfanew);
        m.hi = m.lo + nt->OptionalHeader.SizeOfImage;
    }
}

// One frame -> "E:0x140xxxxxx" (exe, rebased to the link-time image base so addr2line
// works), "R:<export>+0xoff" (Runtime nearest export), "V:0xoff", or "?:<module>+off".
void PrintFrame(FILE* o, uintptr_t rip) {
    for (int i = 0; i < g_nMods; ++i) {
        const ModInfo& m = g_mods[i];
        if (rip < m.lo || rip >= m.hi) continue;
        const uintptr_t off = rip - m.lo;
        if (m.prefix == 'E') { std::fprintf(o, " E:0x%llx", 0x140000000ULL + off); return; }
        if (!m.exports.empty()) {
            auto it = std::upper_bound(m.exports.begin(), m.exports.end(),
                                       std::make_pair((uint32_t)off, (const char*)nullptr),
                                       [](const std::pair<uint32_t, const char*>& a,
                                          const std::pair<uint32_t, const char*>& b) { return a.first < b.first; });
            if (it != m.exports.begin()) {
                --it;
                std::fprintf(o, " %c:%s+0x%llx", m.prefix, it->second, (unsigned long long)(off - it->first));
                return;
            }
        }
        std::fprintf(o, " %c:0x%llx", m.prefix, (unsigned long long)off);
        return;
    }
    HMODULE mod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)rip, &mod) && mod) {
        char path[MAX_PATH] = {0};
        GetModuleFileNameA(mod, path, MAX_PATH);
        const char* nm = std::strrchr(path, '\\');
        std::fprintf(o, " ?:%s+0x%llx", nm ? nm + 1 : path, (unsigned long long)(rip - (uintptr_t)mod));
    } else {
        std::fprintf(o, " ?:0x%llx", (unsigned long long)rip);
    }
}

void WriteSamples(const std::vector<Sample>& v) {
    if (!g_out) g_out = std::fopen("standalone_samples.txt", "w");
    if (!g_out) return;
    for (const Sample& s : v) {
        std::fprintf(g_out, "%d %d", (int)s.tag, (int)s.n);
        for (int i = 0; i < s.n; ++i) PrintFrame(g_out, s.rip[i]);
        std::fputc('\n', g_out);
    }
    std::fflush(g_out);
}

bool TakeSample(Sample& s) {
    s.tag = (uint8_t)g_samplerTag;
    s.n = 0;
    if (SuspendThread(g_mainThread) == (DWORD)-1) return false;
    CONTEXT ctx; std::memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    bool ok = GetThreadContext(g_mainThread, &ctx) != 0;
    if (ok) {
        // Re-read the tag now that the thread is stopped (it may have moved since).
        s.tag = (uint8_t)g_samplerTag;
        for (int f = 0; f < kMaxFrames && ctx.Rip; ++f) {
            s.rip[s.n++] = (uintptr_t)ctx.Rip;
            DWORD64 imageBase = 0;
            PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &imageBase, nullptr);
            if (!fn) {
                // leaf function: return address is at [rsp]
                const uintptr_t* sp = reinterpret_cast<const uintptr_t*>(ctx.Rsp);
                if (IsBadReadPtr(sp, sizeof(uintptr_t))) break;
                ctx.Rip = *sp; ctx.Rsp += 8;
                continue;
            }
            PVOID handlerData = nullptr; ULONG64 est = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, ctx.Rip, fn, &ctx, &handlerData, &est, nullptr);
        }
    }
    ResumeThread(g_mainThread);
    return ok && s.n > 0;
}

DWORD WINAPI SamplerThread(void*) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    HANDLE timer = CreateWaitableTimerA(nullptr, FALSE, nullptr);
    LARGE_INTEGER due; due.QuadPart = -1;
    const LONG periodMs = g_hz >= 1000 ? 1 : (1000 / g_hz);
    if (timer) SetWaitableTimer(timer, &due, periodMs, nullptr, nullptr, FALSE);
    DWORD lastFlush = GetTickCount();
    Sample s;
    while (!g_stop) {
        if (timer) WaitForSingleObject(timer, 50); else Sleep(periodMs);
        if (g_stop) break;
        if (TakeSample(s)) {
            EnterCriticalSection(&g_lock);
            g_samples.push_back(s);
            LeaveCriticalSection(&g_lock);
            ++g_total;
        } else {
            ++g_failed;
        }
        if (GetTickCount() - lastFlush > 10000) { Sampler_Flush(); lastFlush = GetTickCount(); }
    }
    if (timer) CloseHandle(timer);
    return 0;
}

void AtExitFlush() {
    InterlockedExchange(&g_stop, 1);
    if (g_thread) { WaitForSingleObject(g_thread, 2000); }
    Sampler_Flush();
    if (g_out) { std::fclose(g_out); g_out = nullptr; }
    std::fprintf(stderr, "[Sampler] done: %lld samples (%lld failed) -> standalone_samples.txt\n",
                 g_total, g_failed);
}

} // namespace

void Sampler_Flush() {
    EnterCriticalSection(&g_lock);
    g_flushBuf.swap(g_samples);
    LeaveCriticalSection(&g_lock);
    if (!g_flushBuf.empty()) { WriteSamples(g_flushBuf); g_flushBuf.clear(); }
}

void Sampler_Init() {
    const char* e = std::getenv("BANISHED_SAMPLE");
    if (!e || !e[0] || e[0] == '0') return;
    g_mainThread = (HANDLE)HwWatch_MainThreadHandle();
    if (!g_mainThread) { std::fprintf(stderr, "[Sampler] no main-thread handle (HwWatch_Init first)\n"); return; }
    const int hz = std::atoi(e);
    if (hz > 1) g_hz = hz;
    InitializeCriticalSection(&g_lock);
    g_samples.reserve(1 << 16);
    AddMod(nullptr, 'E', false);   // engine DLLs are added at the first flush (loaded later)
    std::fprintf(stderr, "[Sampler] on: %d Hz\n", g_hz);
    g_thread = CreateThread(nullptr, 0, SamplerThread, nullptr, 0, nullptr);
    std::atexit(AtExitFlush);
}
