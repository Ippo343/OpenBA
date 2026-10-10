#pragma once
// ============================================================================
// src/platform/common/event_std.h -- the std-based Win32-event twin
// ============================================================================
// The event half of platform_sync.h built on C++17 std only (std::mutex +
// std::condition_variable), under its own names so it links beside the Win32 impl:
// the unit tests run every event test against BOTH (tests/unit/platform_sync_test.cpp),
// and the POSIX sync impl forwards Platform::Event* here (plans/THREADS.md E.9).
//
// Semantics are Win32's: auto-reset events are consumed by the wait that returns
// them, manual-reset ones are not; WaitAny returns the LOWEST signalled index and
// consumes only that event; results use platform_sync.h's numbering.
// ============================================================================

#include "platform_sync.h"

namespace Platform {
namespace StdEvent {

EventHandle Create(bool manualReset, bool initiallySignaled);
bool Set(EventHandle e);
bool Reset(EventHandle e);
uint32_t Wait(EventHandle e, uint32_t timeoutMs);
uint32_t WaitAny(uint32_t count, const EventHandle* events, uint32_t timeoutMs);
bool Close(EventHandle e);

}  // namespace StdEvent
}  // namespace Platform
