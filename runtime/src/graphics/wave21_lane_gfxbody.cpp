// ============================================================================
// runtime/src/graphics/wave21_lane_gfxbody.cpp — Wave 21 lane GFXBODY
// ============================================================================
//
// Three of the thirteen CODE rows of the Graphics census gap. The triage that
// picked these three out of the thirteen, with a measured size and callee list
// for every row and the re-check of four inherited holdbacks, is
// docs/runtime_port/WAVE21_LANE_GFXBODY.md. Read §3 before adding a fourth.
//
//   | Export                                     | Retail      | Ord  | Bytes |
//   |--------------------------------------------|-------------|------|-------|
//   | ??2FrameCopyImageBuffer@Graphics@@SAPEAX_K@Z| 0x18001efc0 |  787 |    99 |
//   | ??2MaterialInstance@Graphics@@SAPEAX_K@Z    | 0x180002820 |  823 |    99 |
//   | ?Setup@MaterialInstance@Graphics@@QEBAX...  | 0x180102c90 | 4028 |   332 |
//
// All three names, and every callee named below, were checked against the
// export directory of the shipped C:\GOG Games\Banished\Runtime-x64.dll with
// pefile BEFORE a line was written; the addresses and ordinals above are that
// query's output (reference_resolveexport_phantom_names.md).
//
// GHIDRA WAS NOT AVAILABLE to this lane — the MCP bridge refused connection for
// its whole run. Every claim below is from `objdump -d` on the shipped DLL over
// the function's FULL extent, where "full extent" means the union of its
// `.pdata` fragments grouped by unwind chain, NOT the single `.pdata` row that
// contains the entry address. That distinction cost this lane a wrong number
// once already (WAVE21_LANE_GFXBODY.md §1.1) and is the reason no size in this
// file came from a single record.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/graphics/Material.h"
#include "../../types/video/Context.h"

using RtGraphics::MaterialBinding;
using RtGraphics::MaterialConstant;
using RtGraphics::MaterialInstance;
using RtGraphics::MaterialParameterSet;
using RtGraphics::MaterialTexture;
using RtVideo::Context;

// ----------------------------------------------------------------------------
// Declared here rather than pulled in from standalone/resolve.h, for the reason
// renderer.cpp:78 gives at length: resolve.h drags in <windows.h>, whose
// <wingdi.h> declares a FUNCTION called `Rectangle`. These must stay
// byte-identical to resolve.h's declarations; the linker checks them.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
void*     ResolveExport(const char* mangled_name);

namespace {

// ============================================================================
// THE OFFSETS THIS FILE ACTUALLY TOUCHES.
//
// Both layouts are already pinned by earlier waves, so this lane adds no new
// struct — it asserts against the two existing headers instead, which is the
// stronger check: if another lane moves a field, this file stops compiling.
// Every offset below was independently re-read out of Setup's disassembly by
// this lane, and all nine agreed with the header. That agreement is worth
// stating: Material.h's own comments say MaterialBinding, MaterialParameterSet
// and MaterialConstant were pinned BY THIS FUNCTION in Wave 6 — so Wave 6
// reversed Setup, wrote its layout down, and then held the body back on the
// ground that it "read layouts this lane did not pin".
// ============================================================================
static_assert(offsetof(MaterialInstance, material)   == 0x28, "Setup reads this+0x28");
static_assert(offsetof(MaterialInstance, parameters) == 0x30, "Setup reads this+0x30");
static_assert(offsetof(MaterialInstance, constants)  == 0x40, "Setup reads this+0x40");
static_assert(offsetof(MaterialInstance, textures)   == 0x50, "Setup reads this+0x50");
static_assert(sizeof(MaterialParameterSet) == 0x20, "shl $0x5 on the shader index");
static_assert(sizeof(MaterialBinding)      == 0x08, "add $0x8 per binding");
static_assert(sizeof(MaterialTexture)      == 0x10, "2*index then *8 == index*0x10");
static_assert(sizeof(MaterialConstant)     == 0x18, "lea (rax,rax,2) then *8 == index*0x18");
static_assert(offsetof(MaterialConstant, value) == 0x08, "float4 at +0x08 of the constant");

static_assert(offsetof(Context, _dirty)            == 0x0010, "Setup ORs ctx+0x10");
static_assert(offsetof(Context, _imageBufferDirty) == 0x0018, "Setup ORs ctx+0x18");
static_assert(offsetof(Context, _imageBuffers)     == 0x0030, "Setup writes ctx+0x30+slot*8");
static_assert(offsetof(Context, _pixelConstantStaging) == 0x0200,
              "Setup writes ctx+0x208+slot*0x10, i.e. staging[0x08 + slot*0x10]");

// The two dirty bits Setup raises, by VALUE not by the enum name.
//
// 0x2 is Context.h's `kDirtyImageBuffers` and that name is safe to use. 0x8000
// is NOT in Context.h's enum at all — the enum's `kDirtyMaterialConstants` is
// 0x1000, and Context.h:130-149 documents at length that that name is
// MISLEADING (0x1000 feeds the INSTANCE constant buffer; 0x8000 is the one
// BindResources uploads from +0x208, length 0x40). Using the literal here, with
// this note, is deliberate: borrowing a name whose own header says it means
// something else is how a wrong constant survives review.
constexpr uint32_t kDirtyImageBuffersBit    = 0x00000002u;
constexpr uint32_t kDirtyMaterialStagingBit = 0x00008000u;

// The +0x208 staging window is 0x40 bytes = four float4 slots. That is the
// length BindResources uploads (Context.h:139), and it is what bounds the slot
// index Setup writes. Not enforced at runtime — retail does not bound it either
// and this port stays faithful — but recorded so the next reader knows the
// shape.
constexpr size_t kMaterialStagingOffset = 0x208 - 0x200;   // into _pixelConstantStaging

template <typename T> inline T& Field(void* p, size_t off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(p) + off);
}

// ---------------------------------------------------------------------------
// ATTENDANCE, not a diff oracle. Three bodies, three counters.
//
// This exists because a registered port with no caller reports a clean run and
// reads as verification (reference_uncalled_port_is_not_verified.md), and
// because the A/B control for these three is `RT_GFXBODY=0`, under
// which the registrations do not happen at all -- so the flag-off arm MUST show
// zero here. If it did not, the "kill switch" would not be killing anything,
// which is the failure reference_the_gate_was_never_a_control.md describes.
//
// Printed at 1, 10, 1000 and every 100000 calls, so a log tail carries the
// evidence without flooding. Unconditional rather than env-gated: the whole
// point is that the flag-off arm can be compared against the flag-on arm, and
// a second env var would just be another thing to get wrong.
// ---------------------------------------------------------------------------
void Attend(const char* what, unsigned long long& n) {
    ++n;
    if (n == 1 || n == 10 || n == 1000 || (n % 100000) == 0)
        printf("[GFXBODY] %s calls=%llu\n", what, n);
}

unsigned long long g_setupCalls   = 0;
unsigned long long g_fcibNewCalls = 0;
unsigned long long g_matNewCalls  = 0;

// ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z, retail 0x1800c7a50.
//
// Resolved by ADDRESS through ResolveAddr, which consults the port registry
// first, exactly as runtime/src/video/context_ctor.cpp:105 does — this is the
// door that file family already uses and sharing it keeps one allocator arena.
// Never the raw DLL door: that is GetProcAddress, and two of them in a hot body cost
// Wave 2 five points of sim time (reference_rtoriginal_is_getprocaddress.md).
void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(bytes, align) : nullptr;
}

// ?SetupShaders@VideoProgram@Video@@QEBAXAEAVContext@2@@Z, retail 0x1800f4520.
//
// THIS CALL GROWS THE CUTOVER SURFACE BY ONE. `VideoProgram::SetupShaders` is
// exported and unported, and before this file it was not in the census `called`
// column at all — nothing in our tree asked for it. Porting a body that calls
// it adds it to the set that must work at cutover
// (reference_porting_can_grow_the_cutover_surface.md). Said out loud here
// because the census prints the new row without saying who added it.
void SetupShaders(void* program, Context* ctx) {
    using Fn = void (*)(void*, Context*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(
        ResolveExport("?SetupShaders@VideoProgram@Video@@QEBAXAEAVContext@2@@Z"));
    fn(program, ctx);
}

}  // namespace

// ============================================================================
// ??2FrameCopyImageBuffer@Graphics@@SAPEAX_K@Z   retail 0x18001efc0, ord 787
// ??2MaterialInstance@Graphics@@SAPEAX_K@Z       retail 0x180002820, ord 823
// ============================================================================
// Two class `operator new`s. Both bodies are TWENTY-FOUR INSTRUCTIONS and, once
// the rel32 and RIP-relative fields are discounted, they are the same body as
// each other and the same body as the two already ported at
// runtime/src/video/context_ctor.cpp:200 (`??2ImageBuffer@Video@@`,
// `??2RenderBuffer@Video@@`):
//
//     cs = *(void**)0x1801fc718 + 0x640;      // MemoryAllocator singleton lock
//     EnterCriticalSection(cs);               // IAT 0x18014b390
//     p = MemoryAllocator::Allocate(*(void**)0x1801fc718, bytes, 8);
//     LeaveCriticalSection(cs);               // IAT 0x18014b388
//     return p;
//
// ALIGNMENT IS THE WHOLE DIFFERENCE IN THIS FAMILY, AND IT WAS CHECKED BY BODY,
// NOT BY RESEMBLANCE — context_ctor.cpp:192 makes exactly that point about the
// sibling at 0x180011640, which is the same nineteen instructions with
// `MOV R8D,0x4`. Both bodies here disassemble to `MOV R8D,0x8`, at 0x18001eff4
// and 0x180002854 respectively. Read the two immediates; do not infer them.
//
// WHY ROUTING THROUGH MemAlloc IS THE FAITHFUL SPELLING AND NOT A SHORTCUT:
// `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` (0x1800c7a50) was
// disassembled for this lane and is instruction-for-instruction the same body
// with the alignment arriving in EDX instead of being baked into the immediate
// — same singleton, same +0x640 critical section, same
// `Allocate(singleton, bytes, align)`, same return path. Calling it with 8 is
// not an approximation of these two functions; it is them.
// ============================================================================
extern "C" void* RtGraphics_FrameCopyImageBuffer_OperatorNew(uint64_t bytes) {
    Attend("FrameCopyImageBuffer::operator new", g_fcibNewCalls);
    return MemAlloc(bytes, 8);                          // MOV R8D,0x8 @ 0x18001eff4
}

extern "C" void* RtGraphics_MaterialInstance_OperatorNew(uint64_t bytes) {
    Attend("MaterialInstance::operator new", g_matNewCalls);
    return MemAlloc(bytes, 8);                          // MOV R8D,0x8 @ 0x180002854
}

// ============================================================================
// ?Setup@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z
// retail 0x180102c90, ordinal 4028. 332 bytes, 83 instructions, ONE call.
// ============================================================================
// Binds one shader's worth of this material's textures and constants onto a
// Video::Context. `QEBA` — it is CONST, and the disassembly agrees: every write
// lands on the Context, never on `this`.
//
// Three phases:
//
//   1. `SetupShaders` on the VideoProgram this material's Material holds for
//      the requested shader index. The only call in the body, and the only
//      external dependency.
//   2. TEXTURES. Walk `parameters[shader].textures`; for each binding, look up
//      `textures[sourceIndex].image` and, IF IT DIFFERS from what the Context
//      already has in that slot, store it and raise two dirty bits.
//   3. CONSTANTS. Walk `parameters[shader].constants`; for each, compare the
//      float4 at `constants[sourceIndex].value` against the Context's material
//      staging slot and, IF ANY COMPONENT DIFFERS, copy all four and raise the
//      material-staging dirty bit.
//
// FOUR TRANSCRIPTION POINTS WORTH NAMING, because each is a place a plausible
// C++ rewrite silently diverges:
//
//  (a) BOTH LOOPS RE-READ THEIR BOUNDS AND THEIR DATA POINTER EVERY ITERATION.
//      Retail reloads `_data` at the top (`MOV RDX,[RDI]` at 0x180102cd0,
//      `MOV RCX,[RDI+0x10]` at 0x180102d20) and the count at the bottom
//      (`CMP [RDI+8],R9D`, `CMP [RDI+0x18],R10D`). Nothing in either body can
//      change them, so hoisting would be equivalent — it is written unhoisted
//      anyway, because "equivalent under an assumption I checked once" is how a
//      port drifts when the assumption later stops holding.
//
//  (b) THE SHIFT IS MASKED. `MOV EAX,1; SHL EAX,CL` — an x86 32-bit shift
//      masks the count to 5 bits, so slot 32 sets bit 0, not nothing. The
//      `& 31` below is that mask, not defensive programming; without it C++
//      would be UNDEFINED where retail is defined.
//
//  (c) THE FLOAT COMPARE IS UCOMISS + JP + JNE, i.e. UNORDERED COUNTS AS
//      DIFFERENT. Plain C++ `==` on float is false for NaN, which is the same
//      verdict, so `!(a == b)` is faithful here. Do not "improve" it to a
//      bitwise compare: -0.0 and +0.0 compare EQUAL under ucomiss and differ
//      bitwise, and retail takes the equal path.
//
//  (d) THE COPY IS INTEGER, NOT FLOAT. Retail moves the four values with
//      `MOV EAX,[..]` / `MOV [..],EAX`, so a signalling NaN or an unused
//      payload passes through bit-exact. std::memcpy reproduces that; a float
//      assignment would not be guaranteed to.
//
// ORACLE: NONE, AND THIS IS NOT REPLAYABLE. The body's entire effect is a
// mutation of a LIVE Video::Context, and it is deliberately NOT idempotent —
// the second run of a differential pair would find every value already stored
// and take the equal path, raising none of the dirty bits the first run raised.
// A "clean" diff would therefore be an artefact of replay, not agreement
// (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md is the
// same shape one step further on). Its verification tier is stated honestly in
// the lane doc and is not VERIFIED.
// ============================================================================
extern "C" void RtGraphics_MaterialInstance_Setup(const MaterialInstance* self,
                                                  Context* ctx,
                                                  int32_t shader) {
    Attend("MaterialInstance::Setup", g_setupCalls);

    // mat = this->material (ExternalPtr<Material>, by value);
    // prog = ((VideoProgram**)(mat + 0x28))[shader]
    void*  mat      = Field<void*>(const_cast<MaterialInstance*>(self), 0x28);
    void** programs = Field<void**>(mat, 0x28);
    SetupShaders(programs[shader], ctx);

    // set = parameters._data + shader*0x20   (SHL RDI,5 ; ADD RDI,[RSI+0x30])
    MaterialParameterSet* set = self->parameters._data + shader;

    // ---- phase 2: textures ------------------------------------------------
    for (int32_t i = 0; i < set->textures._size; ++i) {
        const MaterialBinding& b = set->textures._data[i];
        void* image = self->textures._data[b.sourceIndex].image;
        int32_t slot = b.shaderSlot;
        if (ctx->_imageBuffers[slot] != image) {
            ctx->_dirty            |= kDirtyImageBuffersBit;
            ctx->_imageBufferDirty |= (1u << (static_cast<uint32_t>(slot) & 31u));  // (b)
            ctx->_imageBuffers[slot] = image;
        }
    }

    // ---- phase 3: constants -----------------------------------------------
    for (int32_t i = 0; i < set->constants._size; ++i) {
        const MaterialBinding& b = set->constants._data[i];
        const float* src = self->constants._data[b.sourceIndex].value;
        float* dst = reinterpret_cast<float*>(
            &ctx->_pixelConstantStaging[kMaterialStagingOffset + b.shaderSlot * 0x10]);

        // (c) unordered counts as different, and all four are compared before
        // any is written — retail bails to the copy on the FIRST mismatch and
        // then copies all four, which is the same set of stores.
        if (dst[0] == src[0] && dst[1] == src[1] &&
            dst[2] == src[2] && dst[3] == src[3]) {
            continue;
        }
        std::memcpy(dst, src, 4 * sizeof(float));       // (d) integer moves
        ctx->_dirty |= kDirtyMaterialStagingBit;
    }
}

// ============================================================================
// Registration.
//
// NAMES ONLY, NO ADDRESSES, AND THAT IS A DECISION RATHER THAN AN OMISSION.
// Lane GFXBODY's own triage found a row in this same namespace
// (`?AddDrawItems@MeshInstance@`) where a RegisterRuntimeAddr would be ACTIVE
// HARM because standalone/scene_watch.cpp:400 uses the retail address as an
// IDENTITY TOKEN against a live vtable slot. That is a reason to check every
// address before registering it, not a reason to register none — so each of the
// three was checked:
//
//   0x18001efc0, 0x180002820  — zero occurrences anywhere in code/ engine/
//                               runtime/ standalone/ dx11/. No door, no token.
//   0x180102c90               — occurs only in COMMENTS (three of them, in
//                               runtime/src/ui/spritecontext_flush.cpp at :95,
//                               :138 and :260). Both live consumers,
//                               code/Game/Player/
//                               tool_base_tilerender.cpp:100 and
//                               runtime/src/ui/spritecontext_flush.cpp:267,
//                               resolve it BY NAME.
//
// So a name registration reaches every consumer that exists and an address
// registration would reach none that a name does not. Adding one anyway would
// be three lines of dead code that a future reader has to re-derive the safety
// of; see reference_count_the_doors_not_the_calls.md for why the count that
// matters is doors, not calls.
//
// Each mangled name is repeated as its own STRING LITERAL rather than via a
// named constant: tools/runtime_census.py matches `RegisterRuntimeExport("<lit>"`
// and cannot see through a constant, which is how Wave 3's Object count read
// 21/41 when it was 27/41
// (reference_census_cannot_see_nonliteral_registrations.md).
// ============================================================================
// standalone/matsetup_detour.cpp (Wave 24 lane GFXMAT). Forward-declared here
// rather than pulled from a header for the reason rt_port.h gives:
// runtime_detour.h drags in <windows.h>, which collides with the engine's own
// type names.
extern "C" bool RtGraphicsMatSetupInstallDetour(void* ourFn);

int RtGraphicsWave21GfxbodyRegister() {
    int count = 0;

    RegisterRuntimeExport("??2FrameCopyImageBuffer@Graphics@@SAPEAX_K@Z",
                          (void*)&RtGraphics_FrameCopyImageBuffer_OperatorNew); ++count;
    RegisterRuntimeExport("??2MaterialInstance@Graphics@@SAPEAX_K@Z",
                          (void*)&RtGraphics_MaterialInstance_OperatorNew); ++count;
    RegisterRuntimeExport("?Setup@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z",
                          (void*)&RtGraphics_MaterialInstance_Setup); ++count;

    // WAVE 24 LANE GFXMAT. The name registration above reaches OUR call sites
    // and, through runtime/src/ui/spritecontext_flush.cpp:267, ONE of retail's
    // two internal callers. It cannot reach the other — the rel32 CALL at
    // 0x180105572 inside Graphics::Renderer::DrawItemList, the every-frame 3D
    // material bind. Only a detour reaches that one, and a registration is not
    // a door for rel32.
    //
    // OPT-IN: it returns false unless RT_MATSETUP_DETOUR=1, so this
    // line changes nothing on a default run. The measured rel32 census (with
    // its positive control), the prologue bytes and the recursion argument are
    // all in standalone/matsetup_detour.cpp. `count` is NOT incremented —
    // nothing new was registered here.
    RtGraphicsMatSetupInstallDetour((void*)&RtGraphics_MaterialInstance_Setup);

    return count;   // 3
}
