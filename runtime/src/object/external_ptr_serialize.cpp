// ============================================================================
// runtime/src/object/external_ptr_serialize.cpp — Wave 10, lane EXTPTR
//   `Object::ExternalPtr<T>::Serialize` — ONE 748-byte body, TWENTY-FIVE
//   addresses, all unexported internals.
// ============================================================================
//
// This is the body that blocked `?Serialize@ElementDescription@UI@@` (0x180116b00)
// through two waves. `docs/runtime_port/WAVE10_EXTPTR_SERIALIZE.md` answered the
// gate — the three callees at +0x48/+0x50/+0x58 are one body parameterised by a
// type id — and this file is that recipe executed. The lane write-up, including
// the measurement that extends the family from three to twenty-five, is
// `docs/runtime_port/WAVE10_LANE_EXTPTR.md`.
//
// ---------------------------------------------------------------------------
// WHY THERE ARE TWENTY-FIVE ADDRESSES AND NOT THREE
// ---------------------------------------------------------------------------
// `ExternalPtr<T>` is a template. MSVC emitted one `Serialize` per T and did not
// fold them, because each one loads its own `?_typeInfo@T@…` global. Every copy
// is otherwise the SAME 215 instructions / 748 bytes.
//
// A raw byte-diff cannot see that: every rel32 and every RIP displacement
// encodes a distance FROM THE INSTRUCTION, so two identical bodies at different
// addresses differ in each of those fields (measured: 210 of the first 900
// bytes, 23.3%, which reads like two different functions and is an artefact).
// The position-resolved diff — decode both, and at each difference resolve the
// operand back to the ABSOLUTE address it encodes — reports for EVERY pair:
//
//     identical = 193 instructions
//     position-only = 20      (same absolute target, different distance)
//     genuinely different = 2 (`MOV EDX, dword [rip+…]`, the type-id load,
//                              twice: once per IsDerivedFrom call site)
//
// A masked byte-pattern sweep of `.text` — the reference body with every rel32
// and RIP displacement wildcarded, 165 of 748 bytes — then finds all 25 copies
// and reads each one's type-id global back out. The table is in the lane doc.
//
// So the port is ONE function with the type id as a parameter, and each address
// gets a two-line thunk that supplies its own `BaseTypeInfo`.
//
// ---------------------------------------------------------------------------
// WHAT IS REGISTERED HERE, AND WHAT IS DELIBERATELY NOT
// ---------------------------------------------------------------------------
// FOUR of the twenty-five:
//
//   0x180038f90  ExternalPtr<UI::ElementDescription>   ElementDescription+0x48
//   0x180076610  ExternalPtr<UI::Dialog>               ElementDescription+0x50
//   0x1800042a0  ExternalPtr<UI::StringTable>          ElementDescription+0x58
//                                                 and  Achievement::Goal+0x20
//   0x180005dc0  ExternalPtr<Achievement::Goal>        GoalInstance+0x18
//
// The first three are the recipe's three: they are exactly what
// `Serialize@ElementDescription` dispatches, and registering them is what
// unblocks that row. The FOURTH is not in the recipe and is the reason this
// port is verifiable at all — see the ORACLE section below.
//
// The other twenty-one are measured, named and tabulated in the lane doc, and
// they are NOT registered. That is a deliberate scope call, not an oversight:
// they reach Video / Graphics / Audio / Collision2D resource loading (meshes,
// materials, shaders, sounds, fonts), the oracle route below does not cover
// them, and a defect in this body would land on all of them at once. They are a
// one-line-each follow-on for whoever owns those namespaces, and the evidence
// they need is already written down.
//
// ---------------------------------------------------------------------------
// THE ONE DEPENDENCY THAT IS STILL THE DLL'S: ExternalFactory::FindOrLoad
// ---------------------------------------------------------------------------
// `?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z`
// (0x1800bf320) is unported and belongs to lane OBJ1 (`WAVE10_PARTITION.md`).
// This lane did NOT take it: it is another lane's row, and porting it here would
// be the duplicate-registration trap (last binding wins, one body silently
// becomes dead code).
//
// It is reached through `RT_EXPORT_ONCE`, i.e. by EXPORTED NAME through the
// port-aware resolver — NOT by address. That matters twice over:
//   * today it falls through to the shipped DLL, exactly as before;
//   * the moment OBJ1 registers that name, this call site picks their body up
//     with no edit here.
// So this file opens no new address door onto an unported callee, which is the
// [[reference_porting_can_grow_the_cutover_surface]] failure the gate doc
// flagged. At cutover the wall is OBJ1's row and stays OBJ1's row.
//
// Everything else this body calls is already ours: `External::GetPath`,
// `Base::IsDerivedFrom`, `Filename::AdjustDelimiters`, `MemoryAllocator::MemFree`,
// `String::Serialize`, `ExternalFactory::GetReferenceId` and
// `ExternalFactory::GetPathForReferenceId` are all registered by name today.
//
// ---------------------------------------------------------------------------
// THE ORACLE
// ---------------------------------------------------------------------------
// Lane S's emission oracle (`runtime/src/io/save_trace.h`,
// `BANISHED_RT_SAVE_TRACE=1`) on the WRITE path, and it reaches this body
// WITHOUT a new harness, because `runtime/src/achievement/goal_serialize.cpp`
// already probes the two functions that call it:
//
//     Achievement::Goal::Serialize   -> 0x1800042a0 over Goal+0x20
//     GoalInstance::Persist          -> 0x180005dc0 over GoalInstance+0x18
//
// Both probes run OUR outer body against RETAIL's outer body into two recording
// sinks. Registering these two addresses makes the `ours` side call THIS file
// while the `theirs` side keeps calling the DLL's copy — so the comparison
// becomes a real end-to-end test of this port, where before registration it
// compared retail's inner body with itself. That is the difference between an
// oracle and a self-check ([[reference_shadow_marker_is_not_an_oracle]]).
//
// IS THE WRITE PATH REPLAYABLE? RE-MEASURED, NOT INHERITED. The recipe warns
// that `GetReferenceId` can register a NEW id and says to cap the prefix with
// `stopAfter` as `SerializeReferenceTable` does. Read at 0x1800bf100 rather
// than assumed, it does not: the body is `GetPath` -> `Filename` copy ->
// `Set::Find(this+0x10, key)` -> `return node[0x14]`, then two String releases.
// It is `QEBA` (const), there is no insert, no counter and no store into the
// factory. So the emission sequence is idempotent and the WHOLE body replays;
// no `stopAfter` is needed and none is used. (`SerializeReferenceTable` needs
// one because it WRITES an id into every node — a different function.)
//
// WHAT THE ORACLE DOES NOT REACH, stated because a partial oracle that reads
// like a full one is worse than none:
//   * the `Encoding` type == 3 arm. Lane S's sink binds all 16 Encoding slots
//     to a recording thunk that returns the stub ReaderWriter POINTER, never 3.
//   * both `FindOrLoad` call sites and the whole READ path: the sink pins
//     `stream.direction = 1` (writing), and every load-side branch is behind
//     `direction == 0`.
//   * the reference-mode arm, unless the live factory's `+0x40` is set on the
//     run. Both sides read the same live global, so whichever arm is taken is
//     taken by both — but only ONE of the two is exercised per run, and the
//     probe's own report is what says which.
//
// ---------------------------------------------------------------------------
// LAYOUT NOTES A READER WILL WANT
// ---------------------------------------------------------------------------
// `param_2` is an `IO::Serializer*` — `{ Stream*, ReaderWriter* }`. The body
// reads `*(Stream**)param_2` and then `Stream+0x08` (direction) and
// `Stream+0x10` (Encoding*), which is why the caller
// `Serialize@ElementDescription` builds the pair on its own stack and passes
// `&pair`, and why `String::Serialize` is handed that same pointer.
//
// Retail's three locals live in the CALLER's 32-byte home space (`MOV RBP,RSP`
// after five pushes, so `[RBP+0x38]`, `[RBP+0x40]` and `[RBP+0x48]` are above
// the return address). Legal in the MS x64 ABI, and noted only so a reader
// checking offsets against the listing does not read them as arguments. They
// are `scratch` (a `Filename` in the encoding-3 arm, then a uint / bool / the
// result pointer), `path` and `tmp`, and they are spelled as separate locals
// below because their lifetimes do not overlap.
//
// PORTABLE: no <windows.h> (PLAN.md §4.1 decision 1).
// ============================================================================

#include <cstdint>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"
#include "../../types/object/External.h"
#include "../../types/system/Filename.h"
#include "../../types/system/String.h"
#include "../system/string.h"

using RtObject::External;
using RtObject::ExternalFactory;
using RtSystem::Filename;
using RtSystem::String;
using rt::io::Encoding;
using rt::io::ReaderWriter;
using rt::io::Serializer;
using rt::io::Stream;

namespace {

// ---- the dependency doors, all by EXPORTED NAME through the port registry ---
//
// RT_EXPORT_ONCE, not RT_ORIGINAL_ONCE: the point is to reach OUR port of each
// of these where one exists, not the shipped body
// ([[reference_rtoriginal_is_getprocaddress]] is about the cost, and
// port-bypass is about the correctness). One cache per call site.

using GetPathFn        = Filename* (*)(const External*, Filename*);
using FindOrLoadFn     = const External* (*)(ExternalFactory*, const Filename*);
using IsDerivedFromFn  = bool (*)(const void*, uint32_t);
using AdjustDelimFn    = void (*)(Filename*);
using GetRefIdFn       = uint32_t (*)(const ExternalFactory*, const External*);
using GetPathForIdFn   = Filename* (*)(const ExternalFactory*, Filename*, uint32_t);
using StringSerializeFn = void (*)(String*, Serializer*);

Filename* CallGetPath(const External* e, Filename* ret) {
    return RT_EXPORT_ONCE(GetPathFn,
        "?GetPath@External@Object@@QEBA?AVFilename@System@@XZ")(e, ret);
}

// The ONE unported callee. See the FindOrLoad block in the banner.
const External* CallFindOrLoad(ExternalFactory* f, const Filename* p) {
    return RT_EXPORT_ONCE(FindOrLoadFn,
        "?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z")(f, p);
}

bool CallIsDerivedFrom(const void* obj, uint32_t typeId) {
    return RT_EXPORT_ONCE(IsDerivedFromFn,
        "?IsDerivedFrom@Base@Object@@QEBA_NI@Z")(obj, typeId);
}

void CallAdjustDelimiters(Filename* f) {
    RT_EXPORT_ONCE(AdjustDelimFn, "?AdjustDelimiters@Filename@System@@AEAAXXZ")(f);
}

uint32_t CallGetReferenceId(const ExternalFactory* f, const External* e) {
    return RT_EXPORT_ONCE(GetRefIdFn,
        "?GetReferenceId@ExternalFactory@Object@@QEBAIPEBVExternal@2@@Z")(f, e);
}

Filename* CallGetPathForReferenceId(const ExternalFactory* f, Filename* ret, uint32_t id) {
    return RT_EXPORT_ONCE(GetPathForIdFn,
        "?GetPathForReferenceId@ExternalFactory@Object@@QEBA?AVFilename@System@@I@Z")(f, ret, id);
}

void CallStringSerialize(Filename* f, Serializer* ser) {
    // Filename and String are layout-identical; retail passes the Filename
    // straight to String::Serialize (Filename.h records the cast idiom).
    RT_EXPORT_ONCE(StringSerializeFn,
        "?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z")(
            reinterpret_cast<String*>(f), ser);
}

// `?_singleton@ExternalFactory@Object@@0PEAV12@EA` — note the `0…EA` spelling;
// the obvious `1PEAV12@A` guess is not an export (External.h records this).
// The export ADDRESS is the slot, so this is a pointer to the pointer.
ExternalFactory** SingletonSlot() {
    using FactorySlot = ExternalFactory**;
    return RT_DATA_SLOT(FactorySlot, "?_singleton@ExternalFactory@Object@@0PEAV12@EA");
}

ExternalFactory* Factory() {
    ExternalFactory** slot = SingletonSlot();
    return slot ? *slot : nullptr;
}

// ---- the two inlined String idioms retail open-codes five times -------------

// `dst = *src` with refcounting, retail's exact shape (0x1800043dd..0x18000441a):
// a same-pointer assignment is a no-op, otherwise release then adopt.
void AssignFilename(Filename* dst, const Filename* src) {
    if (dst->data == src->data) return;
    RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(dst));
    dst->data = nullptr;
    RtSystem::RtString_AdoptInline(reinterpret_cast<String*>(dst), src->data);
}

// ---- the codec accessors ---------------------------------------------------

// `MOV RAX,[RDX]; MOV RCX,[RAX+0x10]; MOV RAX,[RCX]; CALL [RAX+0x10]`
// — Encoding vtable slot 2. NOT a `GetType` on the Serializer: the Encoding
// is fetched off the STREAM, which is why this needs the Stream and not just
// the Serializer.
int EncodingType(const Serializer* ser) {
    Encoding* enc = ser->stream->encoding;
    using GetTypeFn = int (*)(Encoding*);
    return reinterpret_cast<GetTypeFn>(enc->vtable[0x10 / 8])(enc);
}

// `MOV RCX,[RDI+8]; MOV RAX,[RCX]; XOR R8D,R8D; CALL [RAX+<slot>]`
// The third argument is the record's NAME and retail passes NULL at both sites.
// The two slot numbers are RETAIL's: in Runtime-x64-profile.dll the same two
// live at +0x90 and +0xb0, and copying them from a profile decompile would emit
// the wrong record type into every save
// ([[reference_encoding_vtable_offsets_differ_between_builds]]).
void SerializeBool(Serializer* ser, bool* v) {
    ReaderWriter* rw = ser->readerWriter;
    using Fn = void (*)(ReaderWriter*, bool*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kScalarBool / 8])(rw, v, nullptr);
}

void SerializeUInt(Serializer* ser, uint32_t* v) {
    ReaderWriter* rw = ser->readerWriter;
    using Fn = void (*)(ReaderWriter*, uint32_t*, const char*);
    reinterpret_cast<Fn>(rw->vtable[rt::io::rw_vtbl::kScalarUInt / 8])(rw, v, nullptr);
}

constexpr uint32_t kNoReferenceId = 0xFFFFFFFFu;

}  // namespace

// ============================================================================
// void Object::ExternalPtr<T>::Serialize(IO::Serializer&)
//   retail: 25 addresses, 215 instructions / 748 bytes each. Unexported.
//
// RCX = `self`, the eight-byte `ExternalPtr<T>` field IN PLACE (an `External*`).
// RDX = the `IO::Serializer`.
// `typeInfo` is this instantiation's `?_typeInfo@T@…` — retail's two
// `MOV EDX, dword ptr [rip+…]` loads. The FIRST DWORD of a `BaseTypeInfo` is
// its `typeId` (runtime/types/object/TypeInfo.h), and it is read AT CALL TIME
// at each of the two sites, exactly as retail does: the id is 0xFFFFFFFF until
// `Factory::Startup` assigns it, so caching it at init would freeze the
// pre-Startup value.
//
// THREE MODES, in retail's order:
//   1. Encoding type == 3 — no stream I/O at all. Re-resolve the object we
//      already hold through `FindOrLoad`, run the type check and DISCARD its
//      answer (retail does: `CALL IsDerivedFrom` with the result unused), then
//      release the scratch path. This is the resource-reference VISIT pass.
//   2. reference mode ON (`factory+0x40`) — a uint reference id crosses the
//      stream; on load it is turned back into a path through the factory's
//      id-keyed tree.
//   3. reference mode OFF — a bool "present" flag then the path string.
// Modes 2 and 3 converge on one load tail: `FindOrLoad(path)`, type-check, and
// write the result THROUGH `self`.
//
// RETAIL DEREFERENCES NOTHING IT HAS NOT ALREADY TESTED HERE, so there are no
// added guards in this body. Where retail is unguarded downstream — e.g.
// `GetReferenceId` derefs the node `Set::Find` returned with no null test — that
// is the callee's business and its own port already transcribes it.
// ============================================================================
extern "C" void RtObject_ExternalPtr_Serialize(External** self, Serializer* ser,
                                               const void* typeInfo) {
    // ---- mode 1: encoding type 3 (0x1800042ce) -----------------------------
    if (EncodingType(ser) == 3) {
        External* cur = *self;
        if (cur == nullptr) return;

        Filename scratch;                       // [rbp+0x38]; GetPath copy-ctors
        CallGetPath(cur, &scratch);             // into it, so it starts raw.
        const External* found = CallFindOrLoad(Factory(), &scratch);
        if (found != nullptr) {
            // The answer is genuinely discarded — retail calls it for the side
            // effect of nothing, and keeps the pointer it already had.
            (void)CallIsDerivedFrom(found, *static_cast<const uint32_t*>(typeInfo));
        }
        RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&scratch));
        return;
    }

    // ---- modes 2 and 3 (0x18000433e) ---------------------------------------
    ExternalFactory* factory = Factory();       // r14
    Filename path;                              // [rbp+0x40]
    path.data = nullptr;
    Stream* stream = ser->stream;

    if (factory->_referenceModeEnabled) {
        // ---- mode 2: reference id ------------------------------------------
        uint32_t refId = (*self != nullptr)
            ? CallGetReferenceId(factory, *self)
            : kNoReferenceId;

        // 0x180004372. The odd arm, transcribed rather than rationalised: when
        // we are LOADING, already hold an object, and that object has no
        // reference id, retail consumes the uint record and BAILS — it neither
        // resolves a path nor writes through `self`.
        if (*self != nullptr && stream->direction == rt::io::kDirectionReading &&
            refId == kNoReferenceId) {
            SerializeUInt(ser, &refId);
            RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&path));
            return;
        }

        SerializeUInt(ser, &refId);             // 0x1800043ad; load overwrites
        if (refId == kNoReferenceId) {          // 0x1800043b7
            RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&path));
            return;
        }

        if (stream->direction == rt::io::kDirectionReading) {   // 0x1800043c1
            Filename tmp;                       // [rbp+0x48]
            CallGetPathForReferenceId(factory, &tmp, refId);
            AssignFilename(&path, &tmp);
            CallAdjustDelimiters(&path);
            RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&tmp));
        }
        // falls into the load tail
    } else {
        // ---- mode 3: present flag + path string (0x180004459) --------------
        bool present = (*self != nullptr);      // SETNE, one byte
        SerializeBool(ser, &present);
        if (!present) {
            RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&path));
            return;
        }

        External* cur = *self;
        if (cur != nullptr) {                   // saving: fill `path` from the object
            Filename tmp;                       // [rbp+0x48]
            CallGetPath(cur, &tmp);
            AssignFilename(&path, &tmp);
            CallAdjustDelimiters(&path);
            RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&tmp));
        }
        CallStringSerialize(&path, ser);        // 0x180004500; load fills `path`
        // falls into the load tail
    }

    // ---- the shared load tail (0x18000450c) --------------------------------
    // Saving skips it entirely: retail re-tests the direction here and jumps
    // straight to the release below.
    if (stream->direction == rt::io::kDirectionReading) {
        const External* loaded = CallFindOrLoad(Factory(), &path);
        External* result = nullptr;
        if (loaded != nullptr &&
            CallIsDerivedFrom(loaded, *static_cast<const uint32_t*>(typeInfo))) {
            // CMOVNE: the loaded object survives only if it IS a T. A resource
            // of the wrong type is dropped, not kept — this is the check the
            // exe-side sibling in code/Game/Component/ambient_externalptr_serialize.cpp
            // deliberately relaxes, and this one must not.
            result = const_cast<External*>(loaded);
        }
        *self = result;                         // 0x180004555
    }

    RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&path));
}

// ============================================================================
// The per-instantiation thunks.
//
// Each retail address is one `T`, so each gets a two-line entry point that
// supplies its own `?_typeInfo@T@…` and forwards. The type-info slot is
// resolved by EXPORTED NAME (all 25 are exported DATA symbols — `AEAA`/`0…A`
// spellings mean PRIVATE, not unexported, and this DLL exports its privates:
// [[reference_aeaa_is_private_not_unexported]]), so no address door is opened
// for the data either.
// ============================================================================
#define EXTPTR_THUNK(fnName, mangledTypeInfo)                                  \
    extern "C" void fnName(External** self, Serializer* ser) {                 \
        using TypeInfoPtr = const void*;                                       \
        const void* ti = RT_DATA_SLOT(TypeInfoPtr, mangledTypeInfo);           \
        if (ti == nullptr) {                                                   \
            static bool warned = false;                                        \
            if (!warned) {                                                     \
                warned = true;                                                 \
                std::fprintf(stderr,                                           \
                    "[RtExtPtr] %s did not resolve -- this instantiation "     \
                    "cannot run its type check\n", mangledTypeInfo);           \
            }                                                                  \
            return;                                                            \
        }                                                                      \
        RtObject_ExternalPtr_Serialize(self, ser, ti);                         \
    }

// 0x180038f90 — ElementDescription+0x48
EXTPTR_THUNK(RtObject_ExternalPtrSerialize_ElementDescription,
             "?_typeInfo@ElementDescription@UI@@0VBaseTypeInfo@Object@@A")
// 0x180076610 — ElementDescription+0x50 (the field the header called `_userData`)
EXTPTR_THUNK(RtObject_ExternalPtrSerialize_Dialog,
             "?_typeInfo@Dialog@UI@@0VBaseTypeInfo@Object@@A")
// 0x1800042a0 — ElementDescription+0x58, and Achievement::Goal+0x20
EXTPTR_THUNK(RtObject_ExternalPtrSerialize_StringTable,
             "?_typeInfo@StringTable@UI@@0VBaseTypeInfo@Object@@A")
// 0x180005dc0 — Achievement::GoalInstance+0x18. The oracle route; see the banner.
EXTPTR_THUNK(RtObject_ExternalPtrSerialize_Goal,
             "?_typeInfo@Goal@Achievement@@0VBaseTypeInfo@Object@@A")

#undef EXTPTR_THUNK

// ---- Registration ----------------------------------------------------------
//
// BY ADDRESS ONLY, because these are unexported internals — there is no mangled
// name to register and the address door is the only door
// ([[reference_by_address_door_dies_at_cutover]] is exactly why they must be
// registered rather than left to `ResolveAddr`'s DLL fallback).
//
// Grepped before writing: none of the 25 addresses is registered anywhere in
// `runtime/`, `standalone/`, `code/` or `engine/` today, so no binding is being
// shadowed ([[reference_grep_for_the_port_before_writing_it]]).
//
// Two existing call sites in our tree resolve two of these BY ADDRESS already
// (`runtime/src/achievement/goal_serialize.cpp:130` and `:136`), so they start
// reaching this file the moment this runs — which is the point, and which is
// what turns their lane-S probes into real oracles for this body.
int RtObjectExternalPtrSerializeRegister() {
    int n = 0;
    RegisterRuntimeAddr(0x180038f90, (void*)&RtObject_ExternalPtrSerialize_ElementDescription); ++n;
    RegisterRuntimeAddr(0x180076610, (void*)&RtObject_ExternalPtrSerialize_Dialog);            ++n;
    RegisterRuntimeAddr(0x1800042a0, (void*)&RtObject_ExternalPtrSerialize_StringTable);       ++n;
    RegisterRuntimeAddr(0x180005dc0, (void*)&RtObject_ExternalPtrSerialize_Goal);              ++n;
    return n;
}
