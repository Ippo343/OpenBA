#pragma once
// ============================================================================
// code/Game/Entity/ComponentEntity.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5,
// D9 #2)
//
// `ComponentEntity`, the ECS entity container, as a REAL C++ class whose vtable
// the compiler generates. engine/types/ComponentEntity.h forwards here (B8-4).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ComponentEntity", parent "Object::Internal", ti 0x1401c5d70
//   create 0x1400219c0: MemAlloc(0x48, 8), ctor 0x1400c3620.
//   createInPlace = the Runtime's IMPORT `Object::Internal::CreateInPlaceObject`
//   (retail in-place-creates a bare Internal; the tree runs the full ctor, kept).
//   table 0x14015fa78, 4 slots:
//     [0] 0x140021a00 scalar deleting dtor (body 0x1400c3550)
//     [1] 0x1400219b0 TypeName   [2] 0x1400219a0 Type
//     [3] 0x1400c3140 Persist (the entity's description, components, flags)
//   ctors 0x1400c3620 (default) and 0x1400c3720 (from a ComponentDescription —
//   the "CopyCtor", reached by address as fn_EntityFactory_CreateEntity).
//   sizeof 0x48.
//
// The tree's hand-built table was 12 qwords: these 4 slots, a null, and the 7
// slots of `Action`'s table 0x14015faa0 that sit next to it in .rdata (its
// `ComponentEntity_Slot5`/`Slot8` were Action's dtor and Persist). D9 #2: the
// Action run is ACT's (Action::Vtable()); ComponentEntity keeps its own four.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/Internal.h"
#include "Object/exe_type.h"
#include "Component/Component.h"   // FindComponent<T>: Array<InternalPtr<Component>>

class ComponentDescription;

class ComponentEntity final : public Object::Internal {
    DECLARE_EXE_TYPE(ComponentEntity)

public:
    ComponentEntity();                                            // 0x1400c3620
    explicit ComponentEntity(ComponentDescription* description);  // 0x1400c3720

    // 0x1400c3550
    ~ComponentEntity() override;                                  // [0] 0x140021a00
    void          Persist(void* stream) override;                 // [3] 0x1400c3140

    // ---- non-virtual members (retail addresses) ----
    void RefreshEntityFlags();                                    // 0x1400c3470
    void StartupComponents();                                     // 0x1400c30a0
    void ShutdownComponents();                                    // 0x1400c3000
    void RegisterUpdateDraw(uint32_t oldFlags);                   // 0x1400c33e0
    void OnAddedPass();                                           // 0x1400c2f60
    // The EntityList tree walks' per-entity bodies (B4 WORK-a; no retail symbol:
    // named from what they do). Defined in entitylist.cpp.
    void TickComponents(void* time);                              // 0x1400c2de0 slot 8 on flag-bit-2 components
    void PostUpdateComponents();                                  // 0x1400c2c70 slot 10 on flag-bit-4 components
    void SendEvent(int eventId);                                  // 0x1400c2b70 slot 12 on every component (body: modelcomponent.cpp)

    // ---- FindComponent<T> (the template retail instantiates per component class)
    // Every instance has one shape (read 2026-09-28 at 0x140003f60 ModelComponent,
    // 0x14000c460 GrowthComponent, 0x140006270 StorageComponent, 0x140004740
    // MultiObjectComponent, 0x14000b6f0 ConsumeProduceComponent and 20 more):
    //   for (i = 0; i < _componentCount; ++i) {          // count re-read per pass
    //       CheckPtr(&_components[i]);                   // IAT 0x140152ab0, a bare RET
    //       if (_components[i]->Type() == T's cell)      // slot 2, [vtbl+0x10]
    //           return IsDerivedFrom(c, T's cell) ? c : 0;   // IAT 0x140152a98
    //   }
    //   return 0;
    // written through the InternalBasePtr ctor (IAT 0x140152a90, `mov [rcx],rdx`)
    // into the caller's return slot: retail returns an InternalPtr<T> by value,
    // which is these same eight bytes (Object::InternalPtr is trivially
    // copyable), so the member returns the pointer TYPED (B4_RULES B10).
    // No null check on the entity or on a component: retail has none.
    // (The generic 0x1400c3680 is a different body: exact-id compare only, no
    // IsDerivedFrom; it is not this template.)
    template <class T>
    T* FindComponent() const {
        for (int32_t i = 0; i < _componentCount; ++i) {
            Component* c = _components[i].Get();
            if (c->Type() == T::TypeId())
                return c->IsDerivedFrom(T::TypeId()) ? static_cast<T*>(c) : nullptr;
        }
        return nullptr;
    }

    // 0x1400c3680 -- the generic find by a runtime type id: the same walk
    // (CheckPtr, Type() compare, count re-read per pass) but an EXACT id match
    // only, no IsDerivedFrom; the match is stored raw into the return slot
    // (`mov [r14],0; mov [r14],rax`), a miss through the InternalBasePtr ctor
    // with null. No null checks.
    Component* FindComponentByTypeId(uint32_t typeId) const {
        for (int32_t i = 0; i < _componentCount; ++i) {
            Component* c = _components[i].Get();
            if (c->Type() == typeId)
                return c;
        }
        return nullptr;
    }

    // ---- fields (PUBLIC: EntityList and many unconverted readers use the raw
    // offsets) ----
    void*                 _entityList;      // +0x10 owning EntityList (set by RegisterEntity)
    int32_t               _entityId;        // +0x18 -1 sentinel
    uint32_t              _handle;          // +0x1C low 24 bits: category-bucket index
                                            //       (0xFFFFFF = none); +0x1F: category byte
    void*                 _externals;       // +0x20 = entityList[0]
    ComponentDescription* _description;     // +0x28 ExternalPtr<ComponentDescription>
    Object::InternalPtr<Component>* _components;  // +0x30 Array<InternalPtr<Component>> data (stride 8;
                                            //       every FindComponent instance CheckPtr's &_components[i])
    int32_t               _componentCount;  // +0x38
    int32_t               _componentCap;    // +0x3C
    uint32_t              _entityFlags;     // +0x40 OR of every component's +0x20 flags:
                                            //       bit 0 OnAdded, 2 update, 4 draw,
                                            //       7 Startup, 8 Shutdown
    uint8_t               _pad44[4];        // +0x44
};

static_assert(sizeof(ComponentEntity) == 0x48, "ComponentEntity is 0x48 (create 0x1400219c0 MemAlloc(0x48))");
static_assert(alignof(ComponentEntity) == 8, "ComponentEntity alignment");
static_assert(offsetof(ComponentEntity, _entityList)     == 0x10, "ComponentEntity::_entityList @ +0x10");
static_assert(offsetof(ComponentEntity, _entityId)       == 0x18, "ComponentEntity::_entityId @ +0x18");
static_assert(offsetof(ComponentEntity, _handle)         == 0x1C, "ComponentEntity::_handle @ +0x1C");
static_assert(offsetof(ComponentEntity, _externals)      == 0x20, "ComponentEntity::_externals @ +0x20");
static_assert(offsetof(ComponentEntity, _description)    == 0x28, "ComponentEntity::_description @ +0x28");
static_assert(offsetof(ComponentEntity, _components)     == 0x30, "ComponentEntity::_components @ +0x30");
static_assert(offsetof(ComponentEntity, _componentCount) == 0x38, "ComponentEntity::_componentCount @ +0x38");
static_assert(offsetof(ComponentEntity, _componentCap)   == 0x3C, "ComponentEntity::_componentCap @ +0x3C");
static_assert(offsetof(ComponentEntity, _entityFlags)    == 0x40, "ComponentEntity::_entityFlags @ +0x40");
