// ============================================================================
// tests/unit/parallel_for_test.cpp -- System::ParallelFor's pool under contention
// ============================================================================
// wg_parallel_test.cpp pins the happy path (every index once for 0..7 workers, nested
// calls inline). This file pins the two edges the header leaves to the body
// (src/engine/system/parallel_for.cpp):
//   * SetWorkers while a job is in flight on another thread: it takes the call lock, so
//     it WAITS for the job (no chunk lost, none run twice, the caller returns first) and
//     the pool is rebuilt afterwards with the new count; the next call runs on it.
//   * an exception thrown from a chunk: the header says `f` must not throw and the body
//     catches nothing. With no workers (the inline path) the exception propagates to the
//     caller of ParallelFor; the chunk ran once up to the throw; the call lock is released
//     (a later call from another thread proceeds); the pool is intact (the next call with
//     workers covers its range once). What is NOT pinned, because it cannot be observed
//     from a test: with workers, a throw on the CALLER's chunk propagates while the
//     workers may still be running (the caller never waits for them), and a throw on a
//     worker thread is std::terminate. The throwing call runs on a helper thread here
//     because the body's `tl_inParallel` flag stays set on the thread that threw: the
//     helper thread ends and takes the flag with it, so the rest of the unit is unaffected
//     (that leak is the observable the second half of the test records).
// ============================================================================
#include "unit.h"

#include "platform_thread.h"
#include "system/parallel_for.h"

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

struct ChunkLog {
    std::vector<std::atomic<int>> hits;
    std::atomic<int> calls{0};
    explicit ChunkLog(int n) : hits(n > 0 ? n : 1) { for (auto& h : hits) h.store(0); }
    bool EveryIndexOnce(int n) const {
        for (int i = 0; i < n; ++i) if (hits[static_cast<size_t>(i)].load() != 1) return false;
        return true;
    }
};

// ---- SetWorkers during a job ------------------------------------------------------------
struct Contender {
    std::atomic<int> started{0};        // the job's first chunk ran
    std::atomic<int> jobReturned{0};    // ParallelFor returned on the main thread
    std::atomic<int> setReturned{0};    // SetWorkers returned on the helper thread
    std::atomic<int> jobWasDoneWhenSetReturned{0};
    int newWorkers = 2;
};

unsigned int SetWorkersWhileRunning(void* arg) {
    Contender* c = static_cast<Contender*>(arg);
    while (c->started.load() == 0) Platform::SleepMs(1);
    System::Parallel::SetWorkers(c->newWorkers);
    c->jobWasDoneWhenSetReturned.store(c->jobReturned.load());
    c->setReturned.store(1);
    return 0;
}

// ---- the throwing chunk ------------------------------------------------------------------
struct Thrower {
    int n = 0;
    std::atomic<int> chunkCalls{0};
    std::atomic<int> caught{0};
    std::atomic<int> indicesBeforeThrow{0};
    std::atomic<int> secondCallCalls{0};
    std::atomic<int> secondCallOnce{0};
};

unsigned int ThrowFromAChunk(void* arg) {
    Thrower* t = static_cast<Thrower*>(arg);
    try {
        System::ParallelFor(0, t->n, 16, [&](int b, int e) {
            t->chunkCalls.fetch_add(1);
            for (int i = b; i < e; ++i) {
                if (i == 7) throw std::runtime_error("chunk");
                t->indicesBeforeThrow.fetch_add(1);
            }
        });
    } catch (const std::runtime_error&) {
        t->caught.store(1);
    }
    // A second call on this same thread: the flag the throw left set makes it run inline,
    // and it still covers its range exactly once.
    ChunkLog log(t->n);
    System::ParallelFor(0, t->n, 16, [&](int b, int e) {
        log.calls.fetch_add(1);
        for (int i = b; i < e; ++i) log.hits[static_cast<size_t>(i)].fetch_add(1);
    });
    t->secondCallCalls.store(log.calls.load());
    t->secondCallOnce.store(log.EveryIndexOnce(t->n) ? 1 : 0);
    return 0;
}

}  // namespace

UNIT_TEST(parallel_for_set_workers_waits_for_the_job_in_flight) {
    const int n = 4000;
    System::Parallel::SetWorkers(4);
    CHECK_EQ(System::Parallel::Workers(), 4);

    Contender c;
    Platform::ThreadHandle helper = Platform::ThreadCreateSuspended(&SetWorkersWhileRunning, &c, 0x10000);
    CHECK(helper != nullptr);
    if (helper == nullptr) UNIT_SKIP("no helper thread");
    Platform::ThreadResume(helper);

    ChunkLog log(n);
    System::ParallelFor(0, n, 50, [&](int b, int e) {   // 80 chunks, each ~1 ms: the helper lands inside
        c.started.store(1);
        log.calls.fetch_add(1);
        Platform::SleepMs(1);
        for (int i = b; i < e; ++i) log.hits[static_cast<size_t>(i)].fetch_add(1);
    });
    c.jobReturned.store(1);
    Platform::ThreadJoin(helper);
    Platform::ThreadClose(helper);

    CHECK(log.EveryIndexOnce(n));                 // nothing lost, nothing duplicated
    CHECK_EQ(log.calls.load(), 80);
    CHECK_EQ(c.setReturned.load(), 1);
    CHECK_EQ(c.jobWasDoneWhenSetReturned.load(), 1);   // serialised: SetWorkers returned after the job
    CHECK_EQ(System::Parallel::Workers(), 2);           // and the pool was rebuilt with the new count

    // The rebuilt pool runs the next job completely.
    ChunkLog again(n);
    System::ParallelFor(0, n, 7, [&](int b, int e) {
        again.calls.fetch_add(1);
        for (int i = b; i < e; ++i) again.hits[static_cast<size_t>(i)].fetch_add(1);
    });
    CHECK(again.EveryIndexOnce(n));
    System::Parallel::SetWorkers(4);
}

UNIT_TEST(parallel_for_exception_in_an_inline_chunk_propagates_and_releases_the_lock) {
    System::Parallel::SetWorkers(0);              // the inline path: the only one a throw can take safely
    Thrower t;
    t.n = 100;
    Platform::ThreadHandle helper = Platform::ThreadCreateSuspended(&ThrowFromAChunk, &t, 0x10000);
    CHECK(helper != nullptr);
    if (helper == nullptr) UNIT_SKIP("no helper thread");
    Platform::ThreadResume(helper);
    Platform::ThreadJoin(helper);
    Platform::ThreadClose(helper);

    CHECK_EQ(t.caught.load(), 1);                 // propagated to ParallelFor's caller
    CHECK_EQ(t.chunkCalls.load(), 1);             // the inline path: one chunk, the whole range
    CHECK_EQ(t.indicesBeforeThrow.load(), 7);     // ran up to the throw, nothing after
    CHECK_EQ(t.secondCallOnce.load(), 1);         // the next call on that thread still covers its range
    CHECK_EQ(t.secondCallCalls.load(), 1);        // ... inline (the leaked flag; 0 workers anyway here)

    // The call lock was released by the unwind: a call from THIS thread with workers runs
    // (it would hang forever otherwise) and covers its range once.
    System::Parallel::SetWorkers(3);
    CHECK_EQ(System::Parallel::Workers(), 3);
    ChunkLog log(1000);
    System::ParallelFor(0, 1000, 10, [&](int b, int e) {
        log.calls.fetch_add(1);
        for (int i = b; i < e; ++i) log.hits[static_cast<size_t>(i)].fetch_add(1);
    });
    CHECK(log.EveryIndexOnce(1000));
    CHECK_EQ(log.calls.load(), 100);              // on this thread the flag is clear: real chunks
    System::Parallel::SetWorkers(4);
}
