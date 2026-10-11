// ============================================================================
// src/platform/posix/os_posix.cpp -- Platform:: process-level services over POSIX
// ============================================================================
// The POSIX half of src/platform/common/platform_os.h (Phase D.4 THREADS E.9).
//
// IsReadablePage (a diagnostic probe, not retail): the page must be MAPPED and
// readable. A mapped page may still be PROT_NONE (MemoryAllocator's reserved but
// uncommitted ranges, allocator_posix.cpp), so the probe reads the mapping's
// permissions from /proc/self/maps rather than asking whether it is mapped.
// ============================================================================

#include "platform_os.h"

#include <unistd.h>
#include <cstdio>

namespace Platform {

namespace {

uintptr_t PageBase(const void* p) {
    const uintptr_t page = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
    return reinterpret_cast<uintptr_t>(p) & ~(page - 1);  // CAST-OK: ptr_int
}

}  // namespace

bool IsReadablePage(const void* p) {
    const uintptr_t base = PageBase(p);
    // /proc/self/maps: "start-end perms ...", hex addresses, sorted.
    FILE* maps = std::fopen("/proc/self/maps", "r");
    if (maps == nullptr) return false;
    char line[512];
    bool readable = false;
    while (std::fgets(line, sizeof(line), maps) != nullptr) {
        unsigned long long lo = 0, hi = 0;
        char perms[8] = {};
        if (std::sscanf(line, "%llx-%llx %7s", &lo, &hi, perms) != 3) continue;
        if (base >= lo && base < hi) {
            readable = perms[0] == 'r';
            break;
        }
    }
    std::fclose(maps);
    return readable;
}

// ExitProcess: platform_os.h says why error.cpp's Windows arm still declares the
// kernel32 import itself.
void ExitProcess(uint32_t exitCode) {
    std::fflush(nullptr);
    _exit(static_cast<int>(exitCode));
}

}  // namespace Platform
