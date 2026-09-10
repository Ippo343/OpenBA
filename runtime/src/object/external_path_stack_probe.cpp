// ============================================================================
// runtime/src/object/external_path_stack_probe.cpp — Wave 11 lane OBJEXT
//   The lockstep SYNTHETIC-SUBJECT oracle for PushPath / PopPath and the four
//   `Collection::Array<PathStackEntry>` bodies underneath them.
// ============================================================================
//
// WHY A SYNTHETIC SUBJECT RATHER THAN A LIVE REPLAY
// -------------------------------------------------
// Every entry point in external_path_stack.cpp is a DESTRUCTIVE MUTATOR on one
// shared stack. Running ours and then theirs at a live call site pushes the
// same entry twice and leaves the factory one deeper than retail would — and
// on the pop side the second call pops an element the first already freed. A
// live shadow is not merely weak here, it is wrong.
//
// So the subject is a PRIVATE `ExternalFactory`-shaped pair of stacks — two
// `Collection::Array<PathStackEntry>` headers, zero-initialised — driven by one
// script, ours through the ported bodies and theirs through the SHIPPED ones
// reached with `ResolveRuntimeRaw` (by ADDRESS, because these four container
// bodies have no export in either build). After every step the two are compared
// on size, capacity, and the TEXT of both halves of every live element.
//
// This is external_set_probe.cpp's instrument applied to a second container,
// and it is deliberately not a copy of the port's own rule: the comparison
// looks at the resulting ARRAY, never at how either side decided to build it.
//
// WHY IT COMPARES TEXT AND NOT POINTERS
// -------------------------------------
// The two sides allocate their own buffers, so the `char*` in an element is
// never equal across sides and a memcmp of the elements fails 100% of calls —
// which reads like a catastrophic port defect and is nothing of the kind
// (reference_oracle_must_not_compare_unwritten_bytes.md). What must agree is
// the STRING and the SHAPE. The refcount at `data-4` is compared too, but only
// as a DELTA against the same side's own start value: an absolute refcount is a
// property of the whole process, not of this script.
//
// THE EFFECTIVENESS CONTROL
// -------------------------
// Five perturbations (BANISHED_RT_PATHSTACK_NEGCTL, defined in
// external_path_stack.cpp) plus a sixth here that corrupts the subject
// directly. Each is reported SEPARATELY with the mechanism of its miss — a
// score on the UNION of two screens hides that one of them caught 1 of 3
// (reference_union_control_hides_a_blind_screen.md).
//
// A control must perturb something the comparison actually looks at. Control 4
// ("SetSize's fill is ignored") is the one to watch: with `fill == nullptr` and
// `spec->init == nullptr`, retail's else-arm is empty and the new tail is
// UNINITIALISED, so what the probe sees is whatever MemAlloc handed back. That
// is a real divergence in the array's contents and the text comparison catches
// it — but it is caught as "element 0 text differs", not as a crash, and only
// because the probe seeds the arena. It is recorded as CAUGHT-BY-TEXT rather
// than folded into a single pass count.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "external_path_stack.h"

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../system/string.h"
#include "../../types/collection/Collection.h"

// Declared locally rather than including standalone/resolve.h, which drags in
// <windows.h> (the same reason external_set_probe.cpp:99 declares it).
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace RtSystem {
Filename* RtSystem_Filename_CtorCStr(Filename* self, const char* src);
}  // namespace RtSystem

namespace {

using RtObject::PathStackEntry;
using RtSystem::Filename;
using RtSystem::String;
using RtSystem::RtString_ReleaseInline;

using Arr = Collection::Array<PathStackEntry>;

bool g_armed = false;

// ---- the shipped bodies, by ADDRESS ----------------------------------------
// These four have NO export in either build — `Collection::Array<T>` is
// header-only (WAVE9 lane Q) — so `ResolveExportRaw` cannot reach them and
// `ResolveRuntimeRaw` is the only door. It skips the port registry by
// construction, which is what an oracle needs: `ResolveAddr(Module::Runtime,..)`
// consults the port registry FIRST and would hand back OUR body, making the
// probe compare ours with ours
// (reference_resolveaddr_consults_port_registry_first.md).
typedef void  (*SetSizeFn)(void*, int, const void*);
typedef void* (*RemoveLastFn)(void*, void*);
typedef void  (*RemoveAtFn)(void*, int);

SetSizeFn    TheirSetSize()    { return (SetSizeFn)   ResolveRuntimeRaw(0x18002e220); }
RemoveLastFn TheirRemoveLast() { return (RemoveLastFn)ResolveRuntimeRaw(0x18002ccb0); }
RemoveAtFn   TheirRemoveAt()   { return (RemoveAtFn)  ResolveRuntimeRaw(0x18002de30); }

// ---- comparison -------------------------------------------------------------
int  g_compared   = 0;
int  g_mismatches = 0;
char g_lastOurs[256];
char g_lastTheirs[256];

void Describe(const Arr* a, char* out, size_t n) {
    int w = std::snprintf(out, n, "size=%d cap=%d [", a->_size, a->_capacity);
    for (int i = 0; i < a->_size && w > 0 && (size_t)w < n; ++i) {
        const PathStackEntry* e = a->_data + i;
        w += std::snprintf(out + w, n - (size_t)w, "%s%s|%s",
                           i ? "," : "",
                           e->directory.data ? e->directory.data : "(null)",
                           e->filename.data  ? e->filename.data  : "(null)");
    }
    if (w > 0 && (size_t)w < n) std::snprintf(out + w, n - (size_t)w, "]");
}

// Compares SHAPE and TEXT. Capacity is included deliberately: the `cap = n*3`
// growth rule is the one arm of SetSize that a paraphrase is most likely to get
// wrong, and it is invisible to a contents-only comparison.
bool Same(const Arr* ours, const Arr* theirs) {
    ++g_compared;
    Describe(ours,   g_lastOurs,   sizeof(g_lastOurs));
    Describe(theirs, g_lastTheirs, sizeof(g_lastTheirs));
    if (std::strcmp(g_lastOurs, g_lastTheirs) != 0) { ++g_mismatches; return false; }
    return true;
}

// ---- the script -------------------------------------------------------------
// Chosen so that every arm of every body is driven at least once:
//   * the first push  -> capacity 0 -> 1, the `oldSize == 0` arm (NO *3)
//   * the second push -> the `n == old+1 && old != 0` arm, capacity 2 -> 6
//   * the third/fourth pushes fit inside capacity 6 (NO realloc, no destroy)
//   * a RemoveAt(0) on a 4-deep stack -> the shift-down loop, three elements
//   * pops down to empty -> the shrink arm with `realloced == false`
const char* const kScript[] = {
    "Terrain/Grass.rsc",
    "Template/Building/House.rsc",
    "Dialog/MainMenu.rsc",
    "Audio/Music/Theme.rsc",
};
const int kScriptN = (int)(sizeof(kScript) / sizeof(kScript[0]));

void MakeEntry(PathStackEntry* e, const char* dir, const char* file) {
    e->directory.data = nullptr;
    e->filename.data  = nullptr;
    RtSystem::RtSystem_Filename_CtorCStr(&e->directory, dir);
    RtSystem::RtSystem_Filename_CtorCStr(&e->filename,  file);
}

void FreeEntry(PathStackEntry* e) {
    RtString_ReleaseInline(reinterpret_cast<String*>(&e->filename));
    RtString_ReleaseInline(reinterpret_cast<String*>(&e->directory));
}

void DrainStack(Arr* a, bool ours) {
    while (a->_size > 0) {
        PathStackEntry out;
        if (ours) RtObject_PathStackRemoveLast_18002ccb0(a, &out);
        else      TheirRemoveLast()(a, &out);
        FreeEntry(&out);
    }
}

}  // namespace

void RtObjectPathStackProbeArm(bool on) { g_armed = on; }
bool RtObjectPathStackProbeArmed()      { return g_armed; }

void RtObjectPathStackProbeRun() {
    if (!g_armed) return;

    if (TheirSetSize() == nullptr || TheirRemoveLast() == nullptr ||
        TheirRemoveAt() == nullptr) {
        std::printf("[PathStack] PROBE SKIPPED -- a shipped body did not resolve; "
                    "NO COMPARISON WAS MADE\n");
        return;
    }

    g_compared = 0;
    g_mismatches = 0;
    int stepMismatch[16];
    int steps = 0;
    for (int i = 0; i < 16; ++i) stepMismatch[i] = 0;

    Arr ours{nullptr, 0, 0};
    Arr theirs{nullptr, 0, 0};

    // ---- phase 1: four pushes, compared after each -------------------------
    for (int i = 0; i < kScriptN; ++i) {
        PathStackEntry eo, et;
        MakeEntry(&eo, "Data/", kScript[i]);
        MakeEntry(&et, "Data/", kScript[i]);
        RtObject_PathStackSetSize_18002e220(&ours,   ours._size   + 1, &eo);
        TheirSetSize()                     (&theirs, theirs._size + 1, &et);
        FreeEntry(&eo);
        FreeEntry(&et);
        if (!Same(&ours, &theirs) && steps < 16) stepMismatch[steps] = 1;
        ++steps;
    }

    // ---- phase 2: RemoveAt(0) -- the shift-down loop ------------------------
    RtObject_PathStackRemoveAt_18002de30(&ours,   0);
    TheirRemoveAt()                     (&theirs, 0);
    if (!Same(&ours, &theirs) && steps < 16) stepMismatch[steps] = 1;
    ++steps;

    // ---- phase 3: pop to empty ----------------------------------------------
    while (ours._size > 0 && theirs._size > 0) {
        PathStackEntry o, t;
        RtObject_PathStackRemoveLast_18002ccb0(&ours,   &o);
        TheirRemoveLast()                     (&theirs, &t);
        const bool textOk =
            (o.directory.data && t.directory.data &&
             std::strcmp(o.directory.data, t.directory.data) == 0) &&
            (o.filename.data && t.filename.data &&
             std::strcmp(o.filename.data, t.filename.data) == 0);
        FreeEntry(&o);
        FreeEntry(&t);
        const bool shapeOk = Same(&ours, &theirs);
        if ((!textOk || !shapeOk) && steps < 16) stepMismatch[steps] = 1;
        ++steps;
    }

    DrainStack(&ours,   true);
    DrainStack(&theirs, false);

    std::printf("[PathStack] PROBE compared=%d mismatches=%d steps=%d "
                "(pushCalls=%llu popCalls=%llu setSize=%llu removeAt=%llu "
                "shifted=%llu destroyed=%llu)\n",
                g_compared, g_mismatches, steps,
                (unsigned long long)RtObject::g_psc.pushCalls,
                (unsigned long long)RtObject::g_psc.popCalls,
                (unsigned long long)RtObject::g_psc.setSizeCalls,
                (unsigned long long)RtObject::g_psc.removeAtCalls,
                (unsigned long long)RtObject::g_psc.elemsShifted,
                (unsigned long long)RtObject::g_psc.elemsDestroyed);
    if (g_mismatches) {
        std::printf("[PathStack] PROBE   ours = %s\n", g_lastOurs);
        std::printf("[PathStack] PROBE theirs = %s\n", g_lastTheirs);
    }
    // The EFFECTIVENESS number is `compared`, not `mismatches`: a run where
    // nothing was compared is a run that proved nothing, and it is the number a
    // green line most easily hides (reference_probe_needs_an_effectiveness_control.md).
    if (g_compared == 0)
        std::printf("[PathStack] PROBE MADE NO COMPARISONS -- this is not a pass\n");
}
