// ============================================================================
// Thumbnail-object vtables (exe .rdata 0x14015c400 / 0x14015c3b0)
// ============================================================================
// Four dialogs (SaveLoad, Mod, Noise, Map) build a UI thumbnail out of a
// hand-allocated Graphics::MaterialInstance (0x60) + Graphics::SpriteSheet
// (0x58). Retail constructs each with the Runtime ctor and then *overwrites*
// the vtable pointer with the exe's own copy of that class's vtable:
//
//   0x14015c400  MaterialInstance  9 slots, slot0 = FUN_1400121a0
//   0x14015c3b0  SpriteSheet       9 slots, slot0 = FUN_1400121e0
//
// Both exe copies are the compiler-emitted duplicate of the Runtime vtable —
// slots 1..8 are `jmp [dispatch]` thunks onto the same Runtime methods, and
// slot 0 is the exe's inlined scalar-deleting dtor (`~T(this); if (flags&1)
// MemFree(this)`).
//
// Neither address is in the standalone address registry, so
// ResolveAddr(Module::Exe, ...) returned 0 and the stamp wrote a NULL vtable
// pointer into every thumbnail object. Nothing dispatched on it until the
// *second* SaveLoadDialog::PopulateFileList — which destroys the previous
// entries through vtable[0] — so deleting a save (which sets deleteFlag and
// re-scans the list) faulted on `mov (%rax),%rax` with rax = 0.
//
// Rather than guard the store, mirror the retail vtables: build a 9-slot array
// per class whose slots 1..8 are copied from the base vtable the Runtime ctor
// installed, and whose slot 0 is our equivalent of the exe's inlined deleting
// dtor. The mirror is armed by the first object of its class to be
// constructed, so the stamp helpers below are the single entry point — the
// exe addresses are deliberately NOT put in the address registry, because a
// registered-but-unarmed array would hand every caller a vtable full of
// zeroes, which is strictly harder to diagnose than the null it replaced.
// g_ModDialog_*Vtable staying 0 keeps meaning "retail exe vtable absent".

#include <cstdint>
#include "../../../engine/funcs/engine_funcs.h"

namespace {

constexpr int kSlots = 9;

typedef void* (*fn_ScalarDeletingDtor_t)(void* self, uint32_t flags);

struct Mirror {
    void* slots[kSlots] = {};
    fn_ScalarDeletingDtor_t baseDtor = nullptr;
    bool  ready = false;
};

Mirror g_mat;
Mirror g_sheet;

// The exe copies of slot 0 (FUN_1400121a0 / FUN_1400121e0) are
//     ~T(this); if (flags & 1) System::MemoryAllocator::MemFree(this);
// The Runtime's own slot 0 is the same scalar-deleting dtor, so calling it
// with flags=0 runs exactly the `~T(this)` half and leaves the free to us.
void* DeletingDtor(Mirror& m, void* self, uint32_t flags) {
    if (!self) return self;
    if (m.baseDtor) m.baseDtor(self, 0);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

void* MatDtor(void* self, uint32_t flags)   { return DeletingDtor(g_mat, self, flags); }
void* SheetDtor(void* self, uint32_t flags) { return DeletingDtor(g_sheet, self, flags); }

// Fill slots 1..8 from the base vtable the Runtime ctor just installed on
// `constructed`, then plant our slot 0. Idempotent; the first constructed
// object of each class arms it.
void Arm(Mirror& m, void* constructed, void* dtor) {
    if (m.ready || !constructed) return;
    void** base = *reinterpret_cast<void***>(constructed);
    if (!base) return;
    for (int i = 1; i < kSlots; i++) m.slots[i] = base[i];
    m.baseDtor = reinterpret_cast<fn_ScalarDeletingDtor_t>(base[0]);
    m.slots[0] = dtor;
    m.ready = true;
}

} // namespace

// Stamp helpers used by the four dialogs. `obj` must already have been through
// its Runtime ctor. Returns the vtable that was stamped (null only if the
// object had no base vtable to mirror, in which case nothing is written and
// the ctor's vtable survives).
uintptr_t ThumbVtable_StampMaterialInstance(void* obj) {
    Arm(g_mat, obj, reinterpret_cast<void*>(&MatDtor));
    uintptr_t vt = g_ModDialog_MaterialInstanceVtable;
    if (!vt && g_mat.ready) vt = reinterpret_cast<uintptr_t>(g_mat.slots);
    if (vt && obj) *reinterpret_cast<uintptr_t*>(obj) = vt;
    return vt;
}

uintptr_t ThumbVtable_StampSpriteSheet(void* obj) {
    Arm(g_sheet, obj, reinterpret_cast<void*>(&SheetDtor));
    uintptr_t vt = g_ModDialog_SpriteSheetVtable;
    if (!vt && g_sheet.ready) vt = reinterpret_cast<uintptr_t>(g_sheet.slots);
    if (vt && obj) *reinterpret_cast<uintptr_t*>(obj) = vt;
    return vt;
}
