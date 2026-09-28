// code/Game/Game/serialize_arrays.cpp
// Generic Geometry/Transform/Array serialize helpers used by MenuScene::Slot5.
//
// Decompiled from Application-x64.exe. These helpers are not MenuScene-
// specific — Geometry::Serialize and other scene serializers reuse them.
//
// Helper map (M3.5 / notes/M35_recon.md):
//   #1 Geometry init                — exe 0x1400b00e0  (no cascade)
//   #2 Transform::Serialize wrapper — exe 0x1400988c0  (no cascade)
//   #3 Array_Resize stride 0x20     — exe 0x14001baf0  (no cascade)
//   #4 Array_Resize stride 0x08     — exe 0x140001bb0  (no cascade)
//   #5 Array_Resize stride 0x14     — exe 0x14001bf10  (no cascade)
//   #6 Array_Serialize stride 0x20  — exe 0x14001bbf0  (no cascade)
//   #7 Array_Serialize_External GraphicsMesh — exe 0x14001bcc0  (CASCADE)
//   #8 Array_Serialize_External Particle     — exe 0x14001bff0  (CASCADE)
//
// Helpers #7/#8 share a byte-identical body that differs only in the
// BaseTypeInfo* slot it dereferences. We extract that body into a single
// generic implementation parameterized by typeIdPtr.
//
// **param_4 mystery (recon question 2):** the Ghidra decompile of #7 and #8
// shows `undefined8 param_4` threaded into the function but never read past
// `local_res20 = param_4` (which is then overwritten with GetReferenceId's
// return). The caller (MenuScene::Slot5 at 0x1400a3779/0x1400a3928) never
// sets R9 — only RCX/RDX/R8 are loaded before the CALL. **param_4 is
// decompiler noise** (R9 leftover from prior code), so our reimpl signature
// drops it entirely.

#include "Game/WorldGeometry.h"
#include "math/Math.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "graphics/GraphicsMesh.h"   // ENDGAME B.3.9 TR: GraphicsMesh::TypeId()
#include "graphics/Particle.h"       // ENDGAME B.3.9 TR: Particle::TypeId()
#include "Component/coll_bridge.h"
#include "system/Filename.h"
#include "system/String.h"
#include "Object/factory_registration.h"
#include "collection/Collection.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <new>
#include "Game/exe_bodies.h"

// ============================================================
// Helper #1 — Geometry init (FUN_1400b00e0)
// ============================================================
//
// Constructs a 0x230-byte Graphics::Geometry-derived container ("TerrainGenerator"
// per the type-name string at exe 0x14015f158). Layout per Ghidra disasm of
// FUN_1400b00e0:
//   +0x000  Graphics::Geometry::Geometry(self)        — base ctor (0xb8 bytes)
//   +0x000  vtable = exe 0x14015f110 (derived 9-slot)
//   +0x0c0..+0x0f8  zeroed (mesh state slots)
//   +0x100  Video::VertexBuffer ctor (~0x20 bytes)
//   +0x120  Video::IndexBuffer  ctor (~0x30 bytes)
//   +0x150,+0x158  zeroed
//   +0x160  Video::VertexBuffer ctor
//   +0x180  Video::IndexBuffer  ctor
//   +0x1a0..+0x228  zeroed
//   +0x030  = 1  (state flag)
//
// ENDGAME B.3.8 block LOOP: the body is WorldGeometry's constructor
// (code/Game/Game/WorldGeometry.h). Graphics::Geometry() and the 0x14015f110
// stamp are the compiler's (the old body ran the base ctor door and then
// stamped the hand-built table registered at 0x14015f110); the field stores are
// unchanged. (The Reimpl_Geometry_Init door registered at 0x1400b00e0 went at the
// B.3.8 close with fn_Geometry_Init, its only reader.)
WorldGeometry::WorldGeometry() {
    _gridSize = 0;  _fieldC4 = 0;       // qword +0xc0
    _chunkSize = 0; _fieldCC = 0;       // qword +0xc8
    _fieldD0 = 0;                       // dword +0xd0 (+0xd4 left as found)
    _arrayD8 = nullptr;                 // +0xd8
    _arrayD8Size = 0;                   // +0xe0
    _dirtyTreeRoot = nullptr;           // +0xe8
    _dirtyTreeCount = 0;                // dword +0xf0
    _terrainMaterial = nullptr;         // +0xf8

    Video_VertexBuffer_Ctor(reinterpret_cast<Video::VertexBuffer*>(&_vertexBuffer));   // +0x100 (ctor body, B4b-2)
    Video_IndexBuffer_Ctor(reinterpret_cast<Video::IndexBuffer*>(&_indexBuffer));      // +0x120

    _waterMaterialA = nullptr;          // +0x150
    _waterMaterialB = nullptr;          // +0x158

    Video_VertexBuffer_Ctor(reinterpret_cast<Video::VertexBuffer*>(&_waterVertexBuffer));   // +0x160
    Video_IndexBuffer_Ctor(reinterpret_cast<Video::IndexBuffer*>(&_waterIndexBuffer));      // +0x180

    for (Header& h : _arrays) { h.data = nullptr; h.sizeCap = 0; }   // +0x1a0..+0x228

    _materialTypeFlags = 1;             // +0x30 (Graphics::Geometry's)
}

// ============================================================
// Helper #2 — Transform::Serialize wrapper (FUN_1400988c0)
// ============================================================
//
// Wraps a Stream in a Serializer, then delegates to Math::Transform::Serialize
// + Math::Vector3::Serialize at +0x1c + 3 IO::Serializer::Serialize(float)
// calls at +0x28/+0x2c/+0x30.
//
// ⚠ THIS IS A STREAM CALL, NOT A SETTER. It was a warn-and-return stub for a
// long time, and the "unresolved — Transform fields remain at default values"
// wording made that read as a cosmetic loss (a camera left at the origin). It is
// not: GameUpdateLoop::Serialize calls this for the PlayerController at +0xE0
// immediately BEFORE EntityList::Serialize, so a body that consumes zero bytes
// leaves the whole camera record sitting in the stream, and the entity list then
// reads a camera float as its master count (observed 0x3EB7B5ED = 1051929709,
// which is ~0.358f). Our own saves skipped the same bytes on the SAVE leg, so the
// round trip stayed self-consistent — only the RETAIL-authored packaged tutorial
// save could expose it.
//
// All four callees are Runtime exports, so a null here means engine_funcs never
// resolved — report that loudly rather than silently dropping the read.
extern "C" void Reimpl_Transform_SerializeWrapper(void* transform, void* stream) {
    if (!transform || !stream) return;

    // ENDGAME B.3.4a closing: the ctor/float/dtor readiness guard and its
    // "a callee is NULL, the stream is DESYNCED" report are gone with the doors
    // — the serializer below is a real IO::Serializer and its members cannot be
    // null.
    const uintptr_t t = (uintptr_t)transform;
    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    // ?Serialize@Transform@Math@@QEAAXAEAVSerializer@IO@@@Z  retail 0x180059b90
    reinterpret_cast<Math::Transform*>(transform)->Serialize(ser);   // +0x00 Math::Transform
    // ?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z    retail 0x1800150e0
    reinterpret_cast<Math::Vector3*>(t + 0x1c)->Serialize(ser);      // +0x1c Math::Vector3
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(t + 0x28), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(t + 0x2c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(t + 0x30), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Generic dynamic-array resize implementation
// ============================================================
//
// Layout of arrayHeader (16 bytes):
//   +0x00: void*  data        — buffer base (or NULL)
//   +0x08: int32  count       — current length (visible to caller)
//   +0x0C: int32  capacity    — allocated slots (>= count)
//
// Algorithm matches FUN_14001baf0 / FUN_140001bb0 / FUN_14001bf10:
//   if (newCount == count) return;
//   if (newCount > capacity) {
//       capacity = newCount;
//       if (newCount == count + 1 && count != 0) capacity = newCount * 3;
//       newBuf = MemAlloc(capacity * stride, alignment);
//       (caller continues below to copy)
//   }
//   for slot in [start..newCount):
//       if (slot < oldCount) memcpy elem from oldBuf
//       else if (newBuf was reallocated) zero-init elem
//   if (oldBuf != newBuf) MemFree(oldBuf)
//
// `start` = oldCount when buf didn't reallocate, else 0.
//
// Note: ALL variants zero every new slot (slot >= oldCount) on BOTH paths,
// matching the original exe resize functions. An earlier version assumed
// "MemAlloc returns zeroed memory" and only zeroed on the realloc path —
// that was WRONG (the engine boundary-tag allocator at Runtime 0x1800c6e60
// returns DIRTY memory), and on an in-place grow it left new slots holding
// stale bytes (a freed-block pointer) → free-list corruption when a later
// serialize treated the stale pointer as live. See the unconditional-zero
// branch below.
//
// ENDGAME B.3.2 step 2: `GenericArrayResize` (the runtime-stride body the
// block above describes) and the two hand-unrolled bodies after it
// (FUN_140001bb0, FUN_14000c520) WERE copies of `rtcoll::SetSizeCore`
// (runtime/src/collection/array.cpp). Each retail address is now one
// `Collection::Array<>::SetSize` over its own `ArraySpec` row: the zero-init
// the paragraph above insists on is the row's `init`, and SetSizeCore runs it
// for every slot >= oldCount on both the in-place and the realloc path — the
// same rule. Compared arm for arm before the swap: exact-equal early return,
// `_size` stored before the capacity test, treble-on-single-append-from-
// non-empty, byte-copy the preserved prefix only when the block moved, init
// the new tail, no destroy, free the old block on a realloc (null-tolerant
// 0x1800c7ad0, so the `oldData != nullptr` test is folded in). Two
// deliberate differences: the allocation now goes through the LOCKING static
// `MemAlloc` (0x1800c7a50) instead of the unlocked instance `Allocate` these
// bodies reached through `fn_MemAlloc` (same allocator, same heap); and the
// "fn_MemAlloc unavailable" scaffold guard + printf is gone — fn_MemAlloc is
// resolved before any serializer runs, so that arm was dead. The `!arrayHeader`
// early-out stays on each entry point exactly as before.
namespace {

// Whole-stride byte copy, for the plain-data rows.
template <uint32_t N> void ByteAssign(uint8_t* d, const uint8_t* s) { std::memcpy(d, s, N); }

// ENDGAME B.3.2 closing pass: three of the four rows that used to sit here said
// nothing a C++ type's traits do not say — whole-stride byte copy, whole-stride
// zero tail, no destroy, no deliberately-untouched extent — so each is now the
// trait-derived `Collection::Array<T>` over an element that zeroes itself.
// `GameColl::Bytes<N,A>` and `GameColl::Ptr8` are exactly those elements
// (coll_bridge.h: NSDMI-zeroed, trivially copyable, trivially destructible), so
// `SpecFor<T>()` re-derives the identical row. The retail addresses stay here as
// the provenance. `kSpec_SpawnEntry` below does NOT collapse and stays explicit.

// FUN_14001baf0 — stride 0x20, align 4, byte copy, zero tail. (Not the
// citizenlist ProfessionResourceEntry row: that one seeds 100 at +0x08.)
using Pod20A4Elem = GameColl::Bytes<0x20, 4>;            // exe 0x14001baf0
static_assert(sizeof(Pod20A4Elem)  == 0x20, "Pod20A4Elem stride");
static_assert(alignof(Pod20A4Elem) == 4,    "Pod20A4Elem align");
using Pod20A4Array = Collection::Array<Pod20A4Elem>;

// FUN_14001bf10 — stride 0x14, align 4, byte copy, zero tail.
using Pod14A4Elem = GameColl::Bytes20x4;                 // exe 0x14001bf10
static_assert(sizeof(Pod14A4Elem)  == 0x14, "Pod14A4Elem stride");
static_assert(alignof(Pod14A4Elem) == 4,    "Pod14A4Elem align");
using Pod14A4Array = Collection::Array<Pod14A4Elem>;

// FUN_140001bb0 — stride 8, align 8: raw pointer slots, zeroed on every new
// slot (the heap-corruption note above Reimpl_Array_Resize_Stride08 says why
// that zero is load-bearing — `GameColl::Ptr8`'s `= nullptr` NSDMI IS that zero).
static_assert(sizeof(GameColl::Ptr8)  == 8, "Ptr8 stride");   // exe 0x140001bb0
static_assert(alignof(GameColl::Ptr8) == 8, "Ptr8 align");
using Ptr8ZeroArray = Collection::Array<GameColl::Ptr8>;

// FUN_14000c520 — SpawnDescription entry {ExternalPtr @0, float minSpawnAge
// @8, float maxSpawnAge @0xc}, stride 0x10, align 8. Whole-stride copy; the
// tail seed is {0, 0.0f, 10.0f} — see the banner above
// Reimpl_Array_Resize_Stride10_Spawn for why 10.0f is load-bearing.
void InitSpawnEntry(uint8_t* e) {
    *reinterpret_cast<uint64_t*>(e + 0x00) = 0;      // ExternalPtr
    *reinterpret_cast<float*>(e + 0x08)    = 0.0f;   // minSpawnAge
    *reinterpret_cast<float*>(e + 0x0c)    = 10.0f;  // maxSpawnAge (0x41200000)
}
// stays explicit: `init` is a patterned fill (maxSpawnAge = 10.0f at +0x0c), not
// the zero/as-found tail any element type's `T()` would write.
const rtcoll::ArraySpec kSpec_SpawnEntry = {             // exe 0x14000c520
    "Array<SpawnEntry>", 0x10, 8,
    &ByteAssign<0x10>, &InitSpawnEntry, nullptr, true,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,
    /*seed zero       */ 0, 0 };
using SpawnEntryArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                          Collection::ExplicitSpec<&kSpec_SpawnEntry>>;

} // namespace

// ============================================================
// Helpers #3-#5 — Array resize variants
// ============================================================
extern "C" void Reimpl_Array_Resize_Stride20(void* arrayHeader, int newCount) {
    if (!arrayHeader) return;
    // B.3.2 bridge: the Geometry/Struct60B headers this serves are still raw bytes.
    reinterpret_cast<Pod20A4Array*>(arrayHeader)->SetSize(newCount);
}

// NOTE (heap-corruption fix): the stride-0x08 array holds raw 8-byte POINTERS
// (e.g. the Geometry +0x210 External*/MaterialInstance ref array). The original
// FUN_140001bb0 zeroes EVERY new slot (slot >= oldCount) on BOTH the in-place
// grow and realloc paths — see Ghidra decompile, the `else if (puVar1) *puVar1=0`
// branch runs unconditionally. This is essential because
// System::MemoryAllocator::Allocate (Runtime 0x1800c6e60) returns DIRTY memory
// (boundary-tag allocator, no zero-fill); a non-zeroed new pointer slot would
// retain a STALE pointer from a previously-freed block. A later serialize pass
// (sub_0150 / ArraySerializeExternalGeneric) then treats that stale pointer as a
// live External* and runs External::GetPath / Filename refcount ops on the
// already-freed System::String — incrementing the refcount slot at (strPtr-4),
// which is exactly (freedBlock+4), i.e. the high dword of the allocator's
// free-list link → the deterministic +0x100000000 bit-32 flip → wild allocator
// write → menu heap crash.
//
// (Historical: the shared GenericArrayResize once zeroed new slots only on
// the realloc path, which is why stride-8 got its own body. Both now run
// SetSizeCore, whose `init` arm is unconditional for every slot >= oldCount.)
extern "C" void Reimpl_Array_Resize_Stride08(void* arrayHeader, int newCount) {
    if (!arrayHeader) return;
    // B.3.2 bridge: Geometry+0x210 and friends are still raw headers.
    reinterpret_cast<Ptr8ZeroArray*>(arrayHeader)->SetSize(newCount);
}

extern "C" void Reimpl_Array_Resize_Stride14(void* arrayHeader, int newCount) {
    if (!arrayHeader) return;
    // B.3.2 bridge: the Struct60B +0x08 sub-array header is still raw bytes.
    reinterpret_cast<Pod14A4Array*>(arrayHeader)->SetSize(newCount);
}

// ============================================================
// Spawn-entry array resize — FUN_14000c520 (stride 0x10)
// ============================================================
// The growable array behind SpawnDescription+0x20: elements are
// { ExternalPtr @+0x00 (8B), float minSpawnAge @+0x08, float maxSpawnAge @+0x0c }.
// It cannot use GenericArrayResize because a NEW element is not zero-filled: the
// original writes ptr=0, min=0.0f, and max = 0x41200000 = **10.0f** (see the
// `*(undefined4*)((longlong)puVar9 + 0xc) = 0x41200000` stores in all four
// unrolled arms and the scalar tail of the decompile). That 10.0f is a real
// default — a spawn entry with no authored max age is eligible up to age 10 —
// so zero-filling instead would give every defaulted entry the window [0,0] and
// silently disable it.
//
// ⚠ This address was implemented in spawndescription.cpp but never registered,
// which is why SpawnDescription::Serialize read the entry count into a LOCAL,
// found `resize` null, and left the description at count=0 with a null array:
// every tree carried an EMPTY spawn table, so no wild food could ever spawn and
// the Gatherer's Hut had nothing it accepts anywhere on the map. Registered in
// address_registry.cpp alongside the other resize variants.
extern "C" void Reimpl_Array_Resize_Stride10_Spawn(void* arrayHeader, int newCount) {
    if (!arrayHeader) return;
    // B.3.2 bridge: SpawnDescription+0x20 is still a raw header.
    reinterpret_cast<SpawnEntryArray*>(arrayHeader)->SetSize(newCount);
}

// ============================================================
// Helper #6 — Array_Serialize stride 0x20 (FUN_14001bbf0)
// ============================================================
//
// Per element (0x20 bytes = 7 floats + 2 shorts):
//   +0x00, +0x04, +0x08, +0x0c, +0x10, +0x14, +0x18 — 7 floats
//   +0x1c, +0x1e — 2 shorts
//
// In the decompile pfVar1 is offset to (base+8) so the relative writes are
// pfVar1[-2..+4] = 7 floats and pfVar1[+5] / +0x16 = the two shorts.
extern "C" void Reimpl_Array_Serialize_Stride20(void* serializer,
                                                uint32_t count,
                                                void* arrayBase) {
    if (!serializer || !arrayBase || count == 0) return;

    // ENDGAME B.3.4a closing: the `!g_fn_Serialize_Bool` probe (it never called
    // that door, it only "confirmed serialize plumbing") and its soft-fail arm
    // are gone with the door.
    auto* base = reinterpret_cast<uint8_t*>(arrayBase);
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t* elem = base + static_cast<size_t>(i) * 0x20;
        // Use Serialize_Uint with a float* cast — both serialize 4-byte fields
        // through the same encoder-vtable slot for raw types. Original passes
        // them as floats, but the wire format is identical for (uint32, float).
        for (int off = 0; off < 7; ++off) {
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(
                *reinterpret_cast<uint32_t*>(elem + off * 4), nullptr);   // B.3.4 bridge
        }
        // Two shorts at +0x1c and +0x1e — Serializer has a short overload
        // (slot vtable+0x80 in the original). For lack of a dedicated wired
        // primitive, treat them as a single uint32 for now. This is a TODO
        // if tests reveal endianness issues.
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(
            *reinterpret_cast<uint32_t*>(elem + 0x1c), nullptr);   // B.3.4 bridge
    }
}

// ============================================================
// Generic shared body for ExternalPtr-array serialize (#7 / #8)
// ============================================================
//
// Identical to the single-ref helper (in serialize_externals.cpp) but loops
// `count` times, treating param_3 as an array of External* at stride 8.
//
// Cascade safety: when typeIdPtr is null OR the underlying typeId is 0 (Runtime
// hasn't registered the type yet), we skip the FindOrLoad call and the
// IsDerivedFrom validation. This prevents NullFactory crashes during early
// load and matches the "graceful no-op" pattern from
// feedback_factory_cascade_strategy.md.
namespace {

void ArraySerializeExternalGeneric(void* serializer, int count, void* arrayBase,
                                   const uint32_t* typeIdPtr,
                                   const char* typeName) {
    if (!serializer || count <= 0 || !arrayBase) return;

    // Required Runtime primitives. If any are missing, log once and bail.
    if (!::Object::ExternalFactory::_singleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Array::Serialize_External<%s>] runtime primitives unwired — "
                   "skipping %d elements (cascade-safe no-op)\n",
                   typeName ? typeName : "?", count);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    void* factory = ::Object::ExternalFactory::_singleton;
    // ENDGAME B.3.3 step 6A: member calls on the real Object::ExternalFactory.
    // The singleton is still read through the EXISTING door, so the guards
    // above still gate the same cell and the same registered body still runs.
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);   // B.3.3 bridge

    // Resolve typeId — null/zero means "type not yet registered". Skip
    // IsDerivedFrom validation in that case.
    uint32_t typeId = 0;
    bool typeIdResolved = false;
    if (typeIdPtr) {
        typeId = *typeIdPtr;
        typeIdResolved = (typeId != 0);
    }
    // Lazy resolution path — see serialize_externals.cpp for rationale.
    if (!typeIdResolved && typeName) {
        const char* simple = typeName;
        for (const char* p = typeName; *p; ++p) {
            if (p[0] == ':' && p[1] == ':') simple = p + 2;
        }
        typeId = ::Object::ResolveTypeIdByNameLazy(simple);
        typeIdResolved = (typeId != 0);
    }
    if (!typeIdResolved) {
        static bool s_typeWarned = false;
        if (!s_typeWarned) {
            printf("[Array::Serialize_External<%s>] typeId not yet registered — "
                   "loading without type validation (count=%d)\n",
                   typeName ? typeName : "?", count);
            fflush(stdout);
            s_typeWarned = true;
        }
    }

    // Encoding state. Slot vtable+0x10 returns 3 for "saving in
    // export-by-path" mode where we walk existing pointers through GetPath +
    // FindOrLoad without serializing anything.
    IO::Encoding* encoding = const_cast<IO::Encoding*>(
        &reinterpret_cast<IO::Serializer*>(serializer)->GetEncoding());   // B.3.4 bridge
    if (!encoding) return;
    // Encoding vtable +0x10 is slot 2 == IO::Encoding::GetKind() (B34_RULES
    // §1.1: the Encoding table has NO destructor slot, so IsBinary/IsText/
    // GetKind are slots 0/1/2). Unlike `serializer + 8`, this really IS the
    // Encoding, so the member call is the same dispatch, not a different one.
    int encState = encoding->GetKind();   // B.3.4 bridge  (+0x10 = Encoding::GetKind)

    // Each element of arrayBase is an External* slot (8 bytes).
    auto* slots = reinterpret_cast<void**>(arrayBase);
    for (int i = 0; i < count; ++i) {
        void** slot = &slots[i];

        if (encState == 3) {
            // Path-export mode: only walk existing pointers, never write.
            if (!*slot) continue;
            // `GetPath` returns by value through a hidden return slot and the
            // callee CONSTRUCTS into it; copy-initialising the local is the
            // same store.
            System::Filename pathBuf =
                reinterpret_cast<const ::Object::External*>(*slot)->GetPath();  // B.3.3 bridge
            const void* found = efactory->FindOrLoad(pathBuf);
            if (found && typeIdResolved) {
                ::Object::IsDerivedFromOrTrust(const_cast<void*>(found), typeId);
            }
            continue;
        }

        // Load/save mode. Build a temporary Filename used to round-trip the
        // path through the serializer.
        System::Filename fnameBuf;

        bool refMode = efactory->IsReferenceModeEnabled();

        if (refMode) {
            // Reference-mode: serialize a uint reference id.
            uint32_t refId = 0xFFFFFFFFu;
            if (*slot) {
                refId = efactory->GetReferenceId(
                    reinterpret_cast<const ::Object::External*>(*slot));   // B.3.3 bridge
            }

            // Branch decision matches original's:
            //   if (slot==NULL || *(int*)(*serializer + 8) != 0 || refId != UINT_MAX)
            //       Serialize(refId)
            //       if (refId != UINT_MAX) {
            //           if loading: GetPathForReferenceId + Filename::operator=
            //           goto loadPathBranch
            //       }
            //   else Serialize(refId)  // also UINT_MAX
            int serializerInner = *reinterpret_cast<int*>(
                reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);

            bool needsPathSerialize = false;
            if (!*slot || serializerInner != 0 || refId != 0xFFFFFFFFu) {
                reinterpret_cast<IO::Serializer*>(serializer)->Serialize(refId, nullptr);   // B.3.4 bridge
                if (refId != 0xFFFFFFFFu) {
                    if (serializerInner == 0) {
                        // Loading path: ask the factory for the actual path
                        // and assign into our temp Filename.
                        System::Filename tmpFn = efactory->GetPathForReferenceId(refId);
                        fnameBuf = tmpFn;
                    }
                    needsPathSerialize = true;
                }
            } else {
                reinterpret_cast<IO::Serializer*>(serializer)->Serialize(refId, nullptr);   // B.3.4 bridge
            }

            if (needsPathSerialize) {
                int innerCheck = *reinterpret_cast<int*>(
                    reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
                if (innerCheck == 0) {
                    void* loaded = const_cast<::Object::External*>(
                        efactory->FindOrLoad(fnameBuf));                        // B.3.3 bridge
                    void* result = nullptr;
                    if (loaded && (!typeIdResolved || ::Object::IsDerivedFromOrTrust(loaded, typeId))) {
                        result = loaded;
                    }
                    *slot = result;
                }
            }
        } else {
            // Plain mode: serialize a bool flag, then a string path if true.
            bool hasRef = (*slot != nullptr);
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(hasRef, nullptr);   // B.3.4 bridge
            if (hasRef) {
                if (*slot) {
                    System::Filename pathBuf =
                        reinterpret_cast<const ::Object::External*>(*slot)->GetPath();  // B.3.3 bridge
                    fnameBuf = pathBuf;
                }
                fnameBuf.AsString().Serialize(serializer);

                int innerCheck = *reinterpret_cast<int*>(
                    reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serializer)) + 8);
                if (innerCheck == 0) {
                    void* loaded = const_cast<::Object::External*>(
                        efactory->FindOrLoad(fnameBuf));                        // B.3.3 bridge
                    void* result = nullptr;
                    if (loaded && (!typeIdResolved || ::Object::IsDerivedFromOrTrust(loaded, typeId))) {
                        result = loaded;
                    }
                    *slot = result;
                }
            }
        }

    }
}

} // namespace

// ============================================================
// Helpers #7 / #8 — Array_Serialize_External (CASCADE)
// ============================================================
//
// #7 GraphicsMesh exref array (typeInfo @ slot 0x140151f20)
extern "C" void Reimpl_Array_Serialize_External_GraphicsMesh(
    void* serializer, int count, void* arrayBase) {


    const uint32_t tid = Graphics::GraphicsMesh::TypeId();   // ENDGAME B.3.9 TR
    ArraySerializeExternalGeneric(serializer, count, arrayBase,
                                  tid ? &tid : nullptr, "Graphics::GraphicsMesh");
}

// #8 Particle exref array (typeInfo @ slot 0x140151420)
extern "C" void Reimpl_Array_Serialize_External_Particle(
    void* serializer, int count, void* arrayBase) {


    const uint32_t tid = Graphics::Particle::TypeId();   // ENDGAME B.3.9 TR
    ArraySerializeExternalGeneric(serializer, count, arrayBase,
                                  tid ? &tid : nullptr, "Graphics::Particle");
}

// ============================================================
// STANDALONE_EXE: address registry hooks
// ============================================================
//
// In standalone we do NOT have the original exe's code mapped. The
// fn_Geometry_Init / fn_Array_Resize_* / etc. function pointers above stay
// null when ResolveAddr can't find them, and the Reimpl_* fallbacks above
// kick in. To wire the registry up, see standalone/address_registry.cpp.
