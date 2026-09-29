// ============================================================================
// runtime/src/graphics/wave29_lane_slots.cpp — WAVE 29 LANE SLOTS
// ============================================================================
//
// TWO SLOTS of `??_7MeshInstance@Graphics@@6B@`, taking it 6/9 -> 8/9:
//
// | slot | symbol                                        | retail      | extent |
// |------|-----------------------------------------------|-------------|--------|
// |  1   | ?Update@MeshInstance@Graphics@@UEAAX…@Z        | 0x180100800 |  343 B |
// |  7   | ?SetSharedInstanceData@MeshInstance@Graphics@@ | 0x180101660 |  280 B |
//
// The third, slot 2 `?Draw@MeshInstance@` (0x180100960, 1458 B / 5 chained
// .pdata fragments), is DECLINED ON SIZE — see WAVE29_LANE_SLOTS.md §6. That is
// a size judgement with no premise that can expire, stated in those words, and
// NOT dressed as a blocker.
//
// ---------------------------------------------------------------------------
// PROVENANCE — objdump and pefile against the shipped file. ZERO Ghidra calls.
// ---------------------------------------------------------------------------
//     objdump -d -M intel --start-address=A --stop-address=B
//             "C:/GOG Games/Banished/Runtime-x64.dll"
//     pefile against the same file for the export directory, .rdata and .pdata
//
// ---------------------------------------------------------------------------
// EXTENT — derived, and by TWO INDEPENDENT ALGORITHMS
// ---------------------------------------------------------------------------
// Lane GFXBODY found a DEFLATING defect in this lane's first extent walker: it
// walked FORWARD and required each successor `.pdata` row to begin EXACTLY at
// the predecessor's end, so any alignment padding between fragments terminates
// the walk early and the body reports SHORTER than it is. A second walker was
// written to GFXBODY's design — index every chained `UNWIND_INFO` on the PARENT
// `RUNTIME_FUNCTION` in its tail and close transitively, never comparing
// addresses at all — and the two were run side by side.
//
// They agree on all twelve subjects, including five controls drawn from three
// different lanes:
//
//   PARTSYS   ?Update@ParticleSystemInstance@   157 -> 5242  (33.4x)  BOTH
//   Wave 28   ?AddDrawItems@MeshInstance@        83 ->  720  ( 8.7x)  BOTH
//   Wave 28   ?AddDrawItems@EdgeMeshInstance@   205 ->  373  ( 1.8x)  BOTH
//   GFXBODY   ?GetRootPoseAtTime@Animation@      37 ->  495  (13.4x)  BOTH
//   GFXBODY   ?UpdateNodes@AnimationState@      126 -> 2191  (17.4x)  BOTH
//
// So the figures in the table above stand — but the FIRST walker was sound by
// luck and the second is sound by construction, and this comment records which.
//
// The worst understatement this lane measured is `?Draw@EdgeMeshInstance@`
// (0x1800a3150): first `.pdata` entry 25 bytes, full chain 1757 bytes over 7
// fragments — **70.3x**, which beats the project's previously recorded worst
// (33x) by more than two. `?Draw@MeshInstance@` is 29.2x and
// `?DrawShadowMap@SunLight@` is 18.0x. A 25-to-50-byte first entry in this
// family means nothing at all.
//
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../types/graphics/AnimationState.h"   // ENDGAME B.3.5 block GC
#include "../../types/graphics/Mesh.h"
#include "../../types/system/Value.h"              // System::Time, ditto
#include "../../types/video/Context.h"             // ENDGAME B.3.5 caller batch

using RtGraphics::MeshInstance;

namespace {

inline uint8_t*       B(void* p)       { return reinterpret_cast<uint8_t*>(p); }
inline const uint8_t* B(const void* p) { return reinterpret_cast<const uint8_t*>(p); }

template <typename T> T Rd(const void* p, size_t off) {
    T v; std::memcpy(&v, B(p) + off, sizeof(T)); return v;
}

// Math::Transform — 28 bytes, three floats of translation then a four-float
// rotation. The size is pinned by wave23_lane_gfxbody.cpp's own
// `static_assert(sizeof(AnimTransform) == 0x1c)` and re-confirmed here by the
// 0x1c stride of retail's own identity-table fill loop (see kIdentityTable).
struct Transform28 { uint8_t b[0x1c]; };
static_assert(sizeof(Transform28) == 0x1c, "Math::Transform is 28 bytes");

uint64_t g_miUpdate = 0;         // ?Update@MeshInstance@ entries
uint64_t g_miUpdateInit = 0;     // ... times THIS process ran the lazy init
uint64_t g_miUpdateChild = 0;    // ... attached children ticked
uint64_t g_miSSID = 0;           // ?SetSharedInstanceData@MeshInstance@ entries
uint64_t g_miSSIDskip = 0;       // ... early-outs on the non-negative flag

// ---------------------------------------------------------------------------
// THE TWO UNEXPORTED `.data` OBJECTS `?Update@MeshInstance@` OWNS
// ---------------------------------------------------------------------------
// These are MSVC function-local statics: a guard dword and the array it
// guards. Both were checked individually against the retail EXPORT DIRECTORY
// with pefile and NEITHER has an entry, so they are genuinely address-only —
// this is NOT the `AEAA`-means-private misreading that has produced three
// silent address doors in this project
// (reference_aeaa_is_private_not_unexported.md).
//
//   SYMBOL:      (unexported) MeshInstance::Update's static-init guard,
//                retail 0x1801ff30c, `.data`. Bit 0 is the "initialised" bit.
//   CONDITION:   live whenever the DLL is present.
//   CONSEQUENCE: null ⇒ this body must not run the lazy init and must not
//                dereference the table; it returns early and says so once.
//
//   SYMBOL:      (unexported) the 48-entry identity `Math::Transform` table,
//                retail 0x1801ff310 .. 0x1801ff850, `.data`.
//   CONDITION:   same.
//   CONSEQUENCE: it is handed to AnimationState::Update as the SECOND
//                InterpolatedState<Transform>* argument. Passing null there
//                would not be a graceful degradation, it would be a wild read
//                inside the DLL, so this body declines to call at all.
//
// *** THE ADDRESS IS THE POINT, NOT A CONVENIENCE. *** Retail's own other
// readers of this table see whatever we write into it, and we see theirs. A
// copy of our own would be a SECOND table with the same contents and a
// different address, and the lazy-init guard would then be set by whichever of
// the two bodies ran first while the other's table stayed zero — the
// two-owners-one-object shape this project has recorded on singleton rows.
//
// ENDGAME B.3.9 block GFX (B39_RULES D7, rows A47 / A48): FILE STATICS. With
// no DLL there are no "retail's own other readers"; this body is the ONE site
// of both, so the two objects live here and the mint's two by-address storage
// rows are retired (rt_mint_retired/gfx.txt). Zero-initialised by the
// language, as retail's .data is: the guard's bit 0 starts clear and the first
// Update fills the table, exactly as before. Namespace-scope, not
// function-local (loader thread: no lazy statics).
uint32_t    g_updateInitGuard = 0;           // retail 0x1801ff30c
Transform28 g_identityTable[48] = {};        // retail 0x1801ff310 .. 0x1801ff850
static_assert(sizeof(g_identityTable) == 1344, "0x540 bytes, the mint row's size");
uint32_t*    UpdateInitGuard() { return &g_updateInitGuard; }
Transform28* IdentityTable()   { return g_identityTable; }

//   SYMBOL:      ?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA, retail
//                0x1801fc70b, ordinal 4796 — a DATA export, a single static
//                bool. mesh.cpp §3 of its hold-back list already records that
//                it cannot be REGISTERED (the registry redirects calls) and
//                that its three existing consumers read and write it in place.
//   CONDITION:   live whenever the DLL is present; at cutover it must be minted.
//   CONSEQUENCE: null ⇒ retail's byte is unreachable, and this body would have
//                to invent arg7. It declines instead.
// ENDGAME B.3.9 GFX (D7 row A22): the class static (redirected onto the
// mint's row), not the by-address door.
const bool* RootOnlyUpdates() {
    return &Graphics::MeshInstance::_rootOnlyUpdates;
}

// ?Update@AnimationState@Graphics@@ — retail 0x1800fdb20, ordinal 4474, PORTED
// (wave27_lane_gfxbig.cpp) and reached BY NAME, so ResolveExport's
// registry-first lookup hands back OUR body, not the DLL's
// (reference_resolveaddr_consults_port_registry_first.md). Eight parameters
// plus `this`, in the order its own mangled name gives them — the identical
// typedef wave23_lane_gfxbody.cpp:543 uses, repeated rather than shared so this
// file's call site can be read without opening that one.
// ENDGAME B.3.5 block GC: the typedef that used to sit here is GONE with the
// resolve it served -- `AnimationState::Update` is a member now and the call site
// is a qualified member call. The mangled name above is kept because it is what
// settled the argument mapping, and the eight parameters are the member's.

// ?UpdateHierarchy@MeshInstance@Graphics@@QEAAXXZ — retail 0x1800ff910, PORTED
// (mesh.cpp:2903) and reached BY NAME for the same reason.
using UpdateHierarchyFn = void (*)(void* self);

// ?GetBones@MeshInstance@Graphics@@QEBAHMAEBVTransform@Math@@PEAVMatrix@4@@Z
//   SYMBOL:      retail 0x180100070, an ordinary public export.
//   CONDITION:   NOT PORTED. ResolveExport therefore falls through to the DLL's
//                own body (reference_converting_a_door_is_not_satisfying_it.md
//                — this is a door, and opening it is not satisfying it).
//   CONSEQUENCE: null ⇒ no skinning matrices are produced and nothing is handed
//                to the Context, so a skinned mesh would render in its bind
//                pose. Announced once rather than silently skipped.
// ENDGAME B.3.5 caller batch: the "NOT PORTED" verdict above is STALE —
// `Graphics::MeshInstance::GetBones` is a DEFINED member, so the typedef that
// used to sit here is gone and `SetSharedInstanceData` calls the member.

// ?SetBones@Context@Video@@QEAAXHPEBVMatrix@Math@@@Z
//   SYMBOL:      retail 0x18001a780, an ordinary public export.
//   CONDITION:   PORTED (BATCH 8, runtime/src/video/context.cpp
//                RtVideo_Context_SetBones). ResolveExport now reaches our body.
// ENDGAME B.3.5 caller batch: a DEFINED member, so this typedef went too.

}  // namespace

// ============================================================================
// void Graphics::MeshInstance::Update(const System::Time&)
//   retail 0x180100800 — slot 1 — 343 B, ONE .pdata fragment (both walkers)
// ============================================================================
//
// FIVE THINGS THIS BODY DOES THAT READING THE SHAPE DOES NOT SUGGEST, each
// measured in the listing rather than inferred:
//
// 1. IT LAZILY INITIALISES A PROCESS-WIDE STATIC TABLE, AND IT SETS THE GUARD
//    BIT *BEFORE* FILLING THE TABLE (0x180100831 `or eax,1` / 0x180100834
//    store, then the fill loop at 0x180100850). That ordering is retail's and
//    is reproduced. It is not thread-safe and this port does not make it so:
//    adding a fence or an atomic would be a behaviour our standalone has and
//    retail does not, and the project's rule is to fix an upstream difference,
//    not to improve on retail at a faithful site.
//
// 2. THE FILL IS 48 ENTRIES, NOT 47. `lea ecx,[rbx+0x2f]` with rbx = 0 gives
//    ecx = 47, and the loop is `body; dec ecx; jns body` — so the body runs
//    once for ecx = 47 and once more for each of 46..0, i.e. FORTY-EIGHT
//    times, 0x1c bytes apart, covering 0x1801ff310..0x1801ff850 = 0x540.
//    A reader who takes the initialiser as the count is short by one entry,
//    and the last entry is the one that stays zero.
//
// 3. EACH ENTRY IS WRITTEN BY FOUR STORES THAT COVER ALL 28 BYTES WITH NO GAP:
//    qword +0x00 = 0, dword +0x08 = 0, qword +0x0c = 0x3f800000 (i.e. the FLOAT
//    1.0f at +0x0c and zero at +0x10), qword +0x14 = 0. There is no untouched
//    padding here — unlike Particle's ctor, where two gaps cost an earlier lane
//    two false divergences. This port writes the same four stores in the same
//    order rather than a memset plus a float, because the qword carrying
//    0x3f800000 is a single 8-byte store of an integer constant and splitting
//    it into a float and a zero is a different instruction sequence with the
//    same result — reproduced as retail writes it.
//
// 4. ARG 8 OF AnimationState::Update IS THE LITERAL `true`, AND ARG 7 IS THE
//    STATIC BOOL, NOT THE OTHER WAY ROUND. `mov BYTE PTR [rsp+0x40],0x1`
//    (0x1801008d1) is the LAST stack argument; `?_rootOnlyUpdates@` is loaded
//    with `movzx eax,BYTE PTR [rip+…]` into `[rsp+0x38]` (0x1801008d6). Both
//    are BYTE writes into the argument area; the typedef declares both as
//    `bool` so the compiler emits byte-width stores too.
//
// 5. THE SECOND `InterpolatedState<Transform>*` IS THAT GLOBAL IDENTITY TABLE,
//    not a second per-instance array. `lea rax,[rip+0xfea23]` at 0x1801008e6
//    resolves to 0x1801ff310, the same object the fill loop just wrote. The
//    FIRST is `this->_nodes._data` (+0xf0). Crossing the two would hand the
//    animator the instance's own poses as its rest pose, which is a silent
//    no-op on an unanimated mesh and wrong on an animated one.
//
// The tail is ordinary and is reproduced without comment: if the animator
// returns false, re-run `UpdateHierarchy`; then dispatch this instance's OWN
// vtable slot 4 (`+0x20` = `UpdateAttached`); then walk `Geometry::_attached`
// (+0x18 data, +0x20 count) and call each child's vtable slot 1 (`+0x8`).
// ============================================================================

namespace Graphics {

void MeshInstance::Update(const System::Time& timeRef) {
    ++g_miUpdate;

    const void* const time = reinterpret_cast<const void*>(&timeRef);   // B.3.5 bridge
    uint8_t* const me = B(static_cast<void*>(this));

    uint32_t* const guard = UpdateInitGuard();
    Transform28* const ident = IdentityTable();
    const bool* const rootOnly = RootOnlyUpdates();
    if (!guard || !ident || !rootOnly) {
        static bool said = false;
        if (!said) {
            said = true;
            std::fprintf(stderr,
                "[SLOTS29] MeshInstance::Update: guard=%p identityTable=%p "
                "_rootOnlyUpdates=%p -- one is null, declining to call "
                "AnimationState::Update with an invented argument\n",
                (void*)guard, (void*)ident, (void*)rootOnly);
        }
        return;
    }

    // ---- 1..3: the lazy identity-table init (0x180100825 .. 0x180100869) ----
    if ((*guard & 1u) == 0) {                      // 0x18010082d  test al,1
        *guard |= 1u;                              // 0x180100834  BEFORE the fill
        ++g_miUpdateInit;
        for (int i = 0; i < 48; ++i) {             // see note 2: 48, not 47
            uint8_t* const e = reinterpret_cast<uint8_t*>(ident + i);
            const uint64_t zero = 0;
            const uint64_t one  = 0x3f800000ull;   // float 1.0f at +0x0c, 0 at +0x10
            const uint32_t z32  = 0;
            std::memcpy(e + 0x00, &zero, 8);       // 0x180100850
            std::memcpy(e + 0x08, &z32,  4);       // 0x180100854
            std::memcpy(e + 0x0c, &one,  8);       // 0x180100857
            std::memcpy(e + 0x14, &zero, 8);       // 0x18010085f
        }
    }

    // ---- the base Transform, copied field by field from Geometry +0x84 ------
    // Retail copies it as SEVEN separate dword moves (0x18010086b..0x1801008ad)
    // rather than a 28-byte block, and this port keeps the field granularity so
    // the read set is identical; the destination is a local either way.
    Transform28 base;
    for (int i = 0; i < 7; ++i) {
        const uint32_t w = Rd<uint32_t>(me, 0x84 + 4u * i);
        std::memcpy(base.b + 4 * i, &w, 4);
    }

    // ---- the animator call (0x1801008b1 .. 0x1801008ff) --------------------
    void* const mesh = Rd<void*>(me, 0xc0);        // ExternalPtr<GraphicsMesh>: pointee @ +0
    void* const nodesArray = Rd<void*>(me, 0xf0);  // _nodes._data
    // ENDGAME B.3.5 block GC item 6: this was a `ResolveExport` name door onto
    // `?Update@AnimationState@Graphics@@` reaching OUR OWN body through the port
    // registry, with a one-shot "resolved NULL" announcement behind it.
    // `Graphics::AnimationState::Update` is a MEMBER as of block GC commit 2
    // (body in `wave27_lane_gfxbig.cpp`), so the nine-argument resolve is a
    // QUALIFIED non-virtual member call -- retail's own direct CALL to
    // 0x1800fdb20 -- and the resolver, its cache and its null guard are gone with
    // it (the block brief's "a file-local cache whose last caller you converted
    // goes with its guard").
    //
    // `me + 0xd8` is `MeshInstance::_animation`, the 0x18-byte PREFIX of an
    // `AnimationState` that `Mesh.h` embeds; the rest of the record lives in the
    // AnimationManager's instance pool, and `AnimationState.h` pins the boundary
    // at `offsetof(_rootOffset) == 0x18` so it cannot move silently. The cast is
    // that prefix relationship, not a reinterpretation of the class.

    // r8 = mesh + 0x60 is `const Array<Node>&` — the ADDRESS of the embedded
    // array, not a loaded pointer. Mesh.h's own banner records that confusing
    // the embedded +0x60 with the pointer-to-array +0x70 cost Wave 6 an
    // integration run, and is silent whenever the garbage count reads 0.
    const bool ok =
        reinterpret_cast<::Graphics::AnimationState*>(me + 0xd8)    // B.3.5 bridge
            ->::Graphics::AnimationState::Update(
                *reinterpret_cast<const System::Time*>(time),
                *reinterpret_cast<const Collection::Array<uint8_t>*>(
                    B(mesh) + 0x60),                   // &mesh->_nodes
                *reinterpret_cast<const Math::Transform*>(&base),
                nodesArray,                            // arg 4: THIS instance's poses
                ident,                                 // arg 5: the identity table
                *reinterpret_cast<Math::Vector3*>(me + 0x100),  // &_rootAnimationOffset
                *rootOnly,                             // arg 7 (note 4)
                true);                                 // arg 8 (note 4)

    if (!ok) {                                          // 0x180100906
        // ENDGAME B.3.5 block E: this was
        // `ResolveExport("?UpdateHierarchy@MeshInstance@Graphics@@QEAAXXZ")` — a
        // runtime-internal caller reaching its own class through the port
        // registry. The body is now a member (`mesh.cpp`), so the call is direct
        // and QUALIFIED, and the resolve and its null check are gone.
        this->MeshInstance::UpdateHierarchy();
    }

    // ---- this instance's OWN vtable slot 4, UpdateAttached (0x180100916) ----
    // A VIRTUAL dispatch through the object, NOT a direct call: a subclass
    // override must win, and this is the one line that makes the difference
    // observable. ENDGAME B.3.5 block E: this was a hand-inlined
    // `(*(void***)me)[4]`, and an ordinary UNqualified C++ virtual call is the
    // same instruction on the same vptr — including when that vptr is the MINTED
    // table, whose slot 4 holds scene.cpp's registered `UpdateAttached` door.
    // tests/unit/graphics_geometry_abi_test.cpp PART D is that equivalence.
    UpdateAttached();

    // ---- the attached-geometry loop (0x180100919 .. 0x18010093c) -----------
    // Count RELOADED from +0x20 every iteration (0x180100939 compares against
    // memory, not a cached register), so a child that appends during its own
    // Update is visited. The data pointer at +0x18 is also reloaded each pass —
    // reading `_attached._size` / `_attached._data` inside the loop is that same
    // reload, one spelling up.
    //
    // The child call was a hand-inlined `(*(void***)child)[1]`; it is now the
    // virtual `Update` on the real `Graphics::Geometry*` the array holds, which
    // is the same dispatch through the same slot.
    for (int32_t i = 0; i < _attached._size; ++i) {
        Geometry* const child = _attached._data[i];
        child->Update(timeRef);
        ++g_miUpdateChild;
    }

}

// ============================================================================
// int Graphics::MeshInstance::SetSharedInstanceData(Video::Context&, int, float)
//   retail 0x180101660 — slot 7 — 280 B, ONE .pdata fragment (both walkers)
// ============================================================================
//
// THE PACKED ARGUMENT IS THE WHOLE FUNCTION, and it is invisible in C:
// `arg2` carries TWO fields, and retail SIGN-EXTENDS it before splitting
// (`movsxd r9,r8d` at 0x180101670) and then masks:
//
//     lo = arg2 & 0xfff          the LIST index within the detail level
//     hi = (arg2 >> 12) & 0xf    the DETAIL LEVEL
//
// The shift is an ARITHMETIC shift of the sign-extended 64-bit value
// (`sar r8,0xc`), so a negative arg2 shifts in ones — and the `& 0xf` then
// discards them. Reproducing this with a 32-bit logical shift gives the same
// answer for every value the caller can produce and a DIFFERENT one for a
// negative arg2; this port keeps the 64-bit signed form so the two agree
// everywhere, not just on the reachable inputs.
//
// THE EARLY OUT IS THE COMMON PATH AND IT IS A SIGN TEST, NOT A ZERO TEST.
// `cmp DWORD PTR [rax+rdx*8+0x14],0` / `jge` (0x1801016a2) — a NON-NEGATIVE
// dword at +0x14 of the list record means "already has shared data, nothing to
// do", and the function returns 0. Only a NEGATIVE value falls through.
// (reference_a_writer_beats_readers_agreeing_on_a_sign.md applies in reverse
// here: this is a READER, so this comment does not claim to name the field —
// it records the test it performs and nothing more.)
//
// THE ADDRESS ARITHMETIC, exactly as retail computes it:
//   levels = *(void**)(mesh + 0x50)          the detail-level array data
//   rec    = *(void**)(levels + hi*0xb0 + 0x98)
//   flag   = *(int32*)(rec + (lo*3)*8 + 0x14)
// `lea rdx,[r9+r9*2]` is `lo*3`, and it is then scaled by 8 in the addressing
// mode — i.e. the record stride is 24 bytes, reached as three qwords. Written
// here as `lo * 24` with the derivation spelled out, because `lo*3` scaled by
// 8 and `lo*24` are the same number reached two ways and only one of them says
// what the stride is.
//
// THE SCRATCH IS 47 MATRICES, AND THE COUNT COMES FROM THE SAME `dec/jns`
// SHAPE AS Update's — but here the initialiser is 0x2f and the loop writes
// SIXTEEN floats per pass at a 0x40 stride, so the covered range is
// 47 * 0x40 = 0xbc0 bytes. That is an array of 47 `Math::Matrix`, zeroed, and
// it is the buffer `GetBones` fills and `SetBones` consumes.
//   *** THE TWO `dec/jns` LOOPS IN THIS FILE HAVE THE SAME INITIALISER, 0x2f,
//   AND DIFFERENT TRIP COUNTS. *** The difference is WHERE THE `dec` SITS
//   RELATIVE TO THE BODY, and it is the only thing that decides:
//     Update  (0x180100850): body … `dec ecx` … `jns` — the dec is at the END,
//             so the body also runs for ecx = 47 itself. FORTY-EIGHT passes.
//     this one (0x1801016c0): `dec ecx` … body … `jns` — the dec is the LOOP
//             HEAD, so ecx is already 46 on the first body. FORTY-SEVEN passes,
//             covering rsp+0x40 .. rsp+0x40+47*0x40 = 0xbc0 bytes.
//   Counted from the listing in both cases, never from the initialiser, because
//   the initialiser is the same number and the answers differ by one.
//
// THE TRANSFORM HANDED TO GetBones IS AN IDENTITY BUILT ON THE STACK, not the
// instance's own: `[rsp+0x20..0x38]` gets 0,0,0 then 1.0f then 0,0,0 — the same
// seven-float identity the other body writes 48 copies of into `.data`. Two
// bodies in one file building the same constant two different ways is retail's,
// and it is the corroboration that 0x1c is the Transform size and that the
// 1.0f sits at +0x0c.
//
// AND IT RETURNS 0 UNCONDITIONALLY. `xor eax,eax` at 0x18010176d is the MERGE
// POINT of both paths — the early-out and the do-the-work path reach the same
// three instructions. A port that returned the bone count on the working path
// would be a plausible reading of the shape and is wrong.
// ============================================================================
int32_t MeshInstance::SetSharedInstanceData(Video::Context& context, int32_t packed,
                                            float t) const {
    ++g_miSSID;

    // ENDGAME B.3.5 caller batch: the `void* ctx` bridge local is gone with the
    // `SetBones` name door below — `context` is already the real class.
    const uint8_t* const me = B(static_cast<const void*>(this));

    const int64_t wide = static_cast<int64_t>(packed);      // movsxd r9,r8d
    const uint32_t lo  = static_cast<uint32_t>(wide) & 0xfffu;
    const uint32_t hi  = static_cast<uint32_t>(wide >> 12) & 0xfu;   // sar, then &0xf

    const void* const mesh = Rd<const void*>(me, 0xc0);
    const uint8_t* const levels = Rd<const uint8_t*>(mesh, 0x50);
    const uint8_t* const rec = Rd<const uint8_t*>(levels, hi * 0xb0u + 0x98u);
    // UNGUARDED, and deliberately: retail dereferences `levels` and `rec`
    // without a test. If either is null here, something upstream handed us a
    // mesh with no detail level and THAT is the bug
    // (CLAUDE.md, "Don't 'fix' a faithful crash with a null check").
    if (Rd<int32_t>(rec, static_cast<size_t>(lo) * 24u + 0x14u) >= 0) {
        ++g_miSSIDskip;
        return 0;                                            // 0x1801016a7  JGE
    }

    // ---- the 47-matrix scratch, zeroed (0x1801016b5 .. 0x180101715) --------
    float bones[47 * 16];
    for (int i = 0; i < 47 * 16; ++i) bones[i] = 0.0f;

    // ---- the identity Transform (0x18010171f .. 0x180101750) ---------------
    float ident[7] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};

    // ENDGAME B.3.5 caller batch: both were `ResolveExport` name doors into
    // locals, with a one-shot "resolved NULL" announcement behind them. Both are
    // DEFINED members today — `Graphics::MeshInstance::GetBones` in
    // animationstate_transforms.cpp:1127 and `Video::Context::SetBones` in
    // video/context.cpp:396 — so both are direct member calls, `GetBones`
    // qualified and non-virtual on `this` (retail's own direct CALL to
    // 0x180100070). The two typedefs, the two locals and the announcement are
    // gone: a member call cannot be null, so there was nothing left to announce.
    const int32_t n = this->MeshInstance::GetBones(
        t, *reinterpret_cast<const Math::Transform*>(ident),
        reinterpret_cast<Math::Matrix*>(bones));              // 0x180101759
    context.SetBones(n, reinterpret_cast<const Math::Matrix*>(bones));  // 0x180101768

    return 0;                                                // ALWAYS 0 — see banner
}

}  // namespace Graphics

// Kickoff warning 10: a `*Summary()` nothing calls reports a zero that means
// "never printed", not "never called". This one IS wired: RuntimeInit()
// (standalone/runtime_init.cpp) calls it once at boot.
extern "C" void RtGraphicsWave29SlotsSummary() {
    // UNQUALIFIED `printf`, and the Makefile's check-log-strip gate is why:
    // standalone/log_strip.h does `#define printf(...) ((void)0)` and the
    // preprocessor cannot replace a QUALIFIED name, so `std::printf` survives
    // into the log-stripped build. The gate caught this as written and the
    // compiler would have blamed log_strip.h, not this line.
    printf("[SLOTS29] MeshInstance::Update entries=%llu lazyInitRanHere=%llu "
                "childrenTicked=%llu | SetSharedInstanceData entries=%llu skipped=%llu\n",
                (unsigned long long)g_miUpdate, (unsigned long long)g_miUpdateInit,
                (unsigned long long)g_miUpdateChild,
                (unsigned long long)g_miSSID, (unsigned long long)g_miSSIDskip);
}
