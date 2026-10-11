// ============================================================================
// src/platform/common/event_std.cpp -- Win32-style events over std::condition_variable
// ============================================================================
// plans/THREADS.md section C: ONE process-wide mutex + condition variable, and per
// event {signaled, manual}. A wait for any of N events scans from the lowest index
// under the mutex, so the result and the auto-reset consumption are exactly
// WaitForMultipleObjects(.., FALSE, ..)'s. Set notifies every waiter (a waiter whose
// events were not the ones set re-checks and sleeps again). Events are plain `new`
// objects, never MemoryAllocator memory.
// ============================================================================

#include "event_std.h"

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace Platform {

// Complete here only: on Windows the Win32 impl never defines it (its pointer is a
// HANDLE), so the two impls never share an object.
struct EventObject {
    bool signaled;
    bool manual;
};

namespace StdEvent {
namespace {

// IMMORTAL: built on first use and never destroyed. A Win32 event is a kernel object that
// the process's exit does not tear down while threads still wait on it, and retail exits
// with its System::Thread workers still parked in WaitForMultipleObjects (ExitProcess ends
// them). A function-local static WOULD be destroyed by exit()'s handlers, and glibc's
// pthread_cond_destroy waits for the waiters to leave, which parked workers never do: the
// Linux exe hung in QUITTING on exactly that (Phase D.4 LINUX-1's first menu route).
std::mutex& Mutex() {
    static std::mutex* const m = new std::mutex;
    return *m;
}
std::condition_variable& Cond() {
    static std::condition_variable* const c = new std::condition_variable;
    return *c;
}

// Caller holds the mutex. The index of the lowest signalled event, consumed if it is
// auto-reset; `count` when none is signalled.
uint32_t TakeLowest(uint32_t count, const EventHandle* events) {
    for (uint32_t i = 0; i < count; ++i) {
        if (events[i]->signaled) {
            if (!events[i]->manual) events[i]->signaled = false;
            return i;
        }
    }
    return count;
}

}  // namespace

EventHandle Create(bool manualReset, bool initiallySignaled) {
    return new EventObject{initiallySignaled, manualReset};
}

bool Set(EventHandle e) {
    {
        std::lock_guard<std::mutex> g(Mutex());
        e->signaled = true;
    }
    Cond().notify_all();
    return true;
}

bool Reset(EventHandle e) {
    std::lock_guard<std::mutex> g(Mutex());
    e->signaled = false;
    return true;
}

uint32_t Wait(EventHandle e, uint32_t timeoutMs) {
    return WaitAny(1, &e, timeoutMs);
}

uint32_t WaitAny(uint32_t count, const EventHandle* events, uint32_t timeoutMs) {
    if (count == 0) return kWaitFailed;
    std::unique_lock<std::mutex> lk(Mutex());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        const uint32_t i = TakeLowest(count, events);
        if (i < count) return i;
        if (timeoutMs == 0) return kWaitTimeout;
        if (timeoutMs == kInfinite) {
            Cond().wait(lk);
        } else if (Cond().wait_until(lk, deadline) == std::cv_status::timeout) {
            const uint32_t last = TakeLowest(count, events);
            return last < count ? last : kWaitTimeout;
        }
    }
}

bool Close(EventHandle e) {
    delete e;
    return true;
}

}  // namespace StdEvent
}  // namespace Platform
