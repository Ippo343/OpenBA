// code/Game/Entity/componententity.cpp
// Clean reimplementation of ComponentEntity and its non-vtable methods.
//
// ComponentEntity (0x48 bytes) -- The ECS entity container.
// Vtable: 0x14015fa78, 4 slots. Extends Object::Internal.
//
// Each entity holds:
//   - An entity ID (unique identifier)
//   - Flags (24-bit mask)
//   - A description ExternalPtr (entity template)
//   - An array of Component InternalBasePtrs
//   - Aggregated entityFlags from components (update/draw/startup/shutdown bits)
//
// Decompiled from Application-x64.exe using Ghidra.
//
// ENDGAME B.3.8 K1: a REAL class (Entity/ComponentEntity.h) with a
// compiler-generated 4-slot vtable. The 12-qword `g_ComponentEntity_Standalone
// Vtable` (these 4 slots, a null, and Action's 7 — D9 #2) and both of its
// address rows are gone: nothing reads 0x14015fa78 back, and 0x14015faa0 is
// served by Action::Vtable() (ACT).

#include "Entity/componententity_funcs.h"
#include "Component/ComponentDescription.h"
#include "Component/Component.h"
#include "Component/Description.h"
#include "Component/coll_bridge.h"
#include "options.h"
#include <windows.h>
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/TypeInfo.h"   // ENDGAME B.3.3 step 6C
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// TownManager::AbortCitizenActionsFor (exe 0x140089e30) - ported in
// code/Game/Component/destroycomponent_state6.cpp.
extern "C" void TownMgr_AbortCitizenActionsFor_89e30(void* townMgr, void** pEntityIbp);

extern "C" int  EntityCompValidate(void*, void*, void*, int);  // standalone/entity_comp_validator.cpp

namespace {
// B6-5: the diagnostics' one-shot/limit counters are namespace-scope (were
// function-local statics in the C bodies).
std::atomic<int> s_loadLogCount{0};        // Persist: first 8 loads logged
std::atomic<int> s_noFactoryWarnings{0};   // ctor: first 32 factory misses logged
// Persist's BANISHED_SERIALIZE_TRACE stream-cursor probe state (diagnostic only).
uint32_t s_tracePrev[64];
bool     s_traceHavePrev = false;
bool     s_traceDumped   = false;

// Each component template's GetComponentType (Description slot 8, a retslot).
uint32_t TemplateComponentType(void* compTemplate) {
    Object::Type typeId(0);
    reinterpret_cast<const Description*>(compTemplate)->GetComponentType(&typeId);  // B.3.8 bridge
    return typeId;
}
}  // namespace

// ============================================================
// Constructors
// ============================================================

// Default constructor (0x1400c3620): `Internal()`, the stamp (the compiler's),
// then the fields. The +0x1C handle comes out as 0x00FFFFFF: retail ORs the low
// 24 bits in and zeroes the +0x1F category byte.
ComponentEntity::ComponentEntity()
    : Object::Internal(),
      _entityList(nullptr),
      _entityId(-1),
      _handle(0x00FFFFFF),
      _externals(nullptr),
      _description(nullptr),
      _components(nullptr),
      // Retail zeroes the FULL qword at +0x38 — count AND capacity. Zeroing only
      // the int32 count left +0x3C (cap) holding recycled heap bytes; on the
      // load-from-save path the persist-created entity's resize then saw
      // cap > count, skipped the alloc, and the IBP loop deref'd a null array
      // ("a 4-byte port of an 8-byte zero strands an array at NULL").
      _componentCount(0),
      _componentCap(0),
      _entityFlags(0) {}

// Ctor from a description (0x1400c3720, retail's "CopyCtor").
ComponentEntity::ComponentEntity(ComponentDescription* description)
    : Object::Internal(),
      _entityList(nullptr),
      _entityId(-1),
      _handle(0x00FFFFFF),
      _externals(nullptr),
      _description(description),       // +0x28
      // The entity block comes from MemAlloc(0x48), which does NOT zero memory. The
      // original CopyCtor zeroes the full component-array header (data/count/cap);
      // leaving +0x3c (capacity) as heap garbage makes the SetSize below see a
      // bogus oldCap >= newCount and skip the allocation, leaving components==null
      // and crashing the insert. (Latent until the description +0x40 fix made
      // validCount > 0.)
      _components(nullptr),
      _componentCount(0),
      _componentCap(0),
      _entityFlags(0) {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    int descCompCount = description->_subDescCount;

    // Sanity guard: a corrupted/unexpected description (component-count field read
    // as garbage) would size the component array to an absurd value and segfault in
    // the SetSize below. Bail loudly with the desc pointer so the bad template is
    // identifiable. (Seen intermittently during resource scatter — likely heap
    // corruption upstream or an unexpected desc layout.)
    if (descCompCount < 0 || descCompCount > 256) {
        fprintf(stderr, "[CtorFromDesc] ABSURD descCompCount=%d desc=%p — entity gets 0 components\n",
                descCompCount, (void*)description);
        fflush(stderr);
        descCompCount = 0;
    }

    // First pass: count components with non-zero type IDs
    int validCount = 0;
    for (int64_t i = 0; i < (int64_t)descCompCount; i++) {
        if (TemplateComponentType(description->_subDescs[i]) != 0) validCount++;
    }

    // Allocate component array.
    // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
    // new tail zeroed. Element is the 8-byte component InternalBasePtr (byte-copy).
    GameColl::ArrayAt<GameColl::Ptr8>(s, 0x30).SetSize(validCount);

    // Second pass: create components
    int insertIdx = 0;
    for (int64_t i = 0; i < (int64_t)descCompCount; i++) {
        void* compTemplate = description->_subDescs[i];
        uint32_t typeId = TemplateComponentType(compTemplate);
        if (typeId == 0) continue;

        // Create component from factory
        void* factoryPtr = g_pFactorySingleton ? *(void**)*g_pFactorySingleton : nullptr;
        void* newObj = nullptr;
        if (factoryPtr) {
            newObj = reinterpret_cast<const ::Object::Factory*>(factoryPtr)->CreateObject(
                typeId);   // B.3.3 bridge
        }

        // Verify it derives from Component. In standalone the Runtime factory
        // tree is incomplete, so raw fn_IsDerivedFrom rejects valid inert
        // components (returns false / NULL-derefs) — trust a non-null
        // CreateObject result instead (Object::IsDerivedFromOrTrust). This path
        // only began executing once the description +0x40 GetComponentTypeId
        // was implemented; before that typeId was always 0 and no component was
        // ever created, masking the missing CheckPtr below.
        void* newComp = nullptr;
        if (newObj && Object::IsDerivedFromOrTrust(
                          newObj, g_TypeId_ComponentBase ? *g_TypeId_ComponentBase : 0)) {
            newComp = newObj;
        }

        // Create IBP for new component and owner
        uintptr_t newIBP = 0;
        if (fn_InternalBasePtr_Ctor) {
            new ((void*)&newIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(newComp));   // B.3.3 bridge
        }
        uintptr_t ownerIBP = 0;
        if (fn_InternalBasePtr_Ctor) {
            new ((void*)&ownerIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
        }

        // Resolve the component IBP (the original CopyCtor calls CheckPtr
        // before the field writes) then set owner (+0x10) and description
        // (+0x18).
        void* compPtr = (void*)newIBP;
        if (!compPtr) {
            // STANDALONE: this component typeId has no working factory (the
            // Runtime/our factory tree is incomplete — e.g. a Tree's component
            // whose Foundation-D factory is missing). The original game always
            // has every factory, so it never hits this. Skip the component
            // rather than insert a null slot that crashes the array readers
            // (RefreshEntityFlags / OnAddedPass / StartupComponents).
            if (s_noFactoryWarnings.fetch_add(1) < 32) {
                fprintf(stderr, "[CtorFromDesc] no factory for component typeId 0x%08x — skipped\n", typeId);
                fflush(stderr);
            }
            continue;
        }
        *(uintptr_t*)((uintptr_t)compPtr + 0x10) = ownerIBP;
        *(void**)((uintptr_t)compPtr + 0x18) = compTemplate;

        // Store in component array
        static_cast<uintptr_t*>(_components)[insertIdx] = newIBP;
        insertIdx++;
    }

    // Correct the array count: the SetSize above sized it to validCount, but in
    // standalone some components may have been skipped (missing factory), so the
    // real populated count is insertIdx. Leaving it at validCount would let the
    // readers walk past the last valid slot into uninitialised memory.
    _componentCount = insertIdx;

    // Refresh flags and call startup on flagged components
    RefreshEntityFlags();
    StartupComponents();
}

// ============================================================
// Destructor body (0x1400c3550)
// ============================================================
// ShutdownComponents, the weak-ref invalidation, clear + free the component
// IBPs; the compiler then chains Object::Internal::~Internal. (Named "Ctor" in
// Ghidra — misleading.)
ComponentEntity::~ComponentEntity() {
    // Shutdown flagged components
    ShutdownComponents();

    // Retail's weak-ref invalidation, restored (was missing from this port).
    // 0x1400c357c-0x1400c359d: build an InternalBasePtr onto the dying entity and hand
    // it to TownManager::AbortCitizenActionsFor (0x140089e30, GameWorld+0xf0), which
    // walks every citizen's CitizenAIComponent action stack and tears down any stack
    // holding an action that references this entity (Action vtable slot 9 GetTarget ->
    // slot 8 Abort). Without it, an in-flight SendComponentMessage keeps its +0x20
    // target pointing at this block; nothing else invalidates that ref (CheckPtr is an
    // empty function in the shipping Runtime), so once the block is freed AND RECYCLED
    // every downstream probe in SendComponentMessage::AbstractImpl is defeated - the
    // pointer is canonical, the memory is readable, and the marked-for-removal bit
    // reads clear because those bytes now belong to something else. That is the crash
    // that kept moving (+0x1a4 sentinel deref -> +0x24a garbage vtable dispatch) as
    // each downstream guard was added. This is the correct layer: the refs die with
    // the entity, while it is still addressable. Same idea as the drain's
    // EntityList_ClearToolSelectionForDyingEntities, but retail-authored.
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
        if (townMgr) {
            void* entityIbp = nullptr;
            if (fn_InternalBasePtr_Ctor) new (&entityIbp) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
            else                         entityIbp = this;
            TownMgr_AbortCitizenActionsFor_89e30(reinterpret_cast<void*>(townMgr),
                                                 &entityIbp);
        }
    }

    // Iterate component IBPs: null each out, then delete old value
    int count = _componentCount;
    if (count > 0) {
        uintptr_t arr = (uintptr_t)_components;
        for (int i = 0; i < count; i++) {
            uintptr_t ibpAddr = arr + (uintptr_t)i * 8;
            // Save old IBP value
            uintptr_t oldIBP = *(uintptr_t*)ibpAddr;
            // Construct null IBP in place
            if (fn_InternalBasePtr_Ctor) {
                new ((void*)ibpAddr) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            }
            // Delete old IBP (releases the component)
            if (fn_InternalBasePtr_DeletePtr) {
                reinterpret_cast< ::Object::InternalBasePtr*>((void*)&oldIBP)->DeletePtr();   // B.3.3 bridge
            }
        }
    }

    // Free component array memory
    if (_components) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(_components);
        }
    }
    _components = nullptr;
    _componentCount = 0;
}

// [0] 0x140021a00: ~ComponentEntity (0x1400c3550) -> ~Internal, MemFree on &1.
Object::Base* ComponentEntity::DeletingDtor(unsigned flags) {
    this->~ComponentEntity();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// ShutdownComponents (0x1400c3000)
// ============================================================
// If entityFlags bit 8 (0x100) is set, iterates components and calls
// vtable+0x28 (Shutdown) on each component whose Component+0x20 bit 8 is set.
void ComponentEntity::ShutdownComponents() {
    if ((_entityFlags & 0x100) == 0) return;

    int count = _componentCount;
    if (count <= 0) return;

    uintptr_t arr = (uintptr_t)_components;
    for (int i = 0; i < count; i++) {
        uintptr_t ibpAddr = arr + (uintptr_t)i * 8;


        uintptr_t compPtr = *(uintptr_t*)ibpAddr;
        // Component+0x20 is uint32 containing capability flags
        uint32_t compFlags = *(uint32_t*)(compPtr + 0x20);
        if (((compFlags >> 8) & 1) != 0) {
            // Re-check after potential lazy resolve
            compPtr = *(uintptr_t*)ibpAddr;
            // Virtual slot 5 (+0x28, the shutdown slot)
            reinterpret_cast<Component*>(compPtr)->Slot5();  // B.3.8 bridge
        }
    }
}

// ============================================================
// StartupComponents (0x1400c30a0)
// ============================================================
// If entityFlags bit 7 (0x80) is set, iterates components and calls
// vtable+0x20 (Startup) on each component whose Component+0x20 bit 7 is set.
void ComponentEntity::StartupComponents() {
    if ((_entityFlags & 0x80) == 0) return;

    int count = _componentCount;
    if (count <= 0) return;

    uintptr_t arr = (uintptr_t)_components;
    for (int i = 0; i < count; i++) {
        uintptr_t ibpAddr = arr + (uintptr_t)i * 8;


        uintptr_t compPtr = *(uintptr_t*)ibpAddr;
        if (!compPtr) continue;   // defensive: skip null component (standalone factory miss)
        uint32_t compFlags = *(uint32_t*)(compPtr + 0x20);
        if (((compFlags >> 7) & 1) != 0) {
            compPtr = *(uintptr_t*)ibpAddr;
            // Virtual slot 4 (+0x20): Startup
            reinterpret_cast<Component*>(compPtr)->Startup();  // B.3.8 bridge
        }
    }
}

// ============================================================
// RefreshEntityFlags (0x1400c3470)
// ============================================================
// Recomputes entityFlags by ORing in Component::+0x20 (ushort) for each
// component. If update bit (2) or draw bit (4) changed, registers entity
// in the appropriate EntityList bucket.
void ComponentEntity::RefreshEntityFlags() {
    uint32_t oldFlags = _entityFlags;

    // Reset entityFlags, then OR in each component's flags
    _entityFlags = 0;
    int count = _componentCount;
    if (count > 0) {
        uintptr_t arr = (uintptr_t)_components;
        for (int i = 0; i < count; i++) {
            uintptr_t ibpAddr = arr + (uintptr_t)i * 8;
            uintptr_t compPtr = *(uintptr_t*)ibpAddr;
            if (!compPtr) continue;   // defensive: skip null component (standalone factory miss)
            // Component::+0x20 is a uint16 containing capability flags
            uint16_t compFlags = *(uint16_t*)(compPtr + 0x20);
            _entityFlags |= (uint32_t)compFlags;
        }
    }

    // If entityList is set (+0x10), check for flag changes that need registration
    if (_entityList != nullptr) {
        uintptr_t entityListPtr = (uintptr_t)_entityList;

        // If update bit (bit 2) changed, register in update bucket
        if (((oldFlags ^ _entityFlags) & 4) != 0) {
            // EntityList +0x88 = update entity array, +0x90 = update count
            int updateCount = *(int*)(entityListPtr + 0x90) + 1;
            uintptr_t ibp = 0;
            if (fn_InternalBasePtr_Ctor) {
                new ((void*)&ibp) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
            }
            // B.3.2: Collection::Array<T>::SetSize(n, fill) — retail 0x140003110,
            // stride 8 align 8. Element is the 8-byte entity InternalBasePtr.
            GameColl::ArrayAt<GameColl::Ptr8>(entityListPtr, 0x88).SetSize(
                updateCount, *reinterpret_cast<const GameColl::Ptr8*>(&ibp));
        }

        // If draw bit (bit 4) changed, register in draw bucket
        if (((oldFlags ^ _entityFlags) & 0x10) != 0) {
            // EntityList +0x98 = draw entity array, +0xa0 = draw count
            int drawCount = *(int*)(entityListPtr + 0xa0) + 1;
            uintptr_t ibp = 0;
            if (fn_InternalBasePtr_Ctor) {
                new ((void*)&ibp) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
            }
            // B.3.2: Collection::Array<T>::SetSize(n, fill) — retail 0x140003110,
            // stride 8 align 8. Element is the 8-byte entity InternalBasePtr.
            GameColl::ArrayAt<GameColl::Ptr8>(entityListPtr, 0x98).SetSize(
                drawCount, *reinterpret_cast<const GameColl::Ptr8*>(&ibp));
        }
    }
}

// Door for entitylist.cpp (EntityList::FlagAggregation).
void ComponentEntity_RefreshEntityFlags(ComponentEntity* self) {
    self->RefreshEntityFlags();
}

// ============================================================
// [3] Persist (0x1400c3140)
// ============================================================
// Serializes entity state: description external, component count,
// component IBPs, entity flags. On load, creates components from
// factory and inserts them into the component array.
void ComponentEntity::Persist(void* stream) {
    uintptr_t s = (uintptr_t)this;

    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (stream is void* in Internal::Persist)

    // Serialize description ExternalPtr at +0x28
    if (fn_ExternalPtr_BuildSerialize) {
        fn_ExternalPtr_BuildSerialize(&_description, &ser);
    }

    // Serialize component count
    uint32_t compCount = (uint32_t)_componentCount;
    ser.Serialize(*reinterpret_cast<int*>((void*)&compCount), nullptr);

    // Check if loading (encoding mode == 0)
    int encodingMode = static_cast<int>(ser.GetDirection());   // IO::Serializer::GetDirection, retail 0x180006210

    // On load: resize component array to match serialized count
    if (encodingMode == 0) {
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte component InternalBasePtr (byte-copy).
        GameColl::ArrayAt<GameColl::Ptr8>(s, 0x30).SetSize((int)compCount);
        // Save/load wave diagnostics: a garbage count (stream misalignment) or
        // a failed resize (data still null with count>0) crashes the IBP loop
        // below — log the state on the way in so the crash log has context.
        if (s_loadLogCount.load() < 8 || _components == nullptr || compCount > 64) {
            s_loadLogCount.fetch_add(1);
            fprintf(stderr, "[CompEnt::Serialize] LOAD ent=%p desc=%p compCount=%u "
                    "arr=%p cap=%d\n", (void*)this, (void*)_description, compCount,
                    _components, _componentCap);
            fflush(stderr);
        }
    }

    // BANISHED_COMP_VALIDATE=1 — on the SAVE leg, check every component slot
    // BEFORE anything is written. A non-null slot that is not a live object is
    // what puts 0xAA-pattern objId/typeId into the stream and makes the LOAD
    // index PersistObject's id map out of bounds. Diagnostic only.
    if (encodingMode != 0)
        EntityCompValidate(this, _description, _components, (int)compCount);

    // Save/load wave debug (BANISHED_SERIALIZE_TRACE): per-component progress
    // marker so a stream desync pinpoints the last-good component instead of a
    // wild crash address. Read per call (B6-5: no lazy cache).
    const char* traceOpt = Opt().serializeTrace;
    const bool trace = traceOpt && traceOpt[0] && traceOpt[0] != '0';

    // Serialize each component IBP
    uintptr_t ibpArr = (uintptr_t)_components;
    int64_t cnt = (int64_t)(int)compCount;
    if (cnt > 0) {
        for (int64_t i = 0; i < cnt; i++) {
            uintptr_t ibpAddr = ibpArr + (uintptr_t)i * 8;
            if (trace) {
                fprintf(stderr, "[CompEnt::Ser] ent=%p comp %lld/%u begin raw=%p\n",
                        (void*)this, (long long)i, compCount, *(void**)ibpAddr);
                fflush(stderr);
            }
            // Serialize the InternalBasePtr
            reinterpret_cast<Object::InternalBasePtr*>((void*)ibpAddr)->Serialize(ser);
            void* rawAfterSer = *(void**)ibpAddr;   // before our IsDerivedFrom filter
            if (trace && !rawAfterSer) {
                fprintf(stderr, "[CompEnt::Ser] ent=%p comp %lld RAW NULL from "
                        "IBP::Serialize (create failed -> stream desync!)\n",
                        (void*)this, (long long)i);
                fflush(stderr);
            }
            if (trace) {
                void* c = *(void**)ibpAddr;
                uint32_t tid = 0;
                if (c) {
                    tid = reinterpret_cast<Object::Base*>(c)->Type();  // B.3.8 bridge
                }
                // Stream cursor (CompressedMemoryStream + 0x8C — see
                // engine/types/io_serializer.h). Logged on BOTH save and load:
                // diffing the two traces per component finds an ASYMMETRIC
                // serializer (read size != write size — a silent desync that
                // corrupts every later component without crashing).
                // +0x8C is the cursor only for a CompressedMemoryStream (its
                // MemoryStream sub-object starts at +0x68); a PLAIN MemoryStream
                // keeps it at +0x24. Print both -- whichever ADVANCES is the real
                // one. A pos that never moves across components means we are
                // reading an unrelated dword, not that the stream stalled.
                uint32_t pos = 0, pos24 = 0;
                {
                    void* strm = *(void**)&ser;
                    if (strm) {
                        pos   = *(uint32_t*)((uint8_t*)strm + 0x8C);
                        pos24 = *(uint32_t*)((uint8_t*)strm + 0x24);
                        // Neither documented cursor moves for THIS stream type, so
                        // find the real one instead of guessing a third offset:
                        // snapshot the first 0x100 bytes and report which dwords
                        // changed since the previous component. The cursor is the
                        // one that advances monotonically by each component's size.
                        uint32_t cur[64];
                        for (int k = 0; k < 64; ++k)
                            cur[k] = *(uint32_t*)((uint8_t*)strm + k * 4);
                        if (s_traceHavePrev) {
                            char buf[512]; int n = 0;
                            for (int k = 0; k < 64 && n < 400; ++k)
                                if (cur[k] != s_tracePrev[k])
                                    n += snprintf(buf + n, sizeof(buf) - n,
                                                  " +0x%02X:%u->%u", k * 4, s_tracePrev[k], cur[k]);
                            fprintf(stderr, "[CompEnt::Ser]   streamDelta%s\n",
                                    n ? buf : " (none)");
                        }
                        // One-shot full header dump. The 0xAA/0xBB values that
                        // reach PersistObject look like allocator debug fill, which
                        // would mean the reads have run off the END of the
                        // decompressed data rather than merely landing on a wrong
                        // field. Telling those two apart needs the buffer EXTENT,
                        // not another delta -- so print the raw header once and let
                        // the size field settle it against the +0x1C cursor.
                        if (!s_traceDumped) {
                            s_traceDumped = true;
                            fprintf(stderr, "[CompEnt::Ser]   streamHdr");
                            for (int k = 0; k < 40; ++k)
                                fprintf(stderr, " +0x%02X:%08X", k * 4, cur[k]);
                            fprintf(stderr, "\n");
                        }
                        // Which qword in the stream header is the DECOMPRESSED
                        // buffer? Settle it by evidence rather than by guessing an
                        // offset: at this point the previous component's record
                        // began at the previous cursor and its first dword is that
                        // component's objId, which the [Persist] trace already
                        // printed. So for each candidate base, print the dword the
                        // cursor is pointing at. The candidate that yields the
                        // NEXT objId in sequence is the buffer, and it lets the
                        // bytes at a desync be read directly instead of inferred.
                        {
                            const int cand[5] = { 0x10, 0x28, 0x30, 0x40, 0x80 };
                            uint8_t* sb = (uint8_t*)strm;
                            uint32_t cursor = cur[0x1C / 4];
                            fprintf(stderr, "[CompEnt::Ser]   cur=%u", cursor);
                            for (int c2 = 0; c2 < 5; ++c2) {
                                uint8_t* base = *(uint8_t**)(sb + cand[c2]);
                                uint32_t v = 0; bool ok = false;
                                if (base) {
                                    MEMORY_BASIC_INFORMATION mbi;
                                    if (VirtualQuery(base + cursor, &mbi, sizeof(mbi)) &&
                                        mbi.State == MEM_COMMIT &&
                                        !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
                                        v = *(uint32_t*)(base + cursor); ok = true;
                                    }
                                }
                                fprintf(stderr, "  +0x%02X:%s%08X", cand[c2], ok ? "" : "!", v);
                            }
                            fprintf(stderr, "\n");
                        }
                        for (int k = 0; k < 64; ++k) s_tracePrev[k] = cur[k];
                        s_traceHavePrev = true;
                    }
                }
                fprintf(stderr, "[CompEnt::Ser] ent=%p comp %lld -> %p tid=%08x (%s) pos8C=%u pos24=%u\n",
                        (void*)this, (long long)i, c, tid,
                        c ? (::Object::GetTypeNameById(tid) ? ::Object::GetTypeNameById(tid) : "?") : "null",
                        pos, pos24);
                fflush(stderr);
            }
            // Verify component is derived from Component base; null out if not
            void* comp = *(void**)ibpAddr;
            if (comp != nullptr) {
                if (fn_IsDerivedFrom && g_TypeId_ComponentBase) {
                    bool isDerived = fn_IsDerivedFrom(comp, *g_TypeId_ComponentBase);
                    if (!isDerived) {
                        *(void**)ibpAddr = nullptr;
                    }
                }
            }
        }
    }

    // Serialize entityFlags at +0x40
    ser.Serialize(*reinterpret_cast<int*>(&_entityFlags), nullptr);

    // On load: create missing components from description
    if (encodingMode == 0) {
        ComponentDescription* desc = _description;
        if (desc != nullptr) {
            // Description: +0x28 = component desc array (array of ptrs), +0x30 = count
            int descCompCount = desc->_subDescCount;
            int insertIdx = 0;
            int64_t insertPos = 0;

            for (int64_t descIdx = 0; descIdx < (int64_t)descCompCount; descIdx++) {
                void* compTemplate = desc->_subDescs[descIdx];

                // Call vtable+0x40 on component template to get type ID
                uint32_t typeId = TemplateComponentType(compTemplate);

                if (typeId == 0) {
                    continue;
                }

                // Check if existing component at insertPos already matches
                if (insertIdx < _componentCount) {
                    uintptr_t existIBPAddr = (uintptr_t)_components + insertPos * 8;
                    uintptr_t existComp = *(uintptr_t*)existIBPAddr;
                    // Get existing component's type ID via vtable+0x10
                    uint32_t existTypeId = reinterpret_cast<Object::Base*>(existComp)->Type();  // B.3.8 bridge
                    if (existTypeId == typeId) {
                        // Already have this component -- skip
                        insertIdx++;
                        insertPos++;
                        continue;
                    }
                }

                // Create new component from factory
                void* factoryPtr = g_pFactorySingleton ? *(void**)*g_pFactorySingleton : nullptr;
                void* newObj = nullptr;
                if (factoryPtr) {
                    newObj = reinterpret_cast<const ::Object::Factory*>(factoryPtr)->CreateObject(
                        typeId);   // B.3.3 bridge
                }

                // Verify it's a Component
                void* newComp = nullptr;
                if (newObj && fn_IsDerivedFrom && g_TypeId_ComponentBase) {
                    if (fn_IsDerivedFrom(newObj, *g_TypeId_ComponentBase)) {
                        newComp = newObj;
                    }
                }

                // Create IBP for new component
                uintptr_t newIBP = 0;
                if (fn_InternalBasePtr_Ctor) {
                    new ((void*)&newIBP) ::Object::InternalBasePtr(
                        reinterpret_cast<RtObject::Internal*>(newComp));   // B.3.3 bridge
                }

                // Get description template ptr for this component
                void* descTemplate = desc->_subDescs[descIdx];

                // Create IBP for owner (this entity)
                uintptr_t ownerIBP = 0;
                if (fn_InternalBasePtr_Ctor) {
                    new ((void*)&ownerIBP) ::Object::InternalBasePtr(
                        reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
                }

                // Set component fields: +0x10 = owner IBP, +0x18 = description template
                void* compPtr = (void*)newIBP;
                *(uintptr_t*)((uintptr_t)compPtr + 0x10) = ownerIBP;
                *(void**)((uintptr_t)compPtr + 0x18) = descTemplate;

                // Component's virtual slot 4 (+0x20): Startup
                compPtr = (void*)newIBP;
                reinterpret_cast<Component*>(compPtr)->Startup();  // B.3.8 bridge

                // Resize component array to make room
                int oldCount = _componentCount;
                // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8
                // align 8, new tail zeroed. Element is the component InternalBasePtr.
                GameColl::ArrayAt<GameColl::Ptr8>(s, 0x30).SetSize(oldCount + 1);

                // Shift existing components right to make room at insertPos
                uintptr_t compArr = (uintptr_t)_components;
                for (int64_t k = (int64_t)oldCount; k > insertPos; k--) {
                    *(uintptr_t*)(compArr + (uintptr_t)k * 8) =
                        *(uintptr_t*)(compArr + (uintptr_t)(k - 1) * 8);
                }

                // Insert new component IBP at insertPos
                *(uintptr_t*)(compArr + insertPos * 8) = newIBP;

                insertIdx++;
                insertPos++;
            }

            // Refresh entity flags after loading all components
            RefreshEntityFlags();
        }
    }
}

// The 0x1400c3720 door: fn_EntityFactory_CreateEntity resolves here by address
// (standalone/address_registry.cpp). Placement-constructs on the caller's block.
ComponentEntity* ComponentEntity_CtorFromDescription(ComponentEntity* self, void* descriptionPtr) {
    return new (static_cast<void*>(self))
        ComponentEntity(static_cast<ComponentDescription*>(descriptionPtr));   // B.3.8 bridge
}

// ============================================================
// EntityList registration (FUN_1400c48a0 + helpers) — Foundation B
// ============================================================
// In standalone these exe primitives resolve to NULL until registered, so they
// are reimplemented here in clean C++ and registered at their Ghidra addresses
// (address_registry.cpp). Used by the start-town ECS spawn.

// FUN_1400c33e0 — register an entity in its owner's update/draw buckets when the
// update(bit2)/draw(bit4) capability changed vs oldFlags. This is the same tail
// logic as RefreshEntityFlags but with an explicit oldFlags (here always 0, so a
// freshly-added entity with update/draw components gets bucketed).
//   EntityList +0x88 = update array (count +0x90), +0x98 = draw array (count +0xa0)
void ComponentEntity::RegisterUpdateDraw(uint32_t oldFlags) {
    if (_entityList == nullptr) return;
    uintptr_t el = (uintptr_t)_entityList;
    uint32_t changed = oldFlags ^ _entityFlags;
    if ((changed & 4) != 0) {
        int updateCount = *(int*)(el + 0x90) + 1;
        uintptr_t ibp = 0;
        if (fn_InternalBasePtr_Ctor) new ((void*)&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
        // B.3.2: Collection::Array<T>::SetSize(n, fill) — retail 0x140003110,
        // stride 8 align 8. Element is the 8-byte entity InternalBasePtr.
        GameColl::ArrayAt<GameColl::Ptr8>(el, 0x88).SetSize(
            updateCount, *reinterpret_cast<const GameColl::Ptr8*>(&ibp));
    }
    if ((changed & 0x10) != 0) {
        int drawCount = *(int*)(el + 0xa0) + 1;
        uintptr_t ibp = 0;
        if (fn_InternalBasePtr_Ctor) new ((void*)&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
        // B.3.2: Collection::Array<T>::SetSize(n, fill) — retail 0x140003110,
        // stride 8 align 8. Element is the 8-byte entity InternalBasePtr.
        GameColl::ArrayAt<GameColl::Ptr8>(el, 0x98).SetSize(
            drawCount, *reinterpret_cast<const GameColl::Ptr8*>(&ibp));
    }
}

// FUN_1400c2f60 — "OnAdded" lifecycle pass. If entityFlags bit 0 is set, call
// each component's vtable+0x30 (register-with-systems) for components whose
// Component+0x20 bit 0 is set.
void ComponentEntity::OnAddedPass() {

    if ((_entityFlags & 1) == 0) return;
    int count = _componentCount;
    uintptr_t arr = (uintptr_t)_components;
    for (int i = 0; i < count; i++) {
        uintptr_t ibpAddr = arr + (uintptr_t)i * 8;
        uintptr_t compPtr = *(uintptr_t*)ibpAddr;
        if (!compPtr) continue;   // defensive: skip null component (standalone factory miss)
        const bool armed = (*(uint8_t*)(compPtr + 0x20) & 1) != 0;
        if (armed) {
            compPtr = *(uintptr_t*)ibpAddr;
            reinterpret_cast<Component*>(compPtr)->OnAdded();  // B.3.8 bridge (slot 6, +0x30)
        }
    }
}

// FUN_1400c48a0 — EntityList::RegisterEntity(entityList, &entityIBP, category).
// Adds a freshly-instantiated entity to the world entity list. Offsets verified
// against the disassembly:
//   entityList+0xE0 (byte) : "closed" flag — if set, drop (delete) the entity.
//   entityList+0x08        : main entity array header {ptr@+0x08,count@+0x10,cap@+0x14}.
//                            entity id = the pre-insert main count.
//   entityList[0]          : copied to entity+0x20 (externals).
//   per-category C: bucket header at entityList+0x18+C*0x10 (count at +0x20+C*0x10).
//   entity+0x1C low-24     = bucket index (pre-insert bucket count); +0x1F = category.
//   category 6 = no bucket (entity+0x1C low-24 set to 0xFFFFFF sentinel).
// Also registers update/draw (c33e0) and runs the OnAdded pass (c2f60).
void EntityList_RegisterEntity_Impl(void* entityListV, void* entityIBP, int category) {
    uint8_t* el = (uint8_t*)entityListV;
    if (!el || !entityIBP) return;

    if (*(int8_t*)(el + 0xE0) != 0) {                  // list closed → drop the entity
        if (fn_InternalBasePtr_DeletePtr) reinterpret_cast< ::Object::InternalBasePtr*>(entityIBP)->DeletePtr(
            );   // B.3.3 bridge
        return;
    }

    // Resolve the entity pointer (CheckPtr each time, matching the original).
    auto entity = [&]() -> uint8_t* {
        return *(uint8_t**)entityIBP;
    };

    uint8_t* ent = entity();
    *(void**)(ent + 0x10) = el;                         // owner = entityList
    int32_t id = *(int32_t*)(el + 0x10);               // id = current main-array count
    ent = entity();
    *(int32_t*)(ent + 0x18) = id;
    ent = entity();
    *(void**)(ent + 0x20) = *(void**)el;               // externals = entityList[0]
    ent = entity();
    reinterpret_cast<ComponentEntity*>(ent)->RegisterUpdateDraw(0);   // B.3.8 bridge

    // Append to the main entity array (count -> id+1).
    // B.3.2: Collection::Array<T>::SetSize(n, fill) — retail 0x140003110,
    // stride 8 align 8. Element is the 8-byte entity InternalBasePtr.
    GameColl::ArrayAt<GameColl::Ptr8>(el, 0x08).SetSize(
        id + 1, *reinterpret_cast<const GameColl::Ptr8*>(entityIBP));

    ent = entity();
    *(int8_t*)(ent + 0x1F) = (int8_t)category;          // category byte (handle high byte)
    if (category == 6) {
        ent = entity();
        *(uint32_t*)(ent + 0x1C) |= 0xFFFFFF;           // no category bucket
    } else {
        // bucket index = current per-category bucket count (count field aliases
        // the +(C+2)*0x10 slot read by the original).
        uint32_t bucketIdx = *(uint32_t*)(el + (uintptr_t)(category + 2) * 0x10);
        ent = entity();
        uint32_t* pf = (uint32_t*)(ent + 0x1C);
        *pf ^= (*pf ^ bucketIdx) & 0xFFFFFF;            // low-24 = bucket index
        uint8_t* bucket = el + (uintptr_t)category * 0x10 + 0x18;
        int bcount = *(int32_t*)(bucket + 8);
        // B.3.2: Collection::Array<T>::SetSize(n, fill) — retail 0x140003110,
        // stride 8 align 8. Element is the 8-byte entity InternalBasePtr.
        GameColl::AsArray<GameColl::Ptr8>(bucket).SetSize(
            bcount + 1, *reinterpret_cast<const GameColl::Ptr8*>(entityIBP));
    }

    ent = entity();
    reinterpret_cast<ComponentEntity*>(ent)->OnAddedPass();   // B.3.8 bridge

}

// ============================================================
// Reflection (factory cascade — concrete landmine removal)
// ============================================================
// Exe factory ComponentEntity_Factory (0x1400219c0): MemAlloc(0x48,8) ->
// ComponentEntity_Ctor (0x1400c3620) == the default ctor above. Registered as
// the real factory so Object::Factory::CreateObject("ComponentEntity") /
// External::Load no longer hits the NullFactory -> IsDerivedFrom(NULL) crash.
void* ComponentEntity::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ComponentEntity), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ComponentEntity());
}
// Retail's in-place create is the Runtime's `Internal::CreateInPlaceObject`;
// the tree has always run the full ctor here (kept, no behaviour change).
void* ComponentEntity::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ComponentEntity());
}

DEFINE_EXE_VTABLE(ComponentEntity, "_ZTV15ComponentEntity")
DEFINE_EXE_TYPE(ComponentEntity, "Object::Internal")
