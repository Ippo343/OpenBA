---
paths:
  - "standalone/address_registry.cpp"
  - "standalone/reg_*.cpp"
  - "tools/dispatch_slot_map.py"
---

# Address registry rules

Every reimplemented exe function is registered so `ResolveAddr(Module::Exe, addr)` finds
it. Three categories:

1. **Code functions** (`0x140001xxx`-`0x14014Fxxx`): `RegisterExeAddr(addr, (uintptr_t)&OurFunc)`
2. **Dispatch slots** (`0x14015xxxx`): `RegisterDispatchSlot(addr, fn_ptr)` — provides the
   `.rdata` slot storage
3. **Vtables + type IDs** (`0x14015Exxx`-`0x1401Cxxxx`): static vtable arrays + `uint32_t` storage

## Dispatch slots are the exe's PE import table
Every slot has a known mangled name, readable from the binary:
```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/dispatch_slot_map.py 0x140151458
tools/ghidra_mcp/.venv/Scripts/python.exe tools/dispatch_slot_map.py --check
```
`--check` validates every registration against the binary and **must stay at zero
mismatches**. It also reports SHADOWED addresses (a slot registered twice: the later
binding wins and the earlier line is dead code).

## Registering an unregistered slot is not automatically safe
Some slots are null on purpose so a dispatch-or-fallback path takes the fallback, and a
few are read under the WRONG name and work only while null. Before registering, grep the
address across `code/`, `engine/` and `standalone/` and confirm every consumer wants the
same function. History: `docs/waves/unregistered_slots_wave.md`.
