// ============================================================================
// runtime/src/video/shaderprogram.cpp — Video::ShaderProgram, the whole class
// (original file: code\Runtime\Video\ShaderProgram_win.cpp)
// ============================================================================
//
// ??_7ShaderProgram@Video@@6B@ (retail 0x180157b60) — EIGHT slots, not nine.
// ShaderProgram is ABSTRACT: three of its slots are `_purecall`
// (0x18007ec28), and it has no Construct slot at all, which is why its row is
// two shorter than VertexLayout's and why rt_factories_video.inc gives
// Video::ShaderProgram a null step list.
//
//   0  deleting dtor        0x18004e360  runtime/src/object/deleting_dtor.cpp
//   1  TypeName             0x18004e0c0  here
//   2  Type                 0x18004e0a0  here
//   3  _purecall            0x18007ec28  (GetFileVersion — subclass supplies it)
//   4  External::Compile    0x1800066e0  already ported
//   5  Serialize            0x1800f5110  here
//   6  _purecall            0x18007ec28  (Reconstruct)
//   7  _purecall            0x18007ec28  (Destruct)
//
// plus ??0ShaderProgram@Video@@QEAA@XZ (0x1800f5030), ~ShaderProgram
// (0x1800f50b0), ?PlatformSerialize@ShaderProgram@Video@@ (0x1800f52b0) and the
// nine non-virtual accessors the export table names.
//
// Layout: runtime/types/video/ShaderProgram.h (sizeof 0x50).
//
// ============================================================================
// ENDGAME B.3.5 BLOCK B — THE BODIES MOVED INTO THE MEMBERS.
// ============================================================================
// `Video::ShaderProgram` is a real C++ class (runtime/types/video/ShaderProgram.h)
// deriving from the real `Object::CompiledExternal`. Every body below that was a
// `RtVideo_ShaderProgram_*` function is now a member of that class, defined in
// the `namespace Video` block at the bottom of this file (B34_BRIEFS A.2: a
// member of `Video::X` cannot be defined inside a sibling namespace), and each
// registered DOOR is a one-line QUALIFIED non-virtual delegation through a
// `reinterpret_cast` bridge (`// B.3.5 bridge`, which is what the B.4 metric
// counts).
//
// WHY THE QUALIFICATION IS LOAD-BEARING (B34_BRIEFS A.1): a live ShaderProgram
// subclass wears the MINTED `??_7VertexProgram@Video@@6B@` / `??_7PixelProgram@`
// table, whose slot 5 resolves BACK to these doors. An unqualified virtual call
// in a door would dispatch through the minted table into the door itself —
// infinite recursion, not delegation.
//
// NOTHING IS UNREGISTERED OR RETIRED. Every `RegisterRuntimeExport` /
// `RegisterRuntimeAddr` line is byte-for-byte the one that was here; the ctor
// door stays a ctor door (B4b-2: no real constructor on a polymorphic class, and
// no constructor can write the MINTED vtable address the rest of the tree reads);
// the `RT_DELDTOR` row at deleting_dtor.cpp:843 and the `??_7ShaderProgram` mint
// row stay for the whole of step 5 (B35_RULES 11).
//
// TWO BINDINGS RETIRED, both runtime-internal and both inside this block's file
// set (BLOCK_BRIEF "runtime-internal callers"):
//   * `RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(self)` — the
//     base-destructor call is now the destructor CHAIN's, which is what retail's
//     tail-JMP to 0x1800bc950 is. Spelling 3 of the nine.
//   * `RtVideo_ShaderProgram_DestroyArrays` — a cross-file `extern "C"` that
//     vertexprogram.cpp and pixelprogram.cpp called to run ~ShaderProgram's
//     array teardown by hand. The teardown is inside `~ShaderProgram()` now and
//     the subclass destructors reach it as their base-destructor step, exactly
//     as retail's inlined copies do. The symbol is GONE.
//
// WHAT THE DESTRUCTOR DOES AND DOES NOT WRITE BY HAND. Retail's ~ShaderProgram
// is: stamp ??_7ShaderProgram, destroy the three arrays newest-first, stamp
// ??_7CompiledExternal, tail-JMP ~External. The FIRST stamp is written out by
// hand below, naming the MINTED address, for block A's reason: GCC emits its own
// vptr store at the top of a polymorphic destructor and it names GCC's table,
// while the rest of the tree and the mint rows expect the minted one. The LAST
// two are the destructor chain's — `~CompiledExternal()` and `~External()` are
// real members (runtime/src/object/external_class.cpp) and emit their own stores,
// which name the compiler's tables; external_class.cpp's banner records that
// decision and why the two tables are slot-compatible. The object is mid-teardown
// at that point and nothing dispatches on it.
//
// THE THREE ARRAY TEARDOWNS RUN EXACTLY ONCE EACH, and that needs saying because
// `_textures`/`_constants`/`_byteCode` are real `Collection::Array<T>` members
// with real destructors now. The bodies below call the REGISTERED retail array
// destructors (0x18004fb00 / 0x18004fbd0), which release each element's String /
// free each inner array AND THEN zero `_data`/`_size`/`_capacity`
// (`ArrayDtorImpl`, runtime/src/collection/array.cpp:225). The compiler-emitted
// `~Array<T>` that runs after this body therefore sees an empty header and frees
// nothing. That is the same decomposition `Object::External::~External` uses for
// `_refs`, with each half running once.
//
// ---------------------------------------------------------------------------
// Serialize (0x1800f5110) — CONSTANTS FIRST, TEXTURES SECOND
// ---------------------------------------------------------------------------
// The order is not the field order and is not guessable: the body serializes
// the array at +0x30 (`_constants`, the one GetConstantCount reads) BEFORE the
// array at +0x20 (`_textures`). Getting it backwards reads every name into the
// wrong array and shows up only as a shader that binds the wrong sampler.
//
// Each array is: `int n = arr._size; rw->Int(&n); if (reading) SetSize(&arr,n)`
// (0x180055db0 — runtime/src/collection/array.cpp `kSpec_ShaderParam`), then a
// per-element loop of
//     String::Serialize(&e->name, &ser)          0x1800c9930
//     rw->Int(&e->dest)
//     if (rw->GetVersion() >= 1) rw->Bool(&e->internal)
// The version test is INSIDE the loop in retail, re-asking the reader/writer
// once per element. Kept that way.
//
// ---------------------------------------------------------------------------
// PlatformSerialize (0x1800f52b0) — THREE ARMS, ONE PER GRAPHICS INTERFACE
// ---------------------------------------------------------------------------
// Byte code is stored once per backend and only the ACTIVE backend's copy is
// read. The body opens with `if (!stream->encoding->vtable[0](encoding)) return;`
// — the encoding policy object's "does this stream carry platform data at all"
// slot — and then runs the same block three times, for
// `Video::Device::_singleton->activeInterface` == 0, == 1 and == 2. The third
// arm is additionally gated on the OUTER reader/writer's GetVersion() >= 1.
//
// Each arm builds ITS OWN reader/writer over the same stream, so the three
// blocks are independently skippable: when the arm's interface is not the
// active one the body calls `rw->Skip()` (slot +0x10) instead of reading, which
// advances the stream past that backend's blob. That is the whole mechanism —
// three sub-streams, read one, skip two — and it is why the arms cannot be
// collapsed into a loop over a single reader/writer.
//
// Inside an arm:
//     int n = _byteCode._size; rw->Int(&n);
//     if (reading) SetSize(&_byteCode, n)        0x180053f00  kSpec_InnerArrayU8
//     for each inner Array<uint8>:
//         int m = inner->_size; rw->Int(&m);
//         if (reading) SetSize(inner, m)         0x180029630
//         rw->UCharArray(m, inner->_data)
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../../types/io/Serializer.h"
#include "../../types/video/ShaderProgram.h"

// standalone/resolve.h is DELIBERATELY NOT INCLUDED — it drags in <windows.h>,
// and <wingdi.h> declares a function called `AlphaBlend`, which would make
// Video::RenderState::AlphaBlend ambiguous at every use. Same local declaration
// runtime/src/collection/array.cpp and runtime/src/object/deleting_dtor.cpp
// carry, for the same PLAN.md §4.1 reason; the linker checks the two spellings.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

using rt::io::Serializer;
using rt::io::Stream;
using rt::io::ReaderWriter;
using Collection::Array;
using ShaderParameter = Video::ShaderProgram::Parameter;

void* BaseVTable()             { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* ExternalVTable()         { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* CompiledExternalVTable() { return RT_DATA_SLOT(void*, "??_7CompiledExternal@Object@@6B@"); }
void* ShaderProgramVTable()    { return RT_DATA_SLOT(void*, "??_7ShaderProgram@Video@@6B@"); }
const uint8_t* ShaderProgramTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*, "?_typeInfo@ShaderProgram@Video@@0VBaseTypeInfo@Object@@A");
}

// `?_singleton@Device@Video@@` is the ADDRESS of the Device* static;
// Device+0xF8 is `activeInterface` (runtime/types/video/Device.h).
int32_t ActiveInterface() {
    void** slot = RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
    void* device = slot ? *slot : nullptr;
    // Retail derefs the singleton unguarded here (MOV RAX,[0x1801fe7a8] then
    // CMP dword [RAX+0xF8]); the null test exists only because RT_DATA_SLOT can
    // hand back a slot that the mint has not filled yet, which retail's own
    // static could not be.
    if (!device) return -1;
    return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0xF8);
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

using ScalarFn  = void (*)(ReaderWriter*, void*, const char*);
using ArrayFn   = void (*)(ReaderWriter*, int, void*, const char*);
using VersionFn = int  (*)(ReaderWriter*);
using SkipFn    = void (*)(ReaderWriter*);
using SetSizeFn = void (*)(void*, int);

inline void RwInt(ReaderWriter* rw, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, p, nullptr);
}
inline void RwBool(ReaderWriter* rw, void* p) {
    reinterpret_cast<ScalarFn>(rw->vtable[rt::io::rw_vtbl::kScalarBool / 8])(rw, p, nullptr);
}
inline int RwVersion(ReaderWriter* rw) {
    return reinterpret_cast<VersionFn>(rw->vtable[rt::io::rw_vtbl::kGetVersion / 8])(rw);
}
inline void DeleteRw(ReaderWriter* rw) {
    if (rw == nullptr) return;
    using DeleteFn = void (*)(ReaderWriter*, int);
    reinterpret_cast<DeleteFn>(rw->vtable[0])(rw, 1);
}

// The `Serializer ser{stream,null}; rw = encoding->CreateReader/Writer(&ser,v)`
// prologue every Serialize in this family opens with.
ReaderWriter* MakeReaderWriter(Serializer* ser, int version) {
    Stream* stream = ser->stream;
    void** encVt = *reinterpret_cast<void***>(stream->encoding);
    const int slot = (stream->direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader
                         : rt::io::encoding_vtbl::kCreateWriter;
    using MakeRwFn = ReaderWriter* (*)(void*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<MakeRwFn>(encVt[slot / 8])(stream->encoding, ser, version);
    ser->readerWriter = rw;
    return rw;
}

// One `Array<Parameter>` of the two, in the order retail does them.
void SerializeParameters(Serializer* ser, Array<ShaderParameter>* arr, uintptr_t setSizeAddr) {
    ReaderWriter* rw = ser->readerWriter;
    int32_t n = arr->_size;
    RwInt(rw, &n);
    if (ser->stream->direction == rt::io::kDirectionReading)
        RtAddr<SetSizeFn>(setSizeAddr)(arr, n);

    ShaderParameter* e = arr->_data;
    for (int32_t i = 0; i < n; ++i, ++e) {
        // B.3.1 bridge: ShaderProgram::Parameter::name is `void*` because a real
        // `System::String` member would give Parameter a non-trivial destructor
        // and make the compiler release every name a SECOND time (the reason is
        // spelled out on the Parameter declaration); the eight bytes are a
        // System::String.
        reinterpret_cast<System::String&>(e->name).Serialize(ser);
        RwInt(rw, &e->dest);
        if (RwVersion(rw) >= 1) RwBool(rw, &e->internal);
    }
}

// One arm of PlatformSerialize. `wanted` is the interface index this arm is
// written for; when it is not the active one the arm SKIPS instead of reading.
void PlatformSerializeArm(Video::ShaderProgram* self, Serializer* outer, int32_t wanted) {
    Serializer sub = { outer->stream, nullptr };
    ReaderWriter* rw = MakeReaderWriter(&sub, 0);

    if (ActiveInterface() == wanted) {
        int32_t n = self->_byteCode._size;
        RwInt(rw, &n);
        if (sub.stream->direction == rt::io::kDirectionReading)
            RtAddr<SetSizeFn>(0x180053f00ull)(&self->_byteCode, n);   // Array<Array<uint8>>::SetSize

        Array<uint8_t>* blob = self->_byteCode._data;
        for (int32_t i = 0; i < n; ++i, ++blob) {
            int32_t m = blob->_size;
            RwInt(rw, &m);
            if (sub.stream->direction == rt::io::kDirectionReading)
                RtAddr<SetSizeFn>(0x180029630ull)(blob, m);           // Array<uint8>::SetSize
            reinterpret_cast<ArrayFn>(rw->vtable[rt::io::rw_vtbl::kArrayUChar / 8])(
                rw, m, blob->_data, nullptr);
        }
    } else {
        reinterpret_cast<SkipFn>(rw->vtable[rt::io::rw_vtbl::kSkip / 8])(rw);
    }
    DeleteRw(rw);
}

void ConstructBody(Video::ShaderProgram* self) {
    void** vp = reinterpret_cast<void**>(self);
    *vp = BaseVTable();
    *vp = ExternalVTable();
    self->_path.data      = nullptr;   // +0x08
    self->_refs._data     = nullptr;   // +0x10
    self->_refs._size     = 0;         // +0x18
    self->_refs._capacity = 0;
    *vp = CompiledExternalVTable();
    *vp = ShaderProgramVTable();
    self->_textures._data      = nullptr;   // +0x20
    self->_textures._size      = 0;         // +0x28
    self->_textures._capacity  = 0;
    self->_constants._data     = nullptr;   // +0x30
    self->_constants._size     = 0;         // +0x38
    self->_constants._capacity = 0;
    self->_byteCode._data      = nullptr;   // +0x40
    self->_byteCode._size      = 0;         // +0x48
    self->_byteCode._capacity  = 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// The one piece the two subclasses still reuse verbatim. NOT a member: this is
// retail's ??0ShaderProgram inlined into ??0VertexProgram / ??0PixelProgram, and
// the first thing it writes is the vptr — which a C++ constructor cannot write
// to the MINTED address (B4b-2). vertexprogram.cpp and pixelprogram.cpp call it
// from their own ctor doors, exactly as retail's compiler did.
//
// (`RtVideo_ShaderProgram_DestroyArrays` USED to live beside this for the same
// reason. It is gone: the teardown is `~ShaderProgram()` now and the subclass
// destructors reach it through the chain.)
// ---------------------------------------------------------------------------
extern "C" void RtVideo_ShaderProgram_ConstructBody(void* selfv) {
    ConstructBody(reinterpret_cast<Video::ShaderProgram*>(selfv));   // B.3.5 bridge
}

// ---------------------------------------------------------------------------
// The registered doors. Each is one line over the member of the same name.
// ---------------------------------------------------------------------------
extern "C" uint32_t RtVideo_ShaderProgram_Type(const void* selfv) {
    return reinterpret_cast<const Video::ShaderProgram*>(selfv)
               ->Video::ShaderProgram::Type();                       // B.3.5 bridge
}
extern "C" uint32_t RtVideo_ShaderProgram_TypeId() {
    return Video::ShaderProgram::TypeId();
}
extern "C" const char* RtVideo_ShaderProgram_TypeName(const void* selfv) {
    return reinterpret_cast<const Video::ShaderProgram*>(selfv)
               ->Video::ShaderProgram::TypeName();                   // B.3.5 bridge
}
// ?_TypeName@ / ?_ParentTypeName@ return string literals out of .rdata
// (0x180157a28 and 0x180154930); the bytes were read from the shipped file.
// RULE B5-15: the literal is COPIED, never derived from the C++ base name.
extern "C" const char* RtVideo_ShaderProgram_StaticTypeName()   { return Video::ShaderProgram::_TypeName(); }
extern "C" const char* RtVideo_ShaderProgram_ParentTypeName()   { return Video::ShaderProgram::_ParentTypeName(); }

// ??0ShaderProgram@Video@@QEAA@XZ (0x1800f5030) — a ctor DOOR, not a ctor.
extern "C" void* RtVideo_ShaderProgram_Ctor(void* selfv) {
    ConstructBody(reinterpret_cast<Video::ShaderProgram*>(selfv));   // B.3.5 bridge
    return selfv;
}

// ??1ShaderProgram@Video@@UEAA@XZ (0x1800f50b0)
extern "C" void RtVideo_ShaderProgram_Dtor(void* selfv) {
    reinterpret_cast<Video::ShaderProgram*>(selfv)
        ->Video::ShaderProgram::~ShaderProgram();                    // B.3.5 bridge
}

// ---- the nine non-virtual accessors ---------------------------------------
extern "C" bool RtVideo_ShaderProgram_IsValid(const void* selfv) {
    return reinterpret_cast<const Video::ShaderProgram*>(selfv)
               ->Video::ShaderProgram::IsValid();                    // B.3.5 bridge
}
extern "C" const void* RtVideo_ShaderProgram_GetByteCode(const void* selfv, int index) {
    return &reinterpret_cast<const Video::ShaderProgram*>(selfv)
                ->Video::ShaderProgram::GetByteCode(index);          // B.3.5 bridge
}
extern "C" int32_t RtVideo_ShaderProgram_GetTextureCount(const void* selfv) {
    return reinterpret_cast<const Video::ShaderProgram*>(selfv)
               ->Video::ShaderProgram::GetTextureCount();            // B.3.5 bridge
}
extern "C" int32_t RtVideo_ShaderProgram_GetConstantCount(const void* selfv) {
    return reinterpret_cast<const Video::ShaderProgram*>(selfv)
               ->Video::ShaderProgram::GetConstantCount();           // B.3.5 bridge
}
extern "C" int32_t RtVideo_ShaderProgram_GetTextureDest(const void* selfv, int index) {
    return reinterpret_cast<const Video::ShaderProgram*>(selfv)
               ->Video::ShaderProgram::GetTextureDest(index);        // B.3.5 bridge
}
extern "C" int32_t RtVideo_ShaderProgram_GetConstantDest(const void* selfv, int index) {
    return reinterpret_cast<const Video::ShaderProgram*>(selfv)
               ->Video::ShaderProgram::GetConstantDest(index);       // B.3.5 bridge
}
extern "C" bool RtVideo_ShaderProgram_IsTextureInternal(const void* selfv, int index) {
    return reinterpret_cast<const Video::ShaderProgram*>(selfv)
               ->Video::ShaderProgram::IsTextureInternal(index);     // B.3.5 bridge
}

// ?GetTextureName@ / ?GetConstantName@ are NOT here. Both return System::String
// BY VALUE through a hidden RDX return slot, and both were already ported —
// with a byte-level sweep against the shipped exports — in
// runtime/src/video/shader_program_names.cpp, which is also where their MEMBER
// bodies now live. Writing a second copy here would have been the duplicate-body
// drift this tree keeps warning about, and the linker caught it. Read that file
// for the hidden-slot analysis (reference_hidden_retslot_wild_write.md).

// ?PlatformSerialize@ShaderProgram@Video@@QEAAXAEAVSerializer@IO@@@Z (0x1800f52b0)
extern "C" void RtVideo_ShaderProgram_PlatformSerialize(void* selfv, void* serv) {
    reinterpret_cast<Video::ShaderProgram*>(selfv)
        ->Video::ShaderProgram::PlatformSerialize(serv);             // B.3.5 bridge
}

// ?Serialize@ShaderProgram@Video@@UEAA_NAEAVStream@IO@@@Z (0x1800f5110)
extern "C" bool RtVideo_ShaderProgram_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<Video::ShaderProgram*>(selfv)
               ->Video::ShaderProgram::Serialize(streamv);           // B.3.5 bridge
}

// ============================================================================
// THE MEMBER BODIES — `Video::ShaderProgram::*`.
//
// At `namespace Video` scope and AFTER the anonymous namespace, per B34_BRIEFS
// A.2. The helpers above stay reachable by unqualified name because an
// anonymous namespace's members are members of the global namespace's
// declarative region.
// ============================================================================
namespace Video {

// slot 1 — ?TypeName@ShaderProgram@Video@@UEBAPEBDXZ  0x18004e0c0
//          `BaseTypeInfo::name`, the record's +0x08. CONTENT-only, never
//          compared by pointer (reference_basetypeinfo_name_is_content_only.md).
const char* ShaderProgram::TypeName() const {
    const char* v = nullptr;
    std::memcpy(&v, ShaderProgramTypeInfo() + 8, sizeof v);
    return v;
}

// slot 2 — ?Type@ShaderProgram@Video@@UEBAIXZ  0x18004e0a0
uint32_t ShaderProgram::Type() const {
    uint32_t v = 0;
    std::memcpy(&v, ShaderProgramTypeInfo(), sizeof v);
    return v;
}

// ?TypeId@ShaderProgram@Video@@SAIXZ  0x18004e0b0 — the same record's +0x00.
uint32_t ShaderProgram::TypeId() {
    uint32_t v = 0;
    std::memcpy(&v, ShaderProgramTypeInfo(), sizeof v);
    return v;
}

// RULE B5-15: both literals were READ out of the shipped .rdata, not derived
// from the C++ base name. `#Parent` stringisation would give
// "Object::CompiledExternal" here by luck and the wrong answer elsewhere.
const char* ShaderProgram::_TypeName()       { return "ShaderProgram"; }
const char* ShaderProgram::_ParentTypeName() { return "Object::CompiledExternal"; }

// ??1ShaderProgram@Video@@UEAA@XZ (0x1800f50b0) — see the file banner for which
// of retail's four steps are written here and which the chain emits.
ShaderProgram::~ShaderProgram() {
    *reinterpret_cast<void**>(this) = ShaderProgramVTable();

    // The three Array teardowns, in retail's order (+0x40 first, then +0x30,
    // then +0x20). Each registered body ends in `ArrayDtorImpl`, which zeroes
    // the header, so the compiler-emitted `~Array` that follows frees nothing.
    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x18004fbd0ull)(&_byteCode);    // ~Array<Array<uint8>>
    RtAddr<ArrayDtorFn>(0x18004fb00ull)(&_constants);   // ~Array<Parameter>
    RtAddr<ArrayDtorFn>(0x18004fb00ull)(&_textures);    // ~Array<Parameter>
}

// ?IsValid@ShaderProgram@Video@@QEBA_NXZ  0x18004e0f0 — `+0x48 > 0`.
bool ShaderProgram::IsValid() const { return _byteCode._size > 0; }

// ?GetByteCode@ShaderProgram@Video@@QEBAAEBV?$Array@E@Collection@@H@Z
// 0x18004e100: `(Array<uint8>*)_byteCode._data + index`. No bounds check in
// retail and none added here.
const Collection::Array<uint8_t>& ShaderProgram::GetByteCode(int32_t index) const {
    return _byteCode._data[index];
}

int32_t ShaderProgram::GetTextureCount() const  { return _textures._size; }    // 0x18004e410
int32_t ShaderProgram::GetConstantCount() const { return _constants._size; }   // 0x18004e4a0

int32_t ShaderProgram::GetTextureDest(int32_t index) const {                   // 0x18004e470
    return _textures._data[index].dest;
}
int32_t ShaderProgram::GetConstantDest(int32_t index) const {                  // 0x18004e500
    return _constants._data[index].dest;
}
bool ShaderProgram::IsTextureInternal(int32_t index) const {                   // 0x18004e480
    return _textures._data[index].internal != 0;
}

// ?PlatformSerialize@ShaderProgram@Video@@QEAAXAEAVSerializer@IO@@@Z 0x1800f52b0
void ShaderProgram::PlatformSerialize(void* serv) {
    Serializer* ser = static_cast<Serializer*>(serv);

    void** encVt = *reinterpret_cast<void***>(ser->stream->encoding);
    using CarriesPlatformDataFn = bool (*)(void*);
    if (!reinterpret_cast<CarriesPlatformDataFn>(encVt[0])(ser->stream->encoding)) return;

    PlatformSerializeArm(this, ser, 0);
    PlatformSerializeArm(this, ser, 1);
    // The third backend arrived in file version 1; older files have only two
    // sub-streams and must not be advanced past a third.
    if (RwVersion(ser->readerWriter) < 1) return;
    PlatformSerializeArm(this, ser, 2);
}

// slot 5 — ?Serialize@ShaderProgram@Video@@UEAA_NAEAVStream@IO@@@Z 0x1800f5110
bool ShaderProgram::Serialize(void* streamv) {
    Serializer ser = { static_cast<Stream*>(streamv), nullptr };
    ReaderWriter* rw = MakeReaderWriter(&ser, 1);

    SerializeParameters(&ser, &_constants, 0x180055db0ull);   // +0x30 FIRST
    SerializeParameters(&ser, &_textures,  0x180055db0ull);   // +0x20 second
    // Retail CALLs 0x1800f52b0 here; the qualified spelling is that call.
    this->ShaderProgram::PlatformSerialize(&ser);

    DeleteRw(rw);
    return true;
}

}  // namespace Video

int RtVideoShaderProgramRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@ShaderProgram@Video@@UEBAIXZ",        (void*)&RtVideo_ShaderProgram_Type);     ++n;
    RegisterRuntimeAddr  (0x18004e0a0ull,                              (void*)&RtVideo_ShaderProgram_Type);
    RegisterRuntimeExport("?TypeId@ShaderProgram@Video@@SAIXZ",        (void*)&RtVideo_ShaderProgram_TypeId);   ++n;
    RegisterRuntimeAddr  (0x18004e0b0ull,                              (void*)&RtVideo_ShaderProgram_TypeId);
    RegisterRuntimeExport("?TypeName@ShaderProgram@Video@@UEBAPEBDXZ", (void*)&RtVideo_ShaderProgram_TypeName); ++n;
    RegisterRuntimeAddr  (0x18004e0c0ull,                              (void*)&RtVideo_ShaderProgram_TypeName);
    RegisterRuntimeExport("?_TypeName@ShaderProgram@Video@@CAPEBDXZ",       (void*)&RtVideo_ShaderProgram_StaticTypeName); ++n;
    RegisterRuntimeAddr  (0x18004e080ull,                                   (void*)&RtVideo_ShaderProgram_StaticTypeName);
    RegisterRuntimeExport("?_ParentTypeName@ShaderProgram@Video@@CAPEBDXZ", (void*)&RtVideo_ShaderProgram_ParentTypeName); ++n;
    RegisterRuntimeAddr  (0x18004e090ull,                                   (void*)&RtVideo_ShaderProgram_ParentTypeName);

    RegisterRuntimeExport("??0ShaderProgram@Video@@QEAA@XZ", (void*)&RtVideo_ShaderProgram_Ctor); ++n;
    RegisterRuntimeAddr  (0x1800f5030ull,                    (void*)&RtVideo_ShaderProgram_Ctor);
    // UEAA, not QEAA: ~ShaderProgram is virtual
    // (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md).
    RegisterRuntimeExport("??1ShaderProgram@Video@@UEAA@XZ", (void*)&RtVideo_ShaderProgram_Dtor); ++n;
    RegisterRuntimeAddr  (0x1800f50b0ull,                    (void*)&RtVideo_ShaderProgram_Dtor);

    RegisterRuntimeExport("?Serialize@ShaderProgram@Video@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtVideo_ShaderProgram_Serialize); ++n;
    RegisterRuntimeAddr  (0x1800f5110ull, (void*)&RtVideo_ShaderProgram_Serialize);
    RegisterRuntimeExport("?PlatformSerialize@ShaderProgram@Video@@QEAAXAEAVSerializer@IO@@@Z",
                          (void*)&RtVideo_ShaderProgram_PlatformSerialize); ++n;
    RegisterRuntimeAddr  (0x1800f52b0ull, (void*)&RtVideo_ShaderProgram_PlatformSerialize);

    RegisterRuntimeExport("?IsValid@ShaderProgram@Video@@QEBA_NXZ", (void*)&RtVideo_ShaderProgram_IsValid); ++n;
    RegisterRuntimeAddr  (0x18004e0f0ull,                           (void*)&RtVideo_ShaderProgram_IsValid);
    RegisterRuntimeExport("?GetByteCode@ShaderProgram@Video@@QEBAAEBV?$Array@E@Collection@@H@Z",
                          (void*)&RtVideo_ShaderProgram_GetByteCode); ++n;
    RegisterRuntimeAddr  (0x18004e100ull, (void*)&RtVideo_ShaderProgram_GetByteCode);
    RegisterRuntimeExport("?GetTextureCount@ShaderProgram@Video@@QEBAHXZ",
                          (void*)&RtVideo_ShaderProgram_GetTextureCount); ++n;
    RegisterRuntimeAddr  (0x18004e410ull, (void*)&RtVideo_ShaderProgram_GetTextureCount);
    RegisterRuntimeExport("?GetTextureDest@ShaderProgram@Video@@QEBAHH@Z",
                          (void*)&RtVideo_ShaderProgram_GetTextureDest); ++n;
    RegisterRuntimeAddr  (0x18004e470ull, (void*)&RtVideo_ShaderProgram_GetTextureDest);
    RegisterRuntimeExport("?IsTextureInternal@ShaderProgram@Video@@QEBA_NH@Z",
                          (void*)&RtVideo_ShaderProgram_IsTextureInternal); ++n;
    RegisterRuntimeAddr  (0x18004e480ull, (void*)&RtVideo_ShaderProgram_IsTextureInternal);
    RegisterRuntimeExport("?GetConstantCount@ShaderProgram@Video@@QEBAHXZ",
                          (void*)&RtVideo_ShaderProgram_GetConstantCount); ++n;
    RegisterRuntimeAddr  (0x18004e4a0ull, (void*)&RtVideo_ShaderProgram_GetConstantCount);
    // The two by-value name getters are registered by shader_program_names.cpp.
    RegisterRuntimeExport("?GetConstantDest@ShaderProgram@Video@@QEBAHH@Z",
                          (void*)&RtVideo_ShaderProgram_GetConstantDest); ++n;
    RegisterRuntimeAddr  (0x18004e500ull, (void*)&RtVideo_ShaderProgram_GetConstantDest);
    return n;
}
