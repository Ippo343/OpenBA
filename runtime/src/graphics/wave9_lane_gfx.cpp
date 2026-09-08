// ============================================================================
// runtime/src/graphics/wave9_lane_gfx.cpp — Wave 9 lane GFX
// ============================================================================
//
// Cutover boot attempt 06 left 61 exports resolving to NOTHING. Four of them
// are closed here, plus one callee each for two of them:
//
//   ?Lerp@Transform@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z          0x18001d690
//   ?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@...       0x18001aff0
//   ?RenderTextures@MaterialInstance@Graphics@@QEBAX...             0x180102df0
//   ?AddDrawItem@Renderer@Graphics@@QEAAXPEBVGeometry@2@...         0x1801057b0
//   (the PerlinNoise ctor is closed in ../utility/perlin.cpp, where its body
//    already sat, written and unregistered, since Wave 8 lane E.)
//
// WHY A GRAPHICS FILE REGISTERS A MATH EXPORT. `Math::Transform::Lerp` is not
// in any lane's function list and NOTHING in this project's tree resolves it by
// name — it is here only because `InterpolatedState<Transform>::Lerp` is eleven
// instructions of argument shuffling around a CALL to it, and a port of the
// caller that reached the callee through the DLL would be a port that dies at
// cutover (reference_converting_a_door_is_not_satisfying_it.md). It is
// registered under this lane's kill switch, not Math's; that is a deliberate,
// stated coupling, not an oversight.
//
// FLOAT ASSOCIATION ORDER IS LOAD-BEARING. Wave 9 has already found one defect
// where a translation term folded in FIRST instead of being added LAST — the
// same value algebraically, a different value in bits, and visible in one call
// in 23,654 (reference_moved_term_hides_where_terms_cancel.md). Every float
// expression below is transcribed in the order the SUBSS/MULSS/ADDSS sequence
// gives it and must not be "simplified".
//
// KILL SWITCH: BANISHED_RT_GFX9=0 leaves all four on the DLL.
// ============================================================================

#include <cstdint>
#include <cstring>

#include "rt_diff.h"
#include "rt_port.h"

#include "../../types/math/Math.h"
#include "../../types/graphics/Camera.h"
#include "../../types/graphics/Material.h"

// wave9_lane_p.cpp (Wave 9 lane P) — the ported Quaternion::Lerp. Called
// DIRECTLY, exactly as retail's Transform::Lerp calls 0x18001d500, because a
// resolve-by-address here would walk past lane P's own name registration and
// land in the DLL (reference_bypassed_port_needs_a_source_checker.md).
Quaternion* RtMath_Quaternion_Lerp(const Quaternion* self, Quaternion* ret,
                                   const Quaternion* other, const Scalar* t);

using RtGraphics::MaterialInstance;
using RtGraphics::Renderer;
using RtGraphics::RendererDrawGroup;
using RtGraphics::RendererDrawItem;

namespace {

// ---------------------------------------------------------------------------
// Object::Base::IsDerivedFrom, reached PORT-FIRST.
// ?IsDerivedFrom@Base@Object@@QEBA_NI@Z, retail 0x1800c0390, ported in
// runtime/src/object/type_system.cpp. Resolved through ResolveExport (which
// consults the port registry) rather than ResolveAddr, and cached once.
// ---------------------------------------------------------------------------
bool CallIsDerivedFrom(const void* obj, uint32_t typeId) {
    using Fn = bool (*)(const void*, uint32_t);
    Fn fn = RT_EXPORT_ONCE(Fn, "?IsDerivedFrom@Base@Object@@QEBA_NI@Z");
    return fn ? fn(obj, typeId) : false;
}

// ---------------------------------------------------------------------------
// The type id RenderTextures tests against.
//
// Retail's instruction is `MOV EDX,[0x1801f9a20]`. That address is not an
// anonymous global: `pefile` on the shipped DLL says 0x1801f9a20 IS the export
// `?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A`
// (ordinal 4864), and the FIRST DWORD of an Object::BaseTypeInfo is its type
// id — the same shape runtime/src/ui/element.cpp already reads for
// ClipRegionElement and ScrollElement. In the image at rest it is 0xffffffff;
// the reflection registry fills it in at startup.
//
// AT CUTOVER this symbol is NOT minted, so the resolve returns null, the id
// stays 0xffffffff, IsDerivedFrom is false for everything and the
// render-to-texture pre-pass silently does nothing. No crash — a degraded
// sub-feature, and strictly better than today's null AddDrawItem. It is
// written up as the one open caveat in docs/runtime_port/WAVE9_LANE_GFX.md §5.
//
// SPELLED WITH A LITERAL `ResolveExport` ON PURPOSE, rather than through the
// RT_DATA_SLOT macro that expands to exactly this. tools/rt_data_door_audit.py
// matches a door by the identifier immediately in front of the mangled literal;
// RT_DATA_SLOT is not in its door table, so a macro spelling files this read
// under "UNCLASSIFIED doors — triage these, they are not known to be safe".
// The semantics are identical (cache the HIT, retry a miss, so a read before
// the mint is not poisoned) and the audit can now see the door is port-aware.
// ---------------------------------------------------------------------------
uint32_t GeneratedImageBufferTypeId() {
    static const uint32_t* cached = nullptr;
    if (!cached)
        cached = reinterpret_cast<const uint32_t*>(ResolveExport(
            "?_typeInfo@GeneratedImageBuffer@Graphics@@0VBaseTypeInfo@Object@@A"));
    return cached ? *cached : 0xffffffffu;
}

// ---------------------------------------------------------------------------
// MemAlloc / MemFree, the two halves of the Array helper below.
//
// RT_EXPORT_ONCE, not RtOriginal: RtOriginal IS GetProcAddress and it also
// bypasses our own ported allocator (reference_rtoriginal_is_getprocaddress.md,
// and allocator.cpp registers both of these names).
// ---------------------------------------------------------------------------
void* CallMemAlloc(uint64_t size, int align) {
    using Fn = void* (*)(uint64_t, int);
    Fn fn = RT_EXPORT_ONCE(Fn, "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z");
    return fn ? fn(size, align) : nullptr;
}
void CallMemFree(void* p) {
    using Fn = void (*)(void*);
    Fn fn = RT_EXPORT_ONCE(Fn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
    if (fn) fn(p);
}

// ---------------------------------------------------------------------------
// Collection::Array<RendererDrawItem>::Resize(int newCount, const T& fill)
// retail 0x180063680 — UNEXPORTED, so it is reproduced here rather than called.
//
// This is the template instantiation for the 0x38-byte draw-item element: the
// byte size is `IMUL RCX,RCX,0x38` at 0x1800636d3 and the copy loop moves seven
// qwords per element at 0x180063715..0x180063747. It is NOT a general helper
// shared with other element types, so reproducing it here duplicates nothing.
//
// Retail's growth rule, transcribed exactly (it is NOT the usual doubling):
//   * newCount == oldCount            -> return, no work at all
//   * newCount <= capacity            -> no reallocation; only the NEW slots
//                                        [oldCount, newCount) are filled
//   * newCount >  capacity            -> capacity = newCount, EXCEPT that a
//                                        one-element growth from a non-empty
//                                        array sets capacity = newCount*3;
//                                        then allocate and copy [0, oldCount)
//                                        from the old buffer and fill the rest
//   * the old buffer is freed only if it actually changed and was non-null
//
// The `EAX = (RBP == this->_data) ? oldCount : 0` at 0x1800636e4 is what makes
// one loop serve both cases: on a reallocation it restarts at 0 and re-copies
// the surviving elements, on an in-place grow it starts at oldCount.
//
// ELEMENTS ARE COPIED AS RAW BYTES — seven qword MOVs, no element constructor,
// no destructor on the freed buffer. RendererDrawItem is trivially copyable so
// std::memcpy reproduces that exactly, INCLUDING the two padding gaps the
// caller never writes.
// ---------------------------------------------------------------------------
void DrawItemArrayResize(Collection::Array<RendererDrawItem>* self,
                         int32_t newCount,
                         const RendererDrawItem* fill) {
    const int32_t oldCount = self->_size;
    if (newCount == oldCount)
        return;

    RendererDrawItem* oldData = self->_data;
    self->_size = newCount;

    if (newCount > self->_capacity) {
        self->_capacity = newCount;
        // 0x1800636be: the *3 over-allocation applies ONLY to a single-element
        // append onto a non-empty array. A bulk resize allocates exactly what
        // was asked for, and the first append onto an empty array allocates 1.
        if (newCount == oldCount + 1 && oldCount != 0)
            self->_capacity = newCount * 3;
        self->_data = static_cast<RendererDrawItem*>(
            CallMemAlloc(static_cast<uint64_t>(self->_capacity) * 0x38u, 8));
    }

    int32_t i = (oldData == self->_data) ? oldCount : 0;
    for (; i < newCount; ++i) {
        const RendererDrawItem* src = (i < oldCount) ? (oldData + i) : fill;
        std::memcpy(self->_data + i, src, sizeof(RendererDrawItem));
    }

    if (oldData != self->_data && oldData != nullptr)
        CallMemFree(oldData);
}

}  // namespace

// ============================================================================
// Math::Transform Math::Transform::Lerp(const Transform&, const Scalar&) const
// ?Lerp@Transform@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z   retail 0x18001d690
//                                                          ordinal 3193
//
// Member, struct return: RCX = this (the FROM transform), RDX = hidden return
// slot, R8 = the TO transform, R9 = &t.
//
// TRANSLATION, from 0x18001d696..0x18001d6fa, term for term:
//
//   x:  XMM2 = to.x - this->x ; XMM1 = t ; XMM1 = XMM1 * XMM2 ;
//       XMM8 = this->x        ; XMM8 = XMM8 + XMM1
//   y:  XMM4 = to.y - this->y ; XMM4 = XMM4 * t ;
//       XMM7 = this->y        ; XMM7 = XMM7 + XMM4
//   z:  XMM3 = to.z - this->z ; XMM3 = XMM3 * t ;
//       XMM6 = this->z        ; XMM6 = XMM6 + XMM3
//
// i.e. `a + t*(b - a)` for all three, NOT `a*(1-t) + b*t`. Those two are not
// the same float. The x lane multiplies `t * (b-a)` and the y/z lanes multiply
// `(b-a) * t`; IEEE multiplication is commutative to the bit, so the mirrored
// operand order is preserved below only for readers diffing against the
// listing, and changes nothing.
//
// ROTATION: `CALL 0x18001d500` at 0x18001d6fe with RCX unchanged (= this, and
// Transform::rotation is at +0x00, so `this` doubles as `&this->rotation`),
// RDX = a stack scratch at RSP+0x20, R8 = the TO transform, R9 = &t. That is
// Quaternion::Lerp(this->rotation, scratch, to.rotation, t) — lane P's port.
// The four dwords of the scratch are then copied to ret+0x00..0x0f.
//
// ALIASING: every input reaches a register (or the stack scratch) before the
// first store into `ret`, so retail tolerates `ret == this` or `ret == to`.
// This port keeps that property: the three translation floats are computed into
// locals and the quaternion into a local before anything is written.
// ============================================================================
Transform* RtMath_Transform_Lerp(const Transform* self, Transform* ret,
                                 const Transform* other, const Scalar* t) {
    RT_DIFF_BEGIN("Math::Transform::Lerp");

    // The oracle runs the shipped body AFTER ours has written `ret`, and `ret`
    // is allowed to be `self` or `other`. Without these snapshots the original
    // is handed input we just destroyed and "diverges" on nearly every call —
    // the artefact that nearly got a CORRECT Transform::operator* deregistered
    // (reference_oracle_must_snapshot_aliased_inputs.md).
    Transform selfSnap{}, otherSnap{};
    Scalar    tSnap{};
    if (rt_diff_on__) { selfSnap = *self; otherSnap = *other; tSnap = *t; }

    const float tv = t->v;

    const float ax = self->translation.x;
    const float ay = self->translation.y;
    const float az = self->translation.z;

    const float rx = ax + tv * (other->translation.x - ax);   // 0x18001d6d8/e1/ed
    const float ry = ay + (other->translation.y - ay) * tv;   // 0x18001d6dd/e5/f2
    const float rz = az + (other->translation.z - az) * tv;   // 0x18001d6e9/f6/fa

    Quaternion rot{};
    RtMath_Quaternion_Lerp(&self->rotation, &rot, &other->rotation, t);

    ret->rotation = rot;            // the four-dword copy at 0x18001d703..716
    ret->translation.x = rx;        // 0x18001d71c/728/732, in that register order
    ret->translation.y = ry;
    ret->translation.z = rz;

    if (rt_diff_on__) {
        Transform theirs{};
        using Fn = Transform* (*)(const Transform*, Transform*, const Transform*,
                                  const Scalar*);
        RtOriginal<Fn>("?Lerp@Transform@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z")(
            &selfSnap, &theirs, &otherSnap, &tSnap);
        // All 0x1c bytes are written by both sides (quaternion 0x10 +
        // Vector3 0xc, no padding), so a full compare has no unwritten-byte
        // gap to trip over.
        RT_DIFF_BYTES(ret, &theirs, sizeof(Transform));
    }
    return ret;   // retail returns the hidden slot in RAX
}

// ============================================================================
// Math::Transform
// Graphics::InterpolatedState<Math::Transform>::Lerp(const Scalar&) const
// ?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@AEBVScalar@4@@Z
// retail 0x18001aff0, ordinal 3188
//
// Eleven instructions. RCX = this, RDX = hidden return slot, R8 = &t:
//
//     MOV R9,R8  /  MOV R8,RCX  /  ADD RCX,0x1c  /  MOV RBX,RDX
//     CALL 0x18001d690        ; Transform::Lerp(this+0x1c, ret, this+0x00, t)
//     MOV RAX,RBX
//
// PLAN §4.2 deferred this in Wave 1 for want of `InterpolatedState<T>`'s
// layout; Wave 6 lane E pinned the layout (0x38 = two adjacent 0x1c-byte
// Transforms) but explicitly did NOT pin which half is which, and named this
// function as the one that would. It does: the FROM end is +0x1c and the TO end
// is +0x00, so +0x00 is CURRENT and +0x1c is PREVIOUS. That is now written into
// runtime/types/graphics/Mesh.h.
//
// Wave 1's own note in runtime/src/math/transform.cpp read this call site as
// having "only ONE Transform operand (at +0x1c) plus a bare Scalar (at +0x00),
// with no second Transform argument visible". That was wrong, and the missing
// operand is the `MOV R8,RCX`: `this` itself IS the second Transform. The note
// has been corrected in place.
// ============================================================================
Transform* RtGraphics_InterpolatedStateTransform_Lerp(const void* self,
                                                      Transform* ret,
                                                      const Scalar* t) {
    RT_DIFF_BEGIN("Graphics::InterpolatedState<Transform>::Lerp");

    // Same aliasing rule as above — `ret` may point into `self`'s own 0x38.
    uint8_t selfSnap[0x38];
    Scalar  tSnap{};
    if (rt_diff_on__) { std::memcpy(selfSnap, self, sizeof(selfSnap)); tSnap = *t; }

    const uint8_t* base = static_cast<const uint8_t*>(self);
    RtMath_Transform_Lerp(reinterpret_cast<const Transform*>(base + 0x1c),
                          ret,
                          reinterpret_cast<const Transform*>(base),
                          t);

    if (rt_diff_on__) {
        Transform theirs{};
        using Fn = Transform* (*)(const void*, Transform*, const Scalar*);
        RtOriginal<Fn>(
            "?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@AEBVScalar@4@@Z")(
                selfSnap, &theirs, &tSnap);
        RT_DIFF_BYTES(ret, &theirs, sizeof(Transform));
    }
    return ret;
}

// ============================================================================
// void Graphics::MaterialInstance::RenderTextures(Renderer&, const Scene&,
//                                                 const Camera&, int) const
// ?RenderTextures@MaterialInstance@Graphics@@QEBAXAEAVRenderer@2@AEBVScene@2@AEBVCamera@2@H@Z
// retail 0x180102df0, ordinal 3570
//
// Not on the 61-symbol cutover list in its own right — it is here because
// AddDrawItem calls it, and a port of AddDrawItem that reached it through the
// DLL would only MOVE the cutover row one frame deeper instead of closing it.
//
// Walks MaterialInstance::_textures (0x10 stride, count at +0x58), takes the
// ExternalPtr target at each entry's +0x08, and for every one whose +0x2c is
// exactly 5 AND which IsDerivedFrom(GeneratedImageBuffer) dispatches vtable
// slot +0x58 with (renderer, scene, camera, arg) — i.e. "render this
// render-to-texture now, before the item that samples it is drawn".
//
// TWO FAITHFUL UGLINESSES, both kept:
//   * `obj->+0x2c` is dereferenced BEFORE `obj` is null-checked (0x180102e49
//     vs 0x180102e61). A null entry faults in retail and faults here. Adding
//     the guard would not restore retail's behaviour, it would skip it
//     (CLAUDE.md, "don't fix a faithful crash with a null check").
//   * the loop bound `*(int*)(this+0x58)` is RE-READ every iteration
//     (0x180102e83), not hoisted, so a slot-58 handler that appends a texture
//     is seen. Hoisting it would change behaviour in exactly that case.
// ============================================================================
void RtGraphics_MaterialInstance_RenderTextures(const MaterialInstance* self,
                                                void* renderer,
                                                const void* scene,
                                                const void* camera,
                                                int arg) {
    RT_DIFF_BEGIN("Graphics::MaterialInstance::RenderTextures");

    // Effectiveness control: how many calls got past BOTH early-outs and could
    // have dispatched anything at all. A green `calls=` column on a site that
    // always returned at 0x180102e0c would mean nothing
    // (reference_probe_needs_an_effectiveness_control.md).
    const bool didWork = (self->hasRenderedTexture != 0) && (self->textures._size > 0);
    RT_DIFF_WORK(didWork);

    if (self->hasRenderedTexture == 0)
        return;
    if (self->textures._size <= 0)
        return;

    const uint32_t typeId = GeneratedImageBufferTypeId();

    for (int32_t i = 0; i < self->textures._size; ++i) {
        const uint8_t* entry =
            reinterpret_cast<const uint8_t*>(self->textures._data) +
            static_cast<int64_t>(i) * 0x10;
        void* obj = *reinterpret_cast<void* const*>(entry + 8);

        if (*reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(obj) + 0x2c) != 5)
            continue;
        if (!CallIsDerivedFrom(obj, typeId))
            continue;
        if (obj == nullptr)      // retail's order: after the deref above
            continue;

        void** vt = *reinterpret_cast<void***>(obj);
        using Slot58 = void (*)(void*, void*, const void*, const void*, int);
        reinterpret_cast<Slot58>(vt[0x58 / 8])(obj, renderer, scene, camera, arg);
    }
}

// ============================================================================
// void Graphics::Renderer::AddDrawItem(const Geometry*, const MaterialInstance*,
//                                      unsigned short, unsigned short,
//                                      float, float, int, const void*, unsigned)
// ?AddDrawItem@Renderer@Graphics@@QEAAXPEBVGeometry@2@PEBVMaterialInstance@2@GGMMHPEBXI@Z
// retail 0x1801057b0, ordinal 1547
//
// ARGUMENT SLOTS, counted against the MANGLED NAME and confirmed against the
// prologue (`PUSH RDI; SUB RSP,0x70` = 0x78, so the 5th argument's home slot
// 0x28 lands at RSP+0xa0, and the body reads 0xa0/0xa8/0xb0/0xb8/0xc0/0xc8 —
// six stack arguments for arguments 5..10, which is exactly what GGMMHPEBXI
// predicts after the four register slots). NO hidden return slot: the return
// type is `X` (void).
//
// WHY THIS IS PORTED NOW AND WAS HELD BACK IN WAVE 6. Lane F gave two reasons:
// (1) no safe oracle, and (2) "it writes an element type this lane did not pin
// … the array helper it hands that to has a stride this lane did not read out."
// Reason (2) IS NOW FALSE and was falsifiable at the time: the helper is
// 0x180063680, its stride is the literal `0x38` in `IMUL RCX,RCX,0x38`, and the
// element is fully written out in runtime/types/graphics/Camera.h.
// Reason (1) still stands and is honoured — see "ORACLE" below.
//
// THE THREE SILENT DROPS. Retail returns without appending anything when:
//   (a) the draw group's pass mask (+0x1bc) has no bit set for the material's
//       shader class (+0x20) — `BT EAX,R11D`, a 32-bit BT, so the index is
//       taken modulo 32;
//   (b) the group's shader index (+0x1b8) is negative, or >= the Material's
//       program count at +0x30;
//   (c) the selected VideoProgram (Material+0x28 indexed by that shader) has a
//       null +0x20, or a null +0x28/+0x30, or either of those two sub-objects
//       has +0x48 <= 0.
// This is the same list lane F recovered by decompilation; reading the bytes
// reproduces it unchanged, which is a cross-check, and it confirms the claim at
// standalone/scene_watch.cpp:593 that AddDrawItem silently drops items.
//
// ORACLE: THERE IS NONE, AND THAT IS STATED RATHER THAN FAKED. The function's
// entire effect is an append into a LIVE array that the array helper may
// reallocate and free — running the shipped body a second time would either
// share `_data` with the real group (so "theirs" frees memory the live Renderer
// still points at) or need a deep copy of a group whose other 0x1c0 bytes this
// lane does not own. What IS instrumented:
//   * RT_DIFF_WORK records how many calls reached the append at all, so a
//     "clean" run on a site that always dropped cannot read as verification
//     (reference_uncalled_port_is_not_verified.md);
//   * RT_DIFF_SHADOW checks that the element the ARRAY HELPER landed in memory
//     is byte-identical to the record we handed it, over the 0x34 bytes that
//     are actually written. That tests THE HELPER — the one piece of new
//     transcription with a mechanical answer — and it does NOT test the port's
//     agreement with retail. Do not read it as if it did.
//
// The 0x34-not-0x38 span is the point of the two `_pad` fields: retail stores
// uninitialised stack bytes at +0x24 and +0x34 on every append, and a compare
// that included them would fail on garbage neither side wrote
// (reference_oracle_must_not_compare_unwritten_bytes.md).
// ============================================================================
void RtGraphics_Renderer_AddDrawItem(Renderer* self,
                                     const void* geometry,
                                     const MaterialInstance* material,
                                     uint16_t arg4,
                                     uint16_t arg5,
                                     float arg6,
                                     float arg7,
                                     int32_t arg8,
                                     const void* arg9,
                                     uint32_t arg10) {
    RT_DIFF_BEGIN("Graphics::Renderer::AddDrawItem");

    // MOVSXD, both of them — Ghidra's C prints these as zero extensions and
    // that misreading produced three Wave 6 defects including a crash
    // (reference_ghidra_c_output_vs_disassembly.md).
    const int64_t groupIdx = static_cast<int32_t>(self->currentDrawGroup);   // +0x34
    const int64_t pass     = static_cast<int32_t>(material->shaderClass);    // +0x20

    RendererDrawGroup* group = reinterpret_cast<RendererDrawGroup*>(
        reinterpret_cast<uint8_t*>(self->drawGroups._data) + groupIdx * 0x1f0);

    // (a) BT EAX,R11D — 32-bit BT masks the bit index to 5 bits.
    if (((group->passMask >> (static_cast<uint32_t>(pass) & 31u)) & 1u) == 0u) {
        RT_DIFF_WORK(false);
        return;
    }

    const int64_t  shader = static_cast<int32_t>(group->shader);              // +0x1b8
    const uint8_t* mtl    = static_cast<const uint8_t*>(material->material);  // +0x28

    // (b)
    if (static_cast<int32_t>(shader) < 0) { RT_DIFF_WORK(false); return; }
    if (static_cast<int32_t>(shader) >= *reinterpret_cast<const int32_t*>(mtl + 0x30)) {
        RT_DIFF_WORK(false);
        return;
    }

    // (c) Material+0x28 is the VideoProgram* array MaterialInstance::Setup also
    // hands to Video::VideoProgram::SetupShaders, so these are VideoProgram*.
    const uint8_t* const* programs =
        *reinterpret_cast<const uint8_t* const* const*>(mtl + 0x28);
    const uint8_t* prog = programs[shader];

    if (*reinterpret_cast<const void* const*>(prog + 0x20) == nullptr) { RT_DIFF_WORK(false); return; }
    const uint8_t* sub28 = *reinterpret_cast<const uint8_t* const*>(prog + 0x28);
    if (sub28 == nullptr) { RT_DIFF_WORK(false); return; }
    if (*reinterpret_cast<const int32_t*>(sub28 + 0x48) <= 0) { RT_DIFF_WORK(false); return; }
    const uint8_t* sub30 = *reinterpret_cast<const uint8_t* const*>(prog + 0x30);
    if (sub30 == nullptr) { RT_DIFF_WORK(false); return; }
    if (*reinterpret_cast<const int32_t*>(sub30 + 0x48) <= 0) { RT_DIFF_WORK(false); return; }

    RT_DIFF_WORK(true);

    // NOT value-initialised, deliberately: retail's record is a raw stack frame
    // and +0x24 / +0x34 are never stored to before the helper copies all seven
    // qwords out of it. Zeroing them here would be a "tidier" port that no
    // longer reproduces what lands in the array.
    RendererDrawItem rec;

    // 0x18010584d: 0x8000 unless the 7th argument is exactly 1.0f. The compare
    // is UCOMISS + JP/JNZ, so a NaN takes the not-equal path — which `!=` in
    // C++ also does.
    uint16_t flags = (arg7 == 1.0f) ? 0u : 0x8000u;
    flags = static_cast<uint16_t>(flags | arg4);   // OR AX,R9W — a 16-bit OR

    rec.geometry = geometry;                          // [RSP+0x30]
    rec.material = mtl;                               // [RSP+0x38]
    rec.instance = material;                          // [RSP+0x40]
    rec.flags    = flags;                             // [RSP+0x48]
    rec.arg5     = arg5;                              // [RSP+0x4a]
    rec.sortKey  = arg6;                              // [RSP+0x4c]
    rec.arg8     = arg8;                              // [RSP+0x50]
    rec.userData = arg9;                              // [RSP+0x58]
    rec.arg10    = arg10;                             // [RSP+0x60]

    // `group + 0x1c0 + pass*0x10` (LEA RCX,[R11+0x1c]; SHL RCX,4; ADD RCX,R10),
    // written as raw byte arithmetic ON PURPOSE. `&group->items[pass]` would be
    // the same address, but `items` is declared [3] and retail applies NO bound
    // check — spelling it as an array index would let the compiler assume
    // pass < 3 and is undefined behaviour on the path retail simply takes.
    Collection::Array<RendererDrawItem>* list =
        reinterpret_cast<Collection::Array<RendererDrawItem>*>(
            reinterpret_cast<uint8_t*>(group) + 0x1c0 + pass * 0x10);
    const int32_t oldSize = list->_size;
    DrawItemArrayResize(list, oldSize + 1, &rec);

    if (rt_diff_on__) {
        // See the ORACLE paragraph in the banner: this checks the HELPER, not
        // the port. 0x34 bytes, not 0x38 — +0x34..0x37 is never written, and
        // +0x24..0x27 is skipped for the same reason.
        const uint8_t* landed = reinterpret_cast<const uint8_t*>(list->_data + oldSize);
        const uint8_t* mine   = reinterpret_cast<const uint8_t*>(&rec);
        const bool ok = list->_size == oldSize + 1 &&
                        std::memcmp(landed, mine, 0x24) == 0 &&
                        std::memcmp(landed + 0x28, mine + 0x28, 0x0c) == 0;
        RT_DIFF_SHADOW(ok, "appended record (0x34 written bytes)",
                       "array helper's copy");
    }

    // The render-to-texture pre-pass. `hasRenderedTexture` is +0x25, the byte
    // BuildParameters sets when any bound texture's +0x2c == 5.
    if (material->hasRenderedTexture != 0) {
        const uint8_t* entries = reinterpret_cast<const uint8_t*>(self->scenes._data);
        const int64_t  off     = static_cast<int32_t>(self->currentScene) * 0x110;  // MOVSXD
        RtGraphics_MaterialInstance_RenderTextures(
            material,
            self,
            *reinterpret_cast<const void* const*>(entries + off),   // entry.scene
            entries + off + 0x10,                                   // &entry.camera
            self->contextSerial);                                   // +0x44
    }
}

// ============================================================================
// Registration
// ============================================================================
int RtGraphicsWave9GfxRegister() {
    int n = 0;

    RegisterRuntimeExport("?Lerp@Transform@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z",
                          (void*)&RtMath_Transform_Lerp); ++n;
    RegisterRuntimeAddr(0x18001d690, (void*)&RtMath_Transform_Lerp);

    RegisterRuntimeExport(
        "?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@AEBVScalar@4@@Z",
        (void*)&RtGraphics_InterpolatedStateTransform_Lerp); ++n;
    RegisterRuntimeAddr(0x18001aff0, (void*)&RtGraphics_InterpolatedStateTransform_Lerp);

    RegisterRuntimeExport(
        "?RenderTextures@MaterialInstance@Graphics@@QEBAXAEAVRenderer@2@AEBVScene@2@AEBVCamera@2@H@Z",
        (void*)&RtGraphics_MaterialInstance_RenderTextures); ++n;
    RegisterRuntimeAddr(0x180102df0, (void*)&RtGraphics_MaterialInstance_RenderTextures);

    RegisterRuntimeExport(
        "?AddDrawItem@Renderer@Graphics@@QEAAXPEBVGeometry@2@PEBVMaterialInstance@2@GGMMHPEBXI@Z",
        (void*)&RtGraphics_Renderer_AddDrawItem); ++n;
    RegisterRuntimeAddr(0x1801057b0, (void*)&RtGraphics_Renderer_AddDrawItem);

    return n;
}
