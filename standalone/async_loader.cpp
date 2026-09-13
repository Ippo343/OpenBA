// standalone/async_loader.cpp — see async_loader.h for design notes.


#include "async_loader.h"
#include "stall_debug.h"
#include "UpdateLoop.h"
#include "options.h"
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <process.h>
#include <windows.h>

namespace AsyncLoader {

namespace {

struct LoadJob {
    UpdateLoop* loop;
    std::atomic<bool> done;
    HANDLE thread;
};

// Single-job model: only one async load in flight at a time. Splash chain
// transitions are sequential (ShiningRock → Loading → MenuDLT) so we never
// need overlapping loads.
LoadJob* g_currentJob = nullptr;

unsigned __stdcall WorkerProc(void* param) {
    LoadJob* job = reinterpret_cast<LoadJob*>(param);
    UpdateLoop* loop = job->loop;
    if (loop && loop->vtable && loop->vtable->Load) {
        typedef void (*LoadFn)(UpdateLoop*);
        LoadFn fn = reinterpret_cast<LoadFn>(loop->vtable->Load);
        double startMs = StallDebug::NowMs();
        printf("[AsyncLoader] worker thread loop=%p Load=%p — start "
                    "(t=%.2fms)\n",
                    (void*)loop, (void*)fn, startMs);
        std::fflush(stdout);
        fn(loop);
        double endMs = StallDebug::NowMs();
        printf("[AsyncLoader] worker thread loop=%p — done "
                    "(t=%.2fms, duration=%.2fms)\n",
                    (void*)loop, endMs, endMs - startMs);
        std::fflush(stdout);
    } else {
        printf("[AsyncLoader] worker thread WARN loop=%p has no Load slot\n",
                    (void*)loop);
    }
    job->done.store(true, std::memory_order_release);
    return 0;
}

void DrainPriorJob() {
    if (!g_currentJob) return;
    if (!g_currentJob->done.load(std::memory_order_acquire)) {
        printf("[AsyncLoader] DrainPriorJob: waiting on prior worker\n");
        WaitForSingleObject(g_currentJob->thread, INFINITE);
    }
    if (g_currentJob->thread) {
        CloseHandle(g_currentJob->thread);
    }
    delete g_currentJob;
    g_currentJob = nullptr;
}

} // namespace

// Root-cause fix for the intermittent menu/startup heap-corruption crash:
// run the load on the CALLER (main) thread by default, NOT a worker thread.
//
// The engine allocator System::MemoryAllocator::Allocate (Runtime 0x1800c6e60)
// is NOT thread-safe — it manipulates the boundary-tag free-list
// (UnlinkFreeTag/LinkFreeTag) WITHOUT taking the allocator critical section
// (+0x640), which only Free/MemFree take. Running `next->vtable->Load`
// (heavy External::Load allocation, e.g. achievement/goal FindOrLoad) on a
// worker thread while the main thread also allocates (ChunkedSceneLoader::Tick
// building 12-15K menu-scene props, plus per-frame UI/Draw) races the shared
// free-list → a clobbered link → a wild pointer that the next Allocate follows
// to an access violation. ~1/3 of menu launches crashed this way; two threads
// were caught simultaneously faulting inside Allocate on the same wild ptr.
//
// MenuDLT::Load is only ~85ms, and the expensive scene chunking still animates
// the splash via ChunkedSceneLoader on the main thread AFTER this returns, so
// the visible cost is a single ~85ms hitch — well worth eliminating the race.
// StartAsyncLoad is used ONLY by the menu splash chain (splash_loop_stubs.cpp);
// the in-game world build goes through fn_Loader_Load instead, so this is
// surgical to the menu path.
//
// Set BANISHED_ASYNC_LOAD=1 to opt back into the (racy) worker thread — kept
// for experimentation, e.g. once the allocator is made thread-safe by patching
// Allocate to take the critical section.
static bool AsyncLoadOptIn() {
    return Opt().asyncLoad;
}

void StartAsyncLoad(UpdateLoop* loop, bool forceThreaded) {
    DrainPriorJob();
    if (!loop) {
        printf("[AsyncLoader] StartAsyncLoad: null loop, skipping\n");
        return;
    }
    LoadJob* job = new LoadJob{loop, {false}, nullptr};
    if (!AsyncLoadOptIn() && !forceThreaded) {
        printf("[AsyncLoader] running load synchronously on caller thread "
               "(default — avoids non-thread-safe-allocator race; set "
               "BANISHED_ASYNC_LOAD=1 for the old worker thread)\n");
        WorkerProc(job);          // sets job->done = true
        job->thread = nullptr;
        g_currentJob = job;       // DrainPriorJob will just delete it
        return;
    }
    printf("[AsyncLoader] using worker thread (forceThreaded=%d, "
           "BANISHED_ASYNC_LOAD=%d) — allocator free-list is serialized by "
           "allocator_lock.cpp\n", (int)forceThreaded, (int)AsyncLoadOptIn());
    job->thread = reinterpret_cast<HANDLE>(_beginthreadex(
        nullptr, 0, &WorkerProc, job, 0, nullptr));
    if (!job->thread) {
        printf("[AsyncLoader] _beginthreadex failed (errno=%d), running "
                    "load synchronously on caller thread\n", errno);
        WorkerProc(job);  // fallback — at least don't lose the load
    }
    g_currentJob = job;
    printf("[AsyncLoader] StartAsyncLoad: spawned worker for loop=%p "
                "thread=%p\n", (void*)loop, (void*)job->thread);
}

bool IsAsyncLoadBusy() {
    return g_currentJob && !g_currentJob->done.load(std::memory_order_acquire);
}

} // namespace AsyncLoader

