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

void Fail(const char* what);
void Pass(const char* what);

// ===========================================================================
// Test 2b's subject — the RSP-capture control (Wave 10 lane FREEPRO)
// ===========================================================================
//
// WHY TEST 2 CANNOT ANSWER THIS. Test 2's subject is System::Random::Rand,
// whose prologue is `40 53 48 83 EC 20 ...` — PUSH RBX; SUB RSP,0x20. There is
// no `MOV RAX,RSP` in it, so `InstallDetour`'s Bug Class 3 correction never
// fires on that subject and Test 2 is STRUCTURALLY INCAPABLE of expressing
// whether the correction is right. It passes either way. Ask of any instrument
// "could this field express the answer if the answer were different?" — Test 2's
// answer is no, which is why this second subject exists.
//
// The subject below is SYNTHETIC and its bytes mirror
// `?Startup@Engine@1@QEAAIPEBDII_N@Z` (0x1800db8a0) exactly in the one respect
// that matters: nonvolatile registers are SAVED through the RSP captured into
// RAX and RESTORED through an RSP-derived R11. Retail's invariant is RAX == the
// entry RSP; anything that shifts RAX without shifting the restore base by the
// same amount silently swaps the three saved registers by one slot.
//
// Synthetic rather than the real Engine::Startup because Engine::Startup runs
// exactly once, at boot, and boots the whole engine — it cannot be called at
// will, and calling it twice is not a test, it is a second boot.
alignas(16) const uint8_t kRspCaptureSubject[] = {
    // ---- the 19 bytes the detour displaces (identical shape to 0x1800db8a0)
    0x48, 0x8B, 0xC4,                                // MOV RAX,RSP     RAX = E
    0x55,                                            // PUSH RBP
    0x48, 0x8B, 0xEC,                                // MOV RBP,RSP
    0x48, 0x83, 0xEC, 0x60,                          // SUB RSP,0x60
    0x48, 0xC7, 0x45, 0xC0, 0xFE, 0xFF, 0xFF, 0xFF,  // MOV [RBP-0x40],-2
    // ---- body: save the three nonvolatiles THROUGH RAX, as retail does
    0x48, 0x89, 0x58, 0x10,                          // MOV [RAX+0x10],RBX
    0x48, 0x89, 0x70, 0x18,                          // MOV [RAX+0x18],RSI
    0x48, 0x89, 0x78, 0x20,                          // MOV [RAX+0x20],RDI
    // ---- clobber them, so a failed restore is visible rather than lucky
    0x48, 0xC7, 0xC3, 0x0D, 0xF0, 0xAD, 0x0B,        // MOV RBX,0x0BADF00D
    0x48, 0xC7, 0xC6, 0x0E, 0xF0, 0xAD, 0x0B,        // MOV RSI,0x0BADF00E
    0x48, 0xC7, 0xC7, 0x0F, 0xF0, 0xAD, 0x0B,        // MOV RDI,0x0BADF00F
    0xB8, 0x2A, 0x00, 0x00, 0x00,                    // MOV EAX,42  (return value)
    // ---- restore THROUGH RSP, as retail does: R11 = RSP+0x60 = E-8
    0x4C, 0x8D, 0x5C, 0x24, 0x60,                    // LEA R11,[RSP+0x60]
    0x49, 0x8B, 0x5B, 0x18,                          // MOV RBX,[R11+0x18]  = E+0x10
    0x49, 0x8B, 0x73, 0x20,                          // MOV RSI,[R11+0x20]  = E+0x18
    0x49, 0x8B, 0x7B, 0x28,                          // MOV RDI,[R11+0x28]  = E+0x20
    0x49, 0x8B, 0xE3,                                // MOV RSP,R11
    0x5D,                                            // POP RBP
    0xC3,                                            // RET
};
constexpr size_t kRspCapturePrologue = 19;   // exact instruction boundary

constexpr uint64_t kSentinelRbx = 0x1111111111111111ull;
constexpr uint64_t kSentinelRsi = 0x2222222222222222ull;
constexpr uint64_t kSentinelRdi = 0x3333333333333333ull;

struct CalleeSaved { uint64_t rbx, rsi, rdi, rax; };

// Load the three nonvolatiles with sentinels, CALL `fn`, and record what came
// back. The ONLY way to see this defect is at register level: it never touches
// the heap, so no allocator instrument — redzone included — can express it.
//
// The stack dance is deliberate. We align RSP, reserve 0x40 (32 bytes of home
// space the callee owns, plus slack) and stash the old RSP at +0x30, which is
// ABOVE anything either the correct or the corrupted callee writes: the correct
// one writes E+0x10..E+0x20 (= rsp+8..rsp+0x18) and the corrupted one writes
// one slot higher (rsp+0x10..rsp+0x20). Stashing inside that window would make
// the FAILING case crash on the way out instead of reporting.
void CallCapturingCalleeSaved(const void* fn, CalleeSaved* out) {
    __asm__ __volatile__(
        "movq  %%rsp, %%rax\n\t"
        "andq  $-16, %%rsp\n\t"
        "subq  $0x40, %%rsp\n\t"
        "movq  %%rax, 0x30(%%rsp)\n\t"
        "movabsq $0x1111111111111111, %%rbx\n\t"
        "movabsq $0x2222222222222222, %%rsi\n\t"
        "movabsq $0x3333333333333333, %%rdi\n\t"
        "callq *%[fn]\n\t"
        "movq  %%rbx,  0(%[out])\n\t"
        "movq  %%rsi,  8(%[out])\n\t"
        "movq  %%rdi, 16(%[out])\n\t"
        "movq  %%rax, 24(%[out])\n\t"
        "movq  0x30(%%rsp), %%rsp\n\t"
        :
        : [fn] "r"(fn), [out] "r"(out)
        : "rax", "rbx", "rcx", "rdx", "rsi", "rdi",
          "r8", "r9", "r10", "r11", "memory", "cc");
}

bool CalleeSavedIntact(const CalleeSaved& s) {
    return s.rbx == kSentinelRbx && s.rsi == kSentinelRsi && s.rdi == kSentinelRdi;
}

void ReportCalleeSaved(const char* which, const CalleeSaved& s) {
    fprintf(stderr, "[RtSelfTest]   %s: rbx=%016llx rsi=%016llx rdi=%016llx rax=%llu\n",
            which, (unsigned long long)s.rbx, (unsigned long long)s.rsi,
            (unsigned long long)s.rdi, (unsigned long long)s.rax);
}

RtDetour g_rspDetour = {};
uint64_t (*g_rspTramp)() = nullptr;
uint64_t g_rspWrapperCalls = 0;

// Mirrors rt_alloc_swap.cpp's ShadowStartup: an ordinary C++ wrapper that CALLs
// the trampoline. That CALL is the entire premise of the Bug Class 3 correction.
uint64_t RspCaptureWrapper() {
    g_rspWrapperCalls++;
    return g_rspTramp();
}

void RunRspCaptureControl() {
    fprintf(stderr, "[RtSelfTest] ---- Test 2b: RSP-capture prologue through a "
                    "CALLed trampoline ----\n");

    void* code = VirtualAlloc(nullptr, sizeof(kRspCaptureSubject) + 16,
                              MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!code) { Fail("2b: VirtualAlloc for the synthetic subject"); return; }
    memcpy(code, kRspCaptureSubject, sizeof(kRspCaptureSubject));
    FlushInstructionCache(GetCurrentProcess(), code, sizeof(kRspCaptureSubject));

    // BASELINE. If the synthetic does not preserve its own nonvolatiles when
    // called directly, it is not a valid subject and nothing below concludes
    // anything — this is the control's own control.
    CalleeSaved base = {};
    CallCapturingCalleeSaved(code, &base);
    ReportCalleeSaved("undetoured", base);
    if (base.rax != 42 || !CalleeSavedIntact(base)) {
        Fail("2b: the SYNTHETIC SUBJECT is wrong — it does not preserve rbx/rsi/rdi "
             "when called directly. Test 2b concludes nothing about the installer.");
        VirtualFree(code, 0, MEM_RELEASE);
        return;
    }
    Pass("2b baseline: the synthetic subject preserves rbx/rsi/rdi undetoured");

    if (!InstallDetour(reinterpret_cast<uintptr_t>(code),
                       reinterpret_cast<void*>(&RspCaptureWrapper),
                       &g_rspDetour, kRspCapturePrologue)) {
        Fail("2b: InstallDetour on the synthetic subject");
        VirtualFree(code, 0, MEM_RELEASE);
        return;
    }
    g_rspTramp = reinterpret_cast<uint64_t (*)()>(g_rspDetour.trampoline);
    g_rspWrapperCalls = 0;

    CalleeSaved hooked = {};
    CallCapturingCalleeSaved(code, &hooked);
    ReportCalleeSaved("detoured  ", hooked);

    if (g_rspWrapperCalls != 1) {
        Fail("2b: the wrapper was not entered — the patch did not take, so the "
             "register result below is not about the trampoline");
    } else if (!CalleeSavedIntact(hooked)) {
        // Name the mechanism, not just the verdict. The one-slot shift is the
        // signature of an RAX that was moved while the restore base was not.
        Fail("2b: the CALLed trampoline CORRUPTED callee-saved registers");
        if (hooked.rsi == kSentinelRbx && hooked.rdi == kSentinelRsi)
            fprintf(stderr, "[RtSelfTest]   mechanism: the three saved registers came "
                            "back shifted by exactly one 8-byte slot (rsi holds rbx's "
                            "value, rdi holds rsi's). That is an RAX displaced by +8 "
                            "against an RSP-derived restore base.\n");
    } else if (hooked.rax != 42) {
        Fail("2b: the trampoline did not reproduce the subject's return value");
    } else {
        Pass("2b: RSP-capture prologue survives a CALLed trampoline with "
             "rbx/rsi/rdi intact");
    }

    RemoveDetour(&g_rspDetour);
    CalleeSaved after = {};
    CallCapturingCalleeSaved(code, &after);
    if (after.rax != 42 || !CalleeSavedIntact(after))
        Fail("2b: removal did not restore the synthetic subject");
    else
        Pass("2b: removal restores the synthetic subject byte for byte");

    VirtualFree(code, 0, MEM_RELEASE);
    fflush(stderr);
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

    // ---- Test 2b: the RSP-capture control ----------------------------------
    // Test 2's subject has no RSP capture, so it cannot express Bug Class 3.
    // This one can. See kRspCaptureSubject above for why it is synthetic.
    RunRspCaptureControl();

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
