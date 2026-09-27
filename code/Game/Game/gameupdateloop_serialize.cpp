// code/Game/Game/gameupdateloop_serialize.cpp
// Save/load wave 2026-08-27 — the SaveRestore keystone.
//
// GameUpdateLoop vtable +0x38 SaveRestore (exe 0x1400959c0) is what the engine
// calls in BOTH directions:
//   SAVE: Save::SaveLoad::UpdateSave (Runtime 0x1800c2a80, state 1) does
//         currentLoop->vtbl[+0x38](loop, 1, savedState, &out), then Persistence
//         async-writes <Documents>/Banished/Save/<name>.sav.
//   LOAD: retail GameUpdateLoop::Load (0x140096b00) calls its own +0x38 slot at
//         0x14009754c with save=0 when m_savedState (+0x58) is non-null.
// Until this file, the standalone slot was a no-op stub — a save completed
// "successfully" but wrote a 79KB file (metadata + screenshot, no world) vs
// retail's ~3.4MB.
//
// Ported here:
//   0x1400959c0  SaveRestore wrapper — CompressedMemoryStream around 95a90;
//                load: SavedState::Retrieve → Open; save: Create → Close →
//                SavedState::Store.
//   0x140095a90  master GameUpdateLoop serializer (~20 subsystem sections).
//   0x1400952c0  event-log serialize (m_eventNotifier +0x138; 0x28-stride
//                entries {uint,String,5 ints}, capped at the last 200).
//   0x1400199a0  event-log array resize (helper of 952c0).
//   0x140093c90  weather-sim serialize (m_weather +0x130; env ExternalPtr,
//                params, per-effect-slot active flag + 2 floats; on load
//                re-creates active effects via WeatherEffect_SetEnabled).
//   0x140102920  EntityUI serialize (m_entityUI +0x120: paused/speed/times,
//                goal-ref tree, IBP-keyed AVL, saved camera views 1-5).
//   0x14002e2e0  goal-ref tree serialize (helper of 102920).
//   0x14002e470  IBP-keyed AVL serialize (helper of 102920).
//   0x14002e000  camera-view array resize, stride 0x1c (helper of 102920).
//   0x1400a57f0  minimap serialize (m_imageCapture +0x148: two uint32 pixel
//                arrays + width; on load re-creates GPU buffers + uploads —
//                delegated to Minimap_CreateAndUpload which ports that tail).
//   0x1400c4730  EntityList post-load pass (per-component vcall +0x58, entity
//                backptr, update-list enrolment via DynArray8_ResizeFill).
//
// Ported elsewhere (parallel wave lanes, called via ResolveAddr):
//   0x14008f760/0x14008f580  CitizenBehavior serialize + post-pass (+0xF0)
//   0x14009f100              game map-grid serialize (+0xF8)
//   0x1400f9240              goal/tutorial-mgr serialize (+0x118)
// Already ported before this wave:
//   0x1400194a0/0x140019ec0/0x14001a0c0 (config ExternalPtrs), 0x1400988c0
//   (camera/PlayerController), 0x1400c44a0 (EntityList::Serialize),
//   0x1400b04d0 (WorldGeometry).

#include "Game/GameUpdateLoop.h"   // caller batch CB6: SaveRestoreOut, the named fields
#include "Entity/ComponentEntity.h"   // caller batch CB6: TypeId()
#include "Component/Component.h"      // caller batch CB6: slot 11
#include "engine_funcs.h"
#include "options.h"
#include "resolve.h"
#include "system/String.h"
#include "collection/Collection.h"
#include "Component/coll_bridge.h"
#include <new>
#include "address_registry.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>   // QueryPerformanceCounter for the BANISHED_SAVE_TIMING probe
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include "io/Stream.h"       // ENDGAME B.3.9 IO: IO::CompressedMemoryStream
#include "graphics/AnimationManager.h"   // ENDGAME B.3.5 Video/Graphics caller batch
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)
#include "ui/UserInterface.h"  // ENDGAME B.3.6 UI callers (K4)
#include "achievement/AchievementManager.h"   // ENDGAME B.3.7 callers (K2)
#include "save/SaveLoad.h"                    // ENDGAME B.3.7 callers (K2)
#include "audio/Audio.h"                      // ENDGAME B.3.7 callers (K4)
#include "object/External.h"   // ENDGAME B.3.9 SS2: Object::ExternalFactory::_singleton

// code/Game/Game/minimap_upload.cpp — ports FUN_1400a4e80 + the CopyToTexture tail.
extern "C" void Minimap_CreateAndUpload(void* mapObj);
// code/Game/Game/weather_particles.cpp — ports FUN_1400942c0 (enable half; the
// ambient-audio entity spawn is not ported there yet — see WeatherSim_Serialize).
extern "C" void WeatherEffect_SetEnabled(void* sim, int slot, int enable);
// code/Game/Game/serialize_externals.cpp — generic ExternalPtr<T>::Serialize body;
// EnvironmentDescription variant added for this wave (FUN_1400192a0).
extern "C" void Reimpl_ExternalPtr_Serialize_EnvironmentDesc(void* refField, void* serializer);

namespace {

// ------------------------------------------------------------
// Runtime exports resolved locally (not in engine_funcs)
// ------------------------------------------------------------
typedef void  (*fn_EnableRefMode_t)(void* extFactory, bool on);
typedef void  (*fn_SerializeRefTable_t)(void* extFactory, void* stream);
typedef void  (*fn_ResetPersist_t)(void* intFactory, void* stream);
typedef void  (*fn_ClearPersist_t)(void* intFactory);

// ENDGAME B.3.9 IO (W2): the six IO::CompressedMemoryStream door typedefs and
// their resolved statics are gone -- SaveRestoreOut and the roundtrip check
// build the real class (runtime/types/io/Stream.h) and call its members.


static fn_EnableRefMode_t       s_fn_EnableRefMode = nullptr;
static fn_SerializeRefTable_t   s_fn_SerializeRefTable = nullptr;
static fn_ResetPersist_t        s_fn_ResetPersist = nullptr;
static fn_ClearPersist_t        s_fn_ClearPersist = nullptr;

static const uint32_t*          s_pAchInstTypeId = nullptr; // AchievementInstance BaseTypeInfo (typeId at +0)

// ------------------------------------------------------------
// Exe helpers already ported+registered (called via the registry)
// ------------------------------------------------------------
typedef void (*fn_FieldSer_t)(void* fieldPtr, void* serializer);   // config ExternalPtr serializers
typedef void (*fn_ObjStream_t)(void* obj, void* stream);           // (object, raw stream) serializers
typedef void (*fn_ObjStream4_t)(void* obj, void* stream, void* p3, int* out);
typedef void (*fn_ObjOnly_t)(void* obj);
typedef void (*fn_DynArr8_ResizeFill_t)(void* hdr, int newCount, void* ibp);      // 0x140003110
typedef void (*fn_IntArrResize_21d0_t)(void* hdr, int newCount);                  // 0x1400021d0
typedef void (*fn_BuildCompRef_Ser_t)(void* refField, void* serializer);          // 0x140002340
typedef void (*fn_GoalTreeInsert_t)(void* tree, void* outNode, void* ibp);        // 0x140014890
typedef void (*fn_AvlFixup_5920_t)(void* tree, void* nodeRef);                    // 0x140025920

static fn_FieldSer_t   s_fn_CfgSer_194a0 = nullptr;   // +0x50
static fn_FieldSer_t   s_fn_CfgSer_19ec0 = nullptr;   // +0x40
static fn_FieldSer_t   s_fn_CfgSer_1a0c0 = nullptr;   // +0x48
static fn_ObjStream_t  s_fn_Camera_988c0 = nullptr;   // +0xE0 PlayerController
static fn_ObjStream_t  s_fn_EntList_c44a0 = nullptr;  // +0xE8 EntityList::Serialize
static fn_ObjStream_t  s_fn_Geom_b04d0 = nullptr;     // +0x110 WorldGeometry
static fn_ObjStream4_t s_fn_CitList_8f760 = nullptr;  // +0xF0 CitizenBehavior
static fn_ObjStream_t  s_fn_Grid_9f100 = nullptr;     // +0xF8 game map-grid
static fn_ObjStream4_t s_fn_Goals_f9240 = nullptr;    // +0x118 goal manager
static fn_ObjOnly_t    s_fn_CitPost_8f580 = nullptr;  // +0xF0 post-pass
static fn_FieldSer_t   s_fn_EnvPtrSer_192a0 = nullptr;// weather +0x00 EnvironmentDescription ptr
static fn_DynArr8_ResizeFill_t s_fn_DynArr8_ResizeFill = nullptr;
static fn_IntArrResize_21d0_t  s_fn_IntArrResize = nullptr;
static fn_BuildCompRef_Ser_t   s_fn_BuildCompRefSer = nullptr;
static fn_GoalTreeInsert_t     s_fn_GoalTreeInsert = nullptr;
static fn_AvlFixup_5920_t      s_fn_AvlFixup_5920 = nullptr;

static bool s_resolved = false;

void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;

    s_fn_EnableRefMode = reinterpret_cast<fn_EnableRefMode_t>(
        ResolveExport("?EnableReferenceMode@ExternalFactory@Object@@QEAAX_N@Z"));
    s_fn_SerializeRefTable = reinterpret_cast<fn_SerializeRefTable_t>(
        ResolveExport("?SerializeReferenceTable@ExternalFactory@Object@@QEAAXAEAVStream@IO@@@Z"));
    s_fn_ResetPersist = reinterpret_cast<fn_ResetPersist_t>(
        ResolveExport("?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z"));
    s_fn_ClearPersist = reinterpret_cast<fn_ClearPersist_t>(
        ResolveExport("?ClearPersistState@InternalFactory@Object@@QEAAXXZ"));

    s_pAchInstTypeId = reinterpret_cast<const uint32_t*>(
        ResolveExport("?_typeInfo@AchievementInstance@Achievement@@0VBaseTypeInfo@Object@@A"));

    auto R = [](uintptr_t a) { return ResolveAddr(Module::Exe, a); };
    s_fn_CfgSer_194a0 = reinterpret_cast<fn_FieldSer_t>(R(0x1400194a0));
    s_fn_CfgSer_19ec0 = reinterpret_cast<fn_FieldSer_t>(R(0x140019ec0));
    s_fn_CfgSer_1a0c0 = reinterpret_cast<fn_FieldSer_t>(R(0x14001a0c0));
    s_fn_Camera_988c0 = reinterpret_cast<fn_ObjStream_t>(R(0x1400988c0));
    s_fn_EntList_c44a0 = reinterpret_cast<fn_ObjStream_t>(R(0x1400c44a0));
    s_fn_Geom_b04d0 = reinterpret_cast<fn_ObjStream_t>(R(0x1400b04d0));
    s_fn_CitList_8f760 = reinterpret_cast<fn_ObjStream4_t>(R(0x14008f760));
    s_fn_Grid_9f100 = reinterpret_cast<fn_ObjStream_t>(R(0x14009f100));
    s_fn_Goals_f9240 = reinterpret_cast<fn_ObjStream4_t>(R(0x1400f9240));
    s_fn_CitPost_8f580 = reinterpret_cast<fn_ObjOnly_t>(R(0x14008f580));
    s_fn_EnvPtrSer_192a0 = reinterpret_cast<fn_FieldSer_t>(R(0x1400192a0));
    s_fn_DynArr8_ResizeFill = reinterpret_cast<fn_DynArr8_ResizeFill_t>(R(0x140003110));
    s_fn_IntArrResize = reinterpret_cast<fn_IntArrResize_21d0_t>(R(0x1400021d0));
    s_fn_BuildCompRefSer = reinterpret_cast<fn_BuildCompRef_Ser_t>(R(0x140002340));
    s_fn_GoalTreeInsert = reinterpret_cast<fn_GoalTreeInsert_t>(R(0x140014890));
    s_fn_AvlFixup_5920 = reinterpret_cast<fn_AvlFixup_5920_t>(R(0x140025920));

    fprintf(stderr, "[GUL::Serialize] resolved: refMode=%p refTable=%p persist=%p/%p "
            "subs: cit=%p grid=%p goals=%p citPost=%p envPtr=%p\n",
            (void*)s_fn_EnableRefMode, (void*)s_fn_SerializeRefTable,
            (void*)s_fn_ResetPersist, (void*)s_fn_ClearPersist,
            (void*)s_fn_CitList_8f760,
            (void*)s_fn_Grid_9f100, (void*)s_fn_Goals_f9240,
            (void*)s_fn_CitPost_8f580, (void*)s_fn_EnvPtrSer_192a0);
    fflush(stderr);
}

inline bool IsSaving(void* stream) {
    return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(stream) + 8) == 1;
}

} // namespace

// ============================================================
// 0x1400199a0 — event-log array resize (stride 0x28)
// ============================================================
// hdr = { void* data; int count; int cap }. Element: { uint id; String text;
// int a,b,c,d,e }. Grow reallocs (count+1 growth triples the cap, matching the
// generic resize family); shrink String-dtors the tail; realloc copies via
// String copy-ctor and dtors+frees the old block. New elements init to
// { -1, null-String, 0, 0, -1, -1, -1 }.
//
// ENDGAME B.3.2 step 2: this body and CamViewArray_Resize_e000 below WERE
// hand-written copies of `rtcoll::SetSizeCore` (runtime/src/collection/
// array.cpp); each is now one `Collection::Array<>::SetSize` over its own
// `ArraySpec` row. Compared arm for arm: exact-equal early return, `_size`
// stored before the capacity test, treble-on-single-append-from-non-empty,
// copy-construct the preserved prefix only when the block moved, init the new
// tail, destroy the truncated tail in place / the whole old buffer on a
// realloc, then free. The old realloc arm was gated on `oldData != null`;
// SetSizeCore runs destroy over `oldSize` elements (zero when the buffer was
// never allocated) and hands the null to the null-tolerant 0x1800c7ad0 —
// same effect. The one deliberate difference: the allocation now goes
// through the LOCKING static `MemAlloc` (0x1800c7a50) instead of the unlocked
// instance `Allocate` reached through `fn_MemAlloc` — same allocator, same
// heap, plus the critical section retail's own Collection code takes.
namespace {

// Event entry {uint id @0, String text @8, int a,b,c,d,e @0x10..0x23},
// stride 0x28, align 8. Copy-construct writes +0x00..+0x03 and +0x08..+0x23;
// init writes the same bytes. +0x04..+0x07 (String alignment pad) and
// +0x24..+0x27 are touched by NEITHER arm — the `unwritten` pair names the
// trailing hole; +0x04..+0x07 is the second, non-contiguous one. The String
// copy makes byteCopy=false and the String dtor is the destroy.
void AssignEventEntry(uint8_t* dst, const uint8_t* src) {
    *reinterpret_cast<uint32_t*>(dst) = *reinterpret_cast<const uint32_t*>(src);
    // B.3.1 bridge: the event entry is still untyped bytes.
    new (dst + 0x8) System::String(*reinterpret_cast<const System::String*>(src + 0x8));
    memcpy(dst + 0x10, src + 0x10, 5 * 4);
}
// ⚠ Seed values are load-bearing: id=-1, c=d=e=-1, a=b=0.
void InitEventEntry(uint8_t* dst) {
    *reinterpret_cast<uint32_t*>(dst) = 0xffffffff;
    *reinterpret_cast<uint64_t*>(dst + 0x8) = 0;                 // null String
    *reinterpret_cast<uint64_t*>(dst + 0x10) = 0;                // a=b=0
    *reinterpret_cast<uint64_t*>(dst + 0x18) = 0xffffffffffffffff; // c=d=-1
    *reinterpret_cast<uint32_t*>(dst + 0x20) = 0xffffffff;       // e=-1
}
void DestroyEventEntry(void* /*ignoredThis*/, uint8_t* base, int count) {
    for (int i = 0; i < count; i++)
        reinterpret_cast<System::String*>(base + static_cast<size_t>(i) * 0x28 + 0x8)->~String();
}
const rtcoll::ArraySpec kSpec_EventEntry = {             // exe 0x1400199a0
    "Array<EventLogEntry>", 0x28, 8,
    &AssignEventEntry, &InitEventEntry, &DestroyEventEntry, false,
    /*assign unwritten*/ 0x24, 0x28,   // plus +0x04..+0x07
    /*init   unwritten*/ 0x24, 0x28,   // plus +0x04..+0x07
    /*seed zero       */ 0,    0 };
using EventEntryArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                          Collection::ExplicitSpec<&kSpec_EventEntry>>;

// Camera view {Vector3 pos, float a, float b, float c, bool used}, stride
// 0x1c, align 4. Copy writes +0x00..+0x18 and leaves +0x19..+0x1b (retail's
// 4x-unrolled dword loop plus the bool); init memsets the whole stride.
// Partial copy, so byteCopy=false; trivially destructible.
void AssignCamView(uint8_t* dst, const uint8_t* src) {
    memcpy(dst, src, 0x18);
    dst[0x18] = src[0x18];
}
void InitCamView(uint8_t* dst) { memset(dst, 0, 0x1c); }
const rtcoll::ArraySpec kSpec_CamView = {                // exe 0x14002e000
    "Array<CameraView>", 0x1c, 4,
    &AssignCamView, &InitCamView, nullptr, false,
    /*assign unwritten*/ 0x19, 0x1c,
    /*init   unwritten*/ 0,    0,
    /*seed zero       */ 0,    0 };
using CamViewArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                       Collection::ExplicitSpec<&kSpec_CamView>>;

}  // namespace

extern "C" void EventLog_Resize_199a0(void* hdrVoid, int newCount) {
    // B.3.2 bridge: the event notifier's +0x18 header is still raw bytes.
    reinterpret_cast<EventEntryArray*>(hdrVoid)->SetSize(newCount);
}

// ============================================================
// 0x1400952c0 — event-log serialize (GUL+0x138 object)
// ============================================================
// Layout: +0x10 bool[6] filter flags; +0x18 {data,count,cap} entry array
// (stride 0x28). Version-1 section; entries capped at the LAST 200.
extern "C" void EventLog_Serialize_952c0(void* selfVoid, void* stream) {
    EnsureResolved();
    uintptr_t self = (uintptr_t)selfVoid;

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(6, reinterpret_cast<bool*>(self + 0x10), nullptr);   // B.3.4 bridge
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() > 0) {   // B.3.4 bridge
        uint32_t total = *(uint32_t*)(self + 0x20);
        uint32_t capped = (int)total > 200 ? 200u : total;
        int skip = (int)(total - capped);
        if (skip < 0) skip = 0;
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(&capped), nullptr);   // B.3.4 bridge
        if (!IsSaving(stream)) {
            EventLog_Resize_199a0((void*)(self + 0x18), (int)capped);
        }
        for (int i = skip; i < *(int*)(self + 0x20); i++) {
            uint8_t* e = *(uint8_t**)(self + 0x18) + (size_t)i * 0x28;
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(e), nullptr);   // B.3.4 bridge
            reinterpret_cast<System::String*>(e + 0x8)->Serialize(ser);
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(e + 0x10), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(e + 0x14), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(e + 0x18), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(e + 0x1c), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(e + 0x20), nullptr);   // B.3.4 bridge
        }
    }
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// 0x140093c90 — weather-sim serialize (GUL+0x130 object, 0x78 bytes)
// ============================================================
// Layout: +0x00 EnvironmentDescription ExternalPtr; +0x08 ExternalPtr (config,
// serialized via the already-ported 0x1400194a0); +0x10/+0x14 ints; +0x18
// float; +0x1c Vector4; +0x2c,+0x30,+0x44,+0x48 floats; +0x4c int; +0x50
// {geo*}[count at +0x58] active-effect slots. On load an active slot is
// re-created via the ported WeatherEffect_SetEnabled(sim, i, 1) — NOTE: the
// retail create (FUN_1400942c0) also spawns the effect's ambient-audio entity;
// that half is not in the port yet (audio ambience will be silent after load).
// Then each active geo's +0xc4 (intensity) and +0xc0 floats round-trip.
extern "C" void WeatherSim_Serialize_93c90(void* selfVoid, void* stream) {
    EnsureResolved();
    uintptr_t self = (uintptr_t)selfVoid;

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    if (s_fn_EnvPtrSer_192a0) s_fn_EnvPtrSer_192a0((void*)(self + 0x00), ser);
    else Reimpl_ExternalPtr_Serialize_EnvironmentDesc((void*)(self + 0x00), ser);
    s_fn_CfgSer_194a0((void*)(self + 0x08), ser);

    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(self + 0x10), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(self + 0x14), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(self + 0x18), nullptr);   // B.3.4 bridge
    // ?Serialize@Vector4@Math@@QEAAXAEAVSerializer@IO@@@Z  retail 0x18001d360
    reinterpret_cast<Math::Vector4*>(self + 0x1c)->Serialize(ser);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(self + 0x2c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(self + 0x30), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(self + 0x44), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(self + 0x48), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(self + 0x4c), nullptr);   // B.3.4 bridge

    int slotCount = *(int*)(self + 0x58);
    for (int i = 0; i < slotCount; i++) {
        void** slot = (void**)(*(uintptr_t*)(self + 0x50) + (size_t)i * 8);
        bool active = (*slot != nullptr);
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(active, nullptr);   // B.3.4 bridge
        if (active) {
            if (!IsSaving(stream)) {
                WeatherEffect_SetEnabled(selfVoid, i, 1);
            }
            uint8_t* geo = (uint8_t*)*slot;
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(geo + 0xc4), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(geo + 0xc0), nullptr);   // B.3.4 bridge
        }
    }
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// 0x14002e000 — camera-view array resize (stride 0x1c, align 4)
// ============================================================
// hdr = { void* data; int count; int cap }. Element: { Vector3 pos; float a;
// float b; float c; bool used } (0x1c bytes). Retail body is a 4x-unrolled
// copy loop — ported as a plain loop with identical semantics.
// ENDGAME B.3.2 step 2: see the kSpec_CamView row above EventLog_Resize_199a0.
extern "C" void CamViewArray_Resize_e000(void* hdrVoid, int newCount) {
    // B.3.2 bridge: EntityUI+0x70 is still a raw header.
    reinterpret_cast<CamViewArray*>(hdrVoid)->SetSize(newCount);
}

// ============================================================
// 0x14002e2e0 — goal-ref tree serialize (EntityUI +0x50)
// ============================================================
// Tree node (0x30+): +0x00 ExternalPtr-ish component ref (serialized via the
// ported 0x140002340), +0x08 int, +0x0c int, +0x10 bool, +0x18 parent,
// +0x20 left, +0x28 right. Count at tree hdr +0x8. On save: in-order walk
// serializing each node. On load: read count entries and insert via the
// ported 0x140014890.
extern "C" void GoalRefTree_Serialize_e2e0(void* treeVoid, void* serializer) {
    EnsureResolved();
    uintptr_t tree = (uintptr_t)treeVoid;

    int count = *(int*)(tree + 0x8);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(count, nullptr);   // B.3.4 bridge

    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        // LOAD: read `count` records, insert each
        for (int i = 0; i < count; i++) {
            struct { void* ref; int a; int b; bool used; } rec;
            rec.ref = nullptr; rec.a = 0; rec.b = 0; rec.used = false;
            s_fn_BuildCompRefSer(&rec.ref, serializer);
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(rec.a, nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(rec.b, nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(rec.used, nullptr);   // B.3.4 bridge
            void* outNode = nullptr;
            s_fn_GoalTreeInsert(treeVoid, &outNode, &rec.ref);
        }
    } else {
        // SAVE: leftmost-first in-order walk
        uintptr_t node = *(uintptr_t*)tree;
        if (!node) { return; }
        uintptr_t cur = node;
        while (*(uintptr_t*)(cur + 0x20)) cur = *(uintptr_t*)(cur + 0x20);
        while (cur) {
            s_fn_BuildCompRefSer((void*)cur, serializer);
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(cur + 0x8), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(cur + 0xc), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(cur + 0x10), nullptr);   // B.3.4 bridge
            // in-order successor
            uintptr_t next = *(uintptr_t*)(cur + 0x28);
            if (next) {
                while (*(uintptr_t*)(next + 0x20)) next = *(uintptr_t*)(next + 0x20);
                cur = next;
            } else {
                uintptr_t parent = *(uintptr_t*)(cur + 0x18);
                while (parent && *(uintptr_t*)(parent + 0x28) == cur) {
                    cur = parent;
                    parent = *(uintptr_t*)(parent + 0x18);
                }
                cur = parent;
            }
        }
    }
}

// ============================================================
// 0x14002e470 — IBP-keyed AVL serialize (EntityUI +0x60)
// ============================================================
// Node (0x38): +0x00 InternalBasePtr key (a ComponentEntity), +0x08 int,
// +0x0c int, +0x10 bool, +0x18 parent, +0x20 left, +0x28 right, +0x30 balance.
// Count at hdr +0x8. Load inserts by raw pointer order (CheckPtr'd), then
// rebalances via the ported 0x140025920.
extern "C" void IbpTree_Serialize_e470(void* treeVoid, void* serializer) {
    EnsureResolved();
    uintptr_t tree = (uintptr_t)treeVoid;
    const uint32_t s_tidCompEnt = ComponentEntity::TypeId();   // caller batch CB6: was a lazy by-name cache

    int count = *(int*)(tree + 0x8);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(count, nullptr);   // B.3.4 bridge

    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        // LOAD
        for (int i = 0; i < count; i++) {
            void* key = nullptr;
            int a = 0, b = 0;
            bool used = false;
            reinterpret_cast<Object::InternalBasePtr*>(&key)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
            if (key && s_tidCompEnt && !fn_IsDerivedFrom(key, s_tidCompEnt)) key = nullptr;
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(a, nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(b, nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(used, nullptr);   // B.3.4 bridge

            auto makeNode = [&](uintptr_t parent) -> uintptr_t {
                uint8_t* n = (uint8_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
                if (!n) return 0;
                *(void**)n = key;
                *(int*)(n + 0x8) = a;
                *(int*)(n + 0xc) = b;
                *(uint8_t*)(n + 0x10) = used ? 1 : 0;
                *(uintptr_t*)(n + 0x18) = parent;
                *(uintptr_t*)(n + 0x20) = 0;
                *(uintptr_t*)(n + 0x28) = 0;
                *(uint32_t*)(n + 0x30) = 0;
                return (uintptr_t)n;
            };

            uintptr_t root = *(uintptr_t*)tree;
            if (!root) {
                *(uintptr_t*)tree = makeNode(0);
                *(int*)(tree + 0x8) += 1;
                continue;
            }
            uintptr_t cur = root;
            bool inserted = false;
            bool duplicate = false;
            uintptr_t newNode = 0;
            while (!inserted) {
                void* curKey = *(void**)cur;
                if (key < curKey) {
                    uintptr_t l = *(uintptr_t*)(cur + 0x20);
                    if (!l) {
                        newNode = makeNode(cur);
                        *(uintptr_t*)(cur + 0x20) = newNode;
                        inserted = true;
                    } else cur = l;
                } else if (key > curKey) {
                    uintptr_t r = *(uintptr_t*)(cur + 0x28);
                    if (!r) {
                        newNode = makeNode(cur);
                        *(uintptr_t*)(cur + 0x28) = newNode;
                        inserted = true;
                    } else cur = r;
                } else { duplicate = true; break; }
            }
            if (duplicate) continue;
            uintptr_t fixupNode = newNode;
            s_fn_AvlFixup_5920(treeVoid, &fixupNode);
            *(int*)(tree + 0x8) += 1;
        }
    } else {
        // SAVE: in-order walk
        uintptr_t cur = *(uintptr_t*)tree;
        if (!cur) return;
        while (*(uintptr_t*)(cur + 0x20)) cur = *(uintptr_t*)(cur + 0x20);
        while (cur) {
            reinterpret_cast<Object::InternalBasePtr*>((void*)cur)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
            if (*(void**)cur && s_tidCompEnt && !fn_IsDerivedFrom(*(void**)cur, s_tidCompEnt))
                *(void**)cur = nullptr;
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(cur + 0x8), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(cur + 0xc), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(cur + 0x10), nullptr);   // B.3.4 bridge
            uintptr_t next = *(uintptr_t*)(cur + 0x28);
            if (next) {
                while (*(uintptr_t*)(next + 0x20)) next = *(uintptr_t*)(next + 0x20);
                cur = next;
            } else {
                uintptr_t parent = *(uintptr_t*)(cur + 0x18);
                while (parent && *(uintptr_t*)(parent + 0x28) == cur) {
                    cur = parent;
                    parent = *(uintptr_t*)(parent + 0x18);
                }
                cur = parent;
            }
        }
    }
}

// ============================================================
// 0x140102920 — EntityUI serialize (GUL+0x120 object, 0xB0 bytes)
// ============================================================
// Layout: +0x30 bool paused; +0x34 int speed; +0x3c/+0x40/+0x44 floats;
// +0x50 goal-ref tree; +0x60 IBP tree (v>0); +0x70 {data,count,cap} saved
// camera views, stride 0x1c: { Vector3 pos; float a; float b; float c;
// bool used } (v>1).
extern "C" void EntityUI_Serialize_102920(void* selfVoid, void* stream) {
    EnsureResolved();
    uintptr_t self = (uintptr_t)selfVoid;

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 2);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(self + 0x30), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(self + 0x34), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(self + 0x3c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(self + 0x40), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(self + 0x44), nullptr);   // B.3.4 bridge
    GoalRefTree_Serialize_e2e0((void*)(self + 0x50), ser);
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() > 0) {   // B.3.4 bridge
        IbpTree_Serialize_e470((void*)(self + 0x60), ser);
    }
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() > 1) {   // B.3.4 bridge
        uint32_t count = *(uint32_t*)(self + 0x78);
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(&count), nullptr);   // B.3.4 bridge
        if (!IsSaving(stream)) {
            CamViewArray_Resize_e000((void*)(self + 0x70), (int)count);
        }
        for (uint32_t i = 0; i < count; i++) {
            uint8_t* e = *(uint8_t**)(self + 0x70) + (size_t)i * 0x1c;
            // ?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z  retail 0x1800150e0
            reinterpret_cast<Math::Vector3*>(e)->Serialize(ser);
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(e + 0xc), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(e + 0x10), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*(float*)(e + 0x14), nullptr);   // B.3.4 bridge
            reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(e + 0x18), nullptr);   // B.3.4 bridge
        }
    }
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// 0x1400a57f0 — minimap serialize (GUL+0x148 object, 0x38 bytes)
// ============================================================
// Layout: +0x04 int width; +0x08/+0x10 ImageBuffer*; +0x18 {data,count,cap}
// terrain pixels; +0x28 {data,count,cap} overlay pixels. Version 0. On load,
// the GPU buffers are (re)created and both arrays uploaded — that tail is the
// already-ported Minimap_CreateAndUpload (FUN_1400a4e80 + Lock/Copy/Unlock).
extern "C" void Minimap_Serialize_a57f0(void* selfVoid, void* stream) {
    EnsureResolved();
    uintptr_t self = (uintptr_t)selfVoid;

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // overlay array (+0x28) first — matches the retail order
    int n = *(int*)(self + 0x30);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(n, nullptr);   // B.3.4 bridge
    if (!IsSaving(stream)) s_fn_IntArrResize((void*)(self + 0x28), n);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(n, reinterpret_cast<unsigned int*>(*(void**)(self + 0x28)), nullptr);   // B.3.4 bridge

    // terrain array (+0x18)
    n = *(int*)(self + 0x20);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(n, nullptr);   // B.3.4 bridge
    if (!IsSaving(stream)) s_fn_IntArrResize((void*)(self + 0x18), n);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(n, reinterpret_cast<unsigned int*>(*(void**)(self + 0x18)), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(self + 0x4), nullptr);   // B.3.4 bridge

    if (!IsSaving(stream)) {
        Minimap_CreateAndUpload(selfVoid);
    }
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// 0x1400c4730 — EntityList post-load pass
// ============================================================
// LOAD ONLY. For each entity: vcall +0x58 (post-load fixup) on every
// component, write the EntityList backptr at entity+0x10, then enrol the
// entity in the two update lists (+0x88 per-frame, +0x98 per-tick) when its
// flag word +0x40 has bit 2 / bit 4 set (DynArray8_ResizeFill append).
extern "C" void EntityList_PostLoad_c4730(void* elVoid, void* stream) {
    EnsureResolved();
    uintptr_t el = (uintptr_t)elVoid;
    if (*(int*)((uintptr_t)stream + 8) != 0) return;   // save direction: no-op

    int count = *(int*)(el + 0x10);
    for (int i = 0; i < count; i++) {
        void** entSlot = (void**)(*(uintptr_t*)(el + 0x8) + (size_t)i * 8);
        uintptr_t ent = (uintptr_t)*entSlot;

        int compCount = *(int*)(ent + 0x38);
        for (int j = 0; j < compCount; j++) {
            void** compSlot = (void**)(*(uintptr_t*)(ent + 0x30) + (size_t)j * 8);
            uintptr_t comp = (uintptr_t)*compSlot;
            // Caller batch CB6: slot 11 (vtable +0x58) of an entity component-array
            // element (a proved Component).
            reinterpret_cast<Component*>(comp)->Slot11();   // B.3.8 bridge
        }

        *(void**)((uintptr_t)*entSlot + 0x10) = elVoid;

        uintptr_t e = (uintptr_t)*entSlot;
        if (*(uintptr_t*)(e + 0x10) != 0) {
            uintptr_t owner = *(uintptr_t*)(e + 0x10);
            uint32_t flags = *(uint32_t*)(e + 0x40);
            if (flags & 4) {
                void* ibp = nullptr;
                new (&ibp) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>((void*)e));   // B.3.3 bridge
                s_fn_DynArr8_ResizeFill((void*)(owner + 0x88),
                                        *(int*)(owner + 0x90) + 1, &ibp);
            }
            if (flags & 0x10) {
                void* ibp = nullptr;
                new (&ibp) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>((void*)e));   // B.3.3 bridge
                s_fn_DynArr8_ResizeFill((void*)(owner + 0x98),
                                        *(int*)(owner + 0xa0) + 1, &ibp);
            }
        }
    }
}

// ============================================================
// 0x140021ba0 — EntityList bucket-tree serializer
// ============================================================
// Called by EntityList::Serialize (fn_BucketTreeSerialize, was unresolved —
// the null was the first save-run crash). Five bucket arrays (hdr stride 0x10:
// {IBP* data; int count/cap}): serialize count, resize on load
// (Collection::Array<Ptr8>::SetSize = 0x1400032e0), then per-element InternalBasePtr
// serialize with a ComponentEntity IsDerivedFrom filter.
extern "C" void BucketTree_Serialize_21ba0(void* serializer, void* /*unused*/,
                                           void* bucketsPtr) {
    const uint32_t s_tidCompEnt = ComponentEntity::TypeId();   // caller batch CB6: was a lazy by-name cache

    uintptr_t hdr = (uintptr_t)bucketsPtr;
    for (int b = 0; b < 5; b++, hdr += 0x10) {
        int count = *(int*)(hdr + 0x8);
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(count, nullptr);   // B.3.4 bridge
        if (*(int*)((uintptr_t)*(void**)serializer + 8) == 0) {
            // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8
            // align 8, new tail zeroed. Element is the 8-byte InternalBasePtr slot.
            GameColl::AsArray<GameColl::Ptr8>(hdr).SetSize(count);
        }
        uint8_t* slot = *(uint8_t**)hdr;
        for (int i = 0; i < count; i++, slot += 8) {
            reinterpret_cast<Object::InternalBasePtr*>(slot)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
            if (*(void**)slot != nullptr && s_tidCompEnt &&
                !fn_IsDerivedFrom(*(void**)slot, s_tidCompEnt)) {
                *(void**)slot = nullptr;
            }
        }
    }
}

// ============================================================
// 0x140095a90 — master GameUpdateLoop serializer
// ============================================================

// NOTE: a diagnostic mirror of Object::InternalBasePtr::Serialize used to live
// here. It established that the achievement IBP at self+0x150 is ABSENT in the
// tutorial stream (present=0), so retail returns early and that IBP is NOT the
// one that faulted -- the real desync was a skipped GoalInstance::Persist (see
// standalone/reg_goal.cpp). Removed once standalone/persistobject_probe.cpp
// could report the same thing by detouring PersistObject itself, which does not
// require standing in for any retail code.

// Caller batch CB6: `self` is the real class; its fields are read by name.
extern "C" void GameUpdateLoop_Serialize_95a90(GameUpdateLoop* self, void* stream,
                                               void* p3, int* outParam) {
    (void)p3;
    EnsureResolved();
    void* extFactory = ::Object::ExternalFactory::_singleton;
    void* intFactory = ::Object::InternalFactory::_singleton;

    fprintf(stderr, "[GUL::Serialize] %s ENTER self=%p stream=%p\n",
            IsSaving(stream) ? "SAVE" : "LOAD", (void*)self, stream);
    fflush(stderr);

    s_fn_EnableRefMode(extFactory, true);
    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 3);   // B.3.4 bridge
    s_fn_SerializeRefTable(extFactory, stream);
    // --- Preload every external the save references, with reference mode OFF ---
    //
    // Object::ExternalPtr's serializer (Runtime 0x180052a50) branches on
    // ExternalFactory+0x40: with reference mode ON it reads a 4-byte reference id,
    // with it OFF it reads a bool + a filename String. A compiled .rsc stores the
    // bool+string form, so a resource whose FIRST parse happens inside this window
    // is read with the wrong shape: its nested ExternalPtrs come out NULL and every
    // field after them is shifted. The tutorial save hit this on
    // Audio/Effects/RiverEffect.rsc and WindEffect.rsc, whose SoundEffect roots are
    // referenced only by the save; Audio/Effects/Environment.rsc was already loaded
    // by the ambient components and was therefore fine. The null SoundBuffer then
    // faulted inside XAudio2, because Audio::Device::PlayPlatform derefs it as a
    // WAVEFORMATEX with no null check.
    //
    // Retail never hits this because it preloads its resources up front; our
    // standalone loads them lazily (m_resourceList is empty). SerializeReferenceTable
    // has just filled the id -> path map without loading anything, so walking that
    // map here and forcing each resource to parse BEFORE any reference is resolved
    // restores the same invariant. It is not extra work: every one of these is about
    // to be loaded anyway as the stream resolves its references.
    //
    {
        if (!IsSaving(stream) && extFactory) {
            typedef void* (*FnPathForId_t)(void* self, void* outFilename, unsigned int id);
            typedef void* (*FnFindOrLoad_t)(void* self, void* filename);
            static FnPathForId_t s_pathForId = reinterpret_cast<FnPathForId_t>(
                ResolveExport("?GetPathForReferenceId@ExternalFactory@Object@@QEBA?AVFilename@System@@I@Z"));
            static FnFindOrLoad_t s_findOrLoad = reinterpret_cast<FnFindOrLoad_t>(
                ResolveExport("?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z"));
            // +0x28 is the reference map's entry count (SerializeReferenceTable zeroes
            // it at entry and the insert helper bumps it per row).
            const int refCount = *reinterpret_cast<int*>(
                reinterpret_cast<uint8_t*>(extFactory) + 0x28);
            if (!s_pathForId || !s_findOrLoad) {
                fprintf(stderr, "[PreloadRefs] SKIPPED — GetPathForReferenceId=%p "
                        "FindOrLoad=%p unresolved; resources first parsed inside the "
                        "reference-mode window will deserialize corrupt\n",
                        (void*)s_pathForId, (void*)s_findOrLoad);
                fflush(stderr);
            } else if (refCount > 0 && refCount < (1 << 20)) {
                s_fn_EnableRefMode(extFactory, false);
                int loaded = 0, missing = 0;
                for (int id = 0; id < refCount; ++id) {
                    void* fname[1] = { nullptr };
                    s_pathForId(extFactory, fname, (unsigned int)id);
                    if (!fname[0]) continue;
                    void* pl = s_findOrLoad(extFactory, fname);
                    if (pl) { ++loaded; }
                    else {
                        // Report them: these fail on the lazy path too, so the
                        // preload does not create the miss - but an unnamed miss
                        // is the kind of thing that turns into a null deref later.
                        if (++missing <= 20)
                            fprintf(stderr, "[PreloadRefs] MISS id=%d \"%s\"\n", id,
                                    reinterpret_cast<const char*>(fname[0]));
                    }
                    // System::Filename is one refcounted char* (count at str-4,
                    // block at str-8); drop our reference so this does not leak one
                    // string per entry on every load.
                    char* str = reinterpret_cast<char*>(fname[0]);
                    int* rc = reinterpret_cast<int*>(str - 4);
                    if (--*rc == 0 && fn_MemFreeStatic) fn_MemFreeStatic(str - 8);
                }
                s_fn_EnableRefMode(extFactory, true);
                fprintf(stderr, "[PreloadRefs] preloaded %d/%d referenced externals "
                        "(%d not found) with reference mode off\n",
                        loaded, refCount, missing);
                fflush(stderr);
            }
        }
    }
    s_fn_ResetPersist(intFactory, stream);

    if (IsSaving(stream)) {
        reinterpret_cast<UI::UserInterface*>(self->m_userInterface)->SaveAllSettings();  // B.3.6 bridge
    }

    s_fn_CfgSer_194a0(&self->m_climateConfig, ser);
    s_fn_CfgSer_19ec0(&self->m_mapConfig, ser);
    s_fn_CfgSer_1a0c0(&self->m_terrainConfig, ser);

    reinterpret_cast<IO::Serializer*>(ser)->Serialize(self->m_seed, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(self->m_mapSize, nullptr);   // B.3.4 bridge
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() > 0)   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(self->m_gameFlags, nullptr);   // B.3.4 bridge
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() > 2)   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(self->m_scenarioIndex, nullptr);   // B.3.4 bridge

    reinterpret_cast<System::Random*>(self->m_random)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    self->m_saveName.Serialize(ser);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(&self->m_scenarioCompleted), nullptr);   // B.3.4 bridge


    // NB: a diagnostic stand-in for this call (AchIbpPeek, below) established
    // that the achievement IBP is ABSENT in the tutorial stream (present=0), so
    // retail returns early here and this IBP is NOT the one that faults. Kept
    // out of the live path — the PersistObject probe reports the same thing
    // without replacing any retail code.
    reinterpret_cast<Object::InternalBasePtr*>(&self->m_scenarioInstance)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4 bridge
    if (self->m_scenarioInstance != nullptr) {
        uint32_t achTid = s_pAchInstTypeId ? *s_pAchInstTypeId : 0;
        if (!achTid || !fn_IsDerivedFrom(self->m_scenarioInstance, achTid))
            self->m_scenarioInstance = nullptr;
    }

    // Retail calls AchievementManager::Persist UNCONDITIONALLY (0x140095a90).
    // Our `if (AchievementManager::_singleton)` guard is a real deviation: if the
    // manager is null we skip a section retail always writes/reads, which would
    // desync every byte after it. Report when that happens instead of hiding it.
    // Do NOT try to peek the achievement count through `ser`'s encoding:
    // AchievementManager::Persist (Runtime 0x18009e910) builds its own encoding
    // from stream+0x10 via vtable +0x18, so a peek through the serializer reads
    // a different object and returns 0xAAAAAAAA even on a save that loads
    // perfectly. That false reading cost a full detour of this investigation;
    // the PersistObject probe measures the real arguments instead.
    if (::Achievement::AchievementManager::_singleton) {
        ::Achievement::AchievementManager::_singleton->Persist(
            *reinterpret_cast<IO::Stream*>(stream));
    }
    // B.3.5 bridge — ?Serialize@AnimationManager@Graphics@@QEAAXAEAVSerializer@IO@@@Z.
    // The `s_fn_AnimMgr_Serialize` cache, its typedef, its resolver and the
    // `anim=%p` it fed the [GUL::Serialize] fprintf all went in the closing pass:
    // a diagnostic that prints the address of a door nobody calls reports nothing.
    reinterpret_cast<Graphics::AnimationManager*>(self->m_animationManager)->Serialize(
        *reinterpret_cast<IO::Serializer*>(ser));   // B.3.5 bridge
    reinterpret_cast<Audio::Scene*>(self->m_audioScene)->Serialize(ser);   // B.3.7 bridge

    fprintf(stderr, "[GUL::Serialize] core done — subsystems...\n"); fflush(stderr);

    s_fn_Camera_988c0(self->m_playerController, stream);
    s_fn_EntList_c44a0(self->m_entityList, stream);
    s_fn_Geom_b04d0(self->m_worldGeometry, stream);
    s_fn_CitList_8f760(self->m_citizenList, stream, nullptr, outParam);
    s_fn_Grid_9f100(self->m_game, stream);
    s_fn_Goals_f9240(self->m_tutorial, stream, nullptr, outParam);
    EntityUI_Serialize_102920(self->m_entityUI, stream);

    // Day counter (+0x128): { int day; Time accumulated } — version-0 section.
    {
        uint8_t* dc = static_cast<uint8_t*>(self->m_dayCounter);
        alignas(8) uint8_t ser0[24];
        ::new (static_cast<void*>(ser0)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser0)->Serialize(*reinterpret_cast<int*>(dc), nullptr);   // B.3.4 bridge
        reinterpret_cast<System::Time*>(dc + 0x8)->Serialize(*reinterpret_cast<IO::Serializer*>(ser0));   // B.3.4b bridge
        reinterpret_cast<IO::Serializer*>(ser0)->~Serializer();   // B.3.4 bridge
    }

    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() > 1)   // B.3.4 bridge
        EventLog_Serialize_952c0(self->m_eventNotifier, stream);
    WeatherSim_Serialize_93c90(self->m_weather, stream);
    Minimap_Serialize_a57f0(self->m_imageCapture, stream);
    EntityList_PostLoad_c4730(self->m_entityList, stream);
    s_fn_CitPost_8f580(self->m_citizenList);

    if (intFactory) s_fn_ClearPersist(intFactory);
    s_fn_EnableRefMode(extFactory, false);
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge

    fprintf(stderr, "[GUL::Serialize] %s DONE\n", IsSaving(stream) ? "SAVE" : "LOAD");
    fflush(stderr);
}

// ============================================================
// 0x1400959c0 — SaveRestore (GUL vtable +0x38)
// ============================================================
// save != 0: serialize the world into a fresh CompressedMemoryStream, then
// Store the embedded MemoryStream into the SavedState. save == 0: Retrieve
// the SavedState into the stream, Open (decompress), then deserialize.
// Caller batch CB6: the body of the old `GameUpdateLoop_SaveRestore` door, moved
// into the member (the slot's override passes nullptr for retail's 4th argument;
// Load and the roundtrip check pass a real one). The door's reader-less
// RegisterExeAddr(0x1400959c0) row went with it.
void GameUpdateLoop::SaveRestoreOut(bool save, Save::SavedState* savedState, int* outParam) {
    EnsureResolved();

    // ENDGAME B.3.9 IO (W2, foreign site): the REAL IO::CompressedMemoryStream
    // (0x98 bytes: the 0x68 CompressedStream base + the MemoryStream member at
    // +0x68; runtime/types/io/Stream.h) built on stack storage where the door
    // built it, its members called where the doors were, and ended by a
    // non-virtual DeletingDtor(0) (the destructor chain, no free) where
    // ??1CompressedMemoryStream was.
    alignas(16) uint8_t cmsStorage[sizeof(IO::CompressedMemoryStream)];
    IO::CompressedMemoryStream* const cms =
        ::new (static_cast<void*>(cmsStorage)) IO::CompressedMemoryStream();
    if (!save) {
        savedState->Retrieve(cms->GetMemoryStream());
        cms->Open();
        GameUpdateLoop_Serialize_95a90(this, cms, savedState, outParam);
        cms->IO::CompressedMemoryStream::Close();
    } else {
        cms->Create();
        GameUpdateLoop_Serialize_95a90(this, cms, savedState, outParam);
        cms->IO::CompressedMemoryStream::Close();
        savedState->Store(cms->GetMemoryStream());
    }
    cms->IO::CompressedMemoryStream::DeletingDtor(0);
}

// ============================================================
// Load/resave roundtrip check — the serializer gate (2026-09-13)
// ============================================================
// BANISHED_SAVE_TEST=roundtrip. Called from the LOAD-FROM-SAVE branch of
// GameUpdateLoop::Load right after SaveRestore(load) and before the loaded
// SavedState is freed, i.e. before a single sim tick. It serializes the world
// again into a fresh SavedState, decompresses BOTH images through the same
// CompressedMemoryStream path the load uses, and compares the raw serialized
// bytes. Compression, the metadata block (thumbnail, timestamp) and sim drift
// are all out of the picture: a difference here is a Serialize body that does
// not round-trip its own state. Both raw images are written next to the logs
// so a difference can be diffed offline.
// ENDGAME B.3.9 IO (W2): the ?Read / ?GetSize@CompressedStream doors it
// resolved by name are the class's own members now, called non-virtually.
static bool RoundtripDecompress(void* savedState, uint8_t** outData, uint32_t* outSize) {
    alignas(16) uint8_t cmsStorage[sizeof(IO::CompressedMemoryStream)];
    IO::CompressedMemoryStream* const cms =
        ::new (static_cast<void*>(cmsStorage)) IO::CompressedMemoryStream();
    reinterpret_cast<Save::SavedState*>(savedState)->Retrieve(   // B.3.7 bridge
        cms->GetMemoryStream());
    if (!cms->Open()) { cms->IO::CompressedMemoryStream::DeletingDtor(0); return false; }
    const uint32_t size = cms->IO::CompressedStream::GetSize();
    uint8_t* data = static_cast<uint8_t*>(std::malloc(size ? size : 1));
    const bool ok = size == 0 || cms->IO::CompressedStream::Read(static_cast<int>(size), data);
    cms->IO::CompressedMemoryStream::Close();
    cms->IO::CompressedMemoryStream::DeletingDtor(0);
    *outData = data; *outSize = size;
    return ok;
}

static void RoundtripDump(const char* name, const uint8_t* data, uint32_t size) {
    FILE* f = std::fopen(name, "wb");
    if (!f) return;
    std::fwrite(data, 1, size, f);
    std::fclose(f);
}

extern "C" void GameUpdateLoop_SaveRoundtripCheck(GameUpdateLoop* self, void* loadedState) {
    const char* e = Opt().saveTest;
    if (!e || _stricmp(e, "roundtrip") != 0) return;
    EnsureResolved();

    struct SS { uint32_t id; uint32_t size; void* data; };
    const SS* loaded = static_cast<const SS*>(loadedState);
    SS fresh = { loaded->id, 0, nullptr };
    int out = 0;
    fprintf(stderr, "[Roundtrip] SaveRestore(save) into a fresh SavedState (loaded image %u bytes)\n",
            loaded->size);
    fflush(stderr);
    self->SaveRestoreOut(true, reinterpret_cast<Save::SavedState*>(&fresh), &out);   // B.3.8 bridge

    uint8_t* a = nullptr; uint32_t an = 0;
    uint8_t* b = nullptr; uint32_t bn = 0;
    const bool okA = RoundtripDecompress(loadedState, &a, &an);
    const bool okB = RoundtripDecompress(&fresh, &b, &bn);
    if (!okA || !okB) {
        fprintf(stderr, "[Roundtrip] RESULT=ERROR decompress loaded=%d resaved=%d\n", okA, okB);
        fflush(stderr);
        return;
    }
    RoundtripDump("standalone_roundtrip_loaded.bin",  a, an);
    RoundtripDump("standalone_roundtrip_resaved.bin", b, bn);

    const uint32_t n = an < bn ? an : bn;
    uint32_t first = n; uint32_t diffs = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] != b[i]) { if (first == n) first = i; diffs++; }
    }
    const bool equal = (an == bn) && diffs == 0;
    fprintf(stderr, "[Roundtrip] RESULT=%s raw loaded=%u resaved=%u compressed loaded=%u resaved=%u"
                    " firstDiff=%u diffBytes=%u tail=%u\n",
            equal ? "PASS" : "DIFF", an, bn, loaded->size, fresh.size,
            first, diffs, an > bn ? an - bn : bn - an);
    fflush(stderr);
    std::free(a); std::free(b);
    if (fresh.data && fn_MemFreeStatic) fn_MemFreeStatic(fresh.data);
}

// ============================================================
// Registration (called from standalone/address_registry.cpp)
// ============================================================
void RegisterGulSerialize() {
    RegisterExeAddr(0x1400952c0, reinterpret_cast<uintptr_t>(&EventLog_Serialize_952c0));
    RegisterExeAddr(0x1400199a0, reinterpret_cast<uintptr_t>(&EventLog_Resize_199a0));
    RegisterExeAddr(0x140093c90, reinterpret_cast<uintptr_t>(&WeatherSim_Serialize_93c90));
    RegisterExeAddr(0x14002e2e0, reinterpret_cast<uintptr_t>(&GoalRefTree_Serialize_e2e0));
    RegisterExeAddr(0x14002e470, reinterpret_cast<uintptr_t>(&IbpTree_Serialize_e470));
    RegisterExeAddr(0x14002e000, reinterpret_cast<uintptr_t>(&CamViewArray_Resize_e000));
    RegisterExeAddr(0x1400a57f0, reinterpret_cast<uintptr_t>(&Minimap_Serialize_a57f0));
    RegisterExeAddr(0x1400c4730, reinterpret_cast<uintptr_t>(&EntityList_PostLoad_c4730));
    RegisterExeAddr(0x140021ba0, reinterpret_cast<uintptr_t>(&BucketTree_Serialize_21ba0));
}
