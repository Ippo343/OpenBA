// ============================================================================
// tests/unit/link_stubs.cpp — the one symbol the classes under test reference
// but do not own (ENDGAME B.3.1 step 1)
// ============================================================================
// `System::Filename::GetPathExtension` forwards to the C body that still lives
// in src/engine/io/external_path.cpp, wrapped in that lane's path
// counters. Linking that file into a host test would drag in ExternalFactory,
// the package system and Win32; the member is therefore NOT covered by
// filename_test.cpp, and this stub only satisfies the linker.
//
// It goes away with the forward, when Object::ExternalFactory is converted and
// the body moves onto the class.
// ============================================================================
#include "system/Filename.h"
#include "io/Modification.h"
#include "save/SaveLoad.h"
#include "graphics/Scene.h"
#include "graphics/Animation.h"
#include "graphics/GraphicsMesh.h"   // B.4 Y RT: kSpec_MeshNode
#include "graphics/AnimationState.h" // B.4 Z: kSpec_AnimLayer

#include <cstdio>
#include <cstdlib>

namespace RtObject {
System::Filename* RtSystem_Filename_GetPathExtension(const System::Filename* self,
                                                     System::Filename* ret) {
    (void)self;
    ret->_data = nullptr;
    return ret;
}
}  // namespace RtObject

// ============================================================================
// B3R RT-Core: the element-op bodies src/engine/collection/array.cpp and
// array_destroy.cpp now call BY NAME, whose translation units (io/, save/,
// graphics/) a host test does not link. Nothing in the unit build drives the six
// Array specs that reach them (Array<Array<uint8>>, Array<Save::MetaData>,
// Array<Graphics::DrawGroup>, Array<Graphics::Animation>); before B3R the same
// calls went through the stub ResolveAddr, which answered null. Each stub
// aborts loudly rather than faking a body, so a future test that does drive
// one of those specs has to link the real translation unit.
// ============================================================================
namespace {
[[noreturn]] void UnlinkedBody(const char* what) {
    std::fprintf(stderr, "[unit] %s is not linked into the unit build; see link_stubs.cpp.\n", what);
    std::abort();
}
}  // namespace

namespace RtIO {
void* RtColl_ArrayU8Assign_180011220(Collection::Array<uint8_t>*, const Collection::Array<uint8_t>*) { UnlinkedBody("RtColl_ArrayU8Assign_180011220"); }
}  // namespace RtIO
extern "C" void* RtGraphics_DrawGroup_CopyCtor(void*, const void*) { UnlinkedBody("RtGraphics_DrawGroup_CopyCtor"); }
extern "C" void* RtGraphics_Animation_CopyCtor(void*, const void*) { UnlinkedBody("RtGraphics_Animation_CopyCtor"); }
extern "C" void  RtGraphics_Animation_Dtor(void*) { UnlinkedBody("RtGraphics_Animation_Dtor"); }
Save::SaveLoad::SaveInfo::~SaveInfo() { UnlinkedBody("Save::SaveLoad::SaveInfo::~SaveInfo"); }
// B.4 S4 GFX: kSpec_MetaData58's element op names the copy ctor directly now
// (the RtSave_MetaData_CopyCtor door is gone); the ctor's member set names
// ModInfo's copy ctor and dtor in turn.
Save::SaveLoad::SaveInfo::SaveInfo(const SaveInfo&) { UnlinkedBody("Save::SaveLoad::SaveInfo::SaveInfo(const SaveInfo&)"); }
Save::SaveLoad::ModInfo::ModInfo(const ModInfo&) { UnlinkedBody("Save::SaveLoad::ModInfo::ModInfo(const ModInfo&)"); }
Save::SaveLoad::ModInfo::~ModInfo() { UnlinkedBody("Save::SaveLoad::ModInfo::~ModInfo"); }

// B.4 Y RT: `Graphics::GraphicsMesh::_nodes` is `NodeArray`, whose ~Array names
// the hand-derived row `rtcoll::gfxanim::kSpec_MeshNode`
// (src/engine/collection/array_graphics_anim.cpp, not linked into the unit build:
// it pulls in the graphics element copy ctors). The GraphicsMesh probe's inline
// ctor references it from its cleanup path only; no test sizes or destroys a
// node array, so the row's operations abort if one ever does.
namespace {
void UnlinkedNodeAssign(uint8_t*, const uint8_t*) { UnlinkedBody("kSpec_MeshNode assign"); }
void UnlinkedNodeInit(uint8_t*) { UnlinkedBody("kSpec_MeshNode init"); }
void UnlinkedNodeDestroy(void*, uint8_t*, int) { UnlinkedBody("kSpec_MeshNode destroy"); }
}  // namespace
namespace rtcoll { namespace gfxanim {
const ArraySpec kSpec_MeshNode = {
    "Array<GraphicsMesh::Node> (unit stub)", 0x48, 8,
    &UnlinkedNodeAssign, &UnlinkedNodeInit, &UnlinkedNodeDestroy, false,
    0, 0, 0, 0, 0, 0 };
} }  // namespace rtcoll::gfxanim

// B.4 Z MIRROR2 GFX: `Graphics::AnimationState::_layers` (MeshInstance::_animation)
// is on the hand-derived row `kSpec_AnimLayer`, same file, same reason. Its
// destroy step is null in the real row too (J1 only), so a MeshInstance probe's
// ~Array frees exactly as the real build does; assign / init abort.
namespace {
void UnlinkedLayerAssign(uint8_t*, const uint8_t*) { UnlinkedBody("kSpec_AnimLayer assign"); }
void UnlinkedLayerInit(uint8_t*) { UnlinkedBody("kSpec_AnimLayer init"); }
}  // namespace
namespace rtcoll { namespace gfxanim {
const ArraySpec kSpec_AnimLayer = {
    "Array<Graphics::AnimationState::Layer> (unit stub)", 0x38, 8,
    &UnlinkedLayerAssign, &UnlinkedLayerInit, nullptr, true,
    0, 0, 0, 0, 0, 0 };
// `MeshInstance::_nodes` / `InstancedAnimationState::_nodes`: kSpec_InterpTransform,
// likewise (real row: stride 0x38, alignment 4, no destroy step).
const ArraySpec kSpec_InterpTransform = {
    "Array<Graphics::InterpolatedState<Math::Transform>> (unit stub)", 0x38, 4,
    &UnlinkedLayerAssign, &UnlinkedLayerInit, nullptr, true,
    0, 0, 0, 0, 0, 0 };
} }  // namespace rtcoll::gfxanim

// ============================================================================
// ENDGAME B7 (CTOR): `System::Random::Random()` (0x1800c7f20) is a real ctor
// now, so every by-value `Object::InternalFactory` a host test builds (the
// FactoryScope fixtures) runs it. Its body is in src/engine/system/
// value_types.cpp, which the unit build does not link (sources.cmake). The tests
// never read the Random's state (Rand is `g_unitInternalRandHook`), so the
// host body leaves the bytes as the implicit ctor did before B7: it does
// nothing. NOT an UnlinkedBody abort, because it IS driven.
// ============================================================================
#include "system/Value.h"
System::Random::Random() {}

// ============================================================================
// Block EROSION (tests/unit/worldgen_erosion_test.cpp): the erosion stage draws its
// droplet starts from `System::Random::Rand(uint32_t)` and the test seeds the
// heightfield's Random with `Random(uint32_t)`; both bodies live in value_types.cpp,
// which the unit does not link (it would bring System::Timer, and its Random() would
// collide with the one above). The host stand-ins below are a deterministic xorshift
// over the same state words, NOT retail's MT19937: the test compares two erosion
// bodies under one generator, and the state after a run depends on the order and the
// count of the draws, which is what it checks. Rand(0) keeps retail's no-draw arm.
// `TerrainHeightfield::paths`' row kSpec_WgPathVec (worldgen_f390_coastline.cpp, not
// linked) is referenced by the record's Array member; the test never sizes it.
// ============================================================================
System::Random::Random(uint32_t seed) {
    _index = 0;
    for (int i = 0; i < 624; ++i) _mt[i] = seed + 0x9e3779b9u * static_cast<uint32_t>(i);
}
uint32_t System::Random::Rand(uint32_t bound) {
    if (bound == 0) return 0u;
    uint32_t x = _mt[_index] | 1u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    _mt[_index] = x;
    _index = (_index + 1u) % 624u;
    return x % bound;
}

#include "Game/TerrainHeightfield.h"
namespace {
void UnlinkedWgPathAssign(uint8_t*, const uint8_t*) { UnlinkedBody("kSpec_WgPathVec assign"); }
void UnlinkedWgPathInit(uint8_t*) { UnlinkedBody("kSpec_WgPathVec init"); }
void UnlinkedWgPathDestroy(void*, uint8_t*, int) { UnlinkedBody("kSpec_WgPathVec destroy"); }
}  // namespace
const rtcoll::ArraySpec kSpec_WgPathVec = {
    "Array<Array<PathPoint>> (unit stub)", 0x10, 8,
    &UnlinkedWgPathAssign, &UnlinkedWgPathInit, &UnlinkedWgPathDestroy, false,
    0, 0, 0, 0, 0, 0 };
