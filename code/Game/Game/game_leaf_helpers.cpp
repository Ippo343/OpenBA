// code/Game/Game/game_leaf_helpers.cpp
// Closure-safe leaf reimplementations for the Game module (Wave: Game gap).
//
// These are small, fully closure-safe exe helpers referenced (via guarded
// ResolveAddr(Module::Exe, ...)) by code/Game/Game/{mapgenerator,menuscene,
// gameupdateloop}.cpp. In standalone the exe is replaced, so each address must
// resolve to a registered reimpl or the call silently no-ops (leaving arrays
// unfreed / objects un-destroyed). Each body below is a faithful 1:1 port of the
// original (decompiled from Application-x64.exe) and depends only on already-
// resolvable primitives: System::String::~String (Runtime export, fn_String_Dtor),
// the static MemFree (fn_MemFreeStatic), already-registered tree-free 0x14000e320,
// and Scene::RemoveGlobalObject (Runtime export).
//
// ALL of these run on the shutdown / dtor / teardown path, which the autostart
// A/B path never reaches — so registering them is purely additive (no world-build
// regression), it just removes a latent silent-skip.
//
// Registered by standalone/reg_game_leaf.cpp.

// ENDGAME B.3.5 Video/Graphics caller batch: Graphics::Scene is a real class.
// Ahead of "resolve.h" on purpose — resolve.h pulls <windows.h>, and
// <wingdi.h>'s global `Rectangle` function hides `Volume::Rectangle`'s class
// name inside Volume.h's own static_asserts (Geometry.h includes Volume.h).
#include "graphics/Scene.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "graphics/Material.h"   // ENDGAME B.3.9 TI-REST: Graphics::MaterialInstance::TypeId()
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "system/Filename.h"
#include "system/String.h"
#include "collection/Collection.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "video/ImageBuffer.h"   // B.3.x W0: Game_ImageCaptureDtor deletes by name
#include <cstdint>
#include <initializer_list>
#include <new>
#include "Game/exe_bodies.h"
#include "Game/MapGenerator.h"        // ExternalRef / InitialStructure, the element rows (B4 P2 GAME-1)
#include "Game/Minimap.h"             // Game_ImageCaptureDtor's class (B.4 GAME-a)
#include "Game/WeatherSimulation.h"   // Game_WeatherDtor's class (B.4 GAME-a)
#include "Work/exe_bodies.h"   // WorkBase_TreeNodeFree_e320

// ------------------------------------------------------------------
// String-array destructors. Each is `{ void* data; int count; int cap; }`
// header (data @+0, count @+8) holding `count` elements at the given stride,
// with a System::String embedded at `strOffset` within each element. The
// original frees the data block unconditionally even when count<=0.
//
// ENDGAME B.3.2 step 2: these three WERE one hand-written copy of
// `Collection::Array<T>::~Array` (destroy each element, then
// `rtcoll::ArrayDtorImpl`). Re-decompiled 0x14001b030 / 0x14001b0a0 /
// 0x14000a8a0 on 2026-09-13 and compared with `~Array` + ArrayDtorImpl:
//   * `if (0 < count) { ~String(elem + strOffset) for each }` — retail tests
//     ONLY the count, exactly `if (destroy && _size > 0)`. The previous port's
//     extra `&& base` was a guard retail does not have; it is gone.
//   * `MemFree(data)` unconditionally (null test inside 0x1800c7ad0), then
//     `data = 0` and ONE qword store over size+capacity — ArrayDtorImpl's
//     three stores, byte for byte.
// The element rows are the ones the matching grow bodies use:
//   0x18: FUN_14001b340, {String @0, int32 generation @8, External* @0x10},
//         assign = String copy-ctor + dword + qword (+0x0c..+0x10 unwritten),
//         init = {0, -1, 0}. The SAME row exists as kSpec_GoalRefEntry in
//         code/Game/Goal/goal_exe_helpers.cpp (file-local there); the two
//         collapse into one when ExternalPtr becomes a real class.
//   0x30: FUN_14001b4e0, the 0x18 triple followed by int32 _offsetX @0x18,
//         _offsetY @0x1c, _width @0x20, _height @0x24, _rotation @0x28;
//         assign copies each field (+0x0c..+0x10 and +0x2c..+0x30 unwritten),
//         init = {0, -1, 0, 0, 0, -1, -1, 0} (mapgenerator.cpp MG_InitStructure).
//   0x28: the event-log entry; its row lives in eventlog.cpp next to the grow
//         (0x140019b70) and is reached through EventLog_EventArray_Destroy.
// ------------------------------------------------------------------
namespace {

// The element is MapGenerator::ExternalRef (Game/MapGenerator.h), reached
// through Collection.h's element adapters (B4 P2 GAME-1).
using ExternalRef = MapGenerator::ExternalRef;
using InitialStructure = MapGenerator::InitialStructure;

void CopyExternalRef(ExternalRef* d, const ExternalRef* s) {
    new (&d->strPtr) System::String(*reinterpret_cast<const System::String*>(&s->strPtr));   // +0x00  // LEFT: MapGenerator.h (GAME-3): ExternalRef::strPtr is raw String storage (H5)
    d->factoryGen = s->factoryGen;   // +0x08
    d->resolved = s->resolved;       // +0x10
}
// Generation seed is -1, not 0: 0 is a live ExternalFactory generation and
// would make the lazy resolver take its "cache valid" early-out on a null.
void ZeroExternalRef(ExternalRef* d) {
    d->strPtr = nullptr;             // +0x00 qword 0
    d->factoryGen = -1;              // +0x08
    d->resolved = nullptr;           // +0x10 qword 0
}
void DestroyExternalRefs(ExternalRef* elems, int count) {
    for (int i = 0; i < count; ++i)
        reinterpret_cast<System::String*>(&elems[i].strPtr)->~String();   // LEFT: MapGenerator.h (GAME-3): ExternalRef::strPtr is raw String storage (H5)
}
void AssignRefTriple18(uint8_t* d, const uint8_t* s) {
    rtcoll::AssignAs<ExternalRef, &CopyExternalRef>(d, s);
}
void InitRefTriple18(uint8_t* d) {
    rtcoll::InitAs<ExternalRef, &ZeroExternalRef>(d);
}
void DestroyRefTriple18(void* ignoredThis, uint8_t* base, int count) {
    rtcoll::DestroyAs<ExternalRef, &DestroyExternalRefs>(ignoredThis, base, count);
}
// stays explicit: a String copy-ctor `assign` (byteCopy=false) with a real
// `destroy`, a -1 generation seed in `init`, and an unwritten extent at
// [0x0c,0x10) — none of which a C++ element type's traits can state.
const rtcoll::ArraySpec kSpec_RefTriple18 = {           // exe 0x14001b340 / 0x14001b030
    "Array<{String,int32,External*}>", 0x18, 8,
    &AssignRefTriple18, &InitRefTriple18, &DestroyRefTriple18, false,
    /*assign unwritten*/ 0x0c, 0x10,
    /*init   unwritten*/ 0x0c, 0x10,
    /*seed zero       */ 0,    0 };
using RefTriple18Array = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                           Collection::ExplicitSpec<&kSpec_RefTriple18>>;

void CopyInitialStructure(InitialStructure* d, const InitialStructure* s) {
    new (&d->structure.strPtr) System::String(*reinterpret_cast<const System::String*>(&s->structure.strPtr));   // +0x00  // LEFT: MapGenerator.h (GAME-3): ExternalRef::strPtr is raw String storage (H5)
    d->structure.factoryGen = s->structure.factoryGen;   // +0x08
    d->structure.resolved = s->structure.resolved;       // +0x10
    d->offsetX  = s->offsetX;    // +0x18
    d->offsetY  = s->offsetY;    // +0x1c
    d->width    = s->width;      // +0x20
    d->height   = s->height;     // +0x24
    d->rotation = s->rotation;   // +0x28
}
void ZeroInitialStructure(InitialStructure* d) {
    d->structure.strPtr = nullptr;      // +0x00 qword 0
    d->structure.factoryGen = -1;       // +0x08 generation -1
    d->structure.resolved = nullptr;    // +0x10 qword 0
    d->offsetX = 0;                     // _offsetX/_offsetY (one qword 0 before; H7)
    d->offsetY = 0;
    d->width = -1;                      // _width/_height -1 (one qword before; H7)
    d->height = -1;
    d->rotation = 0;                    // _rotation
}
void DestroyInitialStructures(InitialStructure* elems, int count) {
    for (int i = 0; i < count; ++i)
        reinterpret_cast<System::String*>(&elems[i].structure.strPtr)->~String();   // LEFT: MapGenerator.h (GAME-3): ExternalRef::strPtr is raw String storage (H5)
}
void AssignInitialStructure30(uint8_t* d, const uint8_t* s) {
    rtcoll::AssignAs<InitialStructure, &CopyInitialStructure>(d, s);
}
void InitInitialStructure30(uint8_t* d) {
    rtcoll::InitAs<InitialStructure, &ZeroInitialStructure>(d);
}
void DestroyInitialStructure30(void* ignoredThis, uint8_t* base, int count) {
    rtcoll::DestroyAs<InitialStructure, &DestroyInitialStructures>(ignoredThis, base, count);
}
// stays explicit: same three reasons as the 0x18 row above, plus a -1 seed for
// _width/_height at +0x20 and a second unwritten extent at [0x2c,0x30).
const rtcoll::ArraySpec kSpec_InitialStructure30 = {    // exe 0x14001b4e0 / 0x14001b0a0
    "Array<MapGenerator::InitialStructure>", 0x30, 8,
    &AssignInitialStructure30, &InitInitialStructure30, &DestroyInitialStructure30, false,
    /*assign unwritten*/ 0x0c, 0x10,   // and +0x2c..+0x30 (one range field only)
    /*init   unwritten*/ 0x0c, 0x10,   // same
    /*seed zero       */ 0,    0 };
using InitialStructure30Array = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                                  Collection::ExplicitSpec<&kSpec_InitialStructure30>>;

}  // namespace

extern "C" void EventLog_EventArray_Destroy(void* arrBase);   // eventlog.cpp

// FUN_14001b030 (mapgenerator.cpp) — String at element+0, stride 0x18.
void Game_DestroyStringArray18(void* arrayHeader) {
    reinterpret_cast<RefTriple18Array*>(arrayHeader)->~Array();          // LEFT: SIG Game_DestroyStringArray18 (callers mapgenerator.cpp, buildinggoal.cpp hold DynArrayHeader<ExternalRef>, not a Collection::Array)
}

// FUN_14001b0a0 (mapgenerator.cpp) — String at element+0, stride 0x30.
void Game_DestroyStringArray30(void* arrayHeader) {
    reinterpret_cast<InitialStructure30Array*>(arrayHeader)->~Array();   // LEFT: SIG Game_DestroyStringArray30 (caller mapgenerator.cpp declares it locally)
}

// FUN_14000a8a0 (gameupdateloop.cpp, EventNotifier_Cleanup) — String at element+8,
// stride 0x28.
void Game_EventNotifierCleanup(void* arrayHeader) { EventLog_EventArray_Destroy(arrayHeader); }

// ------------------------------------------------------------------
// FUN_140018220 (gameupdateloop.cpp, MusicPlayer_Dtor) — frees 5 buffers at
// +0x50,+0x40,+0x30,+0x20,+0x10 (high to low) and zeroes each {ptr,len} pair.
//
// ENDGAME B.3.2 step 2: each of the five is `Collection::Array<T>::~Array` for a
// trivially-destructible T (re-decompiled 2026-09-13: MemFree(data); data = 0;
// one qword over size+capacity = 0 — ArrayDtorImpl exactly, no element step).
// The element types are not pinned in the tree, and `~Array` of a trivial
// element never reads the stride, so the header alias `rtcoll::ArrayHdr`
// (= Array<uint8_t>, the same form runtime/src/collection/array.cpp's
// ArrayDtorSited uses) is the honest spelling until MusicPlayer is a class.
// ------------------------------------------------------------------
void Game_MusicPlayerDtor(void* self) {
    uint8_t* b = reinterpret_cast<uint8_t*>(self);   // LEFT: SIG Game_MusicPlayerDtor (caller gameupdateloop.cpp passes m_characterGrid; the record is CharacterGrid, whose five arrays Component/CharacterGrid.h declares as raw triples, H5)
    static const uint64_t kOffsets[5] = { 0x50, 0x40, 0x30, 0x20, 0x10 };
    for (uint64_t off : kOffsets)
        reinterpret_cast<rtcoll::ArrayHdr*>(b + off)->~Array();   // LEFT: Component/CharacterGrid.h: the arrays are raw triples (H5)
}

// ------------------------------------------------------------------
// FUN_1400a4fc0 (gameupdateloop.cpp, ImageCapture_Dtor) — virtual scalar-dtor
// of the two sub-objects at +8/+0x10, then frees the two buffers at +0x28/+0x18.
// ------------------------------------------------------------------
void Game_ImageCaptureDtor(Minimap* self) {
    // Retail is (**(code **)*puVar1)(puVar1, 1): deref the object to get its
    // vtable, deref THAT to get slot 0, then call. This port had only one deref
    // and so called the vtable's own address as if it were code — EXEC of
    // g_NoiseUI_ImageBufferVtable_Standalone when GameUpdateLoop::Shutdown
    // finally reached the minimap ImageBuffers.
    for (Video::ImageBuffer* obj : {self->terrainImage, self->overlayImage}) {   // +0x08, +0x10
        if (!obj) continue;
        delete obj;   // slot 0, free
    }
    // ENDGAME B.3.2 step 2: two trivial-element `~Array`s (headers at +0x28 and
    // +0x18, in that order — re-decompiled 2026-09-13: MemFree, data = 0, one
    // qword over size+capacity).
    // LEFT: the pixel arrays are declared as the Array's three fields (H5).
    reinterpret_cast<rtcoll::ArrayHdr*>(&self->overlayPixels)->~Array();   // LEFT: Minimap.h H5 (gameupdateloop_load.cpp asserts Minimap trivially constructible)
    reinterpret_cast<rtcoll::ArrayHdr*>(&self->terrainPixels)->~Array();   // LEFT: Minimap.h H5 (gameupdateloop_load.cpp asserts Minimap trivially constructible)
}

// ------------------------------------------------------------------
// FUN_14001b860 (menuscene.cpp) — tear down a small tree at +0x10 (root's two
// children freed via the registered recursive tree-free 0x14000e320), then free
// the backing array at +0. Mirrors FUN_14000a8a0's sibling layout but with a
// tree node rather than a String element.
// ------------------------------------------------------------------
//
// B4 P2 GAME-1: the record is {Array @+0, tree root @+0x10, int32 count @+0x18}
// (retail 0x14001b860: MemFree(*p), p[0] = p[1] = 0; root at p[2], a dword 0 at
// p[3]); MenuScene holds two of them (+0x1C8, +0x1E8). The node's only fields
// this body reads are its two children at +0x18 / +0x20 (the shared tree free
// 0x14000e320 walks the same two); its key and size are not read here.
namespace {
struct MenuSceneTreeNode {
    uint8_t            _key[0x18];   // +0x00 not read by 0x14001b860
    MenuSceneTreeNode* left;         // +0x18
    MenuSceneTreeNode* right;        // +0x20
};
static_assert(offsetof(MenuSceneTreeNode, left)  == 0x18, "MenuSceneTreeNode::left @ +0x18 (0x14001b860)");
static_assert(offsetof(MenuSceneTreeNode, right) == 0x20, "MenuSceneTreeNode::right @ +0x20 (0x14001b860)");
struct MenuSceneTreeMap {
    rtcoll::ArrayHdr   array;        // +0x00 trivial-element Array
    MenuSceneTreeNode* root;         // +0x10
    int32_t            count;        // +0x18
    uint8_t            _pad1C[4];    // +0x1C
};
static_assert(sizeof(MenuSceneTreeMap) == 0x20, "MenuScene +0x1C8 / +0x1E8 records are 0x20 apart");
static_assert(offsetof(MenuSceneTreeMap, root)  == 0x10, "MenuSceneTreeMap::root @ +0x10 (0x14001b860)");
static_assert(offsetof(MenuSceneTreeMap, count) == 0x18, "MenuSceneTreeMap::count @ +0x18 (0x14001b860)");
}  // namespace

void Game_MenuSceneArrayDtor(void* self) {
    MenuSceneTreeMap* map = reinterpret_cast<MenuSceneTreeMap*>(self);   // LEFT: SIG Game_MenuSceneArrayDtor (caller menuscene.cpp declares it locally with void*)
    MenuSceneTreeNode* node = map->root;  // *(self + 0x10)
    if (node) {
        WorkBase_TreeNodeFree_e320(&map->root, node->left);   // 0x14000e320
        WorkBase_TreeNodeFree_e320(&map->root, node->right);
        System::MemoryAllocator::MemFree(node);
    }
    map->root = nullptr;
    map->count = 0;
    // ENDGAME B.3.2 step 2: the backing array at +0 is a trivial-element
    // `~Array` (re-decompiled 2026-09-13: MemFree(*p); p[0] = 0; p[1] = 0).
    // The tree at +0x10 above is a {root, count} pair, not an Array; untouched.
    map->array.~Array();
}

// ------------------------------------------------------------------
// FUN_140093eb0 (gameupdateloop.cpp, Weather_Dtor) — remove each geometry in the
// array at +0x50 (count +0x58) from the global scene and virtual-dtor it, then
// free the two arrays at +0x60 / +0x50. Scene = (*g_pGameWorldSingleton)+0x60.
// ------------------------------------------------------------------
void Game_WeatherDtor(WeatherSimulation* self) {
    int count = self->effectCount;                 // +0x58
    Graphics::Geometry** arr = self->effects;      // +0x50
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    Graphics::Scene* scene = gw ? gw->m_graphicsScene : nullptr;
    if (arr && count > 0) {
        for (int i = 0; i < count; ++i) {
            Graphics::Geometry* geom = arr[i];
            if (geom) {
                if (scene)
                    scene->RemoveGlobalObject(geom);   // B.3.5 bridge
                // Retail: (**(code **)*puVar2)(puVar2, 1) — object -> vtable ->
                // slot 0. Same one-deref-short bug as Game_ImageCaptureDtor above.
                delete geom;   // slot 0, free
            }
        }
    }
    // ENDGAME B.3.2 step 2: the loop above is Weather's own per-element work
    // (retail 0x140093eb0 runs it inline, before the array frees); the two
    // frees that follow are trivial-element `~Array`s at +0x60 then +0x50
    // (re-decompiled 2026-09-13: MemFree, data = 0, one qword over
    // size+capacity). +0x50 holds Geometry* — `Array<void*>`, trait-derived:
    // byteCopy, no init, no destroy, align 8. +0x60's element is not pinned.
    // LEFT: both arrays are declared as the Array's three fields (H5).
    reinterpret_cast<rtcoll::ArrayHdr*>(&self->emitters)->~Array();          // LEFT: WeatherSimulation.h (GAME-2): raw Array triple (H5)
    reinterpret_cast<Collection::Array<void*>*>(&self->effects)->~Array();  // LEFT: WeatherSimulation.h (GAME-2): raw Array triple (H5)
}

// ------------------------------------------------------------------
// FUN_14001fd10 (terraingenerator.cpp, TerrainGenerator_Slot5_sub_fd10) —
// cached resolve of a {String filename @+0, int generation @+8, External*
// resolved @+0x10} reference triple to a Graphics::MaterialInstance. Re-resolves
// only when the field's cached generation differs from the ExternalFactory's
// (factory+8). Validates via IsDerivedFrom against the MaterialInstance type
// (.rdata slot 0x140151350 = g_ppTypeInfo_MaterialInstance). Runs only on the
// legacy version-0 terrain deserialize path -> dormant on autostart.
//
// Disasm-verified offsets (14001fd10): cmp [RCX+8],factoryGen ; [RBX+0x10]=result
// ; [RBX+8]=factoryGen. Filename built via the String& ctor (slot 0x140151d68 =
// fn_Filename_CtorString = ??0Filename@System@@QEAA@AEBVString@1@@Z), Find via
// ::Object::ExternalFactory::Find. Cascade-safe: if the type slot is unresolved, load
// without IsDerivedFrom validation rather than dereferencing a null BaseTypeInfo.
void Game_TerrainResolveMaterial(void* refTriple) {
    if (!refTriple) return;
    if (!::Object::ExternalFactory::_singleton) return;
    ::Object::ExternalFactory* efactory = ::Object::ExternalFactory::_singleton;
    // The triple is the {String, generation, External*} record MapGenerator.h
    // names ExternalRef (TerrainGenerator's material ref has the same layout).
    MapGenerator::ExternalRef* ref = reinterpret_cast<MapGenerator::ExternalRef*>(refTriple);   // LEFT: SIG Game_TerrainResolveMaterial (caller terraingenerator.cpp declares it locally with void*)
    int factoryGen = efactory->_generation;   // factory+8
    if (ref->factoryGen == factoryGen) return;

    System::Filename fnameBuf(*reinterpret_cast<const System::String*>(&ref->strPtr));  // String @ +0  // LEFT: MapGenerator.h (GAME-3): ExternalRef::strPtr is raw String storage (H5)
    void* obj = const_cast<::Object::External*>(efactory->Find(fnameBuf));  // B.3.3 bridge

    void* result = nullptr;
    if (obj) {
        // Retail: IsDerivedFrom against [0x140151350] (?_typeInfo@MaterialInstance@
        // Graphics@@); ENDGAME B.3.9 TI-REST: the class accessor.
        const uint32_t tid = Graphics::MaterialInstance::TypeId();
        if (::Object::IsDerivedFromNullSafe(obj, tid))
            result = obj;
    }
    ref->resolved = result;
    ref->factoryGen = factoryGen;
}
