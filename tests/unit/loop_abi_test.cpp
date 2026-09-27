// ============================================================================
// tests/unit/loop_abi_test.cpp — ENDGAME B.3.8 block LOOP (B38_RULES.md B8-14)
//
// The ABI claims block LOOP makes for its classes, measured on the SHIPPED
// HEADERS (code/Game/Game/<Class>.h):
//
// PART A — LAYOUT. sizeof / alignof against each retail create site's MemAlloc
//   and the member offsets the ported bodies read.
// PART B — SHAPE. Every class is concrete, `final`, single inheritance from its
//   proved Runtime base, polymorphic, NO virtual destructor.
// PART C — SLOT ORDER, read off pointers to VIRTUAL members (Itanium PMF =
//   1 + vtable byte offset, adjustment 0): each override lands on the retail
//   index of B38_SLOTS.md sec. 6 / B37_RULES sec. 3.1, and slot 0 is
//   `DeletingDtor`.
//
// NOT PROVEN HERE: the bodies (they stand on the address registry and cannot
// link into a host test), and `Vtable() == vptr` (tests/unit/exe_class_abi_test.cpp
// PART C proves DEFINE_EXE_VTABLE on this toolchain; rb_engine_abi_test.cpp
// proves the loop bases' ctor/dtor stamp order through stand-in subclasses).
// ============================================================================
#include "unit.h"

#include "Game/InnerLoaderLoop.h"
#include "Game/GameLoaderLoop.h"
#include "Game/GameLoadDialogLoopT.h"
#include "Game/MenuDialogLoopT.h"
#include "Game/GameUpdateLoop.h"
#include "Game/ClimateGenerator.h"
#include "Game/Disease.h"
#include "Game/MapGenerator.h"
#include "Game/MenuScene.h"
#include "Game/MusicList.h"
#include "Game/NameDatabase.h"
#include "Game/Profession.h"
#include "Game/ProfessionList.h"
#include "Game/TerrainGenerator.h"
#include "Game/Toolbar.h"
#include "Game/Weather.h"
#include "Game/WorldGeometry.h"
#include "Game/WeatherParticleGeometry.h"
#include "Game/PathfinderThread.h"
#include "Game/Tornado.h"
#include "graphics/Material.h"
#include "ui/SpriteSheet.h"
#include "video/ImageBuffer.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <utility>

namespace {

template <typename PMF>
uintptr_t SlotOffset(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);   // not virtual
    if (words[1] != 0) return ~uintptr_t(0);           // this-adjusting: not single inheritance
    return words[0] - 1;
}

constexpr uintptr_t S(int slot) { return static_cast<uintptr_t>(slot) * 8; }

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================
UNIT_TEST(loop_sizes_match_retail) {
    CHECK_EQ(sizeof(InnerLoaderLoop), (size_t)0x48);   // GameMain MemAlloc(0x48)
    CHECK_EQ(sizeof(GameLoaderLoop),  (size_t)0x48);   // MemAlloc(0x48) at all 7 sites
    CHECK_EQ(alignof(InnerLoaderLoop), (size_t)8);
    CHECK_EQ(alignof(GameLoaderLoop),  (size_t)8);
    CHECK_EQ(sizeof(GameLoadDialogLoopT), (size_t)0xb60);   // MemAlloc(0xb60, 4) at all 7 sites
    CHECK_EQ(alignof(GameLoadDialogLoopT), (size_t)8);
    CHECK_EQ(sizeof(MenuDialogLoopT), (size_t)0xc00);        // MemAlloc(0xc00, 8)
    CHECK_EQ(alignof(MenuDialogLoopT), (size_t)8);
    CHECK_EQ(sizeof(GameUpdateLoop), (size_t)0x160);         // MemAlloc(0x160, 8)
    CHECK_EQ(alignof(GameUpdateLoop), (size_t)8);
}

UNIT_TEST(loop_member_offsets) {
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _dialogExternal), (size_t)0x50);   // base's
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _ui),             (size_t)0x60);   // base's
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _fadeColor1),     (size_t)0x188);
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _fadeColor2),     (size_t)0x18c);
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _fadeDone),       (size_t)0x190);
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _random),         (size_t)0x194);
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _tipTimer),       (size_t)0xb58);
    CHECK_EQ(offsetof(MenuDialogLoopT, _ui),                 (size_t)0x40);    // base's
    CHECK_EQ(offsetof(MenuDialogLoopT, _random),             (size_t)0x168);
    CHECK_EQ(offsetof(MenuDialogLoopT, _audioScene),         (size_t)0xb30);
    CHECK_EQ(offsetof(MenuDialogLoopT, _pendingLoop),        (size_t)0xb68);
    CHECK_EQ(offsetof(MenuDialogLoopT, _saveLoad),           (size_t)0xb70);
    CHECK_EQ(offsetof(MenuDialogLoopT, _menuScene),          (size_t)0xb90);
    CHECK_EQ(offsetof(MenuDialogLoopT, _scene),              (size_t)0xba0);
    CHECK_EQ(offsetof(MenuDialogLoopT, _sceneIdArray),       (size_t)0xbf0);
    CHECK_EQ(offsetof(GameUpdateLoop, _resourceList),        (size_t)0x08);    // base's
    CHECK_EQ(offsetof(GameUpdateLoop, _loadProgress),        (size_t)0x24);    // base's (worldgen stages)
    CHECK_EQ(offsetof(GameUpdateLoop, m_saveName),           (size_t)0x28);
    CHECK_EQ(offsetof(GameUpdateLoop, m_savedState),         (size_t)0x58);
    CHECK_EQ(offsetof(GameUpdateLoop, m_userInterface),      (size_t)0x98);
    CHECK_EQ(offsetof(GameUpdateLoop, m_nextLoop),           (size_t)0xc8);
    CHECK_EQ(offsetof(GameUpdateLoop, m_game),               (size_t)0xf8);
    CHECK_EQ(offsetof(GameUpdateLoop, m_skyMesh),            (size_t)0x108);
    CHECK_EQ(offsetof(GameUpdateLoop, m_scenarioInstance),   (size_t)0x150);
    CHECK_EQ(offsetof(GameUpdateLoop, m_scenarioIndex),      (size_t)0x15c);
}

// ===========================================================================
// PART B — SHAPE
// ===========================================================================
UNIT_TEST(loop_classes_shape) {
    CHECK((std::is_base_of<Engine::LoaderLoop, InnerLoaderLoop>::value));
    CHECK((std::is_base_of<Engine::LoaderLoop, GameLoaderLoop>::value));
    CHECK(!std::is_abstract<InnerLoaderLoop>::value);
    CHECK(!std::is_abstract<GameLoaderLoop>::value);
    CHECK(std::is_final<InnerLoaderLoop>::value);
    CHECK(std::is_final<GameLoaderLoop>::value);
    CHECK(!std::has_virtual_destructor<InnerLoaderLoop>::value);
    CHECK(!std::has_virtual_destructor<GameLoaderLoop>::value);
    CHECK((std::is_base_of<Engine::DialogLoaderLoopT, GameLoadDialogLoopT>::value));
    CHECK(!std::is_abstract<GameLoadDialogLoopT>::value);
    CHECK(std::is_final<GameLoadDialogLoopT>::value);
    CHECK(!std::has_virtual_destructor<GameLoadDialogLoopT>::value);
    CHECK((std::is_base_of<Engine::DialogLoopT, MenuDialogLoopT>::value));
    CHECK(!std::is_abstract<MenuDialogLoopT>::value);
    CHECK(std::is_final<MenuDialogLoopT>::value);
    CHECK(!std::has_virtual_destructor<MenuDialogLoopT>::value);
    CHECK((std::is_base_of<Engine::UpdateLoop, GameUpdateLoop>::value));
    CHECK(!std::is_abstract<GameUpdateLoop>::value);
    CHECK(std::is_final<GameUpdateLoop>::value);
    CHECK(!std::has_virtual_destructor<GameUpdateLoop>::value);
}

// ===========================================================================
// PART C — SLOT ORDER
// ===========================================================================
UNIT_TEST(loop_loaderloop_slots) {
    // 0x140158870: own [0], [6], [10].
    CHECK_EQ(SlotOffset(&InnerLoaderLoop::DeletingDtor),        S(0));
    CHECK_EQ(SlotOffset(&InnerLoaderLoop::Load),                S(6));
    CHECK_EQ(SlotOffset(&InnerLoaderLoop::GetResourceFilename), S(10));
    // 0x1401588d0: own [0] only.
    CHECK_EQ(SlotOffset(&GameLoaderLoop::DeletingDtor),         S(0));
    // Both tables are 12 slots: the last is AllowsModChanges, UpdateLoop's.
    CHECK_EQ(SlotOffset(&Engine::UpdateLoop::AllowsModChanges), S(11));
}

UNIT_TEST(loop_splash_slots) {
    // 0x140158800: own [0], [1], [2], [3], [12].
    CHECK_EQ(SlotOffset(&GameLoadDialogLoopT::DeletingDtor),     S(0));
    CHECK_EQ(SlotOffset(&GameLoadDialogLoopT::Startup),          S(1));
    CHECK_EQ(SlotOffset(&GameLoadDialogLoopT::Shutdown),         S(2));
    CHECK_EQ(SlotOffset(&GameLoadDialogLoopT::Update),           S(3));
    CHECK_EQ(SlotOffset(&GameLoadDialogLoopT::CreateLoopDialog), S(12));
}

UNIT_TEST(loop_menu_slots) {
    // 0x14015ed00: own [0]-[4], [6], [8]-[12]; [5] and [7] are UpdateLoop's.
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::DeletingDtor),        S(0));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::Startup),             S(1));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::Shutdown),            S(2));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::Update),              S(3));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::Draw),                S(4));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::Load),                S(6));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::OnVideoError),        S(8));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::OnVideoRestart),      S(9));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::GetResourceFilename), S(10));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::AllowsModChanges),    S(11));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::CreateLoopDialog),    S(12));
}

UNIT_TEST(loop_gameupdateloop_slots) {
    // 0x14015e2f0: own [0]-[10]; [11] AllowsModChanges is UpdateLoop's.
    CHECK_EQ(SlotOffset(&GameUpdateLoop::DeletingDtor),        S(0));
    CHECK_EQ(SlotOffset(&GameUpdateLoop::Startup),             S(1));
    CHECK_EQ(SlotOffset(&GameUpdateLoop::Shutdown),            S(2));
    CHECK_EQ(SlotOffset(&GameUpdateLoop::Update),              S(3));
    CHECK_EQ(SlotOffset(&GameUpdateLoop::Draw),                S(4));
    CHECK_EQ(SlotOffset(&GameUpdateLoop::DrawFailed),          S(5));
    CHECK_EQ(SlotOffset(&GameUpdateLoop::Load),                S(6));
    CHECK_EQ(SlotOffset(&GameUpdateLoop::SaveRestore),         S(7));
    CHECK_EQ(SlotOffset(&GameUpdateLoop::OnVideoError),        S(8));
    CHECK_EQ(SlotOffset(&GameUpdateLoop::OnVideoRestart),      S(9));
    CHECK_EQ(SlotOffset(&GameUpdateLoop::GetResourceFilename), S(10));
    CHECK_EQ(SlotOffset(&GameUpdateLoop::AllowsModChanges),    S(11));   // inherited
}

// The retslot slot 12: RCX = this, RDX = the caller's InternalPtr slot, RAX =
// the slot back (B6-2). Declared type checked; the body is not linked here.
UNIT_TEST(loop_splash_slot12_is_retslot_first) {
    using Ret = Object::InternalPtr<UI::DialogInstance>;
    CHECK((std::is_same<decltype(&GameLoadDialogLoopT::CreateLoopDialog),
                        Ret* (GameLoadDialogLoopT::*)(Ret*)>::value));
    CHECK((std::is_same<decltype(&MenuDialogLoopT::CreateLoopDialog),
                        Ret* (MenuDialogLoopT::*)(Ret*)>::value));
}

// ===========================================================================
// PART D — THE BUILDERS' CONSTRUCTION (ENDGAME B.3.9 EXA)
// Every loop builder (mainmenudialog, newgamedialog, saveloaddialog, tutorialui,
// tutorial_next_scenario, worldsetup_reload, GameMain) writes
// `::new (MemAlloc(size, align)) X(args)` with exactly the argument shapes
// below — the ctor arguments of retail's 0x140098060 / 0x14009bb00 / the inlined
// splash ctor / ??0LoaderLoop. The expression is checked unevaluated (the ctor
// bodies do not link into a host test); a builder that needed a door or a
// by-address vtable stamp again would have to break one of these.
// ===========================================================================
template <typename T, typename... A>
using PlacementNewResult = decltype(::new (std::declval<void*>()) T(std::declval<A>()...));

template <typename, typename T, typename... A>
struct CanPlacementNew : std::false_type {};
template <typename T, typename... A>
struct CanPlacementNew<std::void_t<PlacementNewResult<T, A...>>, T, A...> : std::true_type {};

UNIT_TEST(loop_builders_construct_in_place) {
    CHECK((CanPlacementNew<void, GameUpdateLoop, void*, uint64_t, int32_t,
                           const System::String&, int32_t, int32_t, bool,
                           void*, void*, void*>::value));
    CHECK((CanPlacementNew<void, MenuDialogLoopT, const System::String&>::value));
    CHECK((CanPlacementNew<void, GameLoadDialogLoopT, const System::String&,
                           Engine::UpdateLoop*, const System::Time&>::value));
    CHECK((CanPlacementNew<void, GameLoaderLoop, Engine::UpdateLoop*,
                           const System::Time&>::value));
    // The builders chain the loops through Engine::UpdateLoop*: single
    // inheritance, so the base pointer IS the object's address (no adjustment).
    alignas(8) unsigned char raw[16] = {};
    auto* g = reinterpret_cast<GameUpdateLoop*>(raw);
    CHECK_EQ(reinterpret_cast<uintptr_t>(static_cast<Engine::UpdateLoop*>(g)),
             reinterpret_cast<uintptr_t>(raw));
    auto* d = reinterpret_cast<GameLoadDialogLoopT*>(raw);
    CHECK_EQ(reinterpret_cast<uintptr_t>(static_cast<Engine::UpdateLoop*>(d)),
             reinterpret_cast<uintptr_t>(raw));
    auto* m = reinterpret_cast<MenuDialogLoopT*>(raw);
    CHECK_EQ(reinterpret_cast<uintptr_t>(static_cast<Engine::UpdateLoop*>(m)),
             reinterpret_cast<uintptr_t>(raw));
}

// The exe's three local table copies (0x14015c400 MaterialInstance, 0x14015c3b0
// SpriteSheet, 0x14015c8c0 ImageBuffer) are the Runtime classes' own tables
// (Ghidra: slot 0 the exe scalar deleting dtor, 1..n thunks onto the class's
// own members), so the thumbnail / minimap builders write `::new (p) X()` at
// retail's MemAlloc size. ENDGAME B.3.9 EXA.
UNIT_TEST(exe_table_copies_are_the_runtime_classes) {
    CHECK((CanPlacementNew<void, Graphics::MaterialInstance>::value));
    CHECK((CanPlacementNew<void, UI::SpriteSheet>::value));
    CHECK((CanPlacementNew<void, Video::ImageBuffer>::value));
    CHECK_EQ(sizeof(Graphics::MaterialInstance), (size_t)0x60);   // MemAlloc(0x60, 8)
    CHECK_EQ(sizeof(UI::SpriteSheet), (size_t)0x58);              // MemAlloc(0x58, 8)
    CHECK_EQ(sizeof(Video::ImageBuffer), (size_t)0x68);           // MemAlloc(0x68, 8)
    CHECK(!std::is_abstract<Graphics::MaterialInstance>::value);
    CHECK(!std::is_abstract<UI::SpriteSheet>::value);
    CHECK(!std::is_abstract<Video::ImageBuffer>::value);
}

// ===========================================================================
// THE ELEVEN REFLECTED WORLD EXTERNALS (Object::External, 9 retail slots)
// ===========================================================================
UNIT_TEST(loop_externals_sizes_match_retail) {
    CHECK_EQ(sizeof(ClimateGenerator), (size_t)0x110);
    CHECK_EQ(sizeof(Disease),          (size_t)0x48);
    CHECK_EQ(sizeof(MapGenerator),     (size_t)0x178);
    CHECK_EQ(sizeof(MenuScene),        (size_t)0x208);
    CHECK_EQ(sizeof(MusicList),        (size_t)0x30);
    CHECK_EQ(sizeof(NameDatabase),     (size_t)0x60);
    CHECK_EQ(sizeof(Profession),       (size_t)0x60);
    CHECK_EQ(sizeof(ProfessionList),   (size_t)0x50);
    CHECK_EQ(sizeof(TerrainGenerator), (size_t)0x90);
    CHECK_EQ(sizeof(Toolbar),          (size_t)0x68);
    CHECK_EQ(sizeof(Weather),          (size_t)0x80);
    CHECK_EQ(offsetof(MapGenerator, descRef3),     (size_t)0x158);
    CHECK_EQ(offsetof(MenuScene, transform),       (size_t)0x88);
    CHECK_EQ(offsetof(MenuScene, _field1E8),       (size_t)0x1e8);
    CHECK_EQ(offsetof(TerrainGenerator, _field80), (size_t)0x80);
    CHECK_EQ(offsetof(Weather, sunPitch),          (size_t)0x7c);
}

template <typename T>
void CheckExternalShape() {
    CHECK((std::is_base_of<Object::External, T>::value));
    CHECK(!std::is_abstract<T>::value);
    CHECK(std::is_final<T>::value);
    CHECK(!std::has_virtual_destructor<T>::value);
    CHECK_EQ(alignof(T), (size_t)8);
    CHECK_EQ(SlotOffset(&T::DeletingDtor),   S(0));
    CHECK_EQ(SlotOffset(&T::TypeName),       S(1));
    CHECK_EQ(SlotOffset(&T::Type),           S(2));
    CHECK_EQ(SlotOffset(&T::GetFileVersion), S(3));
    CHECK_EQ(SlotOffset(&T::Compile),        S(4));
    CHECK_EQ(SlotOffset(&T::Serialize),      S(5));
    CHECK_EQ(SlotOffset(&T::Reconstruct),    S(6));
    CHECK_EQ(SlotOffset(&T::Destruct),       S(7));
    CHECK_EQ(SlotOffset(&T::Construct),      S(8));   // the class's own new virtual
}

UNIT_TEST(loop_externals_shape_and_slots) {
    CheckExternalShape<ClimateGenerator>();
    CheckExternalShape<Disease>();
    CheckExternalShape<MapGenerator>();
    CheckExternalShape<MenuScene>();
    CheckExternalShape<MusicList>();
    CheckExternalShape<NameDatabase>();
    CheckExternalShape<Profession>();
    CheckExternalShape<ProfessionList>();
    CheckExternalShape<TerrainGenerator>();
    CheckExternalShape<Toolbar>();
    CheckExternalShape<Weather>();
}

// ===========================================================================
// THE UNREFLECTED EXE Graphics::Geometry SUBCLASSES (0x14015f110, 0x140160240)
// ===========================================================================
template <typename T>
void CheckGeometryShape() {
    CHECK((std::is_base_of<Graphics::Geometry, T>::value));
    CHECK(!std::is_abstract<T>::value);
    CHECK(std::is_final<T>::value);
    CHECK(!std::has_virtual_destructor<T>::value);
    CHECK_EQ(alignof(T), (size_t)8);
    CHECK_EQ(SlotOffset(&T::DeletingDtor), S(0));
    CHECK_EQ(SlotOffset(&T::Update),       S(1));
    CHECK_EQ(SlotOffset(&T::Draw),         S(2));
    CHECK_EQ(SlotOffset(&T::AddDrawItems), S(3));
    CHECK_EQ(SlotOffset(&Graphics::Geometry::Clean), S(8));   // the tables stop at 9
}

UNIT_TEST(loop_geometry_subclasses) {
    CHECK_EQ(sizeof(WorldGeometry), (size_t)0x230);            // MemAlloc(0x230)
    CHECK_EQ(sizeof(WeatherParticleGeometry), (size_t)0xf0);   // MemAlloc(0xf0)
    CHECK_EQ(offsetof(WorldGeometry, _heightImage), (size_t)0x148);
    CHECK_EQ(offsetof(WorldGeometry, _arrays), (size_t)0x1a0);
    CHECK_EQ(offsetof(WeatherParticleGeometry, _particles), (size_t)0xe0);
    CheckGeometryShape<WorldGeometry>();
    CheckGeometryShape<WeatherParticleGeometry>();
}

// THE PATHFINDER System::Thread SUBCLASS (0x14015ef18, 2 slots)
UNIT_TEST(loop_pathfinder_thread) {
    CHECK_EQ(sizeof(PathfinderThread), (size_t)0x1f8);   // MemAlloc(0x1f8)
    CHECK((std::is_base_of<System::Thread, PathfinderThread>::value));
    CHECK(!std::is_abstract<PathfinderThread>::value);
    CHECK(std::is_final<PathfinderThread>::value);
    CHECK(!std::has_virtual_destructor<PathfinderThread>::value);
    CHECK_EQ(SlotOffset(&PathfinderThread::Run),         S(0));
    CHECK_EQ(SlotOffset(&PathfinderThread::OnCompleted), S(1));   // inherited
}

// TORNADO (reflected Graphics::Particle, 10 slots) AND ITS INSTANCE (10 slots)
UNIT_TEST(loop_tornado_classes) {
    CHECK_EQ(sizeof(Tornado), (size_t)0x90);            // MemAlloc(0x90)
    CHECK_EQ(sizeof(TornadoInstance), (size_t)0xe0);    // MemAlloc(0xe0)
    CHECK((std::is_base_of<Graphics::Particle, Tornado>::value));
    CHECK((std::is_base_of<Graphics::ParticleInstance, TornadoInstance>::value));
    CHECK(!std::is_abstract<Tornado>::value);
    CHECK(!std::is_abstract<TornadoInstance>::value);
    CHECK(std::is_final<Tornado>::value);
    CHECK(std::is_final<TornadoInstance>::value);
    CHECK(!std::has_virtual_destructor<Tornado>::value);
    CHECK(!std::has_virtual_destructor<TornadoInstance>::value);
    CHECK_EQ(SlotOffset(&Tornado::DeletingDtor),   S(0));
    CHECK_EQ(SlotOffset(&Tornado::TypeName),       S(1));
    CHECK_EQ(SlotOffset(&Tornado::Type),           S(2));
    CHECK_EQ(SlotOffset(&Tornado::GetFileVersion), S(3));
    CHECK_EQ(SlotOffset(&Tornado::Compile),        S(4));
    CHECK_EQ(SlotOffset(&Tornado::Serialize),      S(5));
    CHECK_EQ(SlotOffset(&Tornado::Reconstruct),    S(6));
    CHECK_EQ(SlotOffset(&Tornado::Destruct),       S(7));
    CHECK_EQ(SlotOffset(&Tornado::CreateSystem),   S(8));
    CHECK_EQ(SlotOffset(&Tornado::Construct),      S(9));
    CHECK_EQ(SlotOffset(&TornadoInstance::DeletingDtor), S(0));
    void (TornadoInstance::*upd)(const System::Time&) = &TornadoInstance::Update;
    CHECK_EQ(SlotOffset(upd), S(1));
    CHECK_EQ(SlotOffset(&Graphics::ParticleInstance::SetParticle), S(9));
    CHECK_EQ(offsetof(Tornado, _particleFadeOut), (size_t)0x88);
    CHECK_EQ(offsetof(TornadoInstance, _funnelSpinDegrees), (size_t)0xd8);
}

}  // namespace
