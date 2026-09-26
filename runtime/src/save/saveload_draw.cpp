// ============================================================================
// runtime/src/save/saveload_draw.cpp — Wave 14 lane DRAW
//
//   ?Draw@SaveLoad@Save@@QEAAXAEAVContext@Video@@V?$ExternalPtr@VMaterial@
//    Graphics@@@Object@@@Z            retail 0x1800c2eb0   ordinal 2260
//
// The save/load thumbnail pass. It runs on exactly ONE frame per save: the
// frame after `UpdateSave` state 0 sets `_captureFlags`'s low byte to 1, and
// it clears that byte on the way out. `UpdateSave` state 1 then blocks until
// `_capturePixels` is non-null, so if this body never fills it the save never
// completes — which is the coarsest possible functional check and it is free.
// ============================================================================
//
// ----------------------------------------------------------------------------
// WHAT IT DOES, in one paragraph.
//
// It renders the CURRENT frame, letterboxed, into a private offscreen render
// target sized `_captureWidth` x `_captureHeight`, and reads the result back
// into a heap buffer it hands to `_capturePixels`. The mechanism is a
// `Graphics::FrameCopyImageBuffer` (a texture that copies the live backbuffer
// into itself), wrapped in a one-parameter `Graphics::MaterialInstance`
// ("diffuse" -> that texture), wrapped in a one-sprite `UI::SpriteSheet`, drawn
// through a stack `UI::SpriteContext` under an orthographic camera. Everything
// it perturbs on the shared `Video::Context` — the render target, the viewport,
// the scissor — it saves first and restores afterwards, except the scissor,
// which it leaves set to the full device size.
//
// ----------------------------------------------------------------------------
// WHY THIS WAS HELD BACK TWICE, AND WHAT WAS ACTUALLY TRUE.
//
// Wave 5 lane C (WAVE5_LANE_C.md, row 6) and the Wave 8.5 hold-back re-check
// (WAVE8_5_HOLDBACK_RECHECK.md, row 10) both declined it, with two reasons.
// Re-checked here before porting, as
// reference_holdback_blockers_are_stale_check_them_first.md requires:
//
//   REASON 1 — "it hand-constructs five Wave 6/7 classes, so porting Draw means
//   porting their constructors first; that is a wave, not a function."
//   *** FALSE, and it was falsified by the disassembly in ten minutes. ***
//   Those constructors are INLINED here — that is precisely why the decompiler
//   shows raw vtable stamps instead of calls. What this body actually calls is
//   fourteen functions, and THIRTEEN OF THEM ARE NAMED EXPORTS of the shipped
//   DLL (`??2FrameCopyImageBuffer@Graphics@@SAPEAX_K@Z`,
//   `??0FrameCopyImageBuffer@Graphics@@QEAA@XZ`, `?Create@RenderBuffer@Video@@`,
//   `?Set@SpriteSheet@UI@@`, `?AddSpriteInfo@SpriteSheet@UI@@`,
//   `??0Camera@Graphics@@`, `?SetCamera@Context@Video@@`, …). The fourteenth
//   family is three IDENTICAL five-line `Array<POD>::~Array` instantiations
//   (0x18001c750, 0x180033bc0, 0x180033de0) plus one
//   `Array<{String,void*}>::SetSize` (0x180061480) — all four transcribed
//   inline below, so this port opens NO address door into the DLL and does not
//   grow the cutover surface.
//   The lesson is the general one of
//   reference_grep_cannot_tell_internal_from_unported_export.md: "the callee is
//   not ported" and "the callee has no name" are different facts, and only the
//   export table can tell them apart.
//
//   REASON 2 — "no oracle is possible: it renders into the live Video::Context
//   and would draw twice and leak a render target per call."
//   *** TRUE ABOUT THE NAIVE REPLAY, FALSE ABOUT ORACLES. ***
//   Drawing twice is only fatal if the second draw's inputs have been spoiled
//   by the first, and they have not: this body restores every piece of shared
//   state it touches, and the backbuffer it samples is not one of the things it
//   writes. The leak is real and is OURS to fix, not the oracle's to suffer:
//   the harness below frees our readback buffer and rewinds `_captureFlags`,
//   `_capturePixels` and the four Context fields before handing the frame to
//   the DLL, so retail runs on genuinely pristine input and the LIVE post-state
//   the game keeps is retail's, not ours.
//   See the ORACLE section at the bottom of this banner.
//
// ----------------------------------------------------------------------------
// FOUR THINGS THE DECOMPILED C GOT WRONG. Every one was caught by reading the
// disassembly, which is what reference_ghidra_c_output_vs_disassembly.md says
// to do and what Wave 13 had to do twice in this same class.
//
//   1. *** A DROPPED ARGUMENT AT A VTABLE CALL. *** Ghidra prints the
//      FrameCopyImageBuffer capture call as
//          (**(code **)(*(longlong *)pMVar18 + 0x50))(pMVar18, param_1);
//      Two arguments. The bytes at 0x1800c2fa1..0x1800c2fad set R8D to 1 as
//      well — it is THREE. This is the exact defect class the lane brief warned
//      about, on the exact call it warned about, and a two-argument call here
//      leaves the third parameter as whatever the previous frame left in R8.
//      CONFIRMED INDEPENDENTLY, because "I read the disassembly more carefully"
//      is a claim only its author can check: slot 0x50 of
//      `??_7FrameCopyImageBuffer@Graphics@@6B@` (0x180154f00) holds 0x1800a3b20,
//      and the export table names that
//      `?Generate@FrameCopyImageBuffer@Graphics@@UEBAXAEAVContext@Video@@H@Z` —
//      `void Generate(Video::Context&, int) const`. The signature settles it
//      without reference to any register. The same walk names slot 0x48
//      `?Create@…UEAA_NW4ImageType@…` with exactly eleven parameters, which is
//      how the 12-entry call below is typed.
//
//   2. *** A FABRICATED 64-BIT ARGUMENT. *** Ghidra folds an unrelated stack
//      dword into the ImageBuffer creation call as
//      `uVar22 = CONCAT44(uVar24,0x100)` and then passes `uVar22 & 0xffffffff00000000`
//      to the SECOND call. The bytes say `MOV dword ptr [RSP+0x20],0x100` for
//      the first and `MOV dword ptr [RSP+0x20],EBX` (EBX = 0) for the second.
//      Two independent DWORD literals, 0x100 and 0, not one aliased qword.
//
//   3. *** AN INVENTED VARIABLE ALIAS. *** `local_320` is reused by the
//      decompiler for the MaterialInstance, the SpriteSheet, a String slot and
//      the saved render buffer. Following the C would have stored the sprite
//      sheet pointer where the saved render target belongs.
//
//   4. *** A DEGENERATE-LOOKING TRANSFORM THAT IS THE IDENTITY. *** The
//      twelve floats at RBP-0x70 read as rows (1,0,0,0)/(1,0,0,0)/(1,0,0,0) if
//      you assume a 3x4 matrix with a 16-byte stride. `Math::Transform3` is
//      twelve CONTIGUOUS floats — four Vector3s, no padding — so the same bytes
//      are axisX=(1,0,0), axisY=(0,1,0), axisZ=(0,0,1), translation=(0,0,0):
//      the identity. runtime/types/math/Math.h settles it; the decompiler's
//      variable numbering does not.
//
// ----------------------------------------------------------------------------
// THE `ExternalPtr<Graphics::Material>` PARAMETER — passed BY VALUE, and the
// answer is NOT the one the ABI rule of thumb predicts.
//
// "A non-trivial class passed by value goes by hidden reference in the MS x64
// ABI" is true and does not apply here. The prologue stores R8 straight into
// its home slot (`MOV qword ptr [RAX+0x18],R8` at 0x1800c2eb3) and 0x1800c3192
// loads that same qword and stores it whole into `MaterialInstance+0x28`. So
// the parameter IS the 8-byte value, passed in a register: `ExternalPtr<T>` is
// a trivially-copyable single pointer.
//
// AND IT CARRIES NO REFCOUNT SIDE EFFECT HERE. There is no increment on the way
// in and no release on the way out — the exe's caller
// (`code/Game/Game/gameupdateloop.cpp:2231`) passes `m_uiMaterial`, a
// borrowed pointer it keeps owning, and retail treats it as borrowed for the
// life of the MaterialInstance it stamps it into. A port that retained or
// released it would be wrong in the direction that does not crash.
//
// ----------------------------------------------------------------------------
// TWO PIECES OF RETAIL BEHAVIOUR THAT LOOK LIKE BUGS AND ARE REPRODUCED.
//
//   * `SpriteContext+0x50` IS NEVER INITIALISED. The inlined SpriteContext
//     constructor zeroes +0x08, +0x10, +0x18, +0x20..+0x2f, +0x38, +0x40,
//     +0x48, +0x58, +0x60, +0x68, +0x70, +0x78 and +0x80, and takes the ADDRESS
//     of +0x50 (0x1800c3815) without ever writing it. Our stack object leaves
//     it uninitialised too. Zeroing it would be a divergence, not a fix.
//
//   * THE FOUR `vtable[0]` TEARDOWN CALLS ARE UNGUARDED. If any `operator new`
//     had returned null the object pointer is null and retail dereferences it
//     anyway (0x1800c3a70 onward). CLAUDE.md's rule applies exactly: retail's
//     unguarded deref is load-bearing, a guard does not restore the behaviour,
//     and the allocator does not return null here. Left unguarded.
//
// ----------------------------------------------------------------------------
// ORACLE — a REPLAY oracle on the payload, gated by the old DLL-era switch.
//
// The observable of a Draw is not a return value; it is the pixels it produces
// and the state it leaves behind. Both are compared:
//
//   ours runs for real  -> snapshot the w*h*4 readback into a private copy,
//                          plus `_captureFlags` and the four Context fields
//   free ours' buffer, rewind `_capturePixels`, `_captureFlags`,
//   `_renderBuffer`, `_dirty`, `_viewport` and `_scissor` to their pre-values
//   THEIRS runs for real on that pristine state
//   compare, on THREE SEPARATE the diff oracle SITES so each is scored on its own:
//     [pixels]                    the w*h*4 readback — the only screen that can
//                                 see the actual rendering
//     [SaveLoad post-state]       `_captureFlags` (`_capturePixels` is excluded:
//                                 two MemAllocs cannot return one address, and
//                                 its CONTENT is the pixel screen)
//     [Video::Context post-state] `_renderBuffer`, `_dirty`, `_viewport`, `_scissor`
//   Three sites rather than one because a `divergences=` scored over the UNION
//   cannot tell "all three passed" from "two passed and the third is blind"
//   (reference_union_control_hides_a_blind_screen.md).
//
// After the round the live object holds RETAIL's buffer and RETAIL's state, so
// the game continues on the authority's result and there is no leak.
//
// The three controls, scored separately in the report:
//   * the old DLL-era switch — the POSITIVE control. Runs THEIRS on both
//     sides. If the pixel compare is not green here the readback is not
//     deterministic across two draws in one frame and an ours-vs-theirs green
//     would mean nothing.
//   * the old DLL-era switch — the NEGATIVE control, and it corrupts
//     something the comparison ACTUALLY READS
//     (reference_control_must_pick_a_compared_subject.md): it halves the
//     frustum's right-plane distance, which changes the projection and so every
//     compared pixel. Not `_captureFlags`, which the pixel screen cannot see.
//   * `calls=` and `worked=` — read them, not just `divergences=`. `worked` is
//     the `_captureFlags` early-out: a run whose `worked` is 0 never entered the
//     body and its clean board is empty
//     (reference_uncalled_port_is_not_verified.md,
//      reference_calls_zero_can_mean_always_took_the_uncovered_branch.md).
//
// with Wave 5's and Wave 13's Save ports still live.
// ============================================================================

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saveload_internal.h"
#include "../../types/math/Math.h"
#include "../../types/volume/Volume.h"
#include "../../types/video/Context.h"
#include "../../types/video/CompanionData.h"   // ENDGAME B.3.5: Video::ContextData
#include "../../types/video/Device.h"
#include "../../types/video/ImageBuffer.h"
#include "../../types/graphics/FrameCopyImageBuffer.h"  // ENDGAME B.3.5
#include "../../types/graphics/Camera.h"
#include "../../types/graphics/Material.h"   // ENDGAME B.3.5: Graphics::MaterialInstance
#include "../../types/ui/SpriteContext.h"
#include "../../types/ui/SpriteSheet.h"
#include "../../types/ui/SpriteCapture.h"   // ENDGAME B.3.6 UI callers (K4)
#include "../../types/object/External.h"   // Object::ExternalPtr<T> — Draw's by-value argument

using RtSave::SaveLoad;
using RtVideo::Context;
using RtVideo::Viewport;
using RtSaveDetail::MemAlloc;
using RtSaveDetail::MemFree;
using RtSaveDetail::ReleaseStringSlot;

namespace {

// ============================================================================
// 1. The four unexported internals, transcribed rather than called by address.
// ============================================================================

// `Collection::Array<T>::~Array` for a POD element type. THREE separate
// template instantiations in the DLL — 0x18001c750, 0x180033bc0 and
// 0x180033de0 — whose decompiled bodies are byte-for-byte the same five lines,
// because the element type has no destructor and only the stride (which never
// appears) would have differed. Draw calls all three, one per SpriteContext
// array member.
//
// The locked free is exactly `System::MemoryAllocator::MemFree`: that export
// (0x1800c7ad0) is `if (p) { EnterCriticalSection(_singleton+0x640); Free(...);
// LeaveCriticalSection(...); }`, which is the inlined sequence character for
// character. So this reaches the SAME allocator lock the DLL uses, through the
// port-aware door, and stays correct when Wave 10's allocator port is armed.
struct ArrayHeader {
    void*    data;      // +0x00
    int32_t  size;      // +0x08
    int32_t  capacity;  // +0x0c
};
static_assert(sizeof(ArrayHeader) == 0x10, "Collection::Array<T> is 0x10 bytes");


// One element of `Graphics::MaterialInstance`'s parameter array at +0x50: a
// refcounted `System::String` name and the borrowed object it binds to.
// Stride 0x10 is pinned by 0x180061480's `SHL/lea *0x10` addressing and by its
// per-element copy of BOTH qwords with a refcount bump on only the first.
struct MaterialBinding {
    const char* name;   // +0x00  System::String
    void*       value;  // +0x08  borrowed; no refcount
};
static_assert(sizeof(MaterialBinding) == 0x10, "MaterialInstance binding stride is 0x10");

// `FUN_180005930` (0x180005930) — release the String half of `count` bindings
// starting at `p`. The idiom is `ReleaseStringSlot` applied over a strided
// range; both of its dead stores are reproduced there and so are reproduced
// here by construction.
void ReleaseBindingNames(MaterialBinding* p, int32_t count) {
    for (int32_t i = 0; i < count; ++i) ReleaseStringSlot(&p[i].name);
}

// `FUN_180061480` (0x180061480) —
// `Collection::Array<MaterialBinding>::SetSize(int newSize, const MaterialBinding& fill)`.
//
// Transcribed, not paraphrased, because two of its branches are the kind that
// a "sensible" rewrite silently drops:
//
//   * THE GROWTH RULE IS NOT DOUBLING. Capacity becomes `newSize`, EXCEPT when
//     the call is a single append onto a non-empty array, where it becomes
//     `newSize * 3`. Draw always takes exactly that arm (size 0 -> 1 on an
//     empty array, so `oldSize != 0` is FALSE and capacity becomes 1), but the
//     arm has to be here for the transcription to be the function.
//   * THE TAIL IS RELEASED, NOT JUST FREED. When the buffer did not move and
//     the array SHRANK, the dropped elements' Strings are released; when it
//     did move, ALL the old elements' Strings are released and then the old
//     block is freed. Collapsing either into a bare `MemFree` would leak one
//     String refcount per element — the shape
//     reference_a_written_recipe_can_be_wrong.md is about.
void ArrayBindingSetSize(ArrayHeader* a, uint32_t newSize, const MaterialBinding* fill) {
    const uint32_t oldSize = static_cast<uint32_t>(a->size);
    if (newSize == oldSize) return;

    MaterialBinding* const oldData = static_cast<MaterialBinding*>(a->data);
    a->size = static_cast<int32_t>(newSize);

    if (a->capacity < static_cast<int32_t>(newSize)) {
        a->capacity = static_cast<int32_t>(newSize);
        if (newSize == oldSize + 1 && oldSize != 0) a->capacity = static_cast<int32_t>(newSize * 3);
        a->data = MemAlloc(static_cast<uint64_t>(a->capacity) << 4, 8);
    }

    // `plVar6 = 0; if (buffer did not move) plVar6 = oldSize;` — the index the
    // fill loop starts at is the old size when the buffer was reused and 0
    // when it was replaced, because a replaced buffer must be re-populated
    // from the start.
    const int32_t start = (oldData == static_cast<MaterialBinding*>(a->data))
                            ? static_cast<int32_t>(oldSize) : 0;

    if (start < static_cast<int32_t>(newSize)) {
        MaterialBinding*       dst = static_cast<MaterialBinding*>(a->data) + start;
        const MaterialBinding* src = oldData + start;
        for (int32_t i = start; i < static_cast<int32_t>(newSize); ++i, ++dst, ++src) {
            if (dst == nullptr) continue;                 // retail's own null test
            const MaterialBinding* from = (i < static_cast<int32_t>(oldSize)) ? src : fill;
            dst->name = nullptr;
            if (from->name != nullptr) {
                dst->name = from->name;
                *reinterpret_cast<int*>(const_cast<char*>(from->name) - 4) += 1;
            }
            dst->value = from->value;
        }
    }

    if (oldData == static_cast<MaterialBinding*>(a->data)) {
        if (static_cast<int32_t>(newSize) < static_cast<int32_t>(oldSize))
            ReleaseBindingNames(oldData + newSize, static_cast<int32_t>(oldSize - newSize));
    } else {
        ReleaseBindingNames(oldData, static_cast<int32_t>(oldSize));
        if (oldData != nullptr) MemFree(oldData);
    }
}

// ============================================================================
// 2. The DLL entry points. Every name below was read out of the SHIPPED export
//    table with pefile, by ADDRESS, before it was written down: the address the
//    disassembly calls was looked up in `DIRECTORY_ENTRY_EXPORT`, so none of
//    these is a name a human spelled from memory
//    (reference_resolveexport_phantom_names.md). The ordinal is recorded next
//    to each so the lookup is reproducible.
// ============================================================================

using OpNewFn      = void* (*)(uint64_t);
using VoidThisFn   = void  (*)(void*);
// ENDGAME B.3.1 step 2: the String::Set door typedef is gone with its one call
// site — it is `diffuse.Set("diffuse")` now.
using CameraCtorFn = void  (*)(void*, const Frustum*, const Transform3*, bool);  // return ignored, see above

// The two Video singletons Draw reads. Both are DATA exports whose slot holds
// the pointer, so the slot is dereferenced, unlike the vtable symbols below.
// Wave 15 lane ORDER converted both from `static T** p = ResolveExport(...)`,
// which freezes the FIRST answer including a non-null one, to the caching door
// that now refuses to freeze a pre-init answer (rt_resolve_cache.cpp §6).
// Neither of these two rows is promoted today; the conversion is for the shape,
// so a future promotion of Device@Video or Configuration@Video cannot strand
// this file's readers on the DLL's `.data` (and so [RtSlotAudit] can see them).
using DeviceSlot = RtVideo::Device**;
using ConfigSlot = RtVideo::Configuration**;
RtVideo::Device* DeviceSingleton() {
    DeviceSlot p = RT_DATA_SLOT(DeviceSlot,
        "?_singleton@Device@Video@@0PEAV12@EA");                         // 0x1801fe7a8
    return p ? *p : nullptr;
}
RtVideo::Configuration* ConfigurationSingleton() {
    ConfigSlot p = RT_DATA_SLOT(ConfigSlot,
        "?_singleton@Configuration@Video@@0PEAV12@EA");                  // 0x1801fe7a0
    return p ? *p : nullptr;
}

// The four vtables the inlined constructors stamp. A `??_7X@@6B@` symbol names
// the ADDRESS OF the first virtual slot, and retail's `LEA RAX,[<that address>]`
// stores the address itself — so these are used undereferenced. They are also a
// reminder that `AEAA`/private has nothing to do with exportedness: this DLL
// exports 147 `??_7` symbols and four of them are exactly what Draw needs
// (reference_aeaa_means_private_not_unexported.md).
//
// ENDGAME B.3.7 B2 (RULE B6-5): these were `static void* p = ResolveExport(...)`
// guarded statics inside what is now a MEMBER body — the freeze-the-first-
// answer shape B7-7 names. They are namespace-scope atomic slots with the
// `RtResolveCached` shape now (saveload_internal.h `AtomicExport`).
std::atomic<void*> g_vtBase{nullptr};
std::atomic<void*> g_vtExternal{nullptr};
std::atomic<void*> g_vtMaterialInstance{nullptr};
std::atomic<void*> g_vtSpriteSheet{nullptr};
void* VtBase()             { return RtSaveDetail::AtomicExport(g_vtBase,             "??_7Base@Object@@6B@"); }              // 1389
void* VtExternal()         { return RtSaveDetail::AtomicExport(g_vtExternal,         "??_7External@Object@@6B@"); }          // 1432
void* VtMaterialInstance() { return RtSaveDetail::AtomicExport(g_vtMaterialInstance, "??_7MaterialInstance@Graphics@@6B@"); } // 1464
void* VtSpriteSheet()      { return RtSaveDetail::AtomicExport(g_vtSpriteSheet,      "??_7SpriteSheet@UI@@6B@"); }            // 1504

// ============================================================================
// 3. `Video::ImageBuffer::Width()` / `Height()`, inlined by retail at four
//    sites in this body (0x1800c32b8, 0x1800c330e, 0x1800c33ae, 0x1800c3404 —
//    the SAME pair of computations performed twice, once for SpriteSheet::Set
//    and once for the sprite entry's pixel size).
//
//    The flag bits match runtime/types/video/ImageBuffer.h's own note exactly:
//    bit 8 = "the device's size", bit 9 = "half the device's size", bit 6 =
//    "shift the stored size right by Configuration::shadowScalar". Retail's
//    halving is `CDQ; SUB EAX,EDX; SAR EAX,1`, which is signed division by 2
//    (round toward zero), not an arithmetic shift.
// ============================================================================
int32_t ImageBufferWidth(const ImageBuffer* ib) {
    const uint32_t f = ib->flags;
    if (f & 0x100u) return DeviceSingleton()->backBufferWidth;
    if (f & 0x200u) return DeviceSingleton()->backBufferWidth / 2;
    int32_t w = ib->width;
    if (f & 0x40u) w >>= (ConfigurationSingleton()->shadowScalar & 0x1f);
    return w;
}
int32_t ImageBufferHeight(const ImageBuffer* ib) {
    const uint32_t f = ib->flags;
    if (f & 0x100u) return DeviceSingleton()->backBufferHeight;
    if (f & 0x200u) return DeviceSingleton()->backBufferHeight / 2;
    int32_t h = ib->height;
    if (f & 0x40u) h >>= (ConfigurationSingleton()->shadowScalar & 0x1f);
    return h;
}

// ============================================================================
// 4. THE BODY.
// ============================================================================
}  // namespace

// ENDGAME B.3.7 B2: the body is the member. `material` is the by-value
// ExternalPtr that arrives as a bare register value (SaveLoad.h banner,
// B37_RULES 13.1 closed); `ctx` names the context the way the transcription
// below always has.
void Save::SaveLoad::Draw(Video::Context& context,
                          Object::ExternalPtr<Graphics::Material> material) {
    Context* const ctx = &context;
    // `CMP byte ptr [RCX+0x7c],0x0` — a BYTE test, matching the byte stores
    // the ctor and UpdateSave make into the same field.
    if (*reinterpret_cast<const uint8_t*>(&_captureFlags) == 0) return;

    RtVideo::Device* const device = DeviceSingleton();

    // ---- the frame-copy texture -------------------------------------------
    // `??2FrameCopyImageBuffer@Graphics@@SAPEAX_K@Z` (787) then
    // `??0FrameCopyImageBuffer@Graphics@@QEAA@XZ` (198).
    //
    // *** DO NOT READ THE CONSTRUCTOR'S RETURN VALUE. *** Retail does
    // (`CALL <ctor>; MOV R12,RAX`) and retail is right to: MSVC returns `this`
    // in RAX from every constructor. But a PORTED constructor is an ordinary
    // C++ function, and this tree's ported constructors are not uniform —
    // `Video_ImageBuffer_Ctor` (runtime/src/video/buffers.cpp:387) is declared
    // `void`, so its RAX is whatever its last expression left there, while
    // `RtGraphics_Geometry_Ctor` does return `Geometry*`.
    //
    // THIS LANE PAID FOR THAT, and it is the second lane to: the first live run
    // of this body crashed at DrawBody+0x19f with `Fault: READ of 0x1`, one
    // instruction into `*(void**)imageBuffer` — because `??0ImageBuffer@Video@@`
    // resolved to our void-returning port and handed back 1. The Wave 9 sunlight
    // lane hit the identical crash and wrote the resolution down at
    // `runtime/src/graphics/sunlight.cpp:230-247`; I found that note only
    // afterwards, which is itself the lesson (grep the tree for the hazard, not
    // just for the symbol).
    //
    // `p` and the returned `this` are the same pointer BY CONSTRUCTION, so using
    // `p` is byte-faithful to what retail computes and is independent of how any
    // port spells its signature — on the DLL-present path AND at cutover.
    ImageBuffer* frameCopy = nullptr;
    {
        void* p = RT_EXPORT_ONCE(OpNewFn, "??2FrameCopyImageBuffer@Graphics@@SAPEAX_K@Z")(0x88);
        if (p != nullptr) {
            RT_EXPORT_ONCE(VoidThisFn, "??0FrameCopyImageBuffer@Graphics@@QEAA@XZ")(p);
            frameCopy = static_cast<ImageBuffer*>(p);
        }
    }

    // vtable[0x48] is `?Create@FrameCopyImageBuffer@Graphics@@UEAA_N...` — and
    // the ELEVEN parameters are not inferred from the call site, they are read
    // out of the export table entry for the function that slot points at:
    //   Create(ImageType, Usage, Format, unsigned int flags, int w, int h,
    //          FilterMode, AddressMode u, AddressMode v, AddressMode w,
    //          Collection::Array<Collection::Array<uchar>>* levels)
    // (the ImageBuffer slot 0x48 used just below is the same signature, spelled
    // `?Create@ImageBuffer@Video@@UEAA_N...`). Argument 4 is the literal DWORD
    // 0x100 — ImageBuffer flag bit 8, "size = the device's size" — and 5/6 are
    // the device's backbuffer dimensions. See banner note 2 for the
    // decompiler's fabricated qword here.
    // ENDGAME B.3.5: +0x48 is slot 9, and the receiver is PROVED — the op-new and
    // ctor two statements up are FrameCopyImageBuffer's own, so the static type is
    // `Graphics::FrameCopyImageBuffer`, which OVERRIDES slot 9 (its own
    // 0x1800a3b70, not `Video::ImageBuffer`'s 0x1800f2680). The eleven parameter
    // types come from the export-table entry, unchanged.
    reinterpret_cast<Graphics::FrameCopyImageBuffer*>(frameCopy)->Create(
        0, 4, 5, 0x100u,
        device->backBufferWidth, device->backBufferHeight,
        1, 1, 1, 1, nullptr);                                // B.3.5 bridge

    // vtable[0x50] is `?Generate@FrameCopyImageBuffer@Graphics@@UEBAXAEAVContext@Video@@H@Z`
    // — `void Generate(Video::Context&, int) const`. TWO parameters plus `this`.
    // That is banner note 1 confirmed a SECOND, independent way: the export
    // table says three arguments and Ghidra's C printed two, so the finding does
    // not rest on my reading of the disassembly alone.
    // +0x50 is slot 10, introduced by `Graphics::GeneratedImageBuffer` and
    // overridden here: `void Generate(Video::Context&, int) const`. Three
    // arguments including `this`, which is banner note 1.
    reinterpret_cast<Graphics::FrameCopyImageBuffer*>(frameCopy)
        ->Generate(ctx, 1);                                  // B.3.5 bridge

    // ---- the offscreen colour target --------------------------------------
    ImageBuffer* target = nullptr;
    {
        void* p = RT_EXPORT_ONCE(OpNewFn, "??2ImageBuffer@Video@@SAPEAX_K@Z")(0x68);
        if (p != nullptr) {
            // Same rule as above: `p`, never the constructor's return.
            RT_EXPORT_ONCE(VoidThisFn, "??0ImageBuffer@Video@@QEAA@XZ")(p);
            target = static_cast<ImageBuffer*>(p);
        }
    }
    // Same slot 9, but the proved static type here is plain `Video::ImageBuffer`
    // (the op-new and ctor above are `??2ImageBuffer@Video@@` /
    // `??0ImageBuffer@Video@@`), so this reaches ImageBuffer's own 0x1800f2680.
    reinterpret_cast<Video::ImageBuffer*>(target)->Create(
        0, 0, 5, 0u,
        _captureWidth, _captureHeight,
        1, 1, 1, 1, nullptr);                                // B.3.5 bridge

    // ---- the render buffer ------------------------------------------------
    // Three qword zero stores, then `PlatformConstruct`. The zeroing is INSIDE
    // the allocation-succeeded arm and `renderBuffer` stays null otherwise.
    void* renderBuffer = nullptr;
    {
        void* p = RT_EXPORT_ONCE(OpNewFn, "??2RenderBuffer@Video@@SAPEAX_K@Z")(0x30);
        if (p != nullptr) {
            reinterpret_cast<uint64_t*>(p)[0] = 0;
            reinterpret_cast<uint64_t*>(p)[1] = 0;
            reinterpret_cast<uint64_t*>(p)[2] = 0;
            reinterpret_cast<Video::RenderBuffer*>(p)
                ->PlatformConstruct();                       // B.3.5 bridge
            renderBuffer = p;
        }
    }
    reinterpret_cast<Video::RenderBuffer*>(renderBuffer)->Create(
        reinterpret_cast<const Video::ImageBuffer*>(target), nullptr);  // B.3.5 bridge

    // ---- save the viewport, and letterbox ---------------------------------
    const Viewport savedViewport = ctx->_viewport;

    const int32_t devW = device->backBufferWidth;
    const int32_t devH = device->backBufferHeight;

    // The whole of retail's float arithmetic, in retail's order. `aspect` is
    // the CAPTURE aspect; the branch asks whether the device is wider than it.
    const float aspect = static_cast<float>(_captureWidth) /
                         static_cast<float>(_captureHeight);
    const float fDevW = static_cast<float>(devW);
    const float fDevH = static_cast<float>(devH);

    int32_t fitW, fitH;
    if (fDevW / fDevH > aspect) {          // COMISS + JBE: strictly greater
        fitW = static_cast<int32_t>(fDevH * aspect);
        fitH = devH;
    } else {
        fitW = devW;
        fitH = static_cast<int32_t>(fDevW / aspect);
    }
    // `CDQ; SUB EAX,EDX; SAR EAX,1` — signed division by two, toward zero.
    const int32_t offX = (devW - fitW) / 2;
    const int32_t offY = (devH - fitH) / 2;

    // ---- the one-parameter material ---------------------------------------
    // The vtable is stamped THREE times in ascending base order — Base, then
    // External, then MaterialInstance — which is MSVC's ordinary emission for a
    // three-level hierarchy with each base's constructor inlined. The first two
    // stores are dead and are reproduced: they are what the bytes do, and a
    // reader who meets only the final store elsewhere should recognise this
    // shape.
    void* mat = nullptr;
    {
        void* p = RT_EXPORT_ONCE(OpNewFn, "??2MaterialInstance@Graphics@@SAPEAX_K@Z")(0x60);
        if (p != nullptr) {
            uint8_t* m = static_cast<uint8_t*>(p);
            *reinterpret_cast<void**>(m)            = VtBase();
            *reinterpret_cast<void**>(m)            = VtExternal();
            *reinterpret_cast<uint64_t*>(m + 0x08)  = 0;
            *reinterpret_cast<uint64_t*>(m + 0x10)  = 0;
            *reinterpret_cast<uint64_t*>(m + 0x18)  = 0;
            *reinterpret_cast<void**>(m)            = VtMaterialInstance();
            *reinterpret_cast<uint32_t*>(m + 0x20)  = 0;
            *reinterpret_cast<uint16_t*>(m + 0x24)  = 0;   // a WORD store, not a dword
            *reinterpret_cast<uint64_t*>(m + 0x28)  = 0;
            *reinterpret_cast<uint64_t*>(m + 0x30)  = 0;
            *reinterpret_cast<uint64_t*>(m + 0x38)  = 0;
            *reinterpret_cast<uint64_t*>(m + 0x40)  = 0;
            *reinterpret_cast<uint64_t*>(m + 0x48)  = 0;
            *reinterpret_cast<uint64_t*>(m + 0x50)  = 0;
            *reinterpret_cast<uint64_t*>(m + 0x58)  = 0;
            mat = p;
        }
    }
    // The by-value ExternalPtr<Material>, stored whole and borrowed. See the
    // banner: no retain, no release. Unguarded, as retail is.
    *reinterpret_cast<void**>(static_cast<uint8_t*>(mat) + 0x28) = material.Get();

    {
        // ENDGAME B.3.1 step 2: a real `System::String` local. The zero-init is
        // the default ctor (0x180006760), `Set` (0x1800c95d0) is the member,
        // and the trailing `ReleaseStringSlot(&diffuse)` is the destructor —
        // which still runs AFTER `binding.name`'s release, retail's order.
        System::String diffuse;
        diffuse.Set("diffuse");

        MaterialBinding binding;
        binding.name  = nullptr;
        binding.value = nullptr;
        if (diffuse._data != nullptr) {
            binding.name = diffuse._data;
            *reinterpret_cast<int*>(diffuse._data - 4) += 1;
        }
        binding.value = frameCopy;

        ArrayHeader* params = reinterpret_cast<ArrayHeader*>(static_cast<uint8_t*>(mat) + 0x50);
        ArrayBindingSetSize(params, static_cast<uint32_t>(params->size) + 1, &binding);

        ReleaseStringSlot(&binding.name);
    }
    // ENDGAME B.3.5: `?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ`
    // 0x1801033e0 is a DEFINED member now (wave27_lane_gfxbig.cpp), so the
    // by-name door call becomes a member call. The door stays registered.
    reinterpret_cast<Graphics::MaterialInstance*>(mat)->BuildParameters();  // B.3.5 bridge

    // ---- the one-sprite sheet ---------------------------------------------
    SpriteSheet* sheet = nullptr;
    {
        void* p = RT_EXPORT_ONCE(OpNewFn, "??2SpriteSheet@UI@@SAPEAX_K@Z")(0x58);
        if (p != nullptr) {
            uint8_t* s = static_cast<uint8_t*>(p);
            *reinterpret_cast<void**>(s)           = VtBase();
            *reinterpret_cast<void**>(s)           = VtExternal();
            *reinterpret_cast<uint64_t*>(s + 0x08) = 0;
            *reinterpret_cast<uint64_t*>(s + 0x10) = 0;
            *reinterpret_cast<uint64_t*>(s + 0x18) = 0;
            *reinterpret_cast<void**>(s)           = VtSpriteSheet();
            *reinterpret_cast<uint64_t*>(s + 0x20) = 0;
            *reinterpret_cast<uint64_t*>(s + 0x28) = 0;
            *reinterpret_cast<uint64_t*>(s + 0x30) = 0;
            *reinterpret_cast<uint64_t*>(s + 0x38) = 0;
            *reinterpret_cast<uint64_t*>(s + 0x40) = 0;
            *reinterpret_cast<uint64_t*>(s + 0x48) = 0;
            *reinterpret_cast<uint8_t*>(s + 0x50)  = 0;   // a BYTE store
            sheet = reinterpret_cast<SpriteSheet*>(p);
        }
    }
    sheet->m_field30 = reinterpret_cast<uint64_t>(mat);   // the sheet's material, +0x30

    {
        // Two EMPTY Strings, passed by reference. `Set`'s third parameter is
        // the one at RBP+0xe0 (R9) and its fourth the one at RSP+0x70; both are
        // null here, so the sheet gets no texture name and no group name.
        const char* emptyA = nullptr;   // RSP+0x70
        const char* emptyB = nullptr;   // RBP+0xe0
        sheet->Set(ImageBufferWidth(frameCopy), ImageBufferHeight(frameCopy),
                   *reinterpret_cast<const System::String*>(&emptyB),   // B.3.6 bridge
                   *reinterpret_cast<const System::String*>(&emptyA), false);

        // AddSpriteInfo's two Strings are a DIFFERENT pair of empty slots (the
        // two above are consumed by Set), zeroed immediately before the call.
        const char* nameA = nullptr;
        const char* nameB = nullptr;
        sheet->AddSpriteInfo(*reinterpret_cast<const System::String*>(&nameA),   // B.3.6 bridge
                             *reinterpret_cast<const System::String*>(&nameB),
                             offX, offY, offX + fitW, offY + fitH);
    }

    // Sprite entry 0's device-space size and normalised UV rect, written
    // DIRECTLY into the array AddSpriteInfo just grew — retail does not go back
    // through an accessor. The width/height recomputation is the same
    // ImageBuffer::Width()/Height() pair as above, evaluated a second time.
    {
        SpriteInfo* entry = static_cast<SpriteInfo*>(sheet->m_sprites);
        entry->width  = ImageBufferWidth(frameCopy);
        entry->height = ImageBufferHeight(frameCopy);
        entry->u0 = static_cast<float>(offX) / fDevW;
        entry->v0 = static_cast<float>(offY) / fDevH;
        entry->u1 = static_cast<float>(offX + fitW) / fDevW;
        entry->v1 = static_cast<float>(offY + fitH) / fDevH;
    }

    // ---- redirect the context ---------------------------------------------
    void* const savedRenderBuffer = ctx->_renderBuffer;
    if (savedRenderBuffer != renderBuffer) {
        ctx->_dirty |= 0x1u;
        ctx->_renderBuffer = renderBuffer;
    }
    if (ctx->_viewport.x != 0 || ctx->_viewport.y != 0 ||
        ctx->_viewport.width != _captureWidth ||
        ctx->_viewport.height != _captureHeight) {
        ctx->_dirty |= 0x200u;
        // `MOV qword ptr [RDI+0x1d0],R8` with R8 == 0 — x and y cleared as ONE
        // qword store, then the two dwords.
        ctx->_viewport.x      = 0;
        ctx->_viewport.y      = 0;
        ctx->_viewport.width  = _captureWidth;
        ctx->_viewport.height = _captureHeight;
    }

    // Clear, through the backend: contextData->vtable[0x20](this, 1, 0xffff6000, 1.0f, 0).
    // The colour argument is an int (`MOV R8D,0xffff6000`), the depth a float in
    // XMM3, and the fifth a DWORD 0 in the shadow space.
    // ENDGAME B.3.5: +0x20 is slot 4 of the real abstract `Video::ContextData`,
    // `Clear(unsigned flags, unsigned color, float depth, int stencil)`. The
    // receiver is PROVED — it is `Video::Context::_contextData` (+0x2160), which
    // Context.h types as `ContextData*`. The concrete subclass and the dx11 seam
    // stay as they are (B35_RULES section 11).
    reinterpret_cast<Video::ContextData*>(ctx->_contextData)
        ->Clear(1u, 0xffff6000u, 1.0f, 0);                   // B.3.5 bridge

    // ---- the orthographic camera ------------------------------------------
    // Six planes and an "orthographic" flag, all literals except the two that
    // carry the capture size. Transcribed plane by plane from
    // 0x1800c3588..0x1800c3623; the two negated terms come from an XORPS
    // against the sign-mask xmmword at 0x1801617c0.
    Transform3 cameraToWorld;
    cameraToWorld.axisX       = { 1.0f, 0.0f, 0.0f };
    cameraToWorld.axisY       = { 0.0f, 1.0f, 0.0f };
    cameraToWorld.axisZ       = { 0.0f, 0.0f, 1.0f };
    cameraToWorld.translation = { 0.0f, 0.0f, 0.0f };

    const float fCapW = static_cast<float>(_captureWidth);
    const float fCapH = static_cast<float>(_captureHeight);

    Frustum frustum;
    frustum.planes[0] = { 0.0f,  1.0f,  0.0f,  0.0f  };
    frustum.planes[1] = { 0.0f, -1.0f,  0.0f, -1.0f  };
    frustum.planes[2] = { 1.0f,  0.0f,  0.0f,  0.0f  };
    frustum.planes[3] = { -1.0f, 0.0f,  0.0f, -fCapW };
    frustum.planes[4] = { 0.0f,  0.0f, -1.0f, -0.0f  };
    frustum.planes[5] = { 0.0f,  0.0f,  1.0f,  fCapH };
    frustum.isOrthographic = true;

    // 0xd8 bytes, pinned twice in runtime/types/graphics/Camera.h. The ctor
    // copies the frustum and the transform in and calls SetFrustum, which fills
    // the projection at +0x94.
    RtGraphics::Camera camera;
    RT_EXPORT_ONCE(CameraCtorFn,
        "??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z")(
            &camera, &frustum, &cameraToWorld, false);

    // The view matrix is the camera's Transform3 widened to 4x4 with the
    // fourth column {0,0,0,1}; the projection is copied out whole.
    Matrix view;
    const float* const t = &camera.cameraToWorld.axisX.x;
    view.m[0]  = t[0];  view.m[1]  = t[1];  view.m[2]  = t[2];  view.m[3]  = 0.0f;
    view.m[4]  = t[3];  view.m[5]  = t[4];  view.m[6]  = t[5];  view.m[7]  = 0.0f;
    view.m[8]  = t[6];  view.m[9]  = t[7];  view.m[10] = t[8];  view.m[11] = 0.0f;
    view.m[12] = t[9];  view.m[13] = t[10]; view.m[14] = t[11]; view.m[15] = 1.0f;

    Matrix projection;
    std::memcpy(&projection, &camera.projection, sizeof(Matrix));

    reinterpret_cast<Video::Context*>(ctx)->SetCamera(view, projection);  // B.3.5 bridge

    // ---- the sprite context, constructed inline ---------------------------
    // A stack object whose constructor retail inlined. See the banner for the
    // ONE member it never initialises (+0x50).
    UI::SpriteContext sc;
    sc._scale        = 1.0f;
    sc._currentSheet = nullptr;
    sc._draws._data = nullptr; sc._draws._size = 0; sc._draws._capacity = 0;
    sc._scissorX = 0; sc._scissorY = 0; sc._scissorW = 0; sc._scissorH = 0;
    sc._context   = ctx;
    sc._deferring = 0;
    sc._deferred._data = nullptr; sc._deferred._size = 0; sc._deferred._capacity = 0;
    // +0x58 and +0x68 are two more Collection::Array<T> members inside the
    // header's `_opaque50` blob — pinned HERE by this constructor zeroing both
    // halves of each and by the destructor at the bottom freeing exactly those
    // two pointers. +0x50 is deliberately left alone.
    std::memset(reinterpret_cast<uint8_t*>(&sc) + 0x58, 0, 0x20);
    sc._clipRegion     = nullptr;
    sc._spriteCapture  = nullptr;

    // The scissor rectangle comes from the render buffer the context is
    // pointing at RIGHT NOW — which is ours — read through ctx, not through the
    // local, exactly as retail does (`MOV R9,[RBP+0x80]; MOV RAX,[R9+0x28]`).
    {
        const RtVideo::RenderBufferPrefix* rb =
            static_cast<const RtVideo::RenderBufferPrefix*>(ctx->_renderBuffer);
        sc._scissorX = 0;
        sc._scissorY = 0;
        sc._scissorW = rb->width;
        sc._scissorH = rb->height;
    }

    {
        // Was `Matrix identity; memset(&identity, 0, sizeof(identity));`.
        // ENDGAME B.3.1 made Math::Matrix a real class whose default ctor
        // (retail 0x18001d770) zeroes all sixteen cells, so the declaration
        // alone is the memset, and -Wclass-memaccess is not tripped.
        Matrix identity;
        identity.m[0] = 1.0f; identity.m[5] = 1.0f; identity.m[10] = 1.0f; identity.m[15] = 1.0f;
        reinterpret_cast<Video::Context*>(ctx)->SetTransform(0, identity);  // B.3.5 bridge
    }
    sc._scale = 1.0f;   // written a second time, after SetTransform

    // The batch-key switch, inlined from SpriteContext::SetSheet: flush only if
    // a DIFFERENT non-null sheet was current.
    if (sc._currentSheet != sheet) {
        if (sc._currentSheet != nullptr)
            sc.Flush();
        sc._currentSheet = sheet;
    }

    UI::SpriteDraw draw;
    draw.sheet       = sheet;
    draw.spriteIndex = 0;
    draw.x1          = 0;
    draw.y1          = 0;
    draw.x2          = _captureWidth;
    draw.y2          = _captureHeight;
    draw.rotation    = 0.0f;
    draw.color       = 0xffffffffu;
    draw.isLine      = 0;
    // +0x25..+0x27 are NOT written by retail (the stores stop at the byte at
    // +0x24), so they are left alone here too.
    sc.AddSpriteDraw(draw);
    sc.Flush();

    if (sc._spriteCapture != nullptr)
        sc._spriteCapture->EndAddingSprites();

    // ---- put the context back ---------------------------------------------
    // The scissor is set to the FULL DEVICE and is NOT restored — the only
    // piece of shared state this function leaves changed besides the readback.
    if (ctx->_scissor.x != 0 || ctx->_scissor.y != 0 ||
        ctx->_scissor.width != devW || ctx->_scissor.height != devH) {
        ctx->_dirty |= 0x400u;
        ctx->_scissor.x      = 0;
        ctx->_scissor.y      = 0;
        ctx->_scissor.width  = devW;
        ctx->_scissor.height = devH;
    }
    if (ctx->_renderBuffer != savedRenderBuffer) {
        ctx->_dirty |= 0x1u;
        ctx->_renderBuffer = savedRenderBuffer;
    }
    if (ctx->_viewport.x      != savedViewport.x     ||
        ctx->_viewport.y      != savedViewport.y     ||
        ctx->_viewport.width  != savedViewport.width ||
        ctx->_viewport.height != savedViewport.height) {
        ctx->_dirty |= 0x200u;
        ctx->_viewport = savedViewport;
    }

    // ---- read back --------------------------------------------------------
    // `IMUL EAX,...; MOVSXD RCX,EAX; SHL RCX,2` — the product is computed in
    // 32 bits and only THEN sign-extended, so a capture large enough to overflow
    // an int would ask for a negative size. Reproduced rather than widened.
    {
        const int32_t pixels = _captureWidth * _captureHeight;
        void* buffer = MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(pixels) << 2), 4);
        _capturePixels = buffer;

        // ImageBuffer+0x60 is the backend companion object; its vtable slot
        // 0x30 is slot INDEX 6, which dx11/types/VideoDX11Data.h already names
        // `kImg_Read` — "staging copy + Map/READ readback" (0x180002ef0). So
        // this is the GPU->CPU readback, identified by our own DX11 lane's slot
        // table rather than guessed from the argument shape.
        void* platform = target->platformData;
        using ReadBackFn = void (*)(void*, void*, int32_t, int32_t);
        (*reinterpret_cast<ReadBackFn*>(
            reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(platform)) + 0x30))(
                platform, buffer, _captureWidth, _captureHeight);
    }

    // ---- tear down --------------------------------------------------------
    // vtable[0](obj, 1) is MSVC's scalar deleting destructor. Unguarded, as
    // retail is; see the banner.
    using DeletingDtorFn = void (*)(void*, int32_t);
    (*reinterpret_cast<DeletingDtorFn*>(*reinterpret_cast<void**>(sheet)))(sheet, 1);
    (*reinterpret_cast<DeletingDtorFn*>(*reinterpret_cast<void**>(mat)))(mat, 1);
    (*reinterpret_cast<DeletingDtorFn*>(*reinterpret_cast<void**>(target)))(target, 1);
    (*reinterpret_cast<DeletingDtorFn*>(*reinterpret_cast<void**>(frameCopy)))(frameCopy, 1);

    if (renderBuffer != nullptr) {
        reinterpret_cast<Video::RenderBuffer*>(renderBuffer)
            ->PlatformDestruct();                            // B.3.5 bridge
        RT_EXPORT_ONCE(VoidThisFn, "??3RenderBuffer@Video@@SAXPEAX@Z")(renderBuffer);
    }

    // The handshake with UpdateSave state 1: a BYTE store, clearing the request.
    *reinterpret_cast<uint8_t*>(&_captureFlags) = 0;

    // The SpriteContext destructor, also inlined by retail: four Array<T>
    // frees in the order +0x68, +0x58, +0x40, +0x10 (0x1800c3acd..0x1800c3af8).
    // ENDGAME B.3.7 B2: that IS `UI::SpriteContext`'s own member-wise
    // destructor (SpriteContext.h: `~SpriteContext() = default` is retail's
    // body) — reverse declaration order runs `_layout._lineWidths` (+0x68),
    // `_layout._entries` (+0x58), `_deferred` (+0x40), `_draws` (+0x10), each
    // `if (data) free; zero` — so `sc` is simply destroyed at the closing brace
    // below, through the Collection allocator door its arrays were grown with.
}

// ============================================================================
// 5. The export.
// ============================================================================
void RtSave_SaveLoad_Draw(SaveLoad* self, Context* ctx, void* material) {
    self->Save::SaveLoad::Draw(*ctx, Object::ExternalPtr<Graphics::Material>(
                                         static_cast<Graphics::Material*>(material)));
}

// ============================================================================
// 7. Registration — BOTH doors, for the reason saveload.cpp's own block states:
//    a name door alone is one typo away from resolving NULL in silence, and an
//    address door alone is invisible to anything that resolves by name.
//    `code/Game/Game/gameupdateloop.cpp:645` reaches this export BY NAME, so
//    the name door is the live one; the address door is the belt.
//
//    NO OTHER ADDRESS IS REGISTERED HERE. Every callee is an exported name and
//    the four unexported internals are transcribed above, so this port does not
//    grow the `called` column with by-address doors
//    (reference_porting_can_grow_the_cutover_surface.md).
// ============================================================================
int RtSaveLoadDrawRegister() {
    int n = 0;
    // ON ONE LINE, DELIBERATELY. tools/runtime_census.py matches
    // `RegisterRuntimeExport\s*\(\s*(?=")` and its "non-literal first argument"
    // guard matches `(?!")[A-Za-z_]` -- a call split so that the open paren and
    // the literal are on DIFFERENT lines satisfies neither, so a line-oriented
    // scan would count this port as unregistered AND not flag it. That is the
    // same silent-blind-spot family as
    // reference_census_cannot_see_nonliteral_registrations.md, reached by
    // formatting rather than by a named constant.
    RegisterRuntimeExport("?Draw@SaveLoad@Save@@QEAAXAEAVContext@Video@@V?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z", (void*)&RtSave_SaveLoad_Draw); ++n;
    RegisterRuntimeAddr(0x1800c2eb0, (void*)&RtSave_SaveLoad_Draw);
    return n;
}
