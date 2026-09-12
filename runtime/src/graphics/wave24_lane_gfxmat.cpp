// ============================================================================
// runtime/src/graphics/wave24_lane_gfxmat.cpp — Wave 24 lane GFXMAT
//   the Graphics/Serializer cluster: four `??_7MaterialInstance@` slot bodies
//   and the two Graphics `?Serialize@` rows.
// ============================================================================
//
// Triage for every row this lane was dealt, including the ones NOT ported here
// and why, is docs/runtime_port/WAVE24_LANE_GFXMAT.md. Read §3 before adding a
// seventh body.
//
//   | Export                                          | Retail      | Ord  | rel32 |
//   |-------------------------------------------------|-------------|------|-------|
//   | ?Serialize@MaterialInstance@…Stream@IO@@@Z      | 0x180103a70 | 3706 |   0   |
//   | ?Reconstruct@MaterialInstance@Graphics@@UEAA…   | 0x180002a50 | 3391 |   0   |
//   | ?Destruct@MaterialInstance@Graphics@@UEAAXXZ    | 0x180002b50 | 2144 |   0   |
//   | ?Construct@MaterialInstance@Graphics@@UEAA…     | 0x180002ad0 | 1855 |   0   |
//   | ?Serialize@MeshInstance@Graphics@@QEAAX…       | 0x1800ff680 | 3707 |   0   |
//   | ?Serialize@AnimationManager@Graphics@@QEAAX…   | 0x1800fc190 | 3623 |   0   |
//
// Every address and ordinal above is `pefile`'s reading of the export directory
// of the shipped C:\GOG Games\Banished\Runtime-x64.dll, taken before a line of
// this file was written (reference_resolveexport_phantom_names.md). The `rel32`
// column is a scan of the whole 1,349,120-byte `.text` for `E8`/`E9` rel32
// whose target is the row — see WAVE24_LANE_GFXMAT.md §2, which carries the
// scanner's POSITIVE CONTROL (`?Serialize@ElementDescription@UI@@`, documented
// at 21 internal callers, scanned as exactly 21).
//
// *** ZERO rel32 callers on all six. *** The four slot bodies are reached only
// through `??_7MaterialInstance@Graphics@@6B@` (DLL 0x1801545e0), and the two
// `?Serialize@` rows only by name. So for THESE rows a name registration IS the
// door, and the `?Setup@` finding (standalone/matsetup_detour.cpp) does not
// generalise to them.
//
// EVERY BODY HERE WAS TRANSCRIBED FROM THE DISASSEMBLY, not from Ghidra's C.
// reference_ghidra_c_output_vs_disassembly.md: the decompiler drops
// dereferences and drops whole arguments at a mistyped vtable call, and every
// body in this file is a chain of mistyped vtable calls through `IO::Encoding`.
//
// ---------------------------------------------------------------------------
// THE `IO::Encoding` OFFSET TRAP, CONFIRMED RATHER THAN INHERITED
// ---------------------------------------------------------------------------
// reference_encoding_vtable_offsets_differ_between_builds.md says the retail
// and profile builds put the Encoding vtable's members at DIFFERENT offsets
// (+0x78/98/a0 vs +0x90/b0/b8). Every offset below was read out of
// **Runtime-x64.dll (retail, image base 0x180000000)**, the binary this build
// links against, and the three call sites agree with each other:
//
//     0x1800fc1ec   CALL qword ptr [RAX + 0xa0]     AnimationManager::Serialize
//     0x1800ff6b5   CALL qword ptr [RAX + 0xa0]     MeshInstance::Serialize
//     0x180103ae0   CALL qword ptr [RAX + 0xa0]     MaterialInstance::Serialize
//
// A port written from the profile decompile would have used +0xb8 here, would
// have compiled, linked and looked right, and would have desynced every save.
//
// ---------------------------------------------------------------------------
// THE TWO SHAPES THAT REPEAT — read once, then the bodies are short
// ---------------------------------------------------------------------------
// `IO::Stream`  +0x08  int32 mode   (0 == READING; anything else == WRITING)
//               +0x10  Encoding FACTORY*
// `IO::Serializer`  +0x00 Stream*   +0x08 Encoding*
//
// Both field sets are pinned by the bodies themselves and nothing else in this
// file depends on any other Stream or Serializer member, which is why no
// `IO::Stream` layout header is added: pinning fields this lane cannot verify
// would be a guess in a header whose whole contract is that it is not.
//
// `MaterialInstance::Serialize` and `AnimationManager::Serialize` MANUFACTURE
// their own Encoding from the factory and destroy it; `MeshInstance::Serialize`
// uses the Serializer's existing one at +0x08 and destroys nothing. That
// asymmetry is retail's, is deliberate here, and is the kind of thing a
// "tidied" port silently unifies.
//
// ---------------------------------------------------------------------------
// DECLARED: THIS FILE GROWS THE CUTOVER SURFACE BY TEN ADDRESS DOORS
// ---------------------------------------------------------------------------
// reference_porting_can_grow_the_cutover_surface.md. Each unexported internal
// below is reached with ResolveAddr(Module::Runtime, …) and DIES AT CUTOVER
// unless something registers that address
// (reference_by_address_door_dies_at_cutover.md). They are listed here, once,
// so the cutover lane can find them with one grep rather than ten:
//
//   0x180061100  ExternalPtr<Material>::Serialize      MaterialInstance +0x28
//   0x180061880  Array<MaterialTexture>::SetSize       stride 0x10
//   0x1800616c0  Array<MaterialConstant>::SetSize      stride 0x18
//   0x1800c9930  System::String::Serialize
//   0x18005d0f0  ExternalPtr<ImageBuffer>::Serialize
//   0x1800fe250  MeshInstance +0xd8 sub-serialize
//   0x18005cc60  Array<?>::SetSize                     MeshInstance +0xc8
//   0x18005d4a0  Array<?>::SerializeElements           MeshInstance +0xc8
//   0x18005c3d0  Array<AnimationState>::SetSize        stride 0x30
//   0x18005c660  Array<AnimationState>::SerializeElements
//
// `?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ` (0x1801033e0) is NOT in
// that list: it is an EXPORT, it is already in the census `called` column, and
// it is reached here BY NAME through RT_EXPORT_ONCE — so it needs no address
// and it grows nothing. It is unported, and a ported body calling an unported
// export is ordinary.
//
// The names beside the addresses are this lane's reading of what each helper
// does from its call site and its stride; they are NOT from a symbol table.
// reference_grep_cannot_tell_internal_from_unported_export.md — absent from the
// export directory by ADDRESS is not the same question as "unexported".
// Measured: all ten are absent from the export directory by address.
//
// ---------------------------------------------------------------------------
// ORACLES — what is real here and what is not
// ---------------------------------------------------------------------------
// PROBE SHAPE (kickoff §1c): **synthesised oracle**, two of them, plus one
// honest non-oracle. This lane has no game-directory allocation by design.
//
//   Construct / Reconstruct / Destruct — REAL DIFFERENTIAL ORACLES, IN-BODY.
//     Under BANISHED_RT_DIFF=1 each site builds TWO scratch 0x60 arenas in our
//     own memory, runs the SHIPPED export (RT_ORIGINAL_ONCE — the raw
//     GetProcAddress door, never ResolveExport, which would hand back ourselves
//     and compare ours to ours) over one and our body over the other, and
//     compares. These three bodies only store into `this` and tail-call vtable
//     slot 0, so a scratch arena with a vptr WE control is a complete and safe
//     universe for them. Slot 0 on both arenas points at the same recorder, so
//     the comparison is about the CALLER and nothing else — the
//     tools/gfxbody23_probe/ shape, run in-process.
//
//   The three ?Serialize@ bodies — NOT ORACLED HERE, AND SAID SO.
//     reference_shadow_marker_is_not_an_oracle.md: a RT_DIFF_SHADOW that takes
//     the port's own verdict and never consults the DLL passes on a shared
//     misunderstanding of retail. These three sites carry RT_DIFF_WORK
//     (attendance and an effectiveness count) and NOTHING THAT CLAIMS TO BE A
//     COMPARISON. A Serialize cannot be replayed in place — the first arm
//     consumes or emits the stream and the second sees a different one — so the
//     replay has to happen against a stream WE build, which is
//     tools/gfxmat24_probe/, not this file.
//     reference_write_path_serialize_is_replayable.md is the reason that probe
//     exists at all rather than "no oracle possible" being written here again.
//
// ---------------------------------------------------------------------------
// EVERY SITE IN THIS FILE CARRIES ITS CLASS IN ITS OWN NAME
// ---------------------------------------------------------------------------
// Wave 24 lane DIFF's armed sweep (99.7M calls over 630 sites) measured that
// **at least 286 of 630 sites — 45% — cannot catch a shared misunderstanding of
// retail**: 115 SELF-CHECK (no door to the shipped body anywhere in the
// function), 38 ATTENDANCE (the equality is the literal `true`), and 133
// NO-COMPARISON (structurally incapable of printing anything but
// `divergences=0`). A site that cannot fail is worse than no site, because it
// reports green. So the six `RT_DIFF_BEGIN` names here are suffixed:
//
//   [ORACLE]      resolves the SHIPPED body through RT_ORIGINAL_ONCE, runs it,
//                 and compares. Construct / Reconstruct / Destruct.
//   [ATTENDANCE]  RT_DIFF_WORK only. It counts calls and claims nothing else.
//                 The three ?Serialize@ rows; their comparison is
//                 tools/gfxmat24_probe/, which the sweep cannot run.
//
// The suffix is in the site NAME so the next sweep classifies these correctly
// from the report without reading the source, and so a future edit that guts an
// [ORACLE] into a self-check leaves a name that visibly lies.
//
// THE NEIGHBOURING SPECIMEN, carried here so it is not rediscovered: DIFF
// measured `Graphics::MaterialInstance::RenderTextures` at **worked=201,103,
// calls=0, divergences=0** — that body ran hard 201k times and its oracle
// covered none of it. So MaterialInstance objects ARE abundant and live in a
// normal run, which is evidence these rows will get attendance; and NO existing
// MaterialInstance green may be read as coverage.
//
// DO NOT COPY `IO::MemoryStream::MemoryStream` AS A CTOR-ORACLE TEMPLATE (DIFF):
// it neutralises the Encoding pointer and three never-written gaps and then
// compares the vptr at +0x00, which MUST differ, through one whole-buffer
// memcmp — permanently red, checking nothing. The Construct oracle below scores
// the vptr on its OWN screen precisely so it cannot become that.
// `RT_DIFF_REPLAY_PRE/POST` has ZERO adopters tree-wide and is not used here.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"
#include "../../types/graphics/Material.h"

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// — same reasoning rt_port.h gives (resolve.h drags in <windows.h>, which
// collides with the engine's own type/member names: <wingdi.h> declares a
// function called `Rectangle`). This declaration must stay byte-identical to
// resolve.h's, and animation.cpp in this same directory carries the same pair.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// ---------------------------------------------------------------------------
// NEGATIVE-CONTROL SEAM — COMPILED ONLY INTO tools/gfxmat24_probe.
//
// The shipping build never defines GFXMAT24_PROBE, so not one byte of the
// control machinery reaches reimpl-x64.exe. The probe defines it and drives
// `GfxmatProbeCorrupt()` from its own `--corrupt N` flag; at N == 0 every
// guarded site is `if (0 == <literal>)`, which the optimiser deletes, so the
// probe's measured body and the shipped body differ only in code that is
// provably dead on the measured run. Said out loud because Wave 21 lane
// GFXBODY closed with the opposite: "the binary measured in §7 is not the one
// described in §4".
// ---------------------------------------------------------------------------
#ifdef GFXMAT24_PROBE
extern "C" int GfxmatProbeCorrupt();
#  define RT_GFXMAT_CORRUPT(n) (GfxmatProbeCorrupt() == (n))
#else
#  define RT_GFXMAT_CORRUPT(n) (false)
#endif

namespace {

// ---------------------------------------------------------------------------
// Attendance. Printed by RtGraphicsWave24GfxmatSummary(); a run that entered
// none of these says so rather than staying silent
// (reference_uncalled_port_is_not_verified.md).
// ---------------------------------------------------------------------------
uint64_t g_constructCalls   = 0;
uint64_t g_reconstructCalls = 0;
uint64_t g_destructCalls    = 0;
uint64_t g_matSerCalls      = 0;
uint64_t g_meshSerCalls     = 0;
uint64_t g_animSerCalls     = 0;
uint64_t g_matSerTextures   = 0;   // elements actually walked, both directions
uint64_t g_matSerConstants  = 0;
uint64_t g_oracleRuns       = 0;   // the effectiveness counter (instrument rule 3)

// ---------------------------------------------------------------------------
// The three vtables Construct/Reconstruct stamp, all three EXPORTED BY NAME
// (checked against the export directory by address: 1389, 1432, 1464), so none
// of them is an address door. Resolved through RT_DATA_SLOT, i.e. through the
// PORT REGISTRY — if a later wave promotes `??_7MaterialInstance@`, our stamp
// follows the promotion exactly as every other promoted consumer does, and the
// in-body oracle below will then report a POINTER divergence at +0x00 which is
// EXPECTED, NOT A REGRESSION (the same note graphics_typeinfo_accessors.cpp
// carries for `?TypeName@`). That is why the oracle scores the vptr on its OWN
// screen and the 0x58 payload bytes on another
// (reference_union_control_hides_a_blind_screen.md).
// ---------------------------------------------------------------------------
void* BaseVTable() {
    return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@");
}
void* ExternalVTable() {
    return RT_DATA_SLOT(void*, "??_7External@Object@@6B@");
}
void* MaterialInstanceVTable() {
    return RT_DATA_SLOT(void*, "??_7MaterialInstance@Graphics@@6B@");
}

// Vtable slot 0 of Object::Base is the scalar-deleting destructor:
//   flag 0 = destroy in place, flag 1 = destroy and free.
using Slot0Fn = void* (*)(void* self, int flag);

// ---------------------------------------------------------------------------
// ?Construct@MaterialInstance@Graphics@@UEAAPEAVExternal@Object@@XZ
// retail 0x180002ad0, ordinal 1855, vtable slot 8.
//
// 0x180002ad0  SUB RSP,0x18 / MOV [RSP],-2            SEH frame only
//              MOV [RSP+0x20],RCX ; TEST RCX,RCX ; JZ -> return 0
//              LEA RAX,[0x180154788] ; MOV [RCX],RAX        ??_7Base@Object@
//              LEA RAX,[0x1801547a0] ; MOV [RCX],RAX        ??_7External@Object@
//              LEA RAX,[RCX+8] ; MOV [RSP+0x28],RAX         SEH sub-object only
//              XOR EDX,EDX
//              MOV [RCX+0x08],RDX / [RCX+0x10],RDX / [RCX+0x18],RDX
//              LEA RAX,[0x1801545e0] ; MOV [RCX],RAX        ??_7MaterialInstance@
//              MOV dword [RCX+0x20],EDX
//              MOV word  [RCX+0x24],DX                <- a WORD, two bytes
//              MOV [RCX+0x28] .. [RCX+0x58],RDX       <- seven qwords
//              MOV RAX,RCX
//
// The THREE vptr stores are retail's, in retail's order, and the first two are
// dead in the byte-result sense — they are the base constructors MSVC inlined.
// They are kept because a port that writes only the final one is a DIFFERENT
// FUNCTION under any interruption, and because the oracle below compares bytes
// and would not notice, which is precisely when a "harmless" tidy-up survives.
//
// `MOV word [RCX+0x24],DX` is TWO BYTES, not four. Material.h already names
// them separately (`hasRenderTexture` / `hasRenderedTexture`) and this body is
// the writer that proves the split: a dword store would have clobbered +0x26.
// ---------------------------------------------------------------------------
void ConstructBody(RtGraphics::MaterialInstance* self) {
    // Caller-visible effect only; the null test is retail's own.
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->external_08 = nullptr;
    self->external_10 = nullptr;
    self->external_18 = nullptr;
    *vp = MaterialInstanceVTable();
    self->shaderClass        = 0;
    self->hasRenderTexture   = 0;
    self->hasRenderedTexture = 0;
    self->material           = nullptr;
    self->parameters._data     = nullptr;
    self->parameters._size     = 0;
    self->parameters._capacity = 0;
    self->constants._data      = nullptr;
    self->constants._size      = 0;
    self->constants._capacity  = 0;
    self->textures._data       = nullptr;
    self->textures._size       = 0;
    self->textures._capacity   = 0;
}

// ---------------------------------------------------------------------------
// THE IN-BODY DIFFERENTIAL ORACLE for the three slot bodies.
//
// Two scratch arenas, both stamped with the SAME synthetic vptr whose slot 0 is
// `OracleSlot0` — so when Reconstruct or Destruct tail-calls slot 0 it reaches
// a recorder instead of a real destructor and neither arm can touch anything
// outside this function. Both arms then get compared on two SEPARATE screens:
//
//   (A) the (this-is-arena, flag) tuple recorded at slot 0, in order
//   (B) the 0x58 payload bytes +0x08..+0x5f
//
// and the vptr at +0x00 is scored separately, by RT_DIFF_SCALAR, so a promotion
// of `??_7MaterialInstance@` shows up as its own line naming identity movement
// rather than poisoning the byte screen.
// ---------------------------------------------------------------------------
struct OracleRecord {
    void* self;
    int   flag;
    int   count;
};
OracleRecord g_oracleRec[2];
int          g_oracleArm = 0;      // 0 = theirs, 1 = ours

void* OracleSlot0(void* self, int flag) {
    OracleRecord& r = g_oracleRec[g_oracleArm];
    r.self = self;
    r.flag = flag;
    r.count += 1;
    return self;
}

// A synthetic vtable big enough that nothing this file calls runs off the end.
// Only slot 0 is ever reached by these three bodies; the rest abort loudly if
// something unexpected dispatches, rather than reading whatever follows.
void* OracleAbort(void*, int) {
    std::fprintf(stderr, "[GFXMAT] oracle: unexpected vtable dispatch\n");
    std::fflush(stderr);
    return nullptr;
}
void* g_oracleVTable[16] = {
    (void*)&OracleSlot0,  (void*)&OracleAbort, (void*)&OracleAbort, (void*)&OracleAbort,
    (void*)&OracleAbort,  (void*)&OracleAbort, (void*)&OracleAbort, (void*)&OracleAbort,
    (void*)&OracleAbort,  (void*)&OracleAbort, (void*)&OracleAbort, (void*)&OracleAbort,
    (void*)&OracleAbort,  (void*)&OracleAbort, (void*)&OracleAbort, (void*)&OracleAbort,
};

// Fill an arena with a recognisable non-zero pattern first, so a body that
// FAILS to write a field is caught. An all-zero start would make "wrote zero"
// and "wrote nothing" indistinguishable, which is
// reference_oracle_must_not_compare_unwritten_bytes.md read the other way up.
void PrepareArena(uint8_t* arena) {
    for (size_t i = 0; i < 0x60; ++i) arena[i] = static_cast<uint8_t>(0xA5 + i);
    *reinterpret_cast<void**>(arena) = static_cast<void*>(g_oracleVTable);
}

}  // namespace

// ---------------------------------------------------------------------------
// The four `??_7MaterialInstance@Graphics@@6B@` slot bodies.
// ---------------------------------------------------------------------------

extern "C" void* RtGraphics_MaterialInstance_Construct(void* selfv) {
    RT_DIFF_BEGIN("Graphics::MaterialInstance::Construct[ORACLE]");
    ++g_constructCalls;

    if (rt_diff_on__) {
        using CtorFn = void* (*)(void*);
        CtorFn theirs = RT_ORIGINAL_ONCE(
            CtorFn, "?Construct@MaterialInstance@Graphics@@UEAAPEAVExternal@Object@@XZ");
        if (theirs) {
            alignas(16) uint8_t a[0x60], b[0x60];
            PrepareArena(a);
            PrepareArena(b);
            // The RETURN VALUE IS NOT COMPARED, deliberately. Our arm runs
            // ConstructBody (void) rather than the registered wrapper, because
            // the wrapper is what the DLL's registry now hands back and calling
            // it here would recurse until the stack dies. So a "does it return
            // self" check would have a CONSTANT on our side and would pass by
            // construction (reference_a_predicate_nobody_evaluates.md). The
            // wrapper's `return selfv` is instead pinned by the disassembly
            // (`MOV RAX,RCX` / the null arm's `XOR EDX,EDX ; MOV EAX,EDX`) and
            // said so here rather than faked into a screen.
            (void)theirs(a);
            ConstructBody(reinterpret_cast<RtGraphics::MaterialInstance*>(b));
            ++g_oracleRuns;
            RT_DIFF_BYTES(b + 8, a + 8, 0x58);                       // screen (B)
            RT_DIFF_SCALAR(*reinterpret_cast<void**>(b),             // identity screen
                           *reinterpret_cast<void**>(a));
        }
    }

    if (!selfv) return nullptr;                 // retail's own null test
    ConstructBody(reinterpret_cast<RtGraphics::MaterialInstance*>(selfv));
    RT_DIFF_WORK(true);
    return selfv;
}

// ?Destruct@MaterialInstance@Graphics@@UEAAXXZ, retail 0x180002b50, slot 7.
//
//   180002b50  MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]
//
// EIGHT BYTES, then CC padding — measured in the file image, and worth writing
// down because it means this row can NEVER be detoured: the smallest inline
// detour this tree installs is twelve. A registration is its only door, and
// (rel32 = 0) a registration reaches every caller there is.
extern "C" void RtGraphics_MaterialInstance_Destruct(void* selfv) {
    RT_DIFF_BEGIN("Graphics::MaterialInstance::Destruct[ORACLE]");
    ++g_destructCalls;

    if (rt_diff_on__) {
        using DtorFn = void (*)(void*);
        DtorFn theirs = RT_ORIGINAL_ONCE(DtorFn, "?Destruct@MaterialInstance@Graphics@@UEAAXXZ");
        if (theirs) {
            alignas(16) uint8_t a[0x60], b[0x60];
            PrepareArena(a);
            PrepareArena(b);
            g_oracleRec[0] = OracleRecord{};
            g_oracleRec[1] = OracleRecord{};
            g_oracleArm = 0; theirs(a);
            g_oracleArm = 1;
            {
                void** vp = reinterpret_cast<void**>(b);
                reinterpret_cast<Slot0Fn>(static_cast<void**>(*vp)[0])(b, 0);
            }
            ++g_oracleRuns;
            RT_DIFF_SCALAR(g_oracleRec[1].count, g_oracleRec[0].count);      // screen (A)
            RT_DIFF_SCALAR(g_oracleRec[1].flag,  g_oracleRec[0].flag);
            RT_DIFF_SCALAR(g_oracleRec[1].self == static_cast<void*>(b),
                           g_oracleRec[0].self == static_cast<void*>(a));
            RT_DIFF_BYTES(b + 8, a + 8, 0x58);                               // screen (B)
        }
    }

    void** vp = reinterpret_cast<void**>(selfv);
    reinterpret_cast<Slot0Fn>(static_cast<void**>(*vp)[0])(selfv, 0);
    RT_DIFF_WORK(true);
}

// ?Reconstruct@MaterialInstance@Graphics@@UEAAPEAVExternal@Object@@XZ,
// retail 0x180002a50, ordinal 3391, slot 6.
//
//   180002a50  ... MOV RBX,RCX ; MOV RAX,[RCX] ; XOR EDX,EDX ; CALL [RAX]
//              then byte-for-byte the Construct body above, then MOV RAX,RBX.
//
// No null test — unlike Construct. That asymmetry is retail's and is kept:
// reconstructing a null faults here exactly as it faults in the DLL
// (CLAUDE.md, "Don't fix a faithful crash with a null check").
extern "C" void* RtGraphics_MaterialInstance_Reconstruct(void* selfv) {
    RT_DIFF_BEGIN("Graphics::MaterialInstance::Reconstruct[ORACLE]");
    ++g_reconstructCalls;

    if (rt_diff_on__) {
        using RecFn = void* (*)(void*);
        RecFn theirs = RT_ORIGINAL_ONCE(
            RecFn, "?Reconstruct@MaterialInstance@Graphics@@UEAAPEAVExternal@Object@@XZ");
        if (theirs) {
            alignas(16) uint8_t a[0x60], b[0x60];
            PrepareArena(a);
            PrepareArena(b);
            g_oracleRec[0] = OracleRecord{};
            g_oracleRec[1] = OracleRecord{};
            g_oracleArm = 0; (void)theirs(a);      // return value: see Construct
            g_oracleArm = 1;
            {
                void** vp = reinterpret_cast<void**>(b);
                reinterpret_cast<Slot0Fn>(static_cast<void**>(*vp)[0])(b, 0);
                ConstructBody(reinterpret_cast<RtGraphics::MaterialInstance*>(b));
            }
            ++g_oracleRuns;
            RT_DIFF_SCALAR(g_oracleRec[1].count, g_oracleRec[0].count);      // screen (A)
            RT_DIFF_SCALAR(g_oracleRec[1].flag,  g_oracleRec[0].flag);
            RT_DIFF_BYTES(b + 8, a + 8, 0x58);                               // screen (B)
            RT_DIFF_SCALAR(*reinterpret_cast<void**>(b), *reinterpret_cast<void**>(a));
        }
    }

    void** vp = reinterpret_cast<void**>(selfv);
    reinterpret_cast<Slot0Fn>(static_cast<void**>(*vp)[0])(selfv, 0);
    ConstructBody(reinterpret_cast<RtGraphics::MaterialInstance*>(selfv));
    RT_DIFF_WORK(true);
    return selfv;
}

// ---------------------------------------------------------------------------
// ?Serialize@MaterialInstance@Graphics@@UEAA_NAEAVStream@IO@@@Z
// retail 0x180103a70, ordinal 3706, vtable slot 5.
//
// Takes a Stream& directly (not a Serializer&) and MAKES its own Encoding:
//
//   local.stream = stream
//   local.enc    = nullptr
//   factory      = stream[+0x10]
//   local.enc    = stream[+0x08] == 0 ? factory->vt[0x18](factory,&local.stream,0)
//                                     : factory->vt[0x20](factory,&local.stream,0)
//   0x180061100(&self->material, &local.stream)
//   n = self->textures._size          ; enc->vt[0xa0](enc,&n,0)
//   if (local.stream[+0x08] == 0) 0x180061880(&self->textures, n)
//   p = self->textures._data
//   repeat n:  0x1800c9930(p,   &local.stream)        String name
//              0x18005d0f0(p+8, &local.stream)        ExternalPtr<ImageBuffer>
//              p += 0x10
//   n = self->constants._size         ; enc->vt[0xa0](enc,&n,0)
//   if (local.stream[+0x08] == 0) 0x1800616c0(&self->constants, n)
//   p = self->constants._data
//   repeat n:  0x1800c9930(p, &local.stream)          String name
//              enc->vt[0x28](enc, 4, p+8, 0)          four floats
//              p += 0x18
//   BuildParameters(self)
//   if (local.enc) local.enc->vt[0](local.enc, 1)
//   return true
//
// TWO THINGS THE DISASSEMBLY SAYS AND THE DECOMPILER DOES NOT.
//
// 1. The loop bound `n` is re-read from the LOCAL each iteration? No — it is
//    latched into EDI once (`MOV EDI,EAX` then `DEC RDI`). It is transcribed
//    latched, because on the READ path the SetSize above has just made the
//    array exactly `n` long and a re-read would be a different function if a
//    helper ever resized it (reference_moved_term_hides_where_terms_cancel.md
//    is the same class of "algebraically identical" change).
//
// 2. `enc->vt[0x28]` is FOUR arguments — (enc, 4, p+8, 0) — from
//    `LEA R8,[RBX+8] ; XOR R9D,R9D ; LEA EDX,[R9+4]`. Ghidra's C prints this
//    call with arguments dropped, which is exactly the failure
//    reference_ghidra_c_output_vs_disassembly.md names.
//
// `p` walks `_data` with the RETAIL stride (0x10 textures, 0x18 constants),
// both of which Material.h already static_asserts, so a stride error is a
// compile error here rather than a heap walk.
//
// NOTE FOR runtime/types/graphics/Material.h: this body is the writer that
// names `MaterialConstant::_head` — it is a System::String, serialized by
// 0x1800c9930, the same helper that serializes `MaterialTexture::name`. The
// header's "not read by lane F" comment is not wrong, it is just superseded;
// the header is left alone by this lane because renaming a field that three
// other files include is not this lane's blast radius. See §6 R2 of the report.
// ---------------------------------------------------------------------------
namespace {

struct RtIoStream {           // ONLY the two fields these bodies read.
    uint8_t  _head_00[0x08];
    int32_t  mode;            // +0x08  0 == reading
    int32_t  _pad_0c;
    void*    encodingFactory; // +0x10
};
static_assert(offsetof(RtIoStream, mode) == 0x08, "IO::Stream mode @ +0x08");
static_assert(offsetof(RtIoStream, encodingFactory) == 0x10, "IO::Stream factory @ +0x10");

struct RtIoSerializer {       // ONLY the two fields these bodies read.
    RtIoStream* stream;       // +0x00
    void*       encoding;     // +0x08
};
static_assert(offsetof(RtIoSerializer, encoding) == 0x08, "IO::Serializer encoding @ +0x08");

using MakeEncodingFn = void* (*)(void* factory, RtIoStream** streamSlot, int zero);
using EncodeIntFn    = void  (*)(void* enc, void* value, int zero);
using EncodeFloatsFn = void  (*)(void* enc, int count, void* values, int zero);
using EncDtorFn      = void  (*)(void* enc, int flag);

// The Encoding vtable, RETAIL offsets, confirmed at three call sites (top of
// file). A profile-build port would use +0xb8 for kEncSerializeInt.
constexpr size_t kEncScalarDtor     = 0x00;
constexpr size_t kEncMakeRead       = 0x18;
constexpr size_t kEncMakeWrite      = 0x20;
constexpr size_t kEncSerializeFloats= 0x28;
constexpr size_t kEncSerializeInt   = 0xa0;

void** VTableOf(void* obj) { return *reinterpret_cast<void***>(obj); }
void*  VSlot(void* obj, size_t byteOffset) {
    return VTableOf(obj)[byteOffset / sizeof(void*)];
}

// Make the Encoding the way retail does: read-side factory slot when the
// stream's mode is 0, write-side otherwise.
void* MakeEncoding(RtIoStream* stream, RtIoStream** slot) {
    void* factory = stream->encodingFactory;
    const size_t which = (stream->mode == 0) ? kEncMakeRead : kEncMakeWrite;
    return reinterpret_cast<MakeEncodingFn>(VSlot(factory, which))(factory, slot, 0);
}

// The ten address doors, each resolved once per call site.
using SerExtPtrFn   = void (*)(void* field, RtIoStream** slot);
using ArraySetSizeFn= void (*)(void* arrayField, int count);
using SerStringFn   = void (*)(void* field, RtIoStream** slot);
using MeshSubSerFn  = void (*)(void* field, RtIoSerializer* ser);
using MeshSerElemFn = void (*)(RtIoSerializer* ser, int count, void* data);
using AnimSerElemFn = void (*)(RtIoStream** slot, int count, void* data);

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) {
    return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra));
}

}  // namespace

extern "C" bool RtGraphics_MaterialInstance_Serialize(void* selfv, void* streamv) {
    RT_DIFF_BEGIN("Graphics::MaterialInstance::Serialize[ATTENDANCE]");
    ++g_matSerCalls;

    RtGraphics::MaterialInstance* self = reinterpret_cast<RtGraphics::MaterialInstance*>(selfv);
    RtIoStream* localStream = reinterpret_cast<RtIoStream*>(streamv);
    void* enc = MakeEncoding(localStream, &localStream);

    // The ExternalPtr<Material> at +0x28.
    RtAddr<SerExtPtrFn>(0x180061100ull)(&self->material, &localStream);

    // Control 2 is THE documented retail-vs-profile Encoding offset trap turned
    // into a negative control: a port written from the profile decompile would
    // read the int encoder at +0xb8 and this is what that costs.
    const size_t encInt = RT_GFXMAT_CORRUPT(2) ? 0xb8 : kEncSerializeInt;

    // --- textures, stride 0x10 --------------------------------------------
    int32_t n = self->textures._size;
    reinterpret_cast<EncodeIntFn>(VSlot(enc, encInt))(enc, &n, 0);
    if (localStream->mode == 0)
        RtAddr<ArraySetSizeFn>(RT_GFXMAT_CORRUPT(1) ? 0x1800616c0ull : 0x180061880ull)(
            &self->textures, n);
    {
        uint8_t* p = reinterpret_cast<uint8_t*>(self->textures._data);
        const size_t stride = RT_GFXMAT_CORRUPT(3)
                                  ? sizeof(RtGraphics::MaterialConstant)
                                  : sizeof(RtGraphics::MaterialTexture);
        // Control 6 is the EFFECTIVENESS control and MUST NOT FIRE: retail
        // latches the bound in EDI, nothing under the loop can resize the
        // array, so a re-read is genuinely unobservable here.
        for (int32_t i = 0; i < (RT_GFXMAT_CORRUPT(6) ? self->textures._size : n); ++i) {
            RtAddr<SerStringFn>(0x1800c9930ull)(p, &localStream);
            RtAddr<SerExtPtrFn>(0x18005d0f0ull)(p + 8, &localStream);
            p += stride;                                     // static_asserted 0x10
            ++g_matSerTextures;
        }
    }

    // --- constants, stride 0x18 -------------------------------------------
    n = self->constants._size;
    reinterpret_cast<EncodeIntFn>(VSlot(enc, encInt))(enc, &n, 0);
    if (localStream->mode == 0)
        RtAddr<ArraySetSizeFn>(RT_GFXMAT_CORRUPT(1) ? 0x180061880ull : 0x1800616c0ull)(
            &self->constants, n);
    {
        uint8_t* p = reinterpret_cast<uint8_t*>(self->constants._data);
        for (int32_t i = n; i > 0; --i) {
            RtAddr<SerStringFn>(0x1800c9930ull)(p, &localStream);
            reinterpret_cast<EncodeFloatsFn>(VSlot(enc, kEncSerializeFloats))(
                enc, RT_GFXMAT_CORRUPT(5) ? 3 : 4, p + 8, 0);
            p += sizeof(RtGraphics::MaterialConstant);       // static_asserted 0x18
            ++g_matSerConstants;
        }
    }

    // BY NAME, through the port registry — BuildParameters is an EXPORT and is
    // already in the census `called` column, so this adds no address door and
    // no census row. It is unported today and this call reaches the DLL's copy.
    if (!RT_GFXMAT_CORRUPT(4)) {
        using BuildParamsFn = void (*)(void*);
        RT_EXPORT_ONCE(BuildParamsFn, "?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ")(self);
    }

    if (enc) reinterpret_cast<EncDtorFn>(VSlot(enc, kEncScalarDtor))(enc, 1);

    // Attendance and an effectiveness count. NOT a comparison — see the header
    // block: the replay for this body lives in tools/gfxmat24_probe/.
    RT_DIFF_WORK(true);
    return true;
}

// ---------------------------------------------------------------------------
// ?Serialize@MeshInstance@Graphics@@QEAAXAEAVSerializer@IO@@@Z
// retail 0x1800ff680, ordinal 3707. 114 bytes.
//
//   0x1800fe250(self+0xd8, ser)
//   n   = *(int32*)(self+0xd0)
//   enc = ser->encoding                       <- ser[+0x08], ALREADY MADE
//   enc->vt[0xa0](enc,&n,0)
//   if (ser->stream->mode == 0) 0x18005cc60(self+0xc8, n)
//   0x18005d4a0(ser, n, *(void**)(self+0xc8))
//
// The asymmetry called out at the top of the file: this body neither makes nor
// destroys an Encoding. It also reads the mode through `ser->stream->mode`, one
// deref deeper than MaterialInstance::Serialize's — the disassembly is
// `MOV RAX,[RDI] ; CMP dword [RAX+8],0`, and a decompile that drops one of
// those two derefs reads as `ser->mode`.
//
// `self+0xc8` / `self+0xd0` / `self+0xd8` are MeshInstance fields this lane does
// NOT name. runtime/types/graphics/ has no MeshInstance layout that reaches
// 0xd8 and inventing one from a single body would be a guess in a header whose
// contract is that it is not; the offsets are used as raw byte offsets and said
// so. reference_a_writer_beats_readers_agreeing_on_a_sign.md cuts the other way
// here: ONE body is not enough to name a field.
// ---------------------------------------------------------------------------
extern "C" void RtGraphics_MeshInstance_Serialize(void* selfv, void* serv) {
    RT_DIFF_BEGIN("Graphics::MeshInstance::Serialize[ATTENDANCE]");
    ++g_meshSerCalls;

    uint8_t* self = reinterpret_cast<uint8_t*>(selfv);
    RtIoSerializer* ser = reinterpret_cast<RtIoSerializer*>(serv);

    RtAddr<MeshSubSerFn>(0x1800fe250ull)(self + 0xd8, ser);

    int32_t n = *reinterpret_cast<int32_t*>(self + 0xd0);
    void* enc = ser->encoding;
    reinterpret_cast<EncodeIntFn>(VSlot(enc, kEncSerializeInt))(enc, &n, 0);

    if (ser->stream->mode == 0)
        RtAddr<ArraySetSizeFn>(0x18005cc60ull)(self + 0xc8, n);

    RtAddr<MeshSerElemFn>(0x18005d4a0ull)(ser, n, *reinterpret_cast<void**>(self + 0xc8));

    RT_DIFF_WORK(true);
}

// ---------------------------------------------------------------------------
// ?Serialize@AnimationManager@Graphics@@QEAAXAEAVSerializer@IO@@@Z
// retail 0x1800fc190, ordinal 3623. 165 bytes.
//
//   local.stream = ser->stream                <- ser[+0x00]
//   local.enc    = nullptr
//   factory      = local.stream[+0x10]
//   local.enc    = local.stream[+0x08] == 0 ? factory->vt[0x18](factory,&local.stream,0)
//                                           : factory->vt[0x20](factory,&local.stream,0)
//   n = *(int32*)(self+0x08)
//   local.enc->vt[0xa0](local.enc,&n,0)
//   if (local.stream[+0x08] == 0) 0x18005c3d0(self, n)     <- SetSize on self+0x00
//   0x18005c660(&local.stream, n, *(void**)self)
//   if (local.enc) local.enc->vt[0](local.enc, 1)
//
// AnimationManager's Array is at self+0x00 — the SetSize call passes `self`
// itself as the array, and the element walk reads `*(void**)self` as `_data`
// and `*(int32*)(self+8)` as `_size`, which is exactly Collection::Array's
// layout at offset zero. Element stride 0x30, read off the SetSize helper
// (`LEA RCX,[RAX+RAX*2] ; SHL RCX,4`).
//
// R4 OF WAVE23_LANE_GFXBODY, ANSWERED — and the answer is NO.
// That residual asked whoever ported this body to settle
// runtime/types/graphics/Animation.h `+0x10`, on the guess that this body
// probably touches it. It does not: the only AnimationManager fields this body
// reads are +0x00 (_data) and +0x08 (_size). `Animation.h +0x10` stays
// UNVERIFIED and the note there must NOT be deleted. Recorded so the next lane
// does not re-open it on the same guess
// (reference_holdback_blockers_are_stale_check_them_first.md, applied to a
// residual that turned out to be stale in the OTHER direction: the body named
// as the answer is not the answer).
// ---------------------------------------------------------------------------
extern "C" void RtGraphics_AnimationManager_Serialize(void* selfv, void* serv) {
    RT_DIFF_BEGIN("Graphics::AnimationManager::Serialize[ATTENDANCE]");
    ++g_animSerCalls;

    uint8_t* self = reinterpret_cast<uint8_t*>(selfv);
    RtIoSerializer* ser = reinterpret_cast<RtIoSerializer*>(serv);

    RtIoStream* localStream = ser->stream;
    void* enc = MakeEncoding(localStream, &localStream);

    int32_t n = *reinterpret_cast<int32_t*>(self + 8);
    reinterpret_cast<EncodeIntFn>(VSlot(enc, kEncSerializeInt))(enc, &n, 0);

    if (localStream->mode == 0)
        RtAddr<ArraySetSizeFn>(0x18005c3d0ull)(self, n);

    RtAddr<AnimSerElemFn>(0x18005c660ull)(&localStream, n, *reinterpret_cast<void**>(self));

    if (enc) reinterpret_cast<EncDtorFn>(VSlot(enc, kEncScalarDtor))(enc, 1);

    RT_DIFF_WORK(true);
}

// ============================================================================
// Registration.
//
// NAMES ONLY. Measured, not assumed: all six rows have ZERO rel32 callers in
// the DLL's `.text` (scanner and its positive control in
// WAVE24_LANE_GFXMAT.md §2), so there is no in-DLL traffic for an address
// registration to catch, and none of the six addresses occurs anywhere in
// code/ engine/ runtime/ standalone/ dx11/ — no identity token of the
// `?AddDrawItems@MeshInstance@` kind to break
// (reference_a_ported_row_can_be_permanently_unpromotable.md).
//
// Each mangled name is its own STRING LITERAL, never a named constant:
// tools/runtime_census.py matches `RegisterRuntimeExport("<literal>"` and
// cannot see through a constant
// (reference_census_cannot_see_nonliteral_registrations.md).
//
// The four MaterialInstance slot names are spelled EXACTLY as the export
// directory spells them — `UEAA` (virtual), not `QEAA`
// (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md). All four were confirmed
// present in the directory by `pefile` before this file was written.
// ============================================================================
int RtGraphicsWave24GfxmatRegister() {
    int count = 0;

    RegisterRuntimeExport("?Construct@MaterialInstance@Graphics@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtGraphics_MaterialInstance_Construct); ++count;
    RegisterRuntimeExport("?Reconstruct@MaterialInstance@Graphics@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtGraphics_MaterialInstance_Reconstruct); ++count;
    RegisterRuntimeExport("?Destruct@MaterialInstance@Graphics@@UEAAXXZ",
                          (void*)&RtGraphics_MaterialInstance_Destruct); ++count;
    RegisterRuntimeExport("?Serialize@MaterialInstance@Graphics@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtGraphics_MaterialInstance_Serialize); ++count;
    RegisterRuntimeExport("?Serialize@MeshInstance@Graphics@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtGraphics_MeshInstance_Serialize); ++count;
    RegisterRuntimeExport("?Serialize@AnimationManager@Graphics@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtGraphics_AnimationManager_Serialize); ++count;

    return count;   // 6
}

// A run that entered none of these says so rather than staying silent, and the
// oracle line is separate from the attendance line so a reader cannot mistake
// "the body ran" for "the body was compared"
// (reference_uncalled_port_is_not_verified.md).
void RtGraphicsWave24GfxmatSummary() {
    std::printf("[GFXMAT24] Construct=%llu Reconstruct=%llu Destruct=%llu "
                "MatSer=%llu (tex=%llu const=%llu) MeshSer=%llu AnimSer=%llu\n",
                (unsigned long long)g_constructCalls,
                (unsigned long long)g_reconstructCalls,
                (unsigned long long)g_destructCalls,
                (unsigned long long)g_matSerCalls,
                (unsigned long long)g_matSerTextures,
                (unsigned long long)g_matSerConstants,
                (unsigned long long)g_meshSerCalls,
                (unsigned long long)g_animSerCalls);
    std::printf("[GFXMAT24] differential-oracle runs (effectiveness counter) = %llu "
                "-- 0 means NOTHING WAS COMPARED, whatever the attendance line says\n",
                (unsigned long long)g_oracleRuns);
}
