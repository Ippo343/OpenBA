// code/Game/Entity/componententity.cpp
// Clean reimplementation of ComponentEntity vtable and non-vtable methods.
//
// ComponentEntity (0x48 bytes) -- The ECS entity container.
// Vtable: 0x14015fa78, 12 slots.
// Extends Object::Internal.
//
// Each entity holds:
//   - An entity ID (unique identifier)
//   - Flags (24-bit mask)
//   - A description ExternalPtr (entity template)
//   - An array of Component InternalBasePtrs
//   - Aggregated entityFlags from components (update/draw/startup/shutdown bits)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Entity/componententity_funcs.h"
#include <windows.h>
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// TownManager::AbortCitizenActionsFor (exe 0x140089e30) - ported in
// code/Game/Component/destroycomponent_state6.cpp.
extern "C" void TownMgr_AbortCitizenActionsFor_89e30(void* townMgr, void** pEntityIbp);

// ============================================================
// Resolved addresses (lazy-init)
// ============================================================
static void* s_vtable_ComponentEntity = nullptr;       // 0x14015fa78
static void* s_vtable_ComponentEntity_Ext = nullptr;    // 0x14015faa0 (external/secondary vtable)
static bool s_resolved = false;

static void EnsureResolved() {
    if (s_resolved) return;
    s_vtable_ComponentEntity = (void*)ResolveAddr(Module::Exe, ComponentEntityAddr::Vtable);
    s_vtable_ComponentEntity_Ext = (void*)ResolveAddr(Module::Exe, 0x14015faa0);
    s_resolved = true;
}

// ============================================================
// Internal helper: destructor body (0x1400c3550)
// ============================================================
// Resets vtable, calls ShutdownComponents, clears component IBPs,
// frees component array, calls Object::Internal::~Internal.
// Called from Dtor (Slot0). Named "Ctor" in Ghidra (misleading).
extern "C" void WorkSerProbe_NoteEntityDtor(void* entity);  // standalone/work_ser_probe.cpp
extern "C" int  EntityCompValidate(void*, void*, void*, int);  // standalone/entity_comp_validator.cpp

static void DtorBody(ComponentEntity* self) {
    WorkSerProbe_NoteEntityDtor(self);
    // Set vtable back (matches original, important for correct virtual dispatch during teardown)
    *(void**)self = s_vtable_ComponentEntity;

    // Shutdown flagged components
    ComponentEntity_ShutdownComponents(self);

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
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&entityIbp, self);
            else                         entityIbp = self;
            TownMgr_AbortCitizenActionsFor_89e30(reinterpret_cast<void*>(townMgr),
                                                 &entityIbp);
        }
    }

    // Iterate component IBPs: null each out, then delete old value
    int count = self->componentCount;
    if (count > 0) {
        uintptr_t arr = (uintptr_t)self->components;
        for (int i = 0; i < count; i++) {
            uintptr_t ibpAddr = arr + (uintptr_t)i * 8;
            // Save old IBP value
            uintptr_t oldIBP = *(uintptr_t*)ibpAddr;
            // Construct null IBP in place
            if (fn_InternalBasePtr_Ctor) {
                fn_InternalBasePtr_Ctor((void*)ibpAddr, nullptr);
            }
            // Delete old IBP (releases the component)
            if (fn_InternalBasePtr_DeletePtr) {
                fn_InternalBasePtr_DeletePtr((void*)&oldIBP);
            }
        }
    }

    // Free component array memory
    if (self->components) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self->components);
        }
    }
    self->components = nullptr;
    self->componentCount = 0;

    // Call Object::Internal::~Internal
    if (fn_ObjectInternal_Dtor) {
        fn_ObjectInternal_Dtor(self);
    }
}

// ============================================================
// ShutdownComponents (0x1400c3000)
// ============================================================
// If entityFlags bit 8 (0x100) is set, iterates components and calls
// vtable+0x28 (Shutdown) on each component whose Component+0x20 bit 8 is set.
void ComponentEntity_ShutdownComponents(ComponentEntity* self) {
    if ((self->entityFlags & 0x100) == 0) return;

    int count = self->componentCount;
    if (count <= 0) return;

    uintptr_t arr = (uintptr_t)self->components;
    for (int i = 0; i < count; i++) {
        uintptr_t ibpAddr = arr + (uintptr_t)i * 8;

        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr((void*)ibpAddr);
        }

        uintptr_t compPtr = *(uintptr_t*)ibpAddr;
        // Component+0x20 is uint32 containing capability flags
        uint32_t compFlags = *(uint32_t*)(compPtr + 0x20);
        if (((compFlags >> 8) & 1) != 0) {
            // Re-check after potential lazy resolve
            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr((void*)ibpAddr);
            }
            compPtr = *(uintptr_t*)ibpAddr;
            // Call vtable+0x28 (Shutdown)
            uintptr_t compVtable = *(uintptr_t*)compPtr;
            typedef void (*fn_CompShutdown_t)(void* self);
            fn_CompShutdown_t shutdown = *(fn_CompShutdown_t*)(compVtable + 0x28);
            shutdown((void*)compPtr);
        }
    }
}

// ============================================================
// StartupComponents (0x1400c30a0)
// ============================================================
// If entityFlags bit 7 (0x80) is set, iterates components and calls
// vtable+0x20 (Startup) on each component whose Component+0x20 bit 7 is set.
void ComponentEntity_StartupComponents(ComponentEntity* self) {
    if ((self->entityFlags & 0x80) == 0) return;

    int count = self->componentCount;
    if (count <= 0) return;

    uintptr_t arr = (uintptr_t)self->components;
    for (int i = 0; i < count; i++) {
        uintptr_t ibpAddr = arr + (uintptr_t)i * 8;

        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr((void*)ibpAddr);
        }

        uintptr_t compPtr = *(uintptr_t*)ibpAddr;
        if (!compPtr) continue;   // defensive: skip null component (standalone factory miss)
        uint32_t compFlags = *(uint32_t*)(compPtr + 0x20);
        if (((compFlags >> 7) & 1) != 0) {
            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr((void*)ibpAddr);
            }
            compPtr = *(uintptr_t*)ibpAddr;
            // Call vtable+0x20 (Startup)
            uintptr_t compVtable = *(uintptr_t*)compPtr;
            typedef void (*fn_CompStartup_t)(void* self);
            fn_CompStartup_t startup = *(fn_CompStartup_t*)(compVtable + 0x20);
            startup((void*)compPtr);
        }
    }
}

// ============================================================
// RefreshEntityFlags (0x1400c3470)
// ============================================================
// Recomputes entityFlags by ORing in Component::+0x20 (ushort) for each
// component. If update bit (2) or draw bit (4) changed, registers entity
// in the appropriate EntityList bucket.
void ComponentEntity_RefreshEntityFlags(ComponentEntity* self) {
    EnsureResolved();
    uint32_t oldFlags = self->entityFlags;

    // Reset entityFlags, then OR in each component's flags
    self->entityFlags = 0;
    int count = self->componentCount;
    if (count > 0) {
        uintptr_t arr = (uintptr_t)self->components;
        for (int i = 0; i < count; i++) {
            uintptr_t ibpAddr = arr + (uintptr_t)i * 8;
            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr((void*)ibpAddr);
            }
            uintptr_t compPtr = *(uintptr_t*)ibpAddr;
            if (!compPtr) continue;   // defensive: skip null component (standalone factory miss)
            // Component::+0x20 is a uint16 containing capability flags
            uint16_t compFlags = *(uint16_t*)(compPtr + 0x20);
            self->entityFlags |= (uint32_t)compFlags;
        }
    }

    // If entityList is set (+0x10), check for flag changes that need registration
    if (self->entityList != nullptr) {
        uintptr_t entityListPtr = (uintptr_t)self->entityList;

        // If update bit (bit 2) changed, register in update bucket
        if (((oldFlags ^ self->entityFlags) & 4) != 0) {
            // EntityList +0x88 = update entity array, +0x90 = update count
            int updateCount = *(int*)(entityListPtr + 0x90) + 1;
            uintptr_t ibp = 0;
            if (fn_InternalBasePtr_Ctor) {
                fn_InternalBasePtr_Ctor((void*)&ibp, self);
            }
            if (fn_ExeArrayResizeFill) {
                fn_ExeArrayResizeFill((void*)(entityListPtr + 0x88), updateCount, (void*)&ibp);
            }
        }

        // If draw bit (bit 4) changed, register in draw bucket
        if (((oldFlags ^ self->entityFlags) & 0x10) != 0) {
            // EntityList +0x98 = draw entity array, +0xa0 = draw count
            int drawCount = *(int*)(entityListPtr + 0xa0) + 1;
            uintptr_t ibp = 0;
            if (fn_InternalBasePtr_Ctor) {
                fn_InternalBasePtr_Ctor((void*)&ibp, self);
            }
            if (fn_ExeArrayResizeFill) {
                fn_ExeArrayResizeFill((void*)(entityListPtr + 0x98), drawCount, (void*)&ibp);
            }
        }
    }
}

// ============================================================
// Slot0: Dtor (0x140021a00) -- Scalar deleting destructor
// ============================================================
void* ComponentEntity_Dtor(ComponentEntity* self, uint32_t flags) {
    EnsureResolved();

    // Set vtable back to ComponentEntity (for correct dispatch during teardown)
    *(void**)self = s_vtable_ComponentEntity;

    // Run destructor body
    DtorBody(self);

    // Free memory if bit 0 set (scalar delete)
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// Slot3: Serialize (0x1400c3140)
// ============================================================
// Serializes entity state: description external, component count,
// component IBPs, entity flags. On load, creates components from
// factory and inserts them into the component array.
//
// Implemented here for the standalone exe.
void ComponentEntity_Serialize(ComponentEntity* self, void* stream) {
    EnsureResolved();
    uintptr_t s = (uintptr_t)self;

    // Create Serializer on stack (24 bytes)
    uint8_t serializer[24];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Serialize description ExternalPtr at +0x28
    if (fn_ExternalPtr_BuildSerialize) {
        fn_ExternalPtr_BuildSerialize((void*)(s + 0x28), serializer);
    }

    // Serialize component count
    uint32_t compCount = (uint32_t)self->componentCount;
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(serializer, (void*)&compCount, nullptr);
    }

    // Check if loading (encoding mode == 0)
    uintptr_t serInner = *(uintptr_t*)serializer;
    int encodingMode = *(int*)(serInner + 0x08);

    // On load: resize component array to match serialized count
    if (encodingMode == 0) {
        fn_ExeArrayResize((void*)(s + 0x30), (int)compCount);
        // Save/load wave diagnostics: a garbage count (stream misalignment) or
        // a failed resize (data still null with count>0) crashes the IBP loop
        // below — log the state on the way in so the crash log has context.
        static int s_dbgN = 0;
        if (s_dbgN < 8 || *(void**)(s + 0x30) == nullptr || compCount > 64) {
            s_dbgN++;
            fprintf(stderr, "[CompEnt::Serialize] LOAD ent=%p desc=%p compCount=%u "
                    "arr=%p cap=%d\n", (void*)self, self->description, compCount,
                    *(void**)(s + 0x30), *(int*)(s + 0x3c));
            fflush(stderr);
        }
    }

    // BANISHED_COMP_VALIDATE=1 — on the SAVE leg, check every component slot
    // BEFORE anything is written. A non-null slot that is not a live object is
    // what puts 0xAA-pattern objId/typeId into the stream and makes the LOAD
    // index PersistObject's id map out of bounds. Diagnostic only.
    if (encodingMode != 0)
        EntityCompValidate(self, self->description, self->components, (int)compCount);

    // Serialize each component IBP
    uintptr_t ibpArr = (uintptr_t)self->components;
    int64_t cnt = (int64_t)(int)compCount;
    if (cnt > 0) {
        for (int64_t i = 0; i < cnt; i++) {
            uintptr_t ibpAddr = ibpArr + (uintptr_t)i * 8;
            // BANISHED_SER_DUMPCOMP=<n> -- destructive one-shot: before component n
            // is read, pull the next 130 dwords straight off the stream and print
            // them, then stop. The stream's +0x1C cursor is a LOGICAL decompressed
            // position, not an index into any buffer in the header (all five
            // candidate bases were checked and none holds the record), so the only
            // way to see the actual bytes is to read them through the serializer.
            // With the objIds running in sequence, finding the next one in this dump
            // gives the record's true length -- i.e. exactly how many bytes our
            // reader is short by -- instead of inferring it from where things crash.
            {
                static int s_dumpAt = -2;
                if (s_dumpAt == -2) {
                    const char* e = std::getenv("BANISHED_SER_DUMPCOMP");
                    s_dumpAt = (e && e[0]) ? atoi(e) : -1;
                }
                if (s_dumpAt >= 0 && (int)i == s_dumpAt && g_fn_Serialize_Int) {
                    void* strm2 = *(void**)serializer;
                    fprintf(stderr, "[SerDump] comp %d cursor=%u — next 130 dwords:\n",
                            s_dumpAt, strm2 ? *(uint32_t*)((uint8_t*)strm2 + 0x1C) : 0);
                    for (int k = 0; k < 130; ++k) {
                        uint32_t v = 0;
                        g_fn_Serialize_Int(serializer, (void*)&v, nullptr);
                        fprintf(stderr, "[SerDump]   [%2d] %08X (%u)\n", k, v, v);
                    }
                    fflush(stderr);
                    fprintf(stderr, "[SerDump] done — exiting (stream is now consumed)\n");
                    fflush(stderr);
                    _exit(0);
                }
            }
            // Save/load wave debug (BANISHED_SERIALIZE_TRACE): per-component
            // progress marker so a stream desync pinpoints the last-good
            // component instead of a wild crash address.
            static int s_trace = -1;
            if (s_trace < 0) { const char* e = std::getenv("BANISHED_SERIALIZE_TRACE"); s_trace = (e && e[0] && e[0] != '0') ? 1 : 0; }
            if (s_trace == 1) {
                fprintf(stderr, "[CompEnt::Ser] ent=%p comp %lld/%u begin raw=%p\n",
                        (void*)self, (long long)i, compCount, *(void**)ibpAddr);
                fflush(stderr);
            }
            // Serialize the InternalBasePtr
            if (fn_InternalBasePtr_Serialize) {
                fn_InternalBasePtr_Serialize((void*)ibpAddr, serializer);
            }
            void* rawAfterSer = *(void**)ibpAddr;   // before our IsDerivedFrom filter
            if (s_trace == 1 && !rawAfterSer) {
                fprintf(stderr, "[CompEnt::Ser] ent=%p comp %lld RAW NULL from "
                        "IBP::Serialize (create failed -> stream desync!)\n",
                        (void*)self, (long long)i);
                fflush(stderr);
            }
            if (s_trace == 1) {
                void* c = *(void**)ibpAddr;
                uint32_t tid = 0;
                if (c) {
                    typedef uint32_t (*fn_gtid_t)(void*);
                    tid = (*(fn_gtid_t*)(*(uintptr_t*)c + 0x10))(c);
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
                    void* strm = serializer ? *(void**)serializer : nullptr;
                    if (strm) {
                        pos   = *(uint32_t*)((uint8_t*)strm + 0x8C);
                        pos24 = *(uint32_t*)((uint8_t*)strm + 0x24);
                        // Neither documented cursor moves for THIS stream type, so
                        // find the real one instead of guessing a third offset:
                        // snapshot the first 0x100 bytes and report which dwords
                        // changed since the previous component. The cursor is the
                        // one that advances monotonically by each component's size.
                        static uint32_t prev[64];
                        static bool haveprev = false;
                        uint32_t cur[64];
                        for (int k = 0; k < 64; ++k)
                            cur[k] = *(uint32_t*)((uint8_t*)strm + k * 4);
                        if (haveprev) {
                            char buf[512]; int n = 0;
                            for (int k = 0; k < 64 && n < 400; ++k)
                                if (cur[k] != prev[k])
                                    n += snprintf(buf + n, sizeof(buf) - n,
                                                  " +0x%02X:%u->%u", k * 4, prev[k], cur[k]);
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
                        static bool dumped = false;
                        if (!dumped) {
                            dumped = true;
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
                            static const int cand[5] = { 0x10, 0x28, 0x30, 0x40, 0x80 };
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
                        for (int k = 0; k < 64; ++k) prev[k] = cur[k];
                        haveprev = true;
                    }
                }
                fprintf(stderr, "[CompEnt::Ser] ent=%p comp %lld -> %p tid=%08x (%s) pos8C=%u pos24=%u\n",
                        (void*)self, (long long)i, c, tid,
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
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(serializer, (void*)(s + 0x40), nullptr);
    }

    // On load: create missing components from description
    if (encodingMode == 0) {
        void* desc = self->description;
        if (desc != nullptr) {
            // Description: +0x28 = component desc array (array of ptrs), +0x30 = count
            int descCompCount = *(int*)((uintptr_t)desc + 0x30);
            int insertIdx = 0;
            int64_t insertPos = 0;

            for (int64_t descIdx = 0; descIdx < (int64_t)descCompCount; descIdx++) {
                uintptr_t descArr = *(uintptr_t*)((uintptr_t)desc + 0x28);
                uintptr_t compTemplate = *(uintptr_t*)(descArr + (uintptr_t)descIdx * 8);

                // Call vtable+0x40 on component template to get type ID
                uintptr_t compVtable = *(uintptr_t*)compTemplate;
                typedef void (*fn_GetTypeId_t)(void* self, uint32_t* outTypeId);
                fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(compVtable + 0x40);
                uint32_t typeId = 0;
                getTypeId((void*)compTemplate, &typeId);

                if (typeId == 0) continue;

                // Check if existing component at insertPos already matches
                if (insertIdx < self->componentCount) {
                    uintptr_t existIBPAddr = (uintptr_t)self->components + insertPos * 8;
                    if (fn_InternalBasePtr_CheckPtr) {
                        fn_InternalBasePtr_CheckPtr((void*)existIBPAddr);
                    }
                    uintptr_t existComp = *(uintptr_t*)existIBPAddr;
                    // Get existing component's type ID via vtable+0x10
                    uintptr_t existVtable = *(uintptr_t*)existComp;
                    typedef uint32_t (*fn_GetId_t)(void*);
                    fn_GetId_t getId = *(fn_GetId_t*)(existVtable + 0x10);
                    uint32_t existTypeId = getId((void*)existComp);
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
                if (factoryPtr && fn_Factory_CreateObject) {
                    newObj = fn_Factory_CreateObject(factoryPtr, typeId);
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
                    fn_InternalBasePtr_Ctor((void*)&newIBP, newComp);
                }

                // Get description template ptr for this component
                uintptr_t descArr2 = *(uintptr_t*)((uintptr_t)desc + 0x28);
                uintptr_t descTemplate = *(uintptr_t*)(descArr2 + (uintptr_t)descIdx * 8);

                // Create IBP for owner (this entity)
                uintptr_t ownerIBP = 0;
                if (fn_InternalBasePtr_Ctor) {
                    fn_InternalBasePtr_Ctor((void*)&ownerIBP, self);
                }

                // Set component fields: +0x10 = owner IBP, +0x18 = description template
                if (fn_InternalBasePtr_CheckPtr) {
                    fn_InternalBasePtr_CheckPtr((void*)&newIBP);
                }
                void* compPtr = (void*)newIBP;
                *(uintptr_t*)((uintptr_t)compPtr + 0x10) = ownerIBP;
                *(uintptr_t*)((uintptr_t)compPtr + 0x18) = descTemplate;

                // Call component's vtable+0x20 (Serialize/Startup from description)
                if (fn_InternalBasePtr_CheckPtr) {
                    fn_InternalBasePtr_CheckPtr((void*)&newIBP);
                }
                compPtr = (void*)newIBP;
                uintptr_t cVtable = *(uintptr_t*)compPtr;
                typedef void (*fn_CompInit_t)(void*);
                fn_CompInit_t compInit = *(fn_CompInit_t*)(cVtable + 0x20);
                compInit(compPtr);

                // Resize component array to make room
                int oldCount = self->componentCount;
                fn_ExeArrayResize((void*)(s + 0x30), oldCount + 1);

                // Shift existing components right to make room at insertPos
                uintptr_t compArr = (uintptr_t)self->components;
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
            ComponentEntity_RefreshEntityFlags(self);
        }
    }

    // Destroy Serializer
    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Slot5 (0x140021c90): Secondary/External destructor
// ============================================================
// Sets vtable to secondary vtable (0x14015faa0), deletes IBP at +0x10,
// calls Object::Internal::~Internal, optionally frees memory.
void* ComponentEntity_Slot5(ComponentEntity* self, uint32_t flags) {
    EnsureResolved();

    // Set vtable to the secondary/external vtable
    *(void**)self = s_vtable_ComponentEntity_Ext;

    // Delete InternalBasePtr at +0x10 (entityList ref) if non-null
    uintptr_t ibpVal = (uintptr_t)self->entityList;
    if (ibpVal != 0) {
        if (fn_InternalBasePtr_DeletePtr) {
            fn_InternalBasePtr_DeletePtr((void*)&self->entityList);
        }
    }

    // Call Object::Internal::~Internal
    if (fn_ObjectInternal_Dtor) {
        fn_ObjectInternal_Dtor(self);
    }

    // Free memory if bit 0 set
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// Slot8 (0x1400c4b40): Serialize entityId helper
// ============================================================
// Creates a Serializer, serializes the entityId (int at +0x18), destroys Serializer.
void ComponentEntity_Slot8(ComponentEntity* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    uint8_t serializer[24];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Serialize entityId at +0x18
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(serializer, (void*)(s + 0x18), nullptr);
    }

    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Default constructor (0x1400c3620)
// ============================================================
ComponentEntity* ComponentEntity_CtorDefault(ComponentEntity* self) {
    EnsureResolved();
    uintptr_t s = (uintptr_t)self;

    // Call Object::Internal::Internal base ctor
    if (fn_ObjectInternal_Ctor) {
        fn_ObjectInternal_Ctor(self);
    }

    // Set vtable
    *(void**)self = s_vtable_ComponentEntity;

    // Initialize all fields
    self->entityList = nullptr;                     // +0x10
    self->entityId = -1;                            // +0x18
    self->flags |= 0xFFFFFF;                        // +0x1C: OR in 24-bit mask
    *(uint8_t*)(s + 0x1F) = 0;                      // +0x1F: zero byte (within flags)
    self->externals = nullptr;                      // +0x20
    self->description = nullptr;                    // +0x28
    self->components = nullptr;                     // +0x30
    // Retail zeroes the FULL qword at +0x38 — count AND capacity. Zeroing only
    // the int32 count left +0x3C (cap) holding recycled heap bytes; on the
    // load-from-save path the persist-created entity's resize then saw
    // cap > count, skipped the alloc, and the IBP loop deref'd a null array
    // ("a 4-byte port of an 8-byte zero strands an array at NULL").
    *(uint64_t*)(s + 0x38) = 0;                     // +0x38 count / +0x3C cap
    self->entityFlags = 0;                          // +0x40

    return self;
}

// ============================================================
// Copy/Factory constructor (0x1400c3720)
// ============================================================
// Creates entity from description: counts valid components, allocates array,
// creates each via Factory, sets owner/description, calls RefreshEntityFlags
// then StartupComponents.
static bool EntityRosterTraceOn() {
    static int en = -1;
    if (en < 0) {
        const char* e = std::getenv("BANISHED_ENTITY_ROSTER");
        en = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return en == 1;
}

ComponentEntity* ComponentEntity_CtorFromDescription(ComponentEntity* self, void* descriptionPtr) {
    EnsureResolved();
    uintptr_t s = (uintptr_t)self;

    // Call Object::Internal::Internal base ctor
    if (fn_ObjectInternal_Ctor) {
        fn_ObjectInternal_Ctor(self);
    }

    // Set vtable
    *(void**)self = s_vtable_ComponentEntity;

    // Initialize fields
    self->entityList = nullptr;
    self->entityId = -1;
    self->flags |= 0xFFFFFF;
    *(uint8_t*)(s + 0x1F) = 0;
    self->externals = nullptr;
    self->description = descriptionPtr;             // +0x28
    self->components = nullptr;                     // +0x30 array data
    self->componentCount = 0;                       // +0x38 array count
    *(uint32_t*)(s + 0x3c) = 0;                     // +0x3c array capacity
    self->entityFlags = 0;                          // +0x40

    // The entity block comes from MemAlloc(0x48), which does NOT zero memory. The
    // original CopyCtor zeroes the full component-array header (data/count/cap);
    // leaving +0x3c (capacity) as heap garbage makes fn_ExeArrayResize below see a
    // bogus oldCap >= newCount and skip the allocation, leaving components==null
    // and crashing the insert. (Latent until the description +0x40 fix made
    // validCount > 0.)

    uintptr_t desc = (uintptr_t)descriptionPtr;
    int descCompCount = *(int*)(desc + 0x30);

    // Sanity guard: a corrupted/unexpected description (component-count field read
    // as garbage) would size the component array to an absurd value and segfault in
    // fn_ExeArrayResize. Bail loudly with the desc pointer so the bad template is
    // identifiable. (Seen intermittently during resource scatter — likely heap
    // corruption upstream or an unexpected desc layout.)
    if (descCompCount < 0 || descCompCount > 256) {
        fprintf(stderr, "[CtorFromDesc] ABSURD descCompCount=%d desc=%p — entity gets 0 components\n",
                descCompCount, descriptionPtr);
        fflush(stderr);
        descCompCount = 0;
    }

    // First pass: count components with non-zero type IDs
    int validCount = 0;
    if (descCompCount > 0) {
        for (int64_t i = 0; i < (int64_t)descCompCount; i++) {
            uintptr_t descArr = *(uintptr_t*)(desc + 0x28);
            uintptr_t compTemplate = *(uintptr_t*)(descArr + (uintptr_t)i * 8);
            uintptr_t compVtable = *(uintptr_t*)compTemplate;
            typedef void (*fn_GetTypeId_t)(void*, uint32_t*);
            fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(compVtable + 0x40);
            uint32_t typeId = 0;
            getTypeId((void*)compTemplate, &typeId);
            if (typeId != 0) validCount++;
        }
    }

    // [CtorFromDesc] HAPPINESS probe -- diagnostic only, opt-in via
    // BANISHED_ENTITY_ROSTER=1. HappinessDescription instances exist in the world but
    // their +0x40 GetComponentTypeId is never called. A plain first-N-entities dump
    // cannot answer why: the budget is spent on trees and citizens long before the
    // chapel is placed. So instead of dumping entities, this watches for the ONE thing
    // in question -- a description sub-block whose own type is HappinessDescription --
    // and dumps that entity's whole roster when it appears. If the run ends with
    // happinessSubBlocksSeen == 0, the chapel's description list genuinely does not
    // carry one and the fault is upstream of this ctor entirely.
    if (EntityRosterTraceOn() && descCompCount > 0) {
        static uint32_t tidHappinessDesc = 0;
        if (tidHappinessDesc == 0)
            tidHappinessDesc = ::Object::GetTypeIdByName("HappinessDescription");
        bool hasHappiness = false;
        for (int64_t i = 0; i < (int64_t)descCompCount && tidHappinessDesc; i++) {
            uintptr_t descArr = *(uintptr_t*)(desc + 0x28);
            uintptr_t compTemplate = *(uintptr_t*)(descArr + (uintptr_t)i * 8);
            typedef uint32_t (*fn_SelfTypeId_t)(void*);
            uint32_t selfTid = (*(fn_SelfTypeId_t*)(*(uintptr_t*)compTemplate + 0x10))((void*)compTemplate);
            if (selfTid == tidHappinessDesc) { hasHappiness = true; break; }
        }
        static int nHit = 0;
        if (hasHappiness && nHit < 6) {
            ++nHit;
            fprintf(stderr, "[CtorFromDesc] HAPPINESS-CARRIER entity=%p desc=%p "
                    "descCompCount=%d validCount=%d\n",
                    (void*)self, descriptionPtr, descCompCount, validCount);
            for (int64_t i = 0; i < (int64_t)descCompCount; i++) {
                uintptr_t descArr = *(uintptr_t*)(desc + 0x28);
                uintptr_t compTemplate = *(uintptr_t*)(descArr + (uintptr_t)i * 8);
                uintptr_t compVtable = *(uintptr_t*)compTemplate;
                typedef uint32_t (*fn_SelfTypeId_t)(void*);
                uint32_t selfTid = (*(fn_SelfTypeId_t*)(compVtable + 0x10))((void*)compTemplate);
                typedef void (*fn_GetTypeId_t)(void*, uint32_t*);
                uint32_t compTid = 0;
                (*(fn_GetTypeId_t*)(compVtable + 0x40))((void*)compTemplate, &compTid);
                const char* dn = ::Object::GetTypeNameById(selfTid);
                const char* cn = ::Object::GetTypeNameById(compTid);
                fprintf(stderr, "[CtorFromDesc]   [%d] descType=%s(%u) -> component=%s(%u)\n",
                        (int)i, dn ? dn : "?", selfTid, cn ? cn : "(none)", compTid);
            }
            fflush(stderr);
        }
    }

    // Allocate component array
    fn_ExeArrayResize((void*)(s + 0x30), validCount);

    // Second pass: create components
    int insertIdx = 0;
    if (descCompCount > 0) {
        for (int64_t i = 0; i < (int64_t)descCompCount; i++) {
            uintptr_t descArr = *(uintptr_t*)(desc + 0x28);
            uintptr_t compTemplate = *(uintptr_t*)(descArr + (uintptr_t)i * 8);
            uintptr_t compVtable = *(uintptr_t*)compTemplate;
            typedef void (*fn_GetTypeId_t)(void*, uint32_t*);
            fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(compVtable + 0x40);
            uint32_t typeId = 0;
            getTypeId((void*)compTemplate, &typeId);
            if (typeId == 0) continue;

            // Create component from factory
            void* factoryPtr = g_pFactorySingleton ? *(void**)*g_pFactorySingleton : nullptr;
            void* newObj = nullptr;
            if (factoryPtr && fn_Factory_CreateObject) {
                newObj = fn_Factory_CreateObject(factoryPtr, typeId);
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
                fn_InternalBasePtr_Ctor((void*)&newIBP, newComp);
            }
            uintptr_t ownerIBP = 0;
            if (fn_InternalBasePtr_Ctor) {
                fn_InternalBasePtr_Ctor((void*)&ownerIBP, self);
            }

            // Resolve the component IBP (the original CopyCtor calls CheckPtr
            // before the field writes) then set owner (+0x10) and description
            // (+0x18).
            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr((void*)&newIBP);
            }
            void* compPtr = (void*)newIBP;
            if (!compPtr) {
                // STANDALONE: this component typeId has no working factory (the
                // Runtime/our factory tree is incomplete — e.g. a Tree's component
                // whose Foundation-D factory is missing). The original game always
                // has every factory, so it never hits this. Skip the component
                // rather than insert a null slot that crashes the array readers
                // (RefreshEntityFlags / OnAddedPass / StartupComponents).
                static int s_warned = 0;
                if (s_warned < 32) {
                    fprintf(stderr, "[CtorFromDesc] no factory for component typeId 0x%08x — skipped\n", typeId);
                    fflush(stderr); s_warned++;
                }
                continue;
            }
            *(uintptr_t*)((uintptr_t)compPtr + 0x10) = ownerIBP;
            *(uintptr_t*)((uintptr_t)compPtr + 0x18) = compTemplate;

            // Store in component array
            uintptr_t compArr = (uintptr_t)self->components;
            *(uintptr_t*)(compArr + (uintptr_t)insertIdx * 8) = newIBP;
            insertIdx++;
        }
    }

    // Correct the array count: fn_ExeArrayResize sized it to validCount, but in
    // standalone some components may have been skipped (missing factory), so the
    // real populated count is insertIdx. Leaving it at validCount would let the
    // readers walk past the last valid slot into uninitialised memory.
    self->componentCount = insertIdx;

    // Refresh flags and call startup on flagged components
    ComponentEntity_RefreshEntityFlags(self);
    ComponentEntity_StartupComponents(self);

    return self;
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
static void ComponentEntity_RegisterUpdateDraw(ComponentEntity* self, uint32_t oldFlags) {
    if (self->entityList == nullptr) return;
    uintptr_t el = (uintptr_t)self->entityList;
    uint32_t changed = oldFlags ^ self->entityFlags;
    if ((changed & 4) != 0) {
        int updateCount = *(int*)(el + 0x90) + 1;
        uintptr_t ibp = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor((void*)&ibp, self);
        if (fn_ExeArrayResizeFill) fn_ExeArrayResizeFill((void*)(el + 0x88), updateCount, (void*)&ibp);
    }
    if ((changed & 0x10) != 0) {
        int drawCount = *(int*)(el + 0xa0) + 1;
        uintptr_t ibp = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor((void*)&ibp, self);
        if (fn_ExeArrayResizeFill) fn_ExeArrayResizeFill((void*)(el + 0x98), drawCount, (void*)&ibp);
    }
}

// FUN_1400c2f60 — "OnAdded" lifecycle pass. If entityFlags bit 0 is set, call
// each component's vtable+0x30 (register-with-systems) for components whose
// Component+0x20 bit 0 is set.
static void ComponentEntity_OnAddedPass(ComponentEntity* self) {
    if ((self->entityFlags & 1) == 0) return;
    int count = self->componentCount;
    uintptr_t arr = (uintptr_t)self->components;
    for (int i = 0; i < count; i++) {
        uintptr_t ibpAddr = arr + (uintptr_t)i * 8;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)ibpAddr);
        uintptr_t compPtr = *(uintptr_t*)ibpAddr;
        if (!compPtr) continue;   // defensive: skip null component (standalone factory miss)
        if ((*(uint8_t*)(compPtr + 0x20) & 1) != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)ibpAddr);
            compPtr = *(uintptr_t*)ibpAddr;
            uintptr_t vt = *(uintptr_t*)compPtr;
            typedef void (*fn_t)(void*);
            (*(fn_t*)(vt + 0x30))((void*)compPtr);
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
    EnsureResolved();
    uint8_t* el = (uint8_t*)entityListV;
    if (!el || !entityIBP) return;

    if (*(int8_t*)(el + 0xE0) != 0) {                  // list closed → drop the entity
        if (fn_InternalBasePtr_DeletePtr) fn_InternalBasePtr_DeletePtr(entityIBP);
        return;
    }

    // Resolve the entity pointer (CheckPtr each time, matching the original).
    auto entity = [&]() -> uint8_t* {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(entityIBP);
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
    ComponentEntity_RegisterUpdateDraw((ComponentEntity*)ent, 0);

    // Append to the main entity array (count -> id+1).
    if (fn_ExeArrayResizeFill)
        fn_ExeArrayResizeFill((void*)(el + 0x08), id + 1, entityIBP);

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
        if (fn_ExeArrayResizeFill)
            fn_ExeArrayResizeFill((void*)bucket, bcount + 1, entityIBP);
    }

    ent = entity();
    ComponentEntity_OnAddedPass((ComponentEntity*)ent);
}

// ============================================================
// Standalone factory (factory cascade — concrete landmine removal)
// ============================================================
// Exe factory ComponentEntity_Factory (0x1400219c0): MemAlloc(0x48,8) ->
// ComponentEntity_Ctor (0x1400c3620) == ComponentEntity_CtorDefault above.
// Registered as the real factory so Object::Factory::CreateObject("ComponentEntity")
// / External::Load no longer hits the NullFactory -> IsDerivedFrom(NULL) crash.
// The default ctor sets the same vtable (0x14015fa78) the start-town spawn already
// uses via CtorFromDescription, so the resulting entity is a valid empty container.
extern "C" void* ComponentEntity_CreateInPlace_SA(void* mem) {
    if (!mem) return nullptr;
    return ComponentEntity_CtorDefault(reinterpret_cast<ComponentEntity*>(mem));
}
extern "C" void* ComponentEntity_Factory_SA(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    if (!p) return nullptr;
    return ComponentEntity_CtorDefault(reinterpret_cast<ComponentEntity*>(p));
}
REGISTER_TYPE(ComponentEntity, "Object::Internal",
              &ComponentEntity_Factory_SA, &ComponentEntity_CreateInPlace_SA);

// ============================================================
// Standalone ComponentEntity vtable (economy brick M-B)
// ============================================================
// 0x14015fa78 was never registered, so EVERY standalone entity was constructed
// with vtable == NULL (EnsureResolved -> ResolveAddr -> NULL). Nothing in the
// normal spawn/tick flow virtual-calls an entity, so this stayed invisible —
// until the produce/removal path: EntityList_RemoveEntity ends with
// InternalBasePtr::DeletePtr(entityIBP), whose Runtime body (0x1800c0150) does
// the FIRST-ever virtual dispatch on the entity (`CALL [vtable[0]]`, the scalar
// deleting dtor) and faulted on the NULL vtable. Same root for the dirty-list
// Serialize fault (vtable slot 3).
//
// .rdata layout at 0x14015fa78 (Ghidra) is TWO adjacent vtables, replicated
// here as one flat 12-qword array exactly like the original:
//   [0..3]  primary  (0x14015fa78): Dtor / GetTypeInfo / GetTypeId / Serialize
//   [4]     0        (RTTI complete-object-locator gap of the next vtable)
//   [5..11] base     (0x14015faa0): Slot5 dtor / GetTypeInfo2 / GetTypeId2 /
//                    Slot8 load / _purecall / ret0 / ret0
// address_registry.cpp registers 0x14015fa78 -> &[0] and 0x14015faa0 -> &[5],
// so both EnsureResolved() lookups above and every vtable-identity compare
// resolve consistently.

// Slots 1/6 (GetTypeInfo): the original returns a Runtime-filled type-info
// global (0x1401c5d78 / 0x1401c5db0) that has no standalone storage yet.
static void* CE_VGetTypeInfo(void*) { return nullptr; }
// Slots 2/7 (GetTypeId): original returns *(uint32_t*)0x1401c5d70 (/0x1401c5da8),
// the factory-assigned type id — REGISTER_TYPE above makes the by-name lookup
// return exactly that assignment.
static uint32_t CE_VGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ComponentEntity");
    return cached;
}
// Slot 9: shared _purecall handler (0x14003addc). Reaching it is a real bug —
// report loudly instead of the original's blocking MessageBox.
static void CE_VPurecall(void*) {
    fprintf(stderr, "[ComponentEntity] _purecall vtable slot 9 invoked — missing override\n");
    fflush(stderr);
}
// Slots 10/11 (0x1400c4a90 / 0x1400c4b90): xor eax,eax; ret.
static int CE_VRet0(void*) { return 0; }

extern "C" { extern void* g_ComponentEntity_StandaloneVtable[12]; }
void* g_ComponentEntity_StandaloneVtable[12] = {
    reinterpret_cast<void*>(&ComponentEntity_Dtor),       // [0] 0x140021a00 scalar dtor
    reinterpret_cast<void*>(&CE_VGetTypeInfo),            // [1] 0x1400219b0
    reinterpret_cast<void*>(&CE_VGetTypeId),              // [2] 0x1400219a0
    reinterpret_cast<void*>(&ComponentEntity_Serialize),  // [3] 0x1400c3140
    nullptr,                                              // [4] RTTI locator gap
    reinterpret_cast<void*>(&ComponentEntity_Slot5),      // [5] 0x140021c90 base dtor
    reinterpret_cast<void*>(&CE_VGetTypeInfo),            // [6] 0x140021c80
    reinterpret_cast<void*>(&CE_VGetTypeId),              // [7] 0x140021c70
    reinterpret_cast<void*>(&ComponentEntity_Slot8),      // [8] 0x1400c4b40 load
    reinterpret_cast<void*>(&CE_VPurecall),               // [9] 0x14003addc _purecall
    reinterpret_cast<void*>(&CE_VRet0),                   // [10] 0x1400c4a90
    reinterpret_cast<void*>(&CE_VRet0),                   // [11] 0x1400c4b90
};
