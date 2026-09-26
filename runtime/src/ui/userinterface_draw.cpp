// ============================================================================
// runtime/src/ui/userinterface_draw.cpp — Wave 10 lane UIDRAW
// ?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z    retail 0x180121890
// ============================================================================
//
// PICKED UP FROM LANE UI1, WHICH LEFT THIS ROW WITH "NOTHING SURVIVES BUT SIZE".
// Read docs/runtime_port/WAVE10_LANE_UI1.md §3 first: it retired the three
// recorded blockers ("split brain", "allocator NO-GO", "no oracle") with
// evidence, ported and detoured the sibling `Update` (0x1801213c0), and handed
// this row the prologue bytes, the 13-byte patch length and the persistent
// write set. This file is that row closed. Its sibling is
// runtime/src/ui/userinterface_frame.cpp and it deliberately mirrors its shape.
//
// ---------------------------------------------------------------------------
// WHAT THE FUNCTION DOES (614 instructions, read off the DISASSEMBLY)
// ---------------------------------------------------------------------------
//   1. Compute the UI-space screen size from Video::Device::_singleton and
//      `_uiScale`, and reset `_clipRegion` to {0, 0, w-1, h-1}.
//   2. Force the Video::Context's `_color` to opaque white if it is not already,
//      marking kDirtyMaterialConstants.
//   3. Build an ORTHOGRAPHIC Volume::Frustum covering the UI rect, construct a
//      Graphics::Camera from it and an IDENTITY Math::Transform3, and push the
//      camera's world matrix + projection into the context.
//   4. Build a UI::SpriteContext on the stack and `Begin` it.
//   5. Walk the dialog list once per LAYER, painting each dialog's backdrop
//      sprite (letterboxed to the sprite's aspect ratio) on the first layer
//      pass and then calling the dialog's root element's vtable Draw, draining
//      the deferred batch each time.
//   6. Paint the fullscreen fade quad, then the mouse cursor.
//   7. Flush, close the sprite capture, restore the context's scissor to the
//      whole render target, and free the four buffers the SpriteContext owns.
//
// ---------------------------------------------------------------------------
// FACTS GHIDRA'S DECOMPILED C DROPS (reference_ghidra_c_output_vs_disassembly)
// ---------------------------------------------------------------------------
//
//  * **A DROPPED DEREFERENCE THAT CHANGES THE VALUE.** The layer pre-pass at
//    0x180121d33 does
//        MOV ECX,[RDX+0x20] ; SHR ECX,4 ; TEST CL,1 ; CMOVNZ ...
//        MOV RCX,[RDX+0x18] ; TEST RCX,RCX ; JZ +5 ; MOV ECX,[RCX+0x34]
//        CMP ECX,R8D
//    When `dialog->_desc` (+0x18) is NULL the `MOV ECX,[RCX+0x34]` is SKIPPED
//    and ECX still holds `flags >> 4` — so the layer compared is the SHIFTED
//    FLAGS WORD, not zero and not a layer. The very next block re-loads +0x18
//    and, on the same null, assigns `XOR R8D,R8D` = 0 instead. Two different
//    answers for the same null on two consecutive instructions. A readable
//    rewrite unifies them and is wrong.
//
//  * **`isOrthographic` is written as a BYTE and copied as a DWORD.** Draw
//    stores `MOV byte [RBP+0x74],1` (Frustum+0x60) and the Camera ctor copies
//    25 DWORDS (0x1800fe99a..0x1800fe9fd), so the three padding bytes above it
//    are stack garbage in retail as well. Not modelled, not initialised.
//
//  * **The `>> 1` halvings are ARITHMETIC (SAR), not logical.** 0x180121e44,
//    0x180121e48, 0x180121e68, 0x180121e70. On a negative width they matter.
//
//  * **`-(float)width` is a SIGN-BIT XOR** (`XORPS XMM0,[0x1801617c0]`, four
//    copies of 0x80000000), and the plane-4 constant at 0x18016174c is
//    **-0.0f**, not 0.0f. Both preserved bit-exactly by NegBits() below rather
//    than by a C negation, because the width==0 case is the difference between
//    +0.0f and -0.0f in a struct the Camera ctor copies verbatim.
//
//  * **THE `.rdata` CONSTANTS, read out of the shipped bytes:**
//        0x180161694 = 0x3f000000 =  0.5f   (the letterbox rounding term)
//        0x18016169c = 0x3f800000 =  1.0f
//        0x18016174c = 0x80000000 = -0.0f
//        0x18016175c = 0xbf800000 = -1.0f
//        0x180161718 = 0x437f0000 =  255.0f (fade alpha scale)
//        0x1801617c0 = four 0x80000000      (the sign mask)
//
//  * **SpriteContext+0x50 is NEVER INITIALISED.** 0x180121cd8 takes its ADDRESS
//    (`LEA RAX,[RBP-0x30]`) and stores that into a home slot; the qword itself
//    is left as stack garbage. Every other field of the object is written.
//    Reproduced by leaving it uninitialised — see the storage declaration.
//
//  * **Four unguarded dereferences kept faithful** (CLAUDE.md, "Don't 'fix' a
//    faithful crash with a null check"): `[[node]+0x18]+0x40` at 0x180121f28,
//    `[[dialog+0x18]]+0x30` at 0x180121e80, `[[cursor+0x28]+0x20]+0x1c/0x20` at
//    0x180122183, and the element root `[[dialog+0x50]]` at 0x180121f52.
//
// ---------------------------------------------------------------------------
// THE ORACLE, AND — SAID FIRST — WHAT IT DOES NOT COVER
// ---------------------------------------------------------------------------
// `Draw`'s PRODUCT is submitted geometry. A predictive oracle that suppresses
// `AddSpriteDraw` / `Flush` / the element vtable Draw verifies NONE of it. What
// it does verify is the function's entire PERSISTENT SCALAR write set, which is
// exactly ten locations:
//
//   PREDICTED AND COMPARED
//     this+0x0fc..+0x10c  `_clipRegion` — determined by Video::Device + _uiScale
//                          and written BEFORE the first call, so no suppressed
//                          callee can move it.
//     ctx +0x408..+0x417  `_color` = opaque white — also written before the
//                          first call. `Flush` writes `_scissor`, never
//                          `_color` (checked in spritecontext_flush.cpp).
//
//   PREDICTED BUT DECLARED UNVERIFIABLE (`Exclude`, so it counts as such)
//     ctx +0x010          `_dirty`. We predict our own `|= 0x1000`, but
//                          `SetCamera` ORs kDirtyGlobalConstants (0x800) and
//                          `Flush` ORs kDirtyScissor (0x400) into the SAME word
//                          while suppressed, so the final value is not ours.
//
//   ABSTAINED FROM ENTIRELY IN PREDICT MODE
//     ctx +0x1e0..+0x1ef  `_scissor`. The tail's reset is GATED on the current
//                          scissor, and `SpriteContext::Flush` overwrites it
//                          (spritecontext_flush.cpp:502-508) on every batch.
//                          With Flush suppressed the branch condition is
//                          UNKNOWABLE, so the body abstains rather than guessing
//                          — the same rule lane UI1 applied to `Update`'s
//                          drag-drop block. A predictor that guesses turns a
//                          coverage hole into a false red.
//
// EFFECTIVENESS IS THE HONEST WEAKNESS HERE AND IT IS STATED, NOT BURIED.
// Both compared regions are IDEMPOTENT in a steady frame: once `_clipRegion`
// matches the resolution and `_color` is white, every later frame predicts a
// value that is already in memory and `changed=` stops moving. `wrong=0` over
// a thousand such rounds is a weak statement. The lane doc reports `changed=`
// beside `rounds=` for exactly that reason, and the negative control is scored
// against the rounds that HAD an armable subject, never against the total
// (reference_probe_needs_an_effectiveness_control.md).
//
// The complement for the geometry is a live in-game run with the DETOUR ON —
// if the layer walk, the letterbox arithmetic or the batch drain were wrong the
// UI would be visibly wrong or absent. That is a functional check, not an
// oracle, and the lane doc says so in those words.
//
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_sink.h"
#include "../../types/ui/UserInterface.h"
#include "../../types/ui/SpriteContext.h"
#include "../../types/video/Context.h"
#include "../../types/graphics/Camera.h"
#include "../../types/ui/SpriteCapture.h"
#include "../../types/ui/Element.h"
// ENDGAME B.3.6 block E: element_common.h carries the same file-scope
// `Module` / `ResolveAddr` declaration every other runtime/src file uses (the
// address door lives in standalone/), and the RULE B6-5 atomic door caches.
#include "element_common.h"

#include <atomic>

namespace {

using UI::UserInterface;
using UI::SpriteContext;
using UI::SpriteDraw;

// ---- data slots -------------------------------------------------------------
// Through RT_DATA_SLOT, which never caches a null: a slot resolved before the
// mint must be able to retry rather than freeze the DLL's answer for the
// process (reference_resolveexport_phantom_names.md).
//
// ENDGAME B.3.6 (RULE B6-5): namespace-scope atomic slots, not RT_DATA_SLOT /
// RT_EXPORT_ONCE function-local statics — the member body runs on UI objects
// the loader thread constructs.
std::atomic<void*> g_videoDeviceSlot{nullptr};
std::atomic<void*> g_inputDeviceSlot{nullptr};
std::atomic<void*> g_inputConfigSlot{nullptr};
std::atomic<void*> g_cameraCtorSlot{nullptr};
std::atomic<void*> g_memFreeSlot{nullptr};
std::atomic<void*> g_spriteDrawSetSize0Slot{nullptr};
inline void** VideoDeviceSingleton() {          // 0x1801fe7a8
    return static_cast<void**>(
        rtuielem::AtomicExport(g_videoDeviceSlot, "?_singleton@Device@Video@@0PEAV12@EA"));
}
inline void** InputDeviceSingleton() {          // 0x1801fe770
    return static_cast<void**>(
        rtuielem::AtomicExport(g_inputDeviceSlot, "?_singleton@Device@Input@@0PEAV12@EA"));
}
inline void** InputConfigSingleton() {          // 0x1801fe768
    return static_cast<void**>(
        rtuielem::AtomicExport(g_inputConfigSlot, "?_singleton@Configuration@Input@@0PEAV12@EA"));
}

// ---- the callees ------------------------------------------------------------
// SEVEN OF THE EIGHT ARE EXPORTED, verified against the shipped export
// directory with pefile rather than assumed — `AEAA` means PRIVATE, not
// unexported, and both `Flush` and `AddSpriteDraw` are `AEAA` names that the DLL
// exports all the same (reference_aeaa_is_private_not_unexported.md).
const char* const kCameraCtor  = "??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z";
const char* const kBegin       = "?Begin@SpriteContext@UI@@QEAAXMPEAVSpriteCapture@2@PEAVViewport@Video@@@Z";
const char* const kFlush       = "?Flush@SpriteContext@UI@@AEAAXXZ";
const char* const kAddSprite   = "?AddSpriteDraw@SpriteContext@UI@@AEAAXAEBUSpriteDraw@12@@Z";
const char* const kEndSprites  = "?EndAddingSprites@SpriteCapture@UI@@QEAAXXZ";
const char* const kMemFree     = "?MemFree@MemoryAllocator@System@@SAXPEAX@Z";

// THE ONE ADDRESS DOOR, declared rather than hidden.
//
// 0x1800783d0 is `Collection::Array<UI::SpriteDraw>::SetSize(0)`, the deferred
// batch's clear. It is the ONLY one of the eight callees absent from the export
// table (checked with pefile), so there is no name door for it. It is however
// ALREADY PORTED AND REGISTERED at that address by Wave 9 lane UIF
// (runtime/src/collection/array.cpp:1638), and `ResolveAddr(Module::Runtime, …)`
// consults the port registry FIRST — so this door resolves to OUR body, not the
// DLL's (reference_resolveaddr_consults_port_registry_first.md). It is still
// recorded as a cutover obligation rather than left for a residue run to
// rediscover (reference_by_address_door_dies_at_cutover.md).
constexpr uintptr_t kSpriteDrawSetSize0Addr = 0x1800783d0;

using CameraCtorFn = void* (*)(void*, const Frustum*, const Transform3*, bool);
using BeginFn      = void  (*)(void*, float, void*, void*);
using FlushFn      = void  (*)(void*);
using AddSpriteFn  = void  (*)(void*, const SpriteDraw*);
using EndSpritesFn = void  (*)(void*);
using FreeFn       = void  (*)(void*);
using SetSize0Fn   = void  (*)(void*);
using ElemDrawFn   = void  (*)(void*, void*, uint8_t);

// A dialog-list node: value, prev, next. Same shape lane UI1 pinned from the
// drain's own stores; `Draw` only ever walks it forwards through `next`.
struct Node { void* value; Node* prev; Node* next; };
static_assert(sizeof(Node) == 0x18, "LinkedList node is value/prev/next");

// Bit-exact float negation. Retail negates through `XORPS` against four copies
// of 0x80000000 (0x1801617c0), which turns +0.0f into -0.0f; a C `-x` is
// permitted to do the same but the whole point of a byte-faithful port is not
// to depend on that permission. The value lands in a Frustum the Camera ctor
// copies dword by dword, so the sign of a zero is observable.
inline float NegBits(float v) {
    uint32_t b;
    __builtin_memcpy(&b, &v, 4);
    b ^= 0x80000000u;
    float r;
    __builtin_memcpy(&r, &b, 4);
    return r;
}

// The sprite-table entry stride and the two size fields, read off
// 0x180121df8 (`IMUL RCX,RCX,0x38`) and 0x180121e00 / 0x180121e1c.
constexpr int32_t kSpriteEntryStride = 0x38;
constexpr int32_t kSpriteEntryWidth  = 0x1c;
constexpr int32_t kSpriteEntryHeight = 0x20;

// ============================================================================
// The body, written against the live sink (rt_sink.h).
// ============================================================================
// `out` is a live sink: every write is a store, every `Call` returns true.
//
// EVERY WRITE INTO `this` OR INTO THE CONTEXT GOES THROUGH `out`. Writes into
// the SpriteContext, the Camera, the Frustum and the SpriteDraw records do NOT:
// those are this frame's own stack, they are ours in both modes, and recording
// them would be recording a prediction about memory retail never shares.
} // namespace

// ENDGAME B.3.6 block E: the body is the member.
void UI::UserInterface::Draw(Video::Context& context) {
    RtSink out;                                                 // the live sink
    void* const ctx = &context;
    uint8_t* const S = reinterpret_cast<uint8_t*>(this);
    uint8_t* const C = reinterpret_cast<uint8_t*>(ctx);

    // ---- 0x180121907: UI-space screen size -------------------------------
    uint8_t* const vdev  = static_cast<uint8_t*>(*VideoDeviceSingleton());
    const float    scale = *reinterpret_cast<float*>(S + 0xdc);
    const int32_t  width = static_cast<int32_t>(
        static_cast<float>(*reinterpret_cast<int32_t*>(vdev + 4)) * scale);
    const int32_t  height = static_cast<int32_t>(
        static_cast<float>(*reinterpret_cast<int32_t*>(vdev + 8)) * scale);

    // ---- 0x180121942: force the context colour to opaque white ------------
    // Four UCOMISS/JP/JNZ pairs: ANY inequality, and an unordered compare
    // (a NaN in the colour), takes the write path. `!(a==1 && b==1 && c==1 &&
    // d==1)` reproduces that including the NaN case, because `NaN == 1.0f` is
    // false and so the negation is true.
    {
        const float* const col = reinterpret_cast<const float*>(C + 0x408);
        if (!(col[0] == 1.0f && col[1] == 1.0f && col[2] == 1.0f && col[3] == 1.0f)) {
            out.U32(C + 0x408, 0x3f800000u, "Context::_color.x = 1.0f");
            out.U32(C + 0x40c, 0x3f800000u, "Context::_color.y = 1.0f");
            out.U32(C + 0x410, 0x3f800000u, "Context::_color.z = 1.0f");
            out.U32(C + 0x414, 0x3f800000u, "Context::_color.w = 1.0f");
            out.U32(C + 0x10, out.ReadU32(C + 0x10) | RtVideo::kDirtyMaterialConstants,
                    "Context::_dirty |= kDirtyMaterialConstants");
        }
    }

    // ---- 0x1801219c0: the clip region ------------------------------------
    // One QWORD clears x and y together (0x1801219c0), then two DWORDs.
    out.U64(S + 0x0fc, 0, "_clipRegion.x/y = 0 (one qword)");
    out.U32(S + 0x104, static_cast<uint32_t>(width - 1),  "_clipRegion.w = width-1");
    out.U32(S + 0x108, static_cast<uint32_t>(height - 1), "_clipRegion.h = height-1");

    // ---- 0x180121a3f: the orthographic UI frustum -------------------------
    // Six planes {normal.xyz, d}. Together they are the box
    //   0 <= x <= width,  0 <= y <= 1,  0 <= z <= height
    // i.e. a screen-sized slab one unit deep, and `isOrthographic` says so.
    // The bool is stored BEFORE the floats in retail; irrelevant for a local,
    // recorded because the write order is the only thing that could ever make
    // it relevant.
    Frustum fr;
    fr.isOrthographic = true;                                   // +0x60, byte
    fr.planes[0] = {  0.0f,  1.0f,  0.0f,  0.0f };
    fr.planes[1] = {  0.0f, -1.0f,  0.0f, -1.0f };
    fr.planes[2] = {  1.0f,  0.0f,  0.0f,  0.0f };
    fr.planes[3] = { -1.0f,  0.0f,  0.0f,  NegBits(static_cast<float>(width)) };
    fr.planes[4] = {  0.0f,  0.0f, -1.0f,  NegBits(0.0f) };     // -0.0f, 0x18016174c
    fr.planes[5] = {  0.0f,  0.0f,  1.0f,  static_cast<float>(height) };

    // ---- 0x180121ae8: identity camera-to-world ----------------------------
    Transform3 t3;
    t3.axisX       = { 1.0f, 0.0f, 0.0f };
    t3.axisY       = { 0.0f, 1.0f, 0.0f };
    t3.axisZ       = { 0.0f, 0.0f, 1.0f };
    t3.translation = { 0.0f, 0.0f, 0.0f };

    // Uninitialised, exactly like retail's `RBP+0x80` local: the ctor fills
    // 0x00..0xd4 and `SetFrustum` fills the projection.
    RtGraphics::Camera cam;

    // `Graphics::Camera`'s ctor STAYS A DOOR (B4b-2, B35_RULES leave list), so
    // it is reached by name through an atomic slot (RULE B6-5).
    CameraCtorFn camCtor = reinterpret_cast<CameraCtorFn>(
        rtuielem::AtomicExport(g_cameraCtorSlot, kCameraCtor));
    if (out.Call("Graphics::Camera::Camera(frustum, identity, false)",
                 (const void*)camCtor))
        camCtor(&cam, &fr, &t3, false);

    // ---- 0x180121b00: Transform3::AsMatrix, inlined -----------------------
    // Rows from the three basis vectors and the translation, w = 0/0/0/1.
    // The 1.0f in m[15] is the SAME XMM14 as the frustum's, not a fresh
    // constant load — noted because it is why the constant appears once.
    {
        const float* const ctw = reinterpret_cast<const float*>(&cam.cameraToWorld);
        Matrix view;
        view.m[0]  = ctw[0];  view.m[1]  = ctw[1];  view.m[2]  = ctw[2];  view.m[3]  = 0.0f;
        view.m[4]  = ctw[3];  view.m[5]  = ctw[4];  view.m[6]  = ctw[5];  view.m[7]  = 0.0f;
        view.m[8]  = ctw[6];  view.m[9]  = ctw[7];  view.m[10] = ctw[8];  view.m[11] = 0.0f;
        view.m[12] = ctw[9];  view.m[13] = ctw[10]; view.m[14] = ctw[11]; view.m[15] = 1.0f;

        // Retail copies the projection into its own scratch and passes THAT,
        // so the argument is a copy, not a pointer into the camera.
        Matrix proj = cam.projection;

        // ENDGAME B.3.5: `?SetCamera@Context@Video@@QEAAXAEBVMatrix@Math@@0@Z`
        // (0x18001a150) is a member of the real `Video::Context`. The
        // `out.Call(label, target)` wrapper went with the door pointer it
        // labelled: `RtSink::Call` ignores every argument and returns true
        // unconditionally (rt_sink.h), so it guarded nothing and dropping it
        // changes no behaviour.
        reinterpret_cast<Video::Context*>(ctx)->SetCamera(view, proj);  // B.3.5 bridge
    }

    // ---- 0x180121cad: the SpriteContext, built on the stack ---------------
    // 0xa0 rather than sizeof(SpriteContext) (0x88): the header states 0x88 as
    // a LOWER BOUND proven by Begin's store at +0x80, not as a measured sizeof.
    // Retail's frame has unused slack above the object; giving ours the same
    // slack costs nothing and is the safe side of an unproven size.
    // DELIBERATELY UNINITIALISED as a whole — retail writes each field, and
    // +0x50 it never writes at all.
    alignas(16) uint8_t scStorage[0xa0];
    SpriteContext* const sc = reinterpret_cast<SpriteContext*>(scStorage);

    sc->_scale        = 1.0f;                       // 0x180121cad, as 0x3f800000
    sc->_currentSheet = nullptr;
    sc->_draws._data     = nullptr;                 // 0x180121cb8
    sc->_draws._size     = 0;                       // 0x180121cbc, one qword with
    sc->_draws._capacity = 0;                       //              _capacity
    sc->_scissorX = 0; sc->_scissorY = 0;           // 0x180121cc3, one MOVDQA
    sc->_scissorW = 0; sc->_scissorH = 0;
    sc->_context     = ctx;                         // 0x180121cc8
    sc->_deferring   = 0;                           // 0x180121ccc
    sc->_deferred._data     = nullptr;              // 0x180121cd0
    sc->_deferred._size     = 0;                    // 0x180121cd4
    sc->_deferred._capacity = 0;
    // +0x50 (RBP-0x30): retail takes its ADDRESS at 0x180121cd8 and never
    // writes the qword. Left uninitialised on purpose.
    std::memset(scStorage + 0x58, 0, 0x20);         // +0x58..+0x77, four qwords
                                                    // (0x180121ce3..0x180121cef)

    // ---- 0x180121d0e: Begin ------------------------------------------------
    // ENDGAME B.3.6: `SpriteContext::Begin` / `Flush` / `AddSpriteDraw` are
    // members of the real class (block D); the three RT_EXPORT_ONCE doors that
    // used to sit here are direct calls now.
    sc->Begin(*reinterpret_cast<float*>(S + 0xdc),
              *reinterpret_cast<UI::SpriteCapture**>(S + 0x110),
              reinterpret_cast<Video::Viewport*>(S + 0x118));

    // A local lambda for retail's repeated "switch the batch key" preamble,
    // which appears FIVE times verbatim (0x180121e96, 0x180121f79, 0x180122084,
    // 0x180122168, and inside the deferred drain). Written once because five
    // copies of the same four instructions is how a transcription error hides.
    auto selectSheet = [&](void* sheet) {
        if (sc->_currentSheet != sheet) {
            if (sc->_currentSheet != nullptr) {
                sc->Flush();                            // batch key change
            }
            sc->_currentSheet = sheet;
        }
    };

    // ---- 0x180121d1b: the layer pre-pass -----------------------------------
    // Two things at once: find the LAST dialog whose flags bit 4 is set (the
    // "topmost" barrier below which only backdrop-flagged dialogs draw), and
    // find the highest layer index.
    int32_t maxLayer = 0;
    Node*   topmost  = *reinterpret_cast<Node**>(S + 0x00);
    for (Node* n = topmost; n != nullptr; n = n->next) {
        uint8_t* const d = static_cast<uint8_t*>(n->value);

        const uint32_t shifted = *reinterpret_cast<uint32_t*>(d + 0x20) >> 4;
        if (shifted & 1u) topmost = n;

        // THE DROPPED DEREFERENCE. On a null +0x18 the compared value is
        // `shifted`, not zero — the load is skipped and ECX is not reset.
        uint8_t* const desc  = *reinterpret_cast<uint8_t**>(d + 0x18);
        const int32_t  probe = desc ? *reinterpret_cast<int32_t*>(desc + 0x34)
                                    : static_cast<int32_t>(shifted);
        if (probe > maxLayer) {
            // ...and here the SAME null yields 0 instead, from `XOR R8D,R8D`.
            uint8_t* const desc2 = *reinterpret_cast<uint8_t**>(d + 0x18);
            maxLayer = desc2 ? *reinterpret_cast<int32_t*>(desc2 + 0x34) : 0;
        }
    }

    // ---- 0x180121d71: the layer loops --------------------------------------
    // `JS` at 0x180121d7e — signed, so a negative maxLayer skips the whole
    // walk. It cannot go negative today (the pre-pass only ever assigns a value
    // strictly greater than the running max, or 0), which makes the branch
    // dead; kept because a dead branch that costs nothing is cheaper than a
    // claim about reachability.
    int32_t curLayer = 0;
    if (maxLayer >= 0) {
        for (int64_t pass = 0;; ++pass, ++curLayer) {
            bool pastTopmost = false;

            for (Node* n = *reinterpret_cast<Node**>(S + 0x00); n != nullptr; n = n->next) {
                uint8_t* const d = static_cast<uint8_t*>(n->value);

                // ---- 0x180121dbe: the backdrop, FIRST PASS ONLY -----------
                // `TEST R12,R12; JNZ` — the whole block is gated on the OUTER
                // pass index being zero, so a dialog's backdrop is painted once
                // no matter how many layers exist.
                if (pass == 0) {
                    int32_t x1 = 0, y1 = 0, x2 = width, y2 = height;
                    uint32_t tint = 0;

                    const int32_t spriteIndex = *reinterpret_cast<int32_t*>(d + 0x60);
                    void* const   sheet0      = *reinterpret_cast<void**>(d + 0x58);
                    if (spriteIndex >= 0 && sheet0 != nullptr) {
                        // Letterbox the backdrop to the sprite's aspect ratio.
                        const float screenAspect =
                            static_cast<float>(width) / static_cast<float>(height);

                        // Both size reads re-do the `idx < 0 -> 0` test, and
                        // both re-load the sheet. Kept as two reads because
                        // that is two loads of `[d+0x58]` in retail, and the
                        // dialog is live memory a suppressed callee could move.
                        int32_t sw = 0, sh = 0;
                        {
                            const int64_t idx = *reinterpret_cast<int32_t*>(d + 0x60);
                            if (idx >= 0) {
                                uint8_t* const tbl = *reinterpret_cast<uint8_t**>(
                                    static_cast<uint8_t*>(*reinterpret_cast<void**>(d + 0x58)) + 0x20);
                                sw = *reinterpret_cast<int32_t*>(
                                    tbl + idx * kSpriteEntryStride + kSpriteEntryWidth);
                            }
                        }
                        {
                            const int64_t idx = *reinterpret_cast<int32_t*>(d + 0x60);
                            if (idx >= 0) {
                                uint8_t* const tbl = *reinterpret_cast<uint8_t**>(
                                    static_cast<uint8_t*>(*reinterpret_cast<void**>(d + 0x58)) + 0x20);
                                sh = *reinterpret_cast<int32_t*>(
                                    tbl + idx * kSpriteEntryStride + kSpriteEntryHeight);
                            }
                        }

                        const float spriteAspect =
                            static_cast<float>(sw) / static_cast<float>(sh);

                        // `COMISS; JBE` — an unordered compare takes the ELSE
                        // arm, which is what `>` gives on a NaN.
                        if (spriteAspect > screenAspect) {
                            const int32_t half = static_cast<int32_t>(
                                spriteAspect * static_cast<float>(height) + 0.5f) >> 1;
                            const int32_t mid = width >> 1;   // SAR, arithmetic
                            x1 = mid - half;
                            x2 = mid + half;
                        } else {
                            const int32_t half = static_cast<int32_t>(
                                static_cast<float>(width) / spriteAspect + 0.5f) >> 1;
                            const int32_t mid = height >> 1;  // SAR, arithmetic
                            y1 = mid - half;
                            y2 = mid + half;
                        }

                        // 0x180121e7c — unguarded on +0x18, but INSIDE the
                        // sprite test: Ghidra shows `local_248 = *(*(d+0x18)
                        // +0x30)` after the letterbox arms and before the
                        // sheet re-read, all under `spriteIndex >= 0 && sheet`.
                        // A dialog with no backdrop sprite never reads it.
                        // ComboDialog's ctor leaves +0x18 NULL by design and
                        // has spriteIndex -1, so hoisting this out of the
                        // block (as this port once did) faulted on the first
                        // dropdown drawn DLL-absent (Options > Video, 2026-09-12).
                        tint = *reinterpret_cast<uint32_t*>(
                            *reinterpret_cast<uint8_t**>(d + 0x18) + 0x30);
                    }

                    void* const sheet = *reinterpret_cast<void**>(d + 0x58);
                    if (sheet != nullptr && *reinterpret_cast<int32_t*>(d + 0x60) >= 0) {
                        selectSheet(sheet);

                        SpriteDraw sd;
                        sd.sheet       = *reinterpret_cast<void**>(d + 0x58);
                        sd.spriteIndex = *reinterpret_cast<int32_t*>(d + 0x60);
                        sd.x1 = x1; sd.y1 = y1; sd.x2 = x2; sd.y2 = y2;
                        sd.rotation    = 0.0f;
                        sd.color       = tint;
                        sd.isLine      = 0;
                        sc->AddSpriteDraw(sd);          // dialog backdrop
                    }
                }

                // ---- 0x180121f0e: is this dialog drawn at all? ------------
                // Everything from the topmost dialog onward draws. Everything
                // BEFORE it draws only if its description carries flag 0x40.
                if (n == topmost) {
                    pastTopmost = true;
                } else if (!pastTopmost) {
                    uint8_t* const desc = *reinterpret_cast<uint8_t**>(d + 0x18);
                    if ((desc[0x40] & 0x40) == 0) continue;   // UNGUARDED deref
                }

                // ---- 0x180121f32: layer test then paint ------------------
                {
                    uint8_t* const desc  = *reinterpret_cast<uint8_t**>(d + 0x18);
                    const int32_t  layer = desc ? *reinterpret_cast<int32_t*>(desc + 0x34) : 0;
                    if (layer != curLayer) continue;
                }

                sc->_deferring = 1;
                {
                    // Element vtable slot 6 (+0x30) `Draw(SpriteContext&, alpha)`
                    // — VIRTUAL, dispatched through the root's own table.
                    void* const root = *reinterpret_cast<void**>(d + 0x50);
                    reinterpret_cast<UI::Element*>(root)->Draw(*sc, 0xff);   // B.3.6 bridge
                }

                // The element pushed its geometry into `_deferred` while
                // `_deferring` was set. Drain it in order, flushing whenever
                // the sheet changes, then flush and clear.
                if (sc->_deferring != 0) {
                    for (int32_t i = 0; i < sc->_deferred._size; ++i) {
                        SpriteDraw* rec = sc->_deferred._data + i;
                        selectSheet(rec->sheet);
                        rec = sc->_deferred._data + i;   // re-load: Flush may
                                                         // have moved the buffer
                        sc->AddSpriteDraw(*rec);        // deferred drain
                    }
                    sc->Flush();                        // end of dialog

                    SetSize0Fn clearDeferred = reinterpret_cast<SetSize0Fn>(
                        rtuielem::AtomicAddr(g_spriteDrawSetSize0Slot, kSpriteDrawSetSize0Addr));
                    if (out.Call("Array<SpriteDraw>::SetSize(0) (clear _deferred)",
                                 (const void*)clearDeferred) && clearDeferred)
                        clearDeferred(&sc->_deferred);
                }
                sc->_deferring = 0;
            }

            if (curLayer >= maxLayer) break;   // `CMP R14D,R8D; JLE` at the tail
        }
    }

    // ---- 0x18012201c: the fullscreen fade quad -----------------------------
    // `UCOMISS XMM0,XMM15; JP +8; JZ skip` — a NaN amount FADES. `!(x == 0.0f)`
    // reproduces that; `x != 0.0f` would too, but the negation is written out
    // because the JP/JZ pair is the only reason to think about it.
    {
        const float amount = *reinterpret_cast<float*>(S + 0xf4);
        if (!(amount == 0.0f) &&
            *reinterpret_cast<int32_t*>(S + 0xe8) >= 0 &&
            *reinterpret_cast<void**>(S + 0xe0) != nullptr) {

            // alpha = (uint8)(int64)(amount * 255.0f), placed in the top byte
            // over the fade colour's low 24 bits.
            const int64_t a64 = static_cast<int64_t>(amount * 255.0f);
            const uint32_t color =
                (static_cast<uint32_t>(static_cast<uint8_t>(a64)) << 24) |
                (*reinterpret_cast<uint32_t*>(S + 0xf8) & 0x00ffffffu);

            void* const sheet = *reinterpret_cast<void**>(S + 0xe0);
            if (sheet != nullptr && *reinterpret_cast<int32_t*>(S + 0xe8) >= 0) {
                selectSheet(sheet);

                SpriteDraw sd;
                sd.sheet       = *reinterpret_cast<void**>(S + 0xe0);
                sd.spriteIndex = *reinterpret_cast<int32_t*>(S + 0xe8);
                sd.x1 = 0; sd.y1 = 0;            // one qword at 0x1801220b9
                sd.x2 = width; sd.y2 = height;
                sd.rotation    = 0.0f;
                sd.color       = color;
                sd.isLine      = 0;
                sc->AddSpriteDraw(sd);                  // fullscreen fade
            }
        }
    }

    // ---- 0x1801220ec: the mouse cursor -------------------------------------
    {
        uint8_t* const cursor = *reinterpret_cast<uint8_t**>(S + 0xd0);
        uint8_t* const cfg    = static_cast<uint8_t*>(*InputConfigSingleton());
        if (cursor != nullptr && *(S + 0xd8) != 0 && cfg[0x0a] != 0) {
            uint8_t* const in = static_cast<uint8_t*>(*InputDeviceSingleton());

            // MOVSX — SIGNED 16-bit mouse coordinates, which Ghidra prints as a
            // zero extension. A negative X off the left edge is real.
            const int32_t rawY = *reinterpret_cast<int16_t*>(in + 0x10b4);
            const float   uiScale = *reinterpret_cast<float*>(S + 0xdc);
            const int32_t rawX = *reinterpret_cast<int16_t*>(in + 0x109c);

            const int32_t x1 = static_cast<int32_t>(static_cast<float>(rawX) * uiScale)
                             - *reinterpret_cast<int32_t*>(cursor + 0x20);
            const int32_t y1 = static_cast<int32_t>(static_cast<float>(rawY) * uiScale)
                             - *reinterpret_cast<int32_t*>(cursor + 0x24);

            void* const sheet = *reinterpret_cast<void**>(cursor + 0x28);
            if (sheet != nullptr) {
                selectSheet(sheet);

                // UNGUARDED, and sprite index 0 is implicit: the size comes
                // from the FIRST table entry, with no index arithmetic.
                uint8_t* const tbl = *reinterpret_cast<uint8_t**>(
                    static_cast<uint8_t*>(sheet) + 0x20);
                const int32_t sh = *reinterpret_cast<int32_t*>(tbl + kSpriteEntryHeight);
                const int32_t sw = *reinterpret_cast<int32_t*>(tbl + kSpriteEntryWidth);

                SpriteDraw sd;
                sd.sheet       = sheet;
                sd.spriteIndex = 0;
                sd.x1 = x1; sd.y1 = y1;
                sd.x2 = sw + x1; sd.y2 = sh + y1;
                sd.rotation    = 0.0f;
                sd.color       = 0xffffffffu;
                sd.isLine      = 0;
                sc->AddSpriteDraw(sd);                  // mouse cursor
            }
        }
    }

    // ---- 0x1801221cf: the tail ---------------------------------------------
    sc->Flush();                                        // final

    if (UI::SpriteCapture* const capture = sc->_spriteCapture) {
        capture->EndAddingSprites();                    // a member of the real class (block D)
    }

    // ---- 0x1801221e6: restore the context scissor to the whole target ------
    {
        uint8_t* const dev = static_cast<uint8_t*>(*VideoDeviceSingleton());
        const int32_t devW = *reinterpret_cast<int32_t*>(dev + 4);
        const int32_t devH = *reinterpret_cast<int32_t*>(dev + 8);

        uint8_t* const cc = static_cast<uint8_t*>(sc->_context);
        int32_t* const vp = reinterpret_cast<int32_t*>(cc + 0x1e0);
        if (vp[0] != 0 || vp[1] != 0 || vp[2] != devW || vp[3] != devH) {
            out.U32(cc + 0x10, out.ReadU32(cc + 0x10) | RtVideo::kDirtyScissor,
                    "Context::_dirty |= kDirtyScissor");
            out.U64(cc + 0x1e0, 0, "Context::_scissor.x/y = 0 (one qword)");
            out.U32(cc + 0x1e8, static_cast<uint32_t>(devW), "Context::_scissor.w");
            out.U32(cc + 0x1ec, static_cast<uint32_t>(devH), "Context::_scissor.h");
        }
    }

    // ---- 0x180122242: free the four buffers the SpriteContext owns ---------
    // Retail inlines EnterCriticalSection / Free / LeaveCriticalSection around
    // the allocator singleton; that sequence IS the body of the exported
    // `?MemFree@MemoryAllocator@System@@SAXPEAX@Z`, so calling the export
    // reproduces the lock discipline with no <windows.h> in a runtime/src file
    // (WAVE10_LANE_UI1.md §1b). Retail's own `if (p)` is OUTSIDE the call here,
    // so the guard is reproduced outside too.
    //
    // The order is retail's: +0x68, +0x58, +0x40, +0x10. Each clears the
    // pointer and the size/capacity qword that follows it.
    FreeFn memFree = reinterpret_cast<FreeFn>(rtuielem::AtomicExport(g_memFreeSlot, kMemFree));
    constexpr size_t kFreeSlots[4] = { 0x68, 0x58, 0x40, 0x10 };
    for (size_t k = 0; k < 4; ++k) {
        uint8_t* const slot = scStorage + kFreeSlots[k];
        if (void* const p = *reinterpret_cast<void**>(slot)) {
            if (out.Call("MemFree (SpriteContext buffer)", (const void*)memFree))
                memFree(p);
        }
        *reinterpret_cast<void**>(slot)     = nullptr;
        *reinterpret_cast<uint64_t*>(slot + 8) = 0;
    }
}

extern "C" void RtUi_UserInterface_Draw(UserInterface* self, void* ctx) {
    self->UI::UserInterface::Draw(*static_cast<Video::Context*>(ctx));
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
int RtUiDrawRegister() {
    int count = 0;

    // Registered under BOTH the mangled name and the address. WAVE7.md §3: a
    // port registered under the name only can be dead at a call site that
    // reaches the DLL through the address fallback while reporting a clean
    // oracle over nothing
    // (reference_half_registered_port_reads_as_absent.md).
    RegisterRuntimeExport("?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z",
                          (void*)&RtUi_UserInterface_Draw); ++count;
    RegisterRuntimeAddr(0x180121890, (void*)&RtUi_UserInterface_Draw);

    return count;
}
