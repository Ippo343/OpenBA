# Dialog Module — Reimplementation Goals

**Directory:** `code/Game/Dialog/`
**Source files:** 57 .cpp + 3 .h files
**Hook status:** All hookable slots reimplemented (zero trampoline passthroughs in dialog/UI classes)

---

## Standalone Exe Gap Analysis

For the clean exe, every vtable slot needs a C++ implementation — even slots too small to hook.
This audit lists every unimplemented slot across all dialog/UI classes.

### Legend
- **Reimplemented** — clean C++ in `code/Game/Dialog/`, called from `engine/reimpl/` Reimpl wrapper
- **Passthrough** — hooked but still calls trampoline (needs decompile + reimpl)
- **Unhookable** — too small / rel branch / thunk (never hooked, needs reimpl for standalone)
- **Disabled hook** — hook exists but disabled in hooks.cpp (needs fix or reimpl)

---

## 1. Dialog Controller Classes (22 classes)

### AutoSaveDialog (0xD0 bytes, vtable 0x14015b120)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x1400106B0 | 8b | DtorVec | Unhookable |
| 2 | +0x10 | 0x1400106A0 | 7b | DtorBase | Unhookable |
| 3 | +0x18 | — | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | 0x14005E110 | 5b | JMP thunk | Unhookable |
| 5 | +0x28 | — | 6b | Runtime thunk | Unhookable |
| 6 | +0x30 | — | — | Draw | Reimplemented |
| 7 | +0x38 | — | — | Update | Reimplemented |
| 8 | +0x40 | 0x14005E0F0 | 12b | CMP+JZ+RET | Unhookable |
| 9 | +0x48 | — | 6b | Runtime thunk | Unhookable |

### EventNotifyDialog (0xF0 bytes, vtable 0x14015bca0)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x1400114f0 | 8b | GetTypeInfo | Unhookable |
| 2 | +0x10 | 0x1400114e0 | 7b | GetTypeId | Unhookable |
| 3 | +0x18 | 0x14003a948 | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | 0x1400686c0 | 3b | Shutdown: RET 0 | Unhookable (no-op) |
| 6 | +0x30 | — | — | Update | Reimplemented |
| 7 | +0x38 | — | — | Load | Reimplemented |
| 8 | +0x40 | 0x140068680 | — | OnEvent (no prologue) | Unhookable |
| 9 | +0x48 | 0x14003a954 | 6b | Runtime thunk | Unhookable |

### EventPopupDialog (0xC0 bytes, vtable 0x14015bd80)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140011910 | 8b | DtorVec | Unhookable |
| 2 | +0x10 | 0x140011900 | 7b | DtorBase | Unhookable |
| 3 | +0x18 | — | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | — | 6b | Runtime thunk | Unhookable |
| 6 | +0x30 | — | 6b | Runtime thunk | Unhookable |
| 7 | +0x38 | — | 6b | Runtime thunk | Unhookable |
| 8 | +0x40 | — | — | OnEvent | Reimplemented |
| 9 | +0x48 | — | 6b | Runtime thunk | Unhookable |

### FollowDialog (0xD8 bytes, vtable 0x14015be00)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140011A10 | 8b | DtorVec | Unhookable |
| 2 | +0x10 | 0x140011A00 | 7b | DtorBase | Unhookable |
| 3 | +0x18 | — | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | — | — | Shutdown | Reimplemented |
| 6 | +0x30 | — | — | Update | Reimplemented |
| 7 | +0x38 | — | — | Load | Reimplemented |
| 8 | +0x40 | — | — | OnEvent | Reimplemented |
| 9 | +0x48 | — | 6b | Runtime thunk | Unhookable |

### GoalDialog (0xC8 bytes, vtable 0x14015beb0)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140011B50 | 8b | DtorVec | Unhookable |
| 2 | +0x10 | 0x140011B40 | 7b | DtorBase | Unhookable |
| 3 | +0x18 | — | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | — | 6b | Runtime thunk | Unhookable |
| 6 | +0x30 | — | — | Update | Reimplemented |
| 7 | +0x38 | 0x140069800 | 3b | ret 0 | Unhookable (no-op) |
| 8 | +0x40 | — | — | OnEvent | Reimplemented |
| 9 | +0x48 | — | 6b | Runtime thunk | Unhookable |

### LoaderDialog (0xC8 bytes, vtable 0x14015f300)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140020BA0 | 8b | DtorVec | Unhookable |
| 2 | +0x10 | 0x140020B90 | 7b | DtorBase | Unhookable |
| 3 | +0x18 | — | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | — | 6b | Runtime thunk | Unhookable |
| 6 | +0x30 | — | — | Draw | Reimplemented |
| 7 | +0x38 | — | — | Update | Reimplemented |
| 8 | +0x40 | — | 6b | Runtime thunk | Unhookable |
| 9 | +0x48 | — | 6b | Runtime thunk | Unhookable |

### MainMenuDialog (0xC8 bytes, vtable 0x14015f550)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140020D00 | 8b | DtorVec | Unhookable |
| 2 | +0x10 | 0x140020CF0 | 7b | DtorBase | Unhookable |
| 3 | +0x18 | — | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | — | — | Shutdown | Reimplemented |
| 6 | +0x30 | — | 6b | Runtime thunk | Unhookable |
| 7 | +0x38 | — | 6b | Runtime thunk | Unhookable |
| 8 | +0x40 | — | — | OnEvent | Reimplemented |
| 9 | +0x48 | — | 6b | Runtime thunk | Unhookable |
| 10 | +0x50 | — | — | OnKey | Reimplemented |

### MapParamsDialog (0xF8 bytes, vtable 0x14015c180)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x1400120f0 | 8b | GetTypeInfo | Unhookable |
| 2 | +0x10 | 0x1400120e0 | 7b | GetTypeId | Unhookable |
| 3 | +0x18 | 0x14003a948 | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | 0x14006b800 | 3b | Shutdown: RET 0 | Unhookable (no-op) |
| 6 | +0x30 | — | — | Update | Reimplemented |
| 7 | +0x38 | 0x14003a990 | 6b | Runtime thunk | Unhookable |
| 8 | +0x40 | 0x14006b5f0 | — | OnEvent (rel branch byte 4) | Unhookable |
| 9 | +0x48 | 0x14003a954 | 6b | Runtime thunk | Unhookable |

### MessageDialog (0xC0 bytes, vtable 0x14015c480)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x1400125C0 | 8b | DtorVec | Unhookable |
| 2 | +0x10 | 0x1400125B0 | 7b | DtorBase | Unhookable |
| 3 | +0x18 | — | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | 0x14006D0C0 | 3b | RET 0 | Unhookable (no-op) |
| 6 | +0x30 | — | 6b | Runtime thunk | Unhookable |
| 7 | +0x38 | — | 6b | Runtime thunk | Unhookable |
| 8 | +0x40 | 0x14006D070 | — | OnCommand (inline dispatch) | Unhookable |
| 9 | +0x48 | — | 6b | Runtime thunk | Unhookable |
| 10-13 | — | — | — | OnOk/Cancel/Button2/3 | Reimplemented |

### ModDetailsDialog (0xC8 bytes, vtable 0x14015c670)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140012780 | 8b | DtorVec | Unhookable |
| 2 | +0x10 | 0x140012770 | 7b | DtorBase | Unhookable |
| 3 | +0x18 | — | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | — | 6b | Runtime thunk | Unhookable |
| 6 | +0x30 | — | 6b | Runtime thunk | Unhookable |
| 7 | +0x38 | — | 6b | Runtime thunk | Unhookable |
| 8 | +0x40 | — | — | OnEvent | Reimplemented |
| 9 | +0x48 | — | 6b | Runtime thunk | Unhookable |

### ModDialog (0xD8 bytes, vtable 0x14015c870)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x1400128F0 | 8b | DtorVec | Unhookable |
| 2 | +0x10 | 0x1400128E0 | 7b | DtorBase | Unhookable |
| 3 | +0x18 | — | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | — | 6b | Runtime thunk | Unhookable |
| 6 | +0x30 | 0x14006E480 | 7b | JMP [rip+xx] thunk | Unhookable |
| 7 | +0x38 | — | 6b | Runtime thunk | Unhookable |
| 8 | +0x40 | — | — | OnEvent | Reimplemented |
| 9 | +0x48 | — | 6b | Runtime thunk | Unhookable |
| 10 | +0x50 | — | — | OnKey | Reimplemented |

### ModUploadDialog (0xB8 bytes, vtable 0x14015c6c0)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140012830 | 8b | DtorVec | Unhookable |
| 2 | +0x10 | 0x140012820 | 7b | DtorBase | Unhookable |
| 3-9 | — | — | 6b | Runtime thunks | Unhookable |

### NewGameDialog (0xAE8 bytes, vtable 0x14015c920)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140012c60 | 8b | GetTypeInfo | Unhookable |
| 2 | +0x10 | 0x140012c50 | 7b | GetTypeId | Unhookable |
| 3 | +0x18 | 0x14003a948 | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | 0x1400706c0 | 3b | Shutdown: RET 0 | Unhookable (no-op) |
| 6 | +0x30 | — | — | Update | Reimplemented |
| 7 | +0x38 | 0x14003a990 | 6b | Runtime thunk | Unhookable |
| 8 | +0x40 | 0x140070110 | — | OnEvent (rel branch) | Unhookable |
| 9 | +0x48 | 0x14003a954 | 6b | Runtime thunk | Unhookable |

### OptionInputBindDialog (0xC8 bytes, vtable 0x14015ce40)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140012e50 | 8b | GetTypeInfo | Unhookable |
| 2 | +0x10 | 0x140012e40 | 7b | GetTypeId | Unhookable |
| 3 | +0x18 | 0x14003a948 | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | 0x14006d0c0 | 3b | Shutdown: RET 0 | Unhookable (no-op) |
| 6 | +0x30 | 0x14003a98a | 6b | Runtime thunk | Unhookable |
| 7 | +0x38 | — | — | Load | Reimplemented |
| 8 | +0x40 | 0x14006d070 | — | OnEvent (rel branch) | Unhookable |
| 9 | +0x48 | 0x14003a954 | 6b | Runtime thunk | Unhookable |

### OptionsDialog (0x1C0 bytes, vtable 0x14015ceb0)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140012de0 | 8b | GetTypeInfo | Unhookable |
| 2 | +0x10 | 0x140012dd0 | 7b | GetTypeId | Unhookable |
| 3 | +0x18 | 0x14003a948 | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | 0x140073480 | 3b | Shutdown: RET 0 | Unhookable (no-op) |
| 6 | +0x30 | 0x14003a98a | 6b | Runtime thunk | Unhookable |
| 7 | +0x38 | 0x14003a990 | 6b | Runtime thunk | Unhookable |
| 8 | +0x40 | — | — | OnEvent | Reimplemented |
| 9 | +0x48 | 0x14003a954 | 6b | Runtime thunk | Unhookable |

### SaveLoadDialog (0x160 bytes, vtable 0x14015f900)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140021150 | 8b | GetTypeInfo | Unhookable |
| 2 | +0x10 | 0x140021140 | 7b | GetTypeId | Unhookable |
| 3 | +0x18 | 0x14003a948 | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | 0x14003a94e | 6b | Runtime thunk | Unhookable |
| 6 | +0x30 | — | — | Update | Reimplemented |
| 7 | +0x38 | — | — | Load | Reimplemented |
| 8 | +0x40 | 0x1400b67e0 | — | OnEvent (rel branch) | Unhookable |
| 9 | +0x48 | 0x14003a954 | 6b | Runtime thunk | Unhookable |

### ScenarioDialog (0xE8 bytes, vtable 0x14015d2d0)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140013960 | 8b | DtorVec | Unhookable |
| 2 | +0x10 | 0x140013950 | 7b | DtorBase | Unhookable |
| 3 | +0x18 | — | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | — | 6b | Runtime thunk | Unhookable |
| 6 | +0x30 | — | — | Update | Reimplemented |
| 7 | +0x38 | 0x140078000 | 3b | ret 0 | Unhookable (no-op) |
| 8 | +0x40 | — | — | OnEvent | Reimplemented |
| 9 | +0x48 | — | 6b | Runtime thunk | Unhookable |

### ToolbarDialog (0x108 bytes, vtable 0x14015fa00)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140021700 | 8b | GetTypeInfo | Unhookable |
| 2 | +0x10 | 0x1400216f0 | 7b | GetTypeId | Unhookable |
| 3 | +0x18 | 0x14003a948 | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | 0x1400b97c0 | 3b | Startup: RET 0 | Unhookable (no-op) |
| 5 | +0x28 | 0x1400b97b0 | 5b | Shutdown: JMP rel | Unhookable |
| 6 | +0x30 | — | — | Update | Reimplemented |
| 7 | +0x38 | 0x14003a990 | 6b | Runtime thunk | Unhookable |
| 8 | +0x40 | — | — | OnEvent | Reimplemented |
| 9 | +0x48 | 0x14003a954 | 6b | Runtime thunk | Unhookable |

### TutorialGoalDialog (0x40 bytes, vtable 0x140160870)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140027430 | 8b | GetTypeInfo | Unhookable |
| 2 | +0x10 | 0x140027420 | 7b | GetTypeId | Unhookable |
| 3 | +0x18 | 0x1400df080 | 6b | MOV EAX,1; RET | Unhookable |
| 4 | +0x20 | 0x14003a912 | 6b | Runtime thunk | Unhookable |
| 5 | +0x28 | — | — | Shutdown | Reimplemented |
| 6 | +0x30 | — | — | OnEvent | Reimplemented |
| 7 | +0x38 | 0x140001640 | 8b | Delegator | Unhookable |
| 8 | +0x40 | — | — | OnInit | Reimplemented |
| 9 | +0x48 | — | — | OnDestroy | Reimplemented |

### TutorialHighlightDialog (0xF0 bytes, vtable 0x1401609e0)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x1400278b0 | 8b | GetTypeInfo | Unhookable |
| 2 | +0x10 | 0x1400278a0 | 7b | GetTypeId | Unhookable |
| 3 | +0x18 | 0x14003a948 | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | 0x14003a94e | 6b | Runtime thunk | Unhookable |
| 6 | +0x30 | 0x1400dffe0 | — | Update (rel branch) | Reimplemented (vtable patch) |
| 7 | +0x38 | 0x1400e0000 | 11b | Load (MOV+ADD+RET) | Reimplemented (vtable patch) |
| 8 | +0x40 | 0x14003aa1a | 6b | Runtime thunk | Unhookable |
| 9 | +0x48 | 0x14003a954 | 6b | Runtime thunk | Unhookable |

### TutorialSetupDialog (0x78 bytes, vtable 0x140160d20)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x1400285a0 | 8b | GetTypeInfo | Unhookable |
| 2 | +0x10 | 0x140028590 | 7b | GetTypeId | Unhookable |
| 3 | +0x18 | 0x1400e2be0 | 6b | MOV EAX,6; RET | Unhookable |
| 4 | +0x20 | 0x14003a912 | 6b | Runtime thunk | Unhookable |
| 5 | +0x28 | — | — | Shutdown | Reimplemented |
| 6 | +0x30 | — | — | OnEvent | Reimplemented |
| 7 | +0x38 | 0x140001640 | 8b | Delegator | Unhookable |
| 8 | +0x40 | — | — | Slot8 | Reimplemented |
| 9 | +0x48 | — | — | Slot9 | Reimplemented |
| 10 | +0x50 | — | — | Slot10 | Reimplemented |
| 11 | +0x58 | 0x1400285f0 | 12b | TEST+branch | Unhookable |

### AchievementDialog (0xD0 bytes, vtable 0x14015b020)
| Slot | Offset | Address | Size | Description | Status |
|------|--------|---------|------|-------------|--------|
| 0 | +0x00 | — | — | ScalarDtor | Reimplemented |
| 1 | +0x08 | 0x140010310 | 8b | DtorVec | Unhookable |
| 2 | +0x10 | 0x140010300 | 7b | DtorBase | Unhookable |
| 3 | +0x18 | — | 6b | Runtime thunk | Unhookable |
| 4 | +0x20 | — | — | Startup | Reimplemented |
| 5 | +0x28 | — | 6b | Runtime thunk | Unhookable |
| 6 | +0x30 | — | — | Update | Reimplemented |
| 7 | +0x38 | 0x14005D480 | 3b | ret 0 | Unhookable (no-op) |
| 8 | +0x40 | — | — | OnEvent | Reimplemented |
| 9 | +0x48 | — | 6b | Runtime thunk | Unhookable |
| 10 | +0x50 | — | — | Slot10 | Reimplemented |
| 11 | +0x58 | 0x1400104A0 | 8b | getter | Unhookable |
| 12 | +0x60 | 0x140010490 | 7b | getter | Unhookable |
| 13 | +0x68 | — | 6b | Runtime thunk | Unhookable |
| 14 | +0x70 | — | — | Slot14 | Reimplemented |
| 15-16 | — | — | 6b | Runtime thunks | Unhookable |
| 17 | +0x88 | — | — | Slot17 | Reimplemented |
| 18-19 | — | — | 6b | Runtime thunks | Unhookable |

---

## 2. UI Panel/Controller Classes (41 classes)

All UI controller classes share a common vtable pattern:
- Slot1 (+0x08): GetDescription/DtorVec (8 bytes, always unhookable)
- Slot2 (+0x10): GetTypeId/DtorBase (7 bytes, always unhookable)
- Remaining unhookable slots are 3-byte RET 0 no-ops or 6-byte Runtime thunks

### AttendanceUI (0x38 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot7 (Update)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/6/8/9/10 (thunks)

### BuildRequirementUI (0x60 bytes)
**Reimplemented:** Dtor, Slot4 (Startup)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot5/6/7/8/9/10 (thunks)

### BuildUI (0x50 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot7 (Update), Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/6/8/10 (thunks)

### CemeteryUI (0x38 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot7 (Update)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/6/8/9/10 (thunks)

### CitizenUI (0xE8 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot6, Slot7 (Update), Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/8/10 (thunks)

### ConsumeProduceUI (0x40 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot7 (Update), Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/6/8/10 (thunks)

### ControlUI (0x40 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot7 (Update), Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/6/8/10 (thunks)

### CropUI (SelectResourceUI, 0x60 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot7, Slot8, Slot9
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/6/10 (thunks)

### DebugUI (0x130 bytes)
**Reimplemented:** Dtor, Slot4, Slot7 (Update), Slot8, Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot6 (3b RET 0), Slot3/5/10 (thunks)

### DestroyUI (0x78 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot7 (Update), Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/6/8/10 (thunks)

### EntityUI (0x48 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot6, Slot7 (Update), Slot8, Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot5/10 (thunks)

### EventLogUI (0x80 bytes)
**Reimplemented:** Dtor, Slot5, Slot7 (Update), Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot6 (3b RET 0), Slot3/4/8/10 (thunks)

### HelpUI (0x78 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot7 (Update), Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot5 (3b RET 0), Slot3/6/8/10 (thunks)

### ImmigrationUI (0x50 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot7 (Update), Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/6/8/10 (thunks)

### MapUI (0x50 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot5/6/7/8/10 (thunks)

### ModBrowseUI (0x30 bytes)
**Reimplemented:** Dtor only
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot4-10 (thunks)

### NoiseUI (0x70 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot5/6/7/8/10 (thunks)

### OrchardUI (SelectResourceUI, 0x48 bytes)
**Reimplemented:** Dtor, Slot4, Slot7, Slot8, Slot9, Slot11, Slot12
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (6b thunk), Slot13 (6b MOV EAX,1; RET), Slot5/6/10 (thunks)

### PageToggleUI (0x48 bytes)
**Reimplemented:** Dtor, Slot4 (Startup), Slot7 (Update), Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot5/6/8/10 (thunks)

### PastureUI (SelectResourceUI, 0x40 bytes)
**Reimplemented:** Dtor, Slot4, Slot7, Slot8, Slot9, Slot11, Slot12
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (6b thunk), Slot13 (6b MOV EAX,2; RET), Slot5/6/10 (thunks)

### ProductionUI (0x50 bytes)
**Reimplemented:** Dtor, Slot4, Slot7 (Update)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot5/6/8/9/10 (thunks)

### ProfessionUI (0x78 bytes)
**Reimplemented:** Dtor, Slot5, Slot8, Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (6b thunk), Slot4 (6b thunk), Slot6/7/10 (thunks)

### RadiusUI (0x38 bytes)
**Reimplemented:** Dtor, Slot5, Slot6
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (6b thunk), Slot4 (6b thunk), Slot7/8/9/10 (thunks)

### ResidenceUI (0x40 bytes)
**Reimplemented:** Dtor, Slot4, Slot7 (Update), Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot5/6/8/10 (thunks)

### ResourceLimitUI (0x40 bytes)
**Reimplemented:** Dtor, Slot4, Slot7 (Update), Slot9 (OnEvent)
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/6/8/10 (6b thunks)

### SpriteCaptureUI (0x70 bytes)
**Reimplemented:** ScalarDtor, Serialize, OnCreate
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot5/6/7/9/10 (thunks)

### StatusBarUI (0x1D0 bytes)
**Reimplemented:** ScalarDtor, Draw, Startup, OnRemove
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/4/7/9/10 (thunks)

### StatusIconUI (0x40 bytes)
**Reimplemented:** ScalarDtor, Serialize, Startup
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/6/7/8/9/10 (thunks)

### StorageCapacityUI (0x38 bytes)
**Reimplemented:** ScalarDtor, Serialize, Startup
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot5/6/7/8/9/10 (thunks)

### StorageUI (0x40 bytes)
**Reimplemented:** ScalarDtor, Serialize, Startup
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot5/6/7/8/9/10 (thunks)

### TitleDescToolTipUI (0x30 bytes)
**Reimplemented:** ScalarDtor, Serialize
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot5/6/7/8/9/10 (thunks)

### ToolUI (0x58 bytes)
**Reimplemented:** ScalarDtor, Serialize, OnCreate, OnRemove
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot5/6/7/9/10 (thunks)

### TownHallUI (0x1E8 bytes)
**Reimplemented:** ScalarDtor, Serialize, Startup, OnCreate, OnRemove, Update
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/6/8/9/10 (thunks)

### TradeUI (0x148 bytes)
**Reimplemented:** ScalarDtor, Serialize, Draw, Slot6, Startup, OnCreate, OnRemove, Update
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (6b thunk)

### TutorialUI (0x60 bytes)
**Reimplemented:** ScalarDtor, Serialize, Startup, OnRemove
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3 (3b RET 0), Slot5/6/7/9/10 (thunks)

### UpgradeUI (0x38 bytes)
**Reimplemented:** ScalarDtor, Serialize, OnRemove
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/6 (thunks), Slot7 (3b RET 0), Slot8/10 (thunks)

### WorkPlaceUI (0x68 bytes)
**Reimplemented:** ScalarDtor, Serialize, Startup, OnRemove
**Unhookable:** Slot1 (8b), Slot2 (7b), Slot3/5/6/7/9/10 (thunks)

### SelectResourceUI (base class, 0x60 bytes)
**Reimplemented:** In `code/Game/Dialog/selectresourceui.cpp` (shared by CropUI, OrchardUI, PastureUI)
**Note:** Not a separate reimpl file — logic shared into subclasses

### UIComponent (0x38 bytes)
**Reimplemented:** Shutdown, Serialize
**Unhookable:** Draw, OnCreate, OnRemove, Update, Slot10, OnUpdate2 (all no-ops)

---

## 3. Dialog-Adjacent Classes

### BuildComponent (ControllerComponent, 0x78 bytes, vtable 0x140162260)
| Slot | Description | Status |
|------|-------------|--------|
| [3] Shutdown | 0x140108D80 | **PASSTHROUGH** (complex serialize with entity arrays) |
| [4] Serialize | 0x140108D60, 3b RET 0 | Unhookable (no-op) |
| [6] Draw | 0x140108D40 | **PASSTHROUGH** (worker assignment logic) |
| [7] OnCreate | 0x140108CB0 | Reimplemented |
| [11] OnUpdate2 | 0x140108D70, 3b RET 0 | Unhookable (no-op) |

**Disabled hook:** `BuildComponent::Draw` — disabled in hooks.cpp (RIP-relative prologue)

### BuildingGoal (Achievement::Goal, 0x48 bytes, vtable 0x140160300)
| Slot | Description | Status |
|------|-------------|--------|
| Slot5, Slot6, Slot10 | — | Reimplemented |
| Slot8 | 0x1400d75d0 | **DISABLED** in hooks.cpp (MOV RAX/R11,RSP prologue) |
| Slot1 (8b), Slot2 (7b), Slot3 (6b), Slot4 (6b), Slot7 (7b) | — | Unhookable |

### BuildStructure (Work, 0x70 bytes, vtable 0x1401610a0)
| Slot | Description | Status |
|------|-------------|--------|
| [0] ScalarDtor | — | Passthrough |
| [3] Serialize | — | Reimplemented |
| [4] Execute | — | Reimplemented |
| [5] Shared | 0x1400f7540, 3b | Unhookable (return false) |
| [6] OnComplete | — | **PASSTHROUGH** (complex tree traversal) |
| [7] OnCancel | — | **PASSTHROUGH** |
| [8] Cleanup | — | **PASSTHROUGH** (tree traversal) |
| [9] Validate | — | **PASSTHROUGH** (very complex) |
| [10] GetTarget | — | (not hooked) |
| [11] GetPriority | — | (not hooked) |

### BuildRoad (Work, 0x70 bytes, vtable 0x140161020)
| Slot | Description | Status |
|------|-------------|--------|
| [0-4,6-9] | — | Reimplemented |
| [5] Shared | 0x1400f7540, 3b | Unhookable (return false) |
| [10] GetTarget | 0x1400e4960 | Unhookable (RIP-relative, bug class 2) |
| [11] GetPriority | 0x1400295a0, 10b | Too small to hook |

---

## 4. Summary Statistics

### Totals
| Category | Reimplemented | Passthrough | Disabled Hook | Unhookable | Total Slots |
|----------|:---:|:---:|:---:|:---:|:---:|
| Dialog Controllers (22) | ~95 | 0 | 0 | ~150 | ~245 |
| UI Panels (41) | ~130 | 0 | 0 | ~300 | ~430 |
| Dialog-Adjacent (4) | ~15 | 5 | 1 | ~25 | ~46 |
| **TOTAL** | **~240** | **5** | **1** | **~475** | **~721** |

### Items Needing Work for Standalone Exe

**5 Passthroughs** (need decompile + reimpl):
1. `BuildComponent::Shutdown` (0x140108D80) — complex serialize with entity arrays
2. `BuildComponent::Draw` (0x140108D40) — worker assignments, game flag checks (also disabled hook)
3. `BuildStructure::OnComplete` (0x1400e4f80) — tree traversal
4. `BuildStructure::OnCancel` (0x1400e4f40)
5. `BuildStructure::Cleanup` (0x1400e57a0) — tree traversal
6. `BuildStructure::Validate` (0x1400e4be0) — very complex

**1 Disabled Hook** (needs reimpl, can't use trampoline):
1. `BuildingGoal::Slot8` (0x1400d75d0) — MOV RAX/R11,RSP in prologue

**~475 Unhookable Slots** (need clean C++ for standalone):
Most are trivial and fall into these categories:
- **~63 GetTypeInfo/DtorVec** (8 bytes) — return static type pointer, one-liner each
- **~63 GetTypeId/DtorBase** (7 bytes) — return type ID, one-liner each
- **~200+ Runtime thunks** (6 bytes) — JMP to base class method, map to base class call
- **~30 RET 0 no-ops** (3 bytes) — empty `{ return 0; }` or `{ }`
- **~10 OnEvent with rel branches** — actual logic, need decompile (MapParams, NewGame, SaveLoad, OptionInputBind, EventNotify, Message, ToolbarDialog Shutdown)
- **~5 Trivial getters** (7-8 bytes) — return field at offset
- **~3 Constant returns** (6 bytes) — `MOV EAX, N; RET`
- **~2 Delegators** (8 bytes) — `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`

### Remaining Exe Dependencies (3, cross-module)
| File | Dependency | Exe Address | Target Module |
|------|-----------|-------------|---------------|
| `destroyui.cpp:371` | `g_StateMachine_Tick` | `0x1400a9660` | Game (StateMachine) |
| `tutorialsetupdialog.cpp:189,207` | `fn_StringArray_Resize` | `0x140028860` | Entity (Serialization) |
| `tutorialsetupdialog.cpp:222` | `fn_SerializeExternalPtr` | `0x140028650` | Entity (Serialization) |

### Priority Order for Remaining Work
1. **BuildStructure passthroughs** (4 methods) — complex but needed for construction gameplay
2. **BuildComponent passthroughs** (2 methods) — building system core
3. **BuildingGoal::Slot8** (disabled hook) — achievement tracking
4. **OnEvent unhookable slots** (~10) — actual dialog logic behind rel-branch barriers
5. **Trivial unhookable slots** (~460) — mechanical work, batch-generate for standalone
