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
// THE FindOrLoad DEPENDENCY: NO LONGER THE DLL'S, AND THE NAME DOOR IS WHY
// ---------------------------------------------------------------------------
// (Updated 2026-09-13, ENDGAME B.3.1 step 2. This block previously said
// `FindOrLoad` was unported and still fell through to the shipped DLL. That is
// stale: the port landed, and the prediction the old text made came true
// without a single edit in this file. The prediction is kept below because it
// is the reason the outcome was free.)
//
// `?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z`
// (0x1800bf320) IS PORTED. `RtObject_ExternalFactory_FindOrLoad` in
// runtime/src/object/external_compiled.cpp is the body, and that file registers
// it BOTH ways: by mangled name via `RegisterRuntimeExport` and at the retail
// address via `RegisterRuntimeAddr(0x1800bf320, ...)`.
//
// This file's one call site (`CallFindOrLoad`, below) reaches it through
// `RT_EXPORT_ONCE`, i.e. by EXPORTED NAME through the PORT-AWARE resolver —
// never by address. The resolver consults the port registry first
// ([[reference_resolveaddr_consults_port_registry_first]]), so the name now
// resolves to our own ported body. That is exactly the outcome the original
// text predicted for the day the name got registered: "the moment that name is
// registered, this call site picks that body up with no edit here." It did, and
// no edit was needed.
//
// So this file still opens NO ADDRESS DOOR — not onto the DLL and not onto
// anything else — which is the [[reference_porting_can_grow_the_cutover_surface]]
// failure the gate doc flagged. Every callee it reaches, `FindOrLoad` included,
// is reached by name and is therefore visible to
// tools/unsatisfied_export_scan.py.
//
// Everything else this body calls is already ours: `External::GetPath`,
// `Base::IsDerivedFrom`, `ExternalFactory::GetReferenceId` and
// `ExternalFactory::GetPathForReferenceId` are all registered by name today.
// Two former name doors are not doors any more — `Filename::AdjustDelimiters`
// and `String::Serialize` are called as members of `System::Filename` /
// `System::String` since B.3.1 step 2, so they are ordinary C++ calls.
//
// ---------------------------------------------------------------------------
// THE ORACLE
// ---------------------------------------------------------------------------
// Lane S's emission oracle (deleted with the DLL in ENDGAME A.4,
// `the old DLL-era switch`) on the WRITE path, and it reaches this body
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
#include "../../types/ui/DialogControllerConfig.h"   // ENDGAME B.3.9 TI-UI: UI::DialogControllerConfig::GetTypeInfo()
#include "../../types/ui/Dialog.h"   // ENDGAME B.3.9 TI-UI: UI::Dialog::GetTypeInfo()
#include "../../types/ui/ElementDescription.h"   // ENDGAME B.3.9 TI-UI: UI::ElementDescription::GetTypeInfo()
#include "../../types/ui/CharacterSet.h"   // ENDGAME B.3.9 TI-UI: UI::CharacterSet::GetTypeInfo()
#include "../../types/ui/Font.h"   // ENDGAME B.3.9 TI-UI: UI::Font::GetTypeInfo()
#include "../../types/ui/SpriteSheet.h"   // ENDGAME B.3.9 TI-UI: UI::SpriteSheet::GetTypeInfo()
#include "../../types/ui/StringTable.h"   // ENDGAME B.3.9 TI-UI: UI::StringTable::GetTypeInfo()
#include "../../types/audio/SoundBuffer.h"     // ENDGAME B.3.9 TI-REST: the thunks' records
#include "../../types/audio/SoundEffect.h"
#include "../../types/collision2d/Surface.h"
#include "../../types/achievement/AchievementManager.h"
#include "../../types/achievement/Goal.h"
#include "../../types/video/ImageBuffer.h"
#include "../../types/video/ShaderProgram.h"
#include "../../types/video/VertexLayout.h"
#include "../../types/video/RenderState.h"
#include "../../types/video/VideoProgram.h"
#include "../../types/graphics/Animation.h"
#include "../../types/graphics/AnimationGroup.h"
#include "../../types/graphics/GraphicsMesh.h"
#include "../../types/graphics/Material.h"
#include "../../types/system/Filename.h"
#include "../../types/system/String.h"
#include "../system/string.h"
#include "../system/pod_bridge.h"   // the ONE transitional POD<->class bridge
#include "external_compiled.h"      // B3R: RtObject_ExternalFactory_FindOrLoad

using RtObject::External;
using RtObject::ExternalFactory;
using RtSystem::Filename;
using RtSystem::String;
using rt::io::Encoding;
using rt::io::Serializer;
using rt::io::Stream;

namespace {

// ---- the dependency bodies (B3R RT-Core: called by name) -----------------
//   GetPath 0x180006250, FindOrLoad 0x1800bf320, Base::IsDerivedFrom
//   0x1800c0390, GetReferenceId 0x1800bf100, GetPathForReferenceId 0x1800bf1d0.
bool IsDerivedFrom(const RtObject::External* obj, uint32_t typeId) {
    return reinterpret_cast<const ::Object::Base*>(obj)->IsDerivedFrom(typeId);
}

// ENDGAME B.3.1 step 2 removed two of this file's name doors:
// `?AdjustDelimiters@Filename@System@@AEAAXXZ`, which is private and whose two
// call sites folded into the `Filename::operator=` that already ends in it; and
// `?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z`, now spelled
// `path.AsString().Serialize(ser)` — the same body, and retail passing the
// Filename straight into String::Serialize is exactly what `AsString()` is.

// `?_singleton@ExternalFactory@Object@@0PEAV12@EA` is the class static
// `Object::ExternalFactory::_singleton` (ENDGAME B.3.9 OBJ, B39_RULES D7).
// The class and the POD are asserted field for field in External.h.
ExternalFactory* Factory() {
    return reinterpret_cast<ExternalFactory*>(                       // B.3.3 bridge
        ::Object::ExternalFactory::_singleton);
}

// ENDGAME B.3.1 step 2 removed this file's `AssignFilename` helper. Retail's
// shape at 0x1800043dd..0x18000441a — same-pointer no-op, otherwise release
// then adopt — is followed at BOTH of its call sites by an explicit
// AdjustDelimiters, and the pair is `Filename::operator=(const Filename&)`
// (0x180006770) instruction for instruction. Both sites now say `path = tmp`.

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
// B3X W0: both by name (+0x78 SerializeBool, +0x98 SerializeUInt).
void SerializeBool(Serializer* ser, bool* v) {
    reinterpret_cast<::IO::ReaderWriter*>(ser->readerWriter)   // B.3.x bridge
        ->SerializeBool(*v, nullptr);
}

void SerializeUInt(Serializer* ser, uint32_t* v) {
    reinterpret_cast<::IO::ReaderWriter*>(ser->readerWriter)   // B.3.x bridge
        ->SerializeUInt(*v, nullptr);
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
// ENDGAME B.3.1 step 1: this IS `Object::ExternalPtr<T>::Serialize` now — the
// one template body, declared in runtime/types/object/External.h and defined
// here because of the dependency doors above. The per-T difference is the
// `typeInfo` argument and nothing else; the clone check across all 22
// single-pointer addresses (2026-09-13, function-size-bounded) reports
// identical=193 / position-only=20 / REAL=2 for every pair, both REALs being
// the type-info load.
namespace Object {

void ExternalPtrSerializeBody(RtObject::External** self, Serializer& serRef,
                              const void* typeInfo) {
    Serializer* const ser = &serRef;   // the helpers above take the pointer

    // ---- mode 1: encoding type 3 (0x1800042ce) -----------------------------
    if (EncodingType(ser) == 3) {
        RtObject::External* cur = *self;
        if (cur == nullptr) return;

        // ENDGAME B.3.1 step 2: `scratch` is GetPath's hidden return slot, so
        // it is CONSTRUCTED INTO by the call and must not be constructed twice;
        // its release is the destructor at the return below.
        System::Filename scratch;               // [rbp+0x38]
        RtObject::RtObject_External_GetPath(cur, reinterpret_cast<Filename*>(&scratch));
        const RtObject::External* found = RtObject::RtObject_ExternalFactory_FindOrLoad(
            ::Factory(), reinterpret_cast<const Filename*>(&scratch));
        if (found != nullptr) {
            // The answer is genuinely discarded — retail calls it for the side
            // effect of nothing, and keeps the pointer it already had.
            (void)IsDerivedFrom(found, *static_cast<const uint32_t*>(typeInfo));
        }
        return;
    }

    // ---- modes 2 and 3 (0x18000433e) ---------------------------------------
    // `path` is released on every one of the five exits below; all five are now
    // its destructor, including the three early returns.
    RtObject::ExternalFactory* factory = ::Factory();       // r14
    System::Filename path;                      // [rbp+0x40]
    Stream* stream = ser->stream;

    if (factory->_referenceModeEnabled) {
        // ---- mode 2: reference id ------------------------------------------
        uint32_t refId = (*self != nullptr)
            ? RtObject::RtObject_ExternalFactory_GetReferenceId(factory, *self)
            : kNoReferenceId;

        // 0x180004372. The odd arm, transcribed rather than rationalised: when
        // we are LOADING, already hold an object, and that object has no
        // reference id, retail consumes the uint record and BAILS — it neither
        // resolves a path nor writes through `self`.
        if (*self != nullptr && stream->direction == rt::io::kDirectionReading &&
            refId == kNoReferenceId) {
            SerializeUInt(ser, &refId);
            return;
        }

        SerializeUInt(ser, &refId);             // 0x1800043ad; load overwrites
        if (refId == kNoReferenceId) {          // 0x1800043b7
            return;
        }

        if (stream->direction == rt::io::kDirectionReading) {   // 0x1800043c1
            System::Filename tmp;               // [rbp+0x48], another retslot
            RtObject::RtObject_ExternalFactory_GetPathForReferenceId(
                factory, reinterpret_cast<Filename*>(&tmp), refId);
            // 0x1800043dd release-then-adopt plus the AdjustDelimiters that
            // follows it IS `Filename::operator=`, 0x180006770.
            path = tmp;
        }
        // falls into the load tail
    } else {
        // ---- mode 3: present flag + path string (0x180004459) --------------
        bool present = (*self != nullptr);      // SETNE, one byte
        SerializeBool(ser, &present);
        if (!present) {
            return;
        }

        RtObject::External* cur = *self;
        if (cur != nullptr) {                   // saving: fill `path` from the object
            System::Filename tmp;               // [rbp+0x48], GetPath's retslot
            RtObject::RtObject_External_GetPath(cur, reinterpret_cast<Filename*>(&tmp));
            path = tmp;                         // operator=, as above
        }
        path.AsString().Serialize(ser);         // 0x180004500; load fills `path`
        // falls into the load tail
    }

    // ---- the shared load tail (0x18000450c) --------------------------------
    // Saving skips it entirely: retail re-tests the direction here and jumps
    // straight to the release below.
    if (stream->direction == rt::io::kDirectionReading) {
        const RtObject::External* loaded =
            RtObject::RtObject_ExternalFactory_FindOrLoad(
                ::Factory(), reinterpret_cast<const Filename*>(&path));
        RtObject::External* result = nullptr;
        if (loaded != nullptr &&
            IsDerivedFrom(loaded, *static_cast<const uint32_t*>(typeInfo))) {
            // CMOVNE: the loaded object survives only if it IS a T. A resource
            // of the wrong type is dropped, not kept — this is the check the
            // exe-side sibling in code/Game/Component/ambient_externalptr_serialize.cpp
            // deliberately relaxes, and this one must not.
            result = const_cast<RtObject::External*>(loaded);
        }
        *self = result;                         // 0x180004555
    }
    // path's final release is its destructor.
}

// The array form of the same template — 818 bytes, eight instantiations, and
// retail's loop body is the single-pointer body above with `elems[i]` in place
// of `*self` (0x18005d4a0: every branch is instruction-for-instruction the
// same, indexed by R14). Clone check 2026-09-13: 8/8 at identical=212 /
// position-only=20 / REAL=2.
void ExternalPtrSerializeArrayBody(Serializer& ser, int count, RtObject::External** elems,
                                   const void* typeInfo) {
    for (int i = 0; i < count; ++i)
        ExternalPtrSerializeBody(&elems[i], ser, typeInfo);
}

}  // namespace Object

// ---- the registered C shims -------------------------------------------------
// B31_RULES step 1: every existing vocabulary keeps working. These two keep
// their signatures and their registry rows; they are one line each now.
// TRANSITIONAL CAST (B31_RULES "not-yet-converted structs"): callers still hand
// us the eight bytes as an `External**`; it goes when they move in step 2.
extern "C" void RtObject_ExternalPtr_Serialize(External** self, Serializer* ser,
                                               const void* typeInfo) {
    reinterpret_cast< ::Object::ExternalPtr<External>*>(self)->Serialize(*ser, typeInfo);
}

// ============================================================================
// The per-instantiation thunks.
//
// Each retail address is one `T`, so each gets a two-line entry point that
// supplies its own `?_typeInfo@T@…` and forwards. Since the ENDGAME B.3.9 D6
// W3 folds every `T` is a real class with a DEFINE_TYPE record, so the thunk
// hands over `&T::GetTypeInfo()` (the by-name RT_DATA_SLOT forms of both
// macros went with their last row, ENDGAME B.3.9 OBJ).
// ============================================================================
// ENDGAME B.3.9 D6 W3: the same thunk over a folded class's own record.
#define EXTPTR_THUNK_OF(fnName, Class)                                         \
    extern "C" void fnName(External** self, Serializer* ser) {                 \
        RtObject_ExternalPtr_Serialize(self, ser, &Class::GetTypeInfo());      \
    }

// 0x180038f90 — ElementDescription+0x48
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_ElementDescription, UI::ElementDescription)
// 0x180076610 — ElementDescription+0x50 (the field the header called `_userData`)
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_Dialog, UI::Dialog)
// 0x1800042a0 — ElementDescription+0x58, and Achievement::Goal+0x20
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_StringTable, UI::StringTable)
// 0x180005dc0 — Achievement::GoalInstance+0x18. The oracle route; see the banner.
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_Goal, Achievement::Goal)
// 0x180004590 — Achievement::Achievement+0x38 (WAVE 26 LANE INT33). The FIFTH,
// and it is registered for the same reason the fourth was: it is on an ORACLE
// route that already exists. See the registration block below.
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_SpriteSheet, UI::SpriteSheet)
// 0x180014520 — the SIXTH (WAVE 29 LANE WALL). `SpriteSheet+0x30`, and also
// the four material slots of `Graphics::EdgeMeshInstance` and
// `Graphics::Particle`. See the registration block below for why it moves now.
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_MaterialInstance, Graphics::MaterialInstance)

// ---- 2026-09-12: the rest of the family that the cutover closure reaches ----
// (docs/runtime_port/closure/WORKLIST.md). Same 748-byte body, one typeInfo
// each; the pairing of address to T is the RIP-relative `_typeInfo` operand
// each body loads, read out of Ghidra, not guessed from the caller.
// 0x18005d0f0 — Video::ImageBuffer
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_ImageBuffer, Video::ImageBuffer)
// 0x180061100 — Graphics::Material (cutover attempt 33's wall, from MaterialInstance::Serialize+0x83)
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_Material, Graphics::Material)
// 0x180014230 — Graphics::GraphicsMesh
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_GraphicsMesh, Graphics::GraphicsMesh)
// 0x1800756d0 — UI::DialogControllerConfig
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_DialogControllerConfig, UI::DialogControllerConfig)
// 0x180051790 — Audio::SoundEffect
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_SoundEffect, Audio::SoundEffect)
// 0x18005b840 — Graphics::AnimationGroup
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_AnimationGroup, Graphics::AnimationGroup)
// 0x180006d10 — Achievement::Achievement. The export spells the class with a
// back-reference (`@Achievement@1@`), which is why a grep for
// `Achievement@Achievement@@` finds nothing; the name below is verified against
// the export directory and is the same key the mint uses.
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_Achievement, Achievement::Achievement)
// The four Video::VideoProgram fields (runtime/src/video/videoprogram.cpp):
// 0x180055090 VertexLayout, 0x1800542a0 VertexProgram, 0x180055380 PixelProgram,
// 0x180055670 RenderState. Reached only through VideoProgram's Serialize slot,
// so invisible to the static closure until that class was ported.
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_VertexLayout, Video::VertexLayout)
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_VertexProgram, Video::VertexProgram)
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_PixelProgram, Video::PixelProgram)
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_RenderState, Video::RenderState)
// 0x18003f1e0 — UI::Font, `UI::TextDescription` +0x68 (vtable batch 2). The
// pairing is this body's own `MOV EDX,dword ptr [0x1801fab68]` at 0x18003f473
// (the type id handed to Object::Base::IsDerivedFrom), and 0x1801fab68 is
// `?_typeInfo@Font@UI@@…` — cross-checked against
// standalone/rt_data_mint_tables.inc:4373, where the mint carries that same
// name at that same address. The class UI::Font itself is NOT ported here and
// is not needed: this body only ever compares its type id.
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_Font, UI::Font)
// VTABLE BATCH 7 (2026-09-12): 0x1800594a0 — Graphics::AnimationEvent. Reached
// from Graphics::AnimationGroup::Serialize's per-clip event-list element loop
// (runtime/src/graphics/vtbatch7_animgroup_particlesystem.cpp), the same
// 748-byte shared template as every other row here; typeInfo confirmed against
// the retail export table (pefile) at 0x1801f9978, not assumed.
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_AnimationEvent, Graphics::AnimationEvent)

// ---- BATCH B (2026-09-12): the last three single-pointer instantiations -----
//
// Each T is the RIP-relative `_typeInfo` operand the body itself loads, read
// out of the SHIPPED DLL with objdump (both `MOV EDX,[rip+…]` sites in each
// body load the same global, as they must), then that absolute address turned
// back into a name TWO independent ways that agree: `standalone/
// rt_data_mint_tables.inc` and `pefile` over the export directory. No T is
// inferred from the caller.
//
//   0x180076e90 -> 0x1801f9cf8  UI::CharacterSet        (UI::Font +0x28)
//   0x180052a50 -> 0x1801fa4d8  Audio::SoundBuffer
//   0x18003dc90 -> 0x1801f9be0  Object::ExternalList    (DocumentTextDescription +0xe0)
//
// Callers, confirmed in Ghidra (get_function_callers on the retail program):
//   0x180076e90  <- Serialize @ 0x1801176d0  = UI::Font::Serialize, the current
//                   cutover wall; our port calls it at runtime/src/ui/font.cpp:381.
//   0x180052a50  <- 0x180053100, 0x180053300, 0x1800e0780 (the Audio side).
//   0x18003dc90  <- runtime/src/ui/documenttextdescription.cpp:194 resolves it
//                   by address for the +0xe0 container field.
// 0x180076e90 — ExternalPtr<UI::CharacterSet>
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_CharacterSet, UI::CharacterSet)
// 0x180052a50 — ExternalPtr<Audio::SoundBuffer>
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_SoundBuffer, Audio::SoundBuffer)
// 0x18003dc90 — ExternalPtr<Object::ExternalList>. The export spells the
// enclosing namespace with a back-reference (`@2@`), exactly as
// `?_typeInfo@External@Object@@0VBaseTypeInfo@2@A` does.
EXTPTR_THUNK_OF(RtObject_ExternalPtrSerialize_ExternalList, ::Object::ExternalList)

#undef EXTPTR_THUNK_OF

// ============================================================================
// The ARRAY form of the same template — 818 bytes, eight instantiations.
//
//     void (Serializer* ser, int count, External** elems)      RCX, EDX, R8
//
// Retail's loop body is the single-pointer body with `elems[i]` in place of
// `*self` (0x18005d4a0: the encoding-type-3 branch, the path/reference
// branches, the FindOrLoad + IsDerivedFrom tail and the String release are all
// instruction-for-instruction the same, indexed by R14). R9 is spilled to its
// home slot and never read. So the port is the loop, calling the shared body.
//
// B.3.1 step 1: the loop is `Object::ExternalPtr<T>::SerializeArray` above;
// this is the registered shim, with the same transitional cast.
// ============================================================================
extern "C" void RtObject_ExternalPtrArray_Serialize(Serializer* ser, int count,
                                                    External** elems, const void* typeInfo) {
    using Ptr = ::Object::ExternalPtr<External>;
    Ptr::SerializeArray(*ser, count, reinterpret_cast<Ptr*>(elems), typeInfo);
}

// ENDGAME B.3.9 D6 W3: the same thunk over a folded class's own record.
#define EXTPTR_ARRAY_THUNK_OF(fnName, Class)                                   \
    extern "C" void fnName(Serializer* ser, int count, External** elems) {     \
        RtObject_ExternalPtrArray_Serialize(ser, count, elems, &Class::GetTypeInfo()); \
    }

// 0x18002f170 — Object::External (note the `@2@` back-reference in the export)
EXTPTR_ARRAY_THUNK_OF(RtObject_ExternalPtrArraySerialize_External, ::Object::External)
// 0x180005220 — Achievement::Goal
EXTPTR_ARRAY_THUNK_OF(RtObject_ExternalPtrArraySerialize_Goal, Achievement::Goal)
// 0x1800493d0 — UI::ElementDescription
EXTPTR_ARRAY_THUNK_OF(RtObject_ExternalPtrArraySerialize_ElementDescription, UI::ElementDescription)
// 0x18005d4a0 — Graphics::MaterialInstance
EXTPTR_ARRAY_THUNK_OF(RtObject_ExternalPtrArraySerialize_MaterialInstance, Graphics::MaterialInstance)
// 0x1800605e0 -- Video::VideoProgram (Material::_programs; not in the static
// closure because Material::Serialize is reached only through a vtable slot)
EXTPTR_ARRAY_THUNK_OF(RtObject_ExternalPtrArraySerialize_VideoProgram, Video::VideoProgram)

// ---- BATCH B (2026-09-12): the last three array instantiations -------------
//
// Shape re-checked in the shipped DLL rather than taken from the byte size:
// each of these three opens `TEST EDX,EDX / JLE <epilogue>` and indexes
// `(%r15,%r14,8)`, i.e. `(Serializer* rcx, int count edx, External** r8)` with
// R9 spilled and never read — the 818-byte array form, not the 748-byte one.
// typeInfo derived and cross-checked exactly as for the single-pointer rows.
//
//   0x18003e710 -> 0x1801fab68  UI::Font              (DocumentTextDescription +0xb0)
//   0x18003eb60 -> 0x1801fadd0  UI::SpriteSheet       (+0xc0, and SpriteCapture +0x20)
//   0x1800667f0 -> 0x1801fa820  Collision2D::Surface  (callers 0x180065b10,
//                                0x180065f00, Serialize @ 0x180108cd0)
//
// Three call sites in our tree resolve these by address today and therefore
// still run retail's copies until this registration block runs:
// documenttextdescription.cpp:171 and :177, spritecapture_class.cpp:216.
EXTPTR_ARRAY_THUNK_OF(RtObject_ExternalPtrArraySerialize_Font, UI::Font)
EXTPTR_ARRAY_THUNK_OF(RtObject_ExternalPtrArraySerialize_SpriteSheet, UI::SpriteSheet)

#undef EXTPTR_ARRAY_THUNK_OF
