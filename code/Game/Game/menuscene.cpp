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
#include <cstdlib>
#include <cstring>

// Heap-overflow checkpoint (standalone diagnostic; no-op when
// BANISHED_REDZONE is unset).
#include "redzone_alloc.h"
#define RZ_CHECK(tag) RedzoneAlloc_CheckLinks(tag)

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

// M3.5 Path B session 8 — top-level Geometry serializers used by the
// standalone Slot5 branch.
extern "C" void Reimpl_TradeWork_impl_04d0(void* self, void* stream);
extern "C" void Reimpl_TradeWork_impl_f1a0(void* self, void* stream);

// ============================================================
// Lazy-init resolver
// ============================================================
// Forward decl — Transform initializer reimplemented locally for standalone.
// Mirrors FUN_140098800 (exe 0x140098800). Built from engine_funcs primitives
// (Math::Transform::Transform + Volume::Frustum + Graphics::Camera) so we
// don't depend on the exe address registry having 0x140098800.
static void MenuScene_TransformInit_Standalone(void* transformObj);

static void EnsureResolved() {
    if (s_fn_External_Ctor) return;

    // Dispatch slot reads -- these are IAT-style function pointers in .rdata
    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140151e98);
    if (slot) s_fn_External_Ctor = *reinterpret_cast<fn_External_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140151ea0);
    if (slot) s_fn_External_BaseDtor = *reinterpret_cast<fn_External_BaseDtor_t*>(slot);

    // Standalone: we don't have FUN_140098800 in our exe registry. Use our
    // local reimplementation instead — it composes the same Runtime DLL
    // primitives the original does (Transform ctor + Frustum + Camera).
    s_fn_Transform_Init = &MenuScene_TransformInit_Standalone;

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

    // M3.5 Path B session 8 — Geometry deserialize sub-sequence.
    //
    // Mirrors the Geometry-allocation + serialize calls inside the original
    // MenuScene_Slot5 (0x1400a3630), specifically:
    //   0x1400a36c4: CALL 0x1400b00e0   ; Geometry ctor on freshly-allocated 0x230B buffer
    //   0x1400a36d5: CALL 0x1400b04d0   ; TradeWork_impl_04d0 (Geometry::Serialize body)
    //   0x1400a36f0: CALL 0x1400af1a0   ; TradeWork_impl_f1a0 (version-gated extra)
    //
    // We skip the surrounding Slot5 body (String name, atmosphere colors,
    // version-gated arrays, etc.) — those depend on Color::Serialize +
    // Vector4::Serialize + Array_Serialize_External_Particle/GraphicsMesh
    // helpers that aren't yet wired into a clean reimpl. Phase A's load/draw
    // is null-tolerant for those fields; the goal of session 8 is just that
    // the Geometry container at +0x28 deserializes end-to-end so terrain
    // renders.
    //
    // Allocate a fresh Geometry on load if there isn't one already. We
    // assume IsLoading by default (the standalone path only loads .rsc
    // payloads — never saves at this level). The original guards on
    // serializer's IsLoading flag plus `*(int*)(serializer + 8) == 0`,
    // but we don't have a parent serializer here; instead we trust the
    // stream's IsLoading state (read by the inner serializers).
    //
    // CASCADE risk: TradeWork_impl_04d0 calls FUN_140007140 three times
    // and FUN_140020150 once, all against MaterialInstance typeInfo slot
    // 0x140151350. If MaterialInstance isn't yet REGISTER_TYPE'd in
    // standalone, those cascades log one-shot warnings and skip. The
    // 0x60B per-element serializer (TradeWork_impl_ffa0) will likewise
    // soft-fail per element. The image-buffer upload (TradeWork_sub_c780)
    // soft-fails if VideoDevice::_singleton isn't ready. Worst case: the
    // menu shows the ctor-supplied atmosphere defaults instead of terrain.
    static bool s_loggedOnce = false;
    if (!s_loggedOnce) {
        printf("[MenuScene::Slot5_Serialize] standalone Geometry-deserialize path "
               "(self=%p stream=%p)\n", (void*)self, stream);
        fflush(stdout);
        s_loggedOnce = true;
    }

    // (1) Construct an OUTER Serializer (version=2) wrapping `stream`, then
    // String::Serialize on &self->_field20 to advance the stream cursor past
    // the type-name marker that the file format encodes at the start of
    // every External payload. Without this prelude the cursor sits at
    // ASCII type-name bytes ("Gase"/"vers" etc.) and Geometry::Serialize
    // reads garbage as int counts → infinite-loop in Stream::Read.
    //
    // Mirrors original 0x1400a3656..0x1400a366f:
    //   ??0Serializer@IO@@QEAA@AEAVStream@1@H@Z(outer, stream, 2)
    //   ?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z(&self+0x20, outer)
    uint64_t outer_ser[3] = {0, 0, 0};
    bool outer_ctored = false;
    if (stream && g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(outer_ser, stream, 2);
        outer_ctored = true;
        if (g_fn_String_Serialize) {
            g_fn_String_Serialize(&self->_field20, outer_ser);
        } else {
            printf("[MenuScene::Slot5_Serialize] g_fn_String_Serialize NULL — "
                   "stream cursor not advanced past type-name; Geometry "
                   "deserialize will likely read garbage\n");
            fflush(stdout);
        }
    } else {
        printf("[MenuScene::Slot5_Serialize] Serializer ctor unavailable — "
               "skipping outer prelude (terrain will be missing)\n");
        fflush(stdout);
    }

    // (2) Allocate + ctor a Geometry container at +0x28 if needed.
    if (self->_field28 == nullptr) {
        if (!fn_MemAlloc || !g_pMemAllocSingleton) {
            printf("[MenuScene::Slot5_Serialize] MemAlloc unavailable — "
                   "skipping Geometry alloc (terrain will be missing)\n");
            fflush(stdout);
        } else {
            void* geom = fn_MemAlloc(g_pMemAllocSingleton, 0x230, 8);
            if (geom) {
                if (fn_Geometry_Init) {
                    fn_Geometry_Init(geom);
                } else {
                    // Fallback: zero-fill. The session 5 helpers should still
                    // resize correctly given a zeroed header.
                    memset(geom, 0, 0x230);
                }
                self->_field28 = geom;
            } else {
                printf("[MenuScene::Slot5_Serialize] MemAlloc(0x230) failed — "
                       "no Geometry container, skipping deserialize\n");
                fflush(stdout);
            }
        }
    }

    // (3) Call into the body serializer + version-gated extra.
    //
    // DISABLED until the deserialize is debugged end-to-end. The current body
    // reads garbage counts at multiple steps even after the prelude advances
    // the cursor — likely a misalignment somewhere in steps 4-8 that makes
    // step 9 read e.g. -65410 as outerCount. The downstream renderer then
    // dereferences wild VertexBuffer/IndexBuffer pointers and crashes on
    // virtual dispatch. Until the alignment bugs are fixed, leave the
    // Geometry zero-initialized — terrain doesn't render but the menu is
    // stable. Defensive negative-count guards in the resize helpers are
    // still in place for any future wiring.
    //
    // To re-enable: uncomment below + investigate steps 4-8 in
    // geometry_serialize.cpp against the original 0x1400b04d0 disasm.
    RZ_CHECK("Slot5 before geometry deserialize");
    // The menu Geometry/terrain deserialize (TradeWork_impl_04d0 + _f1a0) MUST
    // run: besides building the terrain, it advances the stream cursor past the
    // geometry payload so the camera Transform (step 3.5) and the atmosphere/
    // particle tail (step 4) read from the correct offset. Skipping it leaves
    // the camera at a zero (degenerate) transform and misaligns the tail
    // (observed: pos/qrot all zero, menu hangs). It is, however, the source of
    // the intermittent menu heap crash — see geometry_serialize.cpp for the
    // OOB fix. BANISHED_NO_MENU_GEOM=1 skips it (diagnostic only; breaks the menu).
    {
        static int s_noGeom = -1;
        if (s_noGeom < 0) {
            const char* e = getenv("BANISHED_NO_MENU_GEOM");
            s_noGeom = (e && e[0] && e[0] != '0') ? 1 : 0;
        }
        if (!s_noGeom && self->_field28 && stream) {
            Reimpl_TradeWork_impl_04d0(self->_field28, stream);
            Reimpl_TradeWork_impl_f1a0(self->_field28, stream);
        }
    }
    RZ_CHECK("Slot5 after geometry deserialize");

    // (3.5) Camera Transform deserialize — original Slot5 calls this AFTER
    // the Geometry deserialize and BEFORE the atmosphere colors. Without it
    // MenuScene+0x88 stays at the ctor's identity transform and the menu
    // camera is stuck at world origin → entire 3D scene renders behind/inside
    // the terrain → black screen. See FUN_1400988c0 in exe + memory file
    // project_terrain_render_status.md (2026-05-01).
    if (stream) {
        void* xform = reinterpret_cast<uint8_t*>(self) + 0x88;
        // fn_Transform_SerializeWrapper resolves to a NOT_IMPLEMENTED stub via
        // the address registry — DON'T call it, use the inline path.
        if (g_fn_Serializer_Ctor && g_fn_Serializer_Dtor &&
                   fn_Math_Transform_Serialize && fn_Math_Vector3_Serialize &&
                   fn_Serializer_Serialize_Float) {
            // Inline the FUN_1400988c0 body: inner Serializer (v=0) wraps
            // stream; serialize Math::Transform (28B) + Math::Vector3 (12B at
            // +0x1c) + 3 floats (+0x28, +0x2C, +0x30); dtor.
            uint64_t inner_ser[3] = {0, 0, 0};
            g_fn_Serializer_Ctor(inner_ser, stream, 0);
            fn_Math_Transform_Serialize(xform, inner_ser);
            fn_Math_Vector3_Serialize(reinterpret_cast<uint8_t*>(xform) + 0x1C,
                                      inner_ser);
            fn_Serializer_Serialize_Float(inner_ser,
                reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(xform) + 0x28),
                nullptr);
            fn_Serializer_Serialize_Float(inner_ser,
                reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(xform) + 0x2C),
                nullptr);
            fn_Serializer_Serialize_Float(inner_ser,
                reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(xform) + 0x30),
                nullptr);
            g_fn_Serializer_Dtor(inner_ser);
            static bool s_logged = false;
            if (!s_logged) {
                const float* xf = reinterpret_cast<const float*>(xform);
                printf("[MenuScene::Slot5_Serialize] camera Transform "
                       "deserialized: pos=(%.2f, %.2f, %.2f) "
                       "qrot=(%.3f, %.3f, %.3f, %.3f) +0x1c=(%.2f,%.2f,%.2f) "
                       "+0x28=%.2f +0x2c=%.2f +0x30=%.2f\n",
                       xf[0], xf[1], xf[2],
                       xf[3], xf[4], xf[5], xf[6],
                       xf[7], xf[8], xf[9],
                       xf[10], xf[11], xf[12]);
                fflush(stdout);
                s_logged = true;
            }
        } else {
            static bool s_warned = false;
            if (!s_warned) {
                printf("[MenuScene::Slot5_Serialize] Transform serialize primitives "
                       "unwired (TWrapper=%p MathT=%p MathV3=%p SerFloat=%p) — "
                       "camera will stay at identity, screen black\n",
                       (void*)fn_Transform_SerializeWrapper,
                       (void*)fn_Math_Transform_Serialize,
                       (void*)fn_Math_Vector3_Serialize,
                       (void*)fn_Serializer_Serialize_Float);
                fflush(stdout);
                s_warned = true;
            }
        }
    }

    // (4) Tail of MenuScene::Slot5 — runs UNCONDITIONALLY in original
    // (outside the cVar2/IsValid encoding gate). Uses the outer Serializer
    // we already constructed at step 1, so the cursor is in sync with the
    // file. We MUST run the array/extPtr reads before the atmosphere block
    // even if the menu scene's payload has count=0 entries — otherwise the
    // cursor mis-aligns and atmosphere fields read garbage.
    //
    // Maps to original 0x1400a3705..0x1400a3825 (5 calls + atmosphere):
    //   ?Serialize@Serializer@IO@@QEAAXAEAHPEBD@Z(outer, &count38, NULL)
    //   FUN_14001baf0(field30, count38)        ; resize stride-0x20 array
    //   FUN_14001bbf0(outer, count38, field30) ; serialize array elements
    //   ?Serialize@Serializer@IO@@QEAAXAEAHPEBD@Z(outer, &count48, NULL)
    //   FUN_140001bb0(field40, count48)        ; resize stride-0x08 array
    //   FUN_14001bcc0(outer, count48, field40) ; serialize ext-ptr array
    //   FUN_14001b8e0(&field70, outer)         ; single ext-ptr (GraphicsMesh)
    //   Math::Vector4::Serialize(field+0x1B4, outer)
    //   System::Color::Serialize(field+0x194, outer)  x3
    //   Serializer::Serialize_Float(outer, field+0x1A0, NULL) x5
    if (outer_ctored && g_fn_Serialize_Int) {
        // ----- Array #1 (+0x30, stride 0x20) -----
        int count38 = static_cast<int>(reinterpret_cast<uintptr_t>(self->_field38) & 0xFFFFFFFF);
        g_fn_Serialize_Int(outer_ser, &count38, nullptr);
        if (count38 > 0 && fn_Array_Resize_Stride20) {
            fn_Array_Resize_Stride20(&self->_field30, count38);
        }
        if (count38 > 0 && fn_Array_Serialize_Stride20) {
            fn_Array_Serialize_Stride20(outer_ser, static_cast<uint32_t>(count38),
                                        self->_field30);
        }

        RZ_CHECK("Slot5 after arr30 (stride0x20 instances)");

        // ----- Array #2 (+0x40, stride 0x08, GraphicsMesh exrefs) -----
        int count48 = static_cast<int>(reinterpret_cast<uintptr_t>(self->_field48) & 0xFFFFFFFF);
        g_fn_Serialize_Int(outer_ser, &count48, nullptr);
        if (count48 > 0 && fn_Array_Resize_Stride08) {
            fn_Array_Resize_Stride08(&self->_field40, count48);
        }
        if (count48 > 0 && fn_Array_Serialize_External_GraphicsMesh) {
            fn_Array_Serialize_External_GraphicsMesh(outer_ser, count48, self->_field40);
        }

        RZ_CHECK("Slot5 after arr40 (GraphicsMesh exrefs)");

        // ----- Single ExternalPtr at +0x70 (GraphicsMesh) -----
        if (fn_ExternalPtr_Serialize_GraphicsMesh) {
            fn_ExternalPtr_Serialize_GraphicsMesh(&self->_field70, outer_ser);
        }

        // ----- Atmosphere: Vector4 + 3 Colors + 5 Floats -----
        if (fn_Math_Vector4_Serialize) {
            fn_Math_Vector4_Serialize(reinterpret_cast<uint8_t*>(self) + 0x1B4, outer_ser);
        }
        if (fn_System_Color_Serialize) {
            fn_System_Color_Serialize(&self->ambientColor, outer_ser);
            fn_System_Color_Serialize(&self->sunColor,     outer_ser);
            fn_System_Color_Serialize(&self->skyColor,     outer_ser);
            // Color::Serialize from Runtime appears to write only the RGB
            // bytes and leave alpha at zero; Init defaults use 0xFF and the
            // renderer's uint32→Vector4 conversion treats the alpha byte as
            // the 4th component (which downstream lighting/fog math may
            // multiply against). Force opaque alpha so the deserialized
            // colors land in the same range as the Init defaults.
            self->ambientColor |= 0xFF000000;
            self->sunColor     |= 0xFF000000;
            self->skyColor     |= 0xFF000000;
        }
        if (fn_Serializer_Serialize_Float) {
            fn_Serializer_Serialize_Float(outer_ser, &self->fogStart,         nullptr);
            fn_Serializer_Serialize_Float(outer_ser, &self->fogEnd,           nullptr);
            fn_Serializer_Serialize_Float(outer_ser, &self->fogDensity,       nullptr);
            fn_Serializer_Serialize_Float(outer_ser, &self->sunIntensity,     nullptr);
            fn_Serializer_Serialize_Float(outer_ser, &self->ambientIntensity, nullptr);
        }

        // ----- v2-gated tail: Weather extPtr + +0x78 float + particle array
        //                       + particle resource exrefs ----------------
        // Maps to original 0x1400a3779..0x1400a3825 (the `if (1 < iVar3)` block).
        // We always run it since our outer is v2; the stream's version field
        // dictates serialize behavior internally.
        if (fn_ExternalPtr_Serialize_Weather) {
            // Weather externalPtr at +0x80 (FUN_1400196a0).
            fn_ExternalPtr_Serialize_Weather(&self->_field80, outer_ser);
        }
        if (fn_Serializer_Serialize_Float) {
            // Single float at +0x78 (the v2-only "scene-time" or similar).
            fn_Serializer_Serialize_Float(outer_ser, &self->_field78, nullptr);
        }

        // ----- Particle element array (+0x50, stride 0x14: 4 floats + 1 int) -----
        int count58 = static_cast<int>(reinterpret_cast<uintptr_t>(self->_field58) & 0xFFFFFFFF);
        g_fn_Serialize_Int(outer_ser, &count58, nullptr);
        if (count58 > 0 && fn_Array_Resize_Stride14) {
            fn_Array_Resize_Stride14(&self->_field50, count58);
        }
        // Per-element serialize (inlined in original; not a separate helper).
        if (count58 > 0 && self->_field50 && fn_Serializer_Serialize_Float) {
            uint8_t* base = reinterpret_cast<uint8_t*>(self->_field50);
            for (int i = 0; i < count58; ++i) {
                uint8_t* elem = base + static_cast<size_t>(i) * 0x14;
                fn_Serializer_Serialize_Float(outer_ser, reinterpret_cast<float*>(elem + 0x00), nullptr);  // offset.x
                fn_Serializer_Serialize_Float(outer_ser, reinterpret_cast<float*>(elem + 0x04), nullptr);  // offset.y
                fn_Serializer_Serialize_Float(outer_ser, reinterpret_cast<float*>(elem + 0x08), nullptr);  // offset.z
                fn_Serializer_Serialize_Float(outer_ser, reinterpret_cast<float*>(elem + 0x0C), nullptr);  // yaw
                g_fn_Serialize_Int(outer_ser, reinterpret_cast<int*>(elem + 0x10), nullptr);              // resIdx
            }
        }

        RZ_CHECK("Slot5 after particle elem array (+0x50)");

        // ----- Particle resource exrefs (+0x60, stride 0x08, FUN_14001bff0) -----
        int count68 = static_cast<int>(reinterpret_cast<uintptr_t>(self->_field68) & 0xFFFFFFFF);
        g_fn_Serialize_Int(outer_ser, &count68, nullptr);
        if (count68 > 0 && fn_Array_Resize_Stride08) {
            fn_Array_Resize_Stride08(&self->_field60, count68);
        }
        if (count68 > 0 && fn_Array_Serialize_External_Particle) {
            fn_Array_Serialize_External_Particle(outer_ser, count68, self->_field60);
        }

        static bool s_particleLogged = false;
        if (!s_particleLogged) {
            printf("[MenuScene::Slot5_Serialize] particles deserialized: "
                   "count58=%d (elem array) count68=%d (resource exrefs)\n",
                   count58, count68);
            fflush(stdout);
            s_particleLogged = true;
        }

        // 2026-05-02 diagnostic confirmed: even with Init-default atmosphere
        // (ambientColor=0xff595965, sunColor=0xffcbcbbc, etc.) screen stays
        // black. Atmosphere alignment is NOT the screen-black culprit. Bug is
        // downstream in the render path (clear color, depth state, sun light
        // dispatch, or Renderer::Draw consuming our reimpl's geometry).
        static bool s_atmosLogged = false;
        if (!s_atmosLogged) {
            const float* v4 = reinterpret_cast<const float*>(
                reinterpret_cast<uint8_t*>(self) + 0x1B4);
            printf("[MenuScene::Slot5_Serialize] atmosphere deserialized: "
                   "amb=0x%08x sun=0x%08x sky=0x%08x "
                   "fogStart=%.2f fogEnd=%.2f fogDensity=%.3f "
                   "sunInt=%.2f ambInt=%.2f "
                   "vec4_at_1b4=(%.3f,%.3f,%.3f,%.3f) "
                   "(arr30 count=%d arr40 count=%d)\n",
                   self->ambientColor, self->sunColor, self->skyColor,
                   self->fogStart, self->fogEnd, self->fogDensity,
                   self->sunIntensity, self->ambientIntensity,
                   v4[0], v4[1], v4[2], v4[3],
                   count38, count48);
            fflush(stdout);
            s_atmosLogged = true;
        }
    } else {
        static bool s_warnedTail = false;
        if (!s_warnedTail) {
            printf("[MenuScene::Slot5_Serialize] tail unwired "
                   "(outer_ctored=%d Ser_Int=%p) — atmosphere left at Init defaults\n",
                   outer_ctored ? 1 : 0,
                   (void*)g_fn_Serialize_Int);
            fflush(stdout);
            s_warnedTail = true;
        }
    }

    // (5) Dtor the outer Serializer.
    if (outer_ctored && g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(outer_ser);
    }
    RZ_CHECK("Slot5 exit");
    return;
}

// ============================================================
// STANDALONE_EXE: Factory + Vtable + REGISTER_TYPE
// ============================================================
// In standalone mode we own the type registration. The factory allocates
// 0x208 bytes (sizeof(MenuScene)) with 8-byte alignment and runs Init.
// Mirrors the StatGoal pattern in code/Game/Goal/statgoal.cpp.


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

    // Frustum ctor: arg order is (near, far, fovX, fovY). Original uses
    // near=1.0, far=100.0, fovX=fovY=90.0 (FUN_140098800).
    float fovX = 90.0f, fovY = 90.0f, nearZ = 1.0f, farZ = 100.0f;
    uint8_t frustumBuf[0x80] = {};
    fn_Volume_Frustum_Ctor(frustumBuf, &nearZ, &farZ, &fovX, &fovY);

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
    // FUN_140098800 Transform/Camera init — standalone already routes EnsureResolved()
    // to MenuScene_TransformInit_Standalone directly (the 0x140098800 ResolveAddr
    // literal lives in the DLL-only #else branch). Register it so the exe address
    // resolves to our reimpl too, keeping the Game-gap metric honest.
    RegisterExeAddr(0x140098800, reinterpret_cast<uintptr_t>(&MenuScene_TransformInit_Standalone));
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

