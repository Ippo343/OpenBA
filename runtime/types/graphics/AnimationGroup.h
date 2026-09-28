#pragma once
// ============================================================================
// runtime/types/graphics/AnimationGroup.h — ENDGAME B.3.5 step 5, block H1
//   the REAL `Graphics::AnimationGroup`.
// ============================================================================
//
// *** B35_RULES section 2.2 LISTED THIS LAYOUT AS UNDOCUMENTED ("hdr" column
// `—`), AND section 13 item 3 SAID THE SIZE WAS VERIFIED BUT THE FIELDS WERE
// NOT. THEY ARE NOW, AND THE HEADER DID NOT HAVE TO BE INVENTED: *** the whole
// record was already pinned, as a file-local `struct AnimationGroup` inside
// `runtime/src/graphics/vtbatch7_animgroup_particlesystem.cpp:228-239`, by the
// batch that ported all nine of its slots. This header MOVES that declaration
// into `runtime/types/` under the retail name and adds the members; it does not
// re-derive it, and the offsets are byte for byte the ones that file asserted.
//
// The evidence that file records, restated so it travels with the declaration:
//
//   * 0x58 total — the `MOV EDX,0x58` in the vector deleting destructor at
//     0x1800167f0, which `runtime/src/object/deleting_dtor.cpp:974` carries as
//     `RT_DELDTOR(0x1800167f0, 0x1800fb620, 0x058, "Graphics::AnimationGroup")`.
//   * +0x20 `Array<Animation>` — `?GetAnimation@AnimationGroup@Graphics@@
//     QEBAAEBVAnimation@2@H@Z` @ 0x180016660 is the `imul $0x70` element helper
//     over it, and the mangled RETURN TYPE names the element `Animation`, which
//     is what fixes both the base (+0x20) and the stride (0x70).
//     `Array<Animation>::SetSize` is 0x180059e70 (`kSpec_AnimClip70`).
//   * +0x30 `float` — the constructor (0x180016550 / 0x1800165d0, which inline
//     the same stores) writes 0x3dcccccd == 0.1f there, and
//     `?Serialize@AnimationGroup@Graphics@@` (0x1800fb6a0) visits it through the
//     ReaderWriter's FLOAT scalar slot, AFTER the tag array.
//   * +0x38 `Array<{String,String,float}>` stride 0x18 — read off Serialize's
//     own element loop (two `System::String::Serialize` calls then one float);
//     `SetSize` 0x18005a220 (`kSpec_TagStrStrF18`).
//   * +0x48 `Array<float>` — Serialize hands it to the ReaderWriter's BULK
//     float-array slot; `SetSize` 0x18005a410 (`kSpec_I32`).
//     `AnimationManager.h`'s `anim_group::kFadeTable` reads the same field as
//     the blend-time table, indexed `[fromClip * clipCount + toClip]`.
//
// THE +0x34 PAD IS EXPLICIT AND IT IS NOT A GUESS OF CONTENT: no body this
// project has read writes those four bytes. It exists because a `float` at
// +0x30 followed by a pointer at +0x38 has to be separated by something, and
// naming it a pad says exactly as much as is known.
//
// PARENT: `Object::External`, proved by `?_ParentTypeName@AnimationGroup@
// Graphics@@CAPEBDXZ` @ 0x1800162e0 returning the literal "Object::External"
// (B35_SLOTS.md:498, read byte-wise). NOT `CompiledExternal` — the constructor
// stamps `??_7Base@Object@@6B@`, then `??_7External@Object@@6B@`, then its own,
// with no `CompiledExternal` step. That is the exact mistake B35_RULES section
// 2.1 warns about for `Video::VideoProgram`.
//
// RULE B5-15 — THE TWO NAME LITERALS, RECORDED AND NOT USED. Nothing here
// re-mints or retires a reflection row (B35_RULES section 11): the two
// `_typeInfo` rows and the `??_7AnimationGroup@Graphics@@6B@` mint row stay, and
// slots 1/2 keep reading `?_typeInfo@AnimationGroup@Graphics@@0VBaseTypeInfo@
// Object@@A` (0x1801fa6d0). The literals are written down so a later
// `DEFINE_TYPE` is COPIED and not derived from a C++ base name:
//     ?_TypeName@AnimationGroup@Graphics@@       -> "AnimationGroup"
//     ?_ParentTypeName@AnimationGroup@Graphics@@ -> "Object::External"
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "../object/External.h"
#include "../object/Internal.h"      // B.3.9: InternalMemFree (the concrete slot 0)
#include "../collection/Collection.h"
#include "Animation.h"

namespace System { class Time; }

namespace Graphics {

// ============================================================================
// Graphics::AnimationGroup — 0x58 bytes, nine slots.
//
// `??_7AnimationGroup@Graphics@@6B@` @ 0x180154e10, read out of `.rdata` one
// qword at a time (B35_SLOTS.md:41-50):
//
//   [0] +0x00  0x1800167f0  (unexported) the vector deleting dtor
//   [1] +0x08  0x180016310  ?TypeName@
//   [2] +0x10  0x1800162f0  ?Type@
//   [3] +0x18  0x1800fb680  ?GetFileVersion@   (`MOV EAX,8; RET`)
//   [4] +0x20  0x1800fb690  ?Compile@          (`MOV EAX,1; RET`)
//   [5] +0x28  0x1800fb6a0  ?Serialize@
//   [6] +0x30  0x180016550  ?Reconstruct@
//   [7] +0x38  0x180016650  ?Destruct@
//   [8] +0x40  0x1800165d0  ?Construct@        — INTRODUCED HERE (RULE B5-16)
//
// SLOT 0 IS LEFT PURE, which keeps the class abstract. Nothing holds an
// `AnimationGroup` by value anywhere in the tree (B34_BRIEFS A.4 as amended:
// abstract where nothing does), and its deleting-dtor body is
// `rtdtor::RtDelDtor_AnimationGroup`, registered at 0x1800167f0, which
// B35_RULES section 11 forbids retiring. Declaring it `= 0` re-declares the
// slot at the right index without creating a second, divergent one.
//
// NO REAL CONSTRUCTOR (B4b-2 / ENDGAME section B.1 decision 3): `Construct`
// and `Reconstruct` are called on objects that ALREADY EXIST and that wear the
// MINTED table, and they re-stamp the three vtables by hand out of the mint.
// A member constructor there would stamp GCC's tables instead. The
// `ConstructBody` helper in vtbatch7_animgroup_particlesystem.cpp keeps doing
// the stamping; `Construct()` and `Reconstruct()` call it.
//
// THE DESTRUCTOR IS DUPLICATED, NOT DELEGATED — the `Object::External`
// precedent. `??1AnimationGroup@Graphics@@UEAA@XZ` @ 0x1800fb620 is already a
// registered door in `runtime/src/object/deleting_dtor_leaves.cpp`, which is NOT
// block H1's file set; it stamps the MINTED table, which is right for a minted
// object and wrong for a converted one. The member is declared and defined
// empty so the chain emits GCC's stamps, and the door is untouched.
// ============================================================================
//
// ENDGAME B.3 step 9 (block GFX, B39_RULES D1/D2) -- the three paragraphs above
// are HISTORY. The class is CONCRETE: a real public ctor (retail 0x1800fb5a0 /
// the factory's inlined stores), its own vector slot 0, a protected non-virtual
// ~AnimationGroup holding the retired ??1 door's body, the factory pair as the
// class's own statics; Construct / Reconstruct placement-new the class. The
// minted `??_7AnimationGroup@Graphics@@6B@`, its RT_DELDTOR row 0x1800167f0,
// the FACTREST row, the ??1 door and the eight slot doors are retired.
// ============================================================================
class AnimationGroup : public Object::External {
public:
    // ??0AnimationGroup@Graphics@@QEAA@XZ 0x1800fb5a0 -- the REAL ctor (retail
    // also inlines it into CreateNewObject / CreateInPlaceObject / Construct /
    // Reconstruct): External(), the three Array headers zeroed (their own
    // ctors), +0x30 = 0.1f (0x3dcccccd). +0x34 is written by nothing.
    AnimationGroup() : External() {
        _defaultBlend = 0.1f;
    }

    // [0] 0x1800167f0 -- MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0x58` = the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<AnimationGroup*>(self + static_cast<int64_t>(i) * 0x58)->~AnimationGroup();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~AnimationGroup();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }

    // [1] 0x180016310 TypeName / [2] 0x1800162f0 Type, TypeId 0x180016300, `_TypeName` / `_ParentTypeName`
    // and the factory pair: retail's macro (ENDGAME B.3.9 TI-REST, B39_RULES
    // D6); the record is `DEFINE_TYPE`d in
    // runtime/src/graphics/graphics_type_records.cpp.
    DECLARE_TYPE(AnimationGroup)
public:
    int32_t     GetFileVersion() const override;           // [3] 0x1800fb680
    CompileResult Compile(const System::Filename& path) override;  // [4] 0x1800fb690
    bool        Serialize(void* stream) override;          // [5] 0x1800fb6a0
    Object::External* Reconstruct() override;              // [6] 0x180016550
    void        Destruct() override;                       // [7] 0x180016650
    virtual Object::External* Construct();                 // [8] 0x1800165d0 — B5-16

    // ---- the non-slot exported members -------------------------------------
    // ?GetAnimation@AnimationGroup@Graphics@@QEBAAEBVAnimation@2@H@Z 0x180016660
    //   NOT PORTED. `imul $0x70` over `_clips`; it is the body that names the
    //   element type and therefore pins `Graphics::Animation`'s size.
    const Animation& GetAnimation(int index) const;

    // ?GetAnimationFromName@…QEBAHPEBD@Z 0x1800fb340 — NOT PORTED
    int32_t GetAnimationFromName(const char* name) const;

    // ?GetBlendTime@…QEBA?AVTime@System@@HH@Z 0x1800fb4a0 — NOT PORTED.
    //   Hidden retslot (`System::Time` by value). Reads `_blendTimes` at
    //   [from * _clips._size + to] — `anim_group::kFadeTable`.
    System::Time GetBlendTime(int fromClip, int toClip) const;

    // ?GetDefaultBlendTime@…QEBA?AVTime@System@@XZ 0x1800fb4f0 — NOT PORTED.
    //   Hidden retslot. Reads `_defaultBlend` (+0x30), in SECONDS.
    System::Time GetDefaultBlendTime() const;

    // ??0AnimationGroup@Graphics@@QEAA@AEBV01@@Z    0x180016670 — NOT PORTED
    // ??4AnimationGroup@Graphics@@QEAAAEAV01@AEBV01@@Z 0x180016730 — NOT PORTED
    AnimationGroup(const AnimationGroup&);
    AnimationGroup& operator=(const AnimationGroup&);

    // ??2AnimationGroup@Graphics@@SAPEAX_K@Z          0x180016320  — NOT PORTED
    // ??2AnimationGroup@Graphics@@SAPEAX_KPEAX@Z      0x180016390  — NOT PORTED
    // ??3AnimationGroup@Graphics@@SAXPEAX@Z           0x1800163a0  — NOT PORTED
    // ?CreateNewObject@…SAPEAVBase@Object@@XZ         0x180016400  — (D2) the class's own
    // ?CreateInPlaceObject@…SAPEAVBase@Object@@PEAX@Z 0x1800164d0  — (D2) the class's own
    //   The three operators are declared so no caller batch has to edit this
    //   header and stay undefined: placement is spelled `::new`.
    static void* operator new(std::size_t bytes);
    static void* operator new(std::size_t bytes, void* place);
    static void  operator delete(void* p);
    // (CreateNewObject / CreateInPlaceObject: declared by DECLARE_TYPE above.)

protected:
    // ??1AnimationGroup@Graphics@@UEAA@XZ  0x1800fb620. NON-virtual (no virtual
    // dtor until the normal-virtual cutover). Retail's body is in
    // vtbatch7_animgroup_particlesystem.cpp; the compiler chains ~External.
    ~AnimationGroup();

public:
    // ---- the 0x38 bytes AnimationGroup adds --------------------------------
    // PUBLIC for the same reason `Object::External`'s are: unconverted bodies
    // reach them at these offsets through a cast.
    //
    // The three Array members are spelled `Collection::Array<uint8_t>` — a RAW
    // 16-byte header — and NOT `Array<Animation>` / `Array<float>`, for the
    // reason `AnimationManager.h`'s `AnimStateRec` already spells them that way:
    // element construction, copy and destruction for these three run through
    // the REGISTERED `kSpec_*` rows in `runtime/src/collection/array.cpp`, at
    // retail's own addresses, not through a C++ template instantiation. Giving
    // the member the typed form would make the compiler emit a SECOND element
    // lifecycle beside the ported one. The true element type is named here
    // instead, which is what a reader needs.
    Collection::Array<uint8_t> _clips;       // +0x20  Array<Graphics::Animation>, stride 0x70
    float    _defaultBlend;                  // +0x30  seconds; ctor default 0.1f
    uint32_t _pad34;                         // +0x34  never written by any body read
    Collection::Array<uint8_t> _tags;        // +0x38  Array<{String,String,float}>, stride 0x18
    Collection::Array<uint8_t> _blendTimes;  // +0x48  Array<float>, the fade table
};
static_assert(sizeof(AnimationGroup) == 0x58,
              "Graphics::AnimationGroup is 0x58 bytes -- the deleting dtor 0x1800167f0's "
              "element size and the MOV EDX,0x58 it was read from");
static_assert(alignof(AnimationGroup) == 8, "Graphics::AnimationGroup aligns to 8");
static_assert(offsetof(AnimationGroup, _clips)       == 0x20, "AnimationGroup::_clips @ +0x20");
static_assert(offsetof(AnimationGroup, _defaultBlend)== 0x30, "AnimationGroup::_defaultBlend @ +0x30");
static_assert(offsetof(AnimationGroup, _tags)        == 0x38, "AnimationGroup::_tags @ +0x38");
static_assert(offsetof(AnimationGroup, _blendTimes)  == 0x48, "AnimationGroup::_blendTimes @ +0x48");
static_assert(sizeof(Animation) == 0x70,
              "AnimationGroup::_clips has stride 0x70 (GetAnimation's imul); if "
              "Graphics::Animation ever stops being 0x70 the array walks wrong");

}  // namespace Graphics
