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
//   0x140102920  EntityUI serialize (m_toolManager +0x120: paused/speed/times,
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
// Ported elsewhere (parallel wave lanes, called by name since B3R):
//   0x14008f760/0x14008f580  CitizenBehavior serialize + post-pass (+0xF0)
//   0x14009f100              game map-grid serialize (+0xF8)
//   0x1400f9240              goal/tutorial-mgr serialize (+0x118)
// Already ported before this wave:
//   0x1400194a0/0x140019ec0/0x14001a0c0 (config ExternalPtrs), 0x1400988c0
//   (camera/PlayerController), 0x1400c44a0 (EntityList::Serialize),
//   0x1400b04d0 (WorldGeometry).

#include "Game/GameUpdateLoop.h"   // caller batch CB6: SaveRestoreOut, the named fields
#include "Game/Map.h"
#include "Entity/EntityList.h"
#include "Game/PlayerToolManager.h"
#include "Game/Minimap.h"
#include "Game/WeatherSimulation.h"
#include "Game/EventLog.h"            // B4 GAME-a: the pointees the serializer reads by name
#include "Game/Calendar.h"   // m_dayCounter's class (B.4 GAME-a)
#include "Entity/ComponentEntity.h"   // caller batch CB6: TypeId()
#include "Component/Component.h"      // caller batch CB6: slot 11
#include "math/Math.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "options.h"
#include "system/String.h"
#include "collection/Collection.h"
#include "Component/coll_bridge.h"
#include <new>
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
#include "Game/exe_bodies.h"
#include "Action/exe_bodies.h"         // ActHunt_DynArraySetCount_3110
#include "Dialog/exe_bodies.h"         // ToolbarUtility_BstInsert_4890
#include "Player/exe_bodies.h"         // PlayerTrade_Retrace_25920
#include "Entity/entitylist_funcs.h"   // EntityList_Serialize
#include "system/Filename.h"

// code/Game/Game/minimap_upload.cpp — ports FUN_1400a4e80 + the CopyToTexture tail.
extern "C" void Minimap_CreateAndUpload(void* mapObj);
// code/Game/Game/weather_particles.cpp — ports FUN_1400942c0 (enable half; the
// ambient-audio entity spawn is not ported there yet — see WeatherSim_Serialize).
extern "C" void WeatherEffect_SetEnabled(void* sim, int slot, int enable);

namespace {

// B3R: every callee is called by name. The Runtime exports are members of
// Object::ExternalFactory (EnableReferenceMode, SerializeReferenceTable,
// GetPathForReferenceId, FindOrLoad) and Object::InternalFactory
// (ResetPersistState, ClearPersistState). The exe helpers:
//   0x1400194a0 / 0x140019ec0 / 0x14001a0c0 / 0x1400192a0
//       Reimpl_ExternalPtr_Serialize_{ClimateGenerator,MapGenerator,TerrainGenerator,
//       EnvironmentDesc}                                          (Game/exe_bodies.h)
//   0x1400988c0 Reimpl_Transform_SerializeWrapper (PlayerController, +0xE0)
//   0x1400c44a0 EntityList_Serialize (+0xE8)                   (Entity/entitylist_funcs.h)
//   0x1400b04d0 Reimpl_TradeWork_impl_04d0 (WorldGeometry, +0x110)
//   0x14008f760 / 0x14008f580 CitizenList_Serialize_8f760 / _PostLoad_8f580 (+0xF0)
//   0x14009f100 GameGrid_Serialize_9f100 (+0xF8), 0x1400f9240 Goals_Serialize_f9240 (+0x118)
//   0x140003110 ActHunt_DynArraySetCount_3110                  (Action/exe_bodies.h)
//   0x1400021d0 Reimpl_Array_Resize_Stride04, 0x140002340 Reimpl_ExternalPtr_Serialize_StringTable
//   0x140014890 ToolbarUtility_BstInsert_4890                  (Dialog/exe_bodies.h)
//   0x140025920 PlayerTrade_Retrace_25920                      (Player/exe_bodies.h)
// The "[GUL::Serialize] resolved:" stderr line went with the caches.
//
// ENDGAME B.3.9 IO (W2): the six IO::CompressedMemoryStream door typedefs and
// their resolved statics are gone -- SaveRestoreOut and the roundtrip check
// build the real class (runtime/types/io/Stream.h) and call its members.

inline bool IsSaving(const IO::Stream* stream) {
    return stream->GetDirection() == IO::kWriting;   // Stream +0x08 == 1
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
void EventLog::Serialize(IO::Stream& stream) {
    IO::Serializer ser(stream, 1);
    ser.Serialize(6, field_0x10, nullptr);
    if (ser.GetVersion() > 0) {
        int total = entryCount;
        int capped = total > 200 ? 200 : total;
        int skip = total - capped;
        if (skip < 0) skip = 0;
        ser.Serialize(capped, nullptr);
        if (!IsSaving(&stream)) {
            EventLog_Resize_199a0(&entries, capped);
        }
        for (int i = skip; i < entryCount; i++) {
            EventLogEntry& e = entries[i];
            // LEFT: retail serializes the colour through the uint overload
            // (0x1400952c0); the tree's int overload is kept (an H8 question).
            ser.Serialize(reinterpret_cast<int&>(e.color), nullptr);
            e.text.Serialize(&ser);
            ser.Serialize(e.eventType, nullptr);
            ser.Serialize(e.severity, nullptr);
            ser.Serialize(e.tileIndex, nullptr);
            ser.Serialize(e.iconIndex, nullptr);
            ser.Serialize(e.popup, nullptr);
        }
    }
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
void WeatherSimulation::Serialize(IO::Stream& stream) {
    IO::Serializer ser(stream, 0);

    Reimpl_ExternalPtr_Serialize_EnvironmentDesc(&environment, &ser);   // 0x1400192a0
    Reimpl_ExternalPtr_Serialize_ClimateGenerator(&climate, &ser);      // 0x1400194a0

    ser.Serialize(field_0x10, nullptr);
    ser.Serialize(field_0x14, nullptr);
    ser.Serialize(temperature, nullptr);
    // ?Serialize@Vector4@Math@@QEAAXAEAVSerializer@IO@@@Z  retail 0x18001d360
    // LEFT (H5): +0x1C is a Math::Vector4, declared as float[4]: a Vector4
    // member would give the class a constructor that runs in Load.
    reinterpret_cast<Math::Vector4*>(field_0x1C)->Serialize(&ser);
    ser.Serialize(field_0x2C, nullptr);
    ser.Serialize(field_0x30, nullptr);
    ser.Serialize(field_0x44, nullptr);
    ser.Serialize(field_0x48, nullptr);
    ser.Serialize(seasonStep, nullptr);

    int slotCount = effectCount;
    for (int i = 0; i < slotCount; i++) {
        Graphics::Geometry** slot = &effects[i];
        bool active = (*slot != nullptr);
        ser.Serialize(active, nullptr);
        if (active) {
            if (!IsSaving(&stream)) {
                WeatherEffect_SetEnabled(this, i, 1);
            }
            // LEFT: the effect's two floats (+0xC4 / +0xC0) are the weather
            // particle geometry's (weather_particles.cpp, GAME-c): read raw.
            uint8_t* geo = reinterpret_cast<uint8_t*>(*slot);
            ser.Serialize(*(float*)(geo + 0xc4), nullptr);
            ser.Serialize(*(float*)(geo + 0xc0), nullptr);
        }
    }
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
// The set is PlayerToolManager's {root +0x50, count +0x58}: `root` is its
// header (the count is the int at root + 8, read as the header's).
static void GoalRefTree_Serialize_e2e0(TrackedEntityNode** root, int32_t* countField,
                                       IO::Serializer* serializer) {
    int count = *countField;
    serializer->Serialize(count, nullptr);

    if (static_cast<int>(serializer->GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
        // LOAD: read `count` records, insert each
        for (int i = 0; i < count; i++) {
            struct { void* ref; int a; int b; bool used; } rec;
            rec.ref = nullptr; rec.a = 0; rec.b = 0; rec.used = false;
            Reimpl_ExternalPtr_Serialize_StringTable(&rec.ref, serializer);   // 0x140002340
            serializer->Serialize(rec.a, nullptr);
            serializer->Serialize(rec.b, nullptr);
            serializer->Serialize(rec.used, nullptr);
            void* outNode = nullptr;
            // LEFT: the shared set insert takes the header as int64_t* and the
            // record as uint64_t* (Dialog/exe_bodies.h, not GAME-a's).
            ToolbarUtility_BstInsert_4890(reinterpret_cast<int64_t*>(root), &outNode,
                                          reinterpret_cast<uint64_t*>(&rec.ref));   // 0x140014890
        }
    } else {
        // SAVE: leftmost-first in-order walk
        TrackedEntityNode* node = *root;
        if (!node) { return; }
        TrackedEntityNode* cur = node;
        while (cur->left) cur = cur->left;
        while (cur) {
            Reimpl_ExternalPtr_Serialize_StringTable(&cur->key, serializer);   // 0x140002340
            serializer->Serialize(cur->x, nullptr);
            serializer->Serialize(cur->y, nullptr);
            serializer->Serialize(cur->active, nullptr);
            // in-order successor
            TrackedEntityNode* next = cur->right;
            if (next) {
                while (next->left) next = next->left;
                cur = next;
            } else {
                TrackedEntityNode* parent = cur->parent;
                while (parent && parent->right == cur) {
                    cur = parent;
                    parent = parent->parent;
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
// The set is PlayerToolManager's {root +0x60, count +0x68} (see above).
static void IbpTree_Serialize_e470(TrackedEntityNode** root, int32_t* countField,
                                   IO::Serializer* serializer) {
    const uint32_t s_tidCompEnt = ComponentEntity::TypeId();   // caller batch CB6: was a lazy by-name cache

    int count = *countField;
    serializer->Serialize(count, nullptr);

    if (static_cast<int>(serializer->GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
        // LOAD
        for (int i = 0; i < count; i++) {
            ComponentEntity* key = nullptr;
            int a = 0, b = 0;
            bool used = false;
            // LEFT: the key is an InternalPtr's raw slot (Object::InternalBasePtr has
            // no typed slot accessor here).
            reinterpret_cast<Object::InternalBasePtr*>(&key)->Serialize(*serializer);   // B.3.4 bridge
            if (key && s_tidCompEnt && !::Object::IsDerivedFromNullSafe(key, s_tidCompEnt)) key = nullptr;
            serializer->Serialize(a, nullptr);
            serializer->Serialize(b, nullptr);
            serializer->Serialize(used, nullptr);

            auto makeNode = [&](TrackedEntityNode* parent) -> TrackedEntityNode* {
                TrackedEntityNode* n = static_cast<TrackedEntityNode*>(System::MemoryAllocator::MemAlloc(0x38, 8));
                if (!n) return nullptr;
                n->key = key;
                n->x = a;
                n->y = b;
                n->active = used ? 1 : 0;
                n->parent = parent;
                n->left = nullptr;
                n->right = nullptr;
                n->balance = 0;               // +0x30 / +0x32: one dword store of 0
                n->height = 0;
                return n;
            };

            TrackedEntityNode* rootNode = *root;
            if (!rootNode) {
                *root = makeNode(nullptr);
                *countField += 1;
                continue;
            }
            TrackedEntityNode* cur = rootNode;
            bool inserted = false;
            bool duplicate = false;
            TrackedEntityNode* newNode = nullptr;
            while (!inserted) {
                ComponentEntity* curKey = cur->key;
                if (key < curKey) {
                    TrackedEntityNode* l = cur->left;
                    if (!l) {
                        newNode = makeNode(cur);
                        cur->left = newNode;
                        inserted = true;
                    } else cur = l;
                } else if (key > curKey) {
                    TrackedEntityNode* r = cur->right;
                    if (!r) {
                        newNode = makeNode(cur);
                        cur->right = newNode;
                        inserted = true;
                    } else cur = r;
                } else { duplicate = true; break; }
            }
            if (duplicate) continue;
            TrackedEntityNode* fixupNode = newNode;
            // LEFT: the shared AVL retrace takes the header and the node slot as
            // int64_t* (Player/exe_bodies.h, not GAME-a's).
            PlayerTrade_Retrace_25920(reinterpret_cast<int64_t*>(root),
                                      reinterpret_cast<int64_t*>(&fixupNode));   // 0x140025920
            *countField += 1;
        }
    } else {
        // SAVE: in-order walk
        TrackedEntityNode* cur = *root;
        if (!cur) return;
        while (cur->left) cur = cur->left;
        while (cur) {
            reinterpret_cast<Object::InternalBasePtr*>(&cur->key)->Serialize(*serializer);   // B.3.4 bridge
            if (cur->key && s_tidCompEnt && !::Object::IsDerivedFromNullSafe(cur->key, s_tidCompEnt))
                cur->key = nullptr;
            serializer->Serialize(cur->x, nullptr);
            serializer->Serialize(cur->y, nullptr);
            serializer->Serialize(cur->active, nullptr);
            TrackedEntityNode* next = cur->right;
            if (next) {
                while (next->left) next = next->left;
                cur = next;
            } else {
                TrackedEntityNode* parent = cur->parent;
                while (parent && parent->right == cur) {
                    cur = parent;
                    parent = parent->parent;
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
void PlayerToolManager::Serialize(IO::Stream& stream) {
    IO::Serializer ser(stream, 2);
    ser.Serialize(field_0x30, nullptr);
    ser.Serialize(field_0x34, nullptr);
    ser.Serialize(field_0x3C, nullptr);
    ser.Serialize(field_0x40, nullptr);
    ser.Serialize(field_0x44, nullptr);
    GoalRefTree_Serialize_e2e0(&field_0x50, &field_0x58, &ser);
    if (ser.GetVersion() > 0) {
        IbpTree_Serialize_e470(&field_0x60, &field_0x68, &ser);
    }
    if (ser.GetVersion() > 1) {
        int count = cameraViewCount;
        ser.Serialize(count, nullptr);
        if (!IsSaving(&stream)) {
            CamViewArray_Resize_e000(&cameraViews, count);
        }
        for (uint32_t i = 0; i < (uint32_t)count; i++) {
            PlayerCameraView& e = cameraViews[i];
            // ?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z  retail 0x1800150e0
            e.position.Serialize(&ser);
            ser.Serialize(e.field_0x0C, nullptr);
            ser.Serialize(e.field_0x10, nullptr);
            ser.Serialize(e.field_0x14, nullptr);
            ser.Serialize(e.used, nullptr);
        }
    }
}

// ============================================================
// 0x1400a57f0 — minimap serialize (GUL+0x148 object, 0x38 bytes)
// ============================================================
// Layout: +0x04 int width; +0x08/+0x10 ImageBuffer*; +0x18 {data,count,cap}
// terrain pixels; +0x28 {data,count,cap} overlay pixels. Version 0. On load,
// the GPU buffers are (re)created and both arrays uploaded — that tail is the
// already-ported Minimap_CreateAndUpload (FUN_1400a4e80 + Lock/Copy/Unlock).
void Minimap::Serialize(IO::Stream& stream) {
    IO::Serializer ser(stream, 0);

    // overlay array (+0x28) first — matches the retail order
    int n = overlayPixelCount;
    ser.Serialize(n, nullptr);
    if (!IsSaving(&stream)) Reimpl_Array_Resize_Stride04(&overlayPixels, n);   // 0x1400021d0
    ser.Serialize(n, overlayPixels, nullptr);

    // terrain array (+0x18)
    n = terrainPixelCount;
    ser.Serialize(n, nullptr);
    if (!IsSaving(&stream)) Reimpl_Array_Resize_Stride04(&terrainPixels, n);   // 0x1400021d0
    ser.Serialize(n, terrainPixels, nullptr);

    ser.Serialize(size, nullptr);

    if (!IsSaving(&stream)) {
        Minimap_CreateAndUpload(this);
    }
}

// ============================================================
// 0x1400c4730 — EntityList post-load pass
// ============================================================
// LOAD ONLY. For each entity: vcall +0x58 (post-load fixup) on every
// component, write the EntityList backptr at entity+0x10, then enrol the
// entity in the two update lists (+0x88 per-frame, +0x98 per-tick) when its
// flag word +0x40 has bit 2 / bit 4 set (DynArray8_ResizeFill append).
static void EntityList_PostLoad_c4730(EntityList* el, IO::Stream* stream) {
    if (stream->GetDirection() != IO::kReading) return;   // save direction: no-op

    int count = el->masterArray.count;
    for (int i = 0; i < count; i++) {
        ComponentEntity** entSlot = &el->masterArray.data[i];
        ComponentEntity* ent = *entSlot;

        int compCount = ent->_componentCount;
        for (int j = 0; j < compCount; j++) {
            Component* comp = ent->_components[j].Get();
            // Caller batch CB6: slot 11 (vtable +0x58) of an entity component-array
            // element (a proved Component).
            comp->Slot11();
        }

        (*entSlot)->_entityList = el;

        ComponentEntity* e = *entSlot;
        if (e->_entityList != nullptr) {
            // LEFT: ComponentEntity::_entityList is void* (Entity/ComponentEntity.h):
            // retail re-reads the owner it has just stored.
            EntityList* owner = static_cast<EntityList*>(e->_entityList);
            uint32_t flags = e->_entityFlags;
            if (flags & 4) {
                void* ibp = nullptr;
                new (&ibp) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(e));   // B.3.3 bridge
                ActHunt_DynArraySetCount_3110(&owner->stagingUpdate,
                                              owner->stagingUpdate.count + 1, &ibp);   // 0x140003110
            }
            if (flags & 0x10) {
                void* ibp = nullptr;
                new (&ibp) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(e));   // B.3.3 bridge
                ActHunt_DynArraySetCount_3110(&owner->stagingPostUpdate,
                                              owner->stagingPostUpdate.count + 1, &ibp);   // 0x140003110
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
extern "C" void BucketTree_Serialize_21ba0(IO::Serializer* serializer, void* /*unused*/,
                                           UpdateBucket* buckets) {
    const uint32_t s_tidCompEnt = ComponentEntity::TypeId();   // caller batch CB6: was a lazy by-name cache

    UpdateBucket* hdr = buckets;
    for (int b = 0; b < 5; b++, hdr++) {
        int count = hdr->count;
        serializer->Serialize(count, nullptr);
        if (serializer->_stream->GetDirection() == IO::kReading) {   // the stream's +0x08, read inline
            // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8
            // align 8, new tail zeroed. Element is the 8-byte InternalBasePtr slot.
            GameColl::AsArray<GameColl::Ptr8>(hdr).SetSize(count);
        }
        ComponentEntity** slot = hdr->data;
        for (int i = 0; i < count; i++, slot++) {
            // LEFT: an InternalBasePtr slot (the IBP's raw pointer).
            reinterpret_cast<Object::InternalBasePtr*>(slot)->Serialize(*serializer);   // B.3.4 bridge
            if (*slot != nullptr && s_tidCompEnt &&
                !::Object::IsDerivedFromNullSafe(*slot, s_tidCompEnt)) {
                *slot = nullptr;
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

// B4 GAME-a: a private member (retail's non-virtual 0x140095a90, called by
// SaveRestore 0x1400959c0 with this, the stream, the SavedState and the out int).
void GameUpdateLoop::SerializeAll(IO::Stream& stream, Save::SavedState* p3, int* outParam) {
    (void)p3;
    ::Object::ExternalFactory* extFactory = ::Object::ExternalFactory::_singleton;
    ::Object::InternalFactory* intFactory = ::Object::InternalFactory::_singleton;

    fprintf(stderr, "[GUL::Serialize] %s ENTER self=%p stream=%p\n",
            IsSaving(&stream) ? "SAVE" : "LOAD", static_cast<void*>(this), static_cast<void*>(&stream));
    fflush(stderr);

    extFactory->EnableReferenceMode(true);
    {   // the Serializer's scope ends right after reference mode is switched off, where
        // the tree called ~Serializer by hand
    IO::Serializer ser(stream, 3);
    extFactory->SerializeReferenceTable(stream);
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
        if (!IsSaving(&stream) && extFactory) {
            // B3R: GetPathForReferenceId / FindOrLoad are members (their door
            // caches and the "[PreloadRefs] SKIPPED ... unresolved" arm are gone).
            // +0x28 is the reference map's entry count (SerializeReferenceTable zeroes
            // it at entry and the insert helper bumps it per row).
            const int refCount = extFactory->_references.GetCount();
            if (refCount > 0 && refCount < (1 << 20)) {
                extFactory->EnableReferenceMode(false);
                int loaded = 0, missing = 0;
                for (int id = 0; id < refCount; ++id) {
                    void* fname[1] = { nullptr };
                    ::new (static_cast<void*>(fname)) System::Filename(
                        extFactory->GetPathForReferenceId((unsigned int)id));
                    if (!fname[0]) continue;
                    const void* pl = extFactory->FindOrLoad(
                        *reinterpret_cast<const System::Filename*>(fname));
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
                    if (--*rc == 0) System::MemoryAllocator::MemFree(str - 8);
                }
                extFactory->EnableReferenceMode(true);
                fprintf(stderr, "[PreloadRefs] preloaded %d/%d referenced externals "
                        "(%d not found) with reference mode off\n",
                        loaded, refCount, missing);
                fflush(stderr);
            }
        }
    }
    intFactory->ResetPersistState(&stream);

    if (IsSaving(&stream)) {
        m_userInterface->SaveAllSettings();
    }

    Reimpl_ExternalPtr_Serialize_ClimateGenerator(&m_climateConfig, &ser);   // 0x1400194a0
    Reimpl_ExternalPtr_Serialize_MapGenerator(&m_mapConfig, &ser);           // 0x140019ec0
    Reimpl_ExternalPtr_Serialize_TerrainGenerator(&m_terrainConfig, &ser);   // 0x14001a0c0

    ser.Serialize(m_seed, nullptr);
    ser.Serialize(m_mapSize, nullptr);
    if (ser.GetVersion() > 0)
        ser.Serialize(m_gameFlags, nullptr);
    if (ser.GetVersion() > 2)
        ser.Serialize(m_scenarioIndex, nullptr);

    m_random->Serialize(ser);
    m_saveName.Serialize(&ser);
    ser.Serialize(m_scenarioCompleted, nullptr);


    // NB: a diagnostic stand-in for this call (AchIbpPeek, below) established
    // that the achievement IBP is ABSENT in the tutorial stream (present=0), so
    // retail returns early here and this IBP is NOT the one that faults. Kept
    // out of the live path — the PersistObject probe reports the same thing
    // without replacing any retail code.
    reinterpret_cast<Object::InternalBasePtr*>(&m_scenarioInstance)->Serialize(ser);   // B.3.4 bridge
    if (m_scenarioInstance != nullptr) {
        uint32_t achTid = Achievement::AchievementInstance::TypeId();   // ENDGAME B.3.9 TR (was ResolveExport of its ?_typeInfo@)
        if (!achTid || !::Object::IsDerivedFromNullSafe(m_scenarioInstance, achTid))
            m_scenarioInstance = nullptr;
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
        ::Achievement::AchievementManager::_singleton->Persist(stream);
    }
    // B.3.5 bridge — ?Serialize@AnimationManager@Graphics@@QEAAXAEAVSerializer@IO@@@Z.
    // The `s_fn_AnimMgr_Serialize` cache, its typedef, its resolver and the
    // `anim=%p` it fed the [GUL::Serialize] fprintf all went in the closing pass:
    // a diagnostic that prints the address of a door nobody calls reports nothing.
    m_animationManager->Serialize(ser);
    m_audioScene->Serialize(&ser);

    fprintf(stderr, "[GUL::Serialize] core done — subsystems...\n"); fflush(stderr);

    Reimpl_Transform_SerializeWrapper(m_playerController, &stream);                  // 0x1400988c0
    m_entityList->Serialize(stream);                                                 // 0x1400c44a0
    Reimpl_TradeWork_impl_04d0(m_worldGeometry, &stream);                            // 0x1400b04d0
    CitizenList_Serialize_8f760(m_citizenList, &stream, nullptr, outParam);          // 0x14008f760
    m_game->Serialize(stream);                                                       // 0x14009f100
    Goals_Serialize_f9240(m_tutorial, &stream, nullptr, outParam);                   // 0x1400f9240
    m_toolManager->Serialize(stream);                                                // 0x140102920

    // Day counter (+0x128): { int day; Time accumulated } — version-0 section.
    {
        Calendar* dc = m_dayCounter;
        IO::Serializer ser0(stream, 0);
        ser0.Serialize(dc->year, nullptr);
        dc->yearTime.Serialize(ser0);
    }

    if (ser.GetVersion() > 1)
        m_eventNotifier->Serialize(stream);          // 0x1400952c0
    m_weather->Serialize(stream);                    // 0x140093c90
    m_imageCapture->Serialize(stream);               // 0x1400a57f0
    EntityList_PostLoad_c4730(m_entityList, &stream);
    CitizenList_PostLoad_8f580(m_citizenList);   // 0x14008f580

    if (intFactory) intFactory->ClearPersistState();
    extFactory->EnableReferenceMode(false);
    }   // ~Serializer

    fprintf(stderr, "[GUL::Serialize] %s DONE\n", IsSaving(&stream) ? "SAVE" : "LOAD");
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

    // ENDGAME B.3.9 IO (W2, foreign site): the REAL IO::CompressedMemoryStream
    // (0x98 bytes: the 0x68 CompressedStream base + the MemoryStream member at
    // +0x68; runtime/types/io/Stream.h) built on stack storage where the door
    // built it, its members called where the doors were, and ended by a
    // qualified `T::~T()` (the destructor chain, no free) where
    // ??1CompressedMemoryStream was.
    alignas(16) uint8_t cmsStorage[sizeof(IO::CompressedMemoryStream)];
    IO::CompressedMemoryStream* const cms =
        ::new (static_cast<void*>(cmsStorage)) IO::CompressedMemoryStream();
    if (!save) {
        savedState->Retrieve(cms->GetMemoryStream());
        cms->Open();
        SerializeAll(*cms, savedState, outParam);
        cms->IO::CompressedMemoryStream::Close();
    } else {
        cms->Create();
        SerializeAll(*cms, savedState, outParam);
        cms->IO::CompressedMemoryStream::Close();
        savedState->Store(cms->GetMemoryStream());
    }
    cms->IO::CompressedMemoryStream::~CompressedMemoryStream();
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
static bool RoundtripDecompress(Save::SavedState* savedState, uint8_t** outData, uint32_t* outSize) {
    alignas(16) uint8_t cmsStorage[sizeof(IO::CompressedMemoryStream)];
    IO::CompressedMemoryStream* const cms =
        ::new (static_cast<void*>(cmsStorage)) IO::CompressedMemoryStream();
    savedState->Retrieve(cms->GetMemoryStream());
    if (!cms->Open()) { cms->IO::CompressedMemoryStream::~CompressedMemoryStream(); return false; }
    const uint32_t size = cms->IO::CompressedStream::GetSize();
    uint8_t* data = static_cast<uint8_t*>(std::malloc(size ? size : 1));
    const bool ok = size == 0 || cms->IO::CompressedStream::Read(static_cast<int>(size), data);
    cms->IO::CompressedMemoryStream::Close();
    cms->IO::CompressedMemoryStream::~CompressedMemoryStream();
    *outData = data; *outSize = size;
    return ok;
}

static void RoundtripDump(const char* name, const uint8_t* data, uint32_t size) {
    FILE* f = std::fopen(name, "wb");
    if (!f) return;
    std::fwrite(data, 1, size, f);
    std::fclose(f);
}

extern "C" void GameUpdateLoop_SaveRoundtripCheck(GameUpdateLoop* self, Save::SavedState* loadedState) {
    const char* e = Opt().saveTest;
    if (!e || _stricmp(e, "roundtrip") != 0) return;

    // The fresh image is a SavedState's three fields on the stack, WITHOUT its
    // destructor (freed by hand below, on the success path only, as before).
    struct SS { uint32_t id; uint32_t size; void* data; };
    const Save::SavedState* loaded = loadedState;
    SS fresh = { loaded->_id, 0, nullptr };
    int out = 0;
    fprintf(stderr, "[Roundtrip] SaveRestore(save) into a fresh SavedState (loaded image %u bytes)\n",
            loaded->_size);
    fflush(stderr);
    self->SaveRestoreOut(true, reinterpret_cast<Save::SavedState*>(&fresh), &out);   // B.3.8 bridge

    uint8_t* a = nullptr; uint32_t an = 0;
    uint8_t* b = nullptr; uint32_t bn = 0;
    const bool okA = RoundtripDecompress(loadedState, &a, &an);
    const bool okB = RoundtripDecompress(reinterpret_cast<Save::SavedState*>(&fresh), &b, &bn);   // LEFT: the destructor-less stack twin
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
            equal ? "PASS" : "DIFF", an, bn, loaded->_size, fresh.size,
            first, diffs, an > bn ? an - bn : bn - an);
    fflush(stderr);
    std::free(a); std::free(b);
    if (fresh.data) System::MemoryAllocator::MemFree(fresh.data);
}
