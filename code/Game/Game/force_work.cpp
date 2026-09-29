// code/Game/Game/force_work.cpp
// Generic force-work testing framework core — see force_work.h.
// Extracted from forester_force_harness.cpp (unchanged mechanics) so multiple module
// tests (forester, construction, ...) share the crash-safe probes + drive step.


#include "force_work.h"
#include <cstdint>
#include <new>
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "Component/MapComponent.h"   // caller batch CB6: TypeId()
#include "Component/CitizenComponent.h"     // CitizenStackIdle (B.4 GAME-a)
#include "Component/CharacterComponent.h"
#include "Entity/ComponentEntity.h"
#include "Game/Map.h"
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <windows.h>   // VirtualQuery
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/VSlot.h"      // B.3.x W0: SafeGetTypeId's slot probe

namespace ForceWork {

// ---- readable-region cache --------------------------------------------------
// VirtualQuery is a syscall that walks the process VAD tree under a lock; measured
// at ~80 us per call in this process (BANISHED_CITPROF, 2026-07-25). A per-object
// probe therefore costs real milliseconds as soon as a caller loops over entities:
// CollectStats' 18-citizen guard was 2.98 ms of its 3.01 ms.
//
// Every probed object lives in one of a handful of large engine-heap arenas, so the
// queries return the same few regions over and over. Cache the POSITIVE ones only —
// a garbage pointer (the 0x760 desc that originally motivated the guard) is in no
// cached region and still takes the real query, which still fails it.
//
// Staleness: entries are only valid inside one ResetReadableCache() ... next-reset
// window. Callers scope that to a single pass (see CitizenBehavior_CollectStats), in
// which nothing is unmapped, so the cache cannot answer differently than the syscall.
namespace {
struct ReadRegion { uintptr_t base; uintptr_t end; };
// 8 measured the same as 64 (CollectStats guard: 0.166 ms -> ~0.03 ms per citizen
// either way): the surviving cost is the FIRST probe into each engine-heap region,
// which no cache size can avoid. Keep the scan short.
constexpr int kReadCacheSize = 8;
ReadRegion g_readCache[kReadCacheSize] = {};
int        g_readCacheNext = 0;
}  // namespace

void ResetReadableCache() {
    for (int i = 0; i < kReadCacheSize; i++) g_readCache[i].base = g_readCache[i].end = 0;
    g_readCacheNext = 0;
}

bool Readable(const void* p, size_t n) {
    if (!p) return false;
    const uintptr_t a = reinterpret_cast<uintptr_t>(p);
    for (int i = 0; i < kReadCacheSize; i++) {
        const ReadRegion& r = g_readCache[i];
        if (r.end && a >= r.base && a + n <= r.end) return true;
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    DWORD prot = mbi.Protect;
    if (prot & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    const DWORD rd = PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                     PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
    if (!(prot & rd)) return false;
    uintptr_t end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;

    // Remember the readable region (round-robin) — the next probe into the same
    // arena skips the syscall.
    g_readCache[g_readCacheNext].base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    g_readCache[g_readCacheNext].end  = end;
    g_readCacheNext = (g_readCacheNext + 1) % kReadCacheSize;

    return (a + n) <= end;
}

bool Executable(const void* p) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    DWORD prot = mbi.Protect;
    if (prot & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    const DWORD ex = PAGE_EXECUTE | PAGE_EXECUTE_READ |
                     PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (prot & ex) != 0;
}

uint32_t SafeGetTypeId(void* obj) {
    if (!Readable(obj, 8)) return 0;
    // Probe the vptr's table entry for Type() -- the slot is taken from the
    // member, never a literal (B3X_RULES X3) -- then call it by name.
    uintptr_t vt = *reinterpret_cast<uintptr_t*>(obj);
    const long typeSlot = ::Object::CompilerSlotOf(&::Object::Base::Type);
    if (typeSlot < 0) return 0;
    if (!Readable(reinterpret_cast<void*>(vt + static_cast<uintptr_t>(typeSlot) * sizeof(void*)), 8)) return 0;
    if (!Executable(::Object::SlotAddressOf(obj, &::Object::Base::Type))) return 0;
    return reinterpret_cast< ::Object::Base*>(obj)->Type();   // B.3.x bridge (slot 2)
}

void* FindComp(void* entity, uint32_t tid) {
    if (!entity || !tid || !Readable(entity, 0x40)) return nullptr;
    // LEFT: the harnesses hand the entity as void* (it is probed before it is read).
    const ComponentEntity* e = reinterpret_cast<const ComponentEntity*>(entity);
    int count = e->_componentCount;                              // +0x38
    const Object::InternalPtr<Component>* arr = e->_components;  // +0x30
    if (count <= 0 || count > 512 || !Readable(arr, (size_t)count * 8))
        return nullptr;
    for (int i = 0; i < count; ++i) {
        void* comp = arr[i].Get();
        if (SafeGetTypeId(comp) == tid) return comp;
    }
    return nullptr;
}

int WorkAreaCenterCell(void* area) {
    if (GameUpdateLoop::_singleton == nullptr) return -1;
    const uint32_t tidMap = MapComponent::TypeId();   // caller batch CB6: was a lazy by-name cache
    void* found = FindComp(area, tidMap);
    if (!found || !Readable(found, 0x30)) return -1;
    // LEFT: FindComp returns the matched component as void*.
    const MapComponent* mc = reinterpret_cast<const MapComponent*>(found);
    const Map* game = GameUpdateLoop::_singleton->m_game;
    if (!game) return -1;
    int dim = game->size;
    if (dim <= 0) return -1;
    // The footprint words, read signed.
    int16_t tileX = static_cast<int16_t>(mc->tileX0);   // +0x28
    int16_t tileY = static_cast<int16_t>(mc->tileY0);   // +0x2a
    int16_t fpW   = static_cast<int16_t>(mc->tileX1);   // +0x2c
    int16_t fpH   = static_cast<int16_t>(mc->tileY1);   // +0x2e
    int cell = (((int)fpH >> 1) + (int)tileY) * dim + (((int)fpW >> 1) + (int)tileX);
    if (cell < 0 || cell >= dim * dim) return -1;
    return cell;
}

bool CitizenStackIdle(void* citizen) {
    if (!citizen || !Readable(citizen, kCharCompOff + 8)) return false;
    // LEFT: the harnesses hand the citizen component as void* (probed first).
    const CitizenComponent* c = reinterpret_cast<const CitizenComponent*>(citizen);
    const CharacterComponent* charComp = c->character;   // +0x278
    if (!charComp || !Readable(charComp, kActionTopOff + 8)) return false;
    return charComp->currentAction < 0;                 // +0x60
}

int DriveWhenIdle(void* citizen, void* work, int (*driveFn)(void*, void*),
                  const char* tag, int* driveCount, int traceMax) {
    if (!work || !driveFn) return INT_MIN;
    if (!CitizenStackIdle(citizen)) return INT_MIN;

    // CitizenStackIdle proved it readable as a CitizenComponent.
    CitizenComponent* c = reinterpret_cast<CitizenComponent*>(citizen);
    ::Object::InternalPtr<CitizenComponent> citizenIBP(c);
    int rc = driveFn(work, &citizenIBP);

    if (driveCount && *driveCount < traceMax) {
        ++*driveCount;
        const CharacterComponent* charComp = c->character;               // +0x278
        int top = charComp ? charComp->currentAction : -999;             // +0x60
        std::fprintf(stderr, "[%s] DRIVE#%d citizen=%p ret=%d newTop+0x60=%d\n",
                     tag ? tag : "ForceWork", *driveCount, citizen, rc, top);
        std::fflush(stderr);
    }
    return rc;
}

}  // namespace ForceWork

