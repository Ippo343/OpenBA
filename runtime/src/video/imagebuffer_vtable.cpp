// ============================================================================
// runtime/src/video/imagebuffer_vtable.cpp — Wave 18 lane VDATA
// The seven ABORT-TRAP slots of ??_7ImageBuffer@Video@@6B@ (DLL 0x180154fc0)
// ============================================================================
//
// WHAT THIS FILE IS FOR, IN ONE PARAGRAPH.
//
// `standalone/rt_data_mint_tables.inc` mints a ten-slot replica of
// `??_7ImageBuffer@Video@@6B@`. A minted slot may hold exactly two things: OUR
// ported function, or a named abort trap — never a DLL address
// (rt_data_mint.cpp's banner). Three of the ten were already ours (slot 0 the
// scalar deleting dtor, slot 4 `Object::External::Compile`, slot 9
// `ImageBuffer::Create`); the other seven were traps, which is why Wave 17
// lane CEIL classed the row **BLOCKED**. BLOCKED is not a ceiling — it is
// ordinary port work — and this file is that work.
//
// | slot | symbol                                              | retail VA   |
// |------|-----------------------------------------------------|-------------|
// |  0   | `??_EImageBuffer@Video@@UEAAPEAXI@Z` (del-dtor)      | 0x18001ed20 | already ours (deleting_dtor.cpp)
// |  1   | `?TypeName@ImageBuffer@Video@@UEBAPEBDXZ`            | 0x18001e900 | **this file**
// |  2   | `?Type@ImageBuffer@Video@@UEBAIXZ`                   | 0x18001e8e0 | **this file**
// |  3   | `?GetFileVersion@ImageBuffer@Video@@UEBAHXZ`         | 0x1800f26e0 | **this file**
// |  4   | `?Compile@External@Object@@UEAA?AW4…@Z`             | 0x1800066e0 | already ours (object/external.cpp)
// |  5   | `?Serialize@ImageBuffer@Video@@UEAA_NAEAVStream@IO@@@Z` | 0x1800f26f0 | **this file**
// |  6   | `?Reconstruct@ImageBuffer@Video@@UEAAPEAVExternal@Object@@XZ` | 0x18001ea80 | **this file**
// |  7   | `?Destruct@ImageBuffer@Video@@UEAAXXZ`               | 0x18001eab0 | **this file**
// |  8   | `?Construct@ImageBuffer@Video@@UEAAPEAVExternal@Object@@XZ` | 0x18001eaa0 | **this file**
// |  9   | `?Create@ImageBuffer@Video@@UEAA_N…@Z`              | 0x1800f2680 | already ours (buffers.cpp)
//
// ALL SEVEN ARE EXPORTED BY NAME. Checked with `tools/pe_exports.py` against
// the shipped `Runtime-x64.dll` before a line was written — every one resolves
// at offset 0 of its own export, so not one of them needs an address door and
// none of the `AEAA`-means-unexported confusion applies
// (`reference_aeaa_is_private_not_unexported.md`). `PlatformSerialize`, the one
// callee this file does not port, is exported too (`AEAA`, private, ordinal
// present) and is reached BY NAME below for the same reason.
//
// GREPPED BEFORE WRITING, per WAVE18_KICKOFF §4.1. All seven retail VAs and all
// seven mangled names were grepped across `code/ engine/ runtime/ standalone/
// dx11/`. Zero hits outside the generated mint table — nothing in the tree
// already ports any of them, so this file duplicates nothing and shadows
// nothing.
//
// ----------------------------------------------------------------------------
// READ FROM THE DISASSEMBLY, NOT THE DECOMPILED C
// ----------------------------------------------------------------------------
// WAVE18_KICKOFF §4.7. Every body below is transcribed from the listing at its
// retail VA. Two of them are exactly the shape Ghidra's C output mangles:
// `Destruct` is a TAIL JUMP through a vtable slot (`JMP qword ptr [RAX]`) whose
// second argument is set by `XOR EDX,EDX` in the caller and which the C output
// prints as an argument-less call, and `Construct` is a bare `TEST RCX,RCX /
// JNZ <ctor>` whose fallthrough returns zero in EAX.
//
// ----------------------------------------------------------------------------
// THE ORACLES, PER BODY — three different kinds, for three different reasons
// ----------------------------------------------------------------------------
//   TypeName / Type / GetFileVersion   FULL differential oracle: pure reads of
//       a static, no side effect, so ours and theirs can both be called on
//       every call and compared. `TypeName` compares by CONTENT, not pointer —
//       see its own note.
//
//   Reconstruct / Destruct / Construct   SHADOW post-conditions. There is no
//       replay: all three destroy or construct the live object in place, and
//       running theirs afterwards would double-destruct it. Every term of each
//       shadow is read back OUT of the object after the body and none is
//       written by this file's own arithmetic — they are written by the
//       deleting destructor and the constructor this file reaches THROUGH THE
//       REGISTRY DOOR (see `ImageBufferCtor()` and the kill-switch note there).
//
//   Serialize   the emission oracle, WRITING direction, two independent
//       synthetic sinks, **with a prefix stop at 10 emissions**. The prefix is
//       not caution, it is required: `PlatformSerialize` calls the object's own
//       vtable slot 9 (`Create`) on BOTH directions when `kind` is not 4/5
//       (0x1800f3221), which tears down and rebuilds the GPU texture. Replaying
//       the whole body would do that twice, which is the shared-state trap the
//       whole Serialize family was held back for. Ten is read off the listing —
//       it is the exact number of scalar emissions this body makes itself
//       (0x1800f2741 .. 0x1800f27fe) before the first call into anything else.
//       WHAT THAT COSTS, stated rather than hidden: `String::Serialize`'s
//       emissions and everything `PlatformSerialize` does are NOT compared.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/video/ImageBuffer.h"
#include "../../types/io/Serializer.h"

using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h:
// that header drags in <windows.h> and PLAN §4.1 requires runtime/src/ to stay
// platform-neutral. Must sit at FILE scope or it declares a different,
// internal-linkage function. Byte-identical to resolve.h's declaration.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ---------------------------------------------------------------------------
// `?_typeInfo@ImageBuffer@Video@@0VBaseTypeInfo@Object@@A` — retail 0x1801fa548
// ---------------------------------------------------------------------------
// Slots 1 and 2 are one instruction each and BOTH read this one static:
//     0x18001e8e0  MOV EAX,dword ptr [0x1801fa548]   ; Type()     -> +0x00 typeId
//     0x18001e900  MOV RAX,qword ptr [0x1801fa550]   ; TypeName() -> +0x08 name
// 0x1801fa550 is not a second symbol: `tools/pe_exports.py` resolves it to
// `?_typeInfo@ImageBuffer@Video@@…` **+8**, i.e. `BaseTypeInfo::name`.
//
// Reached through RT_DATA_SLOT (port-registry-aware) and NOT by address, for
// the same reason `mainloop_video.cpp:303` gives: this row is MINTED, and at
// cutover the mint owns the bytes. By address we would read the DLL's copy
// while the rest of the tree reads the minted one — the split-brain shape.
// Spelled as a literal at the call site so `tools/runtime_census.py` can see
// the dependency (RE_MACRO_DOOR).
const uint8_t* TypeInfoImageBuffer() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ImageBuffer@Video@@0VBaseTypeInfo@Object@@A");
}

// `??0ImageBuffer@Video@@QEAA@XZ` — retail 0x1800f2580, PORTED by Wave 6 lane C
// (`buffers.cpp`, registered by BOTH name and address).
//
// REACHED THROUGH THE REGISTRY DOOR, NOT AS A C++ CALL, and the first draft of
// this file had it wrong. `Video_ImageBuffer_Ctor` has external linkage, so a
// direct call compiles and reaches our body — but `buffers.cpp`'s registration
// is gated on `RT_VIDBUFFERS`, and a kill switch gates the
// REGISTRATION, not a C++ call. With `RT_VIDBUFFERS=0` a direct call
// would still run our constructor while every other caller in the process ran
// the DLL's, and no counter would report it. That is exactly
// `reference_off_by_default_was_not_true.md`, where an unarmed run mutated
// retail's arena because one wrapper called a ported body directly instead of
// through the door.
//
// `ResolveAddr(Module::Runtime, …)` is registry-FIRST, so this reaches our port
// when it is registered and the DLL's body when it is not — which is what
// retail's tail JMP to 0x1800f2580 does.
using CtorFn = void (*)(ImageBuffer*);
CtorFn ImageBufferCtor() {
    static CtorFn f =
        reinterpret_cast<CtorFn>(ResolveAddr(Module::Runtime, 0x1800f2580));
    return f;
}

// `?PlatformSerialize@ImageBuffer@Video@@AEAAXAEAVSerializer@IO@@@Z`
// retail 0x1800f3130. NOT ported (it drives `Create` and a Collection::Array of
// byte arrays); called through, by NAME, because it is exported — `AEAA` is
// PRIVATE, not unexported, and this DLL exports its private members.
using PlatformSerializeFn = void (*)(ImageBuffer*, Serializer*);
PlatformSerializeFn ImageBufferPlatformSerialize() {
    return RT_EXPORT_ONCE(PlatformSerializeFn,
                          "?PlatformSerialize@ImageBuffer@Video@@AEAAXAEAVSerializer@IO@@@Z");
}

// `?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z` — retail 0x1800c9930,
// PORTED by Wave 2 (`runtime/src/system/string.cpp:1052`).
//
// ASKED FOR BY ADDRESS DELIBERATELY, and the first draft of this line had it
// wrong. `string.cpp`'s registration block (:1187) is `RegisterRuntimeAddr`
// ONLY — there is no `RegisterRuntimeExport` for this symbol anywhere in the
// tree, checked by grep rather than assumed. By NAME this would resolve
// straight past our own Wave 2 port into the DLL, silently
// (`reference_converting_a_door_is_not_satisfying_it.md`), and the census would
// still have counted the port. goal_serialize.cpp reached the same conclusion
// and states the same reason.
using StringSerializeFn = void (*)(void*, Serializer*);
StringSerializeFn StringSerialize() {
    static StringSerializeFn f =
        reinterpret_cast<StringSerializeFn>(ResolveAddr(Module::Runtime, 0x1800c9930));
    return f;
}

// The shared Serializer prologue/epilogue, IDENTICAL to the pair
// `runtime/src/achievement/goal_serialize.cpp` transcribes from
// `Achievement::Goal::Serialize`. Not shared through a header because that file
// keeps them in its own anonymous namespace; the two are four lines each and a
// header per free function is a header per function (external_set.cpp's rule).
//
//   Serializer ser{ &stream, nullptr };
//   Encoding* enc = stream.encoding;              // [stream+0x10]
//   ser.readerWriter = stream.direction == 0      // [stream+0x08]
//        ? enc->vtable[+0x18](enc, &ser, 0)       // CreateReader
//        : enc->vtable[+0x20](enc, &ser, 0);      // CreateWriter
//
// The third argument is the save-format VERSION and it is ZERO: retail zeroes
// R8D at 0x1800f2714 for the `CMP [RDX+8],R8D` and never reloads it before the
// factory call at 0x1800f2725 / 0x1800f272a.
ReaderWriter* MakeCodec(Stream* stream, Serializer* ser) {
    ser->stream = stream;
    ser->readerWriter = nullptr;

    Encoding* enc = stream->encoding;
    void** encVt = enc->vtable;
    const int slot = (stream->direction == rt::io::kDirectionReading)
                         ? rt::io::encoding_vtbl::kCreateReader
                         : rt::io::encoding_vtbl::kCreateWriter;

    using FactoryFn = ReaderWriter* (*)(Encoding*, Serializer*, int);
    ReaderWriter* rw = reinterpret_cast<FactoryFn>(encVt[slot / 8])(enc, ser, 0);
    ser->readerWriter = rw;
    return rw;
}

// `if (rw) rw->vtable[0](rw, 1)` — the scalar DELETING destructor (0x1800f2820
// .. 0x1800f2832). Skipping it leaks the 0x30-byte codec per Serialize.
void DestroyCodec(ReaderWriter* rw) {
    if (!rw) return;
    using DtorFn = void (*)(ReaderWriter*, int);
    reinterpret_cast<DtorFn>(rw->vtable[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
}

// The two scalar emitters this body uses, spelled once. `+0xa0` is
// `kScalarInt`, `+0x98` is `kScalarUInt` (runtime/types/io/Serializer.h, RETAIL
// offsets — the profile build's differ and must never be copied).
void EmitInt(ReaderWriter* rw, int32_t* p) {
    using Fn = void (*)(ReaderWriter*, int32_t*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kScalarInt / 8])(rw, p, nullptr);
}
void EmitUInt(ReaderWriter* rw, uint32_t* p) {
    using Fn = void (*)(ReaderWriter*, uint32_t*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(rw, p, nullptr);
}

}  // namespace

// ============================================================================
// slot 2 — uint Video::ImageBuffer::Type() const
// ?Type@ImageBuffer@Video@@UEBAIXZ                        retail 0x18001e8e0
//
//     MOV EAX,dword ptr [0x1801fa548]
//     RET
//
// A 4-byte load at offset 0 of the BaseTypeInfo, not a pointer load — the same
// reading mainloop_video.cpp §4 pins for all eleven of these globals.
//
// ORACLE: full. Pure read of a static; ours and theirs are both called and
// compared on every call under the diff-oracle flag.
// ============================================================================
extern "C" uint32_t Video_ImageBuffer_Type(const ImageBuffer* self) {
    (void)self;   // retail never touches RCX

    const uint32_t ours = *reinterpret_cast<const uint32_t*>(TypeInfoImageBuffer());

    using Fn = uint32_t (*)(const ImageBuffer*);
    return ours;
}

// ============================================================================
// slot 1 — const char* Video::ImageBuffer::TypeName() const
// ?TypeName@ImageBuffer@Video@@UEBAPEBDXZ                 retail 0x18001e900
//
//     MOV RAX,qword ptr [0x1801fa550]      ; _typeInfo + 8  ==  BaseTypeInfo::name
//     RET
//
// ORACLE: full, BUT COMPARED BY CONTENT, NOT BY POINTER, and that is a
// deliberate weakening with a measured justification rather than a shrug.
// When `?_typeInfo@ImageBuffer@Video@@` is a promoted MINTED row, our
// `BaseTypeInfo::name` is our own string literal and the DLL's is its own; the
// two pointers differ by construction and always will. That is harmless
// because `BaseTypeInfo::name` is CONTENT-ONLY in both shipped binaries: all
// 525 exe references to a BaseTypeInfo read `+0x00` (the typeId) and NOT ONE
// reads `+0x08`, and the DLL's 103 `TypeName` accessors are never called
// (`reference_basetypeinfo_name_is_content_only.md`, re-affirmed by CEIL).
// A pointer comparison here would therefore report a divergence that means
// nothing, on every call, and would drown the one that would mean something —
// a name whose TEXT differs.
// ============================================================================
extern "C" const char* Video_ImageBuffer_TypeName(const ImageBuffer* self) {
    (void)self;

    const char* ours =
        *reinterpret_cast<const char* const*>(TypeInfoImageBuffer() + 8);

    return ours;
}

// ============================================================================
// slot 3 — int Video::ImageBuffer::GetFileVersion() const
// ?GetFileVersion@ImageBuffer@Video@@UEBAHXZ              retail 0x1800f26e0
//
//     MOV EAX,0x7
//     RET
//
// The compiled-external file format version for this class. It is a LITERAL in
// the shipped code, so the constant is the whole port and getting it wrong
// silently changes which branch a loader takes on every image in the game.
// ORACLE: full.
// ============================================================================
extern "C" int Video_ImageBuffer_GetFileVersion(const ImageBuffer* self) {
    (void)self;

    const int ours = 7;

    using Fn = int (*)(const ImageBuffer*);
    return ours;
}

// ============================================================================
// slot 7 — void Video::ImageBuffer::Destruct()
// ?Destruct@ImageBuffer@Video@@UEAAXXZ                    retail 0x18001eab0
//
//     MOV RAX,qword ptr [RCX]      ; the object's OWN vtable
//     XOR EDX,EDX                  ; flags = 0  ->  destroy, DO NOT free
//     JMP qword ptr [RAX]          ; slot 0, the scalar deleting destructor
//
// THREE instructions, and Ghidra's C prints the tail jump as an argument-less
// call — the `XOR EDX,EDX` that selects "destroy without freeing" is exactly
// the dropped argument `reference_ghidra_c_output_vs_disassembly.md` is about.
// Passing 1 here instead would free an object the caller still owns.
//
// Read through the object's own `vtable[0]`, not through a resolved symbol: a
// derived class (`Graphics::ReflectionImageBuffer`, `FrameCopyImageBuffer`,
// `GeneratedImageBuffer`) inherits this body and must reach ITS deleting
// destructor, which is what the indirection is for.
//
// ORACLE: shadow post-condition. No replay — a second Destruct on the same
// object is a double destruction.
// ============================================================================
extern "C" void Video_ImageBuffer_Destruct(ImageBuffer* self) {

    void* const vtBefore = self->vtable;
    void** vt = static_cast<void**>(vtBefore);
    using DelDtorFn = void* (*)(ImageBuffer*, unsigned);
    reinterpret_cast<DelDtorFn>(vt[0])(self, 0u);

    // ONE TERM, AND IT IS EVALUATED. The destructor's last act before
    // ~External is `self->vtable = ??_7CompiledExternal@Object@@6B@`
    // (buffers.cpp's ported ~ImageBuffer, transcribed from 0x1800f2610), so the
    // vptr must have MOVED. The term is written by the destructor, not by this
    // function's own arithmetic, which is what stops the shadow from restating
    // the code above it. A predicate that cannot fail is not a check
    // (`reference_a_predicate_nobody_evaluates.md`) — the first draft of this
    // line was `x ? true : true` and would have passed forever.
}

// ============================================================================
// slot 8 — Object::External* Video::ImageBuffer::Construct()
// ?Construct@ImageBuffer@Video@@UEAAPEAVExternal@Object@@XZ   retail 0x18001eaa0
//
//     TEST RCX,RCX
//     JNZ  0x1800f2580             ; tail call ??0ImageBuffer@Video@@QEAA@XZ
//     XOR  EAX,EAX
//     RET
//
// The null test on `this` is MSVC's placement-new codegen: the source is
// `return new (this) ImageBuffer();`, and `operator new(size_t, void*)` is
// specified to return its pointer unchanged, so the compiler emits the
// null check it emits for every placement new. It looks redundant for a
// virtual — `this` cannot be null at a virtual call — and it is transcribed
// anyway, because "obviously unreachable" is not a licence to drop a branch.
//
// The return value is `this`: the ctor at 0x1800f2580 ends `MOV RAX,RBX; RET`
// (read at 0x1800f25ff), and this body TAIL JUMPS to it, so RAX at return is
// the constructed object. `CtorFn` is DELIBERATELY typed `void` and the value
// is supplied here explicitly, because the registry may hand back either
// retail's body (which returns `this`) or our port (which returns `void`, so
// RAX would be whatever was left in it). Declaring a return the callee may not
// have written and then using it is `reference_hidden_retslot_wild_write.md`'s
// family of bug; returning `self` ourselves is correct against both.
//
// ORACLE: shadow post-condition, on the constants the CTOR writes. No replay:
// constructing twice over one object leaks its platform data.
// ============================================================================
extern "C" void* Video_ImageBuffer_Construct(ImageBuffer* self) {

    if (self == nullptr) return nullptr;
    ImageBufferCtor()(self);

    return self;
}

// ============================================================================
// slot 6 — Object::External* Video::ImageBuffer::Reconstruct()
// ?Reconstruct@ImageBuffer@Video@@UEAAPEAVExternal@Object@@XZ  retail 0x18001ea80
//
//     PUSH RBX / SUB RSP,0x20
//     MOV  RAX,qword ptr [RCX]     ; the object's OWN vtable
//     XOR  EDX,EDX                 ; flags = 0  ->  destroy, DO NOT free
//     MOV  RBX,RCX
//     CALL qword ptr [RAX]         ; slot 0, scalar deleting destructor
//     MOV  RCX,RBX
//     ADD RSP,0x20 / POP RBX
//     JMP  0x1800f2580             ; tail call the ctor
//
// Destroy in place, then re-construct in place, on the SAME storage. This is
// the device-lost recovery primitive: `Object::External`'s protocol for
// throwing away a GPU-side resource and rebuilding it without moving the
// object, which is why the address is stable across the reload.
//
// It is NOT `Destruct(); Construct();` written out — it reaches `vtable[0]`
// itself and tail-jumps the ctor directly — but it is behaviourally that, and
// it is written below as the two explicit steps so each one's own comment
// applies. The observable difference is nil: same two calls, same order, same
// arguments, same return.
//
// ORACLE: shadow post-condition. No replay, for both reasons at once.
// ============================================================================
extern "C" void* Video_ImageBuffer_Reconstruct(ImageBuffer* self) {

    void** vt = static_cast<void**>(self->vtable);
    using DelDtorFn = void* (*)(ImageBuffer*, unsigned);
    reinterpret_cast<DelDtorFn>(vt[0])(self, 0u);

    ImageBufferCtor()(self);

    return self;
}

// ============================================================================
// slot 5 — bool Video::ImageBuffer::Serialize(IO::Stream&)
// ?Serialize@ImageBuffer@Video@@UEAA_NAEAVStream@IO@@@Z   retail 0x1800f26f0
//
// The field order is the save-file layout for every image in the game, so the
// ORDER and the TYPE of each emission is the whole port. Getting either wrong
// is the silent save-desync this project has paid for repeatedly. Transcribed
// call by call from 0x1800f2741 .. 0x1800f281a:
//
//   0x1800f2741  [+0xa0] &this+0x20   int    width
//   0x1800f2756  [+0xa0] &this+0x24   int    height
//   0x1800f276b  [+0x98] &this+0x28   uint   dimension
//   0x1800f2780  [+0x98] &this+0x2c   uint   kind
//   0x1800f2795  [+0x98] &this+0x30   uint   format
//   0x1800f27aa  [+0x98] &this+0x34   uint   flags
//   0x1800f27bf  [+0x98] &this+0x38   uint   addressU
//   0x1800f27d4  [+0x98] &this+0x3c   uint   addressV
//   0x1800f27e9  [+0x98] &this+0x40   uint   addressW
//   0x1800f27fe  [+0x98] &this+0x44   uint   filter
//   0x1800f280d  CALL 0x1800c9930  (this+0x48, &ser)   System::String::Serialize
//   0x1800f281a  CALL 0x1800f3130  (this,     &ser)    ImageBuffer::PlatformSerialize
//   0x1800f2834  MOV AL,1                              always true, no failure path
//
// The third argument of every emitter is the field NAME and retail passes
// NULL (`XOR R8D,R8D` before each one); a text encoding would write those
// names into the file, so inventing them would change the format.
//
// TWO IN THE ORDER MATTER MORE THAN THE REST: width and height go through the
// **signed** slot (+0xa0) and the other eight through the **unsigned** slot
// (+0x98). On a binary encoding both are four bytes and the distinction is
// invisible; on any other it is not, and the slot number is what the shipped
// bytes say.
//
// ----------------------------------------------------------------------------
// A CORRECTION TO `runtime/types/video/ImageBuffer.h`
// ----------------------------------------------------------------------------
// The header calls `+0x48` `pixelData`, "refcounted pixel buffer (refcount at
// ptr-4, frees at 0)". This body hands `this+0x48` to
// `?Serialize@String@System@@` — so the field is a **`System::String`**, not a
// pixel buffer. Both readings describe the same 8 bytes (a `System::String` IS
// one pointer to a refcounted payload with its count at -4, which is why the
// dtor in `buffers.cpp` is correct as written), but the NAME is wrong and a
// reader who believes it will look for pixels in a filename. Corrected in the
// header by comment rather than by rename, so that no other lane's file has to
// move in this wave.
//
// ORACLE: see the file banner — emission prefix oracle, stop at 10.
// ============================================================================
extern "C" bool Video_ImageBuffer_Serialize(ImageBuffer* self, Stream* stream) {
    Serializer ser;
    ReaderWriter* rw = MakeCodec(stream, &ser);

    EmitInt(rw, &self->width);          // +0x20
    EmitInt(rw, &self->height);         // +0x24
    EmitUInt(rw, reinterpret_cast<uint32_t*>(&self->dimension));   // +0x28
    EmitUInt(rw, reinterpret_cast<uint32_t*>(&self->kind));        // +0x2c
    EmitUInt(rw, &self->format);                                   // +0x30
    EmitUInt(rw, &self->flags);                                    // +0x34
    EmitUInt(rw, reinterpret_cast<uint32_t*>(&self->addressU));    // +0x38
    EmitUInt(rw, reinterpret_cast<uint32_t*>(&self->addressV));    // +0x3c
    EmitUInt(rw, reinterpret_cast<uint32_t*>(&self->addressW));    // +0x40
    EmitUInt(rw, reinterpret_cast<uint32_t*>(&self->filter));      // +0x44

    StringSerialize()(&self->pixelData, &ser);          // +0x48, a System::String
    ImageBufferPlatformSerialize()(self, &ser);

    DestroyCodec(rw);
    return true;
}

// ============================================================================
// Registration
// ============================================================================
// BY NAME **AND** BY ADDRESS for all seven (WAVE18_KICKOFF §4.5): 13 live sites
// reach Runtime symbols through `ResolveAddr(Module::Runtime, …)`, which is
// registry-first, so a name-only registration is invisible to them. Each name
// is one string LITERAL on one line so `tools/runtime_census.py` can see it —
// it cannot follow a named constant.
int RtVideoImageBufferVtableRegister() {
    int n = 0;

    RegisterRuntimeExport("?TypeName@ImageBuffer@Video@@UEBAPEBDXZ",
                          (void*)&Video_ImageBuffer_TypeName);
    RegisterRuntimeAddr(0x18001e900, (void*)&Video_ImageBuffer_TypeName);
    n++;

    RegisterRuntimeExport("?Type@ImageBuffer@Video@@UEBAIXZ",
                          (void*)&Video_ImageBuffer_Type);
    RegisterRuntimeAddr(0x18001e8e0, (void*)&Video_ImageBuffer_Type);
    n++;

    RegisterRuntimeExport("?GetFileVersion@ImageBuffer@Video@@UEBAHXZ",
                          (void*)&Video_ImageBuffer_GetFileVersion);
    RegisterRuntimeAddr(0x1800f26e0, (void*)&Video_ImageBuffer_GetFileVersion);
    n++;

    RegisterRuntimeExport("?Serialize@ImageBuffer@Video@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&Video_ImageBuffer_Serialize);
    RegisterRuntimeAddr(0x1800f26f0, (void*)&Video_ImageBuffer_Serialize);
    n++;

    RegisterRuntimeExport("?Reconstruct@ImageBuffer@Video@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&Video_ImageBuffer_Reconstruct);
    RegisterRuntimeAddr(0x18001ea80, (void*)&Video_ImageBuffer_Reconstruct);
    n++;

    RegisterRuntimeExport("?Destruct@ImageBuffer@Video@@UEAAXXZ",
                          (void*)&Video_ImageBuffer_Destruct);
    RegisterRuntimeAddr(0x18001eab0, (void*)&Video_ImageBuffer_Destruct);
    n++;

    RegisterRuntimeExport("?Construct@ImageBuffer@Video@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&Video_ImageBuffer_Construct);
    RegisterRuntimeAddr(0x18001eaa0, (void*)&Video_ImageBuffer_Construct);
    n++;

    return n;
}
