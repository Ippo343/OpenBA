// ============================================================================
// runtime/src/video/context.cpp — Wave 6 lane A — Video::Context
// ============================================================================
//
// Ports 23 of the 27 `Video::Context` exports our tree calls (PLAN.md §4.2
// wave 6, §6). The recovered layout and the evidence for every offset are in
// runtime/types/video/Context.h; the per-function write-up, the oracle
// arguments and everything this lane could NOT verify are in
// docs/runtime_port/WAVE6_LANE_A.md.
//
//   | Export (short form)              | Retail      | Ord  | Ported | Oracle |
//   |----------------------------------|-------------|------|--------|--------|
//   | ??0Context (ctor)                | 0x1800f1820 |  125 | NO     | —      |
//   | ??1Context (dtor)                | 0x1800f16e0 |  562 | NO     | —      |
//   | ??3Context (operator delete)     | 0x180018500 |  945 | NO     | —      |
//   | ?AddVertex@                      | 0x18001ad40 | 1592 | yes    | replay |
//   | ?AddVertices@                    | 0x18001ad70 | 1593 | yes    | replay |
//   | ?BeginIndexedVertices@           | 0x1800f2170 | 1644 | yes    | thunk  |
//   | ?BeginInstancedIndexedVertices@  | 0x1800f2130 | 1645 | yes    | thunk  |
//   | ?BeginVertices@                  | 0x1800f21b0 | 1651 | yes    | thunk  |
//   | ?Clear@                          | 0x1800f23f0 | 1705 | yes    | thunk  |
//   | ?ClearResources@                 | 0x1800f2090 | 1712 | NO     | —      |
//   | ?DrawIndexed@                    | 0x1800f2410 | 2278 | yes    | thunk  |
//   | ?EndIndexedVertices@             | 0x1800f2150 | 2312 | yes    | thunk  |
//   | ?EndInstancedIndexedVertices@    | 0x1800f2110 | 2313 | yes    | thunk  |
//   | ?EndVertices@                    | 0x1800f2190 | 2318 | yes    | thunk  |
//   | ?GetMaxDynamicInstances@         | 0x1800f21d0 | 2774 | yes    | thunk  |
//   | ?Reset@                          | 0x1800f1e50 | 3573 | yes    | replay |
//   | ?SetCamera@                      | 0x18001a150 | 3837 | yes    | replay |
//   | ?SetColor@                       | 0x18001a8e0 | 3846 | yes    | replay |
//   | ?SetDefaultRenderBuffer@         | 0x1800ef3c0 | 3866 | yes    | replay |
//   | ?SetFog@                         | 0x18001a360 | 3884 | yes    | replay |
//   | ?SetImageBuffer@                 | 0x180019ed0 | 3893 | yes    | replay |
//   | ?SetInverseScale@                | 0x18001a880 | 3898 | yes    | replay |
//   | ?SetRenderBuffer@                | 0x1800f2100 | 3950 | yes    | replay |
//   | ?SetTime@                        | 0x18001a330 | 3988 | yes    | replay |
//   | ?SetTransform@                   | 0x18001a6b0 | 3991 | yes    | replay |
//   | ?SetVertexBuffer@                | 0x180019f20 | 4006 | yes    | replay |
//   | ?SetViewport@                    | 0x18001a060 | 4011 | yes    | replay |
//
// All 27 mangled names were checked against the shipped export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with pefile BEFORE any body was
// written (reference_resolveexport_phantom_names.md: a name the DLL does not
// contain resolves to NULL in silence, the port still registers, and the
// census still counts it). All 27 are present; the addresses and ordinals in
// the table above are that query's output, not Ghidra's guess.
//
// ---------------------------------------------------------------------------
// WHY REGISTERING THIS PORT IS SAFE, EVEN THOUGH THE DLL KEEPS CALLING ITSELF
// ---------------------------------------------------------------------------
// Registering redirects OUR callers only. The DLL's own `Graphics::Renderer`,
// `UI::UserInterface` and `Engine::MainLoop` reach these same methods through
// direct near-CALLs the registry cannot see — verified, not assumed:
// get_xrefs_to on ?SetTransform@ names three internal callers
// (Graphics `Draw` 0x1800c38da, UI `Begin` 0x18011d389, `DrawItemList`
// 0x1801055f8), and ?Reset@ has one (`Engine::MainLoop::Draw` 0x1800dcd82).
// So for the rest of Wave 6 there really are two implementations operating on
// one live `Video::Context`, which is the hazard WAVE6.md warns about.
//
// It is safe here because of three specific facts, each of which is checked
// rather than assumed:
//
//   1. The class is NOT polymorphic. The ctor stamps no vtable (its first
//      store is the Device pointer) and no Context method is virtual, so there
//      is no vtable-ownership split (PLAN §4.3 risk 3 simply does not apply).
//   2. Nothing ported here allocates, frees, or changes the layout. Every
//      ported body reads and writes fields of an object somebody else owns,
//      at offsets `static_assert`ed in runtime/types/video/Context.h.
//   3. The replay oracle below proves, on every diffed call, that our body
//      leaves the WHOLE 0x2168-byte object byte-identical to what the DLL's
//      body would have left. That is what makes "both implementations on one
//      object" a non-event: after either one runs, the object is in the same
//      state, so the next call — ours or theirs — sees exactly what it expects.
//
// NO DETOUR IS INSTALLED, deliberately. A detour (PLAN §4.1 decision 3) would
// route the DLL's internal render-loop callers into this port too. That is a
// much larger blast radius on the hottest path in the process, and it buys
// nothing the argument above does not already give us: the DLL's callers are
// happy with the DLL's bodies, and our callers are proven equivalent. When
// `Graphics::Renderer` itself is ported (lane F and later waves) those edges
// become ours by ordinary registration and the question disappears.
//
// ---------------------------------------------------------------------------
// WHAT STAYS IN THE DLL, AND WHY
// ---------------------------------------------------------------------------
// THREE FUNCTIONS ARE HELD BACK. All three are lifecycle, all three would
// split ownership of one object between two implementations, and all three
// have ZERO live callers in our tree today.
//
//   ??0Context@Video@@QEAA@PEAVDevice@1@HHHPEAVRenderBuffer@1@@Z  (ctor)
//       The object is created and destroyed entirely inside the DLL:
//       `Engine::MainLoop::CreateVideoContext` (0x1800dd500) allocates 0x2168
//       bytes from `System::MemoryAllocator` and runs this ctor, and
//       `CreateVideoContext` / `ReleaseVideoContext` / `~MainLoop` /
//       `OnVideoError` run the dtor. (This used to say "`Engine::MainLoop` is
//       Wave 8, not Wave 6", which now reads as resolved -- Wave 8 is closed.
//       It is not a wave-ownership question: `engine.cpp:367` calls the DLL's
//       `CreateVideoContext` BY ADDRESS, which is a call-through to the DLL's
//       own ctor, not a reimplementation, so `fn_VideoCtx_Ctor` still has zero
//       callers. Corrected in the Wave 8.5 close-out.)
//       The ctor also allocates FIVE sub-objects (a VertexBuffer, an
//       IndexBuffer, an instance VertexBuffer, a RenderBuffer and two
//       ImageBuffers) through each class's own `operator new`, and calls each
//       one's `PlatformConstruct`. Porting it would mean our code allocates
//       objects the DLL's dtor later frees, on a path we never take.
//       Our tree's only reference is `fn_VideoCtx_Ctor`
//       (engine/funcs/engine_funcs.cpp:1620), and that pointer is NEVER
//       CALLED — see the stale-comment note at the bottom of this file.
//
//   ??1Context@Video@@QEAA@XZ                                     (dtor)
//       The mirror image: it frees those five sub-objects and destroys seven
//       embedded `ConstantBuffer`s. Same ownership split, same zero callers
//       (`fn_VideoCtx_Dtor` is resolved and never called).
//
//   ??3Context@Video@@SAXPEAX@Z                            (operator delete)
//       Three lines, and every one of them is out of scope. It takes the
//       allocator's critical section (`_singleton + 0x640`) and calls
//       `System::MemoryAllocator::Free`. `EnterCriticalSection` is Win32, and
//       PLAN §4.1 decision 1 keeps runtime/src/ platform-neutral outside a
//       `_win` twin; `MemFree` itself is Wave 2's allocator port, which
//       shipped OPT-IN with a NO-GO verdict (WAVE2_ALLOCATOR.md). A Context
//       port has no business being the thing that finally arms it.
//
// A FOURTH IS HELD BACK FOR A DIFFERENT REASON.
//
//   ?ClearResources@Context@Video@@QEAAXXZ
//       It is pure, self-contained and trivially portable — and it has no
//       caller. `fn_VideoCtx_ClearResources` (engine_funcs.cpp:1567) is
//       resolved and never invoked, and no DLL-internal caller exists either.
//       A registered port on a path nothing executes reports `calls=0` and
//       `divergences=0` forever, which reads as a pass and proves nothing;
//       that is exactly the trap reference_uncalled_port_is_not_verified.md
//       records (a wrong `Quaternion::operator*` survived all of Wave 1 that
//       way). An honest hold-back beats an unverifiable port. The body is
//       fully reversed and written up in WAVE6_LANE_A.md, so landing it costs
//       nothing the day something calls it.
//
// The other ~90 exported Context methods (?SetLight@, ?SetBones@,
// ?BeginCommands@, the private ?Bind*@ family, …) are not in this lane's scope
// because our tree never calls them; they stay in the DLL untouched.
//
// ---------------------------------------------------------------------------
// TWO ORACLES
// ---------------------------------------------------------------------------
// (a) REPLAY, for the fourteen state mutators. Snapshot the live 0x2168-byte
//     Context into a scratch copy BEFORE our body runs, run our body on the
//     real object, then run the DLL's body on the scratch copy and compare all
//     0x2168 bytes. `RtDiffReportBytes` names the first differing offset, and
//     the table in Context.h turns that offset straight into a field name.
//     This is a FULL oracle, not a spot check: it catches a wrong offset, a
//     missed dirty bit, a reassociated float and a stray write into padding.
//
// (b) THUNK, for the nine backend forwarders. Those cannot be replayed —
//     running them twice would issue the draw twice. But a forwarder has
//     exactly one thing that can be wrong: which vtable slot it jumps to.
//     `VerifyThunk` below decodes the shipped three-instruction body
//     (`MOV RCX,[RCX+0x2160] / MOV RAX,[RCX] / JMP [RAX+off]`) at registration
//     time and reports, through a normal diff site, whether the offset our
//     port uses is the offset the DLL uses. It also re-derives Context+0x2160
//     from the same bytes. Each check appears in `RtDiffSummary()` as one call.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/video/Context.h"

using RtVideo::Context;
using RtVideo::Viewport;
using RtVideo::Vector4Raw;
using RtVideo::MatrixRaw;
using RtVideo::RenderBufferPrefix;
using RtVideo::kContextSize;

// ----------------------------------------------------------------------------
// ResolveExport, forward-declared rather than pulled in from standalone/resolve.h
// ----------------------------------------------------------------------------
// Same reasoning rt_port.h gives for declaring RegisterRuntimeExport locally:
// resolve.h drags in <windows.h>, and <wingdi.h>'s `Rectangle` function and
// <windef.h>'s `near`/`far` macros collide with the engine's own type and
// member names — and this file has parameters literally called `nearZ`/`farZ`.
// This declaration must stay byte-identical to resolve.h's; the linker checks
// the two against each other. (Precedent: runtime/src/achievement/
// achievement_manager.cpp:71 declares the same prototype for the same reason.)
void* ResolveExport(const char* mangled_name);

namespace {

// ============================================================================
// Mangled names.
//
// The registrations at the bottom of this file deliberately repeat each name
// as its own string literal instead of using these constants:
// tools/runtime_census.py cannot see through a NAMED CONSTANT and would
// under-count the lane, which is how Wave 3's Object count stood at 21/41 when
// it was really 27/41 (reference_census_cannot_see_nonliteral_registrations.md).
// These constants exist only for the diff oracle's RT_ORIGINAL_ONCE lookups.
// ============================================================================
const char* const kAddVertex       = "?AddVertex@Context@Video@@QEAAXPEBX@Z";
const char* const kAddVertices     = "?AddVertices@Context@Video@@QEAAXHPEBX@Z";
const char* const kReset           = "?Reset@Context@Video@@QEAAXXZ";
const char* const kSetCamera       = "?SetCamera@Context@Video@@QEAAXAEBVMatrix@Math@@0@Z";
const char* const kSetColor        = "?SetColor@Context@Video@@QEAAXAEBVVector4@Math@@@Z";
const char* const kSetDefaultRB    = "?SetDefaultRenderBuffer@Context@Video@@QEAAXPEAVRenderBuffer@2@@Z";
const char* const kSetFog          = "?SetFog@Context@Video@@QEAAXMMAEBVVector4@Math@@@Z";
const char* const kSetImageBuffer  = "?SetImageBuffer@Context@Video@@QEAAXPEBVImageBuffer@2@H@Z";
const char* const kSetInverseScale = "?SetInverseScale@Context@Video@@QEAAXMMMM@Z";
const char* const kSetRenderBuffer = "?SetRenderBuffer@Context@Video@@QEAAXPEBVRenderBuffer@2@@Z";
const char* const kSetTime         = "?SetTime@Context@Video@@QEAAXM@Z";
const char* const kSetTransform    = "?SetTransform@Context@Video@@QEAAXHAEBVMatrix@Math@@@Z";
const char* const kSetVertexBuffer = "?SetVertexBuffer@Context@Video@@QEAAXPEBVVertexBuffer@2@HHH@Z";
const char* const kSetViewport     = "?SetViewport@Context@Video@@QEAAXAEBVViewport@2@@Z";

// ============================================================================
// The replay oracle
// ============================================================================
// One file-scope scratch object, not a stack buffer: 0x2168 bytes is 8.5 KB
// and every one of these functions would reserve it on entry whether diffing
// is on or not. It is safe as a single shared buffer because
//
//   * rendering is single-threaded (the DLL's Context is created and driven
//     only from Engine::MainLoop's thread), and
//   * no ported body below calls another ported body, so a replay can never
//     nest. The one function that calls out at all is SetCamera, and it calls
//     Math::Matrix routines, which do not touch a Context.
//
// If either stops being true this must become a per-call allocation. It is
// only ever touched inside `if (rt_diff_on__)`, so a normal run never pays for
// it and never even reads it.
alignas(16) uint8_t g_replayScratch[kContextSize];

// Snapshot the live object so the DLL's implementation can be replayed against
// a private copy. Returns the copy, or nullptr when diffing is off.
inline Context* ReplaySnapshot(bool on, const Context* live) {
    if (!on) return nullptr;
    std::memcpy(g_replayScratch, live, kContextSize);
    return reinterpret_cast<Context*>(g_replayScratch);
}

// ============================================================================
// The thunk oracle
// ============================================================================
// Nine Context methods are three-instruction tail-jumps into the backend:
//
//   488b8960210000   MOV RCX,[RCX+0x2160]      ; this->_contextData
//   488b01           MOV RAX,[RCX]             ; (or 4c8b09 / 4c8b11 — the
//                                             ;  destination register varies)
//   48ffa080000000   JMP qword ptr [RAX+0x80]  ; (or a disp8 form, and the
//                                             ;  REX/rm bits follow the reg)
//
// Decode both displacements out of the shipped bytes and compare them with
// what the port uses. This is the whole failure surface of a forwarder: get
// the slot wrong and you call a different backend method with this method's
// arguments, which is a crash at best and silent wrong rendering at worst.
struct ThunkFacts {
    bool     ok;         // the body matched the expected shape at all
    uint32_t ctxOffset;  // displacement of the MOV RCX,[RCX+disp32]
    uint32_t slotOffset; // displacement of the JMP [reg+disp]
};

ThunkFacts DecodeThunk(const uint8_t* p) {
    ThunkFacts f = { false, 0, 0 };
    if (!p) return f;

    // MOV RCX,[RCX+disp32]  =  48 8b 89 <disp32>
    if (p[0] != 0x48 || p[1] != 0x8b || p[2] != 0x89) return f;
    std::memcpy(&f.ctxOffset, p + 3, 4);

    // MOV <reg>,[RCX]  =  <REX> 8b <modrm with mod=00, rm=001>
    const uint8_t* q = p + 7;
    if ((q[0] & 0xf8) != 0x48 || q[1] != 0x8b || (q[2] & 0xc7) != 0x01) return f;

    // JMP qword ptr [<reg> + disp]  =  <REX> ff <modrm, reg field == 4>
    const uint8_t* r = q + 3;
    if ((r[0] & 0xf8) != 0x48 || r[1] != 0xff) return f;
    const uint8_t modrm = r[2];
    if (((modrm >> 3) & 7) != 4) return f;
    const uint8_t mod = static_cast<uint8_t>(modrm >> 6);
    if (mod == 1) {
        f.slotOffset = r[3];
    } else if (mod == 2) {
        std::memcpy(&f.slotOffset, r + 3, 4);
    } else {
        return f;
    }
    f.ok = true;
    return f;
}

// One diff site per forwarder, filled in at registration time. Declared here
// so each Register* line reads as "port, then prove it".
void VerifyThunk(RtDiffSite* site, const char* mangled, unsigned expectedSlot) {
    if (!RtDiffEnabled()) return;
    RtDiffRegister(site);

    const uint8_t* body = static_cast<const uint8_t*>(ResolveExportRaw(mangled));
    const ThunkFacts f = DecodeThunk(body);

    char ours[64], theirs[96];
    std::snprintf(ours, sizeof(ours), "ctx+0x%x slot+0x%x",
                  (unsigned)offsetof(Context, _contextData), expectedSlot);
    if (!f.ok) {
        std::snprintf(theirs, sizeof(theirs), "unrecognised thunk shape at %p", (const void*)body);
        RtDiffReport(site, false, ours, theirs);
        return;
    }
    std::snprintf(theirs, sizeof(theirs), "ctx+0x%x slot+0x%x", f.ctxOffset, f.slotOffset);
    const bool equal = (f.ctxOffset == offsetof(Context, _contextData)) &&
                       (f.slotOffset == expectedSlot);
    RtDiffReport(site, equal, ours, theirs);
}

// Call the backend through the ContextData vtable, exactly as the retail thunk
// does: `this` becomes the ContextData, every other argument is untouched.
//
// NO NULL CHECK ON _contextData — retail has none (`MOV RCX,[RCX+0x2160]` then
// `MOV RAX,[RCX]` unguarded), and CLAUDE.md's rule applies: a guard here would
// not restore the behaviour, it would skip it. If this ever faults, the
// ContextData was never created and that is the bug to find.
template <typename Fn>
inline Fn BackendSlot(Context* self, unsigned byteOffset) {
    void* data = self->_contextData;
    void** vt = *reinterpret_cast<void***>(data);
    return reinterpret_cast<Fn>(vt[byteOffset / 8]);
}

// ============================================================================
// Math dependencies of SetCamera
// ============================================================================
// `Math::Matrix::Inverse` and `Math::Matrix::operator*` are NOT ported (Wave 1
// landed Matrix::operator* under ??DMatrix@Math@@QEBA?AV01@AEBV01@@Z but not
// Inverse; either way this call site resolves through ResolveExport, so it
// picks up whichever implementation is registered today).
//
// BOTH RETURN A STRUCT BY VALUE, so both carry a HIDDEN RETURN SLOT, and for a
// non-static member function MSVC puts it in RDX — BETWEEN `this` and the
// declared arguments, not before `this`. Getting that wrong is the wild-write
// family: the function still returns the right answer, in RAX, pointing at the
// memory it just corrupted (reference_hidden_retslot_wild_write.md). Confirmed
// against the disassembly of SetCamera itself at 0x18001a165-0x18001a170:
// `LEA RDX,[RSP+0x20] / MOV RCX,RDI / CALL 0x1800bb310`, i.e. RCX = the matrix,
// RDX = the scratch return slot.
using MatrixInverseFn = MatrixRaw* (*)(const MatrixRaw* self, MatrixRaw* ret);
using MatrixMulFn     = MatrixRaw* (*)(const MatrixRaw* self, MatrixRaw* ret,
                                       const MatrixRaw* rhs);

MatrixInverseFn MatrixInverse() {
    static MatrixInverseFn fn = reinterpret_cast<MatrixInverseFn>(
        ResolveExport("?Inverse@Matrix@Math@@QEBA?AV12@XZ"));
    return fn;
}
MatrixMulFn MatrixMul() {
    static MatrixMulFn fn = reinterpret_cast<MatrixMulFn>(
        ResolveExport("??DMatrix@Math@@QEBA?AV01@AEBV01@@Z"));
    return fn;
}

// Math::Matrix::operator!= (0x18001d850, ordinal 1286) inlined. Retail's body
// is sixteen `==` comparisons with the same short-circuit structure; inlining
// it here reproduces the result exactly (including the NaN case, where every
// `==` is false and the operator reports "not equal") and keeps SetTransform
// free of a resolve on the hottest setter in the class.
inline bool MatrixNotEqual(const MatrixRaw* a, const MatrixRaw* b) {
    for (int i = 0; i < 16; ++i) {
        if (!(a->m[i] == b->m[i])) return true;
    }
    return false;
}

} // namespace

// ============================================================================
// The ports
// ============================================================================
// Every body is byte-faithful to the retail decompilation. Where retail writes
// a field it does not compare first, so do we; where retail compares before
// writing (so the dirty bit is only set on a real change), so do we — that
// early-out is observable state, not an optimisation.

// ---- ?SetColor@Context@Video@@QEAAXAEBVVector4@Math@@@Z  0x18001a8e0 -------
void RtVideo_Context_SetColor(Context* self, const Vector4Raw* color) {
    RT_DIFF_BEGIN("Video::Context::SetColor");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    if (self->_color.x != color->x || self->_color.y != color->y ||
        self->_color.z != color->z || self->_color.w != color->w) {
        self->_color = *color;
        self->_dirty |= RtVideo::kDirtyMaterialConstants;
    }

    if (rt_diff_on__) {
        using Fn = void (*)(Context*, const Vector4Raw*);
        RT_ORIGINAL_ONCE(Fn, kSetColor)(replay, color);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ---- ?SetInverseScale@Context@Video@@QEAAXMMMM@Z  0x18001a880 --------------
void RtVideo_Context_SetInverseScale(Context* self, float x, float y, float z, float w) {
    RT_DIFF_BEGIN("Video::Context::SetInverseScale");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    if (x != self->_inverseScale.x || y != self->_inverseScale.y ||
        z != self->_inverseScale.z || w != self->_inverseScale.w) {
        self->_inverseScale.x = x;
        self->_inverseScale.y = y;
        self->_inverseScale.z = z;
        self->_inverseScale.w = w;
        self->_dirty |= RtVideo::kDirtyMaterialConstants;
    }

    if (rt_diff_on__) {
        using Fn = void (*)(Context*, float, float, float, float);
        RT_ORIGINAL_ONCE(Fn, kSetInverseScale)(replay, x, y, z, w);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ---- ?SetTime@Context@Video@@QEAAXM@Z  0x18001a330 ------------------------
// Note there is no compare-first here: retail writes the four floats and ORs
// the dirty bit unconditionally, and it shares bit 0x800 with SetCamera.
void RtVideo_Context_SetTime(Context* self, float time) {
    RT_DIFF_BEGIN("Video::Context::SetTime");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    self->_time.x = time;
    self->_time.y = 0.0f;
    self->_time.z = 0.0f;
    self->_time.w = 0.0f;
    self->_dirty |= RtVideo::kDirtyGlobalConstants;

    if (rt_diff_on__) {
        using Fn = void (*)(Context*, float);
        RT_ORIGINAL_ONCE(Fn, kSetTime)(replay, time);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ---- ?SetFog@Context@Video@@QEAAXMMAEBVVector4@Math@@@Z  0x18001a360 ------
// _fogParams = { -nearZ, 1 / (farZ - nearZ), 0, 0 }, and the colour is stored
// TWICE: once at +0x3e8 and once at +0x258, inside the pixel-constant staging
// block. Both stores are retail's; the second is not a typo in the decompile.
//
// The negation is a SIGN-BIT XOR (`XORPS XMM3,[0x1801617c0]`, a 16-byte
// 0x80000000 quad), not `0 - nearZ`. Reproduced bitwise so that a fog plane at
// exactly 0.0f yields -0.0f here, the same as retail.
void RtVideo_Context_SetFog(Context* self, float nearZ, float farZ, const Vector4Raw* color) {
    RT_DIFF_BEGIN("Video::Context::SetFog");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    uint32_t nearBits;
    std::memcpy(&nearBits, &nearZ, 4);
    nearBits ^= 0x80000000u;
    float negNear;
    std::memcpy(&negNear, &nearBits, 4);

    const float invRange = 1.0f / (farZ - nearZ);

    self->_fogParams.x = negNear;
    self->_fogParams.y = invRange;
    self->_fogParams.z = 0.0f;
    self->_fogParams.w = 0.0f;
    self->_fogColor = *color;

    // The +0x258 duplicate, inside the pixel-constant staging block.
    std::memcpy(self->_pixelConstantStaging + (0x258 - 0x200), color, sizeof(Vector4Raw));

    self->_dirty |= (RtVideo::kDirtyGlobalConstants | RtVideo::kDirtyPixelConstants);

    if (rt_diff_on__) {
        using Fn = void (*)(Context*, float, float, const Vector4Raw*);
        RT_ORIGINAL_ONCE(Fn, kSetFog)(replay, nearZ, farZ, color);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ---- ?SetViewport@Context@Video@@QEAAXAEBVViewport@2@@Z  0x18001a060 ------
void RtVideo_Context_SetViewport(Context* self, const Viewport* vp) {
    RT_DIFF_BEGIN("Video::Context::SetViewport");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    if (self->_viewport.x != vp->x || self->_viewport.y != vp->y ||
        self->_viewport.width != vp->width || self->_viewport.height != vp->height) {
        // Retail sets the dirty bit BEFORE copying; the order is not
        // observable from outside but is kept for byte-faithfulness.
        self->_dirty |= RtVideo::kDirtyViewport;
        self->_viewport = *vp;
    }

    if (rt_diff_on__) {
        using Fn = void (*)(Context*, const Viewport*);
        RT_ORIGINAL_ONCE(Fn, kSetViewport)(replay, vp);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ---- ?SetImageBuffer@Context@Video@@QEAAXPEBVImageBuffer@2@H@Z  0x180019ed0 -
// `1 << (slot & 0x1f)` is retail's own masking; with 16 slots it never wraps,
// but the AND is in the shipped code and is kept.
void RtVideo_Context_SetImageBuffer(Context* self, const void* image, int slot) {
    RT_DIFF_BEGIN("Video::Context::SetImageBuffer");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    if (image != self->_imageBuffers[slot]) {
        self->_dirty |= RtVideo::kDirtyImageBuffers;
        self->_imageBufferDirty |= 1u << (static_cast<unsigned>(slot) & 0x1fu);
        self->_imageBuffers[slot] = const_cast<void*>(image);
    }

    if (rt_diff_on__) {
        using Fn = void (*)(Context*, const void*, int);
        RT_ORIGINAL_ONCE(Fn, kSetImageBuffer)(replay, image, slot);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ---- ?SetVertexBuffer@Context@Video@@QEAAXPEBVVertexBuffer@2@HHH@Z  0x180019f20
void RtVideo_Context_SetVertexBuffer(Context* self, const void* vb, int slot,
                                     int offset, int stride) {
    RT_DIFF_BEGIN("Video::Context::SetVertexBuffer");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    if (vb != self->_vertexBuffers[slot] || offset != self->_vertexOffsets[slot] ||
        stride != self->_vertexStrides[slot]) {
        self->_dirty |= RtVideo::kDirtyVertexBuffers;
        self->_vertexBufferDirty |= 1u << (static_cast<unsigned>(slot) & 0x1fu);
        self->_vertexBuffers[slot] = const_cast<void*>(vb);
        self->_vertexOffsets[slot] = offset;
        self->_vertexStrides[slot] = stride;
    }

    if (rt_diff_on__) {
        using Fn = void (*)(Context*, const void*, int, int, int);
        RT_ORIGINAL_ONCE(Fn, kSetVertexBuffer)(replay, vb, slot, offset, stride);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ---- ?SetRenderBuffer@Context@Video@@QEAAXPEBVRenderBuffer@2@@Z  0x1800f2100
void RtVideo_Context_SetRenderBuffer(Context* self, const void* rb) {
    RT_DIFF_BEGIN("Video::Context::SetRenderBuffer");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    if (self->_renderBuffer != rb) {
        self->_dirty |= RtVideo::kDirtyRenderBuffer;
        self->_renderBuffer = const_cast<void*>(rb);
    }

    if (rt_diff_on__) {
        using Fn = void (*)(Context*, const void*);
        RT_ORIGINAL_ONCE(Fn, kSetRenderBuffer)(replay, rb);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ---- ?SetDefaultRenderBuffer@Context@Video@@QEAAXPEAVRenderBuffer@2@@Z  0x1800ef3c0
// One store, no compare, no dirty bit. Note this OVERWRITES the pointer the
// ctor may have allocated (+0x2138) without freeing it — retail leaks it
// deliberately, because the caller owns the buffer it hands in. Faithfully
// reproduced; do not add a delete here.
void RtVideo_Context_SetDefaultRenderBuffer(Context* self, void* rb) {
    RT_DIFF_BEGIN("Video::Context::SetDefaultRenderBuffer");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    self->_defaultRenderBuffer = rb;

    if (rt_diff_on__) {
        using Fn = void (*)(Context*, void*);
        RT_ORIGINAL_ONCE(Fn, kSetDefaultRB)(replay, rb);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ---- ?SetTransform@Context@Video@@QEAAXHAEBVMatrix@Math@@@Z  0x18001a6b0 ---
// The transform-count update is OUTSIDE the change test: retail raises the
// high-water mark even when the matrix is unchanged. That asymmetry is the
// kind of thing a "tidied" port silently drops.
void RtVideo_Context_SetTransform(Context* self, int slot, const MatrixRaw* m) {
    RT_DIFF_BEGIN("Video::Context::SetTransform");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    MatrixRaw* dst = &self->_transforms[slot];
    if (MatrixNotEqual(m, dst)) {
        *dst = *m;
        self->_dirty |= RtVideo::kDirtyTransformConstants;
    }
    if (self->_transformCount < slot + 1) {
        self->_transformCount = slot + 1;
    }

    if (rt_diff_on__) {
        using Fn = void (*)(Context*, int, const MatrixRaw*);
        RT_ORIGINAL_ONCE(Fn, kSetTransform)(replay, slot, m);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ---- ?SetCamera@Context@Video@@QEAAXAEBVMatrix@Math@@0@Z  0x18001a150 -----
// _inverseView   = view.Inverse()
// _viewProjection = _inverseView * projection      (in that order — the left
//                                                   operand is the FIELD that
//                                                   was just written, not the
//                                                   local result)
// _cameraPosition = view row 3
void RtVideo_Context_SetCamera(Context* self, const MatrixRaw* view, const MatrixRaw* projection) {
    RT_DIFF_BEGIN("Video::Context::SetCamera");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    MatrixRaw scratch;
    self->_inverseView = *MatrixInverse()(view, &scratch);
    self->_viewProjection = *MatrixMul()(&self->_inverseView, &scratch, projection);

    self->_cameraPosition.x = view->m[12];
    self->_cameraPosition.y = view->m[13];
    self->_cameraPosition.z = view->m[14];
    self->_cameraPosition.w = view->m[15];

    self->_dirty |= RtVideo::kDirtyGlobalConstants;

    if (rt_diff_on__) {
        using Fn = void (*)(Context*, const MatrixRaw*, const MatrixRaw*);
        RT_ORIGINAL_ONCE(Fn, kSetCamera)(replay, view, projection);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ---- ?Reset@Context@Video@@QEAAXXZ  0x1800f1e50 ---------------------------
// The frame reset: clear every binding, mark everything dirty, and re-arm the
// viewport and scissor to the default render buffer's dimensions.
//
// Two details a readable rewrite would get wrong and the replay oracle would
// catch: (1) `_dirty` is ASSIGNED 0x1fe00 and then OR'd bit by bit as each
// binding is found non-null, so a Reset on an already-clean Context leaves a
// different value from one on a dirty Context; (2) the vertex-slot test also
// fires when only the offset or stride is non-zero, while the image-slot test
// looks at the pointer alone.
void RtVideo_Context_Reset(Context* self) {
    RT_DIFF_BEGIN("Video::Context::Reset");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    self->_dirty = RtVideo::kDirtyAllConstants;

    // Retail's three leading memsets. Their boundaries do NOT line up with
    // named fields: the first two start at +0x208 and +0x248 — eight bytes
    // past _pixelProgram, inside the pixel-constant staging block — and stop
    // short of _materialConstants at +0x298, which Reset does not clear. They
    // are written as raw offsets for that reason, not out of laziness; naming
    // them through a field would have changed the region.
    std::memset(reinterpret_cast<uint8_t*>(self) + 0x208, 0, 0x40);
    std::memset(reinterpret_cast<uint8_t*>(self) + 0x248, 0, 0x50);
    std::memset(reinterpret_cast<uint8_t*>(self) + 0x328, 0, 0xd0);
    self->_inverseScale = Vector4Raw{ 0.0f, 0.0f, 0.0f, 0.0f };
    self->_color        = Vector4Raw{ 0.0f, 0.0f, 0.0f, 0.0f };
    std::memset(self->_boneAndProjectionStaging, 0, sizeof(self->_boneAndProjectionStaging));
    std::memset(self->_transforms, 0, sizeof(self->_transforms));
    std::memset(self->_lightStaging, 0, sizeof(self->_lightStaging));

    self->_transformCount = 0;
    self->_unknown0024    = 0;   // retail zeroes +0x20 as one qword

    for (int i = 0; i < RtVideo::kImageBufferSlots; ++i) {
        if (self->_imageBuffers[i] != nullptr) {
            self->_dirty |= RtVideo::kDirtyImageBuffers;
            self->_imageBufferDirty |= 1u << i;
            self->_imageBuffers[i] = nullptr;
        }
    }
    for (int i = 0; i < RtVideo::kVertexBufferSlots; ++i) {
        if (self->_vertexBuffers[i] != nullptr || self->_vertexOffsets[i] != 0 ||
            self->_vertexStrides[i] != 0) {
            self->_dirty |= RtVideo::kDirtyVertexBuffers;
            self->_vertexBufferDirty |= 1u << i;
            self->_vertexBuffers[i] = nullptr;
            self->_vertexOffsets[i] = 0;
            self->_vertexStrides[i] = 0;
        }
    }
    if (self->_indexBuffer != nullptr || self->_indexOffset != 0) {
        self->_dirty |= RtVideo::kDirtyIndexBuffer;
        self->_indexBuffer = nullptr;
        self->_indexOffset = 0;
    }
    if (self->_renderBuffer != nullptr) {
        self->_dirty |= RtVideo::kDirtyRenderBuffer;
        self->_renderBuffer = nullptr;
    }
    if (self->_vertexLayout != nullptr) {
        self->_dirty |= RtVideo::kDirtyVertexLayout;
        self->_vertexLayout = nullptr;
    }
    if (self->_vertexProgram != nullptr) {
        self->_dirty |= RtVideo::kDirtyVertexProgram;
        self->_vertexProgram = nullptr;
    }
    if (self->_pixelProgram != nullptr) {
        self->_dirty |= RtVideo::kDirtyPixelProgram;
        self->_pixelProgram = nullptr;
    }

    // Unguarded deref of _defaultRenderBuffer, exactly as retail: it is set by
    // the ctor (or by SetDefaultRenderBuffer) and Reset relies on it being
    // there. CLAUDE.md: do not "fix" this with a null check.
    const RenderBufferPrefix* rb =
        static_cast<const RenderBufferPrefix*>(self->_defaultRenderBuffer);
    const int32_t w = rb->width;
    const int32_t h = rb->height;

    if (self->_viewport.x != 0 || self->_viewport.y != 0 ||
        self->_viewport.width != w || self->_viewport.height != h) {
        self->_dirty |= RtVideo::kDirtyViewport;
        self->_viewport.x = 0;
        self->_viewport.y = 0;
        self->_viewport.width  = w;
        self->_viewport.height = h;
    }
    // Retail re-reads the two dimensions from the render buffer here rather
    // than reusing the values above. Kept.
    const int32_t w2 = rb->width;
    const int32_t h2 = rb->height;
    if (self->_scissor.x != 0 || self->_scissor.y != 0 ||
        self->_scissor.width != w2 || self->_scissor.height != h2) {
        self->_dirty |= RtVideo::kDirtyScissor;
        self->_scissor.x = 0;
        self->_scissor.y = 0;
        self->_scissor.width  = w2;
        self->_scissor.height = h2;
    }
    if (self->_renderState != nullptr) {
        self->_dirty |= RtVideo::kDirtyRenderState;
        self->_renderState = nullptr;
    }

    if (rt_diff_on__) {
        using Fn = void (*)(Context*);
        RT_ORIGINAL_ONCE(Fn, kReset)(replay);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ---- ?AddVertex@Context@Video@@QEAAXPEBX@Z  0x18001ad40 -------------------
// Copies one vertex of `_dynamicVertexStride` bytes to the mapped write cursor
// and advances it. No bounds check — the caller promised a count to
// BeginVertices and retail trusts it.
//
// REPLAY NOTE. The DLL's body writes through the cursor too, into the SAME
// mapped buffer at the SAME address with the SAME source bytes (the scratch
// Context is a copy, so its cursor is the value ours started from). The second
// write is byte-for-byte identical to the first, so replaying it is idempotent
// rather than a second, different write into live state. That is the one place
// in this lane where the original touches something outside the scratch copy,
// and it is deliberate: the alternative is no oracle at all.
void RtVideo_Context_AddVertex(Context* self, const void* vertex) {
    RT_DIFF_BEGIN("Video::Context::AddVertex");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    const size_t stride = static_cast<size_t>(self->_dynamicVertexStride);
    std::memcpy(self->_dynamicVertexCursor, vertex, stride);
    self->_dynamicVertexCursor += stride;

    if (rt_diff_on__) {
        using Fn = void (*)(Context*, const void*);
        RT_ORIGINAL_ONCE(Fn, kAddVertex)(replay, vertex);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ---- ?AddVertices@Context@Video@@QEAAXHPEBX@Z  0x18001ad70 ----------------
// The byte count is `count * stride` computed in 32-bit INT and then widened,
// which is what retail does (`IMUL` on the 32-bit registers before the
// sign-extending move). A count large enough to overflow would wrap the same
// way in both. Reproduced rather than promoted to 64-bit.
void RtVideo_Context_AddVertices(Context* self, int count, const void* vertices) {
    RT_DIFF_BEGIN("Video::Context::AddVertices");
    Context* replay = ReplaySnapshot(rt_diff_on__, self);

    const int32_t bytes = count * self->_dynamicVertexStride;
    std::memcpy(self->_dynamicVertexCursor, vertices, static_cast<size_t>(bytes));
    self->_dynamicVertexCursor += bytes;

    if (rt_diff_on__) {
        using Fn = void (*)(Context*, int, const void*);
        RT_ORIGINAL_ONCE(Fn, kAddVertices)(replay, count, vertices);
        RT_DIFF_BYTES(self, replay, kContextSize);
    }
}

// ============================================================================
// The nine backend forwarders
//
// Each is a one-line tail call through the ContextData vtable at the byte
// offset named in runtime/types/video/Context.h. Their oracle is VerifyThunk,
// run once at registration; there is no per-call comparison because running
// the original as well would submit the draw twice.
// ============================================================================

void RtVideo_Context_Clear(Context* self, unsigned flags, unsigned color,
                           float depth, int stencil) {
    using Fn = void (*)(void*, unsigned, unsigned, float, int);
    BackendSlot<Fn>(self, RtVideo::kSlotClear)(self->_contextData, flags, color, depth, stencil);
}

void RtVideo_Context_DrawIndexed(Context* self, int primitiveType, int a, int b,
                                 int c, int d, int e) {
    using Fn = void (*)(void*, int, int, int, int, int, int);
    BackendSlot<Fn>(self, RtVideo::kSlotDrawIndexed)(self->_contextData, primitiveType,
                                                     a, b, c, d, e);
}

int RtVideo_Context_GetMaxDynamicInstances(Context* self, int a, int b, int c) {
    using Fn = int (*)(void*, int, int, int);
    return BackendSlot<Fn>(self, RtVideo::kSlotGetMaxDynamicInstances)(self->_contextData, a, b, c);
}

bool RtVideo_Context_BeginVertices(Context* self, int count, int stride) {
    using Fn = bool (*)(void*, int, int);
    return BackendSlot<Fn>(self, RtVideo::kSlotBeginVertices)(self->_contextData, count, stride);
}

void RtVideo_Context_EndVertices(Context* self, int primitiveType) {
    using Fn = void (*)(void*, int);
    BackendSlot<Fn>(self, RtVideo::kSlotEndVertices)(self->_contextData, primitiveType);
}

bool RtVideo_Context_BeginIndexedVertices(Context* self, int a, int b, int c) {
    using Fn = bool (*)(void*, int, int, int);
    return BackendSlot<Fn>(self, RtVideo::kSlotBeginIndexedVertices)(self->_contextData, a, b, c);
}

void RtVideo_Context_EndIndexedVertices(Context* self, int primitiveType) {
    using Fn = void (*)(void*, int);
    BackendSlot<Fn>(self, RtVideo::kSlotEndIndexedVertices)(self->_contextData, primitiveType);
}

bool RtVideo_Context_BeginInstancedIndexedVertices(Context* self, int a, int b, int c, int d) {
    using Fn = bool (*)(void*, int, int, int, int);
    return BackendSlot<Fn>(self, RtVideo::kSlotBeginInstancedIndexedVertices)(
        self->_contextData, a, b, c, d);
}

void RtVideo_Context_EndInstancedIndexedVertices(Context* self, int primitiveType) {
    using Fn = void (*)(void*, int);
    BackendSlot<Fn>(self, RtVideo::kSlotEndInstancedIndexedVertices)(self->_contextData,
                                                                     primitiveType);
}

// ============================================================================
// Registration
//
// NAME ONLY, and that is a considered decision rather than an omission. Every
// one of this lane's live call sites resolves the method BY MANGLED NAME
// through ResolveExport — engine/funcs/engine_funcs.cpp,
// code/Game/Component/statusicon_geometry.cpp, code/Game/Game/minimap_paint.cpp,
// code/Game/Game/gameupdateloop.cpp, code/Game/Game/weather_sun.cpp,
// code/Game/Game/weather_particle_draw.cpp and
// code/Game/Player/tool_base_tilerender.cpp — and a grep across code/, engine/,
// standalone/, runtime/ and dx11/ finds NO `ResolveAddr(Module::Runtime, …)`
// for any ported Context address. The retail address is registered anyway, so
// that a future address-form call site cannot split one live Context between
// two implementations without anybody noticing (Wave 5 lane D's lesson).
//
// The three exe dispatch slots that carry Context methods
// (0x1401525e0 SetTransform, 0x140152930 BeginVertices, 0x140152948 AddVertex,
// 0x140152938 EndVertices, per the comments in tool_base_tilerender.cpp) are
// filled by `RegisterDispatchSlot(slot, ResolveExport(name))` in
// standalone/address_registry.cpp, and `AddressRegistryInit()` runs AFTER
// `RuntimePortInit()` (standalone/main.cpp:1614 then :1626). So those slots
// pick up this port as well, without this file touching them.
// ============================================================================

int RtVideoContextRegister() {
    int count = 0;

    // ---- state mutators (replay oracle) ------------------------------------
    RegisterRuntimeExport("?SetColor@Context@Video@@QEAAXAEBVVector4@Math@@@Z", (void*)&RtVideo_Context_SetColor); ++count;
    RegisterRuntimeAddr(0x18001a8e0, (void*)&RtVideo_Context_SetColor);

    RegisterRuntimeExport("?SetInverseScale@Context@Video@@QEAAXMMMM@Z", (void*)&RtVideo_Context_SetInverseScale); ++count;
    RegisterRuntimeAddr(0x18001a880, (void*)&RtVideo_Context_SetInverseScale);

    RegisterRuntimeExport("?SetTime@Context@Video@@QEAAXM@Z", (void*)&RtVideo_Context_SetTime); ++count;
    RegisterRuntimeAddr(0x18001a330, (void*)&RtVideo_Context_SetTime);

    RegisterRuntimeExport("?SetFog@Context@Video@@QEAAXMMAEBVVector4@Math@@@Z", (void*)&RtVideo_Context_SetFog); ++count;
    RegisterRuntimeAddr(0x18001a360, (void*)&RtVideo_Context_SetFog);

    RegisterRuntimeExport("?SetViewport@Context@Video@@QEAAXAEBVViewport@2@@Z", (void*)&RtVideo_Context_SetViewport); ++count;
    RegisterRuntimeAddr(0x18001a060, (void*)&RtVideo_Context_SetViewport);

    RegisterRuntimeExport("?SetImageBuffer@Context@Video@@QEAAXPEBVImageBuffer@2@H@Z", (void*)&RtVideo_Context_SetImageBuffer); ++count;
    RegisterRuntimeAddr(0x180019ed0, (void*)&RtVideo_Context_SetImageBuffer);

    RegisterRuntimeExport("?SetVertexBuffer@Context@Video@@QEAAXPEBVVertexBuffer@2@HHH@Z", (void*)&RtVideo_Context_SetVertexBuffer); ++count;
    RegisterRuntimeAddr(0x180019f20, (void*)&RtVideo_Context_SetVertexBuffer);

    RegisterRuntimeExport("?SetRenderBuffer@Context@Video@@QEAAXPEBVRenderBuffer@2@@Z", (void*)&RtVideo_Context_SetRenderBuffer); ++count;
    RegisterRuntimeAddr(0x1800f2100, (void*)&RtVideo_Context_SetRenderBuffer);

    RegisterRuntimeExport("?SetDefaultRenderBuffer@Context@Video@@QEAAXPEAVRenderBuffer@2@@Z", (void*)&RtVideo_Context_SetDefaultRenderBuffer); ++count;
    RegisterRuntimeAddr(0x1800ef3c0, (void*)&RtVideo_Context_SetDefaultRenderBuffer);

    RegisterRuntimeExport("?SetTransform@Context@Video@@QEAAXHAEBVMatrix@Math@@@Z", (void*)&RtVideo_Context_SetTransform); ++count;
    RegisterRuntimeAddr(0x18001a6b0, (void*)&RtVideo_Context_SetTransform);

    RegisterRuntimeExport("?SetCamera@Context@Video@@QEAAXAEBVMatrix@Math@@0@Z", (void*)&RtVideo_Context_SetCamera); ++count;
    RegisterRuntimeAddr(0x18001a150, (void*)&RtVideo_Context_SetCamera);

    RegisterRuntimeExport("?Reset@Context@Video@@QEAAXXZ", (void*)&RtVideo_Context_Reset); ++count;
    RegisterRuntimeAddr(0x1800f1e50, (void*)&RtVideo_Context_Reset);

    RegisterRuntimeExport("?AddVertex@Context@Video@@QEAAXPEBX@Z", (void*)&RtVideo_Context_AddVertex); ++count;
    RegisterRuntimeAddr(0x18001ad40, (void*)&RtVideo_Context_AddVertex);

    RegisterRuntimeExport("?AddVertices@Context@Video@@QEAAXHPEBX@Z", (void*)&RtVideo_Context_AddVertices); ++count;
    RegisterRuntimeAddr(0x18001ad70, (void*)&RtVideo_Context_AddVertices);

    // ---- backend forwarders (thunk oracle) ---------------------------------
    // Each site is static so it survives past this function and shows up in
    // RtDiffSummary() with the one call VerifyThunk makes.
    {
        static RtDiffSite site = { "Video::Context::Clear[slot]", 0, 0, false, false };
        RegisterRuntimeExport("?Clear@Context@Video@@QEAAXIIMH@Z", (void*)&RtVideo_Context_Clear); ++count;
        RegisterRuntimeAddr(0x1800f23f0, (void*)&RtVideo_Context_Clear);
        VerifyThunk(&site, "?Clear@Context@Video@@QEAAXIIMH@Z", RtVideo::kSlotClear);
    }
    {
        static RtDiffSite site = { "Video::Context::DrawIndexed[slot]", 0, 0, false, false };
        RegisterRuntimeExport("?DrawIndexed@Context@Video@@QEAAXW4PrimitiveType@12@HHHHH@Z", (void*)&RtVideo_Context_DrawIndexed); ++count;
        RegisterRuntimeAddr(0x1800f2410, (void*)&RtVideo_Context_DrawIndexed);
        VerifyThunk(&site, "?DrawIndexed@Context@Video@@QEAAXW4PrimitiveType@12@HHHHH@Z", RtVideo::kSlotDrawIndexed);
    }
    {
        static RtDiffSite site = { "Video::Context::GetMaxDynamicInstances[slot]", 0, 0, false, false };
        RegisterRuntimeExport("?GetMaxDynamicInstances@Context@Video@@QEBAHHHH@Z", (void*)&RtVideo_Context_GetMaxDynamicInstances); ++count;
        RegisterRuntimeAddr(0x1800f21d0, (void*)&RtVideo_Context_GetMaxDynamicInstances);
        VerifyThunk(&site, "?GetMaxDynamicInstances@Context@Video@@QEBAHHHH@Z", RtVideo::kSlotGetMaxDynamicInstances);
    }
    {
        static RtDiffSite site = { "Video::Context::BeginVertices[slot]", 0, 0, false, false };
        RegisterRuntimeExport("?BeginVertices@Context@Video@@QEAA_NHH@Z", (void*)&RtVideo_Context_BeginVertices); ++count;
        RegisterRuntimeAddr(0x1800f21b0, (void*)&RtVideo_Context_BeginVertices);
        VerifyThunk(&site, "?BeginVertices@Context@Video@@QEAA_NHH@Z", RtVideo::kSlotBeginVertices);
    }
    {
        static RtDiffSite site = { "Video::Context::EndVertices[slot]", 0, 0, false, false };
        RegisterRuntimeExport("?EndVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z", (void*)&RtVideo_Context_EndVertices); ++count;
        RegisterRuntimeAddr(0x1800f2190, (void*)&RtVideo_Context_EndVertices);
        VerifyThunk(&site, "?EndVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z", RtVideo::kSlotEndVertices);
    }
    {
        static RtDiffSite site = { "Video::Context::BeginIndexedVertices[slot]", 0, 0, false, false };
        RegisterRuntimeExport("?BeginIndexedVertices@Context@Video@@QEAA_NHHH@Z", (void*)&RtVideo_Context_BeginIndexedVertices); ++count;
        RegisterRuntimeAddr(0x1800f2170, (void*)&RtVideo_Context_BeginIndexedVertices);
        VerifyThunk(&site, "?BeginIndexedVertices@Context@Video@@QEAA_NHHH@Z", RtVideo::kSlotBeginIndexedVertices);
    }
    {
        static RtDiffSite site = { "Video::Context::EndIndexedVertices[slot]", 0, 0, false, false };
        RegisterRuntimeExport("?EndIndexedVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z", (void*)&RtVideo_Context_EndIndexedVertices); ++count;
        RegisterRuntimeAddr(0x1800f2150, (void*)&RtVideo_Context_EndIndexedVertices);
        VerifyThunk(&site, "?EndIndexedVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z", RtVideo::kSlotEndIndexedVertices);
    }
    {
        static RtDiffSite site = { "Video::Context::BeginInstancedIndexedVertices[slot]", 0, 0, false, false };
        RegisterRuntimeExport("?BeginInstancedIndexedVertices@Context@Video@@QEAA_NHHHH@Z", (void*)&RtVideo_Context_BeginInstancedIndexedVertices); ++count;
        RegisterRuntimeAddr(0x1800f2130, (void*)&RtVideo_Context_BeginInstancedIndexedVertices);
        VerifyThunk(&site, "?BeginInstancedIndexedVertices@Context@Video@@QEAA_NHHHH@Z", RtVideo::kSlotBeginInstancedIndexedVertices);
    }
    {
        static RtDiffSite site = { "Video::Context::EndInstancedIndexedVertices[slot]", 0, 0, false, false };
        RegisterRuntimeExport("?EndInstancedIndexedVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z", (void*)&RtVideo_Context_EndInstancedIndexedVertices); ++count;
        RegisterRuntimeAddr(0x1800f2110, (void*)&RtVideo_Context_EndInstancedIndexedVertices);
        VerifyThunk(&site, "?EndInstancedIndexedVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z", RtVideo::kSlotEndInstancedIndexedVertices);
    }

    return count;
}

// ============================================================================
// STALE COMMENTS THIS LANE DISPROVED (PLAN §6 step 8)
// ============================================================================
// Lane A may only edit its own three files, so the corrections below are
// recorded here and in docs/runtime_port/WAVE6_LANE_A.md for the wave lead to
// apply at their source.
//
//  1. docs/runtime_port/wave6_lanes.txt lists all 27 lane-A rows with
//     `kind = export`. THREE OF THEM ARE RESOLVED BY ADDRESS, not by name:
//     engine/funcs/engine_funcs.cpp:1620 resolves the ctor with
//     `ResolveAddr(Module::Runtime, 0x1800f1820)` and :1628 resolves
//     `operator delete` with `ResolveAddr(Module::Runtime, 0x180018500)`;
//     the dtor at :1622 tries the name first and falls back to
//     `ResolveAddr(Module::Runtime, 0x1800f16e0)`. A name-only registration of
//     those three would have registered nothing anybody calls — which happens
//     to be harmless here only because all three are held back.
//
//  2. `fn_VideoCtx_Ctor`, `fn_VideoCtx_Dtor`, `fn_VideoCtx_Delete` and
//     `fn_VideoCtx_ClearResources` (engine/funcs/engine_funcs.h:95-104 and
//     :50-51) are DEAD RESOLVES. All four are assigned in `EngineFuncsInit`
//     and the only other mention of any of them is the diagnostic printf at
//     engine/funcs/engine_funcs.cpp:4123-4141. Nothing in code/, engine/,
//     standalone/, runtime/ or dx11/ ever calls one. `fn_VideoCtx_Reset` is
//     the only member of that group with a live caller
//     (standalone/main.cpp:1321). The header comment "Video::Context
//     lifecycle" is accurate about what they ARE and silent about the fact
//     that nothing drives them; a `// resolved, never called` line on each
//     would have saved this lane the grep.
// ============================================================================
