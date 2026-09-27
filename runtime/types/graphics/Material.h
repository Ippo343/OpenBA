#pragma once
// ============================================================================
// runtime/types/graphics/Material.h — Graphics::MaterialInstance
// ============================================================================
//
// Wave 6 lane F of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2/§6).
// Offsets read out of the shipped Runtime-x64.dll in Ghidra; the function that
// pins each one is named beside it. Write-up: docs/runtime_port/WAVE6_LANE_F.md.
//
// `Graphics::Material` itself is NOT laid out here. Lane F ports no Material
// method, and the only Material fields any ported body touches are reached
// through a `void*` (SetMaterial just stores the ExternalPtr). Pinning a class
// this lane cannot verify would be a guess in a header whose whole contract is
// that it is not.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../collection/Collection.h"     // Collection::Array
#include "../object/External.h"           // Object::External, Object::ExternalPtr<T>
#include "../object/Internal.h"           // ENDGAME B.3.8 RB: InternalMemFree (the concrete slot 0)
                                          // (ENDGAME B.3.5 block G — the real
                                          // classes at the bottom of this file
                                          // derive from / hold these)

namespace RtGraphics {

// ============================================================
// The three element types MaterialInstance's arrays hold.
// ============================================================

// One texture binding, stride 0x10.
// Pinned by ?AddTexture@MaterialInstance@Graphics@@... (0x180103350), which
// builds exactly this pair on the stack — a System::String (one pointer, its
// length/refcount header at data-8/data-4, WAVE2) and an
// Object::ExternalPtr<Video::ImageBuffer> (one pointer) — and hands it to the
// array-grow helper at 0x180061480, whose element stride is 0x10 and which
// bumps the refcount at `*element - 4` when it copies the pair.
// The +0x08 half is read back by ?BuildParameters@... (0x1801033e0) as an
// object whose +0x2c is compared against 4 and 5.
struct MaterialTexture {
    char* name;    // +0x00  System::String::_data (may be null)
    void* image;   // +0x08  Object::ExternalPtr<Video::ImageBuffer>, by value
};
static_assert(sizeof(MaterialTexture) == 0x10, "MaterialInstance texture entry is 0x10");

// One resolved shader parameter, stride 0x08. Pinned by
// ?Setup@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z (0x180102c90),
// which reads `+0x00` as an index into the texture/constant array and `+0x04`
// as the shader slot to bind it to. BuildParameters writes the same pair.
struct MaterialBinding {
    int32_t sourceIndex;   // +0x00  index into MaterialInstance::textures / constants
    int32_t shaderSlot;    // +0x04  slot number on Video::Context
};
static_assert(sizeof(MaterialBinding) == 0x08, "MaterialInstance binding entry is 0x08");

// One per-shader set of bindings, stride 0x20 — two Array triples back to back.
// Pinned by Setup (0x180102c90): `set = _parameters._data + shader*0x20`, then
// `set[0]`/`(int)set[1]` walk the texture bindings and `set[2]`/`(int)set[3]`
// the constant bindings; and by BuildParameters, which appends into
// `+0x00..+0x0f` and `+0x10..+0x1f` of the same record.
struct MaterialParameterSet {
    Collection::Array<MaterialBinding> textures;    // +0x00..+0x0f
    Collection::Array<MaterialBinding> constants;   // +0x10..+0x1f
};
static_assert(sizeof(MaterialParameterSet) == 0x20, "MaterialInstance parameter set is 0x20");

// One constant value, stride 0x18 with a Vector4 at +0x08. Pinned by Setup,
// which compares and copies four consecutive floats from `+0x08..+0x17` of
// `_constants._data + index*0x18` into Video::Context+0x208 + slot*0x10.
// +0x00..+0x07 is not read by any function this lane decompiled.
struct MaterialConstant {
    uint8_t _head[0x08];   // +0x00  not read by lane F
    float   value[4];      // +0x08..+0x17
};
static_assert(sizeof(MaterialConstant) == 0x18, "MaterialInstance constant entry is 0x18");
static_assert(offsetof(MaterialConstant, value) == 0x08, "MaterialConstant::value @ +0x08");

// ============================================================
// Graphics::MaterialInstance — an Object::External subclass, 0x60 bytes.
//
// Pinned by ??0MaterialInstance@Graphics@@QEAA@XZ (0x180103980), which is the
// whole layout in one function: it stamps ??_7Base@Object@@6B@ (0x180154788),
// then ??_7External@Object@@6B@ (0x1801547a0), zeroes +0x08/+0x10/+0x18, stamps
// ??_7MaterialInstance@Graphics@@6B@ (0x1801545e0), then zeroes +0x20 (dword),
// +0x24 (WORD — so +0x24 and +0x25 are two separate bytes, not an int) and the
// seven qwords +0x28..+0x5f. The last store ends at +0x5f, so sizeof is 0x60 —
// which also matches the pre-existing note at engine/funcs/engine_funcs.h:3513.
//
// The two bytes at +0x24/+0x25 are named by BuildParameters (0x1801033e0):
// it clears +0x24, then ORs into +0x24 whether any bound texture's +0x2c == 4
// and into +0x25 whether any is == 5. AddDrawItem (0x1801057b0) tests +0x25 to
// decide whether to call MaterialInstance::RenderTextures — so +0x25 is "this
// material samples something that must be rendered first".
// ============================================================
struct MaterialInstance {
    void*    vptr;                 // +0x00
    void*    external_08;          // +0x08  Object::External members; lane F does
    void*    external_10;          // +0x10  not port External, so these keep the
    void*    external_18;          // +0x18  names their owner (Wave 3) gave them
    uint32_t shaderClass;          // +0x20  copied from Material+0x20 by BuildParameters;
                                   //        AddDrawItem uses it as a bit index into
                                   //        the draw group's pass mask AND as the
                                   //        0..2 item-list index
    uint8_t  hasRenderTexture;     // +0x24  any bound texture with kind == 4
    uint8_t  hasRenderedTexture;   // +0x25  any bound texture with kind == 5
    uint8_t  _pad_26[2];           // +0x26
    void*    material;             // +0x28  Object::ExternalPtr<Material>, by value
    Collection::Array<MaterialParameterSet> parameters;  // +0x30..+0x3f
    Collection::Array<MaterialConstant>     constants;   // +0x40..+0x4f
    Collection::Array<MaterialTexture>      textures;    // +0x50..+0x5f
};
static_assert(sizeof(MaterialInstance) == 0x60, "Graphics::MaterialInstance must be 0x60 bytes");
static_assert(offsetof(MaterialInstance, shaderClass)        == 0x20, "MaterialInstance::shaderClass @ +0x20");
static_assert(offsetof(MaterialInstance, hasRenderTexture)   == 0x24, "MaterialInstance::hasRenderTexture @ +0x24");
static_assert(offsetof(MaterialInstance, hasRenderedTexture) == 0x25, "MaterialInstance::hasRenderedTexture @ +0x25");
static_assert(offsetof(MaterialInstance, material)           == 0x28, "MaterialInstance::material @ +0x28");
static_assert(offsetof(MaterialInstance, parameters)         == 0x30, "MaterialInstance::parameters @ +0x30");
static_assert(offsetof(MaterialInstance, constants)          == 0x40, "MaterialInstance::constants @ +0x40");
static_assert(offsetof(MaterialInstance, textures)           == 0x50, "MaterialInstance::textures @ +0x50");

}  // namespace RtGraphics

// ############################################################################
// ENDGAME B.3 step 5 (block G) — `Graphics::Material` and
// `Graphics::MaterialInstance`, the REAL C++ classes
// ############################################################################
//
// B35_RULES section 4 welds the two together ("`MaterialInstance` reads
// `Material`'s render state; `RendererDrawItem::material` is
// `MaterialInstance+0x28` by value"), so they convert in one commit, and both
// live here because `MaterialInstance`'s four element types already do.
//
// PARENT — `Object::External` for BOTH, and it is read, not inferred:
//     ?_ParentTypeName@Material@Graphics@@CAPEBDXZ         @0x1800118a0 -> "Object::External"
//     ?_ParentTypeName@MaterialInstance@Graphics@@CAPEBDXZ @0x1800027e0 -> "Object::External"
// (B35_SLOTS.md section 3, read byte-wise.) NOT `CompiledExternal`: both
// constructors stamp `??_7Base@Object@@6B@` then `??_7External@Object@@6B@` then
// their own, with no `CompiledExternal` step — `??0Material@Graphics@@QEAA@XZ`
// @0x1801029a0 disassembled is exactly those three LEA/MOV pairs. That is the
// mistake B35_RULES section 2.1 warns about for `Video::VideoProgram`.
//
// RULE B5-15 — THE FOUR NAME LITERALS, COPIED FROM B35_SLOTS.md AND RECORDED,
// NOT USED. No `_typeInfo` row is retired and no `DEFINE_TYPE` is introduced
// here (B35_RULES section 11); slots 1/2 keep reading
// `?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A` (0x1801fa740) and
// `?_typeInfo@MaterialInstance@Graphics@@0VBaseTypeInfo@Object@@A` (0x1801fa778).
// The literals are written down so a later `DEFINE_TYPE` is COPIED:
//     ?_TypeName@Material@Graphics@@         -> "Material"
//     ?_ParentTypeName@Material@Graphics@@   -> "Object::External"
//     ?_TypeName@MaterialInstance@Graphics@@ -> "MaterialInstance"
//     ?_ParentTypeName@MaterialInstance@…    -> "Object::External"
//
// *** B35_RULES section 2.2 LISTS `Graphics::Material`'s FIELDS AS UNDOCUMENTED
// AND section 13 ITEM 3 SAYS THE SIZE IS VERIFIED BUT THE FIELDS ARE NOT.
// THE FIELDS WERE ALREADY PINNED. *** `runtime/src/graphics/material.cpp:47-55`
// carried a file-local `struct Material` with all of 0x38 asserted, written by
// the batch that ported all seven of its bodies. This header MOVES that
// declaration under the retail name; it does not re-derive it. The one thing
// added is the NAMES, taken from the exported getters and confirmed against the
// retail disassembly this pass:
//
//   +0x20 `_type`     `?GetType@Material@Graphics@@QEBAHXZ` @0x180011bd0 is
//                     `MOV EAX,[RCX+0x20] ; RET` — one instruction, so +0x20 is
//                     the whole of it and it is a dword. (material.cpp called it
//                     `_shaderType`; `GetType` is the stronger evidence, and
//                     `MaterialInstance::shaderClass` at ITS +0x20 is a COPY of
//                     this field made by `BuildParameters`.)
//   +0x28 `_passes`   `?GetPassCount@Material@Graphics@@QEBAHXZ` @0x180102970 is
//                     `MOV EAX,[RCX+0x30] ; RET` — i.e. the `_size` of an Array
//                     header based at +0x28 — and `?IsPassValid@` @0x180102790
//                     bounds-checks the index against that same +0x30 before
//                     doing `MOV RAX,[RCX+0x28] / MOVSXD RDX,EDX /
//                     MOV RAX,[RAX+RDX*8]`. So the element is EIGHT bytes and
//                     the array is based at +0x28. The element TYPE comes from
//                     the same body's follow-on reads: it dereferences
//                     `pass+0x20`, `pass+0x28` and `pass+0x30`, and
//                     `?GetRenderState@Material@…` @0x180102980 reads
//                     `pass+0x38` — which is `Video::VideoProgram`'s four
//                     `ExternalPtr` members in the order its own four getters
//                     (0x18004ee10/20/30/40) expose them. So a "pass" is an
//                     `Object::ExternalPtr<Video::VideoProgram>`.
//
// *** THE `ExternalPtr<T>` GETTERS **ARE** HIDDEN-RETSLOT. SETTLED FROM THE
// RETAIL PROLOGUE, 2026-09-19, BLOCK G. *** The COORDINATOR AMENDMENT to RULE
// B5-12 records block E's doubt that B35_RULES section 2.5's "eighteen
// `ExternalPtr<T>` getters" are retslot at all, on the ground that an 8-byte
// trivially copyable class returns in RAX, and asks each getter to be settled
// from its own prologue. Two of them are this block's, and both disassemble as
// a retslot:
//
//   ?GetRenderState@Material@Graphics@@…H@Z  @0x180102980
//       MOV RAX,[RCX+0x28] ; MOVSXD RCX,R8D ; MOV RCX,[RAX+RCX*8]
//       MOV RAX,[RCX+0x38] ; MOV [RDX],RAX  ; MOV RAX,RDX ; RET
//   ?GetTexture@MaterialInstance@Graphics@@…H@Z  @0x180103100
//       MOV RAX,[RCX+0x50] ; MOVSXD RCX,R8D ; ADD RCX,RCX
//       MOV RCX,[RAX+RCX*8+0x8] ; MOV RAX,RDX ; MOV [RDX],RCX ; RET
//
// `this` is RCX, the caller's storage is RDX, the index is R8D, the value is
// STORED THROUGH RDX and RDX comes back in RAX. That is the MSVC hidden return
// slot exactly, on an 8-byte class — so MSVC did NOT treat `ExternalPtr<T>` as
// trivially copyable for return purposes, and section 2.5 is RIGHT. Every door
// for one of these keeps the `(self, ret, args…)` shape. The C++ MEMBERS below
// are nevertheless declared with an ordinary by-value return, per the
// amendment's second half: none of them is a vtable slot, so both ends of every
// C++ call are this same GCC, and the registered door stays a separate function
// that writes the caller's slot and returns it.
//
// ABSTRACT, per B34_BRIEFS A.4 as amended: nothing in the tree holds either
// class BY VALUE as a C++ member (`RendererDrawItem` holds
// `MaterialInstance`'s +0x28 ExternalPtr, which is a HANDLE, not the object),
// and each slot 0 is a row the mint owns — `runtime/src/object/deleting_dtor.cpp`
// row 50 for Material (0x180011d20) and `standalone/reg_mint_promoted.cpp` for
// MaterialInstance (0x180002d60) — which B35_RULES section 11 forbids retiring
// in step 5. So slot 0 stays `= 0` on both.
//
// NO REAL CONSTRUCTOR (B4b-2, ENDGAME section B.1 decision 3): `Construct` and
// `Reconstruct` run on objects that ALREADY EXIST and that wear the MINTED
// table, and they re-stamp the three vtables by hand out of the mint. A member
// constructor there would stamp GCC's tables into a minted object.
// ############################################################################

// Classes other blocks own; taken by reference / pointer under their real
// retail names, bridged at the use site and never in this header.
namespace System { class Filename; }
namespace Video  { class Context; class ImageBuffer; class RenderState;
                   class VideoProgram; }

namespace Graphics {

class Camera;     // block F  (runtime/types/graphics/Camera.h)
class Renderer;   // block F
class Scene;      // block F

// ============================================================================
// Graphics::Material — 0x38 bytes, NINE slots, `Object::External`.
//
// `??_7Material@Graphics@@6B@` @ 0x180154d10, read as `.rdata` qwords
// (B35_SLOTS.md:137-146):
//
//   [0] +0x00  0x180011d20  (unexported) Graphics_Material_DeletingDtor
//   [1] +0x08  0x1800118d0  ?TypeName@Material@Graphics@@UEBAPEBDXZ
//   [2] +0x10  0x1800118b0  ?Type@Material@Graphics@@UEBAIXZ
//   [3] +0x18  0x180102a40  ?GetFileVersion@…  (`MOV EAX,5 ; RET`)
//   [4] +0x20  0x1800066e0  ?Compile@External@Object@@  INHERITED — NOT declared
//   [5] +0x28  0x180102a50  ?Serialize@…
//   [6] +0x30  0x180011ae0  ?Reconstruct@…
//   [7] +0x38  0x180011bc0  ?Destruct@…
//   [8] +0x40  0x180011b50  ?Construct@…  — INTRODUCED HERE (RULE B5-16)
//
// sizeof 0x38 from the `MOV EDX,0x38` in slot 0's vector deleting destructor,
// and independently from the ctor's last store ending at +0x37.
// ============================================================================
//
// ENDGAME B.3 step 9 (block GFX, B39_RULES D1/D2): CONCRETE. A real public
// ctor (retail 0x1801029a0), its own vector slot 0, a protected non-virtual
// ~Material holding the retired ??1 door's body, the factory pair as the class's
// own statics; Construct / Reconstruct placement-new the class. The minted
// `??_7Material@Graphics@@6B@`, its RT_DELDTOR row 0x180011d20, the FACTREST
// row and the ctor-body / ??1 / slot doors are retired (rt_mint_retired/gfx.txt).
// ============================================================================
class Material : public Object::External {
public:
    // ??0Material@Graphics@@QEAA@XZ 0x1801029a0 -- the REAL ctor (retail also
    // inlines it into both factories; Construct tail-jumps it): External(), then
    // +0x20 = 0 (dword) and the `_passes` header zeroed (its own ctor). +0x24 is
    // written by nothing.
    Material() : External() {
        _type = 0;
    }

    // [0] 0x180011d20 -- MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0x38` = the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<Material*>(self + static_cast<int64_t>(i) * 0x38)->~Material();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~Material();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }

    const char* TypeName() const override;                 // [1] 0x1800118d0
    uint32_t    Type() const override;                     // [2] 0x1800118b0
    int32_t     GetFileVersion() const override;           // [3] 0x180102a40
    bool        Serialize(void* stream) override;           // [5] 0x180102a50
    Object::External* Reconstruct() override;               // [6] 0x180011ae0
    void        Destruct() override;                        // [7] 0x180011bc0
    virtual Object::External* Construct();                  // [8] 0x180011b50 — B5-16
    // slot 4 is `Object::External::Compile`'s own body, inherited unchanged.

    // ---- the non-slot exported surface -------------------------------------
    // PORTED, bodies in `runtime/src/graphics/wave28_lane_gfxbody.cpp`. Both
    // are hidden-retslot exports whose retail body passes the CALLER's slot
    // straight through to `Video::ShaderProgram`'s own retslot export, so the
    // member keeps the out-parameter spelled rather than materialising a
    // `System::String` temporary and copying a refcounted pointer out of it.
    // The door is the same function with the retail argument order.
    RtSystem::String* GetConstantName(RtSystem::String* ret,
                                      int32_t pass, int32_t index) const;  // 0x180102830
    RtSystem::String* GetTextureName(RtSystem::String* ret,
                                     int32_t pass, int32_t index) const;   // 0x180102900

    // Exported, NOT PORTED anywhere in the tree, and no caller: declared so no
    // later block or caller batch has to edit this header, deliberately left
    // undefined. Each walks `_passes` and forwards to the pass's
    // `Video::VideoProgram` / `Video::ShaderProgram`, over offsets this header
    // already pins.
    int32_t  GetType() const;                               // 0x180011bd0
    bool     IsPassValid(int32_t pass) const;               // 0x180102790
    void     Setup(Video::Context& context, int32_t pass) const;  // 0x1801027e0
    int32_t  GetConstantDest(int32_t pass, int32_t index) const;   // 0x180102800
    int32_t  GetConstantCount(int32_t pass) const;          // 0x180102880
    bool     IsTextureInternal(int32_t pass, int32_t index) const; // 0x1801028a0
    int32_t  GetTextureDest(int32_t pass, int32_t index) const;    // 0x1801028d0
    int32_t  GetTextureCount(int32_t pass) const;           // 0x180102950
    int32_t  GetPassCount() const;                          // 0x180102970
    // RETSLOT in retail (see the banner); a plain by-value return here.
    Object::ExternalPtr<Video::RenderState> GetRenderState(int32_t pass) const;  // 0x180102980

    Material(const Material& src);                          // 0x180011be0  not ported
    Material& operator=(const Material& src);               // 0x180011c80  not ported

    static uint32_t TypeId();                               // 0x1800118c0  B.3.9: minted record until W3
    // The three operators stay declared and undefined (placement is `::new`);
    // the factory pair is the class's own (B39_RULES D2, material.cpp).
    static void* operator new(std::size_t bytes);            // 0x1800118e0
    static void* operator new(std::size_t bytes, void* place);// 0x180011950
    static void  operator delete(void* p);                   // 0x180011960
    static Object::Base* CreateNewObject();                  // 0x1800119c0
    static Object::Base* CreateInPlaceObject(void* place);   // 0x180011a70

protected:
    // `??1Material@Graphics@@UEAA@XZ` @ 0x180102a00. NON-VIRTUAL (decision 2).
    // ENDGAME B.3.9 GFX: the retired door's body (material.cpp) -- retail's
    // out-of-line ~Array of `_passes`; the restamp and ~External are the
    // compiler's chain.
    ~Material();

public:
    // ---- the 0x18 bytes Material adds after Object::External ---------------
    // PUBLIC for the reason `Object::External`'s are: unconverted bodies reach
    // them at these offsets through a cast.
    int32_t  _type;      // +0x20  ?GetType@ 0x180011bd0; ctor 0
    uint32_t _pad24;     // +0x24  never written by any body read
    // +0x28 data, +0x30 size, +0x34 capacity. Typed with the REAL
    // `Object::ExternalPtr<T>`, which this header already includes (block E's
    // `Graphics::ExternalPtrHandle` alias in Geometry.h exists only to keep
    // External.h OUT of the Geometry base and is left alone).
    Collection::Array<Object::ExternalPtr<Video::VideoProgram> > _passes;  // +0x28
};
static_assert(sizeof(Material) == 0x38,
              "Graphics::Material is 0x38 bytes (the MOV EDX,0x38 in its own "
              "slot-0 vector deleting destructor 0x180011d20)");
static_assert(alignof(Material) == 8, "Graphics::Material aligns to 8 (the vptr)");
static_assert(offsetof(Material, _type)   == 0x20, "Material::_type @ +0x20 (GetType 0x180011bd0)");
static_assert(offsetof(Material, _passes) == 0x28, "Material::_passes @ +0x28 (IsPassValid 0x180102790)");
static_assert(sizeof(Object::ExternalPtr<Video::VideoProgram>) == 8,
              "Material::_passes has stride 8 (MOV RAX,[RAX+RDX*8] at 0x1801027a0)");

// ============================================================================
// Graphics::MaterialInstance — 0x60 bytes, NINE slots, `Object::External`.
//
// `??_7MaterialInstance@Graphics@@6B@` @ 0x1801545e0 (B35_SLOTS.md:148-157):
//
//   [0] +0x00  0x180002d60  (unexported) vector deleting dtor
//   [1] +0x08  0x180002810  ?TypeName@…
//   [2] +0x10  0x1800027f0  ?Type@…
//   [3] +0x18  0x180103a60  ?GetFileVersion@…  (`MOV EAX,4 ; RET`)
//   [4] +0x20  0x1800066e0  ?Compile@External@Object@@  INHERITED — NOT declared
//   [5] +0x28  0x180103a70  ?Serialize@…
//   [6] +0x30  0x180002a50  ?Reconstruct@…
//   [7] +0x38  0x180002b50  ?Destruct@…
//   [8] +0x40  0x180002ad0  ?Construct@…  — INTRODUCED HERE (RULE B5-16)
//
// The fields are `RtGraphics::MaterialInstance`'s, unchanged and cross-asserted
// below; that POD's banner carries the per-field evidence and stays, because it
// has live users outside this block's file set.
// ============================================================================
class MaterialInstance : public Object::External {
public:
    // [0] CONCRETE (ENDGAME B.3.8 block RB, B38_RULES D1/D2): the exe
    // instantiates the class itself (its table copy 0x14015c400; ModDialog,
    // FrustumOutlineDescription and two more stampers), whose slot 0 is the
    // exe scalar deleting dtor 0x1400121a0 (~MaterialInstance; MemFree on
    // flags & 1). The DLL's 0x180002d60 stays the mint's RT_DELDTOR row.
    Object::Base* DeletingDtor(unsigned flags) override {
        this->~MaterialInstance();
        if (flags & 1) Object::detail::InternalMemFree(this);
        return this;
    }

    const char* TypeName() const override;                 // [1] 0x180002810
    uint32_t    Type() const override;                     // [2] 0x1800027f0
    int32_t     GetFileVersion() const override;           // [3] 0x180103a60
    bool        Serialize(void* stream) override;           // [5] 0x180103a70
    Object::External* Reconstruct() override;               // [6] 0x180002a50
    void        Destruct() override;                        // [7] 0x180002b50
    virtual Object::External* Construct();                  // [8] 0x180002ad0 — B5-16

    // ---- PORTED non-virtual members ---------------------------------------
    // Bodies in the wave-lane files this block owns; each keeps its registered
    // door, which becomes a QUALIFIED non-virtual delegation.
    void    Setup(Video::Context& context, int32_t pass) const;   // 0x180102c90  wave21
    void    RenderTextures(Renderer& renderer, const Scene& scene,
                           const Camera& camera, int32_t pass) const;  // 0x180102df0  wave9
    void    GenerateTextures(Video::Context& context, int32_t pass) const;  // 0x180102eb0  batchc
    int32_t FindConstant(const RtSystem::String& name) const;     // 0x180102f40  wave28
    int32_t FindTexture(const RtSystem::String& name) const;      // 0x180103020  wave28
    void    BuildParameters();                                    // 0x1801033e0  wave27
    // Bodies in `runtime/src/graphics/renderer.cpp`, moved onto the members by
    // block GC. Both take `Object::ExternalPtr<T>` BY VALUE, as retail does: the
    // handle is 8 bytes and trivially copyable, so it arrives in a register and
    // there is NO hidden return slot and no reference to dereference. Neither
    // touches a refcount on the handle itself (`AddTexture` refcounts the NAME).
    void    SetMaterial(Object::ExternalPtr<Material> material);   // 0x180103340  renderer
    void    AddTexture(const RtSystem::String& name,
                       Object::ExternalPtr<Video::ImageBuffer> image);  // 0x180103350  renderer

    // ---- exported, NOT PORTED, no caller: declared, deliberately undefined --
    int32_t GetType() const;                               // 0x180002b60  reads _shaderClass
    const Material* GetMaterial() const;                   // 0x180002b70
    int32_t HasGeneratedTextures() const;                  // 0x180002b80  reads +0x24
    int32_t HasRenderedTextures() const;                   // 0x180002b90  reads +0x25
    int32_t GetMaxTextureHeight() const;                   // 0x180102b10
    int32_t GetMaxTextureWidth() const;                    // 0x180102bd0
    // RETSLOT in retail (0x180103100 disassembled in the banner); plain
    // by-value returns here. THE DOOR DECIDES THE OVERLOAD (B5-14): the two
    // `GetTexture`s differ only in their argument, and retail exports both.
    Object::ExternalPtr<Video::ImageBuffer> GetTexture(int32_t index) const;      // 0x180103100
    Object::ExternalPtr<Video::ImageBuffer> GetTexture(const RtSystem::String& name) const; // 0x180103120
    void SetTexture(const RtSystem::String& name,
                    Object::ExternalPtr<Video::ImageBuffer> image);   // 0x180103180
    // `AddTexture` (0x180103350) and `SetMaterial` (0x180103340) were listed here
    // as unported; they are NOT — both have registered, ported doors in
    // `renderer.cpp` and are declared with the ported members above. Corrected by
    // block GC, 2026-09-19.

    MaterialInstance(const MaterialInstance& src);          // 0x180002ba0  not ported
    MaterialInstance& operator=(const MaterialInstance& src);// 0x180002c80  not ported

    static uint32_t TypeId();                               // 0x180002800  not ported
    // `??2MaterialInstance@Graphics@@SAPEAX_K@Z` (0x180002820) IS ported, in
    // `wave21_lane_gfxbody.cpp`; the other four reflection statics are not.
    // All five are declared so no caller batch edits this header.
    static void* operator new(std::size_t bytes);            // 0x180002820  wave21
    static void* operator new(std::size_t bytes, void* place);// 0x180002890
    static void  operator delete(void* p);                   // 0x1800028a0
    static Object::Base* CreateNewObject();                  // 0x180002900
    static Object::Base* CreateInPlaceObject(void* place);    // 0x1800029d0

    // ??0MaterialInstance@Graphics@@QEAA@XZ 0x180103980 — the REAL constructor
    // (ENDGAME B.3.8 block RB, RULE B8-1), PUBLIC: the class is concrete.
    // External() and the stamps are the compiler's; the field stores are ONE
    // body with the door `RtGraphics_MaterialInstance_Ctor` (renderer.cpp).
    MaterialInstance() { InitFields(); }

    struct DoorBody { static void Init(MaterialInstance* m) { m->InitFields(); } };

protected:
    // ??1MaterialInstance@Graphics@@UEAA@XZ 0x180103a00: restamp, the three
    // retail ~Array calls (+0x50, +0x40, +0x30), ~External. The body is
    // `ReleaseFields` (renderer.cpp); the restamp and ~External are the
    // compiler's. Its registered door `RtGraphics_MaterialInstance_Dtor` is in
    // runtime/src/object/deleting_dtor_leaves.cpp (coordinator-owned, not
    // edited by RB) and spells the same three calls.
    ~MaterialInstance();

private:
    void InitFields() {
        _shaderClass = 0;                                 // +0x20
        _hasRenderTexture = 0;                            // +0x24
        _hasRenderedTexture = 0;                          // +0x25
        *reinterpret_cast<void**>(&_material) = nullptr;  // +0x28 (raw: the door runs this on raw storage)
        _parameters._data = nullptr; _parameters._size = 0; _parameters._capacity = 0;   // +0x30
        _constants._data  = nullptr; _constants._size  = 0; _constants._capacity  = 0;   // +0x40
        _textures._data   = nullptr; _textures._size   = 0; _textures._capacity   = 0;   // +0x50
    }
    void ReleaseFields();   // renderer.cpp

public:
    // ---- the 0x40 bytes MaterialInstance adds ------------------------------
    uint32_t _shaderClass;         // +0x20  a COPY of Material::_type
    uint8_t  _hasRenderTexture;    // +0x24  any bound texture with kind == 4
    uint8_t  _hasRenderedTexture;  // +0x25  any bound texture with kind == 5
    uint8_t  _pad_26[2];           // +0x26
    Object::ExternalPtr<Material> _material;                          // +0x28 BY VALUE
    Collection::Array<RtGraphics::MaterialParameterSet> _parameters;  // +0x30
    Collection::Array<RtGraphics::MaterialConstant>     _constants;   // +0x40
    Collection::Array<RtGraphics::MaterialTexture>      _textures;    // +0x50
};
static_assert(sizeof(MaterialInstance) == 0x60,
              "Graphics::MaterialInstance is 0x60 bytes (the ctor 0x180103980's "
              "last store ends at +0x5f)");
static_assert(alignof(MaterialInstance) == 8, "MaterialInstance aligns to 8 (the vptr)");
static_assert(sizeof(MaterialInstance) == sizeof(RtGraphics::MaterialInstance),
              "the real class and the POD describe the same bytes");
static_assert(offsetof(MaterialInstance, _shaderClass) == offsetof(RtGraphics::MaterialInstance, shaderClass), "alias shaderClass");
static_assert(offsetof(MaterialInstance, _hasRenderTexture) == offsetof(RtGraphics::MaterialInstance, hasRenderTexture), "alias hasRenderTexture");
static_assert(offsetof(MaterialInstance, _hasRenderedTexture) == offsetof(RtGraphics::MaterialInstance, hasRenderedTexture), "alias hasRenderedTexture");
static_assert(offsetof(MaterialInstance, _material)   == offsetof(RtGraphics::MaterialInstance, material),   "alias material");
static_assert(offsetof(MaterialInstance, _parameters) == offsetof(RtGraphics::MaterialInstance, parameters), "alias parameters");
static_assert(offsetof(MaterialInstance, _constants)  == offsetof(RtGraphics::MaterialInstance, constants),  "alias constants");
static_assert(offsetof(MaterialInstance, _textures)   == offsetof(RtGraphics::MaterialInstance, textures),   "alias textures");

static_assert(offsetof(MaterialInstance, _shaderClass)        == 0x20, "MaterialInstance::_shaderClass @ +0x20 (GetType 0x180002b60)");
static_assert(offsetof(MaterialInstance, _hasRenderTexture)   == 0x24, "MaterialInstance::_hasRenderTexture @ +0x24 (0x180002b80)");
static_assert(offsetof(MaterialInstance, _hasRenderedTexture) == 0x25, "MaterialInstance::_hasRenderedTexture @ +0x25 (0x180002b90)");
static_assert(offsetof(MaterialInstance, _material)   == 0x28, "MaterialInstance::_material @ +0x28 (RendererDrawItem holds this handle by value)");
static_assert(offsetof(MaterialInstance, _parameters) == 0x30, "MaterialInstance::_parameters @ +0x30 (Setup 0x180102c90)");
static_assert(offsetof(MaterialInstance, _constants)  == 0x40, "MaterialInstance::_constants @ +0x40 (FindConstant 0x180102f40)");
static_assert(offsetof(MaterialInstance, _textures)   == 0x50, "MaterialInstance::_textures @ +0x50 (FindTexture 0x180103020, GetTexture 0x180103100)");

// The base subobject must start at zero and the derived fields at exactly
// sizeof(Object::External) — no tail-padding reuse, no vptr duplication.
static_assert(offsetof(Material, _type) == sizeof(Object::External),
              "Material's own fields begin at the end of the Object::External base");
static_assert(offsetof(MaterialInstance, _shaderClass) == sizeof(Object::External),
              "MaterialInstance's own fields begin at the end of the base");

}  // namespace Graphics
