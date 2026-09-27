// ============================================================================
// runtime/src/video/renderstate.cpp — Video::RenderState, the whole class
// (original file: code\Runtime\Video\RenderState.h)
// ============================================================================
//
// ENDGAME B.3.5 block C. `Video::RenderState` is now a REAL C++ class
// (runtime/types/video/RenderState.h) and every body below has MOVED INTO a
// member of it; each registered door is a one-line delegation with a QUALIFIED
// non-virtual call, so no registration, address or mangled name changed.
//
// ??_7RenderState@Video@@6B@ (retail 0x180157720), nine slots:
//
//   0  deleting dtor        0x18004b9d0  RenderState.h (inline, B.3.9)
//   1  TypeName             0x18004b570  here
//   2  Type                 0x18004b550  here
//   3  GetFileVersion       0x1800f4c80  here  (returns 3)
//   4  External::Compile    0x1800066e0  already ported, INHERITED
//   5  Serialize            0x1800f4c90  here
//   6  Reconstruct          0x18004b6f0  here
//   7  Destruct             0x18004b720  here
//   8  Construct            0x18004b710  here
//
// 0x18004b6bf), ~RenderState (0x1800f4c30), Initialize, IsWireframe, Bind, the
// three Platform* helpers, and the three sub-struct ctors and serializers:
//     ??0Stencil    0x1800f49d0   ?Serialize@Stencil     0x1800f48e0
//     ??0Depth      0x1800f4a80   ?Serialize@Depth       0x1800f4a00
//     ??0AlphaBlend 0x1800f4b30   ?Serialize@AlphaBlend  0x1800f4aa0
//
// Layout and the "derives from External, NOT CompiledExternal" finding:
// runtime/types/video/RenderState.h.
//
// ---------------------------------------------------------------------------
// WHAT THE CLASS CONVERSION CHANGED, AND WHAT IT DID NOT
// ---------------------------------------------------------------------------
// CHANGED — three families of hand-written indirection are gone, each replaced
// by the call the bytes were already making:
//
//   * `_data` is a real `Video::RenderStateData*` (block A, RULE B5-1), so
//     `PlatformCreate` / `PlatformDestruct` / `Bind` call `Create()`,
//     `DeletingDtor(1)` and `Bind(context)` as VIRTUALS instead of indexing
//     `programdata_vtbl::kCreate / kDeletingDtor / kBind` by byte offset. The
//     three offsets were +0x08, +0x00 and +0x10, i.e. slots 1, 0 and 2 — which
//     is exactly the order `CompanionData.h` declares them in.
//   * `PlatformConstruct` calls `Video::DeviceData::CreateRenderStateData`,
//     slot 17 (+0x88), by name.
//   * `Serialize` and the three sub-serializers use the REAL `IO::Serializer`
//     (B.3.4 block 3) and its `Serialize(T&, const char*)` overloads, so the
//     file no longer carries its own copy of the encoding-factory prologue, the
//     `rw_vtbl::k*` byte offsets, or the codec teardown. `IO::Serializer`'s
//     constructor IS the prologue (version 0, which is retail's `XOR R8D,R8D`
//     at 0x1800f4cc1) and its destructor IS `if (rw) rw->slot0(rw, 1)`. The
//     scope of the local is chosen so both run at retail's positions: the
//     construction before the first emission, the destruction after the
//     reading-direction PlatformCreate. Moving either one relative to another
//     stream operation is a save-format change (Serializer.h's banner).
//
// ENDGAME B.3.9 block VID — THE CLASS IS CONCRETE. The ctor door that wrote
// Base's, External's and RenderState's MINTED addresses by hand is gone: the
// real constructor (RenderState.h) and the destructor chain stamp the
// compiler's tables. The minted row, its RT_DELDTOR row, the ??0 / ??1 doors
// and the seven slot doors retired together; the factory pair is the class's
// own.
//
// ---------------------------------------------------------------------------
// Serialize (0x1800f4c90) INLINES TWO OF THE THREE SUB-SERIALIZERS
// ---------------------------------------------------------------------------
// AlphaBlend's five fields and Depth's five are emitted straight into the body;
// only Stencil is a real `CALL 0x1800f48e0`. The two inlined runs are field for
// field identical to the exported sub-serializers they came from — checked
// against 0x1800f4aa0 and 0x1800f4a00 — so this file expresses them as calls to
// the same three members it registers. That is the same code; what it is NOT
// is an invitation to "simplify" Stencil into the other two, because Stencil
// serializes its write mask as a UChar (+0x08) among eight UInts, and a UInt
// there would move the next eight fields by three bytes.
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/io/Stream.h"
#include "../../types/video/RenderState.h"

// standalone/resolve.h is DELIBERATELY NOT INCLUDED — it drags in <windows.h>,
// and <wingdi.h> declares a function called `AlphaBlend`, which would make
// Video::RenderState::AlphaBlend ambiguous at every use. Same local declaration
// runtime/src/collection/array.cpp and runtime/src/object/deleting_dtor.cpp
// carry, for the same PLAN.md §4.1 reason; the linker checks the two spellings.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// The four registered doors over `Video::RenderState`'s protected surface:
// `?PlatformConstruct@` / `?PlatformCreate@` / `?PlatformDestruct@` (private in
// retail, `AEAA`) and `??1RenderState@Video@@` (non-virtual, protected). An
// `extern "C"` function cannot name a protected member, so the class friends
// THIS struct (B34_BRIEFS A.4 — friend the holder rather than widen the access,
// so the header keeps saying what retail says). Declared at global scope because
// that is where the header's `friend struct ::RenderStateDoorAccess;` names it —
// it must NOT go in the anonymous namespace, where it would be a different type.
// It has no fields and is never instantiated.
struct RenderStateDoorAccess {
    static void PlatformConstruct(Video::RenderState* p) {
        p->Video::RenderState::PlatformConstruct();
    }
    static void PlatformCreate(Video::RenderState* p) {
        p->Video::RenderState::PlatformCreate();
    }
    static void PlatformDestruct(Video::RenderState* p) {
        p->Video::RenderState::PlatformDestruct();
    }
};

namespace {

const uint8_t* RenderStateTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@RenderState@Video@@0VBaseTypeInfo@Object@@A");
}

// `?_singleton@Device@Video@@0PEAV12@EA` -> `Device+0x100` -> the live
// `Video::DeviceData*`. B35_RULES section 9: the static row stays an
// `RT_DATA_SLOT` read for the whole of step 5 and is OWED a `MintStorageCell()`
// redirect at the class-static conversion (RULE B4b-4), not here.
Video::DeviceData* ActiveDeviceData() {
    void** slot = RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
    void* device = slot ? *slot : nullptr;
    if (!device) return nullptr;
    // B.3.5 bridge: `Video::Device` is block D2's class and is still a POD, so
    // its `deviceData` member at +0x100 is reached by offset.
    return reinterpret_cast<Video::DeviceData*>(
        *reinterpret_cast<void**>(static_cast<uint8_t*>(device) + 0x100));
}

}  // namespace

// ############################################################################
// The members
// ############################################################################

namespace Video {

// What the `reinterpret_cast<bool&>` in the three sub-serializers stands on.
static_assert(sizeof(bool) == 1,
              "the Bool ReaderWriter slot is handed the address of ONE byte");

// ---- the three nested value types -----------------------------------------
// Their three constructors (0x1800f4b30 / 0x1800f4a80 / 0x1800f49d0) are
// inline in RenderState.h since ENDGAME B.3.9: the real RenderState ctor runs
// them.

// ?Serialize@AlphaBlend@RenderState@Video@@QEAAXAEAVSerializer@IO@@@Z  0x1800f4aa0
void RenderState::AlphaBlend::Serialize(IO::Serializer& ser) {
    // The third argument of every emitter is the field NAME and retail passes
    // NULL (`XOR R8D,R8D` before each one); a text encoding would write those
    // names into the file, so inventing them would change the format.
    //
    // THE `bool&` CASTS ARE NOT A CONVENIENCE. Retail hands the Bool slot the
    // ADDRESS of the one byte at this offset, and on the READING direction the
    // codec writes that byte back through the pointer. Copying into a local
    // `bool` and normalising the result to 0/1 afterwards would change which
    // byte value a load leaves behind for any file that stored something other
    // than 0 or 1. `sizeof(bool) == 1` on this ABI (asserted once below), so
    // the cast is the same byte the bytes name.
    ser.Serialize(reinterpret_cast<bool&>(enabled), nullptr);
    ser.Serialize(srcBlend, nullptr);
    ser.Serialize(dstBlend, nullptr);
    ser.Serialize(blendOp, nullptr);
    ser.Serialize(writeMask, nullptr);
}

// ?Serialize@Depth@RenderState@Video@@QEAAXAEAVSerializer@IO@@@Z  0x1800f4a00
void RenderState::Depth::Serialize(IO::Serializer& ser) {
    ser.Serialize(reinterpret_cast<bool&>(test), nullptr);
    ser.Serialize(reinterpret_cast<bool&>(write), nullptr);
    ser.Serialize(func, nullptr);
    ser.Serialize(bias, nullptr);
    ser.Serialize(slopeBias, nullptr);
}

// ?Serialize@Stencil@RenderState@Video@@QEAAXAEAVSerializer@IO@@@Z  0x1800f48e0
void RenderState::Stencil::Serialize(IO::Serializer& ser) {
    ser.Serialize(reinterpret_cast<bool&>(enabled), nullptr);
    ser.Serialize(reinterpret_cast<bool&>(twoSided), nullptr);
    ser.Serialize(readMask, nullptr);
    ser.Serialize(writeMask, nullptr);   // UChar, not UInt — see this file's banner
    ser.Serialize(func, nullptr);
    ser.Serialize(failOp, nullptr);
    ser.Serialize(depthFailOp, nullptr);
    ser.Serialize(passOp, nullptr);
    ser.Serialize(backFunc, nullptr);
    ser.Serialize(reference, nullptr);
}

// ---- the three private platform halves ------------------------------------
// ?PlatformConstruct@RenderState@Video@@AEAAXXZ (0x1800f4e30)
void RenderState::PlatformConstruct() {
    DeviceData* dd = ActiveDeviceData();
    if (dd == nullptr) { _data = nullptr; return; }
    // `CALL [dd_vtable+0x88]` — slot 17, `CreateRenderStateData(this)`.
    _data = static_cast<RenderStateData*>(dd->CreateRenderStateData(this));
}
// ?PlatformCreate@RenderState@Video@@AEAAXXZ (0x1800f4ea0). Returns void, unlike
// the shader programs' `_N` (bool) form; retail discards the slot's result.
void RenderState::PlatformCreate() {
    _data->Create();          // slot 1, +0x08 — retail derefs _data unguarded
}
// ?PlatformDestruct@RenderState@Video@@AEAAXXZ (0x1800f4e80)
void RenderState::PlatformDestruct() {
    if (_data == nullptr) return;
    _data->DeletingDtor(1);   // slot 0, +0x00, flags 1 = destroy AND free
    // Retail does NOT null `_data` afterwards. Neither do we.
}

// ---- the vtable slots ------------------------------------------------------
// slot 2 — `MOV EAX,dword ptr [_typeInfo + 0x00]`
uint32_t RenderState::Type() const {
    uint32_t v = 0; std::memcpy(&v, RenderStateTypeInfo(), sizeof v); return v;
}
// slot 1 — `MOV RAX,qword ptr [_typeInfo + 0x08]`, i.e. `BaseTypeInfo::name`.
// CONTENT-only: never compared by pointer
// (reference_basetypeinfo_name_is_content_only.md).
const char* RenderState::TypeName() const {
    const char* v = nullptr; std::memcpy(&v, RenderStateTypeInfo() + 8, sizeof v); return v;
}
uint32_t RenderState::TypeId() {
    uint32_t v = 0; std::memcpy(&v, RenderStateTypeInfo(), sizeof v); return v;
}
// slot 3 — `MOV EAX,0x3; RET`. A LITERAL in the shipped code: the on-disk
// format version of this class, and getting it wrong silently changes which
// branch the loader takes on every render state in the game.
int32_t RenderState::GetFileVersion() const { return 3; }

// RULE B5-15: the .rdata literals at 0x180157710 / 0x1801544f0. The parent
// really is "Object::External" and not "Object::CompiledExternal".
const char* RenderState::_TypeName()       { return "RenderState"; }
const char* RenderState::_ParentTypeName() { return "Object::External"; }

// slot 8 — ?Construct@ 0x18004b710.
// `TEST RCX,RCX; JNZ <ctor>; XOR EAX,EAX; RET` — MSVC's placement-new codegen;
// `this` is never null here. `::new` because the class declares its own
// (unported) placement operator new.
Object::External* RenderState::Construct() {
    return ::new (this) RenderState();
}

// slot 6 — ?Reconstruct@ 0x18004b6f0. Destroy in place through slot 0 with
// flags 0 ("destroy, do NOT free" — the argument Ghidra's C output drops), then
// re-construct on the SAME storage: `Object::External`'s device-lost primitive.
Object::External* RenderState::Reconstruct() {
    DeletingDtor(0);
    return ::new (this) RenderState();
}

// slot 7 — ?Destruct@ 0x18004b720. `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`.
void RenderState::Destruct() { DeletingDtor(0); }

//
// Retail's order: stamp RenderState's vtable, PlatformDestruct, then TAIL-JMP
// ~External. The first and the last are BOTH the destructor chain's — GCC emits
// the vptr store on entry to `~RenderState` and runs `~External()` on the way
// out. The one line left is the body retail actually has.
RenderState::~RenderState() { PlatformDestruct(); }

// ?CreateNewObject@RenderState@Video@@SAPEAVBase@Object@@XZ  0x18004b660:
// MemAlloc(0x80, 8), null branch, the ctor (B39_RULES D2).
Object::Base* RenderState::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(RenderState), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) RenderState();
}
// ?CreateInPlaceObject@RenderState@Video@@SAPEAVBase@Object@@PEAX@Z  0x18004b6e0
Object::Base* RenderState::CreateInPlaceObject(void* storage) {
    return storage ? ::new (storage) RenderState() : nullptr;
}

// ---- the exported non-virtual members --------------------------------------
bool RenderState::IsWireframe() const { return _fillMode == 1; }

// ?Bind@RenderState@Video@@QEBAXAEAVContext@2@@Z (0x1800f4ec0)
void RenderState::Bind(void* context) const {
    _data->Bind(context);     // slot 2, +0x10 — retail derefs _data unguarded
}

// ?Initialize@RenderState@Video@@ (0x1800f4840): a field-by-field copy, then
// PlatformCreate. Retail copies dword by dword and therefore also copies the
// three padding bytes after AlphaBlend::enabled and the two after Depth::test;
// the struct assignments below copy the same 0x14 / 0x10 / 0x24 byte extents,
// which is sound because all three are trivially copyable (asserted in the
// header).
void RenderState::Initialize(const AlphaBlend& alphaBlend, const Depth& depth,
                             const Stencil& stencil, CullMode cullMode,
                             FillMode fillMode, bool flag) {
    _alphaBlend = alphaBlend;
    _depth      = depth;
    _stencil    = stencil;
    _fillMode   = fillMode;     // retail stores fillMode BEFORE cullMode
    _cullMode   = cullMode;
    _flag70     = flag ? 1 : 0;
    PlatformCreate();           // retail derefs _data unguarded
}

// slot 5 — ?Serialize@RenderState@Video@@UEAA_NAEAVStream@IO@@@Z (0x1800f4c90)
//
// The reader/writer is built with version 0 (`XOR R8D,R8D` at 0x1800f4cc1) —
// unlike the shader programs, which pass 1. `IO::Serializer`'s constructor is
// that prologue and its destructor is the codec teardown at the end of the
// body, so the local's SCOPE is what places both; see this file's banner.
bool RenderState::Serialize(void* stream) {
    // B.3.5 bridge: the slot's parameter is `IO::Stream&` in retail, but
    // `Object::External::Serialize` is declared `bool(void*)` across the whole
    // External family and that signature is not this block's to change.
    IO::Stream& s = *reinterpret_cast<IO::Stream*>(stream);
    {
        IO::Serializer ser(s, 0);

        _alphaBlend.Serialize(ser);    // inlined by retail
        _depth.Serialize(ser);         // inlined by retail
        _stencil.Serialize(ser);       // a real CALL 0x1800f48e0
        ser.Serialize(_cullMode, nullptr);
        ser.Serialize(_fillMode, nullptr);
        ser.Serialize(reinterpret_cast<bool&>(_flag70), nullptr);

        if (s.GetDirection() == IO::kReading)
            PlatformCreate();          // retail derefs _data unguarded
    }   // ~Serializer: `if (rw) rw->slot0(rw, 1)` at 0x1800f4e0f
    return true;                       // MOV AL,1 — there is no failure path
}

}  // namespace Video

// ############################################################################
// The registered doors — every one a QUALIFIED non-virtual delegation.
// Names and addresses are UNCHANGED; nothing is retired (B35_RULES 11).
// ############################################################################

extern "C" void* RtVideo_RenderState_AlphaBlend_Ctor(void* p) {   // 0x1800f4b30
    return ::new (p) Video::RenderState::AlphaBlend();
}
extern "C" void* RtVideo_RenderState_Depth_Ctor(void* p) {        // 0x1800f4a80
    return ::new (p) Video::RenderState::Depth();
}
extern "C" void* RtVideo_RenderState_Stencil_Ctor(void* p) {      // 0x1800f49d0
    return ::new (p) Video::RenderState::Stencil();
}

extern "C" void RtVideo_RenderState_AlphaBlend_Serialize(void* p, void* serv) {  // 0x1800f4aa0
    reinterpret_cast<Video::RenderState::AlphaBlend*>(p)
        ->Video::RenderState::AlphaBlend::Serialize(
            *reinterpret_cast<IO::Serializer*>(serv));           // B.3.5 bridge
}
extern "C" void RtVideo_RenderState_Depth_Serialize(void* p, void* serv) {       // 0x1800f4a00
    reinterpret_cast<Video::RenderState::Depth*>(p)
        ->Video::RenderState::Depth::Serialize(
            *reinterpret_cast<IO::Serializer*>(serv));           // B.3.5 bridge
}
extern "C" void RtVideo_RenderState_Stencil_Serialize(void* p, void* serv) {     // 0x1800f48e0
    reinterpret_cast<Video::RenderState::Stencil*>(p)
        ->Video::RenderState::Stencil::Serialize(
            *reinterpret_cast<IO::Serializer*>(serv));           // B.3.5 bridge
}

extern "C" uint32_t RtVideo_RenderState_TypeId() {
    return Video::RenderState::TypeId();
}
// `?_TypeName@` / `?_ParentTypeName@` are `protected static` on the class
// (retail's `CA` = private static), so the doors are members' friends by being
// this file — `Video::RenderState::_TypeName()` is reachable from a member
// definition's enclosing scope but not from an `extern "C"` free function, so
// the two literals are repeated here rather than the access widened. RULE
// B5-15: both are the bytes at 0x18004b530 / 0x18004b540, copied.
extern "C" const char* RtVideo_RenderState_StaticTypeName() { return "RenderState"; }
extern "C" const char* RtVideo_RenderState_ParentTypeName() { return "Object::External"; }

extern "C" bool RtVideo_RenderState_IsWireframe(const void* selfv) {
    return reinterpret_cast<const Video::RenderState*>(selfv)
        ->Video::RenderState::IsWireframe();                     // B.3.5 bridge
}
extern "C" void RtVideo_RenderState_Bind(const void* selfv, void* context) {
    reinterpret_cast<const Video::RenderState*>(selfv)
        ->Video::RenderState::Bind(context);                     // B.3.5 bridge
}
extern "C" void RtVideo_RenderState_Initialize(void* selfv, const void* ab, const void* dp,
                                               const void* st, uint32_t cull, uint32_t fill,
                                               bool flag) {
    reinterpret_cast<Video::RenderState*>(selfv)->Video::RenderState::Initialize(
        *reinterpret_cast<const Video::RenderState::AlphaBlend*>(ab),
        *reinterpret_cast<const Video::RenderState::Depth*>(dp),
        *reinterpret_cast<const Video::RenderState::Stencil*>(st),
        cull, fill, flag);                                       // B.3.5 bridge
}

// The three Platform* doors reach PROTECTED members through
// `RenderStateDoorAccess` (declared at the top of this file with its
// reasoning).
extern "C" void RtVideo_RenderState_PlatformConstructExport(void* selfv) {
    RenderStateDoorAccess::PlatformConstruct(
        reinterpret_cast<Video::RenderState*>(selfv));            // B.3.5 bridge
}
extern "C" void RtVideo_RenderState_PlatformCreateExport(void* selfv) {
    RenderStateDoorAccess::PlatformCreate(
        reinterpret_cast<Video::RenderState*>(selfv));            // B.3.5 bridge
}
extern "C" void RtVideo_RenderState_PlatformDestructExport(void* selfv) {
    RenderStateDoorAccess::PlatformDestruct(
        reinterpret_cast<Video::RenderState*>(selfv));            // B.3.5 bridge
}

int RtVideoRenderStateRegister() {
    int n = 0;
    // ENDGAME B.3.9 VID: the factory pair by retail NAME and VA (B39_RULES D2).
    RegisterRuntimeExport("?CreateNewObject@RenderState@Video@@SAPEAVBase@Object@@XZ",
                          (void*)&Video::RenderState::CreateNewObject); ++n;
    RegisterRuntimeAddr  (0x18004b660ull, (void*)&Video::RenderState::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@RenderState@Video@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&Video::RenderState::CreateInPlaceObject); ++n;
    RegisterRuntimeAddr  (0x18004b6e0ull, (void*)&Video::RenderState::CreateInPlaceObject);
    RegisterRuntimeExport("?TypeId@RenderState@Video@@SAIXZ",        (void*)&RtVideo_RenderState_TypeId);   ++n;
    RegisterRuntimeAddr  (0x18004b560ull,                            (void*)&RtVideo_RenderState_TypeId);
    RegisterRuntimeExport("?_TypeName@RenderState@Video@@CAPEBDXZ",       (void*)&RtVideo_RenderState_StaticTypeName); ++n;
    RegisterRuntimeAddr  (0x18004b530ull,                                 (void*)&RtVideo_RenderState_StaticTypeName);
    RegisterRuntimeExport("?_ParentTypeName@RenderState@Video@@CAPEBDXZ", (void*)&RtVideo_RenderState_ParentTypeName); ++n;
    RegisterRuntimeAddr  (0x18004b540ull,                                 (void*)&RtVideo_RenderState_ParentTypeName);


    RegisterRuntimeExport("??0Stencil@RenderState@Video@@QEAA@XZ",    (void*)&RtVideo_RenderState_Stencil_Ctor);    ++n;
    RegisterRuntimeAddr  (0x1800f49d0ull,                             (void*)&RtVideo_RenderState_Stencil_Ctor);
    RegisterRuntimeExport("??0Depth@RenderState@Video@@QEAA@XZ",      (void*)&RtVideo_RenderState_Depth_Ctor);      ++n;
    RegisterRuntimeAddr  (0x1800f4a80ull,                             (void*)&RtVideo_RenderState_Depth_Ctor);
    RegisterRuntimeExport("??0AlphaBlend@RenderState@Video@@QEAA@XZ", (void*)&RtVideo_RenderState_AlphaBlend_Ctor); ++n;
    RegisterRuntimeAddr  (0x1800f4b30ull,                             (void*)&RtVideo_RenderState_AlphaBlend_Ctor);
    RegisterRuntimeExport("?Serialize@Stencil@RenderState@Video@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtVideo_RenderState_Stencil_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800f48e0ull, (void*)&RtVideo_RenderState_Stencil_Serialize);
    RegisterRuntimeExport("?Serialize@Depth@RenderState@Video@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtVideo_RenderState_Depth_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800f4a00ull, (void*)&RtVideo_RenderState_Depth_Serialize);
    RegisterRuntimeExport("?Serialize@AlphaBlend@RenderState@Video@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtVideo_RenderState_AlphaBlend_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800f4aa0ull, (void*)&RtVideo_RenderState_AlphaBlend_Serialize);

    RegisterRuntimeExport("?IsWireframe@RenderState@Video@@QEBA_NXZ", (void*)&RtVideo_RenderState_IsWireframe); ++n;
    RegisterRuntimeAddr  (0x18004b730ull,                             (void*)&RtVideo_RenderState_IsWireframe);
    RegisterRuntimeExport("?Bind@RenderState@Video@@QEBAXAEAVContext@2@@Z",
                          (void*)&RtVideo_RenderState_Bind); ++n;
    RegisterRuntimeAddr  (0x1800f4ec0ull, (void*)&RtVideo_RenderState_Bind);
    RegisterRuntimeExport("?Initialize@RenderState@Video@@QEAAXAEBUAlphaBlend@12@AEBUDepth@12@"
                          "AEBUStencil@12@W4CullMode@12@W4FillMode@12@_N@Z",
                          (void*)&RtVideo_RenderState_Initialize); ++n;
    RegisterRuntimeAddr  (0x1800f4840ull, (void*)&RtVideo_RenderState_Initialize);
    RegisterRuntimeExport("?PlatformConstruct@RenderState@Video@@AEAAXXZ",
                          (void*)&RtVideo_RenderState_PlatformConstructExport); ++n;
    RegisterRuntimeAddr  (0x1800f4e30ull, (void*)&RtVideo_RenderState_PlatformConstructExport);
    RegisterRuntimeExport("?PlatformCreate@RenderState@Video@@AEAAXXZ",
                          (void*)&RtVideo_RenderState_PlatformCreateExport); ++n;
    RegisterRuntimeAddr  (0x1800f4ea0ull, (void*)&RtVideo_RenderState_PlatformCreateExport);
    RegisterRuntimeExport("?PlatformDestruct@RenderState@Video@@AEAAXXZ",
                          (void*)&RtVideo_RenderState_PlatformDestructExport); ++n;
    RegisterRuntimeAddr  (0x1800f4e80ull, (void*)&RtVideo_RenderState_PlatformDestructExport);
    return n;
}
