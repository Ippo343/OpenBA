// ============================================================================
// tools/gfxmat24_probe/probe.cpp — Wave 24 lane GFXMAT's oracle
// ============================================================================
//
// A DIFFERENTIAL ORACLE FOR THE THREE `?Serialize@` BODIES THAT NEEDS NO GAME
// RUN. Kickoff §1b gives this lane no game-directory allocation; kickoff §1c
// asks every lane to NAME its probe shape first. This is a **synthesised
// oracle** — the fourth shape on that list — and it is here because the other
// four do not fit:
//
//   * input sweep against the shipped export — NO. These bodies are not pure;
//     they mutate a stream, an Encoding and an object.
//   * shadow/diff site exercised by someone else's run — INSUFFICIENT ALONE. A
//     `Serialize` cannot be replayed IN PLACE: the first arm consumes or emits
//     the stream and the second sees a different one. A shadow marker here
//     would take the port's own verdict and never consult the DLL, which is
//     reference_shadow_marker_is_not_an_oracle.md exactly.
//   * write-path replay — THIS, but it needs a stream WE build, which is what
//     this file is. reference_write_path_serialize_is_replayable.md is the
//     reason "no oracle possible" is not written in the port file.
//   * two-sided disassembly check — the fallback, and not needed.
//
// WHAT IT DOES. LoadLibrary()s the shipped Runtime-x64.dll (a READ of the game
// directory; nothing is written there and no game is launched — the same
// footprint tools/gfxbody23_probe/ has), builds a synthetic IO::Stream,
// Encoding factory and Encoding whose VTABLES WE OWN, builds two byte-identical
// synthetic objects in its OWN memory, runs the SHIPPED body over one and OUR
// PORTED BODY over the other, and compares the CALL TRACE.
//
// *** IT COMPILES THE SHIPPING BODY, NOT A COPY OF IT. ***
// runtime/src/graphics/wave24_lane_gfxmat.cpp is compiled straight into this
// executable and the seam below is stubbed. Wave 21 lane GFXBODY closed with
// "the binary measured in §7 is not the one described in §4"; a probe that
// re-types the algorithm measures the retyping.
//
// HOW THE CALLEES ARE NEUTRALISED. The eleven helpers these three bodies reach
// (ten unexported internals plus the exported, unported `BuildParameters`) are
// each overwritten with a 13-byte absolute JMP to a RECORDER, in the loaded
// image, before either arm runs. BOTH arms then call the SAME recorder — which
// is what makes the comparison about the CALLER and nothing else. Our arm gets
// there because the probe's `ResolveAddr`/`ResolveExport` stubs hand back the
// raw image address, which is the patched one; retail's arm gets there because
// its rel32 lands on the same bytes.
//
// ---------------------------------------------------------------------------
// THE SCREENS, AND WHICH ONE IS REAL — read this before the score
// ---------------------------------------------------------------------------
// (A) THE TRACE. Every recorder pushes a tagged record with its arguments,
//     POINTERS NORMALISED TO AN ARENA OFFSET so the two arms' different base
//     addresses are not a divergence. This is the oracle.
//
// (B) THE ARENA BYTES, and it is **REDUNDANT BY CONSTRUCTION HERE, WHICH THE
//     PROBE MEASURES RATHER THAN ASSUMES.** On the Serialize path neither body
//     writes the object directly — every write goes through a helper, and every
//     helper is a recorder. So screen (B) can only differ when screen (A)
//     already differs. It is run, and it is SCORED SEPARATELY, and the expected
//     result is that it catches a strict subset of what (A) catches. That is
//     reference_union_control_hides_a_blind_screen.md applied HONESTLY: the
//     answer there was not "add controls until both screens fire", it was
//     "never score two screens as one". A per-screen score that shows (B)
//     catching 0 is a finding about this subject, not a defect in the probe.
//
// NEGATIVE CONTROLS (--corrupt N). Each perturbs OUR side only, and each was
// chosen so the thing it breaks is a thing screen (A) actually compares:
//   1  swap the two Array::SetSize helper addresses (textures <-> constants)
//   2  read the int encoder at +0xb8 instead of +0xa0 — THE documented
//      retail-vs-profile Encoding offset trap, made into a control
//   3  walk the texture array with stride 0x18 instead of 0x10
//   4  skip the BuildParameters call entirely
//   5  pass 3 floats to the float encoder instead of 4
//   6  re-read the loop bound every iteration instead of latching it
//      -> THE EFFECTIVENESS CONTROL, AND IT IS CONDITIONAL. It MUST NOT FIRE
//      when `--count` == `--inject` (the default), because then `Array::_size`
//      and the count the Encoding supplies are the same number and a re-read is
//      genuinely unobservable. A control set in which every control fires proves
//      only that the controls were chosen to fire.
//
//      *** IT FIRED ON THE FIRST RUN, AND THAT IS A FINDING, NOT A DEFECT. ***
//      The first version of this probe hardcoded an arena of 2 elements while
//      the Encoding injected 3, and control 6 duly fired. The right response was
//      NOT to retune the control until it abstained — that is how a control set
//      becomes a set of controls chosen to pass — but to notice WHY: on the READ
//      path the count comes FROM THE FILE and `Array::_size` is still the OLD,
//      in-memory one. So **retail's latch is load-bearing on every load where
//      the saved count differs from the live one**, and a port that re-read
//      `_size` would serialize the wrong number of elements. Both configurations
//      are now runnable and BOTH ARE REPORTED:
//          --count 3 --inject 3   control 6 ABSTAINS  (the effectiveness case)
//          --count 2 --inject 3   control 6 FIRES     (the real load case)
//
// EXIT CODES: 0 agreed, 1 diverged, 2 setup failure.
// USAGE: probe.exe [--dll <path>] [--corrupt N] [--mode read|write]
//                  [--subject all|mat|mesh|anim] [--count N] [--inject M]
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

// ---------------------------------------------------------------------------
// The seam wave24_lane_gfxmat.cpp expects. Must match the declarations in
// runtime/funcs/rt_port.h, runtime/funcs/rt_diff.h and standalone/resolve.h.
// ---------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };

static HMODULE g_rt = nullptr;
int g_corrupt = 0;

extern "C" int GfxmatProbeCorrupt() { return g_corrupt; }

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
// The rt_diff seam, stubbed OFF. RtDiffEnabled() == false makes the ported
// file's in-body differential oracle (which has its own scratch arenas and is
// a different instrument) dead code on this run, so what this probe measures is
// the Serialize path and nothing else.
// ---------------------------------------------------------------------------
struct RtDiffSite;
extern "C" { }
bool RtDiffEnabled() { return false; }
void RtDiffRegister(RtDiffSite*) {}
void RtDiffNoteWork(RtDiffSite*, bool) {}
void RtDiffReport(RtDiffSite*, bool, const char*, const char*) {}
void RtDiffReportBytes(RtDiffSite*, const void*, const void*, size_t) {}
bool RtDiffInvertReplay() { return false; }
void RtDiffInvertArm(RtDiffSite*) {}
void RtDiffSetReadableProbe(bool (*)(const void*, size_t)) {}
void RtDiffFormat(char* b, size_t n, int v)                { std::snprintf(b, n, "%d", v); }
void RtDiffFormat(char* b, size_t n, unsigned int v)       { std::snprintf(b, n, "%u", v); }
void RtDiffFormat(char* b, size_t n, long long v)          { std::snprintf(b, n, "%lld", v); }
void RtDiffFormat(char* b, size_t n, unsigned long long v) { std::snprintf(b, n, "%llu", v); }
void RtDiffFormat(char* b, size_t n, float v)              { std::snprintf(b, n, "%f", (double)v); }
void RtDiffFormat(char* b, size_t n, double v)             { std::snprintf(b, n, "%f", v); }
void RtDiffFormat(char* b, size_t n, bool v)               { std::snprintf(b, n, "%d", (int)v); }
void RtDiffFormat(char* b, size_t n, const void* v)        { std::snprintf(b, n, "%p", v); }
bool RtDiffFloatEqual(float a, float b, float) { return a == b; }

// ---------------------------------------------------------------------------
// THE TRACE
// ---------------------------------------------------------------------------
enum Ev {
    EV_MAKE_ENC = 1, EV_SER_MATERIAL, EV_SETSIZE_TEX, EV_SETSIZE_CONST,
    EV_SER_STRING, EV_SER_IMAGE, EV_ENC_INT, EV_ENC_FLOATS, EV_BUILDPARAMS,
    EV_ENC_DTOR, EV_MESH_SUB, EV_SETSIZE_MESH, EV_MESH_ELEMS,
    EV_SETSIZE_ANIM, EV_ANIM_ELEMS, EV_ENC_WRONGSLOT
};
static const char* EvName(int e) {
    switch (e) {
    case EV_MAKE_ENC: return "MakeEncoding";
    case EV_SER_MATERIAL: return "SerExtPtr<Material>@0x180061100";
    case EV_SETSIZE_TEX: return "Array<Texture>::SetSize@0x180061880";
    case EV_SETSIZE_CONST: return "Array<Constant>::SetSize@0x1800616c0";
    case EV_SER_STRING: return "String::Serialize@0x1800c9930";
    case EV_SER_IMAGE: return "SerExtPtr<Image>@0x18005d0f0";
    case EV_ENC_INT: return "Encoding::vt[0xa0] int";
    case EV_ENC_FLOATS: return "Encoding::vt[0x28] floats";
    case EV_BUILDPARAMS: return "BuildParameters (export)";
    case EV_ENC_DTOR: return "Encoding::vt[0x00] dtor";
    case EV_MESH_SUB: return "MeshSub@0x1800fe250";
    case EV_SETSIZE_MESH: return "Array::SetSize@0x18005cc60";
    case EV_MESH_ELEMS: return "Array::SerElems@0x18005d4a0";
    case EV_SETSIZE_ANIM: return "Array::SetSize@0x18005c3d0";
    case EV_ANIM_ELEMS: return "Array::SerElems@0x18005c660";
    case EV_ENC_WRONGSLOT: return "Encoding::vt[0xb8] (WRONG SLOT)";
    default: return "?";
    }
}

// COMPARED FIELD BY FIELD, NEVER BY memcmp. The first version of this probe
// memcmp'd `Rec`, and `int ev` leaves FOUR BYTES OF PADDING before `a0` that
// neither arm writes — so two records that PRINTED IDENTICALLY compared unequal
// and the probe reported a divergence on a correct port. That is
// reference_oracle_must_not_compare_unwritten_bytes.md, and it was found by the
// UNCORRUPTED run, not by a control: every control would have scored CAUGHT
// either way, so the control set could not have seen it. Instrument rule 1 —
// "when a tool's output supports the conclusion you already expected you have
// received no evidence that the tool works" — read in reverse: the tool
// DISAGREEING with the expected answer is what exposed it.
struct Rec {
    int ev; uint64_t a0, a1, a2, a3;
    bool operator==(const Rec& o) const {
        return ev == o.ev && a0 == o.a0 && a1 == o.a1 && a2 == o.a2 && a3 == o.a3;
    }
    bool operator!=(const Rec& o) const { return !(*this == o); }
};

static std::vector<Rec> g_trace[2];
static int              g_arm = 0;          // 0 = theirs, 1 = ours
static uint8_t*         g_arenaBase[2] = { nullptr, nullptr };
static size_t           g_arenaSize = 0;
static int              g_encIntTick = 0;
static int32_t          g_injectCount = 3;  // what the READ path's encoder writes back

// Normalise a pointer to an arena offset so the two arms' different base
// addresses are not reported as a divergence. Anything outside the arena is
// recorded as a tag rather than a raw address, for the same reason.
static uint64_t Norm(const void* p) {
    if (!p) return 0;
    uint8_t* b = g_arenaBase[g_arm];
    const uint8_t* q = static_cast<const uint8_t*>(p);
    if (b && q >= b && q < b + g_arenaSize) return 0x1000000000ull + (uint64_t)(q - b);
    return 0x2000000000ull;      // "somewhere else" — a stack local, typically
}

static void Push(int ev, uint64_t a0 = 0, uint64_t a1 = 0, uint64_t a2 = 0, uint64_t a3 = 0) {
    g_trace[g_arm].push_back(Rec{ ev, a0, a1, a2, a3 });
}

// ---------------------------------------------------------------------------
// THE RECORDERS. Each also WRITES a deterministic, non-zero pattern into the
// arena location it was handed, so screen (B) has something real to compare and
// a recorder that was never reached is distinguishable from one that wrote
// zeroes (reference_oracle_must_not_compare_unwritten_bytes.md).
// ---------------------------------------------------------------------------
static void StampAt(void* p, size_t n, uint8_t seed) {
    uint8_t* q = static_cast<uint8_t*>(p);
    for (size_t i = 0; i < n; ++i) q[i] = static_cast<uint8_t>(seed + i);
}

extern "C" void Rec_SerMaterial(void* field, void* slot) {
    Push(EV_SER_MATERIAL, Norm(field), Norm(slot)); StampAt(field, 8, 0x11);
}
extern "C" void Rec_SetSizeTex(void* arr, int n)   { Push(EV_SETSIZE_TEX,   Norm(arr), (uint64_t)(int64_t)n); }
extern "C" void Rec_SetSizeConst(void* arr, int n) { Push(EV_SETSIZE_CONST, Norm(arr), (uint64_t)(int64_t)n); }
extern "C" void Rec_SerString(void* field, void* slot) {
    Push(EV_SER_STRING, Norm(field), Norm(slot)); StampAt(field, 8, 0x22);
}
extern "C" void Rec_SerImage(void* field, void* slot) {
    Push(EV_SER_IMAGE, Norm(field), Norm(slot)); StampAt(field, 8, 0x33);
}
extern "C" void Rec_BuildParams(void* self)        { Push(EV_BUILDPARAMS, Norm(self)); }
extern "C" void Rec_MeshSub(void* field, void* ser){ Push(EV_MESH_SUB, Norm(field), Norm(ser)); }
extern "C" void Rec_SetSizeMesh(void* arr, int n)  { Push(EV_SETSIZE_MESH, Norm(arr), (uint64_t)(int64_t)n); }
extern "C" void Rec_MeshElems(void* ser, int n, void* data) {
    Push(EV_MESH_ELEMS, Norm(ser), (uint64_t)(int64_t)n, Norm(data));
}
extern "C" void Rec_SetSizeAnim(void* arr, int n)  { Push(EV_SETSIZE_ANIM, Norm(arr), (uint64_t)(int64_t)n); }
extern "C" void Rec_AnimElems(void* slot, int n, void* data) {
    Push(EV_ANIM_ELEMS, Norm(slot), (uint64_t)(int64_t)n, Norm(data));
}

// The synthetic Encoding / factory. These are OURS, so they need no patch.
extern "C" void* Enc_MakeRead(void* factory, void** slot, int z) {
    Push(EV_MAKE_ENC, Norm(factory), Norm(slot), (uint64_t)(int64_t)z, 0);
    extern void* g_encoding; return g_encoding;
}
extern "C" void* Enc_MakeWrite(void* factory, void** slot, int z) {
    Push(EV_MAKE_ENC, Norm(factory), Norm(slot), (uint64_t)(int64_t)z, 1);
    extern void* g_encoding; return g_encoding;
}
extern "C" void Enc_Int(void* enc, void* value, int z) {
    int32_t* v = static_cast<int32_t*>(value);
    Push(EV_ENC_INT, Norm(enc), Norm(value), (uint64_t)(uint32_t)*v, (uint64_t)(int64_t)z);
    // On the READ path the encoder is what SUPPLIES the count, so write one
    // back. Deterministic and per-arm-identical: the tick resets between arms.
    ++g_encIntTick;
    *v = g_injectCount;
}
extern "C" void Enc_Floats(void* enc, int count, void* values, int z) {
    Push(EV_ENC_FLOATS, Norm(enc), (uint64_t)(int64_t)count, Norm(values), (uint64_t)(int64_t)z);
    if (count > 0 && values) StampAt(values, (size_t)count * 4, 0x44);
}
extern "C" void Enc_WrongSlot(void* enc, void* value, int z) {
    Push(EV_ENC_WRONGSLOT, Norm(enc), Norm(value), 0, (uint64_t)(int64_t)z);
    int32_t* v = static_cast<int32_t*>(value); *v = g_injectCount;
}
extern "C" void* Enc_Dtor(void* enc, int flag) {
    Push(EV_ENC_DTOR, Norm(enc), (uint64_t)(int64_t)flag); return enc;
}
extern "C" void* Enc_Abort(void*, int) {
    std::fprintf(stderr, "[gfxmat24] unexpected Encoding vtable dispatch\n");
    std::exit(2);
}

// ---------------------------------------------------------------------------
// THE PATCH. 13 bytes: MOV R10, imm64 ; JMP R10. R10 is volatile and is not an
// argument register in the Microsoft x64 ABI, so clobbering it at a function
// entry is safe.
// ---------------------------------------------------------------------------
static bool PatchJmp(void* target, void* dest) {
    uint8_t full[13] = { 0x49, 0xBA, 0,0,0,0,0,0,0,0, 0x41, 0xFF, 0xE2 };
    std::memcpy(full + 2, &dest, 8);
    DWORD old = 0;
    if (!VirtualProtect(target, sizeof(full), PAGE_EXECUTE_READWRITE, &old)) return false;
    std::memcpy(target, full, sizeof(full));
    VirtualProtect(target, sizeof(full), old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(full));
    return true;
}
static bool PatchVa(uintptr_t ghidra, void* dest) {
    return PatchJmp(reinterpret_cast<void*>(ResolveAddr(Module::Runtime, ghidra)), dest);
}

// ---------------------------------------------------------------------------
// THE SYNTHETIC IO OBJECTS
// ---------------------------------------------------------------------------
void* g_encVTable[32];
void* g_factoryVTable[8];
void* g_encoding = nullptr;
static void* g_encObj[1];
static void* g_factoryObj[1];

struct FakeStream {
    uint8_t  head[8];
    int32_t  mode;            // +0x08
    int32_t  pad;
    void*    factory;         // +0x10
};
struct FakeSerializer {
    FakeStream* stream;       // +0x00
    void*       encoding;     // +0x08
};

static void BuildIoObjects(int mode) {
    for (int i = 0; i < 32; ++i) g_encVTable[i] = (void*)&Enc_Abort;
    for (int i = 0; i < 8;  ++i) g_factoryVTable[i] = (void*)&Enc_Abort;
    g_encVTable[0x00 / 8] = (void*)&Enc_Dtor;
    g_encVTable[0x28 / 8] = (void*)&Enc_Floats;
    g_encVTable[0xa0 / 8] = (void*)&Enc_Int;
    g_encVTable[0xb8 / 8] = (void*)&Enc_WrongSlot;   // control 2's destination
    g_factoryVTable[0x18 / 8] = (void*)&Enc_MakeRead;
    g_factoryVTable[0x20 / 8] = (void*)&Enc_MakeWrite;
    g_encObj[0]     = static_cast<void*>(g_encVTable);
    g_factoryObj[0] = static_cast<void*>(g_factoryVTable);
    g_encoding = static_cast<void*>(g_encObj);
    (void)mode;
}

// ---------------------------------------------------------------------------
// THE ARENA. One contiguous block per arm so a single memcmp covers everything
// either body could write, including anything it writes that it SHOULD NOT.
//
//   +0x0000  object      0x100   (MaterialInstance 0x60, or a MeshInstance /
//                                 AnimationManager stand-in)
//   +0x0100  textures    16 * 0x10
//   +0x0300  constants   16 * 0x18
//   +0x0600  spare
// ---------------------------------------------------------------------------
static const size_t kArena = 0x1000;

struct MatLayout {
    void*    vptr;
    void*    e08; void* e10; void* e18;
    uint32_t shaderClass;
    uint8_t  hasRT, hasRTd, pad26[2];
    void*    material;
    void*    parametersData; int32_t parametersSize, parametersCap;
    void*    constantsData;  int32_t constantsSize,  constantsCap;
    void*    texturesData;   int32_t texturesSize,   texturesCap;
};
static_assert(sizeof(MatLayout) == 0x60, "MaterialInstance stand-in must be 0x60");

static void BuildMatArena(uint8_t* a, int liveCount) {
    std::memset(a, 0, kArena);
    MatLayout* m = reinterpret_cast<MatLayout*>(a);
    m->texturesData  = a + 0x100;  m->texturesSize  = liveCount; m->texturesCap  = 16;
    m->constantsData = a + 0x300;  m->constantsSize = liveCount; m->constantsCap = 16;
    // Element payloads get a recognisable pattern so a body that fails to touch
    // one is caught by screen (B) rather than reading as "wrote zero".
    for (size_t i = 0x100; i < 0x600; ++i) a[i] = static_cast<uint8_t>(0x5A + (i & 0x3f));
}

// MeshInstance / AnimationManager stand-ins: only the fields the bodies read.
static void BuildMeshArena(uint8_t* a, int liveCount) {
    std::memset(a, 0, kArena);
    *reinterpret_cast<void**>(a + 0xc8)   = a + 0x100;      // Array::_data
    *reinterpret_cast<int32_t*>(a + 0xd0) = liveCount;      // the count field
    for (size_t i = 0x100; i < 0x600; ++i) a[i] = static_cast<uint8_t>(0x5A + (i & 0x3f));
}
static void BuildAnimArena(uint8_t* a, int liveCount) {
    std::memset(a, 0, kArena);
    *reinterpret_cast<void**>(a + 0x00)   = a + 0x100;      // Array::_data at +0x00
    *reinterpret_cast<int32_t*>(a + 0x08) = liveCount;      // Array::_size at +0x08
    for (size_t i = 0x100; i < 0x600; ++i) a[i] = static_cast<uint8_t>(0x5A + (i & 0x3f));
}

// ---------------------------------------------------------------------------
// OUR BODIES (from the compiled port) and THEIRS (from the export table).
// ---------------------------------------------------------------------------
extern "C" bool RtGraphics_MaterialInstance_Serialize(void* self, void* stream);
extern "C" void RtGraphics_MeshInstance_Serialize(void* self, void* ser);
extern "C" void RtGraphics_AnimationManager_Serialize(void* self, void* ser);

// ---------------------------------------------------------------------------
// COMPARISON
// ---------------------------------------------------------------------------
static int CompareTrace(const char* subject) {
    const std::vector<Rec>& T = g_trace[0];
    const std::vector<Rec>& O = g_trace[1];
    if (T.size() != O.size()) {
        std::printf("  [%s] SCREEN A DIVERGED: theirs=%zu records, ours=%zu\n",
                    subject, T.size(), O.size());
        size_t n = T.size() < O.size() ? T.size() : O.size();
        for (size_t i = 0; i < n; ++i) {
            if (T[i] != O[i]) {
                std::printf("    first differing record #%zu: theirs=%s ours=%s\n",
                            i, EvName(T[i].ev), EvName(O[i].ev));
                break;
            }
        }
        return 1;
    }
    for (size_t i = 0; i < T.size(); ++i) {
        if (T[i] != O[i]) {
            std::printf("  [%s] SCREEN A DIVERGED at record #%zu\n", subject, i);
            std::printf("    theirs %-34s %llx %llx %llx %llx\n", EvName(T[i].ev),
                        (unsigned long long)T[i].a0, (unsigned long long)T[i].a1,
                        (unsigned long long)T[i].a2, (unsigned long long)T[i].a3);
            std::printf("    ours   %-34s %llx %llx %llx %llx\n", EvName(O[i].ev),
                        (unsigned long long)O[i].a0, (unsigned long long)O[i].a1,
                        (unsigned long long)O[i].a2, (unsigned long long)O[i].a3);
            return 1;
        }
    }
    std::printf("  [%s] screen A AGREED: %zu records, identical\n", subject, T.size());
    return 0;
}

static int CompareBytes(const char* subject, uint8_t* a, uint8_t* b) {
    // The two Array::_data pointers hold each arm's OWN arena address and MUST
    // differ; comparing them would report a divergence on a correct port
    // (reference_a_control_can_corrupt_a_byte_that_should_not_be_compared.md is
    // the mirror of this, and both directions have produced false verdicts
    // here). So the element payload region is compared and the header is not.
    if (std::memcmp(a + 0x100, b + 0x100, 0x500) != 0) {
        std::printf("  [%s] SCREEN B DIVERGED in the element payload region\n", subject);
        return 1;
    }
    std::printf("  [%s] screen B agreed (see the header: redundant by construction)\n", subject);
    return 0;
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    const char* dll = "C:\\GOG Games\\Banished\\Runtime-x64.dll";
    const char* mode = "write";
    const char* subject = "all";
    // `--count` is what the ARENA holds; `--inject` is what the Encoding writes
    // back into the count variable, i.e. what a save file would say. They are
    // SEPARATE ON PURPOSE and defaulting them equal is what makes control 6 an
    // abstainer; see the note beside it in main() and WAVE24_LANE_GFXMAT.md §5.
    int liveCount = 3;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--dll") && i + 1 < argc) dll = argv[++i];
        else if (!std::strcmp(argv[i], "--corrupt") && i + 1 < argc) g_corrupt = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--mode") && i + 1 < argc) mode = argv[++i];
        else if (!std::strcmp(argv[i], "--subject") && i + 1 < argc) subject = argv[++i];
        else if (!std::strcmp(argv[i], "--count") && i + 1 < argc) liveCount = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--inject") && i + 1 < argc) g_injectCount = (int32_t)std::atoi(argv[++i]);
        else { std::fprintf(stderr, "unknown argument %s\n", argv[i]); return 2; }
    }
    if (liveCount < 0 || liveCount > 16 || g_injectCount < 0 || g_injectCount > 16) {
        std::fprintf(stderr, "SETUP FAILURE: count/inject must be 0..16 (the arena holds 16)\n");
        return 2;
    }
    const int streamMode = std::strcmp(mode, "read") == 0 ? 0 : 1;

    g_rt = LoadLibraryA(dll);
    if (!g_rt) { std::fprintf(stderr, "cannot load %s (%lu)\n", dll, GetLastError()); return 2; }

    BuildIoObjects(streamMode);

    // Patch every helper BOTH arms reach. A failure here is a setup failure,
    // not a verdict — a probe that silently ran with an unpatched callee would
    // execute a real Array::SetSize against a synthetic arena.
    struct P { uintptr_t va; void* rec; const char* name; };
    const P patches[] = {
        { 0x180061100ull, (void*)&Rec_SerMaterial,  "0x180061100" },
        { 0x180061880ull, (void*)&Rec_SetSizeTex,   "0x180061880" },
        { 0x1800616c0ull, (void*)&Rec_SetSizeConst, "0x1800616c0" },
        { 0x1800c9930ull, (void*)&Rec_SerString,    "0x1800c9930" },
        { 0x18005d0f0ull, (void*)&Rec_SerImage,     "0x18005d0f0" },
        { 0x1801033e0ull, (void*)&Rec_BuildParams,  "BuildParameters" },
        { 0x1800fe250ull, (void*)&Rec_MeshSub,      "0x1800fe250" },
        { 0x18005cc60ull, (void*)&Rec_SetSizeMesh,  "0x18005cc60" },
        { 0x18005d4a0ull, (void*)&Rec_MeshElems,    "0x18005d4a0" },
        { 0x18005c3d0ull, (void*)&Rec_SetSizeAnim,  "0x18005c3d0" },
        { 0x18005c660ull, (void*)&Rec_AnimElems,    "0x18005c660" },
    };
    for (const P& p : patches) {
        if (!PatchVa(p.va, p.rec)) {
            std::fprintf(stderr, "SETUP FAILURE: cannot patch %s\n", p.name);
            return 2;
        }
    }

    static uint8_t arenaA[kArena], arenaB[kArena];
    g_arenaSize = kArena;
    g_arenaBase[0] = arenaA;
    g_arenaBase[1] = arenaB;

    const bool wantMat  = !std::strcmp(subject, "all") || !std::strcmp(subject, "mat");
    const bool wantMesh = !std::strcmp(subject, "all") || !std::strcmp(subject, "mesh");
    const bool wantAnim = !std::strcmp(subject, "all") || !std::strcmp(subject, "anim");

    // STATE WHAT WAS EXAMINED (instrument rule 5): six Wave 23 instruments
    // printed a verdict without naming their subject.
    std::printf("gfxmat24 probe: dll=%s mode=%s corrupt=%d subject=%s arenaCount=%d "
                "injectedCount=%d patched=%d helpers\n",
                dll, mode, g_corrupt, subject, liveCount, (int)g_injectCount,
                (int)(sizeof(patches) / sizeof(patches[0])));
    if (liveCount != g_injectCount) {
        std::printf("  NOTE: arenaCount != injectedCount, so the loop bound the "
                    "Encoding supplies DIFFERS from Array::_size. Control 6 (the "
                    "latch) becomes OBSERVABLE and will fire; that is the real "
                    "load case, not a probe defect. See WAVE24_LANE_GFXMAT.md S5.\n");
    }

    int bad = 0;
    int screenA = 0, screenB = 0;

    if (wantMat) {
        using MatSerFn = bool (*)(void*, void*);
        MatSerFn theirs = (MatSerFn)GetProcAddress(
            g_rt, "?Serialize@MaterialInstance@Graphics@@UEAA_NAEAVStream@IO@@@Z");
        if (!theirs) { std::fprintf(stderr, "SETUP FAILURE: MaterialInstance::Serialize not exported\n"); return 2; }
        g_trace[0].clear(); g_trace[1].clear();

        FakeStream s{}; s.mode = streamMode; s.factory = static_cast<void*>(g_factoryObj);
        g_arm = 0; g_encIntTick = 0; BuildMatArena(arenaA, liveCount); theirs(arenaA, &s);
        FakeStream s2{}; s2.mode = streamMode; s2.factory = static_cast<void*>(g_factoryObj);
        g_arm = 1; g_encIntTick = 0; BuildMatArena(arenaB, liveCount);
        RtGraphics_MaterialInstance_Serialize(arenaB, &s2);

        int a = CompareTrace("MaterialInstance::Serialize");
        int b = CompareBytes("MaterialInstance::Serialize", arenaA, arenaB);
        screenA += a; screenB += b; bad += (a || b);
    }

    if (wantMesh) {
        using MeshSerFn = void (*)(void*, void*);
        MeshSerFn theirs = (MeshSerFn)GetProcAddress(
            g_rt, "?Serialize@MeshInstance@Graphics@@QEAAXAEAVSerializer@IO@@@Z");
        if (!theirs) { std::fprintf(stderr, "SETUP FAILURE: MeshInstance::Serialize not exported\n"); return 2; }
        g_trace[0].clear(); g_trace[1].clear();

        FakeStream st{}; st.mode = streamMode; st.factory = static_cast<void*>(g_factoryObj);
        FakeSerializer ser{ &st, g_encoding };
        g_arm = 0; g_encIntTick = 0; BuildMeshArena(arenaA, liveCount); theirs(arenaA, &ser);
        FakeStream st2{}; st2.mode = streamMode; st2.factory = static_cast<void*>(g_factoryObj);
        FakeSerializer ser2{ &st2, g_encoding };
        g_arm = 1; g_encIntTick = 0; BuildMeshArena(arenaB, liveCount);
        RtGraphics_MeshInstance_Serialize(arenaB, &ser2);

        int a = CompareTrace("MeshInstance::Serialize");
        int b = CompareBytes("MeshInstance::Serialize", arenaA, arenaB);
        screenA += a; screenB += b; bad += (a || b);
    }

    if (wantAnim) {
        using AnimSerFn = void (*)(void*, void*);
        AnimSerFn theirs = (AnimSerFn)GetProcAddress(
            g_rt, "?Serialize@AnimationManager@Graphics@@QEAAXAEAVSerializer@IO@@@Z");
        if (!theirs) { std::fprintf(stderr, "SETUP FAILURE: AnimationManager::Serialize not exported\n"); return 2; }
        g_trace[0].clear(); g_trace[1].clear();

        FakeStream st{}; st.mode = streamMode; st.factory = static_cast<void*>(g_factoryObj);
        FakeSerializer ser{ &st, g_encoding };
        g_arm = 0; g_encIntTick = 0; BuildAnimArena(arenaA, liveCount); theirs(arenaA, &ser);
        FakeStream st2{}; st2.mode = streamMode; st2.factory = static_cast<void*>(g_factoryObj);
        FakeSerializer ser2{ &st2, g_encoding };
        g_arm = 1; g_encIntTick = 0; BuildAnimArena(arenaB, liveCount);
        RtGraphics_AnimationManager_Serialize(arenaB, &ser2);

        int a = CompareTrace("AnimationManager::Serialize");
        int b = CompareBytes("AnimationManager::Serialize", arenaA, arenaB);
        screenA += a; screenB += b; bad += (a || b);
    }

    // PER-SCREEN, NEVER THE UNION. A single "CAUGHT" line here would hide a
    // screen that caught nothing, which is the defect this probe's header block
    // is written around.
    std::printf("VERDICT corrupt=%d : screenA=%s screenB=%s\n", g_corrupt,
                screenA ? "DIVERGED" : "agreed", screenB ? "DIVERGED" : "agreed");
    return bad ? 1 : 0;
}
