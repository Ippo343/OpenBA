// ============================================================================
// src/diag/diag_threads.h -- which thread is which (NOT RETAIL; block LOADSTAMP)
// ============================================================================
// A tiny portable roster of the threads the game creates, keyed by OS thread id:
// the main thread, the engine loader (Engine::Loader), the persistence worker, the
// audio stream, the pathfinder, the path-worker pool, the render thread and the
// cull helpers. Two readers:
//   * the [LoadTime] stamps (load_time.h) name the thread they print on;
//   * the sampling profiler (src/diag/win/sampler.cpp, BANISHED_SAMPLE) samples
//     every registered thread, not only the main one.
//
// A thread registers ITSELF (Register reads the calling thread's id), once: each
// call after the first on the same thread is one thread_local test. Every
// System::Thread registers from System::Thread::ThreadFunc just before its first
// Run (the object is fully constructed there, so its dynamic type names the role);
// the cull helpers register from HelperPool::ThreadMain; the main thread from
// load_time.cpp's static initialiser. Registration never fails the caller: past
// kMaxThreads entries it is dropped.
//
// Lock-free: an append-only array of {tid, role}; the tid is published last
// (release), a reader skips a slot whose tid is still 0. No allocation, no
// constructor (constant-initialised), so it is safe before main.
// ============================================================================
#pragma once

#include <cstdint>

namespace DiagThreads {

// Role letters: one byte, used as the sampler's per-sample thread field.
constexpr char kMain       = 'M';   // the main (sim + draw) thread
constexpr char kLoader     = 'L';   // Engine::Loader (LoaderLoop::_loader): UpdateLoop::Load jobs
constexpr char kPersist    = 'S';   // IO::PersistenceWorker: save / load file I/O
constexpr char kAudio      = 'A';   // Audio::Stream
constexpr char kPathfinder = 'F';   // PathfinderThread
constexpr char kPathWorker = 'P';   // ext.path_workers pool thread
constexpr char kRender     = 'R';   // ext.render_pipe render thread
constexpr char kCull       = 'C';   // ext.gather_threads cull helper
constexpr char kOther      = 'T';   // a System::Thread of another type

constexpr int kMaxThreads = 256;

// Register the calling thread under `role` (idempotent per thread). `busy`, when
// given, is a word whose bit 0 is set while the thread is mid-job (System::Thread's
// _runFlags): the sampler samples such a thread for the whole job, waits included.
void Register(char role, const volatile uint32_t* busy = nullptr);
// The same, the role read from a System::Thread subclass's type name
// (typeid(*thread).name(), mangled or not).
void RegisterNamed(const char* typeName, const volatile uint32_t* busy = nullptr);

// The role registered for `tid`, or 0.
char RoleOf(uint32_t tid);
// "main", "loader", "persist", "audio", "pathfinder", "path", "render", "cull", "thread".
const char* RoleName(char role);

// Snapshot for the sampler: entries [0, Count()) in registration order; a slot whose
// tid reads 0 is still being written (skip it).
int Count();
uint32_t TidAt(int i);
char RoleAt(int i);
const volatile uint32_t* BusyAt(int i);   // null when the thread registered none

}  // namespace DiagThreads
