// standalone/persistobject_probe.cpp
// BANISHED_PERSIST_TRACE=1 — log every Object::InternalFactory::PersistObject call.
//
// WHY: loading the packaged TUTORIAL save faults inside PersistObject
// (Runtime 0x1800bff10) indexing its id map. Every attempt to measure that from
// the caller side was indirect and one of them was outright wrong — a peek that
// read the achievement count through the SERIALIZER's encoding returned
// 0xAAAAAAAA on a save that loads perfectly well, because
// AchievementManager::Persist builds its OWN encoding from stream+0x10 rather
// than using the serializer's. Reading the arguments at the function itself is
// the only measurement that cannot be wrong about which values it is seeing.
//
// Retail body (Ghidra), with the two faulting sites marked:
//     idMap = this[0x10]
//     if (stream[8] == 0) {                       // LOADING
//         slot = idMap[objId]                     // (A) faults if objId is wild
//         if (slot < 0) {
//             walk Factory::_singleton BST by typeId -> desc
//             obj = desc->vt[0x20]()              // (B) faults if the BST missed
//             ...
//         }
//     }
// (A) and (B) are 0xB0 apart, so the crash offset alone says which one hit.
// This probe reports objId against the map's real bounds, so "wild index" and
// "unregistered type" stop being guesses.
//
// Diagnostic only, default OFF: with the env var unset nothing is detoured and
// the load path is byte-identical to an unprobed build.

#include "resolve.h"
#include "runtime_detour.h"
#include "Object/factory_registration.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <windows.h>

namespace {

// Object::InternalFactory::PersistObject
//   Internal* (InternalFactory* this, Stream& s, uint gen, int objId, uint typeId)
constexpr uintptr_t kPersistObject = 0x1800bff10;

// 48 89 5C 24 10 | 48 89 6C 24 18 | 57 | 41 56 | 41 57   = 15 bytes, and the
// next instruction (SUB RSP,0x20) starts at +15, so 15 is a legal patch length.
const uint8_t kPersistPrologue[15] = {
    0x48, 0x89, 0x5C, 0x24, 0x10,
    0x48, 0x89, 0x6C, 0x24, 0x18,
    0x57,
    0x41, 0x56,
    0x41, 0x57,
};
constexpr int kPersistPatchLen = 15;

typedef void* (*PersistObjectFn)(void*, void*, uint32_t, int32_t, uint32_t);
PersistObjectFn g_origPersistObject = nullptr;

bool TraceOn() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_PERSIST_TRACE");
        s = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s != 0;
}

bool Readable(const void* p, size_t n) {
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    const uint8_t* end = (const uint8_t*)mbi.BaseAddress + mbi.RegionSize;
    return (const uint8_t*)p + n <= end;
}

// How many 4-byte entries can we actually read at the id map before running off
// the end of its committed region? That is the real bound on objId, and it is
// what decides whether a given objId is "wild" or merely unexpected.
long IdMapCapacity(const void* map) {
    if (!map) return -1;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(map, &mbi, sizeof(mbi))) return -1;
    if (mbi.State != MEM_COMMIT) return -1;
    const uint8_t* end = (const uint8_t*)mbi.BaseAddress + mbi.RegionSize;
    return (long)((end - (const uint8_t*)map) / 4);
}

long  g_calls   = 0;
long  g_logged  = 0;
long  g_maxLog  = 40;

void* WrapPersistObject(void* self, void* stream, uint32_t gen,
                        int32_t objId, uint32_t typeId) {
    ++g_calls;
    void* idMap = (self && Readable((const uint8_t*)self + 0x10, 8))
                      ? *(void**)((uintptr_t)self + 0x10) : nullptr;
    long cap = IdMapCapacity(idMap);
    const bool loading = stream && Readable((const uint8_t*)stream + 8, 4) &&
                         *(int*)((uintptr_t)stream + 8) == 0;
    const bool wild = (objId < 0) || (cap >= 0 && objId >= cap);

    // Log the first N unconditionally (so the healthy shape is on record, which
    // is what makes an anomaly legible) and then every wild one.
    if (g_logged < g_maxLog || wild) {
        ++g_logged;
        // PersistObject does not merely look an object up: for an id it has not
        // seen it CONSTRUCTS the object and calls obj->vt[0x18](stream), so the
        // object deserializes itself inline, here, from this stream. A desync
        // therefore surfaces on the call AFTER the one whose type read the wrong
        // number of bytes -- which makes the type name of the last good call the
        // single most useful thing this probe can print.
        const char* tn = ::Object::GetTypeNameById(typeId);
        fprintf(stderr, "[Persist] #%ld %s objId=%d typeId=0x%08X (%s) gen=0x%08X "
                        "idMap=%p cap=%ld%s\n",
                g_calls, loading ? "LOAD" : "SAVE", objId, typeId,
                tn ? tn : "?", gen, idMap, cap,
                wild ? "  <== OUT OF RANGE" : "");
        fflush(stderr);
    }
    if (wild) {
        fprintf(stderr, "[Persist] refusing to forward a wild objId — returning "
                        "null instead of faulting (diagnostic build only)\n");
        fflush(stderr);
        return nullptr;
    }
    return g_origPersistObject(self, stream, gen, objId, typeId);
}

} // namespace

extern "C" void InstallPersistObjectProbe() {
    if (!TraceOn()) return;
    uintptr_t addr = ResolveAddr(Module::Runtime, kPersistObject);
    if (!addr) {
        fprintf(stderr, "[Persist] could not resolve InternalFactory::PersistObject\n");
        return;
    }
    g_origPersistObject = reinterpret_cast<PersistObjectFn>(
        RuntimeDetourInstall(addr, reinterpret_cast<void*>(&WrapPersistObject),
                             kPersistPatchLen, kPersistPrologue, kPersistPatchLen,
                             "Object::InternalFactory::PersistObject"));
    if (!g_origPersistObject)
        fprintf(stderr, "[Persist] *** DETOUR FAILED *** probe is OFF\n");
}
