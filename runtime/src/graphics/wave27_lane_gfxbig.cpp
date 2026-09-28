// ============================================================================
// runtime/src/graphics/wave27_lane_gfxbig.cpp — WAVE 27 LANE GFXBIG
// The three large Graphics BODY rows of the census gap.
// ============================================================================
//
//   | Export                                        | Retail      | Ord  | Bytes |
//   |-----------------------------------------------|-------------|------|-------|
//   | ?BuildParameters@MaterialInstance@Graphics@@   | 0x1801033e0 | 1678 |  1421 |
//   | ?Draw@Renderer@Graphics@@                      | 0x180104bb0 | 2258 |   920 |
//   | ?Update@AnimationState@Graphics@@              | 0x1800fdb20 | 4474 |  1062 |
//
// Every address and ordinal above is `pefile`'s reading of the export directory
// of the shipped C:\GOG Games\Banished\Runtime-x64.dll, taken before a line of
// this file was written (reference_resolveexport_phantom_names.md). The row list
// was re-derived with `tools/runtime_census.py --unported` on this tip and
// filtered to ported==0 && symbol_kind==BODY && namespace==Graphics; that filter
// returns FOUR rows, and the fourth (?AddDrawItems@MeshInstance@Graphics@@)
// belongs to lane GFXMESH, not here.
//
// ---------------------------------------------------------------------------
// *** THE SOURCE OF TRUTH FOR EVERY BYTE BELOW IS objdump, NOT GHIDRA. ***
// ---------------------------------------------------------------------------
// The lane brief said to prefer `disassemble_function` over
// `decompile_function`. That was the right instinct and it was not sufficient:
// on `?Update@AnimationState@Graphics@@` (0x1800fdb20) the ghidra-mcp tool
// `disassemble_function(address="0x1800fdb20")` returned THE WRONG FUNCTION'S
// LISTING — 126 instructions spanning 0x1800fd9a0..0x1800fdb58, an
// `Array<T>::SetSize` of stride 0x18 — while printing them under the addresses
// that had been asked for. It reported the instruction at 0x1800fdb20 as
// `LEA RAX,[RBX + -0x8]`.
//
// The bytes in the shipped file at that address are
//   4c 89 4c 24 20   48 89 54 24 10   53 55 56 41 55   48 81 ec 98 00 00 00
// = MOV [RSP+0x20],R9 / MOV [RSP+0x10],RDX / PUSH RBX / PUSH RBP / PUSH RSI /
//   PUSH R13 / SUB RSP,0x98 — a prologue, which is what an export entry has to
// be. Ghidra holds TWO OVERLAPPING function bodies here (`FUN_1800fd9a0`
// claiming 0x1800fd9a0-0x1800fdb58 and `Update` claiming
// 0x1800fdb20-0x1800fdf45) and the endpoint resolves an address to the first
// body that contains it. **It gave no warning of any kind.**
//
// So every listing behind this file was taken from `objdump -d -M intel` over
// the shipped DLL. Ghidra was used only for what it is authoritative about here
// — the function-extent question — and even that was cross-checked against the
// PE `.pdata` table, which is why no extent above is quoted from .pdata alone:
// `?Update@AnimationState@Graphics@@` is assembled from FIVE CHAINED .pdata
// fragments (0x1800fdb20 -> 0xfdb71 -> 0xfdcc0 -> 0xfdf15 -> 0xfdf34 ->
// 0xfdf46), so its first fragment's 81 bytes understate a 1062-byte body 13x
// (reference_pdata_is_a_lower_bound_on_function_extent.md).
//
// ---------------------------------------------------------------------------
// THE GFXR LESSON, RE-RUN ON THIS LANE'S OWN CALLEES
// ---------------------------------------------------------------------------
// wave9_lane_gfxd.cpp's banner is emphatic that a source-tree grep records an
// unnamed internal where the EXPORT TABLE has a name. Every callee of all three
// bodies was put through pefile before it was written down, and the payoff was
// large: of thirteen distinct callees, NINE are ordinary named exports.
//
//   0x180060d30  Array<MaterialParameterSet>::SetSize   NOT EXPORTED  (ported here)
//   0x180102900  ?GetTextureName@Material@Graphics@@           ord 2970
//   0x180103020  ?FindTexture@MaterialInstance@Graphics@@      ord 2361
//   0x180102830  ?GetConstantName@Material@Graphics@@          ord 2506
//   0x180102f40  ?FindConstant@MaterialInstance@Graphics@@     ord 2343
//   0x180105110  ?Reset@Renderer@Graphics@@                    ord 3574
//   0x180105700  ?AddScene@Renderer@Graphics@@                 ord 1579
//   0x180104fd0  ?BeginDrawGroup@Renderer@Graphics@@           ord 1640
//   0x1801017a0  ?AddCulledDrawItems@Scene@Graphics@@          ord 1543
//   0x180062d60  the draw-item sort                 NOT EXPORTED (ported, BATCH 8b)
//   0x180063b90  its selection-sort callee          NOT EXPORTED (ported, BATCH 8b)
//   0x1800fd4c0  ?CopyInstancedToUnique@AnimationState@        ord 1868
//   0x1800fc870  ?UpdateRootOnly@AnimationState@Graphics@@     ord 4525
//   0x1800fcc20  ?UpdateNodes@AnimationState@Graphics@@        ord 4522
//   0x1800fa9a0  ?ApplyEvents@Animation@Graphics@@             ord 1605
//   0x18005aa30  Array<Node>::SetSize                   NOT EXPORTED  (door)
//   0x18005b1e0  Array<AnimationState>::SetSize         NOT EXPORTED  (door)
//
// FOUR of those exported names DECIDED AN ARGUMENT MAPPING that the prologue
// alone could not, which is the whole reason the lookup came first:
// `?UpdateNodes@…QEAAXMAEBV?$Array@…@AEBVTransform@Math@@PEAV?$InterpolatedState
// @…@PEAV56@AEAVVector3@6@@Z` spells six parameters with a FLOAT FIRST, and that
// is what says the `MOVAPS XMM1,XMM6` at 0x180104c03's analogue is argument 2
// rather than a spill (reference_read_the_callee_before_holding_back_on_an_arg.md).
//
// ---------------------------------------------------------------------------
// WHAT THIS LANE DOES NOT CLAIM
// ---------------------------------------------------------------------------
// THREE unexported internals are NOT ported and are called at their retail
// addresses through documented address doors, each commented at its call site in
// the shape WAVE27_KICKOFF.md §4 #10 asks for (SYMBOL / CONDITION /
// CONSEQUENCE). They are sized and given a NAMED ADDRESSEE in
// docs/runtime_port/WAVE27_LANE_GFXBIG.md §6. None is a vtable slot and none
// belongs to another lane's table.
//
// The ONE unexported internal this lane DID port is 0x180060d30, because
// BuildParameters cannot run without it and it is a plain `SetSizeCore`
// instantiation that lane Q's parameterisation already covers.
//
// NO VTABLE ROW IS TOUCHED. All three bodies make virtual calls — Draw makes
// four kinds — and every one of them is a dispatch through an object's own
// vtable, not a slot this lane owns or stamps. `?AddDrawItems@MeshInstance@` is
// reached as slot 3 from Draw's scene walk; it is lane GFXMESH's row and is left
// entirely alone here.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/graphics/AnimationState.h"
#include "../../types/graphics/AnimationManager.h"   // B.3.9: AnimationManager::_singleton
#include "../../types/graphics/Animation.h"   // ENDGAME B.3.5 closing pass:
                                              // Graphics::Animation, the clip record
                                              // ApplyEvents is called on
#include "../../types/system/Value.h"   // System::Time -- ENDGAME B.3.5 block GC
#include "../../types/system/Timer.h"   // ENDGAME B.3.9 close: Timer::_secondsPerTick
#include "../../types/graphics/Camera.h"
#include "../../types/graphics/Scene.h"   // ENDGAME B.3.5 caller batch: Graphics::Scene
#include "../../types/graphics/Light.h"   // B3X W0: Light::DrawShadowMap / DrawLight by name
#include "../../types/graphics/Material.h"
#include "../../types/video/Context.h"
#include "../collection/array_internal.h"
#include "../collection/introsort_msvc.h"
#include "../system/string.h"

// Byte-identical to standalone/resolve.h's declaration; the linker checks the
// two against each other. Not included, for the <wingdi.h> `Rectangle` reason
// wave21_lane_gfxbody.cpp:47 gives at length.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace rtgfxbig {

using RtGraphics::AnimationClipPrefix;
using RtGraphics::AnimationLayer;
using RtGraphics::AnimationState;
using RtGraphics::InterpolatedTransform;
using RtGraphics::MaterialBinding;
using RtGraphics::MaterialInstance;
using RtGraphics::MaterialParameterSet;
using RtGraphics::Renderer;
using RtGraphics::RendererDrawGroup;
using RtGraphics::RendererDrawable;
using RtGraphics::RendererDrawItem;
using RtGraphics::RendererSceneEntry;
using RtSystem::String;
using rtcoll::ArrayHdr;
using rtcoll::ArraySpec;

// ---------------------------------------------------------------------------
// Attendance. Counts the DOOR and, separately, the BRANCH that does the work.
// `calls=0` and `calls=N with the interesting arm never taken` are different
// findings and an entry counter cannot tell them apart
// (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md).
// ---------------------------------------------------------------------------
void Attend(const char* what, unsigned long long& n) {
    ++n;
    if (n == 1 || n == 10 || n == 1000 || (n % 100000) == 0)
        printf("[GFXBIG27] %s calls=%llu\n", what, n);
}

unsigned long long g_buildParamCalls  = 0;
unsigned long long g_buildParamBinds  = 0;   // BRANCH: bindings actually appended
unsigned long long g_drawCalls        = 0;
unsigned long long g_drawGroups       = 0;   // BRANCH: scene entries walked in pass 1
unsigned long long g_drawSorts        = 0;   // BRANCH: item lists actually sorted
unsigned long long g_animStateCalls   = 0;
unsigned long long g_animStateLayers  = 0;   // BRANCH: layers actually stepped
unsigned long long g_animStateExpired = 0;   // RARE ARM: layers removed

// ============================================================================
// §1.  THE ARRAY MACHINERY UNDER BuildParameters
// ============================================================================
// BuildParameters reaches Collection::Array in three DIFFERENT shapes, not one
// repeated:
//
//   (a) 0x180060d30  Array<MaterialParameterSet>::SetSize(int)   [NOT EXPORTED]
//       stride 0x20, align 8, NON-TRIVIAL element (two sub-Arrays). Called ONCE
//       at the top of BuildParameters. PORTED, via lane Q's SetSizeCore.
//   (b) the append into `set.textures` / `set.constants` — element stride 8,
//       **align 4** — which retail INLINES rather than calls. Also SetSizeCore,
//       with a `fill`.
//   (c) System::String release, inlined at the two Get*Name sites.
//
// *** (b) IS NOT kSpec_Fill8. *** kSpec_Fill8 (array.cpp:1012) is stride 8
// ALIGN 8. The allocation inlined at 0x1801035c4 and 0x1801037ec is
// `MOV R8D,0x4` — align FOUR. The element is a {int32,int32} pair, so 4 is the
// honest alignment and 8 would be a different call into the allocator. A spec is
// defined here rather than reusing the neighbour whose NAME matches, because the
// thing that has to match is the immediate in the instruction (array.cpp's own
// rule: "`align` is the immediate in EDX at the MemAlloc call site"). Checked
// against the bytes, not inferred from the element size.
// ----------------------------------------------------------------------------

// 0x180005800 is `Collection::Array<T>::~Array` for a trivially-destructible T,
// instruction-for-instruction lane Q's `ArrayDtorImpl`: MemFree(_data) (the null
// test lives inside 0x1800c7ad0), then _data = 0 and the _size/_capacity qword
// = 0. NOT re-written here; lane Q's body is called
// (reference_grep_for_the_port_before_writing_it.md).

// `Collection::Array<{int32,int32}>::operator=` — retail 0x180005770.
// Destroys the destination first, then copies `src._size` elements. The loop
// bound and both data pointers are RE-READ every iteration (`CMP R8D,[RBX+8]`
// at 0x1800057e9, `MOV RAX,[RDI]` at 0x1800057d8); transcribed unhoisted because
// that is what the bytes do, not for style.
void Array8Assign(ArrayHdr* dst, const ArrayHdr* src) {
    if (dst == src) return;                       // CMP RCX,RDX / JZ

    // ENDGAME B.3.2 step 2 (callers) — `dst` is always `&set->textures` or
    // `&set->constants` (PassAssign, below), both real
    // `Collection::Array<MaterialBinding>` members (Material.h). MaterialBinding
    // is a plain POD, so `Array<MaterialBinding>::~Array` is trait-derived and
    // bit-for-bit `rtcoll::ArrayDtorImpl` (spec->destroy is null either way).
    reinterpret_cast<Collection::Array<MaterialBinding>*>(dst)->~Array();  // CALL 0x180005800

    const int32_t n       = src->_size;           // MOV ECX,[RDI+8]
    const int32_t oldSize = dst->_size;           // MOV EDX,[RBX+8]  (0 by now)
    dst->_size = n;

    if (n > dst->_capacity) {
        dst->_capacity = n;
        // Retail's `*3` bump. DEAD on this path — ArrayDtorImpl just zeroed
        // _size, so `oldSize != 0` never holds. Written anyway: it is in the
        // bytes, and a future caller reaching this body with a live destination
        // would see the difference.
        if (n == static_cast<int32_t>(oldSize + 1) && oldSize != 0)
            dst->_capacity = static_cast<int32_t>(n * 3);
        dst->_data = static_cast<uint8_t*>(
            rtcoll::MemAlloc(static_cast<uint64_t>(static_cast<int64_t>(dst->_capacity)) * 8,
                             4));                 // MOV EDX,0x4 at 0x1800057b0
    }
    for (int32_t i = 0; i < dst->_size; ++i) {    // bound re-read: see above
        uint8_t* const d = dst->_data + static_cast<int64_t>(i) * 8;
        if (d == nullptr) continue;               // retail tests the COMPUTED address
        *reinterpret_cast<uint64_t*>(d) =
            *reinterpret_cast<const uint64_t*>(src->_data + static_cast<int64_t>(i) * 8);
    }
}

// The ASSIGN arm of Array<MaterialParameterSet>::SetSize, read at 0x180060dd5.
// Retail ZEROES each sub-array header before calling operator= on it, because the
// destination is raw MemAlloc memory and operator= starts by DESTROYING the
// destination — i.e. by freeing whatever pointer it finds there. Skipping the
// zero would hand MemFree a garbage pointer.
void PassAssign(uint8_t* dst, const uint8_t* src) {
    reinterpret_cast<uint64_t*>(dst)[0] = 0;              // MOV [RBX],R8 (R8==0)
    reinterpret_cast<uint64_t*>(dst)[1] = 0;              // MOV [RBX+8],0
    Array8Assign(reinterpret_cast<ArrayHdr*>(dst),
                 reinterpret_cast<const ArrayHdr*>(src));

    reinterpret_cast<uint64_t*>(dst + 0x10)[0] = 0;       // LEA RCX,[RBX+0x10] ...
    reinterpret_cast<uint64_t*>(dst + 0x10)[1] = 0;
    Array8Assign(reinterpret_cast<ArrayHdr*>(dst + 0x10),
                 reinterpret_cast<const ArrayHdr*>(src + 0x10));
}

// The INIT arm, read at 0x180060e1b: four zeroed qwords, nothing else.
void PassInit(uint8_t* dst) {
    reinterpret_cast<uint64_t*>(dst)[0] = 0;
    reinterpret_cast<uint64_t*>(dst)[1] = 0;
    reinterpret_cast<uint64_t*>(dst)[2] = 0;
    reinterpret_cast<uint64_t*>(dst)[3] = 0;
}

// The DESTROY arm — retail 0x180005680, a loop of `ArrayDtorImpl` over the two
// sub-arrays. THE ORDER IS +0x10 FIRST, THEN +0x00 (RBX starts at base+0x10 and
// RSI trails it by 0x10, 0x1800056b1/0x1800056ff). Preserved: the two frees reach
// the allocator in that order and the allocator is shared state.
void PassDestroy(void* /*ignoredThis*/, uint8_t* base, int count) {
    if (count <= 0) return;                       // TEST R8D,R8D / JLE ret
    for (int i = 0; i < count; ++i) {
        // `e` is one `MaterialParameterSet` (Material.h): `constants` @ +0x10,
        // `textures` @ +0x00 — trait-equivalent for the same MaterialBinding
        // reason Array8Assign's dtor call gives.
        MaterialParameterSet* const set =
            reinterpret_cast<MaterialParameterSet*>(base + static_cast<int64_t>(i) * 0x20);
        set->constants.~Array();
        set->textures.~Array();
    }
}

// The element assign for (b): one 8-byte copy.
void Binding8Assign(uint8_t* dst, const uint8_t* src) {
    *reinterpret_cast<uint64_t*>(dst) = *reinterpret_cast<const uint64_t*>(src);
}

const ArraySpec kSpec_MatPass = {                 // 0x180060d30
    "Array<Graphics::MaterialParameterSet>", 0x20, 8,
    &PassAssign, &PassInit, &PassDestroy,
    /*byteCopy=*/false,
    /*assignUnwritten=*/0, 0, /*initUnwritten=*/0, 0, /*seedZero=*/0, 0x20 };

// `byteCopy` is FALSE and `seedZero` covers the WHOLE 0x20-byte element, both
// deliberately: the element operations DEREFERENCE every part of it (each
// sub-array's `_data` goes to MemFree), so a probe that seeded this element with
// a varying non-zero pattern would hand the allocator two wild pointers on the
// first assign. That is exactly what array_internal.h's `seedZero` exists for.
// *** NO PROBE DRIVES kSpec_MatPass TODAY *** — said out loud so a correctly
// filled-in field is not read as evidence that one does.

const ArraySpec kSpec_Binding8 = {                // INLINED into BuildParameters
    "Array<Graphics::MaterialBinding>", 8, 4,
    &Binding8Assign, nullptr, nullptr,
    /*byteCopy=*/true, 0, 0, 0, 0, 0, 0 };

// Retail 0x180060d30, 420 bytes, NOT EXPORTED. Registered BY ADDRESS below.
//
// ENDGAME B.3.2 step 2 (callers) — bridged through `AsArray<&kSpec_MatPass>`
// rather than naming `rtcoll::SetSizeCore` directly. NOT trait-equivalent:
// `MaterialInstance::parameters` is a real `Collection::Array<
// MaterialParameterSet>` (Material.h), but MaterialParameterSet has no
// user-defined copy constructor, so a trait derivation would placement-copy
// it via the implicit (shallow, `=default`) copy of its two inner Arrays —
// not the zero-then-deep-assign `PassAssign` performs.
extern "C" void RtCollection_Array_MaterialParameterSet_SetSize(void* a, int n) {
    rtcoll::AsArray<&kSpec_MatPass>(a)->SetSize(n);
}

// ============================================================================
// §2.  ?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ
//      retail 0x1801033e0, ordinal 1678. 1421 bytes / 361 instructions.
// ============================================================================
// Resolves every texture and constant NAME the bound Material declares into an
// INDEX into this instance's own texture/constant arrays, and caches the
// resulting {index, shaderSlot} pairs per pass. `?Setup@MaterialInstance@`
// (already ported, Wave 21 lane GFXBODY) is the consumer.
//
// runtime/types/graphics/Material.h WAS ALREADY PINNED FROM THIS BODY by Wave 6
// lane F — it names BuildParameters beside `hasRenderTexture`,
// `hasRenderedTexture`, `shaderClass` and `MaterialParameterSet`. Every one of
// those readings held against the disassembly, which is worth stating because
// the SIGN of such an agreement carries no information until somebody runs it
// (reference_ghidra_c_output_vs_disassembly.md). Nothing in Material.h needed a
// correction.
//
// THE TWO Get*Name CALLS RETURN A String BY VALUE, SO THERE IS A HIDDEN RETURN
// SLOT, and it is the SECOND argument (RCX=material, RDX=&slot, R8D=pass,
// R9D=index). An `fn_*` typedef that omitted it would be a wild write that still
// returned the right answer, because RAX comes back holding the slot address
// either way (reference_hidden_retslot_wild_write.md). Both mangled names say
// `?AVString@System@@` for the return, so this is read off the export table, not
// guessed from the call site.
//
// THE FINAL LOOP IS ASYMMETRIC AND THE ASYMMETRY IS RETAIL'S. `+0x24` is
// CLEARED before the loop (`MOV BYTE PTR [R15+0x24],0x0` at 0x18010390c) and
// `+0x25` is NOT — it is loaded into R8B (`MOVZX R8D,BYTE PTR [R15+0x25]` at
// 0x18010391b) and OR'd into. So a second BuildParameters on the same instance
// can only ever ADD to `hasRenderedTexture`, never clear it, while
// `hasRenderTexture` is recomputed from scratch. Transcribed, not tidied.
// ----------------------------------------------------------------------------
namespace {

// Both return `System::String` by value; RDX is the hidden return slot.
using GetNameFn = String* (*)(const void* material, String* ret, int pass, int index);
using FindFn    = int32_t (*)(MaterialInstance* self, const String* name);

// The per-pass texture/constant DESCRIPTION arrays hang off the material's pass
// object at +0x30. Both are {data, size} pairs and both hold 0x10-byte records:
//   desc + 0x20 / 0x28   texture descriptions   (0x1801034b0, 0x180103479)
//   desc + 0x30 / 0x38   constant descriptions  (0x18010377d, 0x1801036ca)
// Within a record: +0x08 is the shader slot (int32), and for TEXTURES ONLY
// +0x0c is a SKIP flag (`CMP BYTE PTR [RDX+R12*1+0xc],0x0 / JNZ` at
// 0x1801034b4). The constant loop has NO such test — checked, not assumed by
// symmetry.
constexpr uint32_t kPassDesc      = 0x30;
constexpr uint32_t kTexDescData   = 0x20;
constexpr uint32_t kTexDescCount  = 0x28;
constexpr uint32_t kConDescData   = 0x30;
constexpr uint32_t kConDescCount  = 0x38;

// ENDGAME B.3.5 block GC: the receiver is the REAL `Graphics::MaterialInstance`
// and the three-step chain is spelled with the two real classes' own members —
// `_material` (`ExternalPtr<Material>` by value, +0x28) then that `Material`'s
// `_passes` array (+0x28). Retail's `MOV RAX,[R15+0x28] / MOV RCX,[RAX+0x28] /
// MOV RAX,[RSI+RCX*1]` is unchanged; only the spelling is, and `Material.h`'s
// `static_assert`s now hold the offsets.
inline uint8_t* PassObject(const Graphics::MaterialInstance* m, int32_t pass) {
    // Re-read from `this` on EVERY use in retail (five separate reloads inside
    // the two inner loops), so it is a function rather than a hoisted local.
    return reinterpret_cast<uint8_t*>(                            // B.3.5 bridge
        m->_material.Get()->_passes._data[pass].Get());
}

}  // namespace

// ENDGAME B.3.5 block GC: THE BODY MOVED to
// `Graphics::MaterialInstance::BuildParameters`, at the bottom of this file. The
// door keeps its name, its registration and both counters.
extern "C" void RtGraphics_MaterialInstance_BuildParameters(void* self) {
    Attend("MaterialInstance::BuildParameters", g_buildParamCalls);
    reinterpret_cast<Graphics::MaterialInstance*>(self)            // B.3.5 bridge
        ->Graphics::MaterialInstance::BuildParameters();
}

// The BODY is at the bottom of this file, as `Graphics::MaterialInstance::BuildParameters`.
// ============================================================================
// §3.  ?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@AEBVCamera@2@@Z
//      retail 0x180104bb0, ordinal 2258. 920 bytes / 191 instructions.
// ============================================================================
// The frame's top-level render dispatch, and it is TWO PASSES over the scene
// entry array, not one:
//
//   PASS 1 (forward, 0x180104c31..0x180104d29) — CULL AND COLLECT. For each
//       scene entry: BeginDrawGroup, walk the scene's drawable list calling
//       AddDrawItems (vtable slot 3) on everything whose pass mask matches and
//       whose +0x28 is within the entry's field_f8, optionally
//       Scene::AddCulledDrawItems, then SORT each of the three per-pass item
//       lists.
//
//   PASS 2 (BACKWARD, 0x180104d60..0x180104f1d) — EMIT. r13 counts DOWN from
//       scenes._size-1 to 0 (`DEC R13 / JNS`). For each entry: a prepare call
//       on every drawable (slot 9), then rebind the render buffer, reconcile the
//       viewport and the scissor, Clear, and finally three passes x N drawables
//       of the draw call (slot 10).
//
// THE BACKWARD DIRECTION IS NOT COSMETIC — render-to-texture entries are
// produced before the entry that samples them, so reversing the order would
// sample a stale target. It is transcribed as a decrementing signed counter with
// the `JNS` termination retail uses, not rewritten as a forward loop.
//
// WAVE 6 LANE F ALREADY PINNED EVERY STRUCT THIS BODY TOUCHES, FROM THIS BODY.
// runtime/types/graphics/Camera.h says so in as many words ("One scene entry,
// stride 0x110. Pinned by Renderer::Draw (0x180104bb0), which reads every one of
// these fields"). This lane re-read all of them against the disassembly and
// found NO correction needed — including the two deliberate holes at Renderer
// +0x3c and +0x4c, which this body is the reason for: it ASSIGNS +0x3c from a
// scene entry in pass 2 before anything reads it, and reads it in pass 1 before
// anything writes it, so on the first frame AddScene is handed whatever the
// caller's memory held. That is retail's behaviour and this port reproduces it
// rather than zeroing the field.
// ----------------------------------------------------------------------------
namespace {

using ResetFn          = void (*)(Renderer*);
using AddSceneFn       = void (*)(Renderer*, const void* scene, const void* renderBuffer,
                                  const void* camera, int32_t, int32_t, int32_t, uint32_t);
using BeginDrawGroupFn = int32_t (*)(Renderer*, const void* renderBuffer,
                                     const void* camera, const void* camera2,
                                     int32_t, uint32_t);
// ENDGAME B.3.5 caller batch: `AddCulledFn` is gone with its door — the member
// call in `Renderer::Draw` needs no typedef.

// (B3X W0: slot 3 of a scene drawable -- `Geometry::AddDrawItems` -- and slots
// 9 / 10 of a Renderer drawable -- `Light::DrawShadowMap` / `DrawLight` -- are
// by-name calls on the real classes now; their typedefs are gone.)
// slot 4 of Context::_contextData: Clear(flags, colour, depth, stencil)
using ClearFn = void (*)(void* self, int32_t flags, uint32_t colour, float depth, int32_t stencil);

inline void** VTable(void* obj) { return *reinterpret_cast<void***>(obj); }

// ============================================================================
// Graphics::Renderer::DrawItemSort       retail 0x180062d60  (2311 bytes)
// Graphics::Renderer::DrawItemSortSmall  retail 0x180063b90  ( 344 bytes)
//   - BATCH 8b.  Replaces BATCH 8's std::sort, which was NOT faithful.
// ============================================================================
// Neither address is exported (checked against the PE export directory), so
// both are address-only rows. Both are now ported and registered; the
// `ResolveAddr` in Draw's pass loop therefore lands on our body, because the
// resolver consults the port registry first.
//
// WHY THE PREVIOUS PORT WAS REPLACED. BATCH 8 wrote this as
// `std::sort(first, last, DrawItemLess)` and argued that the comparator is a
// strict total order over distinguishable items, so any correct sort gives the
// same array and the algorithm need not be retail's. Reading the instructions
// instead of reconstructing them shows two defects:
//
//   1. THE FLOAT TERM WAS BACKWARDS. At 0x180062e0e retail does
//        MOVSS XMM0,[mid+0x1c] / MOVSS XMM1,[lo+0x1c]
//        COMISS XMM0,XMM1 / JA  <swap, i.e. "mid sorts before lo">
//        COMISS XMM1,XMM0 / JA  <no swap>
//      so `sortKey` is DESCENDING. The std::sort port had `a.sortKey <
//      b.sortKey`. Every frame's draw order was wrong in that term.
//   2. "EQUAL IMPLIES INDISTINGUISHABLE" IS FALSE HERE. `arg8` (+0x20) and
//      `_pad24` (+0x24) are never compared. Two items that tie on all eight
//      compared terms can still differ in `arg8`, and then which one lands
//      first is decided by the swap sequence - so the swap sequence must be
//      retail's.
//
// FUN_180063b90 IS NOT AN INSERTION SORT, which BATCH 8 also assumed when it
// declined to give it a body ("std::sort's own small-range handling subsumes
// it"). It is a SELECTION SORT that repeatedly moves the MAXIMUM of [lo,hi] to
// `hi`: 0x180063c47 `MOV RDI,R8` sits on the JB arms, so the tracked position
// advances only on STRICTLY less. It is a distinct closure row and is ported
// and registered here in its own right.
//
// The skeleton itself lives in ../collection/introsort_msvc.h, transcribed once
// from these bytes and instantiated a second time for the Array<Mod> pair
// (0x180028760 / 0x180028fc0), which is the same template with a different
// element, comparator and swap.
//
// NO ORACLE WRAPPER: `sort` mutates the pass list in place and returns nothing,
// so there is no value to replay. The diff harness sees it as a registered
// address; the verdict is Draw's per-frame attendance climbing with the frame
// on screen unchanged.
// ============================================================================

// The comparator. Every term below is one `CMP`/`COMISS` pair read out of
// 0x180062dfe..0x180062e85, and the identical chain is repeated at six more
// sites across the two bodies (the three median-of-three tests, the two
// partition scans, the two equal-run walks and the selection sort's inner
// loop). The field ORDER is the order of the compares, not of the struct.
bool DrawItemLess(const RendererDrawItem& a, const RendererDrawItem& b) {
    // +0x18, MOVZX / CMP AX,CX - UNSIGNED 16-bit.  JB -> less, JA -> not less.
    if (a.flags != b.flags) return a.flags < b.flags;

    // +0x1c, COMISS both ways - DESCENDING.  Written as two one-sided tests
    // rather than `!=` so that a NaN (for which both JA arms fail) falls
    // through to the next term exactly as retail's does.
    if (a.sortKey > b.sortKey) return true;
    if (b.sortKey > a.sortKey) return false;

    // The remaining six are unsigned integer compares, in this order.
    if (a.material != b.material)
        return reinterpret_cast<uintptr_t>(a.material) <
               reinterpret_cast<uintptr_t>(b.material);            // +0x08
    if (a.instance != b.instance)
        return reinterpret_cast<uintptr_t>(a.instance) <
               reinterpret_cast<uintptr_t>(b.instance);            // +0x10
    if (a.arg5  != b.arg5)  return a.arg5  < b.arg5;               // +0x1a
    if (a.arg10 != b.arg10) return a.arg10 < b.arg10;              // +0x30
    if (a.userData != b.userData)
        return reinterpret_cast<uintptr_t>(a.userData) <
               reinterpret_cast<uintptr_t>(b.userData);            // +0x28
    // +0x00, the last term: `CMP RAX,[RBP] / JAE <not less>`.
    return reinterpret_cast<uintptr_t>(a.geometry) <
           reinterpret_cast<uintptr_t>(b.geometry);
}

// Retail swaps the 0x38-byte element as SEVEN qword moves (0x180062e87 and the
// five other swap sites), reading one side into registers first. Self-swap is
// a no-op either way, which matters because the selection sort can call it
// with best == last.
void DrawItemSwap(RendererDrawItem* a, RendererDrawItem* b) {
    uint64_t* const pa = reinterpret_cast<uint64_t*>(a);
    uint64_t* const pb = reinterpret_cast<uint64_t*>(b);
    for (int k = 0; k < 7; ++k) {
        const uint64_t t = pa[k];
        pa[k] = pb[k];
        pb[k] = t;
    }
}

}  // namespace

// 0x180063b90 - the small-range pass. `lo`/`hi` are INCLUSIVE bounds, as
// 0x180062dc1 passes them (`MOV RDX,R15 / MOV RCX,RBP`).
extern "C" void RtGraphics_Renderer_DrawItemSortSmall(void* lo, void* hi) {
    rtsort::SelectionSortMaxToHigh(static_cast<RendererDrawItem*>(lo),
                                   static_cast<RendererDrawItem*>(hi),
                                   DrawItemLess, DrawItemSwap);
}

// 0x180062d60. Matches the call site's `void(void* data, int32_t count,
// void* scratch)`; `scratch` is retail's spare register window and the body
// never reads R8.
extern "C" void RtGraphics_Renderer_DrawItemSort(void* data, int32_t count, void*) {
    rtsort::Introsort(static_cast<RendererDrawItem*>(data), count,
                      DrawItemLess, DrawItemSwap,
                      [](RendererDrawItem* lo, RendererDrawItem* hi) {
                          RtGraphics_Renderer_DrawItemSortSmall(lo, hi);
                      });
}

// ENDGAME B.3.5 block GC: THE BODY MOVED onto `Graphics::Renderer::Draw`, in
// this file's trailing `namespace Graphics` block (everything here is inside
// `namespace rtgfxbig`, which is `Graphics`'s SIBLING -- B34_BRIEFS A.2). The
// door keeps its name, its registration and all three counters; the two BRANCH
// counters stay in the loops, inside the member.
extern "C" void RtGraphics_Renderer_Draw(void* self, void* context, float alpha,
                                         const void* scene, const void* camera) {
    Attend("Renderer::Draw", g_drawCalls);
    reinterpret_cast<Graphics::Renderer*>(self)                          // B.3.5 bridge
        ->Graphics::Renderer::Draw(
            *reinterpret_cast<Video::Context*>(context),                 // B.3.5 bridge
            alpha,
            *reinterpret_cast<const Graphics::Scene*>(scene),            // B.3.5 bridge
            *reinterpret_cast<const Graphics::Camera*>(camera));         // B.3.5 bridge
}

// ============================================================================
// §4.  ?Update@AnimationState@Graphics@@QEAA_N...
//      retail 0x1800fdb20, ordinal 4474. 1062 bytes / five chained .pdata
//      fragments. NINE arguments.
// ============================================================================
// THE ARGUMENT MAPPING WAS SETTLED THREE WAYS, not read off the prologue:
//
//   (1) the EXPORTED MANGLED NAME spells all eight parameters and their types:
//       (const Time&, const Array<GraphicsMesh::Node>&, const Transform&,
//        InterpolatedState<Transform>*, InterpolatedState<Transform>*,
//        Vector3&, bool, bool).
//   (2) the CALLER, `?Update@AnimationManager@Graphics@@` — already ported in
//       wave23_lane_gfxbody.cpp — passes them in exactly that order, and its
//       lane read the call site from the disassembly.
//   (3) the HOME-SLOT ARITHMETIC of this body, computed and checked: four pushes
//       (0x20) plus `SUB RSP,0x98` puts the caller's frame base at RSP+0xB8, so
//       [RSP+0xD8]=arg4(R9 home), [RSP+0xE0]=arg5, [RSP+0xE8]=arg6,
//       [RSP+0xF0]=arg7, [RSP+0xF8]=arg8, [RSP+0x100]=arg9. Every one of those
//       six is loaded at exactly the offset that arithmetic predicts, and
//       [RSP+0xF0] is the pointer whose three floats are zeroed in the prologue
//       — which is what the mangled name calls `AEAVVector3@8@`.
//
// Three independent derivations agreeing is worth stating because ONE of them
// would not have been: an nine-argument prototype read off the prologue alone is
// exactly where Ghidra drops arguments.
//
// NO HIDDEN RETURN SLOT. The return is `_N` (bool, in AL: `MOV AL,0x1` at
// 0x1800fdf32 and `XOR AL,AL` at 0x1800fdb66), so the four register arguments
// are not shifted (reference_hidden_retslot_wild_write.md — the trap checked for
// and found absent here).
//
// TIME IS TICKS, AND THE SCALE IS A RUNTIME GLOBAL, NOT A CONSTANT.
// `MULSD XMM0,[0x1801fe728]` looks like a literal. It is not: 0x1801fe728 sits
// at offset 0x8728 in `.data`, whose SizeOfRawData is 0x5400 — i.e. in the
// uninitialised tail. It is `System::Timer::_secondsPerTick`, already resolved
// in the tree at engine/funcs/engine_funcs.cpp:1546, and it is written at engine
// startup. A port that baked in a number read from the file would have baked in
// zero. Found by trying to read the bytes and getting none.
//
// THE dt==0 ARM CONTAINS A NO-OP AND IT IS TRANSCRIBED AS ONE. At
// 0x1800fdc50..0x1800fdc7c retail loads seven dwords from `[RBX-0x10 .. RBX+8]`
// and stores them straight back to the SAME seven addresses, then does the real
// work. It is MSVC's rendering of `a = a` on an inlined Transform assignment. It
// has no observable effect, so this port omits the stores and says so here
// rather than writing seven self-assignments the compiler would delete anyway.
// ----------------------------------------------------------------------------
namespace {

using CopyInstancedFn = void (*)(AnimationState*, const void* nodes, const void* base,
                                 void* interp, int32_t);
using UpdateRootFn    = void (*)(AnimationState*, float dt, void* outVec3);
using UpdateNodesFn   = void (*)(AnimationState*, float dt, const void* nodes,
                                 const void* base, void* interpA, void* interpB,
                                 void* outVec3);
using ApplyEventsFn   = void (*)(const void* clip, const void* base,
                                 const int64_t* fromTime, const int64_t* toTime);

// `System::Timer::_secondsPerTick`, retail 0x1801fe728 — a DOUBLE in .data's
// uninitialised tail, written at engine startup. ENDGAME B.3.9 close (D7): the
// class static, read fresh -- the cached by-address door is gone.
inline double SecondsPerTick() {
    return ::System::Timer::_secondsPerTick;
}

// `Graphics::AnimationManager::_singleton`, retail 0x1801fe7b0 — the same global
// animation.cpp:179 already resolves. Read here only to reach the 0x30-stride
// group record a dying layer's handle names.
// ENDGAME B.3.9 GFX: the class static, not the lazy by-address cache (D7 A44).
inline uint8_t* AnimManagerGroups() {
    uint8_t* const mgr = reinterpret_cast<uint8_t*>(Graphics::AnimationManager::_singleton);
    if (!mgr) return nullptr;
    return *reinterpret_cast<uint8_t**>(mgr);      // _singleton->_groupData
}

}  // namespace

// ENDGAME B.3.5 block GC: THE BODY MOVED to
// `Graphics::AnimationState::Update`, in this file's trailing `namespace Graphics`
// block. It cannot live here: everything above is inside `namespace rtgfxbig` and
// `Graphics` is its SIBLING, not its child (B34_BRIEFS A.2). Block H1 left the
// body here because this file is not block H1's; the door keeps its name, its
// registration and its two counters.
extern "C" bool RtGraphics_AnimationState_Update(
        void* self, const void* time, const void* nodes, const void* baseTransform,
        void* interpA, void* interpB, void* outVec3, bool rootOnly, bool applyEvents) {
    Attend("AnimationState::Update", g_animStateCalls);
    return reinterpret_cast<Graphics::AnimationState*>(self)            // B.3.5 bridge
        ->Graphics::AnimationState::Update(
            *reinterpret_cast<const System::Time*>(time),
            *reinterpret_cast<const Collection::Array<uint8_t>*>(nodes),
            *reinterpret_cast<const Math::Transform*>(baseTransform),
            interpA, interpB,
            *reinterpret_cast<Math::Vector3*>(outVec3),                 // B.3.5 bridge
            rootOnly, applyEvents);
}

// ============================================================================
// REGISTRATION
//
// NAMES FOR THE THREE EXPORTS, one ADDRESS for the one unexported body this lane
// ported. Every mangled name below is its own STRING LITERAL, never a named
// constant: tools/runtime_census.py matches `RegisterRuntimeExport("<literal>"`
// and cannot see through a constant
// (reference_census_cannot_see_nonliteral_registrations.md).
//
// THE ADDRESS REGISTRATION WAS CHECKED FOR HARM BEFORE IT WAS WRITTEN, because
// in this same namespace a RegisterRuntimeAddr is ACTIVE HARM on at least one
// row: standalone/scene_watch.cpp compares `?AddDrawItems@MeshInstance@`'s retail
// address against a live vtable slot as an IDENTITY TOKEN, so registering it
// would break the comparison. 0x180060d30 occurs NOWHERE in code/, engine/,
// runtime/ or standalone/ — grepped, empty — so it is neither an identity token
// nor a shadowed address, and it is a real CALL door this lane's own
// BuildParameters needs at cutover
// (reference_count_the_doors_not_the_calls.md).
//
// THE DOORS THESE THREE ACTUALLY HAVE, so nobody re-derives them:
//   BuildParameters  <- engine/funcs/engine_funcs.cpp:3492 (ResolveExport name),
//                       standalone/reg_unreg_lane3.cpp:154 (an exe dispatch slot
//                       filled by ResolveExport), runtime/src/save/
//                       saveload_draw.cpp:596 and runtime/src/graphics/
//                       wave24_lane_gfxmat.cpp:656 (both RT_EXPORT_ONCE).
//                       FOUR name doors — this row goes live immediately.
//   Draw             <- code/Game/Game/gameupdateloop.cpp:628 and
//                       engine/funcs/engine_funcs.cpp:3615, both ResolveExport
//                       by name. THE EVERY-FRAME 3D RENDER CALL.
//   AnimationState::Update
//                    <- runtime/src/graphics/wave23_lane_gfxbody.cpp:555, an
//                       RT_EXPORT_ONCE inside the ported
//                       AnimationManager::Update. That is a name door and this
//                       registration reaches it, so this row goes live on every
//                       sim iteration that has an animated mesh registered.
//
// Because Draw and AnimationState::Update both go live the moment they are
// registered, they share a kill switch (RT_GFXBIG) with
// BuildParameters and each is separately attributable in the log through its
// own attendance counter.
// ============================================================================
int RtGraphicsWave27GfxbigRegister() {
    int count = 0;

    RegisterRuntimeExport("?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ",
                          (void*)&RtGraphics_MaterialInstance_BuildParameters); ++count;
    RegisterRuntimeExport("?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@"
                          "AEBVCamera@2@@Z",
                          (void*)&RtGraphics_Renderer_Draw); ++count;
    RegisterRuntimeExport("?Update@AnimationState@Graphics@@QEAA_NAEBVTime@System@@AEBV?$Array@"
                          "UNode@GraphicsMesh@Graphics@@@Collection@@AEBVTransform@Math@@PEAV?$"
                          "InterpolatedState@VTransform@Math@@@2@PEAV78@AEAVVector3@8@_N6@Z",
                          (void*)&RtGraphics_AnimationState_Update); ++count;

    RegisterRuntimeAddr(0x180060d30,
                        (void*)&RtCollection_Array_MaterialParameterSet_SetSize); ++count;

    // BATCH 8b: the draw-item sort and its selection-sort callee — see the
    // banner above RtGraphics_Renderer_DrawItemSort.
    RegisterRuntimeAddr(0x180062d60, (void*)&RtGraphics_Renderer_DrawItemSort); ++count;
    RegisterRuntimeAddr(0x180063b90, (void*)&RtGraphics_Renderer_DrawItemSortSmall); ++count;

    return count;   // 6
}

// One line beside the other wave summaries, so a run that entered none of these
// says so rather than staying silent. The BRANCH counters sit next to the door
// counters on purpose: `Draw calls=N sorts=0` is a completely different finding
// from `Draw calls=0`.
void RtGraphicsWave27GfxbigSummary() {
    printf("[GFXBIG27] BuildParameters=%llu (bindings=%llu) Draw=%llu "
           "(entries=%llu sorts=%llu) AnimStateUpdate=%llu (layers=%llu expired=%llu)\n",
           g_buildParamCalls, g_buildParamBinds, g_drawCalls, g_drawGroups, g_drawSorts,
           g_animStateCalls, g_animStateLayers, g_animStateExpired);
}

}  // namespace rtgfxbig

extern "C" int RtGraphicsWave27GfxbigRegisterC() { return rtgfxbig::RtGraphicsWave27GfxbigRegister(); }
void RtGraphicsWave27GfxbigSummary() { rtgfxbig::RtGraphicsWave27GfxbigSummary(); }
int  RtGraphicsWave27GfxbigRegister() { return rtgfxbig::RtGraphicsWave27GfxbigRegister(); }

// ############################################################################
// ENDGAME B.3.5 step 5, block GC -- the member bodies whose live doors are the
// `extern "C"` functions above.
//
// Their own `namespace Graphics { }` block, because a member definition has to
// appear in a namespace ENCLOSING the class and `Graphics` / `RtGraphics` are
// siblings (B34_BRIEFS A.2). Every file-local helper here lives in
// `rtgfxbig`'s own unnamed namespace, NOT the global one, so unqualified lookup
// does not reach it from `namespace Graphics` -- each is spelled `rtgfxbig::`,
// the trap wave28_lane_gfxbody.cpp:957 records.
//
// The transcription banners stay beside the doors above; only the spelling of
// the receiver changed.
// ############################################################################

namespace Graphics {

// ?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ   retail 0x1801033e0
void MaterialInstance::BuildParameters() {
    // MOV RDX,[RCX+0x28] / TEST RDX,RDX / JZ epilogue. The early return skips
    // the whole function INCLUDING the +0x24/+0x25 accumulation block.
    Graphics::Material* const material = _material.Get();
    if (material == nullptr) return;

    _shaderClass = static_cast<uint32_t>(material->_type);      // material+0x20

    // Array<RtGraphics::MaterialParameterSet>::SetSize(material->passCount). 0x180060d30.
    // `material + 0x30` is `Material::_passes._size` — the pass COUNT, which is
    // why `rtgfxbig::kPassDesc` and the pass object's own +0x30 share a number.
    rtgfxbig::RtCollection_Array_MaterialParameterSet_SetSize(
        &_parameters, material->_passes.GetSize());

    // CMP [R15+0x38],R8D / JLE — the bound is `_parameters._size`, and it is
    // RE-READ at the bottom of the loop (0x1801038fe), not hoisted.
    for (int32_t pass = 0; pass < _parameters.GetSize(); ++pass) {
        uint8_t* const desc0 = *reinterpret_cast<uint8_t**>(rtgfxbig::PassObject(this, pass) + rtgfxbig::kPassDesc);

        // ---- textures -----------------------------------------------------
        // `desc ? desc->count : 0` — retail's own null arm (XOR EAX,EAX at
        // 0x180103475), computed ONCE per pass and then used as a DOWN counter.
        const int32_t texCount =
            desc0 ? *reinterpret_cast<const int32_t*>(desc0 + rtgfxbig::kTexDescCount) : 0;
        for (int32_t i = 0; i < texCount; ++i) {
            uint8_t* const desc =
                *reinterpret_cast<uint8_t**>(rtgfxbig::PassObject(this, pass) + rtgfxbig::kPassDesc);
            uint8_t* const recs = *reinterpret_cast<uint8_t**>(desc + rtgfxbig::kTexDescData);
            if (recs[static_cast<int64_t>(i) * 0x10 + 0xc] != 0) continue;   // skip flag

            RtSystem::String name;
            name.data = nullptr;
            // B.3.5 block GC item 6: two `RT_EXPORT_ONCE` edges onto classes that
            // are now real become direct QUALIFIED member calls. `GetTextureName`
            // keeps the out-parameter spelled, which is the member's own signature
            // and retail's argument order (block G's banner in
            // wave28_lane_gfxbody.cpp says why the member does not return by value).
            material->Graphics::Material::GetTextureName(&name, pass, i);

            RtGraphics::MaterialBinding v;
            v.sourceIndex = this->Graphics::MaterialInstance::FindTexture(name);

            // The release is retail's inlined RtSystem::String::Release, byte for byte:
            // decrement the refcount at data-4, and on zero rewind to data-8 and
            // free. RtString_ReleaseInline (runtime/src/system/string.cpp:132) IS
            // that sequence, including the asymmetry where the freeing branch
            // leaves `data - 8` in the slot and the sharing branch nulls it.
            //
            // ONE INSTRUCTION DIFFERS AND IT CANNOT FIRE: retail tests
            // `data - 8 == 0` before freeing (the ZF of `SUB RDI,0x8` at
            // 0x1801034f5), which is true only if `data == 8`. The shared body
            // instead relies on the null test inside 0x1800c7ad0. Recorded rather
            // than quietly treated as identical.
            RtSystem::RtString_ReleaseInline(&name);

            // The shader slot is read AFTER the call, through a FRESHLY RELOADED
            // chain (0x180103546..0x18010355f). Retail reloads because
            // FindTexture takes a non-const `this` and could in principle move
            // the material's arrays; transcribed unhoisted for that reason.
            uint8_t* const recs2 = *reinterpret_cast<uint8_t**>(
                *reinterpret_cast<uint8_t**>(rtgfxbig::PassObject(this, pass) + rtgfxbig::kPassDesc) + rtgfxbig::kTexDescData);
            v.shaderSlot =
                *reinterpret_cast<const int32_t*>(recs2 + static_cast<int64_t>(i) * 0x10 + 8);

            // `_parameters._data` is RE-READ here (MOV RSI,[RBP] at 0x180103563)
            // rather than kept from the top of the pass loop.
            RtGraphics::MaterialParameterSet* const set = &_parameters[pass];
            // retail 0x1801035c4, kSpec_Binding8 — trait-equivalent:
            // MaterialBinding (Material.h) is a plain POD, so
            // SpecFor<MaterialBinding>() is the same {8-byte copy, no init,
            // no destroy, align 4} shape.
            set->textures.SetSize(set->textures.GetSize() + 1, v);
            ++rtgfxbig::g_buildParamBinds;
        }

        // ---- constants ----------------------------------------------------
        uint8_t* const desc1 = *reinterpret_cast<uint8_t**>(rtgfxbig::PassObject(this, pass) + rtgfxbig::kPassDesc);
        const int32_t conCount =
            desc1 ? *reinterpret_cast<const int32_t*>(desc1 + rtgfxbig::kConDescCount) : 0;
        for (int32_t i = 0; i < conCount; ++i) {
            // NO SKIP FLAG on this arm — see the kConDescData note above.
            RtSystem::String name;
            name.data = nullptr;
            // block GC item 6, as in the textures arm above.
            material->Graphics::Material::GetConstantName(&name, pass, i);

            RtGraphics::MaterialBinding v;
            v.sourceIndex = this->Graphics::MaterialInstance::FindConstant(name);
            RtSystem::RtString_ReleaseInline(&name);

            uint8_t* const recs2 = *reinterpret_cast<uint8_t**>(
                *reinterpret_cast<uint8_t**>(rtgfxbig::PassObject(this, pass) + rtgfxbig::kPassDesc) + rtgfxbig::kConDescData);
            v.shaderSlot =
                *reinterpret_cast<const int32_t*>(recs2 + static_cast<int64_t>(i) * 0x10 + 8);

            RtGraphics::MaterialParameterSet* const set = &_parameters[pass];
            // retail 0x1801037ec, kSpec_Binding8 — trait-equivalent, same as
            // the textures arm above.
            set->constants.SetSize(set->constants.GetSize() + 1, v);
            ++rtgfxbig::g_buildParamBinds;
        }
    }

    // ---- the render-texture latches ---------------------------------------
    // See the banner: +0x24 is cleared and recomputed, +0x25 is only ever OR'd
    // into. Both are stored back INSIDE the loop on every iteration, and the
    // texture's `image` pointer is RE-READ between the two tests (`MOV RAX,[RCX]`
    // at both 0x180103930 and 0x180103940). All three quirks transcribed.
    _hasRenderTexture = 0;
    const int32_t texN = _textures._size;
    if (texN <= 0) return;

    uint8_t  acc24 = 0;
    uint8_t  acc25 = _hasRenderedTexture;     // NOT reset — retail's asymmetry
    uint8_t* p     = reinterpret_cast<uint8_t*>(_textures._data) + 8;
    for (int32_t k = 0; k < texN; ++k) {
        const uint8_t* img = *reinterpret_cast<uint8_t**>(p);
        acc24 = static_cast<uint8_t>(
            acc24 | (*reinterpret_cast<const int32_t*>(img + 0x2c) == 4 ? 1 : 0));
        _hasRenderTexture = acc24;

        img = *reinterpret_cast<uint8_t**>(p);                 // re-read
        acc25 = static_cast<uint8_t>(
            acc25 | (*reinterpret_cast<const int32_t*>(img + 0x2c) == 5 ? 1 : 0));
        _hasRenderedTexture = acc25;

        p += 0x10;
    }
}

// ---------------------------------------------------------------------------
// ?Update@AnimationState@Graphics@@QEAA_N...   retail 0x1800fdb20
// ENDGAME B.3.5 block GC. The argument mapping (settled three independent ways),
// the tick/second conversions, the dt==0 snapshot and the layer-removal arm are
// all documented in the banner beside the door above.
//
// THE THREE `RT_EXPORT_ONCE` EDGES ONTO THIS CLASS'S OWN PRIVATE MEMBERS ARE
// GONE (block GC item 6). `CopyInstancedToUnique`, `UpdateRootOnly` and
// `UpdateNodes` are real members of `Graphics::AnimationState` now — the first
// two in `animationstate_transforms.cpp`, the third in
// `wave28_lane_gfxbody.cpp` — so retail's three direct CALLs are three
// QUALIFIED non-virtual member calls instead of three resolver hits. Their
// registered doors are untouched and still serve every caller outside this file.
//
// `?ApplyEvents@Animation@Graphics@@` IS CONVERTED (ENDGAME B.3.5 closing pass).
// The note that used to stand here said the receiver was "NOT a
// `Graphics::Animation` object -- it is an element of the group's clip array", as
// though those were alternatives. They are the same thing: see the proof at the
// call site below.
// ---------------------------------------------------------------------------
bool AnimationState::Update(const System::Time& timeRef,
                            const Collection::Array<uint8_t>& nodesRef,
                            const Math::Transform& root, void* interpA, void* interpB,
                            Math::Vector3& rootOffsetOut, bool rootOnly, bool applyEvents) {
    RtGraphics::AnimationState* const s =
        reinterpret_cast<RtGraphics::AnimationState*>(this);   // B.3.5 bridge
    const void* const nodes = &nodesRef;
    const void* const baseTransform = &root;
    void* const outVec3 = &rootOffsetOut;
    const int64_t deltaTicks = timeRef._ticks;

    // MOVSS [RBX],XMM7 x3 — the out Vector3 is zeroed before the early return,
    // so a caller that ignores the `false` still sees a defined value.
    float* const out = static_cast<float*>(outVec3);
    out[0] = 0.0f; out[1] = 0.0f; out[2] = 0.0f;

    if (s->layerCount == 0) return false;          // CMP [RCX+0x10],0 / XOR AL,AL

    s->layers[0].restarted = 1;                    // MOV BYTE PTR [RAX+0x30],1

    const int32_t nodeCount = *reinterpret_cast<const int32_t*>(
        static_cast<const uint8_t*>(nodes) + 8);   // MOVSXD R14,[R8+8]

    // seconds = ticks * _secondsPerTick, computed in DOUBLE and then narrowed to
    // float by CVTPD2PS — NOT computed in float. The association order is
    // retail's and is preserved exactly, because a moved term in a float sum is
    // algebraically identical and numerically different
    // (reference_moved_term_hides_where_terms_cancel.md).
    const float dt = static_cast<float>(
        static_cast<double>(deltaTicks) * rtgfxbig::SecondsPerTick());

    const int32_t layerCount = s->layerCount;
    RtGraphics::AnimationLayer* const lastLayer = s->layers + (layerCount - 1);

    if (layerCount == 1 && lastLayer->handle >= 0) {
        // CMP ECX,0x1 / JNE, then CMP [RDX+0xc],0 / JL, then MOV [RSI],CL —
        // and CL is the LAYER COUNT, which on this branch is exactly 1.
        s->needsUniqueCopy = 1;
    } else {
        if (s->needsUniqueCopy != 0) {
            this->Graphics::AnimationState::CopyInstancedToUnique(
                nodesRef, root, interpA, lastLayer->handle);
            s->needsUniqueCopy = 0;
        }
        if (rootOnly) {
            this->Graphics::AnimationState::UpdateRootOnly(dt, rootOffsetOut);
        } else {
            this->Graphics::AnimationState::UpdateNodes(
                dt, nodesRef, root, interpA, interpB, rootOffsetOut);
        }
    }

    // A zero frame delta SNAPSHOTS every node's interpolated pair, so an
    // interpolation at any alpha yields the same pose. Copy direction is
    // +0x00 -> +0x1c; see AnimationState.h on why the members are named by
    // position rather than by role.
    if (deltaTicks == 0 && nodeCount > 0) {
        RtGraphics::InterpolatedTransform* const it = static_cast<RtGraphics::InterpolatedTransform*>(interpA);
        for (int32_t i = 0; i < nodeCount; ++i) {
            const uint32_t* const src =
                reinterpret_cast<const uint32_t*>(&it[i].first[0]);
            uint32_t* const dst = reinterpret_cast<uint32_t*>(&it[i].second[0]);
            // INTEGER moves of float storage: retail copies dwords (MOV EAX,...),
            // which is bit-exact where a float assignment is only value-exact.
            // The two differ on a signalling NaN and on negative zero
            // (wave23_lane_gfxbody.cpp §5.3 gives the same reason).
            for (int k = 0; k < 7; ++k) dst[k] = src[k];
        }
    }

    // ---------------- the layer loop --------------------------------------
    // The bound is RE-READ at the bottom (`CMP R12D,[RSI+0x10]` at 0x1800fdefd)
    // because the removal arm DECREMENTS it mid-loop.
    for (int32_t li = 0; li < s->layerCount; ++li) {
        // `layers` is reloaded at the TOP of every iteration (MOV R15,[RSI+8]).
        RtGraphics::AnimationLayer* const L = s->layers + li;
        ++rtgfxbig::g_animStateLayers;

        const uint8_t* const group = static_cast<const uint8_t*>(L->group);
        const RtGraphics::AnimationClipPrefix* const clip =
            reinterpret_cast<const RtGraphics::AnimationClipPrefix*>(
                *reinterpret_cast<const uint8_t* const*>(group + 0x20) +
                static_cast<int64_t>(L->clipIndex) * 0x70);

        const int64_t duration = clip->duration;

        // At speed exactly 1.0 the frame delta is used VERBATIM in ticks — no
        // round trip through seconds. UCOMISS + JP + JNE, so a NaN speed takes
        // the scaled arm (the JP catches unordered). Preserved: a `== 1.0f`
        // comparison in C++ has the same NaN behaviour, but the ORDER of the two
        // branches is retail's.
        int64_t advance;
        if (L->speed == 1.0f) {
            advance = deltaTicks;
        } else {
            // ticks = (int64)((double)(speed * dtSeconds) / _secondsPerTick).
            // MULSS in FLOAT first, then widened by CVTPS2PD, then DIVSD, then
            // CVTTSD2SI (truncation toward zero). Every step in that order.
            const float scaled = L->speed * dt;
            advance = static_cast<int64_t>(static_cast<double>(scaled) / rtgfxbig::SecondsPerTick());
        }

        int64_t newTime = advance + L->time;

        if (applyEvents && li == 0 && clip->eventCount > 0) {
            // ApplyEvents(base, fromTime, toTime) — fromTime is the layer's
            // CURRENT time and toTime a local holding the new one, so retail hands
            // it a reference INTO the layer and a reference to its own stack. The
            // order matters: the store of `newTime` into the layer happens AFTER
            // this call (0x1800fdd80), so the callee sees the OLD value through its
            // third argument -- and it still does here, because `L->time` is bound
            // as a reference at the call and `L->time = newTime` is the next
            // statement.
// THE CLIP RECORD *IS* A `Graphics::Animation`, and the note that used to say
// otherwise is withdrawn. The proof is retail's own mangled RETURN TYPE:
//   ?GetAnimation@AnimationGroup@Graphics@@QEBAAEBVAnimation@2@H@Z  0x180016660
// is the element helper over `AnimationGroup::_clips` and its body is an
// `imul $0x70` -- so retail names the element type `Graphics::Animation` and fixes
// the stride at 0x70 in the same symbol. `AnimationGroup.h:185/195` asserts the
// array at +0x20, `Animation.h:453` asserts `sizeof(Graphics::Animation) == 0x70`
// (block H1 measured it three ways), and this lane's own `LayerClip()` computes
// exactly `*(uint8_t**)(group + 0x20) + clipIndex * 0x70` -- i.e. `&_clips[i]` of
// that same array. Receiver settled; the `AnimationClipPrefix` POD stays, because
// the hand-inlined FIELD reads around the call are not doors and stay
// byte-identical.
            reinterpret_cast<const Graphics::Animation*>(clip)   // B.3.5 bridge
                ->Graphics::Animation::ApplyEvents(
                    *reinterpret_cast<const Math::Transform*>(baseTransform),
                    *reinterpret_cast<const System::Time*>(&L->time),
                    *reinterpret_cast<const System::Time*>(&newTime));
        }

        L->time = newTime;

        if (clip->looping != 0) {
            if (newTime >= duration) {
                // Retail's do/while subtract, not a modulo: `SUB RBX,RDI` in a
                // loop while `RBX >= RDI` (0x1800fdd90). Identical result for
                // positive duration and NON-TERMINATING for duration == 0 in
                // both retail and here — the port does not "fix" that, because a
                // zero-duration looping clip is a data defect retail would also
                // hang on and a guard would hide it.
                do { newTime -= duration; } while (newTime >= duration);
                L->time = newTime;

                // The restart offset. loopFrame == 0 short-circuits the entire
                // fraction to zero (TEST EAX,EAX / JNZ at 0x1800fdd9f), which is
                // NOT the same as computing it and getting zero: with
                // frameLast == frameFirst the division would be 0/0.
                float offsetSeconds;
                if (clip->loopFrame == 0) {
                    offsetSeconds = 0.0f;
                } else {
                    const float num = static_cast<float>(clip->loopFrame - clip->frameFirst);
                    const float den = static_cast<float>(clip->frameLast  - clip->frameFirst);
                    // `rate` was this lane's name for +0x50 and it is WRONG:
                    // +0x50 is the clip's FRAME COUNT (corrected 2026-09-12 in
                    // AnimationState.h from ?GetPoseAtTime@Animation@Graphics@@,
                    // which bounds a frame index with it). The expression is
                    // unchanged -- fraction * frameCount is a frame index, and
                    // +0x48 converts that to seconds.
                    const float rate = static_cast<float>(clip->frameCount);
                    // (((num / den) * rate) * loopScale) — THREE separate
                    // instructions in this exact order (DIVSS, MULSS, MULSS at
                    // 0x1800fddc7..0x1800fddcf). Re-associating would be
                    // algebraically identical and numerically different.
                    offsetSeconds = ((num / den) * rate) * clip->loopScale;
                }
                const int64_t offsetTicks = static_cast<int64_t>(
                    static_cast<double>(offsetSeconds) / rtgfxbig::SecondsPerTick());
                L->time = offsetTicks + newTime;
            }
        } else if (newTime >= duration) {
            L->time = duration;                    // clamp, 0x1800fddf7
        }

        // ---- the fade countdown and the layer removal --------------------
        if (L->fadeLength <= 0) continue;

        L->fadeRemaining -= static_cast<int64_t>(
            static_cast<double>(dt) / rtgfxbig::SecondsPerTick());
        if (L->fadeRemaining > 0) continue;

        ++rtgfxbig::g_animStateExpired;

        // Release the manager-side group this layer's handle names. The handle
        // is RE-READ from the array (`MOV RAX,[RSI+8]` then
        // `MOV ECX,[R14+RAX+0xc]` at 0x1800fde2c) rather than reused from L.
        const int32_t handle = s->layers[li].handle;
        if (handle >= 0) {
            const uint32_t gi = static_cast<uint32_t>(handle >> 8) & 0xFFu;  // SAR then MOVZX CL
            uint8_t* const groups = rtgfxbig::AnimManagerGroups();
            if (groups != nullptr) {
                uint8_t* const g = groups + static_cast<int64_t>(gi) * 0x30;
                if (--*reinterpret_cast<int32_t*>(g + 0x0c) == 0) {
                    *reinterpret_cast<int64_t*>(g)        = 0;
                    *reinterpret_cast<int32_t*>(g + 0x08) = -1;
                    // -----------------------------------------------------
                    // ADDRESS DOORS 2 and 3 of 3.
                    //   SYMBOL:      0x18005aa30 (403 bytes) and 0x18005b1e0
                    //                (382 bytes) — `Collection::Array<T>::SetSize`
                    //                for the group's node array (+0x10) and its
                    //                AnimationState array (+0x20), both called
                    //                with newSize 0. NEITHER is in the export
                    //                directory; checked with pefile rather than
                    //                inferred from a comment
                    //                (reference_aeaa_is_private_not_unexported.md).
                    //   CONDITION:   reached ONLY when a fading layer's countdown
                    //                expires AND its manager group's refcount at
                    //                +0x0c falls to zero — a rare arm, which is
                    //                why `rtgfxbig::g_animStateExpired` counts it
                    //                separately from the door.
                    //   CONSEQUENCE: at cutover, with the DLL absent, these
                    //                resolve to nothing and the group's two
                    //                buffers LEAK (they are not freed and the
                    //                headers keep stale pointers). Not a crash,
                    //                and not visible in a short run. ADDRESSEE:
                    //                the Wave 28 Graphics lane; residuals R2/R3
                    //                in WAVE27_LANE_GFXBIG.md §6. Both are plain
                    //                SetSizeCore instantiations with a
                    //                non-trivial element, so the cheap route is
                    //                two more ArraySpec rows, not two bodies.
                    // -----------------------------------------------------
                    // B3R: the two instantiations by body name.
                    rtcoll::Rt_SetSize_18005aa30(g + 0x10, 0);
                    rtcoll::Rt_SetSize_18005b1e0(g + 0x20, 0);
                }
            }
        }

        // Erase layer `li` by shifting the tail down one slot. Retail copies
        // SEVEN QWORDS per element (0x1800fde9c..0x1800fded6), i.e. the whole
        // 0x38 stride, and RE-READS `_layers` and `layerCount` inside the shift
        // loop. Both transcribed.
        for (int32_t k = li; k + 1 < s->layerCount; ++k) {
            uint8_t* const dst = reinterpret_cast<uint8_t*>(s->layers) +
                                 static_cast<int64_t>(k) * 0x38;
            if (dst == nullptr) break;             // retail tests the COMPUTED address
            const uint8_t* const src = dst + 0x38;
            for (int q = 0; q < 7; ++q)
                reinterpret_cast<uint64_t*>(dst)[q] =
                    reinterpret_cast<const uint64_t*>(src)[q];
        }
        --s->layerCount;
        --li;                                      // DEC R12D then the INC at the top
    }

    return true;                                   // MOV AL,0x1
}

// ---------------------------------------------------------------------------
// ?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@AEBVCamera@2@@Z
// retail 0x180104bb0 -- ENDGAME B.3.5 block GC. THE WHOLE FRAME. The two passes,
// the four kinds of virtual call, the field_3c-read-uninitialised-on-frame-one
// note and the clear colour/flag immediates are in the banner beside the door.//
// THE POD SPELLING OF THE *ELEMENT* RECORDS IS KEPT, DELIBERATELY. `this` is the
// real `Graphics::Renderer`; the draw-group / scene-entry / draw-item records it
// indexes are still spelled `RtGraphics::*` through one bridge local, because
// those PODs have live users in files outside this block and B4b-1 keeps a POD
// until its LAST user converts. Camera.h cross-asserts every one of their offsets
// against the real classes', so the two descriptions cannot drift, and renaming
// ~200 field accesses in a body this size is a second change, not this one.
// ---------------------------------------------------------------------------
void Renderer::Draw(Video::Context& context, float alpha, const Scene& sceneRef,
                    const Camera& cameraRef) {
    RtGraphics::Renderer* const r =
        reinterpret_cast<RtGraphics::Renderer*>(this);          // B.3.5 bridge
    RtVideo::Context* const ctx =
        reinterpret_cast<RtVideo::Context*>(&context);          // B.3.5 bridge
    const void* const scene  = &sceneRef;
    // Reset FIRST, then the two scalars, then AddScene.
    //
    // BLOCK GC ITEM 6: this was `RT_EXPORT_ONCE("?Reset@Renderer@Graphics@@…")`
    // reaching our own body through the resolver. `Reset` is a member of the real
    // `Graphics::Renderer` (block F), so it is a QUALIFIED non-virtual member call
    // -- retail's own direct CALL to 0x180105110. Same for `AddScene` and
    // `BeginDrawGroup` below; all three doors stay registered for outside callers.
    this->Graphics::Renderer::Reset();

    r->frameAlpha    = alpha;                 // MOVSS [RDI+0x40],XMM6
    r->contextSerial = ctx->_frameIndex;      // MOV EAX,[RSI+8] / MOV [RDI+0x44],EAX

    // field_3c is read here UNINITIALISED on the first frame. See the banner.
    this->Graphics::Renderer::AddScene(
        &sceneRef,
        reinterpret_cast<const Video::RenderBuffer*>(ctx->_defaultRenderBuffer),  // B.3.5 bridge
        cameraRef, _shaderDetail, 0x40, 0, 0x8007u);

    // ---------------- PASS 1: cull and collect ----------------------------
    r->currentScene = 0;
    if (r->scenes._size > 0) {
        do {
            RtGraphics::RendererSceneEntry* const e = r->scenes._data + r->currentScene;

            r->field_3c  = e->field_e8;
            r->field_48  = e->field_f8;

            // Retail passes the entry's ONE camera as BOTH the draw camera and
            // the cull camera (`MOV R9,R8` at 0x180104c5f); transcribed.
            const int32_t nextGroup = this->Graphics::Renderer::BeginDrawGroup(
                reinterpret_cast<const Video::RenderBuffer*>(e->renderBuffer),  // B.3.5 bridge
                *reinterpret_cast<const Camera*>(&e->camera),                   // B.3.5 bridge
                *reinterpret_cast<const Camera*>(&e->camera),                   // B.3.5 bridge
                e->field_ec, e->field_f0);

            // The CURRENT group index is written into the entry, and it is the
            // value from BEFORE this BeginDrawGroup (0x180104c78 reads
            // [RDI+0x34], which is only updated at the bottom of the loop).
            e->drawGroupIndex = r->currentDrawGroup;

            const uint32_t passMask =
                r->drawGroups._data[r->currentDrawGroup].passMask;

            // The scene's drawable list: head at Scene+0x60, `next` at +0x10,
            // payload at +0x00. Both the head and every `next` are read as raw
            // offsets because Graphics::Scene's layout is lane D's, not this
            // lane's, and this body reads exactly three of its fields.
            void* node = *reinterpret_cast<void* const*>(
                static_cast<const uint8_t*>(scene) + 0x60);
            while (node != nullptr) {
                void* const obj = *reinterpret_cast<void**>(node);
                const uint8_t* const o = static_cast<const uint8_t*>(obj);
                if ((*reinterpret_cast<const uint32_t*>(o + 0x30) & passMask) != 0 &&
                    *reinterpret_cast<const int32_t*>(o + 0x28) <= r->field_48) {
                    // Slot 3 = ?AddDrawItems@…@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z.
                    // A VIRTUAL DISPATCH through the object's own vtable, NOT a
                    // door: lane GFXMESH owns ??_7MeshInstance@ and this call
                    // reaches whatever is in the live table. Nothing here
                    // registers, stamps or reads that slot as an identity token.
                    // By name since B3X W0.
                    static_cast<const Geometry*>(obj)->AddDrawItems(sceneRef, *this);  // B.3.x bridge
                }
                node = *reinterpret_cast<void**>(static_cast<uint8_t*>(node) + 0x10);
            }

            if (r->field_48 != 0) {
                // ENDGAME B.3.5 caller batch: `Graphics::Scene::AddCulledDrawItems`
                // is a DEFINED member (cull.cpp:363) and the receiver is this
                // body's own `const Scene&` parameter, so this is the qualified
                // non-virtual member call retail makes. `AddCulledFn` went with it.
                sceneRef.Graphics::Scene::AddCulledDrawItems(*this);
            }

            for (int32_t k = 0; k < 3; ++k) {
                Collection::Array<RtGraphics::RendererDrawItem>& list =
                    r->drawGroups._data[r->currentDrawGroup].items[k];
                // TWO tests, `> 0` then `> 1` (0x180104cfa, 0x180104cfe). Only
                // the second can decide anything; the first is retail's and is
                // kept because a zero-size list with a non-null _data would take
                // a different path if it were removed.
                if (list._size > 0 && list._size > 1) {
                    // ---------------------------------------------------------
                    // ADDRESS DOOR 1 of 3 — PORTED (BATCH 8b, faithfully):
                    // RtGraphics_Renderer_DrawItemSort, above, registered at
                    // the bottom of this file together with its 0x180063b90
                    // selection-sort callee; called by name (B3R).
                    // ---------------------------------------------------------
                    // Retail hands it its OWN unused R8 home slot (X+0x18) as
                    // scratch — a float argument in XMM2 leaves that slot free.
                    // Four qwords is strictly more than the 16 bytes retail's
                    // window can hold.
                    uint64_t scratch[4];
                    rtgfxbig::RtGraphics_Renderer_DrawItemSort(list._data, list._size, scratch);
                    ++rtgfxbig::g_drawSorts;
                }
            }

            ++r->currentScene;
            r->currentDrawGroup = nextGroup;
            ++rtgfxbig::g_drawGroups;
        } while (r->currentScene < r->scenes._size);
    }

    // ---------------- PASS 2: emit, BACKWARD ------------------------------
    for (int64_t si = static_cast<int32_t>(r->scenes._size) - 1; si >= 0; --si) {
        // `_data` is RE-READ at the top of every iteration (MOV RBP,[RDI] at
        // 0x180104d60) while the byte offset is carried in RBX and decremented.
        RtGraphics::RendererSceneEntry* const e = r->scenes._data + si;

        r->field_3c = e->field_e8;
        ctx->_dirty |= 0xc0;                        // OR [RSI+0x10],0xc0
        ctx->_shaderDetail = e->field_e8;           // MOV [RSI+0xc],EAX

        // Slot 9 on every drawable the entry names. 5th argument is a literal 1.
        for (int32_t j = 0; j < e->drawableIds._size; ++j) {
            const int32_t id = e->drawableIds._data[j];
            RtGraphics::RendererDrawable& d = r->drawables._data[id];
            static_cast<const Light*>(d.object)->DrawShadowMap(   // B.3.x bridge
                context, *this, d.value, 1);
        }

        // Rebind the render target only on a change.
        if (ctx->_renderBuffer != e->renderBuffer) {
            ctx->_dirty |= 1;
            ctx->_renderBuffer = e->renderBuffer;
        }

        // Viewport then scissor, each reconciled against the buffer's full
        // extent. The RenderBuffer pointer is RE-READ for each of the two blocks
        // (0x180104dec and 0x180104e37) rather than reused.
        {
            const RtVideo::RenderBufferPrefix* rb =
                reinterpret_cast<const RtVideo::RenderBufferPrefix*>(e->renderBuffer);
            const int32_t w = rb->width, h = rb->height;
            if (!(ctx->_viewport.x == 0 && ctx->_viewport.y == 0 &&
                  ctx->_viewport.width == w && ctx->_viewport.height == h)) {
                ctx->_dirty |= 0x200;
                // MOV QWORD PTR [RSI+0x1d0],0x0 — x and y as ONE 8-byte store.
                *reinterpret_cast<uint64_t*>(&ctx->_viewport) = 0;
                ctx->_viewport.width = w;
                ctx->_viewport.height = h;
            }
        }
        {
            const RtVideo::RenderBufferPrefix* rb =
                reinterpret_cast<const RtVideo::RenderBufferPrefix*>(e->renderBuffer);
            const int32_t w = rb->width, h = rb->height;
            if (!(ctx->_scissor.x == 0 && ctx->_scissor.y == 0 &&
                  ctx->_scissor.width == w && ctx->_scissor.height == h)) {
                ctx->_dirty |= 0x400;
                *reinterpret_cast<uint64_t*>(&ctx->_scissor) = 0;
                ctx->_scissor.width = w;
                ctx->_scissor.height = h;
            }
        }

        // Clear — slot 4 of Context::_contextData. The colour 0x1184d6 and the
        // flag word 6 are IMMEDIATES in the instruction stream (0x180104e8d,
        // 0x180104e85); the 1.0f depth is XMM6, loaded once at 0x180104d3d from
        // [0x18016169c], whose four bytes were READ (00 00 80 3f) rather than
        // assumed (reference_crt_coefficients_must_be_lifted_not_retyped.md).
        {
            void* const cd = ctx->_contextData;
            reinterpret_cast<rtgfxbig::ClearFn>(rtgfxbig::VTable(cd)[4])(cd, 6, 0x1184d6u, 1.0f, 0);
        }

        // Three passes x every drawable. Slot 10.
        for (int32_t k = 0; k < 3; ++k) {
            for (int32_t j = 0; j < e->drawableIds._size; ++j) {
                const int32_t id = e->drawableIds._data[j];
                RtGraphics::RendererDrawable& d = r->drawables._data[id];
                static_cast<const Light*>(d.object)->DrawLight(   // B.3.x bridge
                    context, *this, e->drawGroupIndex, d.value, k, e->field_ec);
            }
        }
    }
}

}  // namespace Graphics
