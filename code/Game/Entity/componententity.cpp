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
#include "Entity/EntityList.h"
#include "Component/ComponentDescription.h"
#include "Component/Component.h"
#include "Component/Description.h"
#include "Component/coll_bridge.h"
#include "options.h"
#include <windows.h>
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "Object/factory_registration.h"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/TypeInfo.h"   // ENDGAME B.3.3 step 6C
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>
#include "Entity/exe_bodies.h"
#include "Game/exe_bodies.h"   // Reimpl_ExternalPtr_Serialize_StringTable

// TownManager::AbortCitizenActionsFor (exe 0x140089e30) - ported in
// code/Game/Component/destroycomponent_state6.cpp.
extern "C" void TownMgr_AbortCitizenActionsFor_89e30(void* townMgr, ComponentEntity** pEntityIbp);

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
    GameColl::AsArray<GameColl::Ptr8>(&_components).SetSize(validCount);

    // Second pass: create components
    int insertIdx = 0;
    for (int64_t i = 0; i < (int64_t)descCompCount; i++) {
        // LEFT: ComponentDescription::_subDescs is void** (COMP-c's header, running).
        Description* compTemplate = static_cast<Description*>(description->_subDescs[i]);
        uint32_t typeId = TemplateComponentType(compTemplate);
        if (typeId == 0) continue;

        // Create component from factory
        const ::Object::Factory* factory = ::Object::Factory::_singleton;
        ::Object::Base* newObj = nullptr;
        if (factory) {
            newObj = factory->CreateObject(typeId);
        }

        // Verify it derives from Component. In standalone the Runtime factory
        // tree is incomplete, so raw fn_IsDerivedFrom rejects valid inert
        // components (returns false / NULL-derefs) — trust a non-null
        // CreateObject result instead (Object::IsDerivedFromOrTrust). This path
        // only began executing once the description +0x40 GetComponentTypeId
        // was implemented; before that typeId was always 0 and no component was
        // ever created, masking the missing CheckPtr below.
        Component* newComp = nullptr;
        if (newObj && Object::IsDerivedFromOrTrust(
                          newObj, Component::TypeId())) {   // cell 0x1401c72a8 (ENDGAME B.3.9 RF)
            newComp = static_cast<Component*>(newObj);
        }

        // Create IBP for new component (the owner IBP is `this`)
        ::Object::InternalPtr<Component> newIBP(newComp);

        // Resolve the component IBP (the original CopyCtor calls CheckPtr
        // before the field writes) then set owner (+0x10) and description
        // (+0x18).
        Component* compPtr = newIBP.Get();
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
        compPtr->_entity = this;
        compPtr->_description = compTemplate;

        // Store in component array
        _components[insertIdx] = newIBP;
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
    if (GameUpdateLoop::_singleton != nullptr) {
        void* townMgr = GameUpdateLoop::_singleton->m_citizenList;
        if (townMgr) {
            ComponentEntity* entityIbp = this;   // the InternalBasePtr onto the dying entity
            TownMgr_AbortCitizenActionsFor_89e30(townMgr, &entityIbp);
        }
    }

    // Iterate component IBPs: null each out, then delete old value
    int count = _componentCount;
    if (count > 0) {
        for (int i = 0; i < count; i++) {
            // Save old IBP value
            ::Object::InternalPtr<Component> oldIBP = _components[i];
            // Construct null IBP in place
            _components[i] = ::Object::InternalPtr<Component>();
            // Delete old IBP (releases the component)
            oldIBP.DeletePtr();
        }
    }

    // Free component array memory
    if (_components) {
        {
            System::MemoryAllocator::MemFree(_components);
        }
    }
    _components = nullptr;
    _componentCount = 0;
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

    for (int i = 0; i < count; i++) {
        Component* compPtr = _components[i].Get();
        // Component+0x20 is uint32 containing capability flags
        uint32_t compFlags = compPtr->_flags;
        if (((compFlags >> 8) & 1) != 0) {
            // Re-check after potential lazy resolve
            compPtr = _components[i].Get();
            // Virtual slot 5 (+0x28, the shutdown slot)
            compPtr->Slot5();
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

    for (int i = 0; i < count; i++) {
        Component* compPtr = _components[i].Get();
        if (!compPtr) continue;   // defensive: skip null component (standalone factory miss)
        uint32_t compFlags = compPtr->_flags;
        if (((compFlags >> 7) & 1) != 0) {
            compPtr = _components[i].Get();
            // Virtual slot 4 (+0x20): Startup
            compPtr->Startup();
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
        for (int i = 0; i < count; i++) {
            Component* compPtr = _components[i].Get();
            if (!compPtr) continue;   // defensive: skip null component (standalone factory miss)
            // Retail reads the low WORD of Component::_flags (+0x20)
            uint16_t compFlags = static_cast<uint16_t>(compPtr->_flags);
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
            GameColl::Ptr8 ibp;
            ibp.v = this;   // the InternalBasePtr onto this entity
            // B.3.2: Collection::Array<T>::SetSize(n, fill) — retail 0x140003110,
            // stride 8 align 8. Element is the 8-byte entity InternalBasePtr.
            GameColl::ArrayAt<GameColl::Ptr8>(entityListPtr, 0x88).SetSize(updateCount, ibp);
        }

        // If draw bit (bit 4) changed, register in draw bucket
        if (((oldFlags ^ _entityFlags) & 0x10) != 0) {
            // EntityList +0x98 = draw entity array, +0xa0 = draw count
            int drawCount = *(int*)(entityListPtr + 0xa0) + 1;
            GameColl::Ptr8 ibp;
            ibp.v = this;   // the InternalBasePtr onto this entity
            // B.3.2: Collection::Array<T>::SetSize(n, fill) — retail 0x140003110,
            // stride 8 align 8. Element is the 8-byte entity InternalBasePtr.
            GameColl::ArrayAt<GameColl::Ptr8>(entityListPtr, 0x98).SetSize(drawCount, ibp);
        }
    }
}

// ============================================================
// [3] Persist (0x1400c3140)
// ============================================================
// Serializes entity state: description external, component count,
// component IBPs, entity flags. On load, creates components from
// factory and inserts them into the component array.
void ComponentEntity::Persist(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (stream is void* in Internal::Persist)

    // Serialize description ExternalPtr at +0x28
    Reimpl_ExternalPtr_Serialize_StringTable(&_description, &ser);

    // Serialize component count
    int compCount = _componentCount;
    ser.Serialize(compCount, nullptr);

    // Check if loading (encoding mode == 0)
    int encodingMode = static_cast<int>(ser.GetDirection());   // IO::Serializer::GetDirection, retail 0x180006210

    // On load: resize component array to match serialized count
    if (encodingMode == 0) {
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
        // new tail zeroed. Element is the 8-byte component InternalBasePtr (byte-copy).
        GameColl::AsArray<GameColl::Ptr8>(&_components).SetSize(compCount);
        // Save/load wave diagnostics: a garbage count (stream misalignment) or
        // a failed resize (data still null with count>0) crashes the IBP loop
        // below — log the state on the way in so the crash log has context.
        if (s_loadLogCount.load() < 8 || _components == nullptr || static_cast<uint32_t>(compCount) > 64) {
            s_loadLogCount.fetch_add(1);
            fprintf(stderr, "[CompEnt::Serialize] LOAD ent=%p desc=%p compCount=%u "
                    "arr=%p cap=%d\n", (void*)this, (void*)_description, static_cast<uint32_t>(compCount),
                    _components, _componentCap);
            fflush(stderr);
        }
    }

    // BANISHED_COMP_VALIDATE=1 — on the SAVE leg, check every component slot
    // BEFORE anything is written. A non-null slot that is not a live object is
    // what puts 0xAA-pattern objId/typeId into the stream and makes the LOAD
    // index PersistObject's id map out of bounds. Diagnostic only.
    if (encodingMode != 0)
        EntityCompValidate(this, _description, _components, compCount);

    // Save/load wave debug (BANISHED_SERIALIZE_TRACE): per-component progress
    // marker so a stream desync pinpoints the last-good component instead of a
    // wild crash address. Read per call (B6-5: no lazy cache).
    const char* traceOpt = Opt().serializeTrace;
    const bool trace = traceOpt && traceOpt[0] && traceOpt[0] != '0';

    // Serialize each component IBP
    int64_t cnt = (int64_t)compCount;
    if (cnt > 0) {
        for (int64_t i = 0; i < cnt; i++) {
            Object::InternalPtr<Component>* slot = &_components[i];
            if (trace) {
                fprintf(stderr, "[CompEnt::Ser] ent=%p comp %lld/%u begin raw=%p\n",
                        (void*)this, (long long)i, static_cast<uint32_t>(compCount),
                        static_cast<void*>(slot->Get()));
                fflush(stderr);
            }
            // Serialize the InternalBasePtr
            slot->Object::InternalBasePtr::Serialize(ser);   // the base body: the Component check follows by hand
            Component* rawAfterSer = slot->Get();   // before our IsDerivedFrom filter
            if (trace && !rawAfterSer) {
                fprintf(stderr, "[CompEnt::Ser] ent=%p comp %lld RAW NULL from "
                        "IBP::Serialize (create failed -> stream desync!)\n",
                        (void*)this, (long long)i);
                fflush(stderr);
            }
            if (trace) {
                Component* c = slot->Get();
                uint32_t tid = 0;
                if (c) {
                    tid = c->Type();
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
                        (void*)this, (long long)i, static_cast<void*>(c), tid,
                        c ? (::Object::GetTypeNameById(tid) ? ::Object::GetTypeNameById(tid) : "?") : "null",
                        pos, pos24);
                fflush(stderr);
            }
            // Verify component is derived from Component base; null out if not
            Component* comp = slot->Get();
            if (comp != nullptr) {
                {
                    bool isDerived = ::Object::IsDerivedFromNullSafe(comp, Component::TypeId());   // cell 0x1401c72a8
                    if (!isDerived) {
                        *slot = Object::InternalPtr<Component>();
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
                    Component* existComp = _components[insertPos].Get();
                    // Get existing component's type ID via vtable+0x10
                    uint32_t existTypeId = existComp->Type();
                    if (existTypeId == typeId) {
                        // Already have this component -- skip
                        insertIdx++;
                        insertPos++;
                        continue;
                    }
                }

                // Create new component from factory
                const ::Object::Factory* factory = ::Object::Factory::_singleton;
                ::Object::Base* newObj = nullptr;
                if (factory) {
                    newObj = factory->CreateObject(typeId);
                }

                // Verify it's a Component
                Component* newComp = nullptr;
                if (newObj) {
                    if (::Object::IsDerivedFromNullSafe(newObj, Component::TypeId())) {   // cell 0x1401c72a8
                        newComp = static_cast<Component*>(newObj);
                    }
                }

                // Create IBP for new component
                ::Object::InternalPtr<Component> newIBP(newComp);

                // Get description template ptr for this component
                // LEFT: ComponentDescription::_subDescs is void** (COMP-c's header, running).
                Description* descTemplate = static_cast<Description*>(desc->_subDescs[descIdx]);

                // Set component fields: +0x10 = owner IBP (this entity), +0x18 = description template
                Component* compPtr = newIBP.Get();
                compPtr->_entity = this;
                compPtr->_description = descTemplate;

                // Component's virtual slot 4 (+0x20): Startup
                newIBP.Get()->Startup();

                // Resize component array to make room
                int oldCount = _componentCount;
                // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8
                // align 8, new tail zeroed. Element is the component InternalBasePtr.
                GameColl::AsArray<GameColl::Ptr8>(&_components).SetSize(oldCount + 1);

                // Shift existing components right to make room at insertPos
                Object::InternalPtr<Component>* compArr = _components;
                for (int64_t k = (int64_t)oldCount; k > insertPos; k--) {
                    compArr[k] = compArr[k - 1];
                }

                // Insert new component IBP at insertPos
                compArr[insertPos] = newIBP;

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
        GameColl::Ptr8 ibp;
        ibp.v = this;   // the InternalBasePtr onto this entity
        // B.3.2: Collection::Array<T>::SetSize(n, fill) — retail 0x140003110,
        // stride 8 align 8. Element is the 8-byte entity InternalBasePtr.
        GameColl::ArrayAt<GameColl::Ptr8>(el, 0x88).SetSize(updateCount, ibp);
    }
    if ((changed & 0x10) != 0) {
        int drawCount = *(int*)(el + 0xa0) + 1;
        GameColl::Ptr8 ibp;
        ibp.v = this;   // the InternalBasePtr onto this entity
        // B.3.2: Collection::Array<T>::SetSize(n, fill) — retail 0x140003110,
        // stride 8 align 8. Element is the 8-byte entity InternalBasePtr.
        GameColl::ArrayAt<GameColl::Ptr8>(el, 0x98).SetSize(drawCount, ibp);
    }
}

// FUN_1400c2f60 — "OnAdded" lifecycle pass. If entityFlags bit 0 is set, call
// each component's vtable+0x30 (register-with-systems) for components whose
// Component+0x20 bit 0 is set.
void ComponentEntity::OnAddedPass() {

    if ((_entityFlags & 1) == 0) return;
    int count = _componentCount;
    for (int i = 0; i < count; i++) {
        Component* compPtr = _components[i].Get();
        if (!compPtr) continue;   // defensive: skip null component (standalone factory miss)
        const bool armed = (compPtr->_flags & 1) != 0;   // retail tests the low BYTE
        if (armed) {
            compPtr = _components[i].Get();
            compPtr->OnAdded();   // slot 6, +0x30
        }
    }
}

// EntityList::RegisterEntity (0x1400c48a0) (entityList, &entityIBP, category).
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
void EntityList::RegisterEntity(ComponentEntity** entityIBP, int category) {
    if (shuttingDown) {                                 // list closed -> drop the entity
        // Object::InternalBasePtr::DeletePtr (0x1800c0150) on the slot: delete
        // through the virtual dtor, null the slot.
        if (ComponentEntity* dying = *entityIBP) {
            delete dying;
            *entityIBP = nullptr;
        }
        return;
    }

    // Every step re-reads the slot (CheckPtr each time, matching the original).
    (*entityIBP)->_entityList = this;                   // owner = entityList
    int32_t id = masterArray.count;                     // id = current main-array count
    (*entityIBP)->_entityId = id;
    // entity+0x20 = entityList+0x00: the last-update System::Time takes the list's
    // accumulated time. ComponentEntity.h still declares +0x20 `void* _externals`:
    // LEFT for the header's owner.
    *(int64_t*)&(*entityIBP)->_externals = accumulatedTime._ticks;
    (*entityIBP)->RegisterUpdateDraw(0);

    // Append to the main entity array (count -> id+1).
    // B.3.2: Collection::Array<T>::SetSize(n, fill) — retail 0x140003110,
    // stride 8 align 8. Element is the 8-byte entity InternalBasePtr.
    GameColl::Ptr8 fill;
    fill.v = *entityIBP;
    GameColl::AsArray<GameColl::Ptr8>(&masterArray).SetSize(id + 1, fill);

    // The category byte +0x1F (the handle's high byte): retail stores the BYTE.
    // ComponentEntity.h declares +0x1C..+0x1F as one uint32 `_handle`: LEFT.
    reinterpret_cast<int8_t*>(&(*entityIBP)->_handle)[3] = (int8_t)category;
    if (category == 6) {
        (*entityIBP)->_handle |= 0xFFFFFF;               // no category bucket
    } else {
        // bucket index = the category bucket's current count
        uint32_t bucketIdx = static_cast<uint32_t>(buckets[category].count);
        uint32_t& handle = (*entityIBP)->_handle;
        handle ^= (handle ^ bucketIdx) & 0xFFFFFF;       // low-24 = bucket index
        UpdateBucket* bucket = &buckets[category];
        int bcount = bucket->count;
        // B.3.2: Collection::Array<T>::SetSize(n, fill) — retail 0x140003110,
        // stride 8 align 8. Element is the 8-byte entity InternalBasePtr.
        GameColl::Ptr8 bfill;
        bfill.v = *entityIBP;
        GameColl::AsArray<GameColl::Ptr8>(bucket).SetSize(bcount + 1, bfill);
    }

    (*entityIBP)->OnAddedPass();
}

// B4 FORWARD: 38 outside caller(s) (code/Game/Action, Component, Game, Work,
// standalone; componententity_funcs.h). The null guards are the tree's (retail's
// 0x1400c48a0 has none); kept here, not extended into the member.
void EntityList_RegisterEntity_Impl(void* entityListV, void* entityIBP, int category) {
    if (!entityListV || !entityIBP) return;
    reinterpret_cast<EntityList*>(entityListV)->RegisterEntity(reinterpret_cast<ComponentEntity**>(entityIBP), category);
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

DEFINE_EXE_TYPE(ComponentEntity, "Object::Internal")
