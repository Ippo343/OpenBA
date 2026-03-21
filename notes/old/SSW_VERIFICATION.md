# ShouldSwitchWorkplace Verification Report

**Date**: 2026-03-20
**Reimpl file**: `code/Game/Component/citizencomponent.cpp` lines 104-288
**Original address**: 0x1401109E0 (`CitizenComponent_helper_09e0`)

## Summary

The reimplementation is **structurally correct** and faithfully reproduces the original logic. No critical bugs were found. Several minor observations are documented below.

---

## Area-by-Area Comparison

### A. Unassigned Path (workplaceFlag==0) - Manual Component Scanning Loop

**MATCH** - The original manually iterates components:
- Reads entity+0x30 (component array pointer) and entity+0x38 (component count)
- For each component: calls CheckPtr on the IBP slot, reads the component pointer, calls vtable+0x10 (GetTypeId)
- Compares against `DWORD_1401c80e0` (WorkplaceComponent type ID)
- On match: re-reads the IBP slot, calls CheckPtr, calls `fn_IsDerivedFrom`, stores result

The reimpl at lines 218-243 reproduces this exactly. The type ID source `g_TypeId_WorkplaceComponent` maps to address `0x1401c80e0`, confirmed in disasm at `140110d24: MOV EDI,dword ptr [0x1401c80e0]`.

The original uses two loop counters (ESI and R14) that increment in lockstep -- the reimpl uses a single counter `ci`, which is functionally identical.

### B. StorageLocationComponent Field Offsets

**MATCH**:
- **+0x28 for region ID**: Confirmed in disasm at `140110a96: MOV EDX,dword ptr [RAX + 0x28]` (assigned path) and `140110bfc: MOV EDX,dword ptr [RAX + 0x28]` (assigned loop). Reimpl reads `*(int32_t*)(storageLocComp + 0x28)` -- correct.
- **+0x30 for worker slot cache**: Confirmed at `140110dba: CMP dword ptr [RBX + 0x30],R13D` and `140110dc8: MOV dword ptr [RBX + 0x30],EAX`. Reimpl reads/writes `*(int32_t*)(slc + 0x30)` -- correct.
- **Lazy-compute guard (==0)**: Original compares with R13D (which is 0). Reimpl checks `== 0` -- correct.

### C. Worker Slot Calculation

**MATCH**:
- `fn_CalcWorkerSlots` maps to `FUN_14012e5c0` -- confirmed at disasm `140110dc3: CALL 0x14012e5c0`.
- Parent entity retrieved from WorkplaceComponent+0x10: confirmed at `140110d98: CALL qword ptr [0x140152ab0]` (CheckPtr on wpBase+0x10) then `140110d9e: MOV RCX,qword ptr [RBX + 0x10]`.
- The reimpl at lines 261-264 does `CheckPtr(wpBase + 0x10)` then reads `*(wpBase + 0x10)` -- matches.

### D. Gap Calculation

**MATCH**:
- **minGap initialized to 0xFFFF**: `140110c92: MOV EAX,0xffff` -- reimpl uses `uint32_t minGap = 0xFFFF` -- correct.
- **maxGap initialized to 0**: `140110c8f: MOV R12D,R13D` (R13D=0) -- reimpl uses `int maxGap = 0` -- correct.
- **Comparisons are signed**: `CMOVG` for maxGap update, `CMOVL` for minGap update. The reimpl uses `>` and `<` with appropriate casts to `(int)` -- correct.
- **Final threshold**: `140110e38: SUB R12D,EAX` then `140110e3b: CMP R12D,0x1` then `SETG AL`. This is `maxGap - minGap > 1` (signed greater). Reimpl returns `(maxGap - (int)minGap) > 1` -- correct.

### E. Assigned Path Region Lookup

**MATCH with intentional fix**:
- **Region BST structure**: Left child at +0x18, right child at +0x20, value at +0x8, key at +0x0. Confirmed in disasm for both the initial region lookup (0x140110a99-0x140110b15) and the inner loop lookup (0x140110c06-0x140110c8d).
- **Region priority at +0x24**: `140110ae3: MOV EDI,dword ptr [RAX + 0x24]` and `140110c49: CMP dword ptr [RAX + 0x24],EDI`. Reimpl reads `*(int32_t*)(regionDesc + 0x24)` -- correct.
- **Null-check fix**: The original has a genuine null-deref bug. When the region BST lookup fails, it calls `fn_InternalBasePtr_Ctor(&local, NULL)`, then CheckPtr on the null IBP, then reads `*(int*)(NULL + 0x24)` which crashes. The reimpl's `return false` on null is a safe fix. **Severity: original game bug, reimpl correctly guards against it.**
- The same null-check pattern is applied in the inner loop at line 194 with `continue` instead of `return false` -- this is also correct and safe.

### F. WorkplaceComponent Field Offsets

**MATCH**:
- **+0x50 for active flag**: `140110bcd: CMP byte ptr [RAX + 0x50],R13B` (assigned loop) and `140110d60: CMP byte ptr [RAX + 0x50],R13B` (unassigned loop). Reimpl checks `*(uint8_t*)(wpComp + 0x50)` -- correct.
- **+0x60 for current worker count**: `140110ddc: SUB EBX,dword ptr [RAX + 0x60]`. Reimpl reads `*(int32_t*)(wpIBP + 0x60)` -- correct.

### G. Assigned Path: FindWorkplaceComp / FindStorageLocationComp

**MATCH**:
- `fn_FindWorkplaceComp` = `FUN_140002e10` (0x140002e10) -- called at `140110b85`
- `fn_FindStorageLocationComp` = `FUN_140003050` (0x140003050) -- called at `140110b9c`
- `fn_CitizenComponent_HasVacancy` = `CitizenComponent_helper_09e0_sub_7050` (0x140017050) -- called at `140110be5` and `140110c5c`
- Order of calls and CheckPtr invocations between them matches the original.

### H. Profession Registry BST

**MATCH**: The reimpl's `LookupProfessionJobCount` and `GetJobEntity` helper functions correctly reproduce the BST traversal. Node layout: key at [0], array_ptr at [1], count at [2], left at [4], right at [5] (using 8-byte pointer offsets). The original at `140110a20-140110a40` traverses with `[RDX]` as key, `[RDX+0x20]` as left, `[RDX+0x28]` as right, `[RDX+0x10]` as count -- which matches offsets 0, 0x20, 0x28, 0x10 = indices 0, 4, 5, 2 in a `uintptr_t[]`.

---

## Minor Observations (non-critical)

1. **Cached profKey vs re-read**: The original re-reads `*(param_1 + 0x1f0)` from the citizen struct on every loop iteration (both paths). The reimpl caches it once in `profKey` at line 110 and reuses. This is functionally safe (the value won't change during this synchronous function) but diverges from the original's defensive pattern. **Severity: cosmetic, no behavioral impact.**

2. **GetJobEntity re-traverses BST each iteration**: The reimpl's `GetJobEntity()` does a full BST lookup per iteration, matching the original. Both could theoretically cache the node pointer, but the reimpl correctly mirrors the original's pattern.

3. **fn_FindStorageLocationComp return value**: The reimpl stores the return value in `storageLocResult` (a `void*`) and later calls CheckPtr on it and dereferences it. The original saves RAX (return value) to RBX and does the same. The return value is a pointer to the IBP (either the passed param_2 or an internal IBP). This is correct.

4. **CONCAT44 artifacts**: The Ghidra decompilation shows `CONCAT44(uStackX_14, local_res10)` in several places, which is a decompiler artifact for reading a 64-bit value from a 32-bit-aligned stack slot. The reimpl correctly treats these as 64-bit pointer values.

---

## Conclusion

**No critical bugs found.** The reimplementation is a faithful reproduction of the original with two intentional improvements (null-check guards for region BST lookups that would crash in the original). All field offsets, type IDs, BST traversal logic, comparison operators, and function call sequences match the original disassembly.
