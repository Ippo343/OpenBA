// ============================================================================
// standalone/rt_selftest.cpp — Wave 0 acceptance test for the Runtime port seam
// ============================================================================
//
// Wave 0 delivers three mechanisms and no ported code: the registry seam
// (RegisterRuntimeExport / RegisterRuntimeAddr), the detour installer
// (standalone/runtime_detour.h), and the differential oracle
// (runtime/funcs/rt_diff.h). PLAN.md §4.2 says not to parallelise past Wave 2
// until each of those has caught a real divergence — but before any of that,
// each has to be shown to work at all. This file is that showing.
//
// Everything here is gated on BANISHED_RT_SELFTEST and does nothing otherwise.
// Run it with:
//
//     BANISHED_RT_SELFTEST=1 BANISHED_RT_DIFF=1 make sa-run-menu
//     grep RtSelfTest "C:/GOG Games/Banished/standalone_error.log"
//
// THE TEST SUBJECT is System::Random. It is exported, it is pure with respect
// to its own object, and it is a 0x9C4-byte Mersenne-twister-shaped state
// (engine/types/GameUpdateLoop.h:114) whose sequence can be snapshotted and
// replayed by memcpy — which makes it deterministic without knowing how it is
// seeded. Nothing in the game holds the object we build here.
//
// Test 3 deliberately leaves a NULL PORT registered for the rest of the run:
// Random::Rand routed through a pass-through thunk of ours. If the seam is
// sound the game plays identically with the self-test on; if it is not, an
// otherwise-normal `sa-run` diverges and that is exactly the signal we want
// before a real port depends on the same path.
// ============================================================================

#include "resolve.h"
#include "runtime_detour.h"
#include "rt_diff.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// System::Random — a 0x9C4-byte state. We only ever memcpy it and call two of
// its methods, so no layout beyond the size is needed.
constexpr size_t kRandomSize = 0x9C4;

const char* const kRandomCtor = "??0Random@System@@QEAA@XZ";
const char* const kRandomRand = "?Rand@Random@System@@QEAAII@Z";

typedef void*        (*RandomCtorFn)(void* self);
typedef unsigned int (*RandomRandFn)(void* self, unsigned int mod);

// The original, resolved Raw so the thunk below cannot recurse into itself once
// the null port is registered.
RandomRandFn g_origRand = nullptr;
uint64_t     g_thunkCalls = 0;

// The null port: a reimplementation that reimplements nothing. Registering it
// proves our ~1,180 call sites really do flip to whatever we register, without
// changing a single result.
unsigned int NullPortRand(void* self, unsigned int mod) {
    g_thunkCalls++;
    return g_origRand(self, mod);
}

// Detour subject: same function, patched in the DLL so its INTERNAL callers
// come to us too. Removed again at the end of the test.
RtDetour     g_detour = {};
RandomRandFn g_detourTramp = nullptr;
uint64_t     g_detourCalls = 0;

unsigned int DetouredRand(void* self, unsigned int mod) {
    g_detourCalls++;
    return g_detourTramp(self, mod);
}

bool SelfTestEnabled() {
    const char* e = getenv("BANISHED_RT_SELFTEST");
    return e && *e && strcmp(e, "0") != 0;
}

void Fail(const char* what) {
    fprintf(stderr, "[RtSelfTest] FAIL: %s\n", what);
    fflush(stderr);
}
void Pass(const char* what) {
    fprintf(stderr, "[RtSelfTest] pass: %s\n", what);
    fflush(stderr);
}

// Draw `n` values into `out` from a snapshot of `state`, leaving `state` as it
// was. Deterministic replay is the whole basis of the comparisons below.
void DrawFromSnapshot(const void* snapshot, void* state, RandomRandFn rand,
                      unsigned int* out, int n) {
    memcpy(state, snapshot, kRandomSize);
    for (int i = 0; i < n; i++) out[i] = rand(state, 1000);
}

} // namespace

void RtSelfTest() {
    if (!SelfTestEnabled()) return;

    fprintf(stderr, "[RtSelfTest] ==== Runtime port seam acceptance test ====\n");
    fflush(stderr);

    // ---- Resolve the subject, Raw ------------------------------------------
    RandomCtorFn ctor = reinterpret_cast<RandomCtorFn>(ResolveExportRaw(kRandomCtor));
    g_origRand = reinterpret_cast<RandomRandFn>(ResolveExportRaw(kRandomRand));
    if (!ctor || !g_origRand) {
        Fail("could not resolve System::Random ctor/Rand from the DLL");
        return;
    }

    // Heap, not stack: 0x9C4 bytes twice over is more than this frame should hold.
    void* state = calloc(1, kRandomSize);
    void* snapshot = calloc(1, kRandomSize);
    if (!state || !snapshot) { Fail("allocation"); free(state); free(snapshot); return; }
    ctor(state);
    memcpy(snapshot, state, kRandomSize);

    constexpr int kDraws = 16;
    unsigned int ref[kDraws], got[kDraws];
    DrawFromSnapshot(snapshot, state, g_origRand, ref, kDraws);

    // Replay determinism is the premise of every comparison below; if it does
    // not hold, nothing after this means anything.
    DrawFromSnapshot(snapshot, state, g_origRand, got, kDraws);
    if (memcmp(ref, got, sizeof(ref)) != 0) {
        Fail("System::Random is not replayable from a snapshot — the rest of this "
             "test cannot conclude anything");
        free(state); free(snapshot);
        return;
    }
    Pass("subject is deterministic under snapshot replay");

    // ---- Test 1: Raw vs registry precedence, on a name nothing else uses ----
    // A name the DLL does not export: ResolveExportRaw must fail, ResolveExport
    // must return exactly what we registered.
    const char* kFake = "?RtSelfTestOnly@@YAXXZ";
    if (ResolveExportRaw(kFake) != nullptr) {
        Fail("the DLL unexpectedly exports the self-test's placeholder name");
    } else {
        RegisterRuntimeExport(kFake, reinterpret_cast<void*>(&NullPortRand));
        if (ResolveExport(kFake) != reinterpret_cast<void*>(&NullPortRand))
            Fail("ResolveExport did not return the registered port");
        else if (ResolveExportRaw(kFake) != nullptr)
            Fail("ResolveExportRaw was contaminated by the registry");
        else
            Pass("registry precedence: ResolveExport sees the port, Raw does not");
    }

    // ---- Test 2: detour + trampoline + removal -----------------------------
    // Patch the DLL's own Rand. A correct install means: the trampoline still
    // produces the original sequence, our wrapper is the one being entered, and
    // removal puts the function back byte for byte.
    uintptr_t randAddr = reinterpret_cast<uintptr_t>(g_origRand);
    if (!InstallRuntimeDetourExport("selftest:System::Random::Rand", kRandomRand,
                                    reinterpret_cast<void*>(&DetouredRand), &g_detour)) {
        Fail("detour install");
    } else {
        g_detourTramp = reinterpret_cast<RandomRandFn>(g_detour.trampoline);
        g_detourCalls = 0;

        // Call through the ORIGINAL address, which now jumps to DetouredRand.
        DrawFromSnapshot(snapshot, state, g_origRand, got, kDraws);

        if (g_detourCalls != kDraws)
            Fail("detour was installed but the wrapper was not entered");
        else if (memcmp(ref, got, sizeof(ref)) != 0)
            Fail("detoured call produced a DIFFERENT sequence — the displaced "
                 "prologue or the trampoline is wrong");
        else
            Pass("detour: wrapper entered, trampoline reproduces the original exactly");

        if (FindRuntimeDetourByAddress(randAddr) == nullptr)
            Fail("FindRuntimeDetourByAddress did not recognise its own target");

        RemoveDetour(&g_detour);
        DrawFromSnapshot(snapshot, state, g_origRand, got, kDraws);
        if (memcmp(ref, got, sizeof(ref)) != 0)
            Fail("removal did not restore the original function");
        else
            Pass("detour removal restores the DLL byte for byte");
    }

    // ---- Test 3: the differential oracle -----------------------------------
    // Two comparisons with a known answer: one that must agree and one that must
    // not. With BANISHED_RT_DIFF=1 the divergence has to appear in the log; with
    // it unset neither line costs anything. Read the summary at exit to confirm
    // exactly one divergence was counted here.
    {
        RT_DIFF_BEGIN("selftest::agree");
        unsigned int ours = ref[0];
        RT_DIFF_SCALAR(ours, ref[0]);
    }
    {
        RT_DIFF_BEGIN("selftest::diverge (EXPECTED — this one is supposed to fail)");
        unsigned int ours = ref[0] ^ 0xFFu;
        RT_DIFF_SCALAR(ours, ref[0]);
    }
    if (RtDiffEnabled())
        Pass("diff harness exercised — expect exactly one divergence, in "
             "selftest::diverge");
    else
        fprintf(stderr, "[RtSelfTest] note: BANISHED_RT_DIFF unset, the oracle was "
                        "compiled in but idle\n");

    // ---- Test 4: leave a null port live for the rest of the run ------------
    // Registered AFTER the detour test so g_origRand is still the DLL's code.
    // Every later ResolveExport of Random::Rand — including EngineFuncsInit's —
    // now hands out our thunk, which forwards. The run should be indis-
    // tinguishable from a normal one; if it is not, the seam is not sound.
    RegisterRuntimeExport(kRandomRand, reinterpret_cast<void*>(&NullPortRand));
    Pass("null port registered for System::Random::Rand — the rest of this run "
         "routes through it");

    free(state);
    free(snapshot);

    RuntimeRegistryStats st = GetRuntimeRegistryStats();
    fprintf(stderr, "[RtSelfTest] ==== done; registry now %zu exports / %zu addrs, "
                    "%zu hits so far ====\n", st.exports, st.addrs, st.hits);
    fflush(stderr);
}

// Call count for the null port, printed at shutdown so the run can show the
// seam was actually load-bearing rather than merely installed.
void RtSelfTestSummary() {
    if (!SelfTestEnabled()) return;
    fprintf(stderr, "[RtSelfTest] null port System::Random::Rand handled %llu calls "
                    "this run\n", (unsigned long long)g_thunkCalls);
    fflush(stderr);
}
