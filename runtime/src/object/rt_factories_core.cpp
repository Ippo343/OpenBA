// ============================================================================
// runtime/src/object/rt_factories_core.cpp — WAVE 28 LANE FACTREST
//
// The interpreter shared by all 35 non-UI factory pairs, plus the six
// Object:: rows, plus the aggregate registrar.
//
// THE ALLOCATION DOOR, VERIFIED FOR THIS LANE RATHER THAN INHERITED
// -----------------------------------------------------------------
// Every allocating CreateNewObject in this roster inlines the same triple:
//
//     mov  <?_singleton@MemoryAllocator@System@@>, %rbx
//     add  $0x640, %rbx                  ; the allocator's lock object
//     call *[0x18014b390]                ; EnterCriticalSection
//     mov  $N, %edx ; <align>, %r8d
//     mov  <?_singleton@MemoryAllocator@System@@>, %rcx
//     call ?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z   (0x1800c6e60)
//     call *[0x18014b388]                ; LeaveCriticalSection
//
// `?MemAlloc@System@@YAPEAX_KH@Z` (0x1800c7a50) is that same triple out of
// line. This lane disassembled 0x1800c7a50 and compared it instruction by
// instruction with the inlined copy at 0x180001144-0x18000117c rather than
// taking Wave 26's word for it (reference_a_written_recipe_can_be_wrong.md):
// same singleton slot, same +0x640 lock, same two IAT slots in the same order,
// same callee. So calling the wrapper reproduces the inlined sequence exactly,
// including the lock order.
//
// ATTENDANCE IS NOT VERIFICATION
// ------------------------------
// The counters below will read 0 in any run with the DLL present. See the
// header banner. A zero here is the EXPECTED reading and is not evidence that
// any body is correct (reference_a_green_readiness_screen_is_not_a_route.md).
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "rt_factories.h"

// ResolveAddr lives in standalone/resolve.h, which drags in <windows.h> and so
// must not be included from runtime/src (rt_port.h explains why). Declared here
// exactly as ~30 sibling runtime files already declare it.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module m, uintptr_t ghidra_addr);

namespace {

uint64_t g_rows        = 0;   // classes registered
uint64_t g_newTotal    = 0;   // CreateNewObject entries across the roster
uint64_t g_inPlTotal   = 0;   // CreateInPlaceObject entries
uint64_t g_allocFailed = 0;   // retail's allocation-failure branch taken
uint64_t g_ctorMissing = 0;   // an out-of-line ctor name did not resolve
uint64_t g_inPlaceShared = 0; // slot-1 symbols already registered by another row

// ---- `?MemAlloc@System@@YAPEAX_KH@Z` 0x1800c7a50 ---------------------------
// BY ADDRESS on purpose, and the address is the LOCKED wrapper, not
// `?Allocate@` (0x1800c6e60) which retail calls with the lock already held.
// `ResolveAddr(Module::Runtime, ...)` consults the port registry FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so this follows the
// tree — including the allocator port behind the old DLL-era switch — without this
// file changing. Re-resolve on a miss rather than freezing a null.
void* MemAllocLocked(uint64_t size, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(size, align) : nullptr;
}

// One resolve per DISTINCT name, retried on a miss. The tables hold string
// literals, so pointer identity is not usable as a key; the roster is small
// (98 distinct symbols) and these run once per object creation, not per frame.
void* ResolveDataSlot(const char* mangled) {
    return ResolveExport(mangled);
}

void RunSteps(void* self, const RtCtorStep* s, const RtFactoryClass& c) {
    uint8_t* base = static_cast<uint8_t*>(self);
    for (; s->kind != RtFactOp::End; ++s) {
        switch (s->kind) {
        case RtFactOp::Vptr: {
            // The vtable EXPORT, through the port-aware door, so a promoted
            // (minted) table wins over the DLL's at cutover.
            void* vt = ResolveDataSlot(s->name);
            if (!vt) {
                fprintf(stderr, "[RtFactory] FATAL %s: vtable %s did not resolve "
                                "(step at retail 0x%llx)\n",
                        c.typeName, s->name, (unsigned long long)s->va);
                fflush(stderr);
            }
            std::memcpy(base + s->off, &vt, sizeof(void*));
            break;
        }
        case RtFactOp::Imm: {
            const uint64_t v = s->imm;
            std::memcpy(base + s->off, &v, s->size);   // little-endian, low bytes
            break;
        }
        case RtFactOp::Self: {
            void* p = base + s->imm;
            std::memcpy(base + s->off, &p, sizeof(void*));
            break;
        }
        case RtFactOp::Call: {
            using CtorFn = void* (*)(void*);
            CtorFn fn = reinterpret_cast<CtorFn>(ResolveExport(s->name));
            if (!fn) {
                // NOT a guard over a faithful crash: retail calls this
                // unconditionally and always reaches a real body. This says, in
                // the shape the mint's own per-slot FATAL uses, that the
                // CONSTRUCTOR behind a ported FACTORY is itself unported — the
                // named residual in WAVE28_LANE_FACTREST.md §5.
                ++g_ctorMissing;
                fprintf(stderr, "[RtFactory] FATAL %s: constructor %s is not "
                                "ported and the DLL did not supply it "
                                "(retail call at 0x%llx)\n",
                        c.typeName, s->name, (unsigned long long)s->va);
                fflush(stderr);
                return;
            }
            fn(self);
            break;
        }
        default:
            break;
        }
    }
}

}  // namespace

// ============================================================================
// The two shapes
// ============================================================================
//
//     Base* C::CreateNewObject()
//     { if (!steps) return nullptr;                       // the (a) stub
//       void* p = System::MemAlloc(N, 8);
//       if (!p) return nullptr;                           // RETAIL's branch
//       <inlined ctor>;  return p; }
//
void* RtFactoryNew(const RtFactoryClass& c) {
    if (!c.newSteps) return nullptr;             // retail: `xor eax,eax ; ret`
    void* p = MemAllocLocked(c.allocSize, static_cast<int>(c.allocAlign));
    ++c.counters[0];
    ++g_newTotal;
    if (!p) { ++c.counters[2]; ++g_allocFailed; return nullptr; }
    RunSteps(p, c.newSteps, c);
    return p;
}

//     Base* C::CreateInPlaceObject(void* p)
//     { if (!steps) return nullptr;
//       if (!p) return nullptr;                           // RETAIL's branch
//       <inlined ctor>;  return p; }
//
void* RtFactoryInPlace(const RtFactoryClass& c, void* p) {
    if (!c.inPlaceSteps) return nullptr;         // retail: `xor eax,eax ; ret`
    ++c.counters[1];
    ++g_inPlTotal;
    if (!p) { ++c.counters[2]; return nullptr; }
    RunSteps(p, c.inPlaceSteps, c);
    return p;
}

// ============================================================================
// Registration — BOTH doors for every slot
// ============================================================================
// The mint tries the NAME first and the ADDRESS second. A name-only
// registration leaves any by-address consumer on the DLL; an address-only one
// is invisible to tools/runtime_census.py
// (reference_by_address_door_dies_at_cutover.md).
// WAVE 29 LANE VERIFY — the roster, kept so something can VERIFY it.
//
// Each namespace's `kRegs` is file-static inside its own .inc, so until now the
// only consumer of a row was the registration loop below and NOTHING COULD
// ENUMERATE THE FAMILY at runtime. That is why the family's only evidence was an
// OFFLINE oracle that re-implements the interpreter instead of calling it.
// A flat array of pointers to the caller's static tables; no ownership, no copy.
namespace {
const RtFactoryReg* g_roster[256];
size_t              g_rosterN = 0;
}  // namespace
size_t RtFactoryRoster(const RtFactoryReg** out, size_t cap) {
    size_t n = g_rosterN < cap ? g_rosterN : cap;
    for (size_t i = 0; i < n; ++i) out[i] = g_roster[i];
    return g_rosterN;   // the TRUE size, so a caller can detect truncation
}

int RtFactoryRegisterAll(const RtFactoryReg* regs, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const RtFactoryReg& r = regs[i];
        if (g_rosterN < sizeof(g_roster)/sizeof(g_roster[0]))
            g_roster[g_rosterN++] = &regs[i];
        RegisterRuntimeExport(r.newName, r.newFn);
        RegisterRuntimeAddr(static_cast<uintptr_t>(r.newVa), r.newFn);

        // ONE slot-1 symbol is shared by FIVE minted rows, and registering it
        // per row would SHADOW: `?CreateInPlaceObject@Internal@Object@@`
        // (0x1800070c0) is the in-place factory of Internal, AchievementInstance
        // and GoalInstance here, and of DialogInstance and Element in lanes
        // FACTUI1/FACTUI2. The bodies are identical (retail is `xor eax,eax ;
        // ret`), so a shadow would be harmless — and invisible, which is the
        // objection. Register the FIRST claimant and skip the rest, so the
        // registry holds one binding per symbol and a later reader is not
        // looking at dead code (docs/waves/unregistered_slots_wave.md).
        if (!RuntimeExportIsRegistered(r.inPlaceName)) {
            RegisterRuntimeExport(r.inPlaceName, r.inPlaceFn);
            RegisterRuntimeAddr(static_cast<uintptr_t>(r.inPlaceVa), r.inPlaceFn);
        } else {
            ++g_inPlaceShared;
        }
    }
    g_rows += n;
    return static_cast<int>(n);
}

// ============================================================================
// Object:: — six classes
// ============================================================================
#include "rt_factories_object.inc"

int RtFactoriesObjectRegister() {
    return RtFactoryRegisterAll(kRegs, sizeof(kRegs) / sizeof(kRegs[0]));
}

// ============================================================================
// The aggregate init. Called from RuntimePortInit() BEFORE RtDataMintInit().
// ============================================================================
//
void RtFactoriesPortInit() {
    int n = 0;
    n += RtFactoriesObjectRegister();
    n += RtFactoriesGraphicsRegister();
    n += RtFactoriesVideoRegister();
    // IO:: has no row left: ENDGAME B.3.9 IO made IO::PackageFile a real class
    // whose own CreateNewObject / CreateInPlaceObject register from
    // runtime/src/io/package_file.cpp (B39_RULES D2).
    // Picking:: has no row left: ENDGAME B.3.9's pilot made Picking::PickingMesh
    // a real class whose own CreateNewObject / CreateInPlaceObject register from
    // runtime/src/picking/pickingmesh.cpp (B39_RULES D2).
    printf("[RtFactory] FACTREST registered %d non-UI factory classes, "
           "%d distinct slots, both doors each (%llu slot-1 symbols were already "
           "bound by an earlier row and were NOT re-bound)\n",
           n, n * 2 - static_cast<int>(g_inPlaceShared),
           (unsigned long long)g_inPlaceShared);
}

namespace {
// The attendance line needs no call site: it prints at process exit, and it
// prints even when every counter is zero. A zero that is never printed cannot
// be read, and a zero that is printed WITHOUT its verdict line gets read as
// "verified" (reference_a_green_readiness_screen_is_not_a_route.md).
struct FactoriesReport { ~FactoriesReport() { if (g_rows) RtFactoriesSummary(); } };
FactoriesReport g_factoriesReport;
}  // namespace

void RtFactoriesSummary() {
    printf("[RtFactory] FACTREST ATTENDANCE: classes=%llu CreateNewObject=%llu "
           "CreateInPlaceObject=%llu allocFail=%llu ctorMissing=%llu\n",
           (unsigned long long)g_rows, (unsigned long long)g_newTotal,
           (unsigned long long)g_inPlTotal, (unsigned long long)g_allocFailed,
           (unsigned long long)g_ctorMissing);
    if (g_newTotal == 0 && g_inPlTotal == 0) {
        printf("[RtFactory] FACTREST VERDICT: nothing in this run reached any of "
               "these bodies through a door. EXPECTED with the DLL present "
               "(Wave 11 lane OBJINT) and NOT evidence any body is correct.\n");
    }
    fflush(stdout);
}
