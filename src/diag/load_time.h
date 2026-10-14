// ============================================================================
// src/diag/load_time.h -- [LoadTime] stage stamps (NOT RETAIL; block LOADSTAMP)
// ============================================================================
// One stderr line per load-stage boundary, always on (no flag):
//
//     [LoadTime] <stage> t=<ms since process start, 1 decimal> thread=<name>[ <detail>]
//
// <name> is "main", "loader", another registered role (diag_threads.h: persist,
// audio, pathfinder, path, render, cull, thread) or "tid<N>" for an unregistered
// thread. A stamp marks the START of the step it names; the step's length is the
// next stamp's t minus this one (on the same thread). The origin is captured by this
// file's static initialiser, i.e. before main, within a few ms of process start.
//
// Stamps sit at load boundaries only (boot, new game, load save, loop switches):
// a few dozen lines per load, never a per-frame path. A stamp is a print and
// nothing else: it reads no game state and changes none. fprintf(stderr) survives
// the log-stripped flavours (only `printf` is macro'd away, src/app/log_strip.h).
// The stage list and how to read it: docs/extensions/handins/LOADSTAMP.md.
// ============================================================================
#pragma once

#include <cstdint>
#include <typeinfo>

namespace LoadTime {

// ms since process start (the static-init origin).
double NowMs();
// A Platform::PerfCounter value -> ms since process start.
double CounterToMs(int64_t counter);

// One [LoadTime] line. `detail` (optional) is appended after the thread field.
void Stamp(const char* stage, const char* detail = nullptr);
// The same, with a polymorphic object's demangled dynamic type as the detail
// (e.g. which UpdateLoop a loop switch starts).
void StampType(const char* stage, const std::type_info& type);

// FNV-1a 64 over a byte range, chained through `h` (start with kHashSeed). Used by the
// print-only [WorldHash] lines (the loading session's world-identity proof).
constexpr uint64_t kHashSeed = 0xcbf29ce484222325ull;
uint64_t Hash64(const void* data, uint64_t bytes, uint64_t h = kHashSeed);

}  // namespace LoadTime
