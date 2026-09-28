// code/Game/Component/cemeterycomponent.cpp
// Clean reimplementation of CemeteryComponent lifecycle methods.
//
// CemeteryComponent (0x58 bytes) -- ControllerComponent subclass.
// Fields: +0x48=array ptr, +0x50=count. Manages cemetery plot grid for tombstone placement.
//
// Vtable at 0x1401590a0. Non-trivial overrides:
//   Shutdown  (0x1400475C0) -- serialize array of tile indices
//   Draw      (0x140047200) -- complex grid rendering (233 instrs), delegates to original
//   OnCreate  (0x1400471C0) -- register entity in cemetery list
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/cemeterycomponent_funcs.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "perf_terrain.h"
#include "resolve.h"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <new>
#include "Component/TombstoneComponent.h"   // ENDGAME B.3.9 C1: 0x1401c4a30 cell class

// ============================================================
// Resolved helpers
// ============================================================


// FUN_1400021d0 was Collection::Array<int32>::SetSize monomorphised into the
// exe (ENDGAME B.3.2 step 2) — converted at its call site below.

// FUN_140002cc0 -- add InternalBasePtr to linked list
typedef void (*fn_ListAdd_t)(void* listBase, void* ptrValue);
static fn_ListAdd_t s_fn_ListAdd = nullptr;

// FUN_140004680 -- member-ABI FindComponent<MapComponent>(entity, outIBP) -> outIBP.
// (registered by reg_component_nature)
typedef void** (*fn_FindMapComp_t)(uintptr_t entity, void** outIBP);
static fn_FindMapComp_t s_fn_FindMapComp = nullptr;

// FUN_140004740 -- member-ABI FindComponent<MultiObjectComponent>(entity, outIBP).
// (ported in interactcomponent.cpp, registered by reg_component_economy)
typedef void  (*fn_FindMultiComp_t)(uintptr_t entity, void* outIBP);
static fn_FindMultiComp_t s_fn_FindMultiComp = nullptr;

// FUN_140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
// into the exe (ENDGAME B.3.2 step 2) — converted at its call site below.

// 0x1400a6fe0 PfEnqueueCmd2(pfQueue, cellIdx, cmd) / 0x1400a8c90 FineCellDirty.
typedef void (*fn_PfEnqueueCmd2_t)(void* queue, int cellIdx, int cmd);
static fn_PfEnqueueCmd2_t s_fn_PfEnqueueCmd2 = nullptr;
typedef void (*fn_FineCellDirty_t)(void* obj, int cellIdx, int row);
static fn_FineCellDirty_t s_fn_FineCellDirty = nullptr;

// System::MemoryAllocator::MemAlloc(size, align) / MemFree(ptr) dispatch slots.
typedef void* (*fn_CemMemAlloc_t)(uint64_t size, int align);
static fn_CemMemAlloc_t s_fn_MemAlloc = nullptr;
typedef void  (*fn_CemMemFree_t)(void* p);
static fn_CemMemFree_t s_fn_MemFree = nullptr;

// TombstoneComponent type id (.rdata DAT_1401c4a30, storage + fill in
// standalone/reg_citizen_lifeevent_handler.cpp).

static void** s_g_pGameState = nullptr;

static bool s_initialized = false;

// ============================================================
// CemeteryComponent::Shutdown (vtable slot [3], 0x1400475C0)
// ============================================================
// Serializes:
//   1. Component_Serialize base
//   2. Empty serializer block (controller compat stub)
//   3. Serializer(version=0): count from +0x50, resize on load, bulk serialize int array

// ============================================================
// DynArray<int>::Push -- inlined by the plot-grid builder at 0x1400473a2
// ============================================================
// {ptr @+0x48, count @+0x50, capacity @+0x54}. Retail grows the capacity to 3*newCount
// when the array is already non-empty and this push is the one that overflows it
// (`lea eax,[rbx+rbx*2]`), reallocs, copies the old elements, writes the new one past
// the old count, then frees the old block. 1:1, including the "copy or fill" branch --
// indices below the old count come from the old block, the rest get the new value.
static void CemeteryComponent_PushPlot(CemeteryComponent* self, int cell) {
    const int      oldCount = self->plotCount;
    const int      newCount = oldCount + 1;
    int32_t* const oldPtr   = self->plots;

    self->plotCount = newCount;
    if (newCount > self->plotCapacity) {
        // `lea eax,[rbx+rbx*2]` -- the 3x over-allocate applies only once the array is
        // already non-empty; the very first push allocates exactly one element.
        self->plotCapacity = (oldCount != 0) ? newCount * 3 : newCount;
        if (!s_fn_MemAlloc) { self->plotCount = oldCount; self->plotCapacity = 0; return; }
        self->plots = reinterpret_cast<int32_t*>(
            s_fn_MemAlloc(static_cast<uint64_t>(self->plotCapacity) * 4, 4));
    }
    if (!self->plots) { self->plotCount = oldCount; self->plots = oldPtr; return; }

    // Retail starts the write at 0 when the block moved (so the old elements are copied
    // across) and at oldCount when it did not (only the new element is written).
    for (int i = (oldPtr != self->plots) ? 0 : oldCount; i < newCount; ++i)
        self->plots[i] = (i < oldCount) ? oldPtr[i] : cell;

    if (oldPtr != self->plots && oldPtr && s_fn_MemFree) s_fn_MemFree(oldPtr);
}


// ============================================================
// CemeteryComponent::BuildPlotGrid (vtable slot [6], 0x140047200)
// ============================================================
// Ghidra calls this slot Draw; it draws nothing. It BUILDS the grave-plot cell list
// (self->plots / plotCount / plotCapacity) from the footprint, and it is the reason a
// graveyard could never accept a burial: the previous port here was a passthrough to
// `ResolveAddr(Module::Exe, 0x140047200)` -- its OWN address, which resolves to null in
// standalone -- so the call was silently skipped, the plot array stayed empty, and
// Cemetery_PickFreeSlot_6c60 returned -1 for every death, forever.
//
// Retail shape (0x140047200), read off the disassembly:
//   map    = FindComponent<MapComponent>(owner)      // footprint x,y,w,h at +0x28..+0x2e
//   rot    = map[+0x30]                              // 0 / 90 / 180 / 270
//   world  = *(*0x1401c3dd8 + 0xf8)                  // terrain; *world = map width
//   multi  = FindComponent<MultiObjectComponent>(owner)
//   gate   = FindComponent<MapComponent>(*(multi+0x48)[0])   // the FIRST child entity
//   The gate child is the cemetery's entrance tile; its coordinate along the footprint's
//   long axis gives the two columns (`door`, `door-1`) that stay clear as the path in.
//   The footprint is then inset by one tile on every side (x0/y0 += 1, w/h -= 2) and
//   every OTHER row is walked, skipping the two door columns -- that alternation is what
//   makes graves sit in rows with a gap to walk between them.
//
// Which axis the door is measured on, and which loop steps by two, swaps with rotation:
// rot 0/180 walk rows outer, rot 90/270 walk columns outer. Both arms are transcribed.
// Per-site trace cap; the counter is a constant-initialised atomic (no first-call
// race, B6-5: OnAdded runs on the loader thread).
#define CEM_TRACE(fmt, ...) do { static std::atomic<int> _n{0}; \
    if (_n.fetch_add(1, std::memory_order_relaxed) < 12) { \
    fprintf(stderr, "[CemeteryPlots] " fmt "\n", ##__VA_ARGS__); fflush(stderr); } } while (0)

// ============================================================
// CemeteryComponent::OnRemove (vtable slot [7], 0x1400471C0)
// ============================================================
// UNregisters from the TownManager cemetery list at *(GameWorld+0xF0)+0x40.
// Ghidra labels the slot OnCreate; 0x140002cc0 is the ERASE (see the 1:1 port of it
// further down this file). The matching REGISTER is the state-2 enter at the bottom.

// ============================================================
// Reimpl of FUN_140002cc0 (CemeteryComponent list erase-by-pointer)
// ============================================================
// Scans the InternalBasePtr array {ptr@list+0, count@list+8} for an entry equal to
// the resolved value of `ptrValue` and removes it, shifting the trailing entries
// down by one slot and decrementing the count. Calls CheckPtr on the search key
// before comparing (resolves lazy/weak pointers, mirroring the original). The exe
// shifts via a quirky `*p=0; *p=p[1]` (effectively `p[0]=p[1]`). 1:1 port.
extern "C" void CemeteryListErase_Reimpl(void* listBase, void* ptrValue) {
    int64_t* list = reinterpret_cast<int64_t*>(listBase);
    int i = 0;
    while (i < (int)list[1]) {
        int64_t arr = list[0];
        if (*(int64_t*)(arr + (int64_t)i * 8) == *(int64_t*)ptrValue) {
            for (int j = i + 1; j < (int)list[1]; ++j) {
                int64_t* slot = (int64_t*)(list[0] + (int64_t)(j - 1) * 8);
                if (slot) slot[0] = slot[1];
            }
            *(int*)(list + 1) = (int)list[1] - 1;
            --i;
        }
        ++i;
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void CemeteryComponent_InitFuncs() {
    if (s_initialized) return;


    uintptr_t slot;

    s_fn_ListAdd = reinterpret_cast<fn_ListAdd_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_fn_FindMapComp = reinterpret_cast<fn_FindMapComp_t>(
        ResolveAddr(Module::Exe, 0x140004680));
    s_fn_FindMultiComp = reinterpret_cast<fn_FindMultiComp_t>(
        ResolveAddr(Module::Exe, 0x140004740));
    s_fn_PfEnqueueCmd2 = reinterpret_cast<fn_PfEnqueueCmd2_t>(
        ResolveAddr(Module::Exe, 0x1400a6fe0));
    s_fn_FineCellDirty = reinterpret_cast<fn_FineCellDirty_t>(
        ResolveAddr(Module::Exe, 0x1400a8c90));
    slot = ResolveAddr(Module::Exe, 0x140152b60);
    if (slot) s_fn_MemAlloc = *reinterpret_cast<fn_CemMemAlloc_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152b68);
    if (slot) s_fn_MemFree = *reinterpret_cast<fn_CemMemFree_t*>(slot);

    s_g_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    s_initialized = true;

#ifdef _DEBUG
    printf("[CemeteryComponent] Funcs resolved: Erase=%p FindMap=%p FindMulti=%p\n",
           (void*)s_fn_ListAdd, (void*)s_fn_FindMapComp,
           (void*)s_fn_FindMultiComp);
#endif
}

// ============================================================
// CemeteryComponent state-2 "built" enter -- 0x140046fc0
// ============================================================
// Two jobs, in order:
//   1. Register the graveyard: ArrayPush(town+0x40, town[0x48]+1, IBP(self)).
//      This is the list `Cemetery_FindNearestWithSlot_9740` walks on every death. With
//      the component inert nothing ever entered state 2, so it was empty for the life of
//      the save and no citizen could ever be buried.
//   2. Claim the plot cells: for each cell in self->plots, if the cell already holds an
//      entity that owns a TombstoneComponent, leave it alone (a loaded save re-entering
//      state 2 keeps its existing graves); otherwise clear the cell's entity slot, stamp
//      its zone byte to 2, and tell the pathfinder and the fine-cell raster.
//
// The cell record is 0x30 bytes: +0x08 entity IBP, +0x29 zone byte. Retail returns 0 in
// AL so the state machine's Enter walk continues past this node.
extern "C" char CemeteryComponent_EnterBuilt(void* compV, void* /*sm*/) {
    CemeteryComponent* self = reinterpret_cast<CemeteryComponent*>(compV);
    CemeteryComponent_InitFuncs();
    if (!fn_InternalBasePtr_Ctor || !s_g_pGameState || !*s_g_pGameState)
        return 0;

    const uintptr_t gw = reinterpret_cast<uintptr_t>(*s_g_pGameState);
    const uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(gw + 0xF0);
    if (!townMgr) return 0;

    uint8_t ibp[24] = {};
    new (ibp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
    const int count = *reinterpret_cast<int*>(townMgr + 0x48);
    // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
    GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(townMgr + 0x40))
        .SetSizeFill(count + 1, ibp);

    const uintptr_t world = *reinterpret_cast<uintptr_t*>(gw + 0xF8);
    if (!world) return 0;

    int claimed = 0;
    for (int i = 0; i < self->plotCount; ++i) {
        const int cellIdx = self->plots[i];
        const uintptr_t cells = *reinterpret_cast<uintptr_t*>(world + 0x80);
        if (!cells) break;
        const uintptr_t cell = cells + static_cast<uintptr_t>(cellIdx) * 0x30;

        // Already occupied by a grave? Leave the plot as it stands.
        void* occupant = *reinterpret_cast<void**>(cell + 0x08);
        if (occupant) {
            void* probe = occupant;
            bool hasTombstone = false;
            if (probe) {
                const uint32_t tidTombstone = TombstoneComponent::TypeId();
                const uintptr_t ent = reinterpret_cast<uintptr_t>(probe);
                const int       n   = *reinterpret_cast<int*>(ent + 0x38);
                const uintptr_t arr = *reinterpret_cast<uintptr_t*>(ent + 0x30);
                for (int j = 0; j < n; ++j) {
                    void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(j) * 8);
                    void* c = *slot;
                    if (!c) continue;
                    const uint32_t tid = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.8 bridge (slot 2)
                    if (tid == tidTombstone) { hasTombstone = true; break; }
                }
            }
            if (hasTombstone) continue;
        }

        // Free plot: stamp the zone byte, notify, and null the entity slot.
        const uint8_t zone = *reinterpret_cast<uint8_t*>(cell + 0x29);
        const bool walkableZone = ((1u << zone) & 0x1e0u) != 0;
        *reinterpret_cast<uint8_t*>(world + 0xA2) |= walkableZone ? 1 : 0;
        *reinterpret_cast<uint8_t*>(cell + 0x29) = 2;
        PerfX::TerrainNoteWrite(reinterpret_cast<const uint8_t*>(cell));   // [PerfX] mirror

        if (walkableZone && s_fn_FineCellDirty) {
            const int mapWidth = *reinterpret_cast<int*>(world);
            void* fine = *reinterpret_cast<void**>(world + 0x70);
            if (fine && mapWidth) s_fn_FineCellDirty(fine, cellIdx, cellIdx / mapWidth);
        }
        if (s_fn_PfEnqueueCmd2) {
            void* pf = *reinterpret_cast<void**>(world + 0x68);
            if (pf) s_fn_PfEnqueueCmd2(pf, cellIdx, 2);
        }

        uint8_t nullIbp[24] = {};
        new (nullIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *reinterpret_cast<void**>(cell + 0x08) = *reinterpret_cast<void**>(nullIbp);
        ++claimed;
    }

    {
        static int n = 0;
        if (n < 8) { ++n;
            fprintf(stderr, "[Cemetery] graveyard REGISTERED: comp=%p townMgr=%p "
                            "cemeteryCount=%d plots=%d claimed=%d\n",
                    compV, reinterpret_cast<void*>(townMgr),
                    *reinterpret_cast<int*>(townMgr + 0x48), self->plotCount, claimed);
            fflush(stderr);
        }
    }
    return 0;
}

// CemeteryComponent state-3 "demolish" enter -- 0x140046f80. Byte-identical to the
// slot-7 removal hook (0x1400471c0): wrap the component, call the erase.
extern "C" char CemeteryComponent_EnterDemolish(void* comp, void* /*sm*/) {
    reinterpret_cast<CemeteryComponent*>(comp)->CemeteryComponent::OnRemove();   // B.3.8 bridge (state enters take void*)
    return 0;
}

// ============================================================================
// ENDGAME B.3.8 MI-A — CemeteryComponent as a REAL class (CemeteryComponent.h).
//
// Before: a 48-slot hand-built `CemeteryComponent_LiveVtable` filled at every
// create (FillTypedVtable + FinishLiveVtable), `Inert_ScalarDtor` in slot 0 (only `~Internal`), the
// ControllerLive::Dispatcher<CemeteryCtrlTag> table stamped into raw bytes at +0x28.
// After: the compiler's 18-slot vtable, a real constructor and destructor
// chain, the controller a member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/CemeteryComponent.h"
#include "Component/controller_state_table.h"

using CemeteryCtrlDispatch = ::ControllerLive::Dispatcher<CemeteryCtrlTag>;

// The retail table, transcribed from 0x1400477a0 call for call. Its RegisterState
// instantiation (0x140004250) zero-fills the trans30/trans38/update columns, so every
// handler but Enter is null -- verified in the callee, not assumed from the call site.
// Built ONCE at static-init time, as retail's static initializer does (B38 D6/D7).
static void CemeteryComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    StateTable& t = CemeteryCtrlDispatch::table;
    if (t.count != 0) return;                      // idempotent
    TableRegisterState(t, 2, reinterpret_cast<void*>(&CemeteryComponent_EnterBuilt),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 3, reinterpret_cast<void*>(&CemeteryComponent_EnterDemolish),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 6, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 5, nullptr);
}
static const bool s_cemeteryStateTableBuilt = (CemeteryComponent_PopulateStateTable(), true);

// 0x1400476f0. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// `_controller(this)` is the member's back-pointer; +0x48 = +0x50 = 0; then
// `_flags |= 0x83`.
CemeteryComponent::CemeteryComponent()
    : _controller(this), plots(nullptr), plotCount(0), plotCapacity(0) {
    OrFlagsInCtor(0x83);
}

// [0] 0x140004220: ~CemeteryComponent (0x140047680), MemFree on &1.
Object::Base* CemeteryComponent::DeletingDtor(unsigned flags) {
    this->~CemeteryComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}
// 0x140047680: the grave-plot list (+0x48 data, +0x50 count, +0x54 capacity) is
// freed and zeroed; the member's and the bases' dtors (restamps only) and ~Internal
// are the compiler's chain.
CemeteryComponent::~CemeteryComponent() {
    Object::detail::InternalMemFree(plots);
    plots = nullptr;
    plotCount = 0;
    plotCapacity = 0;
}

// create 0x140004040: MemAlloc(0x58, 8), then the ctor.
void* CemeteryComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CemeteryComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CemeteryComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* CemeteryComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CemeteryComponent());
}

// [3] 0x1400475c0. Slot 3 is the FIRST call into this class on a load-from-save:
// PersistObject creates the object and dispatches vt[0x18] before Startup or any
// tick. Until the resolver runs every s_fn_* is null and the fully null-guarded
// body consumes ZERO bytes, so the stream desyncs several records later. Rule, from
// uicomponent_serialize.cpp: resolve unconditionally at the top of every call.
// Body: Component::Persist (a direct CALL 0x1401189b0), an empty controller scope,
// then the plot tile-index array (count +0x50, resized on a read, bulk ints).
void CemeteryComponent::Persist(void* stream) {
    CemeteryComponent_InitFuncs();
    Component::Persist(stream);
    { IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0); }   // B.3.8 bridge (stream is void* in Internal::Persist)
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);         // B.3.8 bridge
    int count = plotCount;
    ser.Serialize(count, nullptr);
    if (static_cast<int>(ser.GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
        // B.3.2 bridge: exe 0x1400021d0 IS Collection::Array<int32>::SetSize(n), called
        // here at 14004763a. I32NoInit — a grown tail is left as the allocation found
        // it, like retail. Safe: this arm runs only when reading, and the bulk int
        // deserialize below writes all `count` tile indices before any is read.
        GameColl::AsArray<GameColl::I32NoInit>(&plots).SetSize(count);
    }
    ser.Serialize(count, plots, nullptr);
}

// [6] 0x140047200 — build the grave-plot grid from the footprint.
void CemeteryComponent::OnAdded() {
    CemeteryComponent_InitFuncs();
    CEM_TRACE("enter comp=%p owner=%p", (void*)this, _entity);
    if (!s_fn_FindMapComp || !s_fn_FindMultiComp || !s_g_pGameState || !*s_g_pGameState) {
        CEM_TRACE("BAIL deps: findMap=%p findMulti=%p gs=%p",
                  (void*)s_fn_FindMapComp, (void*)s_fn_FindMultiComp,
                  (void*)(s_g_pGameState ? *s_g_pGameState : nullptr));
        return;
    }

    const uintptr_t entity = reinterpret_cast<uintptr_t>(_entity);

    void* mapIbp = nullptr;
    s_fn_FindMapComp(entity, &mapIbp);
    if (!mapIbp) { CEM_TRACE("BAIL no MapComponent"); return; }
    const uintptr_t map = reinterpret_cast<uintptr_t>(mapIbp);

    int x0  = *reinterpret_cast<int16_t*>(map + 0x28);
    int y0  = *reinterpret_cast<int16_t*>(map + 0x2a);
    int w   = *reinterpret_cast<int16_t*>(map + 0x2c);
    int h   = *reinterpret_cast<int16_t*>(map + 0x2e);
    const int rot = *reinterpret_cast<int*>(map + 0x30);

    const uintptr_t world = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*s_g_pGameState) + 0xF8);
    if (!world) { CEM_TRACE("BAIL no world"); return; }
    const int mapWidth = *reinterpret_cast<int*>(world);

    // The gate child: multi->children[0], then that entity's MapComponent.
    uint8_t multiIbp[24] = {};
    s_fn_FindMultiComp(entity, multiIbp);
    const uintptr_t multi = *reinterpret_cast<uintptr_t*>(multiIbp);
    if (!multi) { CEM_TRACE("BAIL no MultiObjectComponent"); return; }
    const uintptr_t childArr = *reinterpret_cast<uintptr_t*>(multi + 0x48);
    if (!childArr) { CEM_TRACE("BAIL no child array"); return; }
    // Retail copies children[0] into a local and calls CheckPtr on THAT COPY
    // (0x1400472a7..0x1400472af) before using it -- the child slot holds a lazy/weak
    // InternalBasePtr, and reading it raw yields an unresolved value. Skipping this
    // step produced a gate 37 tiles outside a 12-wide footprint (door=38), so the two
    // path-in columns were never excluded and every tile became a plot.
    void* gateEnt = *reinterpret_cast<void**>(childArr);
    if (!gateEnt) { CEM_TRACE("BAIL no child[0]"); return; }

    void* gateIbp = nullptr;
    s_fn_FindMapComp(reinterpret_cast<uintptr_t>(gateEnt), &gateIbp);
    if (!gateIbp) { CEM_TRACE("BAIL gate has no MapComponent"); return; }
    // NOT `*(uintptr_t*)gateIbp`. Retail's RBX is the ADDRESS of the out buffer, so its
    // `mov rax,[rbx]` yields the component pointer; `gateIbp` here IS that pointer
    // already. Dereferencing it once more hands back the component's VTABLE, and
    // [vtable+0x28] is a slice of a function pointer -- which read as door=38 on a
    // 12-wide footprint and door=383 on a 6-wide one, so the two path-in columns were
    // never excluded and every tile became a grave plot.
    const uintptr_t gateMap = reinterpret_cast<uintptr_t>(gateIbp);
    if (!gateMap) { CEM_TRACE("BAIL gate map null"); return; }

    // Door offset along the axis the footprint runs on for this rotation.
    const bool rotated = (rot != 0 && rot != 180);
    int door = rotated ? (*reinterpret_cast<int16_t*>(gateMap + 0x2a) - y0)
                       : (*reinterpret_cast<int16_t*>(gateMap + 0x28) - x0);

    // Inset by one tile all round; the door pair moves with it.
    ++door; ++x0; ++y0; w -= 2; h -= 2;
    const int doorPrev = door - 1;

    CEM_TRACE("footprint x0=%d y0=%d w=%d h=%d rot=%d door=%d mapWidth=%d",
              x0, y0, w, h, rot, door, mapWidth);
    if (rotated) {
        // 90 / 270: outer walks the width by twos, inner walks the height.
        for (int col = 0; col < w; col += 2)
            for (int row = 0; row < h; ++row) {
                if (row == door || row == doorPrev) continue;
                CemeteryComponent_PushPlot(this, (row + y0) * mapWidth + col + x0);
            }
    } else {
        // 0 / 180: outer walks the height by twos, inner walks the width.
        for (int row = 0; row < h; row += 2)
            for (int col = 0; col < w; ++col) {
                if (col == door || col == doorPrev) continue;
                CemeteryComponent_PushPlot(this, (row + y0) * mapWidth + col + x0);
            }
    }
    CEM_TRACE("built plots=%d cap=%d", plotCount, plotCapacity);
}

// [7] 0x1400471c0.
void CemeteryComponent::OnRemove() {
    uint8_t ptrBuf[8];
    if (fn_InternalBasePtr_Ctor)
        new (ptrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(_entity));   // B.3.3 bridge

    if (s_g_pGameState && *s_g_pGameState && s_fn_ListAdd) {
        void* gameState = *s_g_pGameState;
        void* subObj = *(void**)((uintptr_t)gameState + 0xF0);
        void* cemeteryListBase = (void*)((uintptr_t)subObj + 0x40);
        s_fn_ListAdd(cemeteryListBase, ptrBuf);
    }
}

DEFINE_EXE_TYPE(CemeteryComponent, "ControllerComponent")
