// ============================================================================
// runtime/src/object/external_load_probe.cpp — Wave 11 follow-up 2, lane IOSTREAM
//   The oracle for `Object::External::Load` (retail 0x1800bc620).
// ============================================================================
//
// WHY A LIVE REPLAY IS POSSIBLE HERE AND WAS NOT FOR `SaveText`
// -------------------------------------------------------------
// `Load` is STATIC and takes a PATH. It does not touch `ExternalFactory`'s
// resource tree — its caller (`FindOrLoad`) does that afterwards — so calling
// it a second time on the same path with `existing == nullptr` builds a SECOND,
// INDEPENDENT object out of the same file. That is a replay, not a destructive
// mutation, and it is what makes an ours-vs-theirs comparison possible at a
// real call site instead of only on a synthetic subject.
//
// (Contrast `Collection::Set::Insert`, where lane RES could not replay because
// retail's second insert of the same key returns early and the comparison goes
// vacuously green — the `KeyMap::RegisterInput` trap. Nothing here short-
// circuits on a repeat: the file is re-opened and re-parsed from scratch.)
//
// THEIRS IS `ResolveExportRaw`, which never consults the port registry — an
// oracle that reached the DLL through `ResolveExport` would be handed OUR body
// once registration has happened and would compare ours with ours
// (reference_resolveaddr_consults_port_registry_first.md).
//
// WHAT IS COMPARED, AND WHY THAT IS THE WHOLE OUTPUT
// --------------------------------------------------
// Everything `Load` computes ends up in the object it returns. So, per subject:
//
//   * both sides returned non-null;
//   * the two objects have the same TYPE ID (their own virtual at +0x10) —
//     this is the object-creation path's answer, i.e. that the type id read out
//     of the file selected the same `BaseTypeInfo::factoryFn`;
//   * the same VERSION (their virtual at +0x18);
//   * the same DEPENDENCY COUNT; and
//   * per dependency element, the same `Filename` TEXT and the same two int32s
//     at +0x08 and +0x0c.
//
// The dependency array is the part worth having: it is the only place `Load`
// writes a variable amount of data, it is where the element stride and the two
// int offsets live, and it is the one loop a resource with no dependencies
// leaves unentered. `deps=` in the report is that loop's own count.
//
// POSITIVE CONTROL: `deps=` and `subjects=`. A run in which every subject had
// zero dependencies would compare four scalars per object and report green
// while the loop under test never executed, so a cell with `deps=0` is called
// out rather than passed (reference_probe_needs_an_effectiveness_control.md).
//
// THE DUPLICATE IS DESTROYED, through its own deleting destructor (vtable slot
// 0 with flags=1), which is how everything else in this engine frees an object
// the factory made. Leaking one External per subject would be a slow leak
// nobody would attribute to a diff harness.
//
// NEGATIVE CONTROLS — `BANISHED_RT_EXTLOAD_NEGCTL=1..2`
// ------------------------------------------------------
// Both perturb the BODY (external_load.cpp), not a copy of it, both attend at
// their own site, and both produce a COMPARABLE DIVERGENCE rather than
// undefined behaviour — neither changes a pointer, a size or a stride, so
// neither can write out of bounds (WAVE11_LANE_OBJEXT.md §10.2, the control
// that crashed the run instead of diverging).
//
//   1  the two per-element int32s read in the OPPOSITE ORDER (+0x0c before
//      +0x08). Attends once per dependency element, so it can only catch on a
//      subject that HAS dependencies — which is exactly why `deps=` is the
//      positive control.
//   2  the dependency COUNT serialized through the int32 overload (+0x98)
//      instead of the uint32 one (+0xa0). Whether this catches is a fact about
//      whether the two overloads share a wire format, and the answer is
//      reported either way rather than the control being dropped
//      (reference_union_control_hides_a_blind_screen.md).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../system/string.h"
#include "../../funcs/rt_port.h"
#include "../../types/object/TypeInfo.h"
#include "../../types/object/External.h"

namespace RtObject {

// Set by this probe, read by the body. Defined in external_load.cpp.
extern int      g_lcCtl;
extern uint64_t g_lcAtt;

namespace {

using RtSystem::Filename;

using LoadFn = External* (*)(const Filename*, External*);

const char* const kLoad =
    "?Load@External@Object@@SAPEAV12@AEBVFilename@System@@PEAV12@@Z";

// Bounded on purpose: each subject re-opens, re-decompresses and re-parses a
// resource file and then constructs and destroys a whole object. Eight is
// enough to enter the dependency loop many times (a boot run reports
// deps=1274 across 2847 loads) without doubling the boot.
constexpr int kMaxSubjects = 8;

int      g_subjects = 0;
int      g_checked  = 0;
int      g_failed   = 0;
uint64_t g_deps     = 0;
bool     g_started  = false;
bool     g_done     = false;

void CheckI(bool ok, const char* what, long long got, long long want) {
    ++g_checked;
    if (!ok) {
        ++g_failed;
        printf("[LoadProbe]   FAIL %-28s ours=%lld theirs=%lld\n", what, got, want);
    }
}

void* VSlot(const void* obj, size_t byteOffset) {
    void** vtbl = *reinterpret_cast<void** const*>(obj);
    return vtbl[byteOffset / sizeof(void*)];
}

uint32_t TypeIdOf(const External* o) {
    using Fn = uint32_t (*)(const External*);
    return reinterpret_cast<Fn>(VSlot(o, 0x10))(o);
}
int32_t VersionOf(const External* o) {
    using Fn = int32_t (*)(const External*);
    return reinterpret_cast<Fn>(VSlot(o, 0x18))(o);
}

}  // namespace

// ============================================================================
// Called from the tail of RtObject_External_Load, with the object OUR body just
// produced. Only the `existing == nullptr` case is compared: when the caller
// supplies an object, retail skips the whole creation branch and the two sides
// are not building comparable things.
// ============================================================================
void RtObjectLoadProbeStep(const Filename* path, External* ours) {
    if (g_done) return;
    if (ours == nullptr || path == nullptr) return;

    if (!g_started) {
        g_started = true;
        const char* e = std::getenv("BANISHED_RT_EXTLOAD_NEGCTL");
        g_lcCtl = (e && e[0]) ? std::atoi(e) : 0;
        printf("[LoadProbe] ctl=%d -- replaying each load against the shipped DLL\n",
                    g_lcCtl);
    }

    auto theirs = reinterpret_cast<LoadFn>(ResolveExportRaw(kLoad));
    if (theirs == nullptr) {
        printf("[LoadProbe] FATAL phantom export name: %s\n", kLoad);
        g_done = true;
        return;
    }

    // THE REPLAY. `existing = nullptr`, so retail builds its own object out of
    // the same file through the same factory pointer.
    External* dup = theirs(path, nullptr);

    ++g_subjects;
    CheckI(dup != nullptr, "theirs returned non-null", (long long)(uintptr_t)ours,
           (long long)(uintptr_t)dup);

    if (dup != nullptr) {
        CheckI(TypeIdOf(ours) == TypeIdOf(dup), "type id (+0x10)",
               TypeIdOf(ours), TypeIdOf(dup));
        CheckI(VersionOf(ours) == VersionOf(dup), "version (+0x18)",
               VersionOf(ours), VersionOf(dup));
        CheckI(ours->_refs._size == dup->_refs._size, "dependency count",
               ours->_refs._size, dup->_refs._size);

        const int32_t n = (ours->_refs._size < dup->_refs._size)
                              ? ours->_refs._size : dup->_refs._size;
        const uint8_t* a = reinterpret_cast<const uint8_t*>(ours->_refs._data);
        const uint8_t* b = reinterpret_cast<const uint8_t*>(dup->_refs._data);
        for (int32_t i = 0; i < n; ++i, a += 0x10, b += 0x10) {
            ++g_deps;
            const char* sa = *reinterpret_cast<const char* const*>(a);
            const char* sb = *reinterpret_cast<const char* const*>(b);
            const bool same = (sa == nullptr && sb == nullptr) ||
                              (sa != nullptr && sb != nullptr && std::strcmp(sa, sb) == 0);
            ++g_checked;
            if (!same) {
                ++g_failed;
                printf("[LoadProbe]   FAIL dep[%d] filename ours=%s theirs=%s\n",
                            i, sa ? sa : "<null>", sb ? sb : "<null>");
            }
            CheckI(*reinterpret_cast<const int32_t*>(a + 0x08) ==
                   *reinterpret_cast<const int32_t*>(b + 0x08), "dep int (+0x08)",
                   *reinterpret_cast<const int32_t*>(a + 0x08),
                   *reinterpret_cast<const int32_t*>(b + 0x08));
            CheckI(*reinterpret_cast<const int32_t*>(a + 0x0c) ==
                   *reinterpret_cast<const int32_t*>(b + 0x0c), "dep int (+0x0c)",
                   *reinterpret_cast<const int32_t*>(a + 0x0c),
                   *reinterpret_cast<const int32_t*>(b + 0x0c));
        }

        // Destroy the duplicate through its own deleting destructor -- slot 0
        // with flags = 1, the same call retail makes on the Serializer at
        // 0x1800bc841 and the one every factory-built object is freed with.
        using DeleteFn = void* (*)(External*, int);
        reinterpret_cast<DeleteFn>(VSlot(dup, 0))(dup, 1);
    }

    if (g_subjects < kMaxSubjects) return;

    g_done = true;
    const char* effect = (g_deps == 0)
        ? " -- WEAK: no subject had dependencies, the replay loop never ran"
        : "";
    if (g_lcCtl == 0) {
        printf("[LoadProbe] VERDICT ctl=0 subjects=%d checked=%d failed=%d deps=%llu  %s%s\n",
                    g_subjects, g_checked, g_failed, (unsigned long long)g_deps,
                    g_failed == 0 ? "PASS" : "*** MISMATCH ***", effect);
    } else if (g_failed == 0) {
        printf("[LoadProbe] VERDICT ctl=%d subjects=%d checked=%d failed=0 att=%llu "
                    "deps=%llu  control NOT CAUGHT%s\n",
                    g_lcCtl, g_subjects, g_checked, (unsigned long long)g_lcAtt,
                    (unsigned long long)g_deps, effect);
    } else if (g_lcAtt == 0) {
        printf("[LoadProbe] VERDICT ctl=%d subjects=%d checked=%d failed=%d att=0  "
                    "CAUGHT BUT NOT VIA ITS OWN MECHANISM -- NOT BANKED\n",
                    g_lcCtl, g_subjects, g_checked, g_failed);
    } else {
        printf("[LoadProbe] VERDICT ctl=%d subjects=%d checked=%d failed=%d att=%llu "
                    "deps=%llu  control CAUGHT\n",
                    g_lcCtl, g_subjects, g_checked, g_failed,
                    (unsigned long long)g_lcAtt, (unsigned long long)g_deps);
    }
    std::fflush(stdout);
    g_lcCtl = 0;   // leave the body unperturbed for the rest of the run
}

}  // namespace RtObject
