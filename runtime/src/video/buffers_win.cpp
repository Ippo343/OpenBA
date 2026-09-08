// ============================================================================
// runtime/src/video/buffers_win.cpp — the Win32 half of Video::Device's
// recursive render lock, used by the four Platform{Construct,Destruct} bodies
// in buffers.cpp (Wave 9 lane W).
// ============================================================================
//
// WHY THIS FILE EXISTS AT ALL. `Video::ImageBuffer::PlatformConstruct` and its
// three siblings are not "platform" in any interesting sense — they create the
// dx11 companion object and splice `this` into a class-static intrusive list.
// The one OS-specific thing they do is take `Video::Device`'s lock, and they do
// it by INLINE COPY of `?Lock@Device@Video@@QEAAXXZ` (0x18000ca90) /
// `?Unlock@Device@Video@@QEAAXXZ` (0x18000cac0) rather than by calling them —
// four EnterCriticalSection / LeaveCriticalSection pairs sitting directly in
// the bodies.
//
// PLAN.md §4.1 decision 1 keeps `runtime/src/` free of <windows.h> outside a
// `_win` twin (only four files in the whole tree include it), so the lock moves
// here and buffers.cpp stays portable. A `_posix`/`_sdl` twin implements the
// same two entry points.
//
// THE LOCK IS RECURSIVE-BY-HAND, AND THAT IS NOT DECORATION. Retail keeps an
// owner thread id at Device+0x88 and a recursion depth at Device+0x8C alongside
// the CRITICAL_SECTION at +0x60, and `?VerifyLock@Device@Video@@QEAAXXZ`
// (0x18000cae0) reads them. dx11/ reads Device+0x100 at exactly this offset
// and calls Lock/Unlock by name, so the two fields have live readers on BOTH
// sides of the port: writing the depth but not the owner (or vice versa) would
// leave a lock that works and a VerifyLock that lies.
//
// Retail's exact sequence, from the four decompiles (they agree byte for byte):
//
//   lock:    d = *_singleton;
//            EnterCriticalSection(d + 0x60);
//            d->ownerTid = GetCurrentThreadId();
//            d->depth   += 1;
//
//   unlock:  d = *_singleton;              // RE-READ, not the cached `d`
//            d->depth -= 1;
//            if (d->depth == 0) d->ownerTid = 0xFFFFFFFF;
//            LeaveCriticalSection(d + 0x60);
//
// The unlock's re-read of the singleton is reproduced deliberately. It is
// observable if anything swaps `Device::_singleton` while the lock is held
// (device reset does exactly that in the retail teardown path), and "the
// compiler would have cached it" is not a licence to cache it for retail.
//
// NOTE ON THE NULL SINGLETON. `*_singleton` is dereferenced unguarded here,
// exactly as retail does. That is deliberate per CLAUDE.md's "don't fix a
// faithful crash with a null check": every caller of these four functions runs
// after `Video::Device` construction, and a null here means an upstream
// ordering defect that a guard would hide rather than fix.
// ============================================================================

#include <windows.h>
#include <cstdint>

// The offsets are asserted against runtime/types/video/Device.h in buffers.cpp,
// which owns the struct. This file deliberately does not include that header:
// pulling an engine type in behind <windows.h> is the collision PLAN §4.1 is
// about (<wingdi.h> declares a function called `Rectangle`; <windef.h> #defines
// `near` and `far` away). The three numbers are repeated here with the
// static_assert that binds them living in the portable half.
static const size_t kDeviceLockOffset  = 0x60;
static const size_t kDeviceOwnerOffset = 0x88;
static const size_t kDeviceDepthOffset = 0x8C;

static_assert(sizeof(CRITICAL_SECTION) == 0x28,
              "Device's layout puts the owner tid at +0x88, which requires a "
              "40-byte CRITICAL_SECTION at +0x60");

// `deviceSingleton` is the ADDRESS OF the exported `?_singleton@Device@Video@@`
// slot, not the Device itself — the re-read above is the whole reason it is
// passed as a slot pointer and not as an already-loaded `Device*`.
void RtVideoDeviceLock(void** deviceSingleton) {
    uint8_t* d = static_cast<uint8_t*>(*deviceSingleton);
    EnterCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(d + kDeviceLockOffset));
    *reinterpret_cast<uint32_t*>(d + kDeviceOwnerOffset) = GetCurrentThreadId();
    *reinterpret_cast<int32_t*>(d + kDeviceDepthOffset) += 1;
}

void RtVideoDeviceUnlock(void** deviceSingleton) {
    uint8_t* d = static_cast<uint8_t*>(*deviceSingleton);   // retail re-reads
    int32_t* depth = reinterpret_cast<int32_t*>(d + kDeviceDepthOffset);
    *depth -= 1;
    if (*depth == 0)
        *reinterpret_cast<uint32_t*>(d + kDeviceOwnerOffset) = 0xFFFFFFFFu;
    LeaveCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(d + kDeviceLockOffset));
}

// ============================================================================
// The EXPORTED Video::Device::Lock / ::Unlock  (Wave 9 lane VID)
//
// Retail 0x18000ca90 / 0x18000cac0. These are NOT the two functions above with
// a different spelling, and the difference is worth stating because getting it
// wrong is invisible:
//
//   * the two above are the INLINE COPIES the PlatformConstruct/PlatformDestruct
//     bodies carry, and they read `Device::_singleton` themselves;
//   * these two are the exported members, and they operate on the `this` the
//     caller passed. dx11/ calls them by name on a Device it already holds.
//
// The store ORDER also differs, and is preserved rather than normalised:
//
//   0x18000ca99  ADD RCX,0x60
//   0x18000ca9d  CALL EnterCriticalSection
//   0x18000caa3  CALL GetCurrentThreadId
//   0x18000caa9  INC dword ptr [RBX+0x8c]        <-- depth FIRST
//   0x18000caaf  MOV dword ptr [RBX+0x88],EAX    <-- then the owner id
//
// while the inline copy stores the owner first. Neither order is observable
// from a single thread; both are transcribed as written, because "the compiler
// would have reordered it anyway" is not a licence to reorder it for retail.
//
// Unlock takes no singleton re-read at all — RCX IS the object:
//
//   0x18000cac0  DEC dword ptr [RCX+0x8c]
//   0x18000cac6  JNZ .leave
//   0x18000cac8  MOV dword ptr [RCX+0x88],0xffffffff
//   0x18000cad2  ADD RCX,0x60
//   0x18000cad6  JMP LeaveCriticalSection
//
// Note the predicate: retail tests the ZF from the DEC (depth != 0), which is
// the same as comparing the decremented value with zero. Reproduced as such.
// ============================================================================
void RtVideoDeviceLockThis(void* device) {
    uint8_t* d = static_cast<uint8_t*>(device);
    EnterCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(d + kDeviceLockOffset));
    DWORD tid = GetCurrentThreadId();
    *reinterpret_cast<int32_t*>(d + kDeviceDepthOffset) += 1;
    *reinterpret_cast<uint32_t*>(d + kDeviceOwnerOffset) = tid;
}

void RtVideoDeviceUnlockThis(void* device) {
    uint8_t* d = static_cast<uint8_t*>(device);
    int32_t* depth = reinterpret_cast<int32_t*>(d + kDeviceDepthOffset);
    *depth -= 1;
    if (*depth == 0)
        *reinterpret_cast<uint32_t*>(d + kDeviceOwnerOffset) = 0xFFFFFFFFu;
    LeaveCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(d + kDeviceLockOffset));
}
