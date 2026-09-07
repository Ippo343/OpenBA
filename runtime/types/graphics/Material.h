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
