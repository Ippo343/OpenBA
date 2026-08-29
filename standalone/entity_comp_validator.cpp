// standalone/entity_comp_validator.cpp
// BANISHED_COMP_VALIDATE=1 — save-side component-slot validator.
//
// WHY: an in-game Load crashed inside Object::InternalFactory::PersistObject
// executing `idMap[objId]` with objId=0xAAA12C08 / typeId=0xF6AAAAAA — an
// out-of-bounds index built from 0xAA-pattern uninitialised memory. Measured
// facts that place the blame on the SAVE leg:
//   * per-component byte counts on load match the save leg exactly, so the
//     stream is NOT desynced and the cursor is where it should be;
//   * the destination IBP slot reads raw=0 before the call, so the garbage did
//     not come from our load-side array — IBP::Serialize read it OUT of the
//     stream.
// So the save wrote a component record from a slot holding uninitialised
// memory: Object::InternalBasePtr::Serialize takes the id from `garbage[1]` and
// the type from `garbage[0xC]` and writes those numbers out. The real defect is
// upstream in entity construction — an entity whose componentCount includes a
// slot nothing ever populated.
//
// This probe catches it at the moment of writing, naming the entity, its
// description type and the offending index, plus the type of every sibling slot
// so the building can be identified by its component signature.
//
// Diagnostic only: it never modifies the slot, so the faithful save path is
// unchanged whether the probe is on or off.

#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <windows.h>

namespace {

bool ValidateOn() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_COMP_VALIDATE");
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
    const uint8_t* end = static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    return static_cast<const uint8_t*>(p) + n <= end;
}

// A live object: readable, and its vtable pointer points into a mapped image.
bool LooksLikeObject(const void* p) {
    if (!Readable(p, 0x18)) return false;
    const void* vt = *reinterpret_cast<void* const*>(p);
    if (!Readable(vt, 8)) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(vt, &mbi, sizeof(mbi))) return false;
    return mbi.Type == MEM_IMAGE;
}

const char* TypeNameOf(const void* obj) {
    if (!LooksLikeObject(obj)) return nullptr;
    typedef uint32_t (*fn_gtid_t)(const void*);
    const uintptr_t vt = *reinterpret_cast<const uintptr_t*>(obj);
    if (!Readable(reinterpret_cast<void*>(vt + 0x10), 8)) return nullptr;
    fn_gtid_t f = *reinterpret_cast<fn_gtid_t*>(vt + 0x10);
    if (!f) return nullptr;
    return ::Object::GetTypeNameById(f(obj));
}

} // namespace

// Called from ComponentEntity_Serialize on the SAVE leg, before the component
// IBP loop writes anything. Returns the number of bad slots found.
extern "C" int EntityCompValidate(void* entity, void* desc, void* ibpArr, int count) {
    if (!ValidateOn() || !ibpArr || count <= 0) return 0;

    // Self-test, once: prove the detector DISCRIMINATES before trusting its
    // silence. A validator that says "no bad slots" is worthless if it would
    // also stay quiet on an obviously bogus pointer.
    static bool s_selftest = false;
    if (!s_selftest) {
        s_selftest = true;
        const bool bogus = LooksLikeObject(reinterpret_cast<void*>(0xAAAAAAAAull));
        const bool real  = LooksLikeObject(entity);
        fprintf(stderr, "[CompValidate] self-test: bogus(0xAAAAAAAA)=%d (want 0)  "
                        "realEntity=%d (want 1) -> detector %s\n",
                (int)bogus, (int)real,
                (!bogus && real) ? "DISCRIMINATES" : "IS BROKEN — ignore its silence");
        fflush(stderr);
    }

    int bad = 0;
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(
            reinterpret_cast<uint8_t*>(ibpArr) + static_cast<size_t>(i) * 8);
        if (!Readable(slot, 8)) { ++bad; continue; }
        void* obj = *slot;
        // A null slot is legal (IBP::Serialize writes "absent"); only a NON-null
        // value that is not a live object is the bug we are hunting.
        if (obj && !LooksLikeObject(obj)) ++bad;
    }
    if (!bad) return 0;

    static int s_reported = 0;
    if (s_reported >= 12) return bad;
    ++s_reported;

    fprintf(stderr, "[CompValidate] BAD ENTITY ent=%p desc=%p (%s) count=%d badSlots=%d\n",
            entity, desc, TypeNameOf(desc) ? TypeNameOf(desc) : "?", count, bad);
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(
            reinterpret_cast<uint8_t*>(ibpArr) + static_cast<size_t>(i) * 8);
        void* obj = Readable(slot, 8) ? *slot : reinterpret_cast<void*>(~0ull);
        const char* n = obj ? TypeNameOf(obj) : "(null)";
        // Dump the two words IBP::Serialize would read off a bad pointer — those
        // are the exact numbers that end up in the stream as objId/typeId.
        if (obj && !n && Readable(obj, 0x10)) {
            fprintf(stderr, "[CompValidate]   slot %2d = %p  *** NOT AN OBJECT *** "
                    "wouldWrite objId=%08x typeId=%08x\n", i, obj,
                    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(obj) + 8),
                    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(obj) + 0xC));
        } else if (obj && !n) {
            fprintf(stderr, "[CompValidate]   slot %2d = %p  *** NOT AN OBJECT (unreadable) ***\n",
                    i, obj);
        } else {
            fprintf(stderr, "[CompValidate]   slot %2d = %p  %s\n", i, obj, n ? n : "(null)");
        }
    }
    fflush(stderr);
    return bad;
}
