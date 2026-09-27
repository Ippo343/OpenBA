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
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "system/Filename.h"
#include "system/String.h"
#include "collection/Collection.h"
#include "resolve.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <initializer_list>
#include <new>

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

void AssignRefTriple18(uint8_t* d, const uint8_t* s) {
    // B.3.2 bridge: the entry is still untyped bytes in MapGenerator/BuildingGoal.
    new (d) System::String(*reinterpret_cast<const System::String*>(s));       // +0x00
    *reinterpret_cast<uint32_t*>(d + 0x08) = *reinterpret_cast<const uint32_t*>(s + 0x08);
    *reinterpret_cast<uint64_t*>(d + 0x10) = *reinterpret_cast<const uint64_t*>(s + 0x10);
}
// Generation seed is -1, not 0: 0 is a live ExternalFactory generation and
// would make the lazy resolver take its "cache valid" early-out on a null.
void InitRefTriple18(uint8_t* d) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = 0;
    *reinterpret_cast<uint32_t*>(d + 0x08) = 0xFFFFFFFFu;
    *reinterpret_cast<uint64_t*>(d + 0x10) = 0;
}
void DestroyRefTriple18(void* /*ignoredThis*/, uint8_t* base, int count) {
    for (int i = 0; i < count; ++i)
        reinterpret_cast<System::String*>(base + static_cast<int64_t>(i) * 0x18)->~String();
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

void AssignInitialStructure30(uint8_t* d, const uint8_t* s) {
    // B.3.2 bridge: MapGenerator's InitialStructure is still untyped bytes.
    new (d) System::String(*reinterpret_cast<const System::String*>(s));       // +0x00
    *reinterpret_cast<uint32_t*>(d + 0x08) = *reinterpret_cast<const uint32_t*>(s + 0x08);
    *reinterpret_cast<uint64_t*>(d + 0x10) = *reinterpret_cast<const uint64_t*>(s + 0x10);
    *reinterpret_cast<uint32_t*>(d + 0x18) = *reinterpret_cast<const uint32_t*>(s + 0x18);
    *reinterpret_cast<uint32_t*>(d + 0x1c) = *reinterpret_cast<const uint32_t*>(s + 0x1c);
    *reinterpret_cast<uint32_t*>(d + 0x20) = *reinterpret_cast<const uint32_t*>(s + 0x20);
    *reinterpret_cast<uint32_t*>(d + 0x24) = *reinterpret_cast<const uint32_t*>(s + 0x24);
    *reinterpret_cast<uint32_t*>(d + 0x28) = *reinterpret_cast<const uint32_t*>(s + 0x28);
}
void InitInitialStructure30(uint8_t* d) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = 0;
    *reinterpret_cast<uint32_t*>(d + 0x08) = 0xFFFFFFFFu;               // generation -1
    *reinterpret_cast<uint64_t*>(d + 0x10) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x18) = 0;                         // _offsetX/_offsetY
    *reinterpret_cast<uint64_t*>(d + 0x20) = 0xFFFFFFFFFFFFFFFFull;     // _width/_height -1
    *reinterpret_cast<uint32_t*>(d + 0x28) = 0;                         // _rotation
}
void DestroyInitialStructure30(void* /*ignoredThis*/, uint8_t* base, int count) {
    for (int i = 0; i < count; ++i)
        reinterpret_cast<System::String*>(base + static_cast<int64_t>(i) * 0x30)->~String();
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
    reinterpret_cast<RefTriple18Array*>(arrayHeader)->~Array();          // B.3.2 bridge
}

// FUN_14001b0a0 (mapgenerator.cpp) — String at element+0, stride 0x30.
void Game_DestroyStringArray30(void* arrayHeader) {
    reinterpret_cast<InitialStructure30Array*>(arrayHeader)->~Array();   // B.3.2 bridge
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
    uint8_t* b = reinterpret_cast<uint8_t*>(self);
    static const uint64_t kOffsets[5] = { 0x50, 0x40, 0x30, 0x20, 0x10 };
    for (uint64_t off : kOffsets)
        reinterpret_cast<rtcoll::ArrayHdr*>(b + off)->~Array();   // B.3.2 bridge
}

// ------------------------------------------------------------------
// FUN_1400a4fc0 (gameupdateloop.cpp, ImageCapture_Dtor) — virtual scalar-dtor
// of the two sub-objects at +8/+0x10, then frees the two buffers at +0x28/+0x18.
// ------------------------------------------------------------------
void Game_ImageCaptureDtor(void* self) {
    uint8_t* b = reinterpret_cast<uint8_t*>(self);
    // Retail is (**(code **)*puVar1)(puVar1, 1): deref the object to get its
    // vtable, deref THAT to get slot 0, then call. This port had only one deref
    // and so called the vtable's own address as if it were code — EXEC of
    // g_NoiseUI_ImageBufferVtable_Standalone when GameUpdateLoop::Shutdown
    // finally reached the minimap ImageBuffers.
    for (int off : {8, 0x10}) {
        void* obj = *reinterpret_cast<void**>(b + off);
        if (!obj) continue;
        void** vtbl = *reinterpret_cast<void***>(obj);
        if (!vtbl || !vtbl[0]) continue;
        reinterpret_cast<void(*)(void*, int)>(vtbl[0])(obj, 1);
    }
    // ENDGAME B.3.2 step 2: two trivial-element `~Array`s (headers at +0x28 and
    // +0x18, in that order — re-decompiled 2026-09-13: MemFree, data = 0, one
    // qword over size+capacity), element types not pinned; see MusicPlayerDtor.
    reinterpret_cast<rtcoll::ArrayHdr*>(b + 0x28)->~Array();   // B.3.2 bridge
    reinterpret_cast<rtcoll::ArrayHdr*>(b + 0x18)->~Array();   // B.3.2 bridge
}

// ------------------------------------------------------------------
// FUN_14001b860 (menuscene.cpp) — tear down a small tree at +0x10 (root's two
// children freed via the registered recursive tree-free 0x14000e320), then free
// the backing array at +0. Mirrors FUN_14000a8a0's sibling layout but with a
// tree node rather than a String element.
// ------------------------------------------------------------------
void Game_MenuSceneArrayDtor(void* self) {
    uint64_t* p = reinterpret_cast<uint64_t*>(self);
    void* node = reinterpret_cast<void*>(p[2]);  // *(self + 0x10)
    if (node) {
        typedef void (*treefree_t)(void* headerSlot, void* child);
        treefree_t e320 = reinterpret_cast<treefree_t>(ResolveAddr(Module::Exe, 0x14000e320));
        if (e320) {
            e320(&p[2], *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(node) + 0x18));
            e320(&p[2], *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(node) + 0x20));
        }
        if (fn_MemFreeStatic) fn_MemFreeStatic(node);
    }
    p[2] = 0;
    *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(p) + 0x18) = 0;
    // ENDGAME B.3.2 step 2: the backing array at +0 is a trivial-element
    // `~Array` (re-decompiled 2026-09-13: MemFree(*p); p[0] = 0; p[1] = 0).
    // The tree at +0x10 above is a {root, count} pair, not an Array; untouched.
    reinterpret_cast<rtcoll::ArrayHdr*>(p)->~Array();   // B.3.2 bridge
}

// ------------------------------------------------------------------
// FUN_140093eb0 (gameupdateloop.cpp, Weather_Dtor) — remove each geometry in the
// array at +0x50 (count +0x58) from the global scene and virtual-dtor it, then
// free the two arrays at +0x60 / +0x50. Scene = (*g_pGameWorldSingleton)+0x60.
// ------------------------------------------------------------------
void Game_WeatherDtor(void* self) {
    uint8_t* b = reinterpret_cast<uint8_t*>(self);
    int count = *reinterpret_cast<int*>(b + 0x58);
    void** arr = *reinterpret_cast<void***>(b + 0x50);
    void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    void* scene = gw ? reinterpret_cast<GameUpdateLoop*>(gw)->m_graphicsScene : nullptr;   // B.3.8 bridge
    if (arr && count > 0) {
        for (int i = 0; i < count; ++i) {
            void* geom = arr[i];
            if (geom) {
                if (scene)
                    reinterpret_cast<Graphics::Scene*>(scene)->RemoveGlobalObject(
                        reinterpret_cast<Graphics::Geometry*>(geom));   // B.3.5 bridge
                // Retail: (**(code **)*puVar2)(puVar2, 1) — object -> vtable ->
                // slot 0. Same one-deref-short bug as Game_ImageCaptureDtor above.
                void** vtbl = *reinterpret_cast<void***>(geom);
                if (vtbl && vtbl[0])
                    reinterpret_cast<void(*)(void*, int)>(vtbl[0])(geom, 1);
            }
        }
    }
    // ENDGAME B.3.2 step 2: the loop above is Weather's own per-element work
    // (retail 0x140093eb0 runs it inline, before the array frees); the two
    // frees that follow are trivial-element `~Array`s at +0x60 then +0x50
    // (re-decompiled 2026-09-13: MemFree, data = 0, one qword over
    // size+capacity). +0x50 holds Geometry* — `Array<void*>`, trait-derived:
    // byteCopy, no init, no destroy, align 8. +0x60's element is not pinned.
    reinterpret_cast<rtcoll::ArrayHdr*>(b + 0x60)->~Array();          // B.3.2 bridge
    reinterpret_cast<Collection::Array<void*>*>(b + 0x50)->~Array();  // B.3.2 bridge
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
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) return;
    void* factory = *g_pExternalFactorySingleton;
    // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
    // The singleton is still read through the EXISTING door, so the guard
    // above still gates the same cell and the same registered body still runs.
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);   // B.3.3 bridge
    uint8_t* b = reinterpret_cast<uint8_t*>(refTriple);
    int factoryGen = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(factory) + 8);
    if (*reinterpret_cast<int*>(b + 8) == factoryGen) return;

    System::Filename fnameBuf(*reinterpret_cast<const System::String*>(b));  // String @ b+0
    void* obj = const_cast<::Object::External*>(efactory->Find(fnameBuf));  // B.3.3 bridge

    void* result = nullptr;
    if (obj) {
        const uint32_t* tid =
            (g_ppTypeInfo_MaterialInstance && *g_ppTypeInfo_MaterialInstance)
                ? *g_ppTypeInfo_MaterialInstance : nullptr;
        if (!tid || (fn_IsDerivedFrom && fn_IsDerivedFrom(obj, *tid)))
            result = obj;
    }
    *reinterpret_cast<void**>(b + 0x10) = result;
    *reinterpret_cast<int*>(b + 8) = factoryGen;
}
