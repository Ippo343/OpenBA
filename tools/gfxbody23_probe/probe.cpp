// ============================================================================
// tools/gfxbody23_probe/probe.cpp — Wave 23 lane GFXBODY's oracle
// ============================================================================
//
// A DIFFERENTIAL ORACLE FOR `?Update@AnimationManager@Graphics@@` THAT NEEDS NO
// GAME RUN. Wave 23 §1b told every lane to assume it is binary-and-source work,
// and the game directory was held by Wave 22 lane ALLOC for the whole of this
// lane; this probe is the answer to that, not a substitute for it. It
// LoadLibrary()s the shipped Runtime-x64.dll (a READ of the game directory —
// nothing is written there and no game is launched), builds two byte-identical
// synthetic AnimationManagers in its OWN memory, runs the SHIPPED body over one
// and OUR PORTED BODY over the other, and compares:
//
//   (A) the nine-argument tuple recorded at every entry to the callee, in
//       order — this is the exact thing the inherited holdback said it could
//       not pin (runtime/src/graphics/animation.cpp:70-79);
//   (B) every byte of both arenas afterwards.
//
// *** IT COMPILES THE SHIPPING BODY, NOT A COPY OF IT. ***
// runtime/src/graphics/wave23_lane_gfxbody.cpp is compiled straight into this
// executable and the tiny seam below (ResolveAddr / RtResolveCached /
// RegisterRuntimeExport) is stubbed. That is deliberate: Wave 21 lane GFXBODY
// closed with "the binary measured in §7 is not the one described in §4", and a
// probe that re-types the algorithm measures the retyping.
//
// HOW THE CALLEE IS NEUTRALISED. `?Update@AnimationState@Graphics@@` (retail
// 0x1800fdb20) is a real animation evaluator we do not want to run. A 12-byte
// absolute JMP is written over its prologue IN THE LOADED IMAGE, pointing at
// `RecordingCallee` below. BOTH sides then call the same recorder, which is
// what makes the comparison about the CALLER and nothing else. The recorder is
// deterministic and WRITES A NON-TRIVIAL Vector3 through argument 7, so the
// caller's copy-back step has something real to copy — a recorder that left
// that vector zero would make the post-state compare trivially equal and the
// negative control unable to bite
// (reference_control_must_pick_a_compared_subject.md).
//
// USAGE
//   probe.exe [--dll <path>] [--corrupt <n>] [--groups N] [--states M]
//   exit 0 = agreed; 1 = diverged; 2 = setup failure.
//
// NEGATIVE CONTROLS (--corrupt N). Each perturbs OUR side only, and each was
// chosen so the thing it breaks is a thing the probe actually compares:
//   1  swap arguments 5 and 6 (the two InterpolatedState<Transform>*)  -> tuple
//   2  pass `false` for argument 8 instead of the caller's own flag    -> tuple
//   3  write 0.0f instead of 1.0f into the identity quaternion's w     -> bytes
//   4  walk the state array with the wrong stride (0x48 -> 0x40)       -> FAULTS.
//      Measured, not predicted: it SEGFAULTS (exit 139) before any comparison
//      runs, so it says nothing about either arm and is EXCLUDED from the score.
//      Kept because deleting a control that behaved unexpectedly is how a
//      control set quietly becomes a set of controls that were chosen to pass.
//   6  write 0.0f as the copied-back quaternion's w                    -> bytes ONLY
//   7  copy the Vector3 back from state+0x1c instead of state+0x18     -> bytes ONLY
//   5  hoist the inner loop bound (read it once)                       -> NEITHER,
//      on purpose: it is the EFFECTIVENESS control. Nothing in this probe grows
//      an array, so a hoist is genuinely unobservable here and MUST report
//      NOT CAUGHT. A control set in which every control fires proves only that
//      the controls were chosen to fire.
//
// WHY 6 AND 7 EXIST AT ALL — this is the finding, not a detail. The first
// control set was 1-5, and it scored 3/4 CAUGHT, which reads fine. It was not
// fine: controls 1, 2 AND 3 all fired on the ARGUMENT-TUPLE arm, control 4
// faulted, and control 5 correctly abstained — so the BYTE-COMPARE arm, which
// is half the oracle, had NOT ONE control that fired and could have been
// entirely dead. That is
// reference_union_control_hides_a_blind_screen.md exactly: a verdict scored on
// the union of two screens hides that one screen caught nothing. Controls 6 and
// 7 corrupt the copy-back ONLY, leaving every argument identical, so they can
// be caught by the byte arm and by nothing else.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

// ---------------------------------------------------------------------------
// The seam wave23_lane_gfxbody.cpp expects. Must match the declarations in
// runtime/funcs/rt_port.h and standalone/resolve.h exactly.
// ---------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };

static HMODULE g_rt = nullptr;
int g_corrupt = 0;               // read by the ported file via GfxbodyProbeCorrupt()

extern "C" int GfxbodyProbeCorrupt() { return g_corrupt; }

uintptr_t ResolveAddr(Module, uintptr_t ghidra_addr) {
    return reinterpret_cast<uintptr_t>(g_rt) + (ghidra_addr - 0x180000000ull);
}
void* ResolveExportRaw(const char* n) {
    return reinterpret_cast<void*>(GetProcAddress(g_rt, n));
}
void* ResolveExport(const char* n) { return ResolveExportRaw(n); }
void  RegisterRuntimeExport(const char*, void*) {}
void  RegisterRuntimeAddr(uintptr_t, void*) {}
void  RegisterRuntimeOrdinal(uint16_t, void*) {}
bool  RuntimeExportIsRegistered(const char*) { return false; }

void* RtResolveCached(const char* mangled, void** cache) {
    if (*cache) return *cache;
    void* p = ResolveExportRaw(mangled);
    if (p) *cache = p;
    return p;
}
uint64_t RtResolveMissCount() { return 0; }
void RtResolveMissSummary() {}
void RtResolveNoteEarly(const char*, uintptr_t, uint16_t, void*) {}
bool RtResolvePortInitDone() { return true; }
void RtResolveMarkPortInitDone() {}
uint64_t RtResolveEarlyCount() { return 0; }
void RtResolveEarlySummary() {}
uint64_t RtResolvePreInitHitCount() { return 0; }
uint64_t RtResolvePreInitFrozenCount() { return 0; }
int RtResolveSlotAudit() { return 0; }

// ---------------------------------------------------------------------------
// THE RECORDER
// ---------------------------------------------------------------------------
struct Call {
    void*    self;
    const void* time;
    const void* nodes;
    float    base[7];       // the Transform is taken BY VALUE here, so a caller
                            // that reuses one stack slot cannot hide behind the
                            // pointer still being alive at compare time
                            // (reference_oracle_must_snapshot_aliased_inputs.md)
    void*    interp5;
    void*    interp6;
    void*    outVec3;
    bool     flag7;
    bool     flag8;
};

static std::vector<Call> g_calls;
static uint32_t          g_tick = 0;

extern "C" bool RecordingCallee(void* self, const void* time, const void* nodes,
                                const float* base, void* interp5, void* interp6,
                                void* outVec3, bool flag7, bool flag8) {
    Call c{};
    c.self = self; c.time = time; c.nodes = nodes;
    std::memcpy(c.base, base, sizeof(c.base));
    c.interp5 = interp5; c.interp6 = interp6; c.outVec3 = outVec3;
    c.flag7 = flag7; c.flag8 = flag8;
    g_calls.push_back(c);

    // Deterministic, NON-ZERO, and different on every call, so the caller's
    // copy-back is observable and order-sensitive.
    float* v = static_cast<float*>(outVec3);
    ++g_tick;
    v[0] = 1.0f + static_cast<float>(g_tick);
    v[1] = 2.0f + static_cast<float>(g_tick) * 0.5f;
    v[2] = -3.0f - static_cast<float>(g_tick);
    return true;
}

// ---------------------------------------------------------------------------
// THE DETOUR. 13 bytes: MOV R10, imm64 ; JMP R10. R10 is volatile and is not
// an argument register in the Microsoft x64 ABI, so clobbering it is safe at a
// function entry.
// ---------------------------------------------------------------------------
static bool PatchJmp(void* target, void* dest) {
    // 13 bytes: 49 BA <imm64>  MOV R10, dest
    //           41 FF E2       JMP R10
    uint8_t full[13] = { 0x49, 0xBA, 0,0,0,0,0,0,0,0, 0x41, 0xFF, 0xE2 };
    std::memcpy(full + 2, &dest, 8);
    DWORD old = 0;
    if (!VirtualProtect(target, sizeof(full), PAGE_EXECUTE_READWRITE, &old)) return false;
    std::memcpy(target, full, sizeof(full));
    VirtualProtect(target, sizeof(full), old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(full));
    return true;
}

// ---------------------------------------------------------------------------
// THE ARENA. One contiguous block so a single memcmp covers everything either
// body could write, including anything it writes that it SHOULD NOT.
//
//   +0x0000  manager       0x20   (only +0x00 and +0x08 are read)
//   +0x0100  groups        G * 0x30
//   +0x0400  states        G * S * 0x48
//   +0x1000  interp pool   G * S * 2 * 0x40
// ---------------------------------------------------------------------------
static const size_t kArena = 0x8000;

struct Layout { int groups; int states; };

// Does this shape fit the fixed regions? A probe that silently overruns its own
// arena SEGFAULTS, and a segfault in a sweep cell reads as a blank line, which
// reads as neither pass nor fail. Checked and refused loudly instead.
static bool ArenaFits(Layout L) {
    const size_t groupsEnd = 0x0100 + (size_t)L.groups * 0x30;
    const size_t statesEnd = 0x0400 + (size_t)L.groups * (size_t)L.states * 0x48;
    const size_t interpEnd = 0x1000 + (size_t)L.groups * (size_t)L.states * 2 * 0x40;
    return groupsEnd <= 0x0400 && statesEnd <= 0x1000 && interpEnd <= kArena;
}

static void BuildArena(uint8_t* a, Layout L) {
    std::memset(a, 0, kArena);

    uint8_t* mgr    = a + 0x0000;
    uint8_t* groups = a + 0x0100;
    uint8_t* states = a + 0x0400;
    uint8_t* interp = a + 0x1000;

    *reinterpret_cast<uint8_t**>(mgr + 0x00) = groups;
    *reinterpret_cast<int32_t*>(mgr + 0x08)  = L.groups;
    *reinterpret_cast<int32_t*>(mgr + 0x0c)  = L.groups;

    int stateNo = 0;
    for (int g = 0; g < L.groups; ++g) {
        uint8_t* gp = groups + g * 0x30;
        // +0x00 must be non-null or the group is skipped entirely. Group 1 (when
        // there is one) is left NULL on purpose so the skip arm is exercised.
        *reinterpret_cast<uintptr_t*>(gp + 0x00) = (g == 1) ? 0 : (0xA1000 + g);
        // +0x10 is an Array<Node> passed BY REFERENCE; give it recognisable bytes.
        *reinterpret_cast<uint64_t*>(gp + 0x10) = 0xD0DEBEEFull;
        *reinterpret_cast<int32_t*>(gp + 0x18)  = 7 + g;
        *reinterpret_cast<uint8_t**>(gp + 0x20) = states + g * L.states * 0x48;
        *reinterpret_cast<int32_t*>(gp + 0x28)  = (g == 2) ? 0 : L.states;  // zero-count arm

        for (int s = 0; s < L.states; ++s) {
            uint8_t* st = states + (g * L.states + s) * 0x48;
            // +0x18 Vector3 — the callee writes it, the caller copies it back.
            reinterpret_cast<float*>(st + 0x18)[0] = -1.0f;
            reinterpret_cast<float*>(st + 0x18)[1] = -1.0f;
            reinterpret_cast<float*>(st + 0x18)[2] = -1.0f;
            uint8_t* i28 = interp + (stateNo * 2 + 0) * 0x40;
            uint8_t* i38 = interp + (stateNo * 2 + 1) * 0x40;
            std::memset(i28, 0x5A, 0x40);
            std::memset(i38, 0xA5, 0x40);
            *reinterpret_cast<uint8_t**>(st + 0x28) = i28;
            *reinterpret_cast<uint8_t**>(st + 0x38) = i38;
            ++stateNo;
        }
    }
}

// The Time& argument is only ever passed through, never read by this body.
static uint8_t g_time[0x20];

extern "C" void RtGraphics_AnimationManager_Update(void* self, const void* time, bool rootOnly);

int main(int argc, char** argv) {
    const char* dll = "C:\\GOG Games\\Banished\\Runtime-x64.dll";
    Layout L{ 4, 3 };
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--dll") && i + 1 < argc)      dll = argv[++i];
        else if (!std::strcmp(argv[i], "--corrupt") && i + 1 < argc) g_corrupt = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--groups") && i + 1 < argc)  L.groups = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--states") && i + 1 < argc)  L.states = std::atoi(argv[++i]);
    }

    if (!ArenaFits(L)) {
        std::printf("SETUP-FAIL: groups=%d states=%d does not fit the fixed arena "
                    "(groups<=21, groups*states<=54)\n", L.groups, L.states);
        return 2;
    }

    g_rt = LoadLibraryA(dll);
    if (!g_rt) { std::printf("SETUP-FAIL: LoadLibrary(%s) = %lu\n", dll, GetLastError()); return 2; }

    void* theirs = ResolveExportRaw("?Update@AnimationManager@Graphics@@QEAAXAEBVTime@System@@_N@Z");
    void* callee = ResolveExportRaw(
        "?Update@AnimationState@Graphics@@QEAA_NAEBVTime@System@@AEBV?$Array@UNode@GraphicsMesh@"
        "Graphics@@@Collection@@AEBVTransform@Math@@PEAV?$InterpolatedState@VTransform@Math@@@2@"
        "PEAV78@AEAVVector3@8@_N6@Z");
    if (!theirs || !callee) {
        std::printf("SETUP-FAIL: theirs=%p callee=%p\n", theirs, callee);
        return 2;
    }
    if (!PatchJmp(callee, reinterpret_cast<void*>(&RecordingCallee))) {
        std::printf("SETUP-FAIL: could not patch the callee\n"); return 2;
    }

    std::memset(g_time, 0x33, sizeof(g_time));

    // *** ONE ARENA, RUN TWICE, SNAPSHOT BETWEEN. ***
    // The first cut of this probe used two arenas side by side and reported 53
    // byte divergences at corrupt=0 — every one of them an INTERIOR POINTER that
    // BuildArena had written, which necessarily differs when the two arenas sit
    // at different addresses. That is exactly
    // reference_position_only_diffs_hide_one_body.md in miniature, and a probe
    // that reports 53 differences on a correct body would have been read as a
    // broken port. Running both bodies over the SAME address and snapshotting in
    // between removes the whole class rather than special-casing the fields.
    static uint8_t arena[kArena];
    static uint8_t snapT[kArena];
    static uint8_t snapO[kArena];

    // ---- retail ----
    BuildArena(arena, L);
    g_calls.clear(); g_tick = 0;
    reinterpret_cast<void (*)(void*, const void*, bool)>(theirs)(arena, g_time, true);
    std::memcpy(snapT, arena, kArena);
    std::vector<Call> callsT = g_calls;

    // ---- ours ----
    BuildArena(arena, L);
    g_calls.clear(); g_tick = 0;
    RtGraphics_AnimationManager_Update(arena, g_time, true);
    std::memcpy(snapO, arena, kArena);
    std::vector<Call> callsO = g_calls;

    // ---- compare ----
    int bad = 0;
    std::printf("calls: theirs=%zu ours=%zu\n", callsT.size(), callsO.size());
    if (callsT.size() != callsO.size()) { std::printf("DIVERGE: call count\n"); ++bad; }

    const size_t n = callsT.size() < callsO.size() ? callsT.size() : callsO.size();
    size_t argDiffs = 0;
    for (size_t i = 0; i < n; ++i) {
        const Call& a = callsT[i]; const Call& b = callsO[i];
        // `self`, `outVec3` and the two interp pointers live in DIFFERENT arenas,
        // so compare them as OFFSETS from each side's own base, never as values.
        auto offT = [&](const void* p) { return p ? (const uint8_t*)p - arena : (ptrdiff_t)-1; };
        auto offO = [&](const void* p) { return p ? (const uint8_t*)p - arena : (ptrdiff_t)-1; };
        bool eq = offT(a.self)    == offO(b.self)
               && offT(a.nodes)   == offO(b.nodes)
               && offT(a.interp5) == offO(b.interp5)
               && offT(a.interp6) == offO(b.interp6)
               && offT(a.outVec3) == offO(b.outVec3)
               && a.time  == b.time
               && a.flag7 == b.flag7
               && a.flag8 == b.flag8
               && std::memcmp(a.base, b.base, sizeof(a.base)) == 0;
        if (!eq) {
            ++argDiffs;
            if (argDiffs <= 4) {
                std::printf("DIVERGE call %zu:\n", i);
                std::printf("  self   %td / %td\n", offT(a.self), offO(b.self));
                std::printf("  nodes  %td / %td\n", offT(a.nodes), offO(b.nodes));
                std::printf("  arg5   %td / %td\n", offT(a.interp5), offO(b.interp5));
                std::printf("  arg6   %td / %td\n", offT(a.interp6), offO(b.interp6));
                std::printf("  arg7   %td / %td\n", offT(a.outVec3), offO(b.outVec3));
                std::printf("  flags  %d,%d / %d,%d\n", a.flag7, a.flag8, b.flag7, b.flag8);
                std::printf("  xform  [%g %g %g %g | %g %g %g] / [%g %g %g %g | %g %g %g]\n",
                            a.base[0],a.base[1],a.base[2],a.base[3],a.base[4],a.base[5],a.base[6],
                            b.base[0],b.base[1],b.base[2],b.base[3],b.base[4],b.base[5],b.base[6]);
            }
        }
    }
    if (argDiffs) { std::printf("DIVERGE: %zu of %zu argument tuples\n", argDiffs, n); ++bad; }

    size_t byteDiffs = 0, firstDiff = 0;
    for (size_t i = 0; i < kArena; ++i)
        if (snapT[i] != snapO[i]) { if (!byteDiffs) firstDiff = i; ++byteDiffs; }
    if (byteDiffs) {
        std::printf("DIVERGE: %zu of %zu arena bytes, first at +0x%zx (%02x vs %02x)\n",
                    byteDiffs, kArena, firstDiff, snapT[firstDiff], snapO[firstDiff]);
        ++bad;
    }

    // EFFECTIVENESS: how many bytes did either side actually CHANGE? A run where
    // this is 0 compared nothing but the setup
    // (reference_probe_needs_an_effectiveness_control.md), and a green verdict
    // on it would be worthless.
    static uint8_t pristine[kArena];
    BuildArena(arena, L);
    std::memcpy(pristine, arena, kArena);
    size_t worked = 0;
    for (size_t i = 0; i < kArena; ++i) if (snapT[i] != pristine[i]) ++worked;
    std::printf("effectiveness: %zu arena bytes changed by the SHIPPED body, %zu calls made\n",
                worked, callsT.size());

    std::printf("corrupt=%d  VERDICT: %s\n", g_corrupt, bad ? "DIVERGED" : "AGREED");
    return bad ? 1 : 0;
}
