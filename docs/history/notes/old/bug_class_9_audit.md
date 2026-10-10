# Bug Class 9 Audit -- Shared Functions Across Vtables

Date: 2026-03-27

## Summary
- Total hooked functions checked: 741
- Functions with multiple vtable refs (different base vtables): 7 unique functions
- Already known/fixed (skip list): 6 entries
- Already disabled: 1 (ComponentEntity::Slot3 / _purecall)
- NEEDS CONVERSION (prologue hook on shared function): 2 high-priority
- LOW PRIORITY (safe due to base-class semantics): 5

## Methodology
All 741 addresses from `hook_addresses.txt` were batch-queried via Ghidra MCP
`get_bulk_xrefs` (19 batches of ~40 addresses). DATA xrefs in the .rdata vtable
range (0x140150000-0x140165000) were collected. Functions with DATA xrefs from 2+
distinct vtable base addresses were flagged.

---

## Findings

### HIGH PRIORITY (prologue hook on shared function -- type-specific reimpl)

#### CropUI_Slot8 / OrchardUI_OnRemove (0x140078D30)
- **Hooked by**: `cropui.cpp` (InstallHook, prologue=13 bytes)
- **Reimpl type**: DEEP -- calls `CropUI_Slot8((CropUI*)self)` which accesses `self->internalBasePtr` (CropUI-specific field)
- **Vtable refs**:
  - 0x14015d370 = CropUI vtable (0x14015d330) + 0x40 (slot 8)
  - 0x14015cf70 = OrchardUI vtable (0x14015cf30) + 0x40 (slot 8)
  - 0x14015f650 = PastureUI vtable (0x14015f610) + 0x40 (slot 8)
  - 0x14015f2b0 = BuildUI vtable (0x14015f210) + 0xA0 (slot different offset -- verify)
- **Risk**: HIGH. The reimpl hard-casts to `CropUI*` and reads CropUI-specific field layout. When called for OrchardUI, PastureUI, or BuildUI objects, the field at that offset may hold different data, causing silent corruption or wrong behavior.
- **Recommendation**: Convert to `InstallHookVTable` targeting only CropUI vtable (0x14015d330, slot 8). Other classes keep the original function. OR: verify all SelectResourceUI subclasses have identical layout at the accessed offsets and rename to a shared base function.

#### SelectResourceUI_Slot9 (0x140078D80)
- **Hooked by**: `cropui.cpp` (InstallHook, prologue=13 bytes)
- **Reimpl type**: DEEP -- calls `SelectResourceUI_Slot9(self, ...)` which accesses offset +0x30 (combo element pointer)
- **Vtable refs**:
  - 0x14015d378 = CropUI vtable (0x14015d330) + 0x48 (slot 9)
  - 0x14015cf78 = OrchardUI vtable (0x14015cf30) + 0x48 (slot 9)
  - 0x14015f2b8 = BuildUI vtable (0x14015f210) + 0xA8 (slot -- verify offset)
- **Risk**: MEDIUM-HIGH. Accesses +0x30 on self and dispatches through vtable[11]. If all SelectResourceUI subclasses share the same base layout (SelectResourceUI is the common base), +0x30 may be a shared field. But note the comment in code says "shared by CropUI and OrchardUI" -- BuildUI presence is unexpected.
- **Recommendation**: Convert to `InstallHookVTable` for CropUI vtable only, OR verify BuildUI has compatible layout at +0x30.

---

### ALREADY DISABLED

#### _purecall / ComponentEntity::Slot3 (0x14003addc)
- **Hooked by**: `componententity.cpp` (InstallHook, prologue=17 bytes)
- **Status**: DISABLED in hooks.cpp (in the "Remaining Disabled Hooks" list)
- **Vtable refs**: 49+ DATA refs across 11+ distinct vtable base addresses
  - ComponentEntity, all Description types, all Tool types, all Goal types, all Tutorial classes, Pickup/Removal classes, etc.
- **Analysis**: This is the MSVC `_purecall` stub (0x14003addc). It serves as the pure virtual placeholder in every abstract class vtable. Prologue hooking it would intercept ALL pure virtual call sites across the entire binary -- catastrophic corruption. Correctly identified and disabled.

---

### ALREADY KNOWN/FIXED (from skip list)

These were identified and fixed prior to this audit:
- TutorialGoalDialog: 5 slots at vtable 0x140160870
- BuildComponent::Draw (vtable 0x140162260)
- NomadComponent::OnCreate (vtable 0x14015a020)
- ClearStorage::Slot10 (vtable 0x1401611a0)
- Pickup::Slot10 (vtable 0x140161810)
- RemoveAnimal::Purecall (vtable 0x14015ffe0)

---

### LOW PRIORITY (shared but safe -- base class semantics)

#### BridgeTool_Draw / DragTool::OnKey (0x1400fc6b0)
- **Hooked by**: `dragtool.cpp` (InstallHook, prologue=13 bytes)
- **Reimpl type**: DEEP -- calls `DragTool_OnKey((DragTool*)self, key)`
- **Vtable refs** (4 vtables, all at slot 6 = +0x30):
  - 0x140161b30 = BridgeTool vtable (0x140161b00) + 0x30
  - 0x140161bc0 = DragPlacementTool vtable (0x140161b90) + 0x30
  - 0x140161c40 = DragTool vtable (0x140161c10) + 0x30
  - 0x140161e10 = RemovalTool vtable (0x140161de0) + 0x30
- **Risk**: LOW. DragTool is the abstract base class for all these tool types. The reimpl only accesses DragTool-level fields (0x00-0x38) and dispatches subclass behavior through virtual method calls (e.g., BuildPath). This is correct base-class inheritance -- all subclasses are intended to use the same OnKey implementation.
- **Recommendation**: No conversion needed. Document that this is intentionally shared base class behavior.

#### MessageDialog Slot10-13 (Button Handlers)

**Slot10 ButtonOk (0x14006CFF0)**
- **Hooked by**: `messagedialog.cpp` (InstallHook, prologue=16 bytes)
- **Vtable refs**:
  - 0x14015c4d0 = MessageDialog vtable (0x14015c480) + 0x50
  - 0x14015ce90 = OptionInputBindDialog vtable (0x14015ce40) + 0x50

**Slot11 ButtonCancel (0x14006D030)**
- **Vtable refs**: 0x14015c4d8 (MessageDialog+0x58), 0x14015ce98 (OptionInputBindDialog+0x58)

**Slot12 ButtonYes (0x14006CFB0)**
- **Vtable refs**: 0x14015c4e0 (MessageDialog+0x60), 0x14015cea0 (OptionInputBindDialog+0x60)

**Slot13 ButtonNo (0x14006CF70)**
- **Vtable refs**: 0x14015c4e8 (MessageDialog+0x68), 0x14015cea8 (OptionInputBindDialog+0x68)

- **Reimpl type**: DEEP -- `MessageDialog_SetResultAndClose(self, N)` which writes to +0x24 (result) and reads +0x10 (parent IBP)
- **Risk**: LOW. Both MessageDialog and OptionInputBindDialog extend DialogInstance (0xB8 base). The accessed offsets +0x24 and +0x10 are DialogInstance base fields, identical in both classes. The reimpl is functionally correct for both callers.
- **Recommendation**: No conversion needed immediately. However, the `g_lastHook` breadcrumb will say "MessageDialog" when called from OptionInputBindDialog -- consider adding a comment documenting this shared usage for future maintainers.

#### ClimateGenerator Slot (0x140018f60)
- **Hooked by**: `climategenerator.cpp` (InstallHook, prologue=19 bytes)
- **Vtable refs**: 0x14015e190 and 0x14015e140
  - Both are within 0x50 of each other -- this is the SAME vtable (ClimateGenerator vtable at ~0x14015e140)
  - The function appears at two different slots within the same vtable (common for Get/Set pairs or multi-interface)
- **Risk**: NONE. Same vtable, not a cross-class sharing issue.
- **Recommendation**: No action needed.

---

## Summary Table

| Address | Function | Reimpl File | Shared With | Risk | Action |
|---------|----------|-------------|-------------|------|--------|
| 0x140078D30 | CropUI_Slot8 | cropui.cpp | OrchardUI, PastureUI, BuildUI | **HIGH** | Convert to vtable patch |
| 0x140078D80 | SelectResourceUI_Slot9 | cropui.cpp | OrchardUI, BuildUI | **MED-HIGH** | Convert to vtable patch |
| 0x14003addc | _purecall | componententity.cpp | ~30 classes | N/A | Already disabled |
| 0x1400fc6b0 | DragTool::OnKey | dragtool.cpp | BridgeTool, DragPlacement, Removal | Low | Safe (base class) |
| 0x14006CFF0 | MessageDialog::Slot10 | messagedialog.cpp | OptionInputBindDialog | Low | Safe (shared base fields) |
| 0x14006D030 | MessageDialog::Slot11 | messagedialog.cpp | OptionInputBindDialog | Low | Safe (shared base fields) |
| 0x14006CFB0 | MessageDialog::Slot12 | messagedialog.cpp | OptionInputBindDialog | Low | Safe (shared base fields) |
| 0x14006CF70 | MessageDialog::Slot13 | messagedialog.cpp | OptionInputBindDialog | Low | Safe (shared base fields) |
| 0x140018f60 | ClimateGenerator | climategenerator.cpp | (same vtable) | None | False positive |

## Recommended Actions (Priority Order)
1. **Convert CropUI_Slot8 (0x140078D30)** to `InstallHookVTable(CropUI_vtable, 8)` -- type-specific field access on shared function
2. **Convert SelectResourceUI_Slot9 (0x140078D80)** to `InstallHookVTable(CropUI_vtable, 9)` -- or verify all subclass layouts match
3. Document DragTool::OnKey and MessageDialog button handlers as intentionally shared base class methods
