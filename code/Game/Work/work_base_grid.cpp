// code/Game/Work/work_base_grid.cpp
// Shared Work grid / work-assignment helpers (hand-ported + Ghidra-verified).
//
// Resolved via ResolveAddr(Module::Exe, 0xADDR) from many code/Game/Work/*.cpp; registered
// via RegisterWorkGridBatch in standalone/reg_work_grid.cpp. Faithful 1:1 ports.
//
// NOTE: this replaces an earlier parallel-agent draft that was NOT faithful (the draft is
// kept under notes/wave2_drafts/work_base_grid.cpp.draft). Each function here was verified
// against the Ghidra decompile.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// FUN_14009ccf0 — WorldGrid_FindCell
// ============================================================
// Scan the 3x3 block around srcCell for the passable cell nearest destCell. The original
// iterates with the COLUMN tied to the outer loop and the ROW tied to the inner loop (a
// transposed traversal); ties resolve to the first-visited minimum, so the exact order is
// preserved here. Cell array at grid+0x80, stride 0x30, pass byte +0x29.
// Caller: int fn(int* grid, int srcCell, int destCell, uint32_t passFlags)
int WorkGrid_FindCell_9ccf0(int* mapGrid, int srcCell, int destCell, uint32_t passFlags) {
    int gridW = *mapGrid;
    int best = -1;
    int bestDist = 0;
    uintptr_t cellArr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(mapGrid) + 0x80);

    int col = srcCell % gridW - 1;                 // iVar9, advances with the outer loop
    for (int rowD = -1; rowD <= 1; rowD++, col++) {
        int rowBase = (srcCell / gridW - 1) * gridW;   // iVar6, advances with the inner loop
        for (int colD = -1; colD < 2; colD++, rowBase += gridW) {
            if (!(rowD != 0 || colD != 0)) continue;   // skip srcCell itself
            if (col < 0) continue;
            int row = colD + srcCell / gridW;
            if (row < 0 || col >= gridW || row >= gridW) continue;
            int cand = col + rowBase;
            uint8_t passByte = *reinterpret_cast<const uint8_t*>(cellArr + static_cast<uintptr_t>(cand) * 0x30 + 0x29);
            if (!(passFlags & (1u << (passByte & 0x1f)))) continue;
            int dRow = cand / gridW - destCell / gridW;
            int dCol = cand % gridW - destCell % gridW;
            int dist = dRow * dRow + dCol * dCol;
            if (best < 0 || dist < bestDist) { best = cand; bestDist = dist; }
        }
    }
    return best;
}

// ============================================================
// FUN_1400f8620 — TryAssignWorkplace
// ============================================================
// Look up cellId in the assignment BST at mgr+0x30 (cell-BST node: key@+0, IBP@+8,
// left@+0x18, right@+0x20). If the matched entry's stored generation (obj+0x2c) already
// equals the manager generation (mgr+8), it's a no-op. Otherwise fire vtable +0x48
// (notify) and +0x20 (CanAssign(citizen, mgr+0x130)); 2 = accepted; 1 = assignable -> when
// `flag`, fire +0x48 then +0x28 (Assign); on success return low-byte 1, else stamp the
// generation. Caller: uint64_t fn(uintptr_t mgr, void* citizenRef, int cellId, char flag)
uint64_t WorkGrid_TryAssignWorkplace_f8620(uintptr_t mgr, void* citizenRef, int cellId, char flag) {
    int* node = *reinterpret_cast<int**>(mgr + 0x30);
    for (;;) {
        for (;;) {
            if (node == nullptr) return 0;
            if (*node <= cellId) break;
            node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x18);   // left
        }
        if (cellId <= *node) break;
        node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x20);       // right
    }
    if (node == nullptr) return 0;

    uintptr_t* self = reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(node) + 0x08);   // IBP
    uint32_t mgrGen = *reinterpret_cast<uint32_t*>(mgr + 8);
    if (*reinterpret_cast<uint32_t*>(*self + 0x2c) == mgrGen)
        return static_cast<uint64_t>(mgrGen & 0xffffff00u);

    uintptr_t obj = *self;
    (*reinterpret_cast<void(**)(void*)>(*reinterpret_cast<uintptr_t*>(obj) + 0x48))(reinterpret_cast<void*>(obj));  // notify

    uintptr_t citizenCopy = *reinterpret_cast<uintptr_t*>(citizenRef);
    obj = *self;
    uint64_t uVar2 = (*reinterpret_cast<uint64_t(**)(void*, void*, void*)>(*reinterpret_cast<uintptr_t*>(obj) + 0x20))(
        reinterpret_cast<void*>(obj), &citizenCopy, reinterpret_cast<void*>(mgr + 0x130));   // CanAssign

    if (static_cast<int>(uVar2) == 2)
        return (uVar2 & 0xffffffffffffff00ull) | 1;

    if (static_cast<int>(uVar2) == 1) {
        if (flag != '\0') {
            obj = *self;
            (*reinterpret_cast<void(**)(void*)>(*reinterpret_cast<uintptr_t*>(obj) + 0x48))(reinterpret_cast<void*>(obj));  // notify
            citizenCopy = *reinterpret_cast<uintptr_t*>(citizenRef);
            obj = *self;
            uint64_t assignR = (*reinterpret_cast<uint64_t(**)(void*, void*, void*)>(*reinterpret_cast<uintptr_t*>(obj) + 0x28))(
                reinterpret_cast<void*>(obj), &citizenCopy, reinterpret_cast<void*>(mgr + 0x130));   // Assign
            if (static_cast<char>(assignR) != '\0')
                return (assignR & 0xffffffffffffff00ull) | 1;
        }
        uVar2 = mgrGen;
        *reinterpret_cast<uint32_t*>(*self + 0x2c) = mgrGen;
    }
    return uVar2 & 0xffffffffffffff00ull;
}

// ============================================================
// FUN_1400e3db0 — AutoPickup_CheckAssignments
// ============================================================
// Find the AutoPickupComponent on self's entity (self+0x10) via FUN_140022520. If active
// (+0x28), walk its cell-index list (base +0x30, count +0x38): for each cell, look it up in
// the game-world assignment BST (*g_pGameWorldSingleton + 0x118, root +0x30). If found, try
// to assign a citizen (TryAssignWorkplace) — return 2 on success, else keep scanning; if NOT
// found, compact the cell out of the list and re-examine the shifted-in slot.
// Caller: int fn(uintptr_t self, void* citizenRef)
typedef void (*fn_FindAutoPickup_t)(uintptr_t entity, void* outIBP);
static fn_FindAutoPickup_t s_fn_FindAutoPickup = nullptr;
static bool s_findAutoPickupInit = false;

int WorkGrid_AutoPickup_e3db0(uintptr_t self, void* citizenRef) {
    if (!s_findAutoPickupInit) {
        s_fn_FindAutoPickup = reinterpret_cast<fn_FindAutoPickup_t>(ResolveAddr(Module::Exe, 0x140022520));
        s_findAutoPickupInit = true;
    }
    uintptr_t apComp = 0;
    if (s_fn_FindAutoPickup) s_fn_FindAutoPickup(*reinterpret_cast<uintptr_t*>(self + 0x10), &apComp);
    if (apComp == 0 || *reinterpret_cast<char*>(apComp + 0x28) == '\0') return 0;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 0;
    uintptr_t mgr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x118);

    int i = 0;
    if (*reinterpret_cast<int*>(apComp + 0x38) <= 0) return 0;

    intptr_t off = 0;   // i * 4
    do {
        int* node = *reinterpret_cast<int**>(mgr + 0x30);
        int cellId = *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(apComp + 0x30) + off);
        bool keepElement = false;
        while (node != nullptr) {
            if (cellId < *node) {
                node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x18);   // left
            } else {
                if (cellId <= *node) {   // found
                    uintptr_t citizenCopy = *reinterpret_cast<uintptr_t*>(citizenRef);
                    uint64_t r = fn_TryAssignWorkplace ? fn_TryAssignWorkplace(mgr, &citizenCopy, cellId, '\x01') : 0;
                    if (static_cast<char>(r) != '\0') return 2;
                    keepElement = true;
                    break;
                }
                node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x20);   // right
            }
        }
        if (!keepElement) {
            // not found in BST -> compact arr[i] out of the list
            int count = *reinterpret_cast<int*>(apComp + 0x38);
            intptr_t shiftOff = off;
            int j = i;
            while (j < count) {
                j++;
                if (j < count) {
                    int* p = reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(apComp + 0x30) + shiftOff);
                    p[0] = p[1];
                }
                shiftOff += 4;
            }
            *reinterpret_cast<int*>(apComp + 0x38) = count - 1;
            i -= 1;
            off -= 4;
        }
        i += 1;
        off += 4;
    } while (i < *reinterpret_cast<int*>(apComp + 0x38));

    return 0;
}
