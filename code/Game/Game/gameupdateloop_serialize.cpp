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

#include "engine_funcs.h"
#include "resolve.h"
#include "address_registry.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>   // QueryPerformanceCounter for the BANISHED_SAVE_TIMING probe

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
typedef void  (*fn_SaveAllSettings_t)(void* ui);
typedef void  (*fn_Random_Serialize_t)(void* random, void* serializer);
typedef void  (*fn_AchMgr_Persist_t)(void* mgr, void* stream);
typedef void  (*fn_AnimMgr_Serialize_t)(void* animMgr, void* serializer);
typedef void  (*fn_AudioScene_Serialize_t)(void* scene, void* serializer);
typedef void* (*fn_CMS_Ctor_t)(void* self);
typedef void  (*fn_CMS_Dtor_t)(void* self);
typedef bool  (*fn_CMS_Open_t)(void* self);
typedef bool  (*fn_CMS_Create_t)(void* self);
typedef void  (*fn_CMS_Close_t)(void* self);
typedef void* (*fn_CMS_GetMemStream_t)(void* self);
typedef void  (*fn_SavedState_Xfer_t)(void* state, void* memStream);
typedef void  (*fn_Ser_BoolArr_t)(void* ser, int count, void* data, const char* name);
typedef void  (*fn_Ser_UintArr_t)(void* ser, int count, void* data, const char* name);

static fn_EnableRefMode_t       s_fn_EnableRefMode = nullptr;
static fn_SerializeRefTable_t   s_fn_SerializeRefTable = nullptr;
static fn_ResetPersist_t        s_fn_ResetPersist = nullptr;
static fn_ClearPersist_t        s_fn_ClearPersist = nullptr;
static fn_SaveAllSettings_t     s_fn_SaveAllSettings = nullptr;
static fn_Random_Serialize_t    s_fn_Random_Serialize = nullptr;
static fn_AchMgr_Persist_t      s_fn_AchMgr_Persist = nullptr;
static fn_AnimMgr_Serialize_t   s_fn_AnimMgr_Serialize = nullptr;
static fn_AudioScene_Serialize_t s_fn_AudioScene_Serialize = nullptr;
static fn_CMS_Ctor_t            s_fn_CMS_Ctor = nullptr;
static fn_CMS_Dtor_t            s_fn_CMS_Dtor = nullptr;
static fn_CMS_Open_t            s_fn_CMS_Open = nullptr;
static fn_CMS_Create_t          s_fn_CMS_Create = nullptr;
static fn_CMS_Close_t           s_fn_CMS_Close = nullptr;
static fn_CMS_GetMemStream_t    s_fn_CMS_GetMemStream = nullptr;
static fn_SavedState_Xfer_t     s_fn_SavedState_Store = nullptr;
static fn_SavedState_Xfer_t     s_fn_SavedState_Retrieve = nullptr;
static fn_Ser_BoolArr_t         s_fn_Ser_BoolArr = nullptr;
static fn_Ser_UintArr_t         s_fn_Ser_UintArr = nullptr;
static void**                   s_ppIntFactory = nullptr;   // InternalFactory::_singleton cell
static void**                   s_ppAchMgr = nullptr;       // AchievementManager::_singleton cell
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
    s_fn_SaveAllSettings = reinterpret_cast<fn_SaveAllSettings_t>(
        ResolveExport("?SaveAllSettings@UserInterface@UI@@QEAAXXZ"));
    s_fn_Random_Serialize = reinterpret_cast<fn_Random_Serialize_t>(
        ResolveExport("?Serialize@Random@System@@QEAAXAEAVSerializer@IO@@@Z"));
    s_fn_AchMgr_Persist = reinterpret_cast<fn_AchMgr_Persist_t>(
        ResolveExport("?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z"));
    s_fn_AnimMgr_Serialize = reinterpret_cast<fn_AnimMgr_Serialize_t>(
        ResolveExport("?Serialize@AnimationManager@Graphics@@QEAAXAEAVSerializer@IO@@@Z"));
    s_fn_AudioScene_Serialize = reinterpret_cast<fn_AudioScene_Serialize_t>(
        ResolveExport("?Serialize@Scene@Audio@@QEAAXAEAVSerializer@IO@@@Z"));
    s_fn_CMS_Ctor = reinterpret_cast<fn_CMS_Ctor_t>(
        ResolveExport("??0CompressedMemoryStream@IO@@QEAA@XZ"));
    s_fn_CMS_Dtor = reinterpret_cast<fn_CMS_Dtor_t>(
        ResolveExport("??1CompressedMemoryStream@IO@@UEAA@XZ"));
    s_fn_CMS_Open = reinterpret_cast<fn_CMS_Open_t>(
        ResolveExport("?Open@CompressedMemoryStream@IO@@QEAA_NXZ"));
    s_fn_CMS_Create = reinterpret_cast<fn_CMS_Create_t>(
        ResolveExport("?Create@CompressedMemoryStream@IO@@QEAA_NXZ"));
    s_fn_CMS_Close = reinterpret_cast<fn_CMS_Close_t>(
        ResolveExport("?Close@CompressedMemoryStream@IO@@UEAAXXZ"));
    s_fn_CMS_GetMemStream = reinterpret_cast<fn_CMS_GetMemStream_t>(
        ResolveExport("?GetMemoryStream@CompressedMemoryStream@IO@@QEAAAEAVMemoryStream@2@XZ"));
    s_fn_SavedState_Store = reinterpret_cast<fn_SavedState_Xfer_t>(
        ResolveExport("?Store@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z"));
    s_fn_SavedState_Retrieve = reinterpret_cast<fn_SavedState_Xfer_t>(
        ResolveExport("?Retrieve@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z"));
    s_fn_Ser_BoolArr = reinterpret_cast<fn_Ser_BoolArr_t>(
        ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEA_NPEBD@Z"));
    s_fn_Ser_UintArr = reinterpret_cast<fn_Ser_UintArr_t>(
        ResolveExport("?Serialize@Serializer@IO@@QEAAXHPEAIPEBD@Z"));
    s_ppIntFactory = reinterpret_cast<void**>(
        ResolveExport("?_singleton@InternalFactory@Object@@0PEAV12@EA"));
    s_ppAchMgr = reinterpret_cast<void**>(
        ResolveExport("?_singleton@AchievementManager@Achievement@@0PEAV12@EA"));
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
            "saveSettings=%p rand=%p achP=%p anim=%p audio=%p cms=%p/%p sst=%p/%p "
            "subs: cit=%p grid=%p goals=%p citPost=%p envPtr=%p\n",
            (void*)s_fn_EnableRefMode, (void*)s_fn_SerializeRefTable,
            (void*)s_fn_ResetPersist, (void*)s_fn_ClearPersist,
            (void*)s_fn_SaveAllSettings, (void*)s_fn_Random_Serialize,
            (void*)s_fn_AchMgr_Persist, (void*)s_fn_AnimMgr_Serialize,
            (void*)s_fn_AudioScene_Serialize, (void*)s_fn_CMS_Ctor,
            (void*)s_fn_CMS_Open, (void*)s_fn_SavedState_Store,
            (void*)s_fn_SavedState_Retrieve, (void*)s_fn_CitList_8f760,
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
extern "C" void EventLog_Resize_199a0(void* hdrVoid, int newCount) {
    uintptr_t hdr = (uintptr_t)hdrVoid;
    int oldCount = *(int*)(hdr + 0x8);
    if (newCount == oldCount) return;

    uint8_t* oldData = *(uint8_t**)hdr;
    *(int*)(hdr + 0x8) = newCount;
    if (*(int*)(hdr + 0xc) < newCount) {
        int cap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) cap = newCount * 3;
        *(int*)(hdr + 0xc) = cap;
        *(void**)hdr = fn_MemAlloc(g_pMemAllocSingleton, (int64_t)cap * 0x28, 8);
    }
    uint8_t* newData = *(uint8_t**)hdr;

    int copyFrom = (oldData == newData) ? oldCount : 0;
    for (int i = copyFrom; i < newCount; i++) {
        uint8_t* dst = newData + (size_t)i * 0x28;
        if (i < oldCount) {
            uint8_t* src = oldData + (size_t)i * 0x28;
            *(uint32_t*)dst = *(uint32_t*)src;
            fn_String_Ctor_Copy(dst + 0x8, src + 0x8);
            memcpy(dst + 0x10, src + 0x10, 5 * 4);
        } else {
            *(uint32_t*)dst = 0xffffffff;
            *(uint64_t*)(dst + 0x8) = 0;                 // null String
            *(uint64_t*)(dst + 0x10) = 0;                // a=b=0
            *(uint64_t*)(dst + 0x18) = 0xffffffffffffffff; // c=d=-1
            *(uint32_t*)(dst + 0x20) = 0xffffffff;       // e=-1
        }
    }

    if (oldData == newData) {
        // shrink in place: dtor the removed tail Strings
        for (int i = newCount; i < oldCount; i++)
            fn_String_Dtor(oldData + (size_t)i * 0x28 + 0x8);
    } else if (oldData) {
        for (int i = 0; i < oldCount; i++)
            fn_String_Dtor(oldData + (size_t)i * 0x28 + 0x8);
        fn_MemFreeStatic(oldData);
    }
}

// ============================================================
// 0x1400952c0 — event-log serialize (GUL+0x138 object)
// ============================================================
// Layout: +0x10 bool[6] filter flags; +0x18 {data,count,cap} entry array
// (stride 0x28). Version-1 section; entries capped at the LAST 200.
extern "C" void EventLog_Serialize_952c0(void* selfVoid, void* stream) {
    EnsureResolved();
    uintptr_t self = (uintptr_t)selfVoid;

    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 1);
    if (s_fn_Ser_BoolArr) s_fn_Ser_BoolArr(ser, 6, (void*)(self + 0x10), nullptr);
    if (g_fn_Serializer_GetVersion(ser) > 0) {
        uint32_t total = *(uint32_t*)(self + 0x20);
        uint32_t capped = (int)total > 200 ? 200u : total;
        int skip = (int)(total - capped);
        if (skip < 0) skip = 0;
        g_fn_Serialize_Int(ser, &capped, nullptr);
        if (!IsSaving(stream)) {
            EventLog_Resize_199a0((void*)(self + 0x18), (int)capped);
        }
        for (int i = skip; i < *(int*)(self + 0x20); i++) {
            uint8_t* e = *(uint8_t**)(self + 0x18) + (size_t)i * 0x28;
            g_fn_Serialize_Int(ser, e, nullptr);
            g_fn_String_Serialize(e + 0x8, ser);
            g_fn_Serialize_Int(ser, e + 0x10, nullptr);
            g_fn_Serialize_Int(ser, e + 0x14, nullptr);
            g_fn_Serialize_Int(ser, e + 0x18, nullptr);
            g_fn_Serialize_Int(ser, e + 0x1c, nullptr);
            g_fn_Serialize_Int(ser, e + 0x20, nullptr);
        }
    }
    g_fn_Serializer_Dtor(ser);
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

    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 0);

    if (s_fn_EnvPtrSer_192a0) s_fn_EnvPtrSer_192a0((void*)(self + 0x00), ser);
    else Reimpl_ExternalPtr_Serialize_EnvironmentDesc((void*)(self + 0x00), ser);
    s_fn_CfgSer_194a0((void*)(self + 0x08), ser);

    g_fn_Serialize_Int(ser, (void*)(self + 0x10), nullptr);
    g_fn_Serialize_Int(ser, (void*)(self + 0x14), nullptr);
    fn_Serializer_Serialize_Float(ser, (float*)(self + 0x18), nullptr);
    fn_Math_Vector4_Serialize((void*)(self + 0x1c), ser);
    fn_Serializer_Serialize_Float(ser, (float*)(self + 0x2c), nullptr);
    fn_Serializer_Serialize_Float(ser, (float*)(self + 0x30), nullptr);
    fn_Serializer_Serialize_Float(ser, (float*)(self + 0x44), nullptr);
    fn_Serializer_Serialize_Float(ser, (float*)(self + 0x48), nullptr);
    g_fn_Serialize_Int(ser, (void*)(self + 0x4c), nullptr);

    int slotCount = *(int*)(self + 0x58);
    for (int i = 0; i < slotCount; i++) {
        void** slot = (void**)(*(uintptr_t*)(self + 0x50) + (size_t)i * 8);
        bool active = (*slot != nullptr);
        g_fn_Serialize_Bool(ser, &active, nullptr);
        if (active) {
            if (!IsSaving(stream)) {
                WeatherEffect_SetEnabled(selfVoid, i, 1);
            }
            uint8_t* geo = (uint8_t*)*slot;
            fn_Serializer_Serialize_Float(ser, (float*)(geo + 0xc4), nullptr);
            fn_Serializer_Serialize_Float(ser, (float*)(geo + 0xc0), nullptr);
        }
    }
    g_fn_Serializer_Dtor(ser);
}

// ============================================================
// 0x14002e000 — camera-view array resize (stride 0x1c, align 4)
// ============================================================
// hdr = { void* data; int count; int cap }. Element: { Vector3 pos; float a;
// float b; float c; bool used } (0x1c bytes). Retail body is a 4x-unrolled
// copy loop — ported as a plain loop with identical semantics.
extern "C" void CamViewArray_Resize_e000(void* hdrVoid, int newCount) {
    uintptr_t hdr = (uintptr_t)hdrVoid;
    int oldCount = *(int*)(hdr + 0x8);
    if (newCount == oldCount) return;

    uint8_t* oldData = *(uint8_t**)hdr;
    *(int*)(hdr + 0x8) = newCount;
    if (*(int*)(hdr + 0xc) < newCount) {
        int cap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) cap = newCount * 3;
        *(int*)(hdr + 0xc) = cap;
        *(void**)hdr = fn_MemAlloc(g_pMemAllocSingleton, (int64_t)cap * 0x1c, 4);
    }
    uint8_t* newData = *(uint8_t**)hdr;

    int copyFrom = (oldData == newData) ? oldCount : 0;
    for (int i = copyFrom; i < newCount; i++) {
        uint8_t* dst = newData + (size_t)i * 0x1c;
        if (i < oldCount) {
            memcpy(dst, oldData + (size_t)i * 0x1c, 0x18);
            dst[0x18] = oldData[(size_t)i * 0x1c + 0x18];
        } else {
            memset(dst, 0, 0x1c);
        }
    }
    if (oldData != newData && oldData) fn_MemFreeStatic(oldData);
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
    g_fn_Serialize_Int(serializer, &count, nullptr);

    void* stream = *(void**)serializer;   // Serializer+0x00 = stream
    if (*(int*)((uintptr_t)stream + 8) == 0) {
        // LOAD: read `count` records, insert each
        for (int i = 0; i < count; i++) {
            struct { void* ref; int a; int b; bool used; } rec;
            rec.ref = nullptr; rec.a = 0; rec.b = 0; rec.used = false;
            s_fn_BuildCompRefSer(&rec.ref, serializer);
            g_fn_Serialize_Int(serializer, &rec.a, nullptr);
            g_fn_Serialize_Int(serializer, &rec.b, nullptr);
            g_fn_Serialize_Bool(serializer, &rec.used, nullptr);
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
            g_fn_Serialize_Int(serializer, (void*)(cur + 0x8), nullptr);
            g_fn_Serialize_Int(serializer, (void*)(cur + 0xc), nullptr);
            g_fn_Serialize_Bool(serializer, (void*)(cur + 0x10), nullptr);
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
    static uint32_t s_tidCompEnt = 0;
    if (!s_tidCompEnt) s_tidCompEnt = ::Object::GetTypeIdByName("ComponentEntity");

    int count = *(int*)(tree + 0x8);
    g_fn_Serialize_Int(serializer, &count, nullptr);

    void* stream = *(void**)serializer;
    if (*(int*)((uintptr_t)stream + 8) == 0) {
        // LOAD
        for (int i = 0; i < count; i++) {
            void* key = nullptr;
            int a = 0, b = 0;
            bool used = false;
            fn_InternalBasePtr_Serialize(&key, serializer);
            if (key && s_tidCompEnt && !fn_IsDerivedFrom(key, s_tidCompEnt)) key = nullptr;
            g_fn_Serialize_Int(serializer, &a, nullptr);
            g_fn_Serialize_Int(serializer, &b, nullptr);
            g_fn_Serialize_Bool(serializer, &used, nullptr);

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
                fn_InternalBasePtr_CheckPtr(&cur);   // retail CheckPtrs the node slot
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
            fn_InternalBasePtr_Serialize((void*)cur, serializer);
            if (*(void**)cur && s_tidCompEnt && !fn_IsDerivedFrom(*(void**)cur, s_tidCompEnt))
                *(void**)cur = nullptr;
            g_fn_Serialize_Int(serializer, (void*)(cur + 0x8), nullptr);
            g_fn_Serialize_Int(serializer, (void*)(cur + 0xc), nullptr);
            g_fn_Serialize_Bool(serializer, (void*)(cur + 0x10), nullptr);
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

    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 2);
    g_fn_Serialize_Bool(ser, (void*)(self + 0x30), nullptr);
    g_fn_Serialize_Int(ser, (void*)(self + 0x34), nullptr);
    fn_Serializer_Serialize_Float(ser, (float*)(self + 0x3c), nullptr);
    fn_Serializer_Serialize_Float(ser, (float*)(self + 0x40), nullptr);
    fn_Serializer_Serialize_Float(ser, (float*)(self + 0x44), nullptr);
    GoalRefTree_Serialize_e2e0((void*)(self + 0x50), ser);
    if (g_fn_Serializer_GetVersion(ser) > 0) {
        IbpTree_Serialize_e470((void*)(self + 0x60), ser);
    }
    if (g_fn_Serializer_GetVersion(ser) > 1) {
        uint32_t count = *(uint32_t*)(self + 0x78);
        g_fn_Serialize_Int(ser, &count, nullptr);
        if (!IsSaving(stream)) {
            CamViewArray_Resize_e000((void*)(self + 0x70), (int)count);
        }
        for (uint32_t i = 0; i < count; i++) {
            uint8_t* e = *(uint8_t**)(self + 0x70) + (size_t)i * 0x1c;
            fn_Math_Vector3_Serialize(e, ser);
            fn_Serializer_Serialize_Float(ser, (float*)(e + 0xc), nullptr);
            fn_Serializer_Serialize_Float(ser, (float*)(e + 0x10), nullptr);
            fn_Serializer_Serialize_Float(ser, (float*)(e + 0x14), nullptr);
            g_fn_Serialize_Bool(ser, e + 0x18, nullptr);
        }
    }
    g_fn_Serializer_Dtor(ser);
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

    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 0);

    // overlay array (+0x28) first — matches the retail order
    int n = *(int*)(self + 0x30);
    g_fn_Serialize_Int(ser, &n, nullptr);
    if (!IsSaving(stream)) s_fn_IntArrResize((void*)(self + 0x28), n);
    if (s_fn_Ser_UintArr) s_fn_Ser_UintArr(ser, n, *(void**)(self + 0x28), nullptr);

    // terrain array (+0x18)
    n = *(int*)(self + 0x20);
    g_fn_Serialize_Int(ser, &n, nullptr);
    if (!IsSaving(stream)) s_fn_IntArrResize((void*)(self + 0x18), n);
    if (s_fn_Ser_UintArr) s_fn_Ser_UintArr(ser, n, *(void**)(self + 0x18), nullptr);

    g_fn_Serialize_Int(ser, (void*)(self + 0x4), nullptr);

    if (!IsSaving(stream)) {
        Minimap_CreateAndUpload(selfVoid);
    }
    g_fn_Serializer_Dtor(ser);
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
        fn_InternalBasePtr_CheckPtr(entSlot);
        uintptr_t ent = (uintptr_t)*entSlot;

        int compCount = *(int*)(ent + 0x38);
        for (int j = 0; j < compCount; j++) {
            void** compSlot = (void**)(*(uintptr_t*)(ent + 0x30) + (size_t)j * 8);
            fn_InternalBasePtr_CheckPtr(compSlot);
            uintptr_t comp = (uintptr_t)*compSlot;
            typedef void (*fn_postload_t)(void* self);
            (*(fn_postload_t*)(*(uintptr_t*)comp + 0x58))((void*)comp);
        }

        fn_InternalBasePtr_CheckPtr(entSlot);
        *(void**)((uintptr_t)*entSlot + 0x10) = elVoid;

        fn_InternalBasePtr_CheckPtr(entSlot);
        uintptr_t e = (uintptr_t)*entSlot;
        if (*(uintptr_t*)(e + 0x10) != 0) {
            uintptr_t owner = *(uintptr_t*)(e + 0x10);
            uint32_t flags = *(uint32_t*)(e + 0x40);
            if (flags & 4) {
                void* ibp = nullptr;
                fn_InternalBasePtr_Ctor(&ibp, (void*)e);
                s_fn_DynArr8_ResizeFill((void*)(owner + 0x88),
                                        *(int*)(owner + 0x90) + 1, &ibp);
            }
            if (flags & 0x10) {
                void* ibp = nullptr;
                fn_InternalBasePtr_Ctor(&ibp, (void*)e);
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
// (fn_ExeArrayResize = 0x1400032e0), then per-element InternalBasePtr
// serialize with a ComponentEntity IsDerivedFrom filter.
extern "C" void BucketTree_Serialize_21ba0(void* serializer, void* /*unused*/,
                                           void* bucketsPtr) {
    static uint32_t s_tidCompEnt = 0;
    if (!s_tidCompEnt) s_tidCompEnt = ::Object::GetTypeIdByName("ComponentEntity");

    uintptr_t hdr = (uintptr_t)bucketsPtr;
    for (int b = 0; b < 5; b++, hdr += 0x10) {
        int count = *(int*)(hdr + 0x8);
        g_fn_Serialize_Int(serializer, &count, nullptr);
        if (*(int*)((uintptr_t)*(void**)serializer + 8) == 0) {
            fn_ExeArrayResize((void*)hdr, count);
        }
        uint8_t* slot = *(uint8_t**)hdr;
        for (int i = 0; i < count; i++, slot += 8) {
            fn_InternalBasePtr_Serialize(slot, serializer);
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

// ---------------------------------------------------------------------------
// BANISHED_SER_TRACE — stream-cursor probe.
//
// A load that dies inside a sub-serializer is almost always a *stream desync*
// that started in an earlier section: one side wrote N bytes, the other read
// M. There is no IO::Stream::Tell export, so dump the head of the stream
// object at each section boundary — whichever qword advances monotonically is
// the cursor. Run once saving and once loading with the env var set and diff
// the two traces: the first section where the cursors stop matching is the
// section that is wrong.
// ---------------------------------------------------------------------------
static bool SerTraceOn() {
    static int s_on = -1;
    if (s_on < 0) {
        const char* e = std::getenv("BANISHED_SER_TRACE");
        s_on = (e && *e && e[0] != '0') ? 1 : 0;
    }
    return s_on != 0;
}

static void SerTrace(void* stream, const char* stage) {
    if (!SerTraceOn() || !stream) return;
    const uint64_t* q = reinterpret_cast<const uint64_t*>(stream);
    fprintf(stderr, "[SerTrace] %-4s %-22s",
            IsSaving(stream) ? "SAVE" : "LOAD", stage);
    for (int i = 0; i < 8; i++) fprintf(stderr, " %016llx", (unsigned long long)q[i]);
    fprintf(stderr, "\n");
    fflush(stderr);
}

// ---------------------------------------------------------------------------
// BANISHED_SAVE_TIMING - per-section wall clock for the save/load snapshot.
//
// The whole snapshot runs SYNCHRONOUSLY on the main thread inside
// Save::SaveLoad::UpdateSave (Runtime 0x1800c2a80 calls the loop vtable +0x38
// before it queues the async file write), exactly as retail does - so however
// long this takes is a hard freeze with the spinner stopped. Retail is fast
// enough that nobody notices; when ours is not, this says which section owns
// the time.
// ---------------------------------------------------------------------------
static bool SaveTimingOn() {
    static int s_on = -1;
    if (s_on < 0) {
        const char* e = std::getenv("BANISHED_SAVE_TIMING");
        s_on = (e && *e && e[0] != 0x30) ? 1 : 0;
    }
    return s_on != 0;
}

static long long SerQpc() {
    LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart;
}
static double SerQpcMs(long long ticks) {
    static long long f = 0;
    if (!f) { LARGE_INTEGER q; QueryPerformanceFrequency(&q); f = q.QuadPart; }
    return f ? 1000.0 * (double)ticks / (double)f : 0.0;
}

namespace { long long g_serMark = 0; }

static void SerMark(const char* stage) {
    if (!SaveTimingOn()) return;
    long long now = SerQpc();
    if (g_serMark) {
        fprintf(stderr, "[SerTiming] %-22s %8.2fms\n", stage, SerQpcMs(now - g_serMark));
        fflush(stderr);
    }
    g_serMark = now;
}
extern "C" void GameUpdateLoop_Serialize_95a90(void* selfVoid, void* stream,
                                               void* p3, int* outParam) {
    (void)p3;
    EnsureResolved();
    uintptr_t self = (uintptr_t)selfVoid;
    void* extFactory = *g_pExternalFactorySingleton;
    void* intFactory = s_ppIntFactory ? *s_ppIntFactory : nullptr;

    fprintf(stderr, "[GUL::Serialize] %s ENTER self=%p stream=%p\n",
            IsSaving(stream) ? "SAVE" : "LOAD", selfVoid, stream);
    fflush(stderr);

    g_serMark = 0;
    SerMark("enter");
    s_fn_EnableRefMode(extFactory, true);
    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 3);
    s_fn_SerializeRefTable(extFactory, stream);
    s_fn_ResetPersist(intFactory, stream);

    if (IsSaving(stream)) {
        s_fn_SaveAllSettings(*(void**)(self + 0x98));
    }

    s_fn_CfgSer_194a0((void*)(self + 0x50), ser);
    s_fn_CfgSer_19ec0((void*)(self + 0x40), ser);
    s_fn_CfgSer_1a0c0((void*)(self + 0x48), ser);

    g_fn_Serialize_Int(ser, (void*)(self + 0x30), nullptr);
    g_fn_Serialize_Int(ser, (void*)(self + 0x34), nullptr);
    if (g_fn_Serializer_GetVersion(ser) > 0)
        fn_Serializer_Serialize_Uint(ser, (uint32_t*)(self + 0xd0), nullptr);
    if (g_fn_Serializer_GetVersion(ser) > 2)
        g_fn_Serialize_Int(ser, (void*)(self + 0x15c), nullptr);

    s_fn_Random_Serialize(*(void**)(self + 0xd8), ser);
    g_fn_String_Serialize((void*)(self + 0x28), ser);
    g_fn_Serialize_Bool(ser, (void*)(self + 0x158), nullptr);

    fn_InternalBasePtr_Serialize((void*)(self + 0x150), ser);
    if (*(void**)(self + 0x150) != nullptr) {
        uint32_t achTid = s_pAchInstTypeId ? *s_pAchInstTypeId : 0;
        if (!achTid || !fn_IsDerivedFrom(*(void**)(self + 0x150), achTid))
            *(void**)(self + 0x150) = nullptr;
    }

    if (s_ppAchMgr && *s_ppAchMgr) s_fn_AchMgr_Persist(*s_ppAchMgr, stream);
    s_fn_AnimMgr_Serialize(*(void**)(self + 0x68), ser);
    s_fn_AudioScene_Serialize(*(void**)(self + 0x78), ser);

    fprintf(stderr, "[GUL::Serialize] core done — subsystems...\n"); fflush(stderr);

    SerMark("pre-camera");
    SerTrace(stream, "pre-camera");
    s_fn_Camera_988c0(*(void**)(self + 0xe0), stream);
    SerMark("pre-entlist");
    SerTrace(stream, "pre-entlist");
    s_fn_EntList_c44a0(*(void**)(self + 0xe8), stream);
    SerMark("pre-geom");
    SerTrace(stream, "pre-geom");
    s_fn_Geom_b04d0(*(void**)(self + 0x110), stream);
    SerMark("pre-citlist");
    SerTrace(stream, "pre-citlist");
    s_fn_CitList_8f760(*(void**)(self + 0xf0), stream, nullptr, outParam);
    SerMark("pre-grid");
    SerTrace(stream, "pre-grid");
    s_fn_Grid_9f100(*(void**)(self + 0xf8), stream);
    SerMark("pre-goals");
    SerTrace(stream, "pre-goals");
    s_fn_Goals_f9240(*(void**)(self + 0x118), stream, nullptr, outParam);
    SerMark("post-goals");
    SerTrace(stream, "post-goals");
    EntityUI_Serialize_102920(*(void**)(self + 0x120), stream);
    SerMark("reached-entityui");

    // Day counter (+0x128): { int day; Time accumulated } — version-0 section.
    {
        uint8_t* dc = *(uint8_t**)(self + 0x128);
        uint8_t ser0[24];
        g_fn_Serializer_Ctor(ser0, stream, 0);
        g_fn_Serialize_Int(ser0, dc, nullptr);
        g_fn_Time_Serialize(dc + 0x8, ser0);
        g_fn_Serializer_Dtor(ser0);
    }

    if (g_fn_Serializer_GetVersion(ser) > 1)
        EventLog_Serialize_952c0(*(void**)(self + 0x138), stream);
    SerMark("reached-eventlog");
    WeatherSim_Serialize_93c90(*(void**)(self + 0x130), stream);
    SerMark("reached-weather");
    Minimap_Serialize_a57f0(*(void**)(self + 0x148), stream);
    SerMark("reached-minimap");
    EntityList_PostLoad_c4730(*(void**)(self + 0xe8), stream);
    SerMark("reached-postload");
    s_fn_CitPost_8f580(*(void**)(self + 0xf0));
    SerMark("reached-citpost");

    if (intFactory) s_fn_ClearPersist(intFactory);
    s_fn_EnableRefMode(extFactory, false);
    g_fn_Serializer_Dtor(ser);

    fprintf(stderr, "[GUL::Serialize] %s DONE\n", IsSaving(stream) ? "SAVE" : "LOAD");
    fflush(stderr);
}

// ============================================================
// 0x1400959c0 — SaveRestore (GUL vtable +0x38)
// ============================================================
// save != 0: serialize the world into a fresh CompressedMemoryStream, then
// Store the embedded MemoryStream into the SavedState. save == 0: Retrieve
// the SavedState into the stream, Open (decompress), then deserialize.
extern "C" void GameUpdateLoop_SaveRestore(void* self, char save,
                                           void* savedState, int* outParam) {
    EnsureResolved();
    if (!s_fn_CMS_Ctor || !s_fn_CMS_GetMemStream ||
        !s_fn_SavedState_Store || !s_fn_SavedState_Retrieve) {
        fprintf(stderr, "[GUL::SaveRestore] MISSING Runtime exports — no-op\n");
        fflush(stderr);
        return;
    }

    alignas(16) uint8_t cms[0x100];   // sizeof(CompressedMemoryStream) = 0x68 + MemoryStream
    s_fn_CMS_Ctor(cms);
    if (save == 0) {
        s_fn_SavedState_Retrieve(savedState, s_fn_CMS_GetMemStream(cms));
        s_fn_CMS_Open(cms);
        GameUpdateLoop_Serialize_95a90(self, cms, savedState, outParam);
        s_fn_CMS_Close(cms);
    } else {
        s_fn_CMS_Create(cms);
        GameUpdateLoop_Serialize_95a90(self, cms, savedState, outParam);
        SerMark("reached-serialize-end");
        s_fn_CMS_Close(cms);
        SerMark("reached-compress-close");
        s_fn_SavedState_Store(savedState, s_fn_CMS_GetMemStream(cms));
        SerMark("reached-savedstate-store");
    }
    s_fn_CMS_Dtor(cms);
}

// ============================================================
// Registration (called from standalone/address_registry.cpp)
// ============================================================
void RegisterGulSerialize() {
    RegisterExeAddr(0x1400959c0, reinterpret_cast<uintptr_t>(&GameUpdateLoop_SaveRestore));
    RegisterExeAddr(0x140095a90, reinterpret_cast<uintptr_t>(&GameUpdateLoop_Serialize_95a90));
    RegisterExeAddr(0x1400952c0, reinterpret_cast<uintptr_t>(&EventLog_Serialize_952c0));
    RegisterExeAddr(0x1400199a0, reinterpret_cast<uintptr_t>(&EventLog_Resize_199a0));
    RegisterExeAddr(0x140093c90, reinterpret_cast<uintptr_t>(&WeatherSim_Serialize_93c90));
    RegisterExeAddr(0x140102920, reinterpret_cast<uintptr_t>(&EntityUI_Serialize_102920));
    RegisterExeAddr(0x14002e2e0, reinterpret_cast<uintptr_t>(&GoalRefTree_Serialize_e2e0));
    RegisterExeAddr(0x14002e470, reinterpret_cast<uintptr_t>(&IbpTree_Serialize_e470));
    RegisterExeAddr(0x14002e000, reinterpret_cast<uintptr_t>(&CamViewArray_Resize_e000));
    RegisterExeAddr(0x1400a57f0, reinterpret_cast<uintptr_t>(&Minimap_Serialize_a57f0));
    RegisterExeAddr(0x1400c4730, reinterpret_cast<uintptr_t>(&EntityList_PostLoad_c4730));
    RegisterExeAddr(0x140021ba0, reinterpret_cast<uintptr_t>(&BucketTree_Serialize_21ba0));
}
