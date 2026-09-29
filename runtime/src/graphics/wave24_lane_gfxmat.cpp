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
//     Under the diff oracle armed each site builds TWO scratch 0x60 arenas in our
//     own memory, runs the SHIPPED export (the raw DLL door — the raw
//     GetProcAddress door, never ResolveExport, which would hand back ourselves
//     and compare ours to ours) over one and our body over the other, and
//     compares. These three bodies only store into `this` and tail-call vtable
//     slot 0, so a scratch arena with a vptr WE control is a complete and safe
//     universe for them. Slot 0 on both arenas points at the same recorder, so
//     the comparison is about the CALLER and nothing else — the
//     tools/gfxbody23_probe/ shape, run in-process.
//
//   The three ?Serialize@ bodies — NOT ORACLED HERE, AND SAID SO.
//     reference_shadow_marker_is_not_an_oracle.md: a diff-oracle site that takes
//     the port's own verdict and never consults the DLL passes on a shared
//     misunderstanding of retail. These three sites carry the diff oracle
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
// reports green. So the six `the diff oracle` names here are suffixed:
//
//   [ORACLE]      resolves the SHIPPED body through the raw DLL door, runs it,
//                 and compares. Construct / Reconstruct / Destruct.
//   [ATTENDANCE]  the diff oracle only. It counts calls and claims nothing else.
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
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

#include "../../types/graphics/Material.h"
#include "../../types/graphics/Mesh.h"              // ENDGAME B.3.5 block G
#include "../../types/graphics/AnimationManager.h"  // ENDGAME B.3.5 block G
#include "../../types/io/Serializer.h"             // IO::ReaderWriter (B3X W0)
#include "../../types/io/Stream.h"                 // B.4: IO::Stream::_direction
#include "../../types/graphics/AnimationState.h"   // Graphics::AnimationState::Serialize, the anim-array bodies
#include "../../types/collection/Collection.h"     // rtcoll:: the array instantiations
#include "../../types/object/External.h"           // the ExternalPtr serializers
#include "../../types/system/String.h"

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
// ENDGAME B.3.9 block GFX: `BaseVTable` / `ExternalVTable` /
// `MaterialInstanceVTable` (the minted tables, by name) and `Slot0Fn` are gone:
// the class is built by its real ctor and destroyed through `DeletingDtor`.

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
// ENDGAME B.3.9 block GFX: `ConstructBody` (these stores with the three MINTED
// vptr stamps) is gone. The listing above IS the real `MaterialInstance()`
// inlined: Object::Base's and Object::External's stamps and External's three
// zero qwords are the compiler's base chain, the MaterialInstance stamp is the
// compiler's, and the field stores are the ctor's body (Material.h).

// (B3X W0: the in-body differential oracle for the three slot bodies -- two
// arenas stamped with a 16-slot synthetic vptr whose slot 0 recorded the
// (self, flag) tuple -- is DELETED. Its only entry, `PrepareArena`, had no
// caller anywhere once ENDGAME B.3.9 GFX retired the slot doors it compared,
// and a table built by index would have mis-aimed after the normal-virtual
// cutover. The replay for these bodies lives in tools/gfxmat24_probe/.)

}  // namespace

// ---------------------------------------------------------------------------
// The four `??_7MaterialInstance@Graphics@@6B@` slot bodies.
// ---------------------------------------------------------------------------

// ENDGAME B.3.9 block GFX: the four slot doors (Construct, Destruct,
// Reconstruct, Serialize) are RETIRED -- their only reader was the minted
// `??_7MaterialInstance@Graphics@@6B@`. The listings stay here as the record;
// the entry counters moved into the members.

// ?Destruct@MaterialInstance@Graphics@@UEAAXXZ, retail 0x180002b50, slot 7.
//
//   180002b50  MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]
//
// EIGHT BYTES, then CC padding — measured in the file image, and worth writing
// down because it means this row can NEVER be detoured: the smallest inline
// detour this tree installs is twelve. A registration is its only door, and
// (rel32 = 0) a registration reaches every caller there is.

// ?Reconstruct@MaterialInstance@Graphics@@UEAAPEAVExternal@Object@@XZ,
// retail 0x180002a50, ordinal 3391, slot 6.
//
//   180002a50  ... MOV RBX,RCX ; MOV RAX,[RCX] ; XOR EDX,EDX ; CALL [RAX]
//              then byte-for-byte the Construct body above, then MOV RAX,RBX.
//
// No null test — unlike Construct. That asymmetry is retail's and is kept:
// reconstructing a null faults here exactly as it faults in the DLL
// (CLAUDE.md, "Don't fix a faithful crash with a null check").
// NO null test, unlike Construct one slot over. That asymmetry is retail's and
// is kept: reconstructing a null faults here exactly as it faults in the DLL.

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

using MakeEncodingFn = void* (*)(void* factory, RtIoSerializer* ser, int zero);

// The Encoding factory slots, RETAIL offsets (IO::Encoding is exempt from the
// normal-virtual cutover: its slot 0 is not a destructor, B3X_RULES X1).
constexpr size_t kEncMakeRead       = 0x18;
constexpr size_t kEncMakeWrite      = 0x20;

// What the factory returns is the IO::ReaderWriter (the lane called it the
// "encoding"). Its three slots these bodies use -- [0] DeletingDtor, +0x28
// SerializeFloatArray, +0xa0 SerializeInt; a profile-build port would use +0xb8
// for the int -- are by-name calls on the real class since B3X W0.
IO::ReaderWriter* AsRw(void* enc) {
    return static_cast<IO::ReaderWriter*>(enc);   // B.3.x bridge
}

void** VTableOf(void* obj) { return *reinterpret_cast<void***>(obj); }
void*  VSlot(void* obj, size_t byteOffset) {
    return VTableOf(obj)[byteOffset / sizeof(void*)];
}

// Make the Encoding the way retail does: read-side factory slot when the
// stream's mode is 0, write-side otherwise.
// Retail keeps the IO::Serializer PAIR on its own stack -- [RSP+0x28] = stream,
// [RSP+0x30] = the reader/writer the factory returns (0x1800fc1a5..0x1800fc1d0)
// -- and hands `LEA [RSP+0x28]` to every callee. The first draft of this file
// passed the address of a lone `RtIoStream*` local instead, so every callee
// read its reader from whatever stack slot happened to sit next to it. With
// the DLL present that slot happened to survive; the first cutover boot to
// reach ExternalPtr<Material>::Serialize through it died on EXEC-of-8.
void* MakeEncoding(RtIoSerializer* ser) {
    RtIoStream* stream = ser->stream;
    void* factory = stream->encodingFactory;
    const size_t which = (stream->mode == 0) ? kEncMakeRead : kEncMakeWrite;
    void* enc = reinterpret_cast<MakeEncodingFn>(VSlot(factory, which))(factory, ser, 0);
    ser->encoding = enc;                                   // MOV [RSP+0x30],RAX
    return enc;
}

// The ten callees, by name (B3R). B.3.x bridges: `RtIoSerializer` is this
// lane's view of an rt::io::Serializer; the fields are ExternalPtr / String.
using ArraySetSizeFn = void (*)(void* arrayField, int count);   // the probe's control 1
inline rt::io::Serializer* AsSer(RtIoSerializer* ser) {
    return reinterpret_cast<rt::io::Serializer*>(ser);
}
inline void SerExtPtrMaterial(void* field, RtIoSerializer* ser) {      // 0x180061100
    RtObject_ExternalPtrSerialize_Material(reinterpret_cast<RtObject::External**>(field), AsSer(ser));
}
inline void SerExtPtrImageBuffer(void* field, RtIoSerializer* ser) {   // 0x18005d0f0
    RtObject_ExternalPtrSerialize_ImageBuffer(reinterpret_cast<RtObject::External**>(field), AsSer(ser));
}
inline void SerString(void* field, RtIoSerializer* ser) {              // 0x1800c9930
    static_cast<System::String*>(field)->Serialize(ser);
}

}  // namespace

// ENDGAME B.3.5 block G: BODY MOVED to `Graphics::MaterialInstance::Serialize`,
// below. Every `self->x` became `x`, the six `RT_GFXMAT_CORRUPT` negative
// controls came with it unchanged (the macro is at file scope), and the two
// stride literals are still `sizeof()` of the header's element types so a stride
// error stays a compile error rather than a heap walk.
namespace Graphics {

// ---------------------------------------------------------------------------
// The three lifecycle slots, 8 / 7 / 6. ENDGAME B.3.9 GFX: placement new of the
// class and `~T()` (slot 0, flags 0); the doors are retired.
// ---------------------------------------------------------------------------

// [8] ?Construct@MaterialInstance@Graphics@@UEAAPEAVExternal@Object@@XZ 0x180002ad0
//     MSVC's placement-new codegen (`TEST RCX,RCX ; JZ -> return 0`, then the
//     inlined ctor); a virtual is never entered with null.
Object::External* MaterialInstance::Construct() {
    ++g_constructCalls;
    return ::new (this) MaterialInstance();
}

// [7] ?Destruct@MaterialInstance@Graphics@@UEAAXXZ 0x180002b50
//     `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]` -- EIGHT BYTES, then CC padding,
//     which means this row can NEVER be detoured (the smallest inline detour this
//     tree installs is twelve). A registration is its only door. Slot 0 is
//     dispatched VIRTUALLY on purpose: a subclass's deleting destructor is what
//     has to run.
void MaterialInstance::Destruct() {
    ++g_destructCalls;
    this->~MaterialInstance();
}

// [6] ?Reconstruct@MaterialInstance@Graphics@@UEAAPEAVExternal@Object@@XZ 0x180002a50
//     Slot 0 with flag 0 (destroy IN PLACE, not free), then byte-for-byte the
//     Construct body. NO null test, unlike Construct one slot over; retail's
//     asymmetry, kept.
Object::External* MaterialInstance::Reconstruct() {
    ++g_reconstructCalls;
    this->~MaterialInstance();
    return ::new (this) MaterialInstance();
}

// ?CreateNewObject@MaterialInstance@Graphics@@SAPEAVBase@Object@@XZ 0x180002900
// -- `??2MaterialInstance` (MemAlloc 0x60, align 8), the null branch, the
// inlined ctor (the FACTREST row's transcription). Replaces that row (D2).
Object::Base* MaterialInstance::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MaterialInstance), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) MaterialInstance();
}
// ?CreateInPlaceObject@MaterialInstance@Graphics@@SAPEAVBase@Object@@PEAX@Z 0x1800029d0
Object::Base* MaterialInstance::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) MaterialInstance();
}

bool MaterialInstance::Serialize(void* streamv) {
    ++g_matSerCalls;

    RtIoStream* localStream = reinterpret_cast<RtIoStream*>(streamv);
    RtIoSerializer ser = { localStream, nullptr };          // [RSP+0x28], [RSP+0x30]
    void* enc = MakeEncoding(&ser);

    // The ExternalPtr<Material> at +0x28.
    SerExtPtrMaterial(&_material, &ser);

    // Control 2 is THE documented retail-vs-profile Encoding offset trap turned
    // into a negative control: a port written from the profile decompile would
    // read the int encoder at +0xb8 and this is what that costs.
    // The control's +0xb8 is `SerializeUChar`, handed the same `&n` the raw
    // call passed.
    auto serializeCount = [&enc](int32_t& v) {
        if (RT_GFXMAT_CORRUPT(2))
            AsRw(enc)->SerializeUChar(*reinterpret_cast<unsigned char*>(&v), nullptr);
        else
            AsRw(enc)->SerializeInt(v, nullptr);
    };

    // --- textures, stride 0x10 --------------------------------------------
    int32_t n = _textures._size;
    serializeCount(n);
    if (localStream->mode == 0)
        (RT_GFXMAT_CORRUPT(1) ? ArraySetSizeFn(&rtcoll::RtColl_ArraySetSize_1800616c0)
                              : ArraySetSizeFn(&rtcoll::RtColl_ArraySetSize_180061880))(
            &_textures, n);
    {
        uint8_t* p = reinterpret_cast<uint8_t*>(_textures._data);
        const size_t stride = RT_GFXMAT_CORRUPT(3)
                                  ? sizeof(RtGraphics::MaterialConstant)
                                  : sizeof(RtGraphics::MaterialTexture);
        // Control 6 is the EFFECTIVENESS control and MUST NOT FIRE: retail
        // latches the bound in EDI, nothing under the loop can resize the
        // array, so a re-read is genuinely unobservable here.
        for (int32_t i = 0; i < (RT_GFXMAT_CORRUPT(6) ? _textures._size : n); ++i) {
            SerString(p, &ser);
            SerExtPtrImageBuffer(p + 8, &ser);
            p += stride;                                     // static_asserted 0x10
            ++g_matSerTextures;
        }
    }

    // --- constants, stride 0x18 -------------------------------------------
    n = _constants._size;
    serializeCount(n);
    if (localStream->mode == 0)
        (RT_GFXMAT_CORRUPT(1) ? ArraySetSizeFn(&rtcoll::RtColl_ArraySetSize_180061880)
                              : ArraySetSizeFn(&rtcoll::RtColl_ArraySetSize_1800616c0))(
            &_constants, n);
    {
        uint8_t* p = reinterpret_cast<uint8_t*>(_constants._data);
        for (int32_t i = n; i > 0; --i) {
            SerString(p, &ser);
            AsRw(enc)->SerializeFloatArray(
                RT_GFXMAT_CORRUPT(5) ? 3 : 4, reinterpret_cast<float*>(p + 8), nullptr);
            p += sizeof(RtGraphics::MaterialConstant);       // static_asserted 0x18
            ++g_matSerConstants;
        }
    }

    // BY NAME, through the port registry — BuildParameters is an EXPORT and is
    // already in the census `called` column, so this adds no address door and
    // no census row.
    //
    // CORRECTED 2026-09-12: this comment used to end "It is unported today and
    // this call reaches the DLL's copy." Both halves are now false.
    // `?BuildParameters@MaterialInstance@Graphics@@` (retail 0x1801033e0) is
    // ported and registered, and the 2026-09-12 cutover boot ran it with the
    // DLL ABSENT — `[GFXBIG27] MaterialInstance::BuildParameters calls=1`,
    // followed by `[GFXBODY28] Material::GetTextureName calls=1` and
    // `MaterialInstance::FindTexture calls=1`, then a completed
    // `[SpriteSheetLoad] Dialog/SpriteSheet.rsc`. RT_EXPORT_ONCE resolves
    // registry-first, so this call site needed no change to follow the port.
    if (!RT_GFXMAT_CORRUPT(4)) {
        // ENDGAME B.3.5 caller batch: `MaterialInstance::BuildParameters` is a
        // DEFINED member (wave27_lane_gfxbig.cpp:773), so this is the qualified
        // non-virtual member call retail makes, and `BuildParamsFn` is gone.
        this->MaterialInstance::BuildParameters();
    }

    if (enc) delete AsRw(enc);

    // Attendance and an effectiveness count. NOT a comparison — see the header
    // block: the replay for this body lives in tools/gfxmat24_probe/.
    return true;
}

}  // namespace Graphics

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
// `self+0xc8` / `self+0xd0` / `self+0xd8` WERE "MeshInstance fields this lane
// does not name", and the comment that stood here said naming them from one body
// would be a guess. THEY ARE NAMED NOW, by somebody else and from other bodies:
// `Mesh.h` (Wave 6 lane E, then ENDGAME B.3.5 block E's real class) pins
// `_materials` at +0xc8 with its `_size` at +0xd0 and `_animation` at +0xd8, all
// from `??0MeshInstance@` and `IsAnimating`. So this body is retyped onto
// `Graphics::MeshInstance` and the three offsets become three field names.
//
// ENDGAME B.3.5 block G: BODY MOVED to `Graphics::MeshInstance::Serialize` —
// block E's class, declared in `Mesh.h` and left declared-undefined there. NO
// HEADER EDIT WAS NEEDED; `Mesh.h`'s "exported, NOT PORTED" comment beside the
// declaration is STALE and belongs to the closing pass, not to a concurrent
// block's diff.
// ---------------------------------------------------------------------------
namespace Graphics {

void MeshInstance::Serialize(IO::Serializer& serializer) {
    ++g_meshSerCalls;

    // The lane's two-field views of the serializer pair; the same bytes the real
    // `IO::Serializer` describes.
    RtIoSerializer* ser = reinterpret_cast<RtIoSerializer*>(&serializer);  // B.3.5 bridge

    reinterpret_cast<Graphics::AnimationState*>(&_animation)      // +0xd8, 0x1800fe250
        ->Graphics::AnimationState::Serialize(serializer);        // B.3.x bridge

    int32_t n = _materials._size;                                 // +0xd0
    void* enc = ser->encoding;
    AsRw(enc)->SerializeInt(n, nullptr);

    // `ser->stream->mode`, one deref deeper than MaterialInstance::Serialize's —
    // the disassembly is `MOV RAX,[RDI] ; CMP dword [RAX+8],0`, and a decompile
    // that drops one of those two derefs reads as `ser->mode`.
    if (ser->stream->mode == 0)
        rtcoll::RtColl_ArraySetSize_18005cc60(&_materials, n);     // +0xc8

    RtObject_ExternalPtrArraySerialize_MaterialInstance(          // 0x18005d4a0
        AsSer(ser), n, reinterpret_cast<RtObject::External**>(_materials._data));
}

}  // namespace Graphics

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
// ENDGAME B.3.5 block G: BODY MOVED to `Graphics::AnimationManager::Serialize` —
// block H1's class, declared in `AnimationManager.h:297` and left
// declared-undefined there because this file holds the live body. No header edit
// was needed. `_instances` is the `Array` at +0x00, which is why the SetSize call
// passes the object itself: `&_instances == this`.
namespace Graphics {

void AnimationManager::Serialize(IO::Serializer& s) {
    ++g_animSerCalls;

    // B.4: the local `{ stream, enc }` pair, the factory call and the closing
    // `if (enc) delete enc` are `IO::Serializer`'s ctor and dtor, inlined.
    IO::Serializer localSer(*s._stream, 0);                 // [RSP+0x28], [RSP+0x30]

    int32_t n = _instances._size;                           // +0x08
    localSer._readerWriter->SerializeInt(n, nullptr);

    if (localSer._stream->_direction == IO::kReading)
        rtcoll::Rt_SetSize_18005c3d0(&_instances, n);            // +0x00 == this

    RtGraphics_SerializeAnimInstances(localSer, n, _instances._data);   // 0x18005c660
}

}  // namespace Graphics

// A run that entered none of these says so rather than staying silent, and the
// oracle line is separate from the attendance line so a reader cannot mistake
// "the body ran" for "the body was compared"
// (reference_uncalled_port_is_not_verified.md).
void RtGraphicsWave24GfxmatSummary() {
    printf("[GFXMAT24] Construct=%llu Reconstruct=%llu Destruct=%llu "
                "MatSer=%llu (tex=%llu const=%llu) MeshSer=%llu AnimSer=%llu\n",
                (unsigned long long)g_constructCalls,
                (unsigned long long)g_reconstructCalls,
                (unsigned long long)g_destructCalls,
                (unsigned long long)g_matSerCalls,
                (unsigned long long)g_matSerTextures,
                (unsigned long long)g_matSerConstants,
                (unsigned long long)g_meshSerCalls,
                (unsigned long long)g_animSerCalls);
    printf("[GFXMAT24] differential-oracle runs (effectiveness counter) = %llu "
                "-- 0 means NOTHING WAS COMPARED, whatever the attendance line says\n",
                (unsigned long long)g_oracleRuns);
}
