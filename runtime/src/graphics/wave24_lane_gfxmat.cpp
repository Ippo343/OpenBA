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

// B.4: the `{ stream, enc }` pair, the factory call and the closing
// `if (enc) enc->vt[0](enc, 1)` are `IO::Serializer`'s ctor and dtor (retail
// keeps the pair on its own stack at [RSP+0x28] / [RSP+0x30] and hands
// `LEA [RSP+0x28]` to every callee -- the Serializer object is that pair).
// The lane's RtIoStream / RtIoSerializer views, the factory-slot constants and
// the VSlot reader are gone with the hand-built pair; so are the six
// `RT_GFXMAT_CORRUPT` negative controls, which compiled only under
// GFXMAT24_PROBE -- a define nothing in the tree sets, for a
// tools/gfxmat24_probe that does not exist.
//
// LEFT: the ExternalPtr serializers take object's POD spellings (External.h),
// so the Serializer is viewed once per call as `rt::io::Serializer`.
inline rt::io::Serializer* PodSerializer(IO::Serializer& ser) {
    return reinterpret_cast<rt::io::Serializer*>(&ser);
}

// 0x1800c9930, `System::String::Serialize`: each element's leading String.
// LEFT: `MaterialTexture::name` / `MaterialConstant::_head` stay raw (a
// `System::String` member would make the element non-trivial and change its
// array's lifecycle rows), so the String is viewed at the field.
inline void SerString(void* field, IO::Serializer& ser) {
    static_cast<System::String*>(field)->Serialize(&ser);
}

}  // namespace

// ENDGAME B.3.5 block G: BODY MOVED to `Graphics::MaterialInstance::Serialize`,
// below. The two strides are the element types' (Material.h static_asserts
// 0x10 and 0x18).
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

bool MaterialInstance::Serialize(IO::Stream& streamv) {
    ++g_matSerCalls;

    IO::Stream& stream = streamv;
    IO::Serializer ser(stream, 0);                          // [RSP+0x28], [RSP+0x30]
    IO::ReaderWriter* const rw = ser._readerWriter;

    // The ExternalPtr<Material> at +0x28 (0x180061100).
    RtObject_ExternalPtrSerialize_Material(
        reinterpret_cast<RtObject::External**>(&_material), PodSerializer(ser));  // LEFT: POD ExternalPtr serialize thunk takes RtObject::External** (External.h, R-OBJ / wave X)

    // --- textures, stride 0x10 --------------------------------------------
    int32_t n = _textures._size;
    rw->SerializeInt(n, nullptr);                           // +0xa0
    if (stream._direction == IO::kReading)
        rtcoll::RtColl_ArraySetSize_180061880(&_textures, n);
    {
        // The bound is latched (EDI), as retail does.
        RtGraphics::MaterialTexture* p = _textures._data;
        for (int32_t i = 0; i < n; ++i) {
            SerString(&p->name, ser);                       // 0x1800c9930
            RtObject_ExternalPtrSerialize_ImageBuffer(      // 0x18005d0f0
                reinterpret_cast<RtObject::External**>(&p->image), PodSerializer(ser));  // LEFT: POD ExternalPtr serialize thunk takes RtObject::External** (External.h, R-OBJ / wave X)
            ++p;
            ++g_matSerTextures;
        }
    }

    // --- constants, stride 0x18 -------------------------------------------
    n = _constants._size;
    rw->SerializeInt(n, nullptr);
    if (stream._direction == IO::kReading)
        rtcoll::RtColl_ArraySetSize_1800616c0(&_constants, n);
    {
        RtGraphics::MaterialConstant* p = _constants._data;
        for (int32_t i = n; i > 0; --i) {
            SerString(&p->name, ser);
            rw->SerializeFloatArray(4, p->value, nullptr);  // +0x28, (enc, 4, p+8, 0)
            ++p;
            ++g_matSerConstants;
        }
    }

    // ?BuildParameters@MaterialInstance@Graphics@@ (retail 0x1801033e0), a
    // DEFINED member (wave27_lane_gfxbig.cpp): the qualified non-virtual call
    // retail makes, before the Encoding is released.
    this->MaterialInstance::BuildParameters();

    return true;                                            // ~Serializer: `if (enc) delete enc`
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
// deref deeper than MaterialInstance::Serialize's.
//
// ENDGAME B.3.5 block G: BODY MOVED to `Graphics::MeshInstance::Serialize`.
// B.4: the serializer's `_stream` / `_readerWriter` by name.
// ---------------------------------------------------------------------------
namespace Graphics {

void MeshInstance::Serialize(IO::Serializer& serializer) {
    ++g_meshSerCalls;

    // LEFT: `_animation` is the 0x18-byte retail AnimationState (Mesh.h's
    // MeshAnimationState); the tree's 0x48 class is its prefix-compatible
    // superset, so the member is reached through a view of the same bytes.
    reinterpret_cast<Graphics::AnimationState*>(&_animation)      // +0xd8, 0x1800fe250
        ->Graphics::AnimationState::Serialize(serializer);

    int32_t n = _materials._size;                                 // +0xd0
    serializer._readerWriter->SerializeInt(n, nullptr);

    // `ser->stream->mode`, one deref deeper than MaterialInstance::Serialize's —
    // the disassembly is `MOV RAX,[RDI] ; CMP dword [RAX+8],0`.
    if (serializer._stream->_direction == IO::kReading)
        rtcoll::RtColl_ArraySetSize_18005cc60(&_materials, n);     // +0xc8

    // LEFT: External.h's POD spellings (the serializer and the element array).
    RtObject_ExternalPtrArraySerialize_MaterialInstance(          // 0x18005d4a0
        PodSerializer(serializer), n,
        reinterpret_cast<RtObject::External**>(_materials._data));
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
