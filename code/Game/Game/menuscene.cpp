// code/Game/Game/menuscene.cpp
// Clean reimplementation of MenuScene game logic.
//
// MenuScene (0x208 bytes) -- main menu scene controller.
// Extends Object::External (0x20 base).
// Vtable at 0x14015ede0.
//
// Contains scene geometry (+0x28), dynamic arrays of scene objects
// (+0x30/+0x40/+0x50/+0x60), an external reference (+0x70),
// a weather reference (+0x80), an embedded transform at +0x88,
// atmosphere/lighting colors and fog parameters, plus two tree/map
// structures at +0x1C8 and +0x1E8.
//
// Decompiled from Application-x64.exe.

#include "Game/menuscene_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

// ============================================================
// Local function typedefs (resolved lazily)
// ============================================================

// External base ctor -- dispatch slot 0x140151e98
typedef void (*fn_External_Ctor_t)(void* self);
static fn_External_Ctor_t s_fn_External_Ctor = nullptr;

// External base dtor -- dispatch slot 0x140151ea0
typedef void (*fn_External_BaseDtor_t)(void* self);
static fn_External_BaseDtor_t s_fn_External_BaseDtor = nullptr;

// Transform init helper -- 0x140098800
typedef void (*fn_Transform_Init_t)(void* transform);
static fn_Transform_Init_t s_fn_Transform_Init = nullptr;

// Tree/map cleanup helper -- 0x14001b860
typedef void (*fn_TreeCleanup_t)(void* treeBase);
static fn_TreeCleanup_t s_fn_TreeCleanup = nullptr;

// Original Slot5/Serialize function -- 0x1400a3630
// Delegated to directly (201 instructions, too many sub-dependencies).
typedef void (*fn_MenuScene_Serialize_Orig_t)(void* self, void* stream);
static fn_MenuScene_Serialize_Orig_t s_fn_Serialize_Orig = nullptr;

// ============================================================
// Lazy-init resolver
// ============================================================
#ifdef STANDALONE_EXE
// Forward decl — Transform initializer reimplemented locally for standalone.
// Mirrors FUN_140098800 (exe 0x140098800). Built from engine_funcs primitives
// (Math::Transform::Transform + Volume::Frustum + Graphics::Camera) so we
// don't depend on the exe address registry having 0x140098800.
static void MenuScene_TransformInit_Standalone(void* transformObj);
#endif

static void EnsureResolved() {
    if (s_fn_External_Ctor) return;

    // Dispatch slot reads -- these are IAT-style function pointers in .rdata
    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140151e98);
    if (slot) s_fn_External_Ctor = *reinterpret_cast<fn_External_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140151ea0);
    if (slot) s_fn_External_BaseDtor = *reinterpret_cast<fn_External_BaseDtor_t*>(slot);

#ifdef STANDALONE_EXE
    // Standalone: we don't have FUN_140098800 in our exe registry. Use our
    // local reimplementation instead — it composes the same Runtime DLL
    // primitives the original does (Transform ctor + Frustum + Camera).
    s_fn_Transform_Init = &MenuScene_TransformInit_Standalone;
#else
    s_fn_Transform_Init = reinterpret_cast<fn_Transform_Init_t>(
        ResolveAddr(Module::Exe, 0x140098800));
#endif

    s_fn_TreeCleanup = reinterpret_cast<fn_TreeCleanup_t>(
        ResolveAddr(Module::Exe, 0x14001b860));

    s_fn_Serialize_Orig = reinterpret_cast<fn_MenuScene_Serialize_Orig_t>(
        ResolveAddr(Module::Exe, 0x1400a3630));
}

// ============================================================
// MenuScene::Init (also Slot8 body) -- Constructor
// ============================================================
// Original: 0x1400a3d70
//
// Calls External base ctor, sets MenuScene vtable, zeroes all fields
// from +0x20 to +0x80, initializes embedded Transform at +0x88,
// sets atmosphere defaults (colors, fog, intensities), zeroes
// remaining fields from +0x1B8 to +0x200, and initializes two
// tree structures at +0x1C8 and +0x1E8.
void MenuScene_Init(MenuScene* self) {
    EnsureResolved();

    // External base construction
    if (s_fn_External_Ctor) {
        s_fn_External_Ctor(self);
    }

    // Set MenuScene vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, MenuSceneAddr::Vtable);
    self->vtable = s_vtable;

    // Zero pointer/array fields (+0x20 through +0x80, skipping +0x78)
    self->_field20 = nullptr;   // +0x20: String (name)
    self->_field28 = nullptr;   // +0x28: Geometry* (scene object)
    self->_field30 = nullptr;   // +0x30: array data ptr
    self->_field38 = nullptr;   // +0x38: array count/cap
    self->_field40 = nullptr;   // +0x40: array data ptr
    self->_field48 = nullptr;   // +0x48: array count/cap
    self->_field50 = nullptr;   // +0x50: array data ptr
    self->_field58 = nullptr;   // +0x58: array count/cap
    self->_field60 = nullptr;   // +0x60: array data ptr
    self->_field68 = nullptr;   // +0x68: array count/cap
    self->_field70 = nullptr;   // +0x70: ExternalPtr (typed reference)
    self->_field80 = nullptr;   // +0x80: ExternalPtr (Weather)

    // Initialize embedded Transform at +0x88
    if (s_fn_Transform_Init) {
        s_fn_Transform_Init(&self->transform);
    }

    // Atmosphere/lighting defaults (same as Weather initial values)
    self->ambientColor     = 0xff595965;
    self->sunColor         = 0xffcbcbbc;
    self->skyColor         = 0xffbfbff2;
    self->fogStart         = 90.0f;       // 0x42b40000
    self->fogEnd           = 225.0f;      // 0x43610000
    self->fogDensity       = 0.95f;       // 0x3f733333
    self->sunIntensity     = 120.0f;      // 0x42f00000

    // +0x1B0: ambientIntensity = 65.0f (0x42820000), packed as qword with +0x1B4 = 0
    self->ambientIntensity = 65.0f;
    memset(&self->_pad1B4, 0, 4);

    // Zero remaining fields
    self->_field1B8 = nullptr;  // +0x1B8
    self->_field1C0 = 0;        // +0x1C0

    // Initialize tree/map at +0x1C8 (20 bytes: ptr, ptr, ptr+8, count)
    // Layout: +0x1C8=data, +0x1D0=data, +0x1D8=root, +0x1E0=count
    self->_field1C8 = nullptr;
    self->_field1D0 = nullptr;
    self->_field1D8 = nullptr;
    self->_field1E0 = 0;

    // Initialize tree/map at +0x1E8 (20 bytes: same layout)
    self->_field1E8 = nullptr;
    self->_field1F0 = nullptr;
    self->_field1F8 = nullptr;
    self->_field200 = 0;
}

// ============================================================
// MenuScene::Slot8 (vtable +0x40) -- Null-safe Init
// ============================================================
// Original: 0x14001b800
//
// If self is null, returns null. Otherwise jumps to Init (constructor).
void* MenuScene_Slot8(MenuScene* self) {
    if (!self) return nullptr;
    MenuScene_Init(self);
    return self;
}

// ============================================================
// MenuScene::Dtor (vtable +0x00) -- Scalar deleting destructor
// ============================================================
// Original: 0x14001b830 (entry) -> 0x1400a3970 (body)
//
// Restores vtable, destroys Geometry at +0x28 (scalar delete via vtable),
// cleans up tree structures at +0x1E8 and +0x1C8, frees arrays at
// +0x60, +0x50, +0x40, +0x30, destroys String at +0x20, calls
// External base dtor, and conditionally frees memory.
void* MenuScene_Dtor(MenuScene* self, uint32_t flags) {
    EnsureResolved();

    // --- Destructor body (0x1400a3970) ---

    // Restore vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, MenuSceneAddr::Vtable);
    self->vtable = s_vtable;

    // Destroy Geometry object at +0x28 via its vtable scalar deleting dtor
    void* geom = self->_field28;
    if (geom) {
        typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
        void** geomVtable = *(void***)geom;
        fn_ScalarDtor_t geomDtor = (fn_ScalarDtor_t)geomVtable[0];
        geomDtor(geom, 1);  // flags=1 => free memory
    }

    // Cleanup tree/map at +0x1E8 and +0x1C8
    if (s_fn_TreeCleanup) {
        s_fn_TreeCleanup(&self->_field1E8);
        s_fn_TreeCleanup(&self->_field1C8);
    }

    // Free array at +0x60, zero +0x60/+0x68
    if (fn_MemFreeStatic && self->_field60) {
        fn_MemFreeStatic(self->_field60);
    }
    self->_field60 = nullptr;
    self->_field68 = nullptr;

    // Free array at +0x50, zero +0x50/+0x58
    if (fn_MemFreeStatic && self->_field50) {
        fn_MemFreeStatic(self->_field50);
    }
    self->_field50 = nullptr;
    self->_field58 = nullptr;

    // Free array at +0x40, zero +0x40/+0x48
    if (fn_MemFreeStatic && self->_field40) {
        fn_MemFreeStatic(self->_field40);
    }
    self->_field40 = nullptr;
    self->_field48 = nullptr;

    // Free array at +0x30, zero +0x30/+0x38
    if (fn_MemFreeStatic && self->_field30) {
        fn_MemFreeStatic(self->_field30);
    }
    self->_field30 = nullptr;
    self->_field38 = nullptr;

    // Destroy String at +0x20
    if (fn_String_Dtor) {
        fn_String_Dtor(&self->_field20);
    }

    // Call External base destructor
    if (s_fn_External_BaseDtor) {
        s_fn_External_BaseDtor(self);
    }

    // --- End dtor body, back to scalar deleting dtor (0x14001b830) ---

    // Conditional free
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// MenuScene::Cleanup (vtable +0x30, Slot6)
// ============================================================
// Original: 0x14001b810
//
// Calls dtor via vtable with flags=0 (don't free memory), then
// re-constructs in place via the constructor.
void MenuScene_Cleanup(MenuScene* self) {
    // Call dtor through vtable with flags=0
    typedef void* (*fn_VtableDtor_t)(void* self, uint32_t flags);
    fn_VtableDtor_t dtor = *(fn_VtableDtor_t*)(*(uintptr_t*)self);
    dtor(self, 0);

    // Re-construct in place
    MenuScene_Init(self);
}

// ============================================================
// MenuScene::Serialize (vtable +0x28, Slot5)
// ============================================================
// Original: 0x1400a3630
//
// This is a large function (201 instructions) that serializes all
// MenuScene fields including arrays, external references, transforms,
// colors, floats, and version-gated sections. It requires many
// unresolved helper functions (Color::Serialize, Vector4::Serialize,
// Geometry serialize, Transform serialize, typed ExternalPtr serialize,
// array resize/serialize helpers).
//
// Delegated to the original function until those dependencies are
// available in engine_funcs.h.
void MenuScene_Serialize(MenuScene* self, void* stream) {
    EnsureResolved();

#ifdef STANDALONE_EXE
    // Standalone: 0x1400a3630 isn't in the exe registry and the helper chain
    // (Color::Serialize, Vector4::Serialize, Geometry/Transform serialize,
    // typed ExternalPtr serialize, dynamic-array serialize) isn't wired yet.
    // Skip serialization and let the caller see a default-init MenuScene.
    // Phase A's load/draw are null-tolerant, so the menu renders with the
    // ctor-supplied atmosphere defaults instead of the .rsc payload.
    static bool s_warned = false;
    if (!s_warned) {
        printf("[MenuScene::Slot5_Serialize] standalone no-op "
               "(self=%p stream=%p; scene data not loaded)\n",
               (void*)self, stream);
        fflush(stdout);
        s_warned = true;
    }
    return;
#else
    if (s_fn_Serialize_Orig) {
        s_fn_Serialize_Orig(self, stream);
    }
#endif
}

// ============================================================
// STANDALONE_EXE: Factory + Vtable + REGISTER_TYPE
// ============================================================
// In standalone mode we own the type registration. The factory allocates
// 0x208 bytes (sizeof(MenuScene)) with 8-byte alignment and runs Init.
// Mirrors the StatGoal pattern in code/Game/Goal/statgoal.cpp.

#ifdef STANDALONE_EXE

#include "Object/factory_registration.h"
#include "address_registry.h"

// Local mirror of FUN_140098800 (exe 0x140098800). Composes the same
// Runtime DLL primitives (Math::Transform::Transform + Volume::Frustum +
// Graphics::Camera) that menudialogloopt.cpp's MenuScene_Helper_8800 uses.
// Replicated here so the menuscene.cpp TU is self-contained for standalone.
static void MenuScene_TransformInit_Standalone(void* transformObj) {
    if (!transformObj) return;

    if (!fn_Math_Transform_Ctor || !fn_Math_Transform3_Identity ||
        !fn_Volume_Frustum_Ctor || !fn_Graphics_Camera_Ctor) {
        printf("[MenuScene::TransformInit] missing Runtime fn ptrs "
               "(TCtor=%p T3Id=%p FrustC=%p CamC=%p) — leaving transform zeroed\n",
               (void*)fn_Math_Transform_Ctor,
               (void*)fn_Math_Transform3_Identity,
               (void*)fn_Volume_Frustum_Ctor,
               (void*)fn_Graphics_Camera_Ctor);
        return;
    }

    fn_Math_Transform_Ctor(transformObj);

    // Zero scalars at +0x1C (qword) and +0x24 (dword) per original disasm.
    *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(transformObj) + 0x1C) = 0;
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(transformObj) + 0x24) = 0;

    void* ident = fn_Math_Transform3_Identity();

    // Frustum ctor — original uses near=1.0, far=100.0, fovX=fovY=90.0.
    float fovX = 90.0f, fovY = 90.0f, nearZ = 1.0f, farZ = 100.0f;
    uint8_t frustumBuf[0x80] = {};
    fn_Volume_Frustum_Ctor(frustumBuf, &fovX, &fovY, &nearZ, &farZ);

    // Camera lives at transform+0x34 (per the embedded Camera at +0x34 inside
    // the 0x10C-byte Transform block).
    void* cameraObj = reinterpret_cast<uint8_t*>(transformObj) + 0x34;
    fn_Graphics_Camera_Ctor(cameraObj, frustumBuf, ident, true);
}

// Standalone vtable for MenuScene (11 slots; matches Object::External shape).
// Slots not yet reimplemented are nullptr — the loader's instantiation path
// only touches Dtor / GetTypeId / Serialize / Cleanup / Slot8.
static uint32_t MenuScene_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) {
        cached = ::Object::GetTypeIdByName("MenuScene");
    }
    return cached;
}

// Adapter trampolines for vtable slots that don't directly match the
// (void*, ...) shape the Runtime expects.
static void* MenuScene_Slot8_Trampoline(void* self) {
    return MenuScene_Slot8(reinterpret_cast<MenuScene*>(self));
}
static void MenuScene_Slot6_Trampoline(void* self) {
    MenuScene_Cleanup(reinterpret_cast<MenuScene*>(self));
}
static void MenuScene_Slot5_Trampoline(void* self, void* stream) {
    MenuScene_Serialize(reinterpret_cast<MenuScene*>(self), stream);
}
static void* MenuScene_Dtor_Trampoline(void* self, uint32_t flags) {
    return MenuScene_Dtor(reinterpret_cast<MenuScene*>(self), flags);
}

// Safe stubs for vtable slots whose original implementation is unknown.
// Runtime's FindOrLoad chain calls some of these on the freshly-allocated
// MenuScene before Slot5 (Serialize). Originals for Slot3/Slot4 are simple
// `mov eax,1; ret` (return 1). Others return 0/null.
static int MenuScene_VtblStub_Return1(void* /*self*/) { return 1; }
static int MenuScene_VtblStub_Return0(void* /*self*/) { return 0; }
static void* MenuScene_VtblStub_ReturnNull(void* /*self*/, ...) { return nullptr; }

static void* g_MenuScene_Vtable_Standalone[11] = {
    reinterpret_cast<void*>(&MenuScene_Dtor_Trampoline),   // +0x00 dtor
    reinterpret_cast<void*>(&MenuScene_VtblStub_ReturnNull),// +0x08 GetTypeInfo
    reinterpret_cast<void*>(&MenuScene_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&MenuScene_VtblStub_Return1),   // +0x18 Slot3 (mov eax,1; ret)
    reinterpret_cast<void*>(&MenuScene_VtblStub_Return1),   // +0x20 Slot4 (mov eax,1; ret)
    reinterpret_cast<void*>(&MenuScene_Slot5_Trampoline),   // +0x28 Slot5 / Serialize
    reinterpret_cast<void*>(&MenuScene_Slot6_Trampoline),   // +0x30 Slot6 / Cleanup
    reinterpret_cast<void*>(&MenuScene_VtblStub_Return0),   // +0x38 Slot7 base
    reinterpret_cast<void*>(&MenuScene_Slot8_Trampoline),   // +0x40 Slot8 / Init
    reinterpret_cast<void*>(&MenuScene_VtblStub_Return0),   // +0x48 Slot9 (unused)
    reinterpret_cast<void*>(&MenuScene_VtblStub_ReturnNull),// +0x50 Slot10 (unused)
};

extern "C" void RegisterMenuSceneVtable() {
    RegisterExeAddr(MenuSceneAddr::Vtable,
                    reinterpret_cast<uintptr_t>(g_MenuScene_Vtable_Standalone));
}

extern "C" void* MenuScene_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) {
        printf("[MenuScene::Factory] MemAlloc unavailable — returning NULL\n");
        return nullptr;
    }
    void* p = fn_MemAlloc(g_pMemAllocSingleton, sizeof(MenuScene), 8);
    if (!p) return nullptr;
    MenuScene_Init(reinterpret_cast<MenuScene*>(p));
    printf("[MenuScene::Factory] allocated %p (size=0x%x)\n",
           p, (unsigned)sizeof(MenuScene));
    return p;
}

extern "C" void* MenuScene_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    MenuScene_Init(reinterpret_cast<MenuScene*>(memory));
    return memory;
}

REGISTER_TYPE(MenuScene, "Object::External",
              &MenuScene_Factory, &MenuScene_CreateInPlace);

#endif // STANDALONE_EXE
