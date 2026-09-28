// code/Game/Game/menuscene.cpp
// Clean reimplementation of MenuScene game logic.
//
// MenuScene (0x208 bytes) -- main menu scene controller.
// Extends Object::External (0x20 base). Vtable at 0x14015ede0.
//
// ENDGAME B.3.8 block LOOP: a REAL class (code/Game/Game/MenuScene.h, field
// layout and retail slot map there) with a compiler-generated vtable. Gone:
// the 11-slot `g_MenuScene_Vtable_Standalone` (two over-read guards past
// retail's 9), its trampolines and stubs, the lazy EnsureResolved (a non-atomic
// first-call cache on the loader thread, B6-5: the External ctor/dtor dispatch
// slots it read are the compiler's base chain now) and the C entry points.
//
// Contains scene geometry (+0x28), dynamic arrays of scene objects
// (+0x30/+0x40/+0x50/+0x60), an external reference (+0x70),
// a weather reference (+0x80), an embedded transform at +0x88,
// atmosphere/lighting colors and fog parameters, plus two tree/map
// structures at +0x1C8 and +0x1E8.
//
// Decompiled from Application-x64.exe.

#include "Game/MenuScene.h"
#include "Game/WorldGeometry.h"   // caller batch CB6: +0x28's class
#include "engine_funcs.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "math/Math.h"
#include "system/String.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include "object/Internal.h" // Object::detail::InternalMemAlloc / InternalMemFree
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// Heap-overflow checkpoint (standalone diagnostic; no-op when
// BANISHED_REDZONE is unset).
#include "redzone_alloc.h"
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)
#define RZ_CHECK(tag) RedzoneAlloc_CheckLinks(tag)

// M3.5 Path B session 8 — top-level Geometry serializers used by Serialize.
extern "C" void Reimpl_TradeWork_impl_04d0(void* self, void* stream);
extern "C" void Reimpl_TradeWork_impl_f1a0(void* self, void* stream);

// FUN_14001b860 — the tree/map cleanup for +0x1C8 / +0x1E8 (game_leaf_helpers.cpp,
// registered at that address in reg_game_leaf.cpp). Called directly: the old
// lazily-resolved pointer (a non-atomic first-call cache, B6-5) resolved to it.
void Game_MenuSceneArrayDtor(void* self);

// Local mirror of FUN_140098800 (below; registered at that address).
static void MenuScene_TransformInit_Standalone(void* transformObj);

// One-shot log latches for Serialize (namespace scope, atomic: B6-5 — the scene
// loads on the loader thread).
namespace {
std::atomic<bool> s_serializeLogged{false};
std::atomic<bool> s_cameraLogged{false};
std::atomic<bool> s_particleLogged{false};
std::atomic<bool> s_atmosLogged{false};
std::atomic<bool> s_tailWarned{false};
}  // namespace

// ============================================================
// MenuScene::MenuScene -- Constructor
// ============================================================
// Original: 0x1400a3d70
//
// External() and the 0x14015ede0 stamp are the compiler's; then zero all
// fields from +0x20 to +0x80, initialize the embedded Transform at +0x88,
// set atmosphere defaults (colors, fog, intensities), zero the remaining
// fields from +0x1B8 to +0x200, and initialize two tree structures at
// +0x1C8 and +0x1E8.
MenuScene::MenuScene() {
    // Zero pointer/array fields (+0x20 through +0x80, skipping +0x78)
    _field20 = nullptr;   // +0x20: String (name)
    _field28 = nullptr;   // +0x28: Geometry* (scene object)
    _field30 = nullptr;   // +0x30: array data ptr
    _field38 = nullptr;   // +0x38: array count/cap
    _field40 = nullptr;   // +0x40: array data ptr
    _field48 = nullptr;   // +0x48: array count/cap
    _field50 = nullptr;   // +0x50: array data ptr
    _field58 = nullptr;   // +0x58: array count/cap
    _field60 = nullptr;   // +0x60: array data ptr
    _field68 = nullptr;   // +0x68: array count/cap
    _field70 = nullptr;   // +0x70: ExternalPtr (typed reference)
    _field80 = nullptr;   // +0x80: ExternalPtr (Weather)

    // Initialize embedded Transform at +0x88
    MenuScene_TransformInit_Standalone(&transform);

    // Atmosphere/lighting defaults (same as Weather initial values)
    ambientColor     = 0xff595965;
    sunColor         = 0xffcbcbbc;
    skyColor         = 0xffbfbff2;
    fogStart         = 90.0f;       // 0x42b40000
    fogEnd           = 225.0f;      // 0x43610000
    fogDensity       = 0.95f;       // 0x3f733333
    sunIntensity     = 120.0f;      // 0x42f00000

    // +0x1B0: ambientIntensity = 65.0f (0x42820000), packed as qword with +0x1B4 = 0
    ambientIntensity = 65.0f;
    memset(&_pad1B4, 0, 4);

    // Zero remaining fields
    _field1B8 = nullptr;  // +0x1B8
    _field1C0 = 0;        // +0x1C0

    // Tree/map at +0x1C8 (ptr, ptr, root, count)
    _field1C8 = nullptr;
    _field1D0 = nullptr;
    _field1D8 = nullptr;
    _field1E0 = 0;

    // Tree/map at +0x1E8 (same layout)
    _field1E8 = nullptr;
    _field1F0 = nullptr;
    _field1F8 = nullptr;
    _field200 = 0;
}

// ============================================================
// MenuScene::Construct (vtable +0x40)
// ============================================================
// Original: 0x14001b800 — placement-new of the class on `this` (MSVC's
// placement new null-checks `this`, as the old slot did).
Object::External* MenuScene::Construct() {
    return ::new (static_cast<void*>(this)) MenuScene();
}

// ============================================================
// MenuScene::~MenuScene (body 0x1400a3970) / DeletingDtor (slot 0, 0x14001b830)
// ============================================================
// Restores the vtable (the compiler's), destroys the Geometry at +0x28
// (scalar delete via its vtable), cleans up the tree structures at +0x1E8 and
// +0x1C8, frees the arrays at +0x60, +0x50, +0x40, +0x30, destroys the String
// at +0x20; External::~External is the compiler's chain.
MenuScene::~MenuScene() {
    // Destroy Geometry object at +0x28 via its vtable scalar deleting dtor.
    // Caller batch CB6: slot 0 of the proved WorldGeometry (Serialize builds
    // +0x28 with its ctor); still a virtual call, as retail's is.
    void* geom = _field28;
    if (geom) {
        delete static_cast<WorldGeometry*>(geom);  // flags=1 => free memory  B.3.8 bridge
    }

    // Cleanup tree/map at +0x1E8 and +0x1C8
    Game_MenuSceneArrayDtor(&_field1E8);
    Game_MenuSceneArrayDtor(&_field1C8);

    // Free array at +0x60, zero +0x60/+0x68
    if (_field60) Object::detail::InternalMemFree(_field60);
    _field60 = nullptr;
    _field68 = nullptr;

    // Free array at +0x50, zero +0x50/+0x58
    if (_field50) Object::detail::InternalMemFree(_field50);
    _field50 = nullptr;
    _field58 = nullptr;

    // Free array at +0x40, zero +0x40/+0x48
    if (_field40) Object::detail::InternalMemFree(_field40);
    _field40 = nullptr;
    _field48 = nullptr;

    // Free array at +0x30, zero +0x30/+0x38
    if (_field30) Object::detail::InternalMemFree(_field30);
    _field30 = nullptr;
    _field38 = nullptr;

    // Destroy String at +0x20
    // B.3.1 bridge: MenuScene's String field is still a POD member.
    reinterpret_cast<System::String*>(&_field20)->~String();
}

// ============================================================
// MenuScene::Reconstruct (vtable +0x30, Slot6)
// ============================================================
// Original: 0x14001b810. Destroys the object (slot 0, flags 0: no free) then
// re-constructs in place.
Object::External* MenuScene::Reconstruct() {
    this->~MenuScene();
    return ::new (static_cast<void*>(this)) MenuScene();
}

// [7] 0x140001640 — the shared body `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`:
// slot 0 with flags 0 (destroy, no free).
void MenuScene::Destruct() { this->~MenuScene(); }

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
bool MenuScene::Serialize(void* stream) {

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
    if (!s_serializeLogged.exchange(true)) {
        printf("[MenuScene::Slot5_Serialize] standalone Geometry-deserialize path "
               "(self=%p stream=%p)\n", (void*)this, stream);
        fflush(stdout);
    }

    // (1) Construct an OUTER Serializer (version=2) wrapping `stream`, then
    // String::Serialize on &_field20 to advance the stream cursor past
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
    if (stream) {
        ::new (static_cast<void*>(outer_ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 2);   // B.3.4 bridge
        outer_ctored = true;
        reinterpret_cast<System::String*>(&_field20)->Serialize(outer_ser);
    } else {
        printf("[MenuScene::Slot5_Serialize] no stream — "
               "skipping outer prelude (terrain will be missing)\n");
        fflush(stdout);
    }

    // (2) Allocate + ctor a Geometry container at +0x28 if needed.
    if (_field28 == nullptr) {
        if (!System::MemoryAllocator::_singleton) {
            printf("[MenuScene::Slot5_Serialize] MemAlloc unavailable — "
                   "skipping Geometry alloc (terrain will be missing)\n");
            fflush(stdout);
        } else {
            void* geom = System::MemoryAllocator::MemAlloc(0x230, 8);
            if (geom) {
                // Caller batch CB6: the real ctor (0x1400b00e0), where this went
                // through the fn_Geometry_Init door (Reimpl_Geometry_Init, which
                // placement-news the same class) and a zero-fill fallback for an
                // unresolved door.
                ::new (geom) WorldGeometry();
                _field28 = geom;
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
    // OOB fix.
    {
        if (_field28 && stream) {
            Reimpl_TradeWork_impl_04d0(_field28, stream);
            Reimpl_TradeWork_impl_f1a0(_field28, stream);
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
        void* xform = transform;   // +0x88 (caller batch CB6: the named field)
        // Transform::SerializeWrapper (exe 0x1400988c0) resolves to a
        // NOT_IMPLEMENTED stub via the address registry — DON'T call it, use
        // the inline path.
        // ENDGAME B.3.4a closing: the ctor/dtor/float readiness guard is gone and
        // with it the else arm, which existed only to report "the Serializer
        // primitives did not resolve" — a state a real IO::Serializer cannot be in.
        //
        // Inline the FUN_1400988c0 body: inner Serializer (v=0) wraps
        // stream; serialize Math::Transform (28B) + Math::Vector3 (12B at
        // +0x1c) + 3 floats (+0x28, +0x2C, +0x30); dtor.
        uint64_t inner_ser[3] = {0, 0, 0};
        ::new (static_cast<void*>(inner_ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
        // ?Serialize@Transform@Math@@QEAAXAEAVSerializer@IO@@@Z  0x180059b90
        reinterpret_cast<Math::Transform*>(xform)->Serialize(inner_ser);
        // ?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z    0x1800150e0
        reinterpret_cast<Math::Vector3*>(
            reinterpret_cast<uint8_t*>(xform) + 0x1C)->Serialize(inner_ser);
        reinterpret_cast<IO::Serializer*>(inner_ser)->Serialize(
            *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(xform) + 0x28),
            nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(inner_ser)->Serialize(
            *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(xform) + 0x2C),
            nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(inner_ser)->Serialize(
            *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(xform) + 0x30),
            nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(inner_ser)->~Serializer();   // B.3.4 bridge
        if (!s_cameraLogged.exchange(true)) {
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
    if (outer_ctored) {
        // ----- Array #1 (+0x30, stride 0x20) -----
        int count38 = static_cast<int>(reinterpret_cast<uintptr_t>(_field38) & 0xFFFFFFFF);
        reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(count38, nullptr);   // B.3.4 bridge (cast dropped: count38 is int)
        if (count38 > 0) {
            Reimpl_Array_Resize_Stride20(&_field30, count38);
        }
        if (count38 > 0) {
            Reimpl_Array_Serialize_Stride20(outer_ser, static_cast<uint32_t>(count38),
                                        _field30);
        }

        RZ_CHECK("Slot5 after arr30 (stride0x20 instances)");

        // ----- Array #2 (+0x40, stride 0x08, GraphicsMesh exrefs) -----
        int count48 = static_cast<int>(reinterpret_cast<uintptr_t>(_field48) & 0xFFFFFFFF);
        reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(count48, nullptr);   // B.3.4 bridge (cast dropped: count48 is int)
        if (count48 > 0) {
            Reimpl_Array_Resize_Stride08(&_field40, count48);
        }
        if (count48 > 0) {
            Reimpl_Array_Serialize_External_GraphicsMesh(outer_ser, count48, _field40);
        }

        RZ_CHECK("Slot5 after arr40 (GraphicsMesh exrefs)");

        // ----- Single ExternalPtr at +0x70 (GraphicsMesh) -----
        Reimpl_ExternalPtr_Serialize_GraphicsMesh(&_field70, outer_ser);

        // ----- Atmosphere: Vector4 + 3 Colors + 5 Floats -----
        // ?Serialize@Vector4@Math@@QEAAXAEAVSerializer@IO@@@Z  retail 0x18001d360
        reinterpret_cast<Math::Vector4*>(_pad1B4)->Serialize(outer_ser);   // +0x1B4 (the Vector4 starts at _pad1B4)
        reinterpret_cast<System::Color*>(&ambientColor)->Serialize(*reinterpret_cast<IO::Serializer*>(outer_ser));   // B.3.4b bridge
        reinterpret_cast<System::Color*>(&sunColor)->Serialize(*reinterpret_cast<IO::Serializer*>(outer_ser));   // B.3.4b bridge
        reinterpret_cast<System::Color*>(&skyColor)->Serialize(*reinterpret_cast<IO::Serializer*>(outer_ser));   // B.3.4b bridge
        // Color::Serialize from Runtime appears to write only the RGB
        // bytes and leave alpha at zero; Init defaults use 0xFF and the
        // renderer's uint32→Vector4 conversion treats the alpha byte as
        // the 4th component (which downstream lighting/fog math may
        // multiply against). Force opaque alpha so the deserialized
        // colors land in the same range as the Init defaults.
        ambientColor |= 0xFF000000;
        sunColor     |= 0xFF000000;
        skyColor     |= 0xFF000000;
        reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(fogStart,         nullptr);   // B.3.4 bridge (cast dropped: fogStart is float)
        reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(fogEnd,           nullptr);   // B.3.4 bridge (cast dropped: fogEnd is float)
        reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(fogDensity,       nullptr);   // B.3.4 bridge (cast dropped: fogDensity is float)
        reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(sunIntensity,     nullptr);   // B.3.4 bridge (cast dropped: sunIntensity is float)
        reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(ambientIntensity, nullptr);   // B.3.4 bridge (cast dropped: ambientIntensity is float)

        // ----- v2-gated tail: Weather extPtr + +0x78 float + particle array
        //                       + particle resource exrefs ----------------
        // Maps to original 0x1400a3779..0x1400a3825 (the `if (1 < iVar3)` block).
        // We always run it since our outer is v2; the stream's version field
        // dictates serialize behavior internally.
        // Weather externalPtr at +0x80 (FUN_1400196a0).
        Reimpl_ExternalPtr_Serialize_Weather(&_field80, outer_ser);
        // Single float at +0x78 (the v2-only "scene-time" or similar).
        reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(_field78, nullptr);   // B.3.4 bridge (cast dropped: _field78 is float)

        // ----- Particle element array (+0x50, stride 0x14: 4 floats + 1 int) -----
        int count58 = static_cast<int>(reinterpret_cast<uintptr_t>(_field58) & 0xFFFFFFFF);
        reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(count58, nullptr);   // B.3.4 bridge (cast dropped: count58 is int)
        if (count58 > 0) {
            Reimpl_Array_Resize_Stride14(&_field50, count58);
        }
        // Per-element serialize (inlined in original; not a separate helper).
        if (count58 > 0 && _field50) {
            uint8_t* base = reinterpret_cast<uint8_t*>(_field50);
            for (int i = 0; i < count58; ++i) {
                uint8_t* elem = base + static_cast<size_t>(i) * 0x14;
                reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(*reinterpret_cast<float*>(elem + 0x00), nullptr);   // B.3.4 bridge  offset.x
                reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(*reinterpret_cast<float*>(elem + 0x04), nullptr);   // B.3.4 bridge  offset.y
                reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(*reinterpret_cast<float*>(elem + 0x08), nullptr);   // B.3.4 bridge  offset.z
                reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(*reinterpret_cast<float*>(elem + 0x0C), nullptr);   // B.3.4 bridge  yaw
                reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(*reinterpret_cast<int*>(elem + 0x10), nullptr);   // B.3.4 bridge  resIdx
            }
        }

        RZ_CHECK("Slot5 after particle elem array (+0x50)");

        // ----- Particle resource exrefs (+0x60, stride 0x08, FUN_14001bff0) -----
        int count68 = static_cast<int>(reinterpret_cast<uintptr_t>(_field68) & 0xFFFFFFFF);
        reinterpret_cast<IO::Serializer*>(outer_ser)->Serialize(count68, nullptr);   // B.3.4 bridge (cast dropped: count68 is int)
        if (count68 > 0) {
            Reimpl_Array_Resize_Stride08(&_field60, count68);
        }
        if (count68 > 0) {
            Reimpl_Array_Serialize_External_Particle(outer_ser, count68, _field60);
        }

        if (!s_particleLogged.exchange(true)) {
            printf("[MenuScene::Slot5_Serialize] particles deserialized: "
                   "count58=%d (elem array) count68=%d (resource exrefs)\n",
                   count58, count68);
            fflush(stdout);
        }

        // 2026-05-02 diagnostic confirmed: even with Init-default atmosphere
        // (ambientColor=0xff595965, sunColor=0xffcbcbbc, etc.) screen stays
        // black. Atmosphere alignment is NOT the screen-black culprit. Bug is
        // downstream in the render path (clear color, depth state, sun light
        // dispatch, or Renderer::Draw consuming our reimpl's geometry).
        if (!s_atmosLogged.exchange(true)) {
            const float* v4 = reinterpret_cast<const float*>(_pad1B4);   // +0x1B4
            printf("[MenuScene::Slot5_Serialize] atmosphere deserialized: "
                   "amb=0x%08x sun=0x%08x sky=0x%08x "
                   "fogStart=%.2f fogEnd=%.2f fogDensity=%.3f "
                   "sunInt=%.2f ambInt=%.2f "
                   "vec4_at_1b4=(%.3f,%.3f,%.3f,%.3f) "
                   "(arr30 count=%d arr40 count=%d)\n",
                   ambientColor, sunColor, skyColor,
                   fogStart, fogEnd, fogDensity,
                   sunIntensity, ambientIntensity,
                   v4[0], v4[1], v4[2], v4[3],
                   count38, count48);
            fflush(stdout);
        }
    } else {
        if (!s_tailWarned.exchange(true)) {
            printf("[MenuScene::Slot5_Serialize] tail unwired "
                   "(outer_ctored=%d) — atmosphere left at Init defaults\n",
                   outer_ctored ? 1 : 0);
            fflush(stdout);
        }
    }

    // (5) Dtor the outer Serializer.
    if (outer_ctored) {
        reinterpret_cast<IO::Serializer*>(outer_ser)->~Serializer();   // B.3.4 bridge
    }
    RZ_CHECK("Slot5 exit");
    return true;   // retail returns AL=1
}

// Local mirror of FUN_140098800 (exe 0x140098800). Composes the same
// Runtime DLL primitives (Math::Transform::Transform + Volume::Frustum +
// Graphics::Camera) that menudialogloopt.cpp's MenuScene_Helper_8800 uses.
// Replicated here so the menuscene.cpp TU is self-contained for standalone.
static void MenuScene_TransformInit_Standalone(void* transformObj) {
    if (!transformObj) return;

    *reinterpret_cast<Math::Transform*>(transformObj) = Math::Transform();

    // Zero scalars at +0x1C (qword) and +0x24 (dword) per original disasm.
    *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(transformObj) + 0x1C) = 0;
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(transformObj) + 0x24) = 0;

    Math::Transform3 ident = Math::Transform3::Identity();

    // Frustum ctor: arg order is (near, far, fovX, fovY). Original uses
    // near=1.0, far=100.0, fovX=fovY=90.0 (FUN_140098800).
    float fovX = 90.0f, fovY = 90.0f, nearZ = 1.0f, farZ = 100.0f;
    uint8_t frustumBuf[0x80] = {};
    RtVolume_Frustum_Ctor_Perspective(reinterpret_cast<::Frustum*>(frustumBuf), reinterpret_cast<const Math::Scalar*>(&nearZ), reinterpret_cast<const Math::Scalar*>(&farZ), reinterpret_cast<const Math::Scalar*>(&fovX), reinterpret_cast<const Math::Scalar*>(&fovY));

    // Camera lives at transform+0x34 (per the embedded Camera at +0x34 inside
    // the 0x10C-byte Transform block).
    void* cameraObj = reinterpret_cast<uint8_t*>(transformObj) + 0x34;
    RtGraphics_Camera_Ctor(reinterpret_cast<RtGraphics::Camera*>(cameraObj), reinterpret_cast<const ::Frustum*>(frustumBuf), reinterpret_cast<const Math::Transform3*>(&ident), true);
}

// ============================================================
// Reflection: create / createInPlace, the registration
// ============================================================
void* MenuScene::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MenuScene), 8);
    if (!p) return nullptr;
    Object::Base* obj = ::new (p) MenuScene();
    printf("[MenuScene::Factory] allocated %p (size=0x%x)\n",
           p, (unsigned)sizeof(MenuScene));
    return obj;
}
void* MenuScene::CreateInPlaceObject(void* memory) {
    if (!memory) return nullptr;
    return static_cast<Object::Base*>(::new (memory) MenuScene());
}


DEFINE_EXE_TYPE(MenuScene, "Object::External")
