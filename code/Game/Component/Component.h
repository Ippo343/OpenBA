#pragma once
// ============================================================================
// code/Game/Component/Component.h — ENDGAME B.3.8 (B38_RULES.md §2.1, D1-D3)
//
// `Component`, the exe's ECS component base, as a REAL C++ class whose vtable
// the compiler generates. Global namespace, as in retail (the reflected name is
// the bare "Component", parent literal "Object::Internal").
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §1, read 2026-09-26):
//   ctor   0x1401188b0  `??0Internal@Object@@` (IAT 0x140152768); stamp
//                       0x140162920; +0x10 = 0; +0x18 = 0; +0x20 = 0x80; then
//                       `if (+0x10 != 0) { InternalBasePtr ctor; 0x1400c3470 }`
//                       — DEAD, +0x10 was zeroed three instructions earlier
//                       (the inlined flag-change notify; see SetFlags below).
//   table  0x140162920, 16 slots, NO pure slot, create = 0x140003730 (abstract)
//   [0] 0x140031cc0 scalar deleting dtor: stamp 0x140162920, `~Internal`
//                   (IAT), `if (flags & 1) MemFree`. ~Component has no body.
//   [1] TypeName   [2] Type
//   [3] 0x1401189b0 Persist: `Internal::Persist`, Serializer(stream,0),
//                   InternalPtr +0x10 (entity; nulled unless IsDerivedFrom
//                   ComponentEntity), dword +0x20, 0x140031d10(+0x18), ~Serializer
//   [4]-[12], [14], [15]  shared `RET` no-ops (0x140118a50 … 0x140118ae0)
//   [13] 0x140118ac0 `XOR EAX,EAX; RET`
//
// LAYOUT: Object::Internal (0x10) + three fields = 0x28. The old POD
// (engine/types/Component.h, now a forwarder to this file) called +0x08 `next`
// and +0x10 `owner`: +0x08 is Object::Internal's `_factoryIndex/_generation`
// pair, and +0x10 is the owning ComponentEntity.
//
// SLOT NAMES 4-15 have no retail symbol (the exe has none). They are the tree's
// names from the slot stores (B38_RULES §2.1). ARGUMENT LISTS of 8, 9, 12-15
// are the tree's C bodies' parameter lists, NOT read from retail callers —
// B38_RULES §6 NOT VERIFIED; the owning block reads each caller before it
// relies on an argument.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/Internal.h"

class Component : public Object::Internal {
public:
    // ---- the 16 slots, in retail order -------------------------------------
    Object::Base* DeletingDtor(unsigned flags) override;         // [0]  0x140031cc0
    // [1] TypeName / [2] Type: the abstract base answers its own registration
    // (factory_registrations_bulk.cpp keeps it, B38 D4); every leaf overrides.
    const char* TypeName() const override;                        // [1]  0x140031c40
    uint32_t    Type() const override;                            // [2]  0x140031c30
    void        Persist(void* stream) override;                   // [3]  0x1401189b0
    virtual void     Startup();                                   // [4]  0x140118a50 RET
    virtual void     Slot5();                                     // [5]  0x140118a60 RET
    virtual void     OnAdded();                                   // [6]  0x140118820 RET
    virtual void     OnRemove();                                  // [7]  0x140118830 RET
    virtual void     Tick(void* time);                            // [8]  0x140118a70 RET
    virtual void     Update(void* time);                          // [9]  0x140118a80 RET
    virtual void     PostUpdate();                                // [10] 0x140118a90 RET
    virtual void     Slot11();                                    // [11] 0x140118aa0 RET
    virtual void     OnEvent(int eventId);                        // [12] 0x140118ab0 RET
    virtual uint32_t CommandMask(int command);                    // [13] 0x140118ac0 ret 0
    virtual void     Command(int command);                        // [14] 0x140118ad0 RET
    virtual void     OnMessage(int message, void* data,
                               uint32_t arg0, uint32_t arg1);     // [15] 0x140118ae0 RET

    // ---- the 0x18 bytes Component adds, retail order ------------------------
    // PUBLIC while unconverted bodies read them at raw offsets (decision 1).
    void*    _entity;        // +0x10 owning ComponentEntity (InternalPtr in retail)
    void*    _description;   // +0x18 serialized by 0x140031d10 (the tree's "description ref")
    uint32_t _flags;         // +0x20 capability flags; the ctor sets 0x80
    uint32_t _pad24;         // +0x24 never written by the ctor

protected:
    // 0x1401188b0. The `Internal()` base constructor is the retail
    // `??0Internal@Object@@` call; the vptr stores are the compiler's.
    Component();
    // No body in retail (slot 0 only re-stamps the table before ~Internal).
    ~Component() = default;

    // The retail flag-OR idiom every leaf ctor inlines after its own stamps:
    //   old = _flags; _flags |= bits; if ((old ^ _flags) & 0xffff && _entity)
    //       { InternalBasePtr(&_entity) ; 0x1400c3470(_entity) }
    // In a ctor `_entity` is always null (Component() zeroed it), so only the
    // OR survives. The notify arm is NOT ported here: a leaf that ORs flags
    // outside its ctor ports it with the caller (B38_RULES §6).
    void OrFlagsInCtor(uint32_t bits) { _flags |= bits; }
};

static_assert(sizeof(Component) == 0x28, "Component is 0x28 bytes (0x10 Internal + 0x18)");
static_assert(offsetof(Component, _entity)      == 0x10, "Component +0x10 entity (0x1401188de)");
static_assert(offsetof(Component, _description) == 0x18, "Component +0x18 (0x1401188e2)");
static_assert(offsetof(Component, _flags)       == 0x20, "Component +0x20 flags = 0x80 (0x1401188e6)");

// Ported Component base Persist (retail 0x1401189b0). The body lives in
// absorbresourcecomponent.cpp until block K1 moves it into Component::Persist;
// the member delegates to it (one body, B38 D5).
void Component_SerializeReimpl(void* self, void* stream);
