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
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Entity/componententity_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

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
static void DtorBody(ComponentEntity* self) {
    // Set vtable back (matches original, important for correct virtual dispatch during teardown)
    *(void**)self = s_vtable_ComponentEntity;

    // Shutdown flagged components
    ComponentEntity_ShutdownComponents(self);

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
// NOTE: Hook is DISABLED due to trampoline local_res corruption.
// Implemented here for standalone exe completeness.
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
    }

    // Serialize each component IBP
    uintptr_t ibpArr = (uintptr_t)self->components;
    int64_t cnt = (int64_t)(int)compCount;
    if (cnt > 0) {
        for (int64_t i = 0; i < cnt; i++) {
            uintptr_t ibpAddr = ibpArr + (uintptr_t)i * 8;
            // Serialize the InternalBasePtr
            if (fn_InternalBasePtr_Serialize) {
                fn_InternalBasePtr_Serialize((void*)ibpAddr, serializer);
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
    self->componentCount = 0;                       // +0x38
    self->entityFlags = 0;                          // +0x40

    return self;
}

// ============================================================
// Copy/Factory constructor (0x1400c3720)
// ============================================================
// Creates entity from description: counts valid components, allocates array,
// creates each via Factory, sets owner/description, calls RefreshEntityFlags
// then StartupComponents.
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
    self->components = nullptr;
    self->componentCount = 0;
    self->entityFlags = 0;

    uintptr_t desc = (uintptr_t)descriptionPtr;
    int descCompCount = *(int*)(desc + 0x30);

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

            // Verify it derives from Component
            void* newComp = nullptr;
            if (newObj && fn_IsDerivedFrom && g_TypeId_ComponentBase) {
                if (fn_IsDerivedFrom(newObj, *g_TypeId_ComponentBase)) {
                    newComp = newObj;
                }
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

            // Set component owner (+0x10) and description (+0x18)
            void* compPtr = (void*)newIBP;
            *(uintptr_t*)((uintptr_t)compPtr + 0x10) = ownerIBP;
            *(uintptr_t*)((uintptr_t)compPtr + 0x18) = compTemplate;

            // Store in component array
            uintptr_t compArr = (uintptr_t)self->components;
            *(uintptr_t*)(compArr + (uintptr_t)insertIdx * 8) = newIBP;
            insertIdx++;
        }
    }

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
