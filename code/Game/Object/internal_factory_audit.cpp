// code/Game/Object/internal_factory_audit.cpp
// See internal_factory_audit.h for the table layout and the two corruption modes.

#include "Object/internal_factory_audit.h"
#include "Game/force_work.h"
#include "resolve.h"
#include "object/Internal.h"   // ENDGAME B.3.9 SS2: Object::InternalFactory::_singleton

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int kIdxOff = 0x08;   // Internal::_factoryIndex
constexpr int kGenOff = 0x0c;   // Internal::_generation

}  // namespace

namespace ObjAudit {

void* Factory() {
    // The singleton is created once at engine init and never moves, so the
    // VirtualQuery probes only run until the first successful resolve. This keeps
    // IsRegistered() cheap enough to call from a free path.
    static void* s_cached = nullptr;
    if (s_cached) return s_cached;

    // Object::InternalFactory::_singleton (Runtime 0x1801fc6d8), the class static.
    void* f = ::Object::InternalFactory::_singleton;
    // factory header is {ptr@+0, count@+8, cap@+0xc} plus a Random at +0x20.
    if (!f || !ForceWork::Readable(f, 0x24)) return nullptr;
    s_cached = f;
    return f;
}

// Fetch table base + count, sanity-checked. Returns false if unusable.
static bool Table(void*** outTable, int* outCount) {
    void* f = Factory();
    if (!f) return false;
    uint8_t* fb = reinterpret_cast<uint8_t*>(f);
    void** tbl  = *reinterpret_cast<void***>(fb);
    int    cnt  = *reinterpret_cast<int*>(fb + 8);
    if (cnt < 0 || cnt > (1 << 24)) return false;              // absurd → don't walk it
    if (cnt > 0 && !ForceWork::Readable(tbl, (size_t)cnt * 8)) return false;
    *outTable = tbl;
    *outCount = cnt;
    return true;
}

int Count() {
    void** tbl = nullptr; int cnt = 0;
    if (!Table(&tbl, &cnt)) return -1;
    return cnt;
}

// Cheap variant used on the free path: assumes `obj` is a live, readable object
// (the caller is about to free it, so it must be), and only touches the factory
// header + one table slot. No VirtualQuery once the factory is cached.
bool IsRegistered(const void* obj) {
    if (!obj) return false;
    void* f = Factory();
    if (!f) return false;
    uint8_t* fb = reinterpret_cast<uint8_t*>(f);
    void** tbl  = *reinterpret_cast<void***>(fb);
    int    cnt  = *reinterpret_cast<int*>(fb + 8);
    if (!tbl || cnt <= 0 || cnt > (1 << 24)) return false;
    int idx = *reinterpret_cast<const int*>(reinterpret_cast<const uint8_t*>(obj) + kIdxOff);
    if (idx < 0 || idx >= cnt) return false;
    return tbl[idx] == obj;
}

}  // namespace ObjAudit
