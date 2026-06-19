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

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ------------------------------------------------------------------
// String-array destructors. Each is `{ void* data; int count; int cap; }`
// header (data @+0, count @+8) holding `count` elements at the given stride,
// with a System::String embedded at `strOffset` within each element. The
// original frees the data block unconditionally even when count<=0.
// ------------------------------------------------------------------
static inline void DestroyStringArray(void* arrayHeader, uint64_t stride, uint64_t strOffset) {
    uint64_t* hdr = reinterpret_cast<uint64_t*>(arrayHeader);
    uint8_t* base = reinterpret_cast<uint8_t*>(hdr[0]);
    int count = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(hdr) + 8);
    if (count > 0 && base && fn_String_Dtor) {
        uint8_t* p = base;
        for (int i = 0; i < count; ++i) {
            fn_String_Dtor(p + strOffset);
            p += stride;
        }
    }
    if (base && fn_MemFreeStatic) fn_MemFreeStatic(base);
    hdr[0] = 0;
    hdr[1] = 0;
}

// FUN_14001b030 (mapgenerator.cpp) — String at element+0, stride 0x18.
void Game_DestroyStringArray18(void* arrayHeader) { DestroyStringArray(arrayHeader, 0x18, 0); }

// FUN_14001b0a0 (mapgenerator.cpp) — String at element+0, stride 0x30.
void Game_DestroyStringArray30(void* arrayHeader) { DestroyStringArray(arrayHeader, 0x30, 0); }

// FUN_14000a8a0 (gameupdateloop.cpp, EventNotifier_Cleanup) — String at element+8,
// stride 0x28.
void Game_EventNotifierCleanup(void* arrayHeader) { DestroyStringArray(arrayHeader, 0x28, 8); }

// ------------------------------------------------------------------
// FUN_140018220 (gameupdateloop.cpp, MusicPlayer_Dtor) — frees 5 buffers at
// +0x50,+0x40,+0x30,+0x20,+0x10 (high to low) and zeroes each {ptr,len} pair.
// ------------------------------------------------------------------
void Game_MusicPlayerDtor(void* self) {
    uint8_t* b = reinterpret_cast<uint8_t*>(self);
    static const uint64_t kOffsets[5] = { 0x50, 0x40, 0x30, 0x20, 0x10 };
    for (uint64_t off : kOffsets) {
        void** slot = reinterpret_cast<void**>(b + off);
        if (fn_MemFreeStatic) fn_MemFreeStatic(slot[0]);
        slot[0] = nullptr;
        *reinterpret_cast<void**>(b + off + 8) = nullptr;
    }
}

// ------------------------------------------------------------------
// FUN_1400a4fc0 (gameupdateloop.cpp, ImageCapture_Dtor) — virtual scalar-dtor
// of the two sub-objects at +8/+0x10, then frees the two buffers at +0x28/+0x18.
// ------------------------------------------------------------------
void Game_ImageCaptureDtor(void* self) {
    uint8_t* b = reinterpret_cast<uint8_t*>(self);
    void** obj = *reinterpret_cast<void***>(b + 8);
    if (obj) reinterpret_cast<void(*)(void*, int)>(*obj)(obj, 1);
    obj = *reinterpret_cast<void***>(b + 0x10);
    if (obj) reinterpret_cast<void(*)(void*, int)>(*obj)(obj, 1);
    if (fn_MemFreeStatic) {
        fn_MemFreeStatic(*reinterpret_cast<void**>(b + 0x28));
        *reinterpret_cast<void**>(b + 0x28) = nullptr;
        *reinterpret_cast<void**>(b + 0x30) = nullptr;
        fn_MemFreeStatic(*reinterpret_cast<void**>(b + 0x18));
        *reinterpret_cast<void**>(b + 0x18) = nullptr;
        *reinterpret_cast<void**>(b + 0x20) = nullptr;
    }
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
    if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(p[0]));
    p[0] = 0;
    p[1] = 0;
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
    void* scene = gw ? *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(gw) + 0x60) : nullptr;
    static auto removeGlobal = reinterpret_cast<void(*)(void*, void*)>(
        ResolveExport("?RemoveGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z"));
    if (arr && count > 0) {
        for (int i = 0; i < count; ++i) {
            void* geom = arr[i];
            if (geom) {
                if (removeGlobal && scene) removeGlobal(scene, geom);
                void** v = reinterpret_cast<void**>(geom);
                reinterpret_cast<void(*)(void*, int)>(*v)(geom, 1);
            }
        }
    }
    if (fn_MemFreeStatic) {
        fn_MemFreeStatic(*reinterpret_cast<void**>(b + 0x60));
        *reinterpret_cast<void**>(b + 0x60) = nullptr;
        *reinterpret_cast<void**>(b + 0x68) = nullptr;
        fn_MemFreeStatic(*reinterpret_cast<void**>(b + 0x50));
        *reinterpret_cast<void**>(b + 0x50) = nullptr;
        *reinterpret_cast<void**>(b + 0x58) = nullptr;
    }
}
