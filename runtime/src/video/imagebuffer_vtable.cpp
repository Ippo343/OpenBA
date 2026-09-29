// ============================================================================
// runtime/src/video/imagebuffer_vtable.cpp — Wave 18 lane VDATA
// The seven slots of ??_7ImageBuffer@Video@@6B@ (DLL 0x180154fc0) this file owns
// ============================================================================
//
// ENDGAME B.3.5 block C. `Video::ImageBuffer` is now a REAL C++ class
// (runtime/types/video/ImageBuffer.h) and the seven bodies below have MOVED INTO
// members of it; each registered door is a one-line delegation with a QUALIFIED
// non-virtual call, so no registration, address or mangled name changed.
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
// (`reference_aeaa_is_private_not_unexported.md`).
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
// WHAT THE B.3.5 CLASS CONVERSION CHANGED
// ----------------------------------------------------------------------------
//   * `Serialize` uses the REAL `IO::Serializer` (B.3.4 block 3) and its
//     `Serialize(T&, const char*)` overloads, so this file no longer carries its
//     own copy of the encoding-factory prologue (`MakeCodec`), the codec
//     teardown (`DestroyCodec`) or the two `rw_vtbl::k*` byte offsets. The
//     Serializer local's SCOPE is what places construction and destruction at
//     retail's positions — the prologue before the first emission, the teardown
//     AFTER `PlatformSerialize` — because moving either relative to another
//     stream operation is a save-format change (Serializer.h's banner).
//   * `PlatformSerialize` is a member of the class now (its body is
//     `imagebuffer_platform_serialize.cpp`, also this block's), so the
//     `RT_EXPORT_ONCE` name door it was reached through is gone. That is the
//     ONE binding in this file that could be retired: the other two —
//     `??0ImageBuffer` (body in `buffers.cpp`, block D2's) and
//     `?Serialize@String@System@@` — keep their doors, because the door is what
//     decides which body runs while the body still lives behind one.
//   * The two dead `using Fn = …` typedefs in `Type` and `GetFileVersion`, left
//     over from a differential oracle that no longer exists, are gone with the
//     two `-Wunused-local-typedefs` warnings they carried.
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
//       running theirs afterwards would double-destruct it.
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

#include "../../types/video/ImageBuffer.h"
#include "../../types/io/Serializer.h"
#include "../../types/io/Stream.h"
#include "../../types/system/String.h"

// The door over `Video::ImageBuffer`'s protected surface. `?PlatformSerialize@`
// is PRIVATE in retail (`AEAA`) and `protected` on the class, yet it is exported
// and keeps its registered door in `imagebuffer_platform_serialize.cpp`; this
// file also has to CALL it from `Serialize`, and neither an `extern "C"` function
// nor an anonymous-namespace helper can name a protected member. B34_BRIEFS A.4:
// friend the holder rather than widen the access, so the header keeps saying what
// retail says. It has no fields and is never instantiated. A DERIVED helper would
// not work — deriving forces GCC to reference `vtable for Video::ImageBuffer`,
// whose slot 0 is a `deleting_dtor.cpp` table row defined in no translation unit.
struct ImageBufferDoorAccess {
    static void PlatformSerialize(Video::ImageBuffer* p, IO::Serializer& ser) {
        p->Video::ImageBuffer::PlatformSerialize(ser);
    }
};

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
// ENDGAME B.3.9 TI-REST: that static is the class's own `DEFINE_TYPE` record
// (video_type_records.cpp); slots 1 and 2 and TypeId() are DECLARE_TYPE's.

// `??0ImageBuffer@Video@@QEAA@XZ` — retail 0x1800f2580, PORTED by Wave 6 lane C
// (`buffers.cpp`, registered by BOTH name and address). `buffers.cpp` belongs to
// ENDGAME B.3.5 block D2, so the body is still behind a door and STAYS behind
// one until that block converts it.
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
// ENDGAME B.3.9 VS: the door, the by-address lookup and its function-local
// cache are GONE. Construct / Reconstruct run the real constructor in place,
// `::new (this) ImageBuffer()` -- retail's `JMP 0x1800f2580`, which stores
// ImageBuffer's own table exactly as the compiler's constructor does.

// (B3X W0: `CallSlot0`, which read slot 0 through the object's vptr, is gone.
// Every live ImageBuffer wears the compiler's table since ENDGAME B.3.9 VS, so
// `~T()` (slot 0, flags 0) -- unqualified, VIRTUAL -- reaches a derived class's own
// slot 0 (`Graphics::ReflectionImageBuffer`, `FrameCopyImageBuffer`,
// `GeneratedImageBuffer` inherit `Destruct`/`Reconstruct`) exactly as it did.)

}  // namespace

// ############################################################################
// The members
// ############################################################################

namespace Video {

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
// (DECLARE_TYPE's `Type()` / `TypeId()`; retail never touches RCX.)

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
// (DECLARE_TYPE's `TypeName()`. The two literals, COPIED from B35_SLOTS section
// 3 (0x18001e8c0 / 0x18001e8d0), are DEFINE_TYPE's in video_type_records.cpp.)

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
int32_t ImageBuffer::GetFileVersion() const { return 7; }

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
// ORACLE: shadow post-condition. No replay — a second Destruct on the same
// object is a double destruction. The one term is the vptr, which ~ImageBuffer
// MOVES to `??_7CompiledExternal@Object@@6B@` (buffers.cpp, transcribed from
// 0x1800f2610); the term is written by the destructor, not by this function's
// own arithmetic, which is what stops the shadow from restating the code above
// it. A predicate that cannot fail is not a check
// (`reference_a_predicate_nobody_evaluates.md`).
// ============================================================================
void ImageBuffer::Destruct() { this->~ImageBuffer(); }

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
// It lives in the DOOR (`Video_ImageBuffer_Construct` below) rather than here,
// because `this == nullptr` inside a member is a comparison GCC folds and warns
// about, and the door is where retail's test is observable.
//
// The return value is `this`: the ctor at 0x1800f2580 ends `MOV RAX,RBX; RET`
// (read at 0x1800f25ff), and this body TAIL JUMPS to it, so RAX at return is
// the constructed object. `CtorFn` is DELIBERATELY typed `void` and the value
// is supplied here explicitly, because the registry may hand back either
// retail's body (which returns `this`) or our port (which returns `void`, so
// RAX would be whatever was left in it). Declaring a return the callee may not
// have written and then using it is `reference_hidden_retslot_wild_write.md`'s
// family of bug; returning `this` ourselves is correct against both.
//
// ORACLE: shadow post-condition, on the constants the CTOR writes. No replay:
// constructing twice over one object leaks its platform data.
// ============================================================================
Object::External* ImageBuffer::Construct() {
    ::new (this) ImageBuffer();
    return this;
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
Object::External* ImageBuffer::Reconstruct() {
    this->~ImageBuffer();
    ::new (this) ImageBuffer();
    return this;
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
// bytes say. With the real `IO::Serializer` the distinction is carried by the
// OVERLOAD chosen — `Serialize(int&)` reaches +0xa0 and `Serialize(unsigned
// int&)` reaches +0x98 — which is why the eight unsigned emissions cast through
// `uint32_t&` on fields the class declares as `int32_t`, exactly as retail does
// by picking the other slot for the same address.
//
// ORACLE: see the file banner — emission prefix oracle, stop at 10.
// ============================================================================
bool ImageBuffer::Serialize(void* stream) {
    // B.3.5 bridge: the slot's parameter is `IO::Stream&` in retail, but
    // `Object::External::Serialize` is declared `bool(void*)` across the whole
    // External family and that signature is not this block's to change.
    IO::Stream& s = *reinterpret_cast<IO::Stream*>(stream);
    {
        // Version 0: retail zeroes R8D at 0x1800f2714 for the `CMP [RDX+8],R8D`
        // and never reloads it before the factory call at 0x1800f2725/0x1800f272a.
        IO::Serializer ser(s, 0);

        ser.Serialize(_width, nullptr);                                     // +0x20
        ser.Serialize(_height, nullptr);                                    // +0x24
        ser.Serialize(reinterpret_cast<uint32_t&>(_dimension), nullptr);    // +0x28
        ser.Serialize(reinterpret_cast<uint32_t&>(_kind), nullptr);         // +0x2c
        ser.Serialize(reinterpret_cast<uint32_t&>(_format), nullptr);       // +0x30
        ser.Serialize(_flags, nullptr);                                     // +0x34
        ser.Serialize(reinterpret_cast<uint32_t&>(_addressU), nullptr);     // +0x38
        ser.Serialize(reinterpret_cast<uint32_t&>(_addressV), nullptr);     // +0x3c
        ser.Serialize(reinterpret_cast<uint32_t&>(_addressW), nullptr);     // +0x40
        ser.Serialize(reinterpret_cast<uint32_t&>(_filter), nullptr);       // +0x44

        // +0x48, a System::String (0x1800c9930). B.3.x bridge: the field is a void*.
        reinterpret_cast<System::String*>(&_sourceName)->Serialize(&ser);
        ImageBufferDoorAccess::PlatformSerialize(this, ser);
    }   // ~Serializer: `if (rw) rw->slot0(rw, 1)` at 0x1800f2820 .. 0x1800f2832
    return true;
}

}  // namespace Video
