// ============================================================================
// runtime/src/video/companion_data.cpp — Wave 9 lane B2
//
//   the twelve Video::*Data companion base classes (ctor + dtor),
//   Video::IndexBuffer::Lock/Unlock, Video::VertexBuffer::Lock/Unlock,
//   Video::ConstantBuffer::Bind, and Video::Device::DisplayMode's ctor/dtor.
//
// 29 functions, closing 16 classes. Layouts and the evidence for every offset
// are in runtime/types/video/CompanionData.h and runtime/types/video/Device.h.
// The lane report is docs/runtime_port/WAVE9_LANE_B2.md.
// ============================================================================
//
// WHY THESE, AND WHY TOGETHER. Every function in this file is reached from
// `dx11/`, our VideoDX11 replacement, and from nowhere else in the tree.
// WAVE9.md §4 records that `dx11/` is part of the cutover surface and was
// invisible to the census until 2026-09-07; these 29 are the cheapest part of
// that surface. Sixteen classes go to 100% in one file, which is what PLAN
// §4.1 decision 5 (class atomicity) asks for: an object built by our ctor
// carries our vtable and must then find every method ours.
//
// EVERY BODY BELOW WAS READ AS DISASSEMBLY, NOT DECOMPILED C
// (reference_ghidra_c_output_vs_disassembly.md). The twelve ctors and twelve
// dtors are three and four instructions each, and all 24 were disassembled
// INDIVIDUALLY rather than one being read and the rest assumed — the point of
// checking was the vtable operand, which differs per class and is the only
// thing that can be wrong. The table:
//
//   | Class              | ctor        | dtor        | ??_7 (LEA operand, both) |
//   |--------------------|-------------|-------------|--------------------------|
//   | RenderBufferData   | 0x1800f8b70 | 0x1800f8db0 | 0x180158a28              |
//   | VideoProgramData   | 0x1800f8b90 | 0x1800f8d90 | 0x180158a48              |
//   | RenderStateData    | 0x1800f8bb0 | (ported)    | 0x180158a60              |
//   | PixelProgramData   | 0x1800f8bd0 | 0x1800f8dd0 | 0x180158a78              |
//   | VertexProgramData  | 0x1800f8bf0 | 0x1800f8d10 | 0x180158a90              |
//   | VertexLayoutData   | 0x1800f8c10 | 0x1800f8d30 | 0x180158aa8              |
//   | VertexBufferData   | 0x1800f8c30 | 0x1800f8d50 | 0x180158ac0              |
//   | IndexBufferData    | 0x1800f8c50 | 0x1800f8df0 | 0x180158af8              |
//   | ConstantBufferData | 0x1800f8c70 | 0x1800f8e50 | 0x180158b30              |
//   | ImageBufferData    | 0x1800f8c90 | (ported)    | 0x180158b50              |
//   | ContextData        | 0x1800f8cb0 | 0x1800f8e30 | 0x180158b90              |
//   | DeviceData         | 0x1800f8cd0 | 0x1800f8cf0 | 0x180158c30              |
//
// The two "(ported)" dtors are NOT re-registered here — `??1ImageBufferData`
// landed in buffers.cpp (Wave 6 lane C) and `??1RenderStateData` in device.cpp
// (Wave 6 lane B). Registering a name twice is not an error the linker or the
// registry reports: the later binding silently wins and the earlier line
// becomes dead code, which is the SHADOWED case `tools/dispatch_slot_map.py
// --check` reports on the exe side. Those two classes complete here on their
// ctor alone.
//
// ALL 24 MANGLED NAMES WERE READ OUT OF THE SHIPPED EXPORT DIRECTORY, not
// reconstructed by hand (`tools/pe_exports.py` over
// C:\GOG Games\Banished\Runtime-x64.dll). That matters more than usual here:
// the dtors are `UEAA` (virtual) and DisplayMode's is `QEAA` (non-virtual),
// and reference_phantom_dtor_mangling_qeaa_vs_ueaa.md is exactly the bug of
// guessing the wrong one — a name the DLL does not contain resolves NULL
// silently, the port registers, the census counts it and the diff reports
// calls=0.
//
// ORACLE. Full differential on 25 of the 29, and it is a real one rather than
// a tautology. Our ctor writes the vtable pointer this file resolves by
// exported name, where retail writes an absolute LEA; those agree only if the
// export name is right and resolves, which is the phantom-name failure mode
// above, caught here rather than at cutover.
// Retail's ctor is replayed into a PRIVATE 16-byte scratch (never into the
// live object) and the two are compared byte for byte. This is sound because
// the ctor is a pure write of two fields: it reads no prior contents of
// `self`, so ours having already run cannot change what theirs produces, and
// its only input is the scalar `owner`, which nothing aliases —
// reference_oracle_must_snapshot_aliased_inputs.md's trap (a call site passing
// one buffer as both `self` and `ret`) cannot arise. The dtors replay the same
// way into a bare `void*`, following device.cpp's `??1RenderStateData` port.
//
// The remaining four are trampolines (Lock/Unlock/Bind, DisplayMode's dtor)
// and carry the diff oracle with the reason stated at each one.
//
// NEW LANE-D ROW: exactly one, `0x180010760`, named below.
// ============================================================================

#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/video/Buffers.h"
#include "../../types/video/CompanionData.h"
// ENDGAME B.3.5 block D1 — `Video::Context`, for the one line
// `Video::ConstantBuffer::Bind` takes it by reference in.
#include "../../types/video/Context.h"
#include "../../types/video/Device.h"

using RtVideo::CompanionData;
using RtVideo::ConstantBuffer;
using RtVideo::DisplayMode;

// ----------------------------------------------------------------------------
// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// — byte-identical to the declaration in resolve.h and in
// runtime/src/graphics/scene.cpp, for the reason rt_port.h gives: resolve.h
// drags in <windows.h>, whose `Rectangle`, `near` and `far` collide with the
// engine's own names.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

using CompanionCtorFn = void* (*)(void*, void*);
using CompanionDtorFn = void  (*)(void*);

// ----------------------------------------------------------------------------
// The twelve exported base vtables, one accessor each, resolved once.
//
// SPELLED OUT RATHER THAN BUILT BY THE MACRO, deliberately. The obvious
// version pastes the class name into the mangled string inside the ctor macro
// — and it works, and `tools/export_name_audit.py` then cannot see a
// single one of these twelve names, because it scans SOURCE for a literal
// argument and stringization hides them. That is exactly the shape of
// reference_census_cannot_see_nonliteral_registrations.md, where twelve live
// ports were invisible to the census because the name was a named constant.
// Twelve one-line functions cost nothing and keep the audit able to do its job.
//
// The accessors follow buffers.cpp's existing ImageBufferDataVtable() idiom.
// ----------------------------------------------------------------------------
void* Vt_RenderBufferData()   { static void* v = ResolveExport("??_7RenderBufferData@Video@@6B@");   return v; }
void* Vt_VideoProgramData()   { static void* v = ResolveExport("??_7VideoProgramData@Video@@6B@");   return v; }
void* Vt_RenderStateData()    { static void* v = ResolveExport("??_7RenderStateData@Video@@6B@");    return v; }
void* Vt_PixelProgramData()   { static void* v = ResolveExport("??_7PixelProgramData@Video@@6B@");   return v; }
void* Vt_VertexProgramData()  { static void* v = ResolveExport("??_7VertexProgramData@Video@@6B@");  return v; }
void* Vt_VertexLayoutData()   { static void* v = ResolveExport("??_7VertexLayoutData@Video@@6B@");   return v; }
void* Vt_VertexBufferData()   { static void* v = ResolveExport("??_7VertexBufferData@Video@@6B@");   return v; }
void* Vt_IndexBufferData()    { static void* v = ResolveExport("??_7IndexBufferData@Video@@6B@");    return v; }
void* Vt_ConstantBufferData() { static void* v = ResolveExport("??_7ConstantBufferData@Video@@6B@"); return v; }
void* Vt_ImageBufferData()    { static void* v = ResolveExport("??_7ImageBufferData@Video@@6B@");    return v; }
void* Vt_ContextData()        { static void* v = ResolveExport("??_7ContextData@Video@@6B@");        return v; }
void* Vt_DeviceData()         { static void* v = ResolveExport("??_7DeviceData@Video@@6B@");         return v; }

}  // namespace

// ============================================================================
// The twelve companion-base constructors.
//
//   1800f8cd0  LEA RAX,[0x180158c30]      ; ??_7DeviceData@Video@@6B@
//   1800f8cd7  MOV qword ptr [RCX+0x8],RDX
//   1800f8cdb  MOV qword ptr [RCX],RAX
//   1800f8cde  MOV RAX,RCX
//   1800f8ce1  RET
//
// Note retail's store ORDER: owner first, then the vtable. Reproduced here
// even though nothing can observe the difference in a single-threaded ctor —
// a port that is faithful only where it is observable is a port whose
// faithfulness nobody can check.
//
// The macro emits one function per class rather than one shared body with a
// vtable parameter, deliberately: each needs its own RT_DIFF site so the
// summary says which of the twelve diverged, and each resolves its own
// exported vtable, which is the thing under test.
// ============================================================================
#define RT_COMPANION_CTOR(CLASS, OWNER)                                        \
    void* Video_##CLASS##_Ctor(CompanionData* self, void* owner) {             \
                           \
        self->owner  = owner;                                                  \
        self->vtable = Vt_##CLASS();                                           \
                                                                      \
        return self;                                                           \
    }

RT_COMPANION_CTOR(RenderBufferData,   RenderBuffer)
RT_COMPANION_CTOR(VideoProgramData,   VideoProgram)
RT_COMPANION_CTOR(RenderStateData,    RenderState)
RT_COMPANION_CTOR(PixelProgramData,   PixelProgram)
RT_COMPANION_CTOR(VertexProgramData,  VertexProgram)
RT_COMPANION_CTOR(VertexLayoutData,   VertexLayout)
RT_COMPANION_CTOR(VertexBufferData,   VertexBuffer)
RT_COMPANION_CTOR(IndexBufferData,    IndexBuffer)
RT_COMPANION_CTOR(ConstantBufferData, ConstantBuffer)
RT_COMPANION_CTOR(ImageBufferData,    ImageBuffer)
RT_COMPANION_CTOR(ContextData,        Context)
RT_COMPANION_CTOR(DeviceData,         Device)

#undef RT_COMPANION_CTOR

// ============================================================================
// The ten companion-base destructors (ImageBufferData's and RenderStateData's
// are already ported elsewhere — see the banner).
//
//   1800f8cf0  LEA RAX,[0x180158c30]      ; ??_7DeviceData@Video@@6B@
//   1800f8cf7  MOV qword ptr [RCX],RAX
//   1800f8cfa  RET
//
// The whole body is the base-class vtable re-stamp: the standard MSVC idiom of
// running a dtor body under its OWN class's vtable rather than a derived
// override. There is nothing else to free — these bases own no memory; the
// derived dx11 object's scalar-deleting dtor does the MemFree, and it
// tail-calls this export as its `im->BaseDtor(self)`.
//
// The `owner` field at +0x08 is deliberately LEFT ALONE. Retail does not clear
// it and neither do we — reference_oracle_must_not_compare_unwritten_bytes.md
// is the mirror of this: comparing a field neither side writes is the failure,
// and so is writing one retail leaves.
// ============================================================================
// ENDGAME B.3.5 block A — THE BODY MOVED INTO THE MEMBER. `~XData()` is now a
// real, non-virtual, public destructor on the real abstract class
// (runtime/types/video/CompanionData.h); the twelve member bodies are at the
// bottom of this file, inside `namespace Video`, and each door below is a
// one-line QUALIFIED non-virtual delegation.
//
// WHY THE QUALIFICATION IS LOAD-BEARING HERE TOO (B34_BRIEFS A.1): a live
// `*Data` object wears dx11's vtable, whose slot 0 is the derived scalar
// deleting destructor, which is what CALLS this door. An unqualified virtual
// call would dispatch back into it. `~XData()` is not in any slot, so the
// qualification is what keeps this a base-subobject destructor call and not a
// re-entry — the same reason RULE B5-3 forbids `DeletingDtor(0)` on the dx11
// side.
//
// The receiver is still a `CompanionData*` because the ctor doors above hand
// out that view and dx11 hands us a raw pointer, so each door bridges with a
// `reinterpret_cast` (`// B.3.5 bridge`, which is what the B.4 metric counts).
#define RT_COMPANION_DTOR(CLASS)                                               \
    void Video_##CLASS##_Dtor(CompanionData* self) {                           \
        reinterpret_cast<Video::CLASS*>(self)->Video::CLASS::~CLASS();  /* B.3.5 bridge */ \
    }

RT_COMPANION_DTOR(RenderBufferData)
RT_COMPANION_DTOR(VideoProgramData)
RT_COMPANION_DTOR(PixelProgramData)
RT_COMPANION_DTOR(VertexProgramData)
RT_COMPANION_DTOR(VertexLayoutData)
RT_COMPANION_DTOR(VertexBufferData)
RT_COMPANION_DTOR(IndexBufferData)
RT_COMPANION_DTOR(ConstantBufferData)
RT_COMPANION_DTOR(ContextData)
RT_COMPANION_DTOR(DeviceData)

#undef RT_COMPANION_DTOR

// ============================================================================
// The twelve member bodies — `Video::XData::~XData()`.
//
// This block is at `namespace Video` scope and comes AFTER the `RtVideo`
// using-declarations and the anonymous namespace, per B34_BRIEFS A.2: a member
// of `Video::X` cannot be defined inside a sibling namespace, and the
// anonymous-namespace helpers above are still reachable here by unqualified
// name because they are members of the global namespace's declarative region.
//
// THE WHOLE BODY IS THE BASE-CLASS VPTR RE-STAMP, as retail's is:
//
//   1800f8cf0  LEA RAX,[0x180158c30]      ; ??_7DeviceData@Video@@6B@
//   1800f8cf7  MOV qword ptr [RCX],RAX
//   1800f8cfa  RET
//
// the standard MSVC idiom of running a destructor body under its OWN class's
// vtable rather than a derived override. There is nothing else to free — these
// bases own no memory; the derived dx11 object's scalar deleting destructor
// does the `MemFree`, and it calls this as its base-destructor step.
//
// THE STORE IS WRITTEN OUT BY HAND AND IS NOT LEFT TO THE COMPILER. GCC emits
// its own vptr store at the top of a polymorphic destructor, but it stores
// GCC's vtable for the class — and these twelve objects are expected by the
// rest of the tree, by `standalone/rt_data_mint_tables.inc` and by the
// `RT_DELDTOR` rows to be wearing the MINTED `??_7XData@Video@@6B@` address
// (B35_RULES 11 keeps those rows for the whole of step 5). So the address the
// accessors above resolve is stored explicitly, last, exactly as retail's LEA
// operand does. The per-class operand table is in this file's banner and every
// one of the twelve was checked against its own ctor AND dtor.
//
// The `owner` field at +0x08 is deliberately LEFT ALONE. Retail does not clear
// it and neither do we — reference_oracle_must_not_compare_unwritten_bytes.md
// is the mirror of this: comparing a field neither side writes is the failure,
// and so is writing one retail leaves.
// ============================================================================
namespace Video {

#define RT_COMPANION_DTOR_BODY(CLASS)                                          \
    CLASS::~CLASS() {                                                          \
        *reinterpret_cast<void**>(this) = Vt_##CLASS();                        \
    }

RT_COMPANION_DTOR_BODY(RenderBufferData)
RT_COMPANION_DTOR_BODY(VideoProgramData)
RT_COMPANION_DTOR_BODY(PixelProgramData)
RT_COMPANION_DTOR_BODY(VertexProgramData)
RT_COMPANION_DTOR_BODY(VertexLayoutData)
RT_COMPANION_DTOR_BODY(VertexBufferData)
RT_COMPANION_DTOR_BODY(IndexBufferData)
RT_COMPANION_DTOR_BODY(ConstantBufferData)
RT_COMPANION_DTOR_BODY(ContextData)
RT_COMPANION_DTOR_BODY(DeviceData)

// The remaining two. Their registered DOORS are elsewhere and this block does
// NOT touch them: `??1ImageBufferData` is `Video_ImageBufferData_Dtor` in
// `buffers.cpp` (Wave 6 lane C) and `??1RenderStateData` is in `device.cpp`
// (Wave 6 lane B) — see this file's banner on why registering a name twice is
// silent. The member has to exist for all twelve or `Video::ImageBufferData`
// would be a class whose declared destructor never links, so the bodies live
// here with the other ten and those two doors become one-line delegations when
// block C takes `buffers.cpp` and block D takes `device.cpp`. Until then the
// same three-instruction store exists in two places, which is a duplicate of
// four instructions and not of a decision.
RT_COMPANION_DTOR_BODY(ImageBufferData)
RT_COMPANION_DTOR_BODY(RenderStateData)

#undef RT_COMPANION_DTOR_BODY

}  // namespace Video

// ============================================================================
// Video::IndexBuffer::Lock / Unlock  —  0x1800f34e0 / 0x1800f34c0
// Video::VertexBuffer::Lock / Unlock —  0x1800f5640 / 0x1800f5620
//
//   1800f34e0  MOV RCX,qword ptr [RCX+0x8]   ; this->platformData
//   1800f34e4  MOV RAX,qword ptr [RCX]       ; its vtable
//   1800f34e7  JMP qword ptr [RAX+0x18]      ; slot 3
//
// Unlock is the same three instructions through slot +0x20 (slot 4). All four
// are pure tail-call trampolines into the platform buffer object at +0x08 —
// dx11::IndexBufferData / VertexBufferData, dx11/types/VideoDX11Data.h — and
// the index/vertex pair use identical slots, which is why one comment covers
// both. This completes the shape Wave 6 lane C started: Create and Set are
// already ported here through slots +0x08 and +0x10 (buffers.cpp).
//
// ORACLE: the diff oracle, not a replay, and the reason is not caution. The far
// side of the dispatch is dx11's vtable, and dx11 is ALREADY OURS — there is
// no second implementation to diverge from. A replay would additionally be
// unsound: Lock/Unlock are a matched pair over a mapped GPU buffer, and
// calling Lock twice or Unlock on an already-unlocked buffer is exactly the
// mutate-twice trap WAVE9.md describes for Audio::Scene::Pause.
// ============================================================================
// ENDGAME B.3.5 block VC — A FRAGMENT THE BLOCKS MISSED BETWEEN THEM. These four
// are `Video::{Index,Vertex}Buffer`'s members, declared in
// `runtime/types/video/Buffers.h` (block D2's header, where they still read
// "NOT PORTED"); the BODIES have been in this file, block A's, since Wave 6 lane
// C. Neither block owned the edge, so all four stayed doors with the logic in
// them while their class was real. They are members now, at `namespace Video`
// scope below, and the four doors are one-line QUALIFIED non-virtual delegations.
//
// The hand-inlined `+0x18` / `+0x20` dispatches went with the move: they are
// slots 3 and 4 of block A's own real `Video::{Index,Vertex}BufferData`, which
// `CompanionData.h` names `Lock` and `Unlock` — the same two byte offsets,
// spelled as the slots they are, with the index checked by the compiler. Both
// companion classes declare an identical pair, which is why retail shares one
// body per slot between the two owners.
//
// `outByteOffset` is an `int&` in retail's mangling (`AEAH`), so the member takes
// a reference and the door keeps the pointer the registered ABI passes — the
// pointer-to-reference step moved into the door, where the ABI is, exactly as
// `ImageBuffer::SaveImageToFile`'s `Filename&` did.
void* Video_IndexBuffer_Lock(IndexBuffer* self, int count, int* outOffset) {
    return self->Video::IndexBuffer::Lock(count, *outOffset);
}

void Video_IndexBuffer_Unlock(IndexBuffer* self) {
    self->Video::IndexBuffer::Unlock();
}

void* Video_VertexBuffer_Lock(VertexBuffer* self, int count, int* outOffset) {
    return self->Video::VertexBuffer::Lock(count, *outOffset);
}

void Video_VertexBuffer_Unlock(VertexBuffer* self) {
    self->Video::VertexBuffer::Unlock();
}

namespace Video {

void* IndexBuffer::Lock(int sizeBytes, int& outByteOffset) {
    // B.3.5 bridge — platformData is a pinned void* field at +0x08.
    return reinterpret_cast<Video::IndexBufferData*>(platformData)
               ->Lock(sizeBytes, &outByteOffset);
}

void IndexBuffer::Unlock() {
    // B.3.5 bridge — platformData is a pinned void* field at +0x08.
    reinterpret_cast<Video::IndexBufferData*>(platformData)->Unlock();
}

void* VertexBuffer::Lock(int sizeBytes, int& outByteOffset) {
    // B.3.5 bridge — platformData is a pinned void* field at +0x08.
    return reinterpret_cast<Video::VertexBufferData*>(platformData)
               ->Lock(sizeBytes, &outByteOffset);
}

void VertexBuffer::Unlock() {
    // B.3.5 bridge — platformData is a pinned void* field at +0x08.
    reinterpret_cast<Video::VertexBufferData*>(platformData)->Unlock();
}

}  // namespace Video

// ============================================================================
// Video::ConstantBuffer::Bind  —  0x1800eef70
//
//   1800eef70  MOV RCX,qword ptr [RCX+0x40]  ; this->platformData
//   1800eef74  MOV R10,qword ptr [RCX]
//   1800eef77  JMP qword ptr [R10+0x10]      ; slot 2
//
// Same trampoline shape as the four above, through +0x40 rather than +0x08
// (the field the ConstantBuffer ctor at 0x1800eedf0 fills from the Device
// factory — see CompanionData.h). Five arguments, so the fifth arrives on the
// stack; retail's tail JMP preserves the frame and so does an ordinary call
// here, since the signature matches exactly.
// ============================================================================
// ENDGAME B.3.5 block D1: the BODY moved onto `Video::ConstantBuffer::Bind`
// (runtime/src/video/constant_buffer.cpp), because `Video::ConstantBuffer` is
// block D1's class and the member belongs with the rest of it. This door keeps
// its name and BOTH its registrations below — nothing moved out of this file but
// the four lines of body — and is now a one-line QUALIFIED non-virtual call.
void Video_ConstantBuffer_Bind(ConstantBuffer* self, void* context, void* data,
                               int first, int count) {
    self->Video::ConstantBuffer::Bind(*static_cast<Video::Context*>(context),
                                      data, first, count);
}

// ============================================================================
// Video::Device::DisplayMode::DisplayMode(int, int)  —  0x18000d300
//
//   18000d311  MOV dword ptr [RCX],EDX        ; width
//   18000d313  MOV dword ptr [RCX+0x4],R8D    ; height
//   18000d317  XOR EAX,EAX
//   18000d319  MOV qword ptr [RCX+0x8],RAX    ; Array<RefreshRate>::_data
//   18000d31d  MOV qword ptr [RCX+0x10],RAX   ; _size, _capacity (one qword)
//
// (The `MOV qword ptr [RSP],-2` at 18000d309 is the SEH unwind cookie, not a
// field write.) Pure constants and two scalar parameters: a full replay into a
// private scratch DisplayMode is sound, and unlike the companion ctors this
// one is worth the whole 0x18 bytes because the two zeroing stores span three
// named fields.
// ============================================================================
void* Video_DisplayMode_Ctor(DisplayMode* self, int width, int height) {
    self->width           = width;
    self->height          = height;
    self->refreshData     = nullptr;
    self->refreshCount    = 0;
    self->refreshCapacity = 0;
    return self;
}

// ============================================================================
// Video::Device::DisplayMode::~DisplayMode  —  0x18000cb80
//
//   18000cb92  ADD RCX,0x8
//   18000cb9a  JMP 0x180010760
//
// The entire body is a tail call to the `Collection::Array<T>::~Array`
// instantiation for `Array<RefreshRate>`: free `_data` under the allocator's
// critical section, then zero the {data,size,capacity} triple.
//
// *** THIS PORT CREATES ONE NEW LANE-D ROW: 0x180010760. *** It is the twelfth
// member of the J1 family lane Q ported (runtime/src/collection/array.cpp's
// J1_ENTRY list) — byte-identical shape to the eleven there, including
// 0x18001c750 — but it is NOT one of the eleven and is NOT registered, so this
// call reaches the shipped DLL. Called by ADDRESS, the same posture ClearAll,
// KeyMap::RegisterInput and Audio::Scene::Serialize already take toward
// Collection internals: what is needed from it is an entry point, not a body.
// Adding it to lane Q's J1 list is a one-line change and the obvious follow-up,
// but that file belongs to another lane this round and a registration race is
// not worth one line.
//
// ORACLE: the diff oracle. A replay is not merely unhelpful here, it is a
// DOUBLE FREE — ours frees `_data`, and retail's body run afterwards on a copy
// of the pre-state would pass the same pointer to MemoryAllocator::Free again.
// Run on the post-state it takes the null branch and only re-zeroes, which
// compares nothing and reports green: the Audio::Scene::Pause early-out trap
// (WAVE9.md rows 5-7) in its exact shape. The shadow asserts what our body can
// honestly assert — that the triple came back zeroed.
// ============================================================================
void Video_DisplayMode_Dtor(DisplayMode* self) {
    using Fn = void (*)(void*);
    static Fn freeRefreshArray =
        reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180010760));
    freeRefreshArray(&self->refreshData);
}

// ============================================================================
// Registration
//
// By NAME and by ADDRESS for every function. dx11 resolves these by name
// through Dx11ResolveRuntimeImport / ResolveExport, both of which consult the
// port registry first, so the name registration is what flips those call
// sites. The address registration covers by-address callers inside our own
// tree — WAVE3_LANE_A.md §6's lesson, and the one lane R re-learned this round
// when runtime/src/graphics/scene.cpp reached a ported function by address and
// walked past the port into the DLL.
// ============================================================================
int RtVideoCompanionRegister() {
    int n = 0;

    // The twenty-two names below are written out in full rather than pasted
    // together by a macro, for the same reason the vtable accessors above are:
    // tools/runtime_census.py reads a RegisterRuntimeExport LITERAL, and a
    // stringized macro argument is invisible to it. The first draft of this file
    // used a macro and the census reported two ports named "??0" and "??1" with
    // no caller — reference_census_cannot_see_nonliteral_registrations.md,
    // reproduced live in the same file that cites it.

    RegisterRuntimeExport("??0RenderBufferData@Video@@QEAA@PEAVRenderBuffer@1@@Z",
                          (void*)&Video_RenderBufferData_Ctor);
    RegisterRuntimeAddr(0x1800f8b70, (void*)&Video_RenderBufferData_Ctor);
    n++;

    RegisterRuntimeExport("??0VideoProgramData@Video@@QEAA@PEAVVideoProgram@1@@Z",
                          (void*)&Video_VideoProgramData_Ctor);
    RegisterRuntimeAddr(0x1800f8b90, (void*)&Video_VideoProgramData_Ctor);
    n++;

    RegisterRuntimeExport("??0RenderStateData@Video@@QEAA@PEAVRenderState@1@@Z",
                          (void*)&Video_RenderStateData_Ctor);
    RegisterRuntimeAddr(0x1800f8bb0, (void*)&Video_RenderStateData_Ctor);
    n++;

    RegisterRuntimeExport("??0PixelProgramData@Video@@QEAA@PEAVPixelProgram@1@@Z",
                          (void*)&Video_PixelProgramData_Ctor);
    RegisterRuntimeAddr(0x1800f8bd0, (void*)&Video_PixelProgramData_Ctor);
    n++;

    RegisterRuntimeExport("??0VertexProgramData@Video@@QEAA@PEAVVertexProgram@1@@Z",
                          (void*)&Video_VertexProgramData_Ctor);
    RegisterRuntimeAddr(0x1800f8bf0, (void*)&Video_VertexProgramData_Ctor);
    n++;

    RegisterRuntimeExport("??0VertexLayoutData@Video@@QEAA@PEAVVertexLayout@1@@Z",
                          (void*)&Video_VertexLayoutData_Ctor);
    RegisterRuntimeAddr(0x1800f8c10, (void*)&Video_VertexLayoutData_Ctor);
    n++;

    RegisterRuntimeExport("??0VertexBufferData@Video@@QEAA@PEAVVertexBuffer@1@@Z",
                          (void*)&Video_VertexBufferData_Ctor);
    RegisterRuntimeAddr(0x1800f8c30, (void*)&Video_VertexBufferData_Ctor);
    n++;

    RegisterRuntimeExport("??0IndexBufferData@Video@@QEAA@PEAVIndexBuffer@1@@Z",
                          (void*)&Video_IndexBufferData_Ctor);
    RegisterRuntimeAddr(0x1800f8c50, (void*)&Video_IndexBufferData_Ctor);
    n++;

    RegisterRuntimeExport("??0ConstantBufferData@Video@@QEAA@PEAVConstantBuffer@1@@Z",
                          (void*)&Video_ConstantBufferData_Ctor);
    RegisterRuntimeAddr(0x1800f8c70, (void*)&Video_ConstantBufferData_Ctor);
    n++;

    RegisterRuntimeExport("??0ImageBufferData@Video@@QEAA@PEAVImageBuffer@1@@Z",
                          (void*)&Video_ImageBufferData_Ctor);
    RegisterRuntimeAddr(0x1800f8c90, (void*)&Video_ImageBufferData_Ctor);
    n++;

    RegisterRuntimeExport("??0ContextData@Video@@QEAA@PEAVContext@1@@Z",
                          (void*)&Video_ContextData_Ctor);
    RegisterRuntimeAddr(0x1800f8cb0, (void*)&Video_ContextData_Ctor);
    n++;

    RegisterRuntimeExport("??0DeviceData@Video@@QEAA@PEAVDevice@1@@Z",
                          (void*)&Video_DeviceData_Ctor);
    RegisterRuntimeAddr(0x1800f8cd0, (void*)&Video_DeviceData_Ctor);
    n++;

    RegisterRuntimeExport("??1DeviceData@Video@@UEAA@XZ",
                          (void*)&Video_DeviceData_Dtor);
    RegisterRuntimeAddr(0x1800f8cf0, (void*)&Video_DeviceData_Dtor);
    n++;

    RegisterRuntimeExport("??1VertexProgramData@Video@@UEAA@XZ",
                          (void*)&Video_VertexProgramData_Dtor);
    RegisterRuntimeAddr(0x1800f8d10, (void*)&Video_VertexProgramData_Dtor);
    n++;

    RegisterRuntimeExport("??1VertexLayoutData@Video@@UEAA@XZ",
                          (void*)&Video_VertexLayoutData_Dtor);
    RegisterRuntimeAddr(0x1800f8d30, (void*)&Video_VertexLayoutData_Dtor);
    n++;

    RegisterRuntimeExport("??1VertexBufferData@Video@@UEAA@XZ",
                          (void*)&Video_VertexBufferData_Dtor);
    RegisterRuntimeAddr(0x1800f8d50, (void*)&Video_VertexBufferData_Dtor);
    n++;

    RegisterRuntimeExport("??1VideoProgramData@Video@@UEAA@XZ",
                          (void*)&Video_VideoProgramData_Dtor);
    RegisterRuntimeAddr(0x1800f8d90, (void*)&Video_VideoProgramData_Dtor);
    n++;

    RegisterRuntimeExport("??1RenderBufferData@Video@@UEAA@XZ",
                          (void*)&Video_RenderBufferData_Dtor);
    RegisterRuntimeAddr(0x1800f8db0, (void*)&Video_RenderBufferData_Dtor);
    n++;

    RegisterRuntimeExport("??1PixelProgramData@Video@@UEAA@XZ",
                          (void*)&Video_PixelProgramData_Dtor);
    RegisterRuntimeAddr(0x1800f8dd0, (void*)&Video_PixelProgramData_Dtor);
    n++;

    RegisterRuntimeExport("??1IndexBufferData@Video@@UEAA@XZ",
                          (void*)&Video_IndexBufferData_Dtor);
    RegisterRuntimeAddr(0x1800f8df0, (void*)&Video_IndexBufferData_Dtor);
    n++;

    RegisterRuntimeExport("??1ContextData@Video@@UEAA@XZ",
                          (void*)&Video_ContextData_Dtor);
    RegisterRuntimeAddr(0x1800f8e30, (void*)&Video_ContextData_Dtor);
    n++;

    RegisterRuntimeExport("??1ConstantBufferData@Video@@UEAA@XZ",
                          (void*)&Video_ConstantBufferData_Dtor);
    RegisterRuntimeAddr(0x1800f8e50, (void*)&Video_ConstantBufferData_Dtor);
    n++;


    RegisterRuntimeExport("?Lock@IndexBuffer@Video@@QEAAPEAXHAEAH@Z",
                          (void*)&Video_IndexBuffer_Lock);
    RegisterRuntimeAddr(0x1800f34e0, (void*)&Video_IndexBuffer_Lock);
    n++;

    RegisterRuntimeExport("?Unlock@IndexBuffer@Video@@QEAAXXZ",
                          (void*)&Video_IndexBuffer_Unlock);
    RegisterRuntimeAddr(0x1800f34c0, (void*)&Video_IndexBuffer_Unlock);
    n++;

    RegisterRuntimeExport("?Lock@VertexBuffer@Video@@QEAAPEAXHAEAH@Z",
                          (void*)&Video_VertexBuffer_Lock);
    RegisterRuntimeAddr(0x1800f5640, (void*)&Video_VertexBuffer_Lock);
    n++;

    RegisterRuntimeExport("?Unlock@VertexBuffer@Video@@QEAAXXZ",
                          (void*)&Video_VertexBuffer_Unlock);
    RegisterRuntimeAddr(0x1800f5620, (void*)&Video_VertexBuffer_Unlock);
    n++;

    RegisterRuntimeExport("?Bind@ConstantBuffer@Video@@QEAAXAEAVContext@2@PEAXHH@Z",
                          (void*)&Video_ConstantBuffer_Bind);
    RegisterRuntimeAddr(0x1800eef70, (void*)&Video_ConstantBuffer_Bind);
    n++;

    RegisterRuntimeExport("??0DisplayMode@Device@Video@@QEAA@HH@Z",
                          (void*)&Video_DisplayMode_Ctor);
    RegisterRuntimeAddr(0x18000d300, (void*)&Video_DisplayMode_Ctor);
    n++;

    RegisterRuntimeExport("??1DisplayMode@Device@Video@@QEAA@XZ",
                          (void*)&Video_DisplayMode_Dtor);
    RegisterRuntimeAddr(0x18000cb80, (void*)&Video_DisplayMode_Dtor);
    n++;

    return n;
}
