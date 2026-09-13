// ============================================================================
// runtime/src/graphics/graphics_typeinfo_accessors.cpp — WAVE 21 lane GFXVT
// ============================================================================
//
// EIGHT MINTED VTABLE SLOT BODIES, ACROSS THREE Graphics `??_7` ROWS.
//
// Subject and denominator, stated first. Lane GFXVT owns the 8 VTABLE rows of
// the 29-row Graphics census gap. Three of them carry a `?TypeName@` / `?Type@`
// (and two a `?GetFileVersion@`) slot with no body anywhere in the tree —
// verified by a mangled-name AND by-address grep over code/ engine/ runtime/
// standalone/ dx11/ before a line was written, which returned nothing
// (reference_grep_for_the_port_before_writing_it.md). This file is the first
// body for all eight.
//
// The population this file addresses is 8 slots out of the 29 unported
// non-purecall slots those 8 rows carry (see WAVE21_LANE_GFXVT.md §4 for the
// per-row table and its denominator). It does NOT address the other 21.
//
// THE SHAPE, AND IT IS THE ONE `object_typeinfo_accessors.cpp` ALREADY PORTED.
// Wave 11 lane OBJDATA did the identical seven for `??_7…@Object@@6B@`. This
// file follows it deliberately — same door, same oracle idiom, same
// `memcpy`-out-of-the-node reading — so that a reader who has read that file
// has read this one.
//
// THE BODIES, READ FROM THE SHIPPED Runtime-x64.dll BYTES (not from Ghidra's C,
// which drops dereferences — reference_ghidra_c_output_vs_disassembly.md). Each
// RIP displacement was resolved by hand against the NEXT instruction address:
//
//   ?Type@AnimationEvent@Graphics@@UEBAIXZ        0x1800116d0
//       8b 05 a2 82 1e 00 c3     MOV EAX,[0x1801f9978]   = node+0x00
//   ?TypeName@AnimationEvent@Graphics@@UEBAPEBDXZ 0x1800116f0
//       48 8b 05 89 82 1e 00 c3  MOV RAX,[0x1801f9980]   = node+0x08
//
//   ?Type@MaterialInstance@Graphics@@UEBAIXZ        0x1800027f0
//       8b 05 82 7f 1f 00 c3     MOV EAX,[0x1801fa778]   = node+0x00
//   ?TypeName@MaterialInstance@Graphics@@UEBAPEBDXZ 0x180002810
//       48 8b 05 69 7f 1f 00 c3  MOV RAX,[0x1801fa780]   = node+0x08
//
//   ?Type@Particle@Graphics@@UEBAIXZ        0x18001ff10
//       8b 05 9a a8 1d 00 c3     MOV EAX,[0x1801fa7b0]   = node+0x00
//   ?TypeName@Particle@Graphics@@UEBAPEBDXZ 0x18001ff30
//       48 8b 05 81 a8 1d 00 c3  MOV RAX,[0x1801fa7b8]   = node+0x08
//
//   ?GetFileVersion@MaterialInstance@Graphics@@UEBAHXZ 0x180103a60
//       b8 04 00 00 00 c3        MOV EAX,4    ; RET      -> literal 4
//   ?GetFileVersion@Particle@Graphics@@UEBAHXZ         0x180104080
//       b8 00 10 00 00 c3        MOV EAX,0x1000 ; RET    -> literal 4096
//
// `this` is read by NONE of the eight. That is not an inference from the
// decompile: the encodings above have no RCX operand at all. It is what makes
// the six node readers shareable across every instance of the class, and it is
// why the `self` parameter below is unnamed-but-present (the vtable slot is
// still called with the object in RCX).
//
// THE +0x00 / +0x08 READING IS CROSS-CHECKED, not merely decoded. All three
// node addresses appear in standalone/rt_data_mint_tables.inc as the minted
// `?_typeInfo@…@Graphics@@0VBaseTypeInfo@Object@@A` rows at exactly those VAs:
//
//   ?_typeInfo@AnimationEvent@Graphics@@    0x1801f9978   (.inc line 1833)
//   ?_typeInfo@MaterialInstance@Graphics@@  0x1801fa778   (.inc line 2057)
//   ?_typeInfo@Particle@Graphics@@          0x1801fa7b0   (.inc line 2089)
//
// so the two fields a `BaseTypeInfo` node's readers touch are checked against
// the generated table and not only against this lane's own byte decode.
//
// THE DOOR, and why it is a NAME door. `0V…A` is the PRIVATE static spelling
// and this DLL exports its private members, so a name door is the correct door
// and no address door is needed (reference_aeaa_is_private_not_unexported.md).
// Reading through `RT_DATA_SLOT` rather than pinning the DLL's node is what
// makes these bodies FOLLOW a promotion of the `?_typeInfo@` row: promote
// `?_typeInfo@MaterialInstance@Graphics@@` (lane GFXDATA's subject, not this
// lane's) and this file's `?Type@MaterialInstance@` reads OUR node, which is
// the arrangement cutover needs. That coupling is the reason these accessors
// live here and the nodes stay with GFXDATA — two lanes, one contract, no
// shared file.
//
// ⚠ A POINTER DIVERGENCE IS EXPECTED, NOT A REGRESSION, once any of the three
//   `?_typeInfo@` rows is promoted: our node lives in our image and the shipped
//   export returns the DLL's — same characters, different object. This is
//   exactly what `UI::SpriteSheet::TypeName` and `Object::External::TypeName`
//   already report. The comparison below is deliberately left AS A POINTER
//   rather than relaxed to a strcmp, because it is the one screen that can
//   still see identity move, and the identity question itself is retired from
//   both shipped binaries (zero consumers of any node's +0x08 —
//   reference_basetypeinfo_name_is_content_only.md).
//
// WHAT THIS FILE DOES NOT DO. It does not promote any `??_7` row: registering a
// slot body makes the row COMPLETE-able, and COMPLETE is the STATIC half of
// readiness only (reference_a_green_readiness_screen_is_not_a_route.md). The
// promotability verdict for each row, with its stamper evidence, is in
// WAVE21_LANE_GFXVT.md §5. `??_7AnimationEvent@Graphics@@6B@` reaches 4/4
// COMPLETE with this file; `??_7MaterialInstance@` reaches 5/9 and
// `??_7Particle@` reaches 5/6, both still incomplete.
//
// KILL SWITCH: RT_GFXTI=0 leaves all eight on the DLL.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"

extern "C" uint32_t    RtGraphics_AnimationEvent_Type(const void*);
extern "C" const char* RtGraphics_AnimationEvent_TypeName(const void*);
extern "C" uint32_t    RtGraphics_MaterialInstance_Type(const void*);
extern "C" const char* RtGraphics_MaterialInstance_TypeName(const void*);
extern "C" int32_t     RtGraphics_MaterialInstance_GetFileVersion(const void*);
extern "C" uint32_t    RtGraphics_Particle_Type(const void*);
extern "C" const char* RtGraphics_Particle_TypeName(const void*);
extern "C" int32_t     RtGraphics_Particle_GetFileVersion(const void*);

namespace {

// ---- the exported BaseTypeInfo nodes these bodies read ----------------------
//
// BaseTypeInfo's first two fields, and the ONLY two any of these bodies touch:
//     +0x00  uint32_t     typeId
//     +0x08  const char*  name
// Read with memcpy rather than a struct cast so no alignment or strict-aliasing
// assumption is made about storage that may be ours or the DLL's depending on
// whether the `?_typeInfo@` row is promoted.
constexpr size_t kTypeInfoTypeIdOffset = 0x00;
constexpr size_t kTypeInfoNameOffset   = 0x08;
static_assert(kTypeInfoTypeIdOffset == 0, "BaseTypeInfo::typeId is at +0x00");
static_assert(kTypeInfoNameOffset == 8,   "BaseTypeInfo::name is at +0x08");
static_assert(sizeof(uint32_t) == 4,      "typeId is a DWORD (MOV EAX, not RAX)");
static_assert(sizeof(const char*) == 8,   "name is a QWORD (MOV RAX)");

const uint8_t* AnimationEventTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@AnimationEvent@Graphics@@0VBaseTypeInfo@Object@@A");
}
const uint8_t* MaterialInstanceTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@MaterialInstance@Graphics@@0VBaseTypeInfo@Object@@A");
}
const uint8_t* ParticleTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@Particle@Graphics@@0VBaseTypeInfo@Object@@A");
}

typedef uint32_t    (*U32ThisFn)(const void*);
typedef const char* (*CStrThisFn)(const void*);
typedef int32_t     (*I32ThisFn)(const void*);

uint32_t ReadTypeId(const uint8_t* node) {
    uint32_t v = 0;
    std::memcpy(&v, node + kTypeInfoTypeIdOffset, sizeof v);
    return v;
}
const char* ReadTypeName(const uint8_t* node) {
    const char* v = nullptr;
    std::memcpy(&v, node + kTypeInfoNameOffset, sizeof v);
    return v;
}

}  // namespace

// ---- ??_7AnimationEvent@Graphics@@6B@  slots 2 and 1 -----------------------

extern "C" uint32_t RtGraphics_AnimationEvent_Type(const void* self) {
    const uint32_t ours = ReadTypeId(AnimationEventTypeInfo());
    return ours;
}

extern "C" const char* RtGraphics_AnimationEvent_TypeName(const void* self) {
    const char* const ours = ReadTypeName(AnimationEventTypeInfo());
    return ours;
}

// ---- ??_7MaterialInstance@Graphics@@6B@  slots 2, 1 and 3 ------------------

extern "C" uint32_t RtGraphics_MaterialInstance_Type(const void* self) {
    const uint32_t ours = ReadTypeId(MaterialInstanceTypeInfo());
    return ours;
}

extern "C" const char* RtGraphics_MaterialInstance_TypeName(const void* self) {
    const char* const ours = ReadTypeName(MaterialInstanceTypeInfo());
    return ours;
}

// `MOV EAX,4 ; RET`. A literal, not a read: the on-disk serialised format
// version of a Graphics::MaterialInstance. Faithful means returning 4, NOT
// deriving it from anything in our tree — a derived spelling would silently
// track OUR format if this class's Serialize is ever revised, and retail's
// does not.
extern "C" int32_t RtGraphics_MaterialInstance_GetFileVersion(const void* self) {
    const int32_t ours = 4;
    return ours;
}

// ---- ??_7Particle@Graphics@@6B@  slots 2, 1 and 3 --------------------------

extern "C" uint32_t RtGraphics_Particle_Type(const void* self) {
    const uint32_t ours = ReadTypeId(ParticleTypeInfo());
    return ours;
}

extern "C" const char* RtGraphics_Particle_TypeName(const void* self) {
    const char* const ours = ReadTypeName(ParticleTypeInfo());
    return ours;
}

// `MOV EAX,0x1000 ; RET` — 4096, and it is NOT the same constant as
// MaterialInstance's 4. The two were decoded separately and the difference is
// the reason each is written as its own literal here.
extern "C" int32_t RtGraphics_Particle_GetFileVersion(const void* self) {
    const int32_t ours = 0x1000;
    return ours;
}

// ============================================================================
// REGISTRATION — by NAME and by ADDRESS, both, for every body.
//
// A body PORTED but never REGISTERED is indistinguishable from an ABSENT one to
// the census (reference_half_registered_port_reads_as_absent.md), and the mint
// fills a slot by NAME for RT_MINT_NAMED and by ADDRESS otherwise, so a row
// that wants to become COMPLETE needs whichever door its slot uses. All eight
// of these are RT_MINT_NAMED; the address door is registered anyway so that a
// by-address call site (ResolveAddr consults the port registry FIRST —
// reference_resolveaddr_consults_port_registry_first.md) reaches the same body.
// ============================================================================
int RtGraphicsTypeInfoAccessorsRegister() {
    int n = 0;

    RegisterRuntimeExport("?Type@AnimationEvent@Graphics@@UEBAIXZ",
                          (void*)&RtGraphics_AnimationEvent_Type); ++n;
    RegisterRuntimeAddr(0x1800116d0ull, (void*)&RtGraphics_AnimationEvent_Type);

    RegisterRuntimeExport("?TypeName@AnimationEvent@Graphics@@UEBAPEBDXZ",
                          (void*)&RtGraphics_AnimationEvent_TypeName); ++n;
    RegisterRuntimeAddr(0x1800116f0ull, (void*)&RtGraphics_AnimationEvent_TypeName);

    RegisterRuntimeExport("?Type@MaterialInstance@Graphics@@UEBAIXZ",
                          (void*)&RtGraphics_MaterialInstance_Type); ++n;
    RegisterRuntimeAddr(0x1800027f0ull, (void*)&RtGraphics_MaterialInstance_Type);

    RegisterRuntimeExport("?TypeName@MaterialInstance@Graphics@@UEBAPEBDXZ",
                          (void*)&RtGraphics_MaterialInstance_TypeName); ++n;
    RegisterRuntimeAddr(0x180002810ull, (void*)&RtGraphics_MaterialInstance_TypeName);

    RegisterRuntimeExport("?GetFileVersion@MaterialInstance@Graphics@@UEBAHXZ",
                          (void*)&RtGraphics_MaterialInstance_GetFileVersion); ++n;
    RegisterRuntimeAddr(0x180103a60ull, (void*)&RtGraphics_MaterialInstance_GetFileVersion);

    RegisterRuntimeExport("?Type@Particle@Graphics@@UEBAIXZ",
                          (void*)&RtGraphics_Particle_Type); ++n;
    RegisterRuntimeAddr(0x18001ff10ull, (void*)&RtGraphics_Particle_Type);

    RegisterRuntimeExport("?TypeName@Particle@Graphics@@UEBAPEBDXZ",
                          (void*)&RtGraphics_Particle_TypeName); ++n;
    RegisterRuntimeAddr(0x18001ff30ull, (void*)&RtGraphics_Particle_TypeName);

    RegisterRuntimeExport("?GetFileVersion@Particle@Graphics@@UEBAHXZ",
                          (void*)&RtGraphics_Particle_GetFileVersion); ++n;
    RegisterRuntimeAddr(0x180104080ull, (void*)&RtGraphics_Particle_GetFileVersion);

    return n;
}
