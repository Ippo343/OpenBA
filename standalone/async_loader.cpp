// standalone/async_loader.cpp — see async_loader.h for design notes.

#ifdef STANDALONE_EXE

#include "async_loader.h"
#include "stall_debug.h"
#include "UpdateLoop.h"
#include <atomic>
#include <cstdio>
#include <cstdint>
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

void StartAsyncLoad(UpdateLoop* loop) {
    DrainPriorJob();
    if (!loop) {
        printf("[AsyncLoader] StartAsyncLoad: null loop, skipping\n");
        return;
    }
    LoadJob* job = new LoadJob{loop, {false}, nullptr};
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

#endif // STANDALONE_EXE
