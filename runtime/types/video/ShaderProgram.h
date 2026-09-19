#pragma once
// ============================================================================
// runtime/types/video/ShaderProgram.h — `Video::ShaderProgram` and its two
// concrete subclasses, `Video::VertexProgram` and `Video::PixelProgram`, as
// REAL C++ CLASSES (ENDGAME B.3.5 block B, B35_RULES 2.1/2.2, section 10 move 4).
// ============================================================================
//
// Every offset is pinned by a named instruction in RETAIL Runtime-x64.dll
// (image base 0x180000000), read out of `objdump -d` over the shipped file —
// never out of Ghidra's decompiled C
// (reference_ghidra_c_output_vs_disassembly.md). The layout below is UNCHANGED
// from the POD this header carried before B.3.5: the three asserted offsets and
// the two sizes are the same numbers, now on a class with a base and a vtable.
//
// ---------------------------------------------------------------------------
// Video::ShaderProgram : Object::CompiledExternal   (sizeof 0x50, 8 slots)
// ---------------------------------------------------------------------------
// ??0ShaderProgram@Video@@QEAA@XZ (0x1800f5030) stamps four vtables in
// sequence — ??_7Base@Object@@6B@ (0x180154788), ??_7External@Object@@6B@
// (0x1801547a0), ??_7CompiledExternal@Object@@6B@ (0x180154a30) and finally
// ??_7ShaderProgram@Video@@6B@ (0x180157b60) — zeroes +0x08/+0x10/+0x18
// (External's Filename and Array) and then six more qwords, +0x20 .. +0x48.
// It never touches +0x50, and the vector deleting destructor at 0x18004e360
// carries `MOV EDX,0x50`, so sizeof is exactly 0x50. THE THIRD STAMP IS THE
// PROOF OF THE BASE: `?_ParentTypeName@ShaderProgram@Video@@CAPEBDXZ`
// (0x18004e090) returns "Object::CompiledExternal" and the ctor stamps that
// table, which `Video::VideoProgram`'s ctor does NOT (see VideoProgram.h).
//
//   +0x20  Array<Parameter>        _textures    (GetTextureCount  0x18004e410
//                                                reads +0x28; GetTextureName
//                                                0x18004e420 indexes +0x20 by
//                                                SHL 4; GetTextureDest
//                                                0x18004e470 reads elem+0x08;
//                                                IsTextureInternal 0x18004e480
//                                                reads elem+0x0c)
//   +0x30  Array<Parameter>        _constants   (GetConstantCount 0x18004e4a0
//                                                reads +0x38; GetConstantName
//                                                0x18004e4b0 indexes +0x30;
//                                                GetConstantDest  0x18004e500
//                                                reads elem+0x08)
//   +0x40  Array<Array<uint8_t>>   _byteCode    (GetByteCode 0x18004e100 is
//                                                `(Array<E>*)(+0x40) + idx*0x10`
//                                                and IsValid 0x18004e0f0 is
//                                                `+0x48 > 0`)
//
// ---------------------------------------------------------------------------
// Video::VertexProgram / Video::PixelProgram : ShaderProgram  (0x58, 9 slots)
// ---------------------------------------------------------------------------
// Each adds ONE pointer at +0x50 — the platform companion object — and nothing
// else. ??0VertexProgram (0x1800f6100) and ??0PixelProgram (0x1800f3950) are
// the ShaderProgram ctor inlined, followed by the class's own vtable stamp
// (0x180157b10 / 0x180157ac0) and an inlined PlatformConstruct that asks
// Video::Device::_singleton->deviceData for the companion through a DIFFERENT
// DeviceData vtable slot per class:
//     VertexProgram  slot +0x78   (CreateVertexProgramData)
//     PixelProgram   slot +0x80   (CreatePixelProgramData)
// ?GetData@VertexProgram@Video@@QEBAPEBVVertexProgramData@2@XZ (0x18004e720) is
// `MOV RAX,[RCX+0x50]; RET`, which names +0x50 in the export table itself.
// Both deleting destructors (0x18004e810, 0x18004eb90) carry `MOV EDX,0x58`.
//
// THE TWO ARE SEPARATE CLASSES, not one class and two typedefs (which is what
// this header carried before B.3.5). Their LAYOUT is identical; their vtable,
// their `_typeInfo`, their `_FileExtension` and the DeviceData slot their
// PlatformConstruct calls are not, and retail's `_ParentTypeName` for both is
// the literal "ShaderProgram" (0x18004e530 / 0x18004e8b0).
//
// ============================================================================
// *** NOT CONSTRUCTIBLE, AND SLOT 0 IS PURE. *** (B34_BRIEFS A.4)
// ============================================================================
// All three classes are ABSTRACT in C++ even though retail's VertexProgram and
// PixelProgram are concrete, and the reason is slot 0. Retail's slot 0 is a real
// per-class body (0x18004e360 / 0x18004e810 / 0x18004eb90) but OURS is not a
// per-class function at all: it is one table-driven body in
// `runtime/src/object/deleting_dtor.cpp`, registered BY ADDRESS through the
// `RT_DELDTOR(deletingDtor, dtor, size, name)` rows at lines 843-845, and
// ENDGAME B.2.5 / B35_RULES 11 keep every one of those rows for the whole of
// step 5. So slot 0 is declared `override = 0` — exactly the shape B.3.4 settled
// on for `IO::BinaryReader` — and that is also what makes the vtable EMITTABLE
// once `~ShaderProgram()` exists: defining a destructor on a polymorphic class
// makes GCC emit a vptr store, which needs `vtable for Video::ShaderProgram`,
// which needs every non-pure slot to have a body. Block A hit the same wall on
// the twelve `*Data` bases and answered it the same way.
//
// Abstract costs nothing here because NOTHING constructs one of these as a C++
// object: every live instance is allocated by `Object::ExternalFactory` /
// `rt_factories_video.inc` and built by the registered ctor DOORS in
// `runtime/src/video/{shaderprogram,vertexprogram,pixelprogram}.cpp`, which
// stamp the MINTED `??_7…@Video@@6B@` addresses. A real constructor cannot write
// a minted vtable address, which is B4b-2's rule and why those doors stay doors.
// The `protected = default` constructors are there so a probe SUBCLASS in
// `tests/unit/` can still reach them.
//
// NO VIRTUAL DESTRUCTOR (ENDGAME B.1 decision 2): slot 0 is
// `DeletingDtor(unsigned)`; `~ShaderProgram()` is non-virtual and has no slot.
// The destructor CHAIN is retail's teardown, and that is not a coincidence:
// ~VertexProgram (0x1800f61b0) is ~ShaderProgram (0x1800f50b0) inlined followed
// by ~External (0x1800bc950), which is precisely what GCC emits for
// `VertexProgram : ShaderProgram : CompiledExternal : External`.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "../collection/Collection.h"
#include "../object/External.h"
#include "../system/String.h"

namespace Video {

// ============================================================
// Video::ShaderProgram — a compiled shader and its parameter bindings.
// ============================================================
class ShaderProgram : public Object::CompiledExternal {
public:
    // ========================================================
    // Video::ShaderProgram::Parameter — 0x10 bytes, a `struct` (the mangled
    // `??0Parameter@ShaderProgram@Video@@QEAA@AEBU012@@Z` spells `U`, not `V`).
    // Named by ??0Parameter@ShaderProgram@Video@@QEAA@VString@System@@H_N@Z
    // (0x1800f4f30), whose (String, int, bool) argument list is exactly the
    // three fields the Serialize loop inside 0x1800f5110 touches:
    // String::Serialize at elem+0x00, ReaderWriter::Int at elem+0x08 and, when
    // GetVersion() >= 1, Bool at elem+0x0c. The last three bytes are padding
    // and are never written.
    //
    // *** IT STAYS A TRIVIAL VALUE TYPE, AND THAT IS A DECISION, NOT AN
    // OVERSIGHT. *** Retail exports five special members for it —
    //     ??0Parameter…@XZ          0x1800f4fa0   default ctor
    //     ??0Parameter…@VString@System@@H_N@Z  0x1800f4f30
    //     ??0Parameter…@AEBU012@@Z  0x18004e150   copy ctor
    //     ??4Parameter…@AEBU012@@Z  0x18004e190   assignment
    //     ??1Parameter…@XZ          0x18004e110   destructor
    // — and DECLARING any of them here would make `Parameter` non-trivially
    // copyable / destructible, which changes `rtcoll::SpecFor<Parameter>`
    // (Collection.h) from `destroy == nullptr` to `&TDestroy<Parameter>`. The
    // compiler-emitted `~Array<Parameter>` would then release each element's
    // `name` String — and the REGISTERED array destructor this class's own
    // destructor calls (0x18004fb00, `J2_ENTRY(…, ReleaseStringsCounted,
    // kD_Str_10)` in `runtime/src/collection/array_destroy.cpp`) already does
    // exactly that. That is a DOUBLE RELEASE of every parameter name, in the
    // shape B31_RULES records. `name` stays a `void*` for the same reason
    // `Video::VertexLayout::Element::name` does.
    //
    // So: only the non-special exported member is declared, and the five above
    // are named here rather than in code. Nothing in the tree calls them.
    // ========================================================
    struct Parameter {
        void*   name;      // +0x00  System::String (refcounted, str[-4] is the count)
        int32_t dest;      // +0x08  the register/sampler index this binds to
        uint8_t internal;  // +0x0c  bool; only serialized when GetVersion() >= 1
        uint8_t _pad0d[3]; // +0x0d  never written by any arm

        // ?Serialize@Parameter@ShaderProgram@Video@@QEAAXAEAVSerializer@IO@@@Z
        // 0x1800f4ed0 — not ported; `ShaderProgram::Serialize` inlines it, which
        // is what retail's 0x1800f5110 does too.
        void Serialize(void* serializer);
    };

    // ---- the eight slots, in measured order -----------------------------
    // Slots 3 (`GetFileVersion`), 6 (`Reconstruct`) and 7 (`Destruct`) are
    // `_purecall` in retail's table and are NOT redeclared: `Object::External`
    // already declares all three `= 0`, so they stay pure and this class stays
    // abstract in retail's sense as well as C++'s. Slot 4 is
    // `Object::External::Compile`, INHERITED (retail's slot 4 is 0x1800066e0,
    // External's own body).
    Object::Base* DeletingDtor(unsigned flags) override = 0;  // [0] 0x18004e360
    const char*   TypeName() const override;                  // [1] 0x18004e0c0
    uint32_t      Type() const override;                      // [2] 0x18004e0a0
    bool          Serialize(void* stream) override;           // [5] 0x1800f5110

    // ---- the exported non-virtual members ------------------------------
    // Every `?…@ShaderProgram@Video@@` in the shipped export table is declared
    // here, ported or not, so no later block has to edit this header.
    bool    IsValid() const;                                     // 0x18004e0f0
    const Collection::Array<uint8_t>& GetByteCode(int32_t index) const;  // 0x18004e100
    int32_t GetTextureCount() const;                             // 0x18004e410
    int32_t GetTextureDest(int32_t index) const;                  // 0x18004e470
    bool    IsTextureInternal(int32_t index) const;               // 0x18004e480
    int32_t GetConstantCount() const;                             // 0x18004e4a0
    int32_t GetConstantDest(int32_t index) const;                 // 0x18004e500

    // The two hidden-return-slot members (B35_RULES 2.5, RULE B5-12). `?AV` in
    // `?AVString@System@@` is the by-VALUE spelling: MSVC hands the caller's
    // storage in RDX and returns it in RAX, and GCC's Microsoft-ABI lowering of
    // a by-value class return produces the same slot and the same RAX — which is
    // the point of spelling it as a real return type here. The analysis is in
    // `runtime/src/video/shader_program_names.cpp`.
    System::String GetTextureName(int32_t index) const;           // 0x18004e420
    System::String GetConstantName(int32_t index) const;          // 0x18004e4b0

    // ?PlatformSerialize@ShaderProgram@Video@@QEAAXAEAVSerializer@IO@@@Z
    // 0x1800f52b0 — PUBLIC (`QEAA`), and called by both subclasses' Serialize.
    void PlatformSerialize(void* serializer);

    static uint32_t TypeId();                                     // 0x18004e0b0

    // `?CreateNewObject@` / `?CreateInPlaceObject@` exist for every external;
    // ShaderProgram's are the ABSTRACT class's and `rt_factories_video.inc`
    // gives Video::ShaderProgram a null step list, so nothing calls them.
    static Object::Base* CreateNewObject();                       // 0x18004e0d0 — not ported
    static Object::Base* CreateInPlaceObject(void* storage);      // 0x18004e0e0 — not ported

    // ??0ShaderProgram@Video@@QEAA@AEBV01@@Z (0x18004e200) and
    // ??4ShaderProgram@Video@@QEAAAEAV01@AEBV01@@Z (0x18004e290) are REAL,
    // EXPORTED bodies, not compiler defaults — the situation RULE B5-10 records
    // for `Graphics::Geometry`. Declared, and deliberately left undefined:
    // nothing in the tree calls either, and `= default` would silently
    // substitute a bitwise copy for whatever they do with the three arrays and
    // the parameter names.
    ShaderProgram(const ShaderProgram& other);                    // 0x18004e200 — not ported
    ShaderProgram& operator=(const ShaderProgram& other);         // 0x18004e290 — not ported

    // ??1ShaderProgram@Video@@UEAA@XZ (0x1800f50b0). NON-virtual (`UEAA` is the
    // mangling of a virtual dtor in retail, where slot 0 reaches it — here slot
    // 0's table row reaches it instead; see
    // reference_phantom_dtor_mangling_qeaa_vs_ueaa.md).
    //
    // PUBLIC, for the reason block A's `Video::XData::~XData()` is public: the
    // REGISTERED DOOR that fills `??1ShaderProgram@Video@@UEAA@XZ` is a free
    // function, not a member or a friend, and it has to spell
    // `->Video::ShaderProgram::~ShaderProgram()`. The same applies below to
    // every member a door calls, including the two static `_*TypeName` reflection
    // companions retail spells `CA` (private static). The one thing that stays
    // inaccessible is the CONSTRUCTOR, which is the property that matters.
    ~ShaderProgram();

    static const char* _TypeName();        // 0x18004e080 -> "ShaderProgram"   (retail: CA, private)
    static const char* _ParentTypeName();  // 0x18004e090 -> "Object::CompiledExternal"

protected:
    // See the "NOT CONSTRUCTIBLE" banner. ??0ShaderProgram@Video@@QEAA@XZ
    // (0x1800f5030) stays a registered ctor DOOR (B4b-2 / B35_RULES 11).
    ShaderProgram() = default;

public:
    // ---- the 0x30 bytes ShaderProgram adds ------------------------------
    // PUBLIC because `dx11/` reads `_byteCode` directly — the same posture
    // `Object::External` takes with `_path` / `_refs`, and what lets
    // `dx11/src/shaderprograms.cpp` drop its `ShaderProgramOwner` view.
    Collection::Array<Parameter>                  _textures;   // +0x20
    Collection::Array<Parameter>                  _constants;  // +0x30
    Collection::Array<Collection::Array<uint8_t>> _byteCode;   // +0x40
};

static_assert(sizeof(ShaderProgram) == 0x50,
              "Video::ShaderProgram is 0x50 (MOV EDX,0x50 at 0x18004e392)");
static_assert(alignof(ShaderProgram) == 8, "Video::ShaderProgram is qword-aligned");
static_assert(offsetof(ShaderProgram, _textures)  == 0x20, "ShaderProgram::_textures @ +0x20");
static_assert(offsetof(ShaderProgram, _constants) == 0x30, "ShaderProgram::_constants @ +0x30");
static_assert(offsetof(ShaderProgram, _byteCode)  == 0x40, "ShaderProgram::_byteCode @ +0x40");

static_assert(sizeof(ShaderProgram::Parameter) == 0x10,
              "ShaderProgram::Parameter is 0x10 (SHL RAX,0x4 at 0x18004e103)");
static_assert(alignof(ShaderProgram::Parameter) == 8, "Parameter is qword-aligned");
static_assert(offsetof(ShaderProgram::Parameter, dest) == 0x08,
              "Parameter::dest @ +0x08 (MOV EAX,[RAX+R8*8+0x8] at 0x18004e47a)");
static_assert(offsetof(ShaderProgram::Parameter, internal) == 0x0c,
              "Parameter::internal @ +0x0c (MOVZX EAX,[RAX+R8*8+0xc] at 0x18004e48a)");
// The triviality the array spec depends on — see the Parameter banner. If this
// ever fails, `~Array<Parameter>` has started releasing the names a second time.
static_assert(std::is_trivially_destructible<ShaderProgram::Parameter>::value,
              "ShaderProgram::Parameter must stay trivially destructible "
              "(rtcoll::SpecFor<Parameter>::destroy must stay null)");
static_assert(std::is_trivially_copyable<ShaderProgram::Parameter>::value,
              "ShaderProgram::Parameter must stay trivially copyable");

// ============================================================
// Video::VertexProgram — the compiled vertex shader. 0x58, 9 slots.
// ============================================================
class VertexProgram : public ShaderProgram {
public:
    // Slot 4 is `Object::External::Compile`, inherited through ShaderProgram.
    Object::Base*     DeletingDtor(unsigned flags) override = 0;  // [0] 0x18004e810
    const char*       TypeName() const override;                  // [1] 0x18004e560
    uint32_t          Type() const override;                      // [2] 0x18004e540
    int32_t           GetFileVersion() const override;            // [3] 0x1800f6230 — `MOV EAX,5`
    bool              Serialize(void* stream) override;           // [5] 0x1800f6240
    Object::External* Reconstruct() override;                     // [6] 0x18004e6e0
    void              Destruct() override;                        // [7] 0x18004e710
    // [8] 0x18004e700. `virtual`, NOT `override`: `Object::External`'s and
    // `CompiledExternal`'s retail tables are eight entries, so this branch
    // introduces `Construct` itself (RULE B5-16).
    virtual Object::External* Construct();

    // ?GetData@VertexProgram@Video@@QEBAPEBVVertexProgramData@2@XZ  0x18004e720
    const void* GetData() const;
    // ?Bind@VertexProgram@Video@@QEBAXAEAVContext@2@H@Z  0x1800f6660
    void Bind(void* context, int32_t permutation) const;

    static uint32_t      TypeId();                                // 0x18004e550
    static Object::Base* CreateNewObject();                       // 0x18004e650 — rt_factories_video.inc
    static Object::Base* CreateInPlaceObject(void* storage);      // 0x18004e6d0 — rt_factories_video.inc
    static void*         operator new(size_t size);               // 0x18004e570 — not ported
    static void*         operator new(size_t size, void* storage);// 0x18004e5e0 — not ported
    static void          operator delete(void* p);                // 0x18004e5f0 — class_operator_delete.cpp

    // Real exported bodies, not compiler defaults; see ShaderProgram's pair.
    VertexProgram(const VertexProgram& other);                    // 0x18004e730 — not ported
    VertexProgram& operator=(const VertexProgram& other);         // 0x18004e780 — not ported

    ~VertexProgram();            // ??1VertexProgram@Video@@UEAA@XZ 0x1800f61b0

    // The three platform halves. `AEAA` is PRIVATE IN RETAIL, not unexported
    // (reference_aeaa_is_private_not_unexported.md) — so each has its own
    // registered door, which is why they are public here (see ~ShaderProgram).
    void PlatformConstruct();    // 0x1800f65e0
    void PlatformDestruct();     // 0x1800f6620
    bool PlatformCreate();       // 0x1800f6640 — returns the companion's slot 1

    static const char* _TypeName();        // 0x18004e520 -> "VertexProgram"
    static const char* _ParentTypeName();  // 0x18004e530 -> "ShaderProgram"
    static const char* _FileExtension();   // 0x18004e510 -> "srsl"

protected:
    VertexProgram() = default;   // ??0VertexProgram…@XZ 0x1800f6100 stays a DOOR

public:
    void* _data;   // +0x50  Video::VertexProgramData*, from DeviceData slot +0x78
};

static_assert(sizeof(VertexProgram) == 0x58,
              "Video::VertexProgram is 0x58 (MOV EDX,0x58 at 0x18004e82f)");
static_assert(alignof(VertexProgram) == 8, "Video::VertexProgram is qword-aligned");
static_assert(offsetof(VertexProgram, _data) == 0x50,
              "VertexProgram::_data @ +0x50 (MOV RAX,[RCX+0x50] — ?GetData@VertexProgram@Video@@)");

// ============================================================
// Video::PixelProgram — the compiled pixel shader. 0x58, 9 slots. Same layout
// as VertexProgram, different vtable / typeinfo / DeviceData slot (+0x80).
// ============================================================
class PixelProgram : public ShaderProgram {
public:
    Object::Base*     DeletingDtor(unsigned flags) override = 0;  // [0] 0x18004eb90
    const char*       TypeName() const override;                  // [1] 0x18004e8e0
    uint32_t          Type() const override;                      // [2] 0x18004e8c0
    int32_t           GetFileVersion() const override;            // [3] 0x1800f3a80 — `MOV EAX,5`
    bool              Serialize(void* stream) override;           // [5] 0x1800f3a90
    Object::External* Reconstruct() override;                     // [6] 0x18004ea60
    void              Destruct() override;                        // [7] 0x18004ea90
    virtual Object::External* Construct();                        // [8] 0x18004ea80 (RULE B5-16)

    // ?GetData@PixelProgram@Video@@QEBAPEBVPixelProgramData@2@XZ  0x18004eaa0
    const void* GetData() const;
    // ?Bind@PixelProgram@Video@@QEBAXAEAVContext@2@H@Z  0x1800f3ed0
    void Bind(void* context, int32_t permutation) const;

    static uint32_t      TypeId();                                // 0x18004e8d0
    static Object::Base* CreateNewObject();                       // 0x18004e9d0 — rt_factories_video.inc
    static Object::Base* CreateInPlaceObject(void* storage);      // 0x18004ea50 — rt_factories_video.inc
    static void*         operator new(size_t size);               // 0x18004e8f0 — not ported
    static void*         operator new(size_t size, void* storage);// 0x18004e960 — not ported
    static void          operator delete(void* p);                // 0x18004e970 — class_operator_delete.cpp

    PixelProgram(const PixelProgram& other);                      // 0x18004eab0 — not ported
    PixelProgram& operator=(const PixelProgram& other);           // 0x18004eb00 — not ported

    ~PixelProgram();             // ??1PixelProgram@Video@@UEAA@XZ 0x1800f3a00

    void PlatformConstruct();    // 0x1800f3e40
    void PlatformDestruct();     // 0x1800f3e90
    bool PlatformCreate();       // 0x1800f3eb0

    static const char* _TypeName();        // 0x18004e8a0 -> "PixelProgram"
    static const char* _ParentTypeName();  // 0x18004e8b0 -> "ShaderProgram"
    static const char* _FileExtension();   // 0x18004e890 -> "srsl"

protected:
    PixelProgram() = default;    // ??0PixelProgram…@XZ 0x1800f3950 stays a DOOR

public:
    void* _data;   // +0x50  Video::PixelProgramData*, from DeviceData slot +0x80
};

static_assert(sizeof(PixelProgram) == 0x58,
              "Video::PixelProgram is 0x58 (MOV EDX,0x58 at 0x18004ebaf)");
static_assert(alignof(PixelProgram) == 8, "Video::PixelProgram is qword-aligned");
static_assert(offsetof(PixelProgram, _data) == 0x50,
              "PixelProgram::_data @ +0x50 (MOV RAX,[RCX+0x50] — ?GetData@PixelProgram@Video@@)");

}  // namespace Video

// ============================================================================
// The `rt::video` spellings this header used to DECLARE are now ALIASES of the
// real classes — ONE definition of the layout, not two (BLOCK_BRIEF "the
// shape": "where class and POD are field-identical prefer ONE definition and an
// alias over a duplicate layout"). B4b-1 does not fire: `rt::video` is a sibling
// of `Video`, so the POD never occupied the class's own name.
//
// `ShaderProgramWithData` is GONE rather than aliased: it was ONE struct
// standing for two classes, and there is no single class it can name now.
// Nothing outside this header ever used the name.
//
// The two vtable-offset namespaces below are UNCHANGED and are not aliases.
// `runtime/src/video/renderstate.cpp` (block C's file) includes this header for
// exactly those two, and step 5 does not move them.
// ============================================================================
namespace rt {
namespace video {

using ShaderParameter = ::Video::ShaderProgram::Parameter;
using ShaderProgram   = ::Video::ShaderProgram;
using VertexProgram   = ::Video::VertexProgram;
using PixelProgram    = ::Video::PixelProgram;

// DeviceData vtable slots that make the platform companion. Same object, two
// different slots; getting these two swapped produces a PixelProgramData behind
// a VertexProgram's vtable, which fails only on the first Bind.
namespace devicedata_vtbl {
    constexpr size_t kCreateVertexProgramData = 0x78;  // CALL [RAX+0x78] @0x1800f6192
    constexpr size_t kCreatePixelProgramData  = 0x80;  // CALL [RAX+0x80] @0x1800f39e2
    constexpr size_t kCreateRenderStateData   = 0x88;  // CALL [RAX+0x88] @0x1800f4c0d
}

// The companion's own vtable, shared by all three *Data classes. The typed
// declaration of these three slots is `Video::{Vertex,Pixel}ProgramData` in
// runtime/types/video/CompanionData.h (block A); these constants stay because
// the receivers are still `void*` on the paths that use them.
//   slot 0 (+0x00) deleting destructor(flags)
//   slot 1 (+0x08) Create / Rebuild
//   slot 2 (+0x10) Bind
namespace programdata_vtbl {
    constexpr size_t kDeletingDtor = 0x00;
    constexpr size_t kCreate       = 0x08;
    constexpr size_t kBind         = 0x10;
}

} // namespace video
} // namespace rt
