// ============================================================================
// src/diag/device_lost_drive.h -- BANISHED_FORCE_DEVICE_LOST=bgsave (diagnostic, NOT RETAIL)
// ============================================================================
// The forced device-lost drive (BANISHED_FORCE_DEVICE_LOST=<n>, src/render/dx11/contextdata.cpp
// and render_pipe_finish.cpp) injects DXGI_ERROR_DEVICE_RESET on Present #n. A Present number
// cannot be aimed at a window of a few frames whose position depends on the frame rate, so the
// value `bgsave` arms a request mode instead: ext.background_save requests the injection when it
// starts a save's worker, and the next Present takes it. The device-lost route then runs while
// the worker is reading the world (docs/extensions/handins/HITCH_BGSAVE.md, scenario 4).
//
// The value is parsed by the two injectors and by the extension; one injection per process.
// ============================================================================
#pragma once

#include "options.h"

#include <atomic>
#include <cstring>

namespace DeviceLostDrive {

inline std::atomic<bool> g_request{false};

// The flag's value is `bgsave`.
inline bool Armed() {
    const char* v = Opt().forceDeviceLost;
    return v != nullptr && std::strcmp(v, "bgsave") == 0;
}

inline void Request() { g_request.store(true, std::memory_order_release); }

// The injector's test in request mode: true once, on the first Present after a request.
inline bool TakeRequest() { return g_request.exchange(false, std::memory_order_acq_rel); }

}  // namespace DeviceLostDrive
