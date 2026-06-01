# Foundation C — Entity Description Factories (standalone) — Agent Brief

## Mission
Make Banished entity-**description** types instantiable + deserializable in the
**standalone** exe so entity Templates (e.g. `Template\Citizen.rsc`) can load and
the start-town ECS spawn works. Each description type is currently registered with
a **NullFactory stub** (`REGISTER_NULL_FACTORY` in
`code/Game/Object/factory_registrations_bulk.cpp`) → `Factory::CreateObject` returns
null → crash. You replace the stub with a **real factory + accurate Serialize**.

## Background (how it works)
- An entity template is a `ComponentDescription` External resource holding a list of
  typed sub-description blocks (ModelDescription, CitizenDescription, …). Each
  sub-description is itself an `Object::External` resource with its own
  vtable / Factory / Serialize, loaded by `ExternalFactory::FindOrLoad` →
  `Factory::CreateObject(typeId)` → the type's factory → the type's Serialize.
- In **standalone** the original exe is NOT mapped: `ResolveAddr(Module::Exe,a)`
  returns 0 unless registered. Runtime-x64.dll IS loaded (engine helpers resolve via
  `ResolveExport`/`ResolveAddr(Module::Runtime,…)`).
- **Serialize must be byte/stream-accurate** (exact field order + types), like the
  proven `MapGenerator` reimpl — a misread desyncs the stream and corrupts every
  following field. This is the core of the work.

## Proven reference patterns (READ THESE FIRST)
1. `code/Game/Game/mapgenerator.cpp` — a FULL External-description deserialize
   (`MapGenerator_Serialize`, `#ifdef STANDALONE_EXE`). Mirror its structure: Serializer
   ctor → per-field reads in exact stream order → array pools → ~Serializer. Uses
   `g_fn_Serializer_Ctor/Dtor`, `g_fn_String_Serialize`, `g_fn_Serialize_Int/Bool`,
   `fn_Serializer_Serialize_Float`, `fn_ExternalPtr_Serialize2`. All declared in
   `engine/funcs/engine_funcs.h`.
2. `code/Game/Goal/statgoal.cpp` — the FULL standalone factory scaffolding pattern:
   per-type vtable array, `Xxx_Factory()`, `Xxx_CreateInPlace()`, `Xxx_GetTypeId()`
   (via `Object::GetTypeIdByName("Xxx")`, cached), `RegisterXxxVtable()`, `REGISTER_TYPE`.
3. Trivial factory example — `ComponentDescription_Factory` (exe 0x14014ea90 area):
   `MemAlloc(size,8)` → base init → `*self = &vtable` → set field defaults → return.

## Per-type deliverable
For EACH assigned type `XxxDescription`:

1. **Ghidra recon (READ-ONLY — do NOT rename/annotate, other agents share the DB):**
   - Find its row in `decomp/application.md` (size, vtable, ctor, serialize, factory addrs),
     OR `search_functions_enhanced("XxxDescription")` / look for `XxxDescription_Factory`,
     `XxxDescription_Register`, and the vtable.
   - `decompile_function` the **Factory** (field defaults + size + vtable addr), the
     **ctor**, and the **Serialize** (vtable slot +0x28 — read the vtable via
     `read_memory` to get the slot addr, like MapGenerator's was 0x1400a2980).
   - Note the EXACT field offsets/types/order the Serialize reads. Verify ambiguous
     pointer-arithmetic against the disassembly (`disassemble_function`) when unsure —
     getting field order wrong corrupts the stream.

2. **Implement** in a NEW file `code/Game/Component/<xxxdescription>.cpp` (lowercase;
   Makefile auto-discovers `code/Game/*/*.cpp` — no build-file edits needed):
   - `Xxx_Ctor_Impl(void* mem)` — base init + vtable + field defaults (from the exe ctor/factory).
     Base init: most descriptions extend `Object::External` → call `fn_External_External(mem)`
     (already in engine_funcs.h) like MapGenerator does, then set the type vtable.
   - `Xxx_Serialize(void* self, void* stream)` — stream-accurate field reads (mirror MapGenerator).
   - `Xxx_Dtor(void* self, uint32_t flags)` — vtable reset + String/array field dtors + base dtor +
     `fn_MemFreeStatic` if `flags&1` (see StatGoal_Dtor).
   - Standalone scaffolding under `#ifdef STANDALONE_EXE`: vtable array (fill Dtor/GetTypeId/
     Serialize(+0x28)/CreateInPlace(+0x50); leave unknown inherited slots `nullptr` with a
     comment), `Xxx_GetTypeId`, `Xxx_Factory` (MemAlloc size + Ctor_Impl), `Xxx_CreateInPlace`,
     `extern "C" void RegisterXxxVtable()` (calls `RegisterExeAddr(<vtableAddr>, vtable_array)`),
     `REGISTER_TYPE(XxxDescription, "<parentName>", &Xxx_Factory, &Xxx_CreateInPlace)`.
   - Use raw offset access (`*(int32_t*)((uint8_t*)self+0x..)`) — no need for a struct header
     unless you want one.

3. **Self-check**: syntax-check your file (won't fully link in isolation, that's fine):
   `g++ -std=c++17 -fsyntax-only -DSTANDALONE_EXE -I. -Ipatches/src -Iengine/types -Iengine/funcs -Icode/Game -Iruntime/types -Iruntime/funcs -Istandalone code/Game/Component/<xxx>.cpp`
   (expect missing-symbol/link issues only; fix all SYNTAX/type errors).

## Coordination rules (CRITICAL)
- **Create NEW files only.** Do NOT edit `factory_registrations_bulk.cpp`,
  `address_registry.cpp`, the `Makefile`, or any existing file. (The orchestrator
  applies central registration.)
- Ghidra: **decompile/read only**. No renames, no comments, no struct edits (shared DB).
- If a type needs an engine helper not yet in `engine_funcs.h`, DON'T add it —
  **report** the function (name/addr/signature) so the orchestrator adds it.
- Some UI types (UIDescription, ElementDescription, ToolbarDescription) may be
  **Runtime-self-registered** already — check `decomp`/`list_classes`; if the Runtime
  provides the factory, the fix may be just removing the NullFactory entry (report it,
  don't reimplement).

## LESSONS FROM WAVE 1 (apply these — they were real crashes)
1. **Fill EVERY vtable slot with a real stub — never `nullptr`.** The Runtime's
   `External::Load` virtual-calls **GetVersion (vtable +0x18)** before Serialize; a
   null slot there faults. Read each type's `GetVersion` exe body (`mov eax,N; ret`)
   and return that exact N. Fill all other slots with `Return0`/`ReturnNull` stubs.
   Use the 11-slot layout: [Dtor, ReturnNull, GetTypeId, GetVersion(+0x18),
   Return0(+0x20 Compile), Serialize(+0x28), Return0(+0x30), Return0(+0x38 GetDesc),
   ReturnNull(+0x40), CreateInPlace(+0x48), ReturnNull(+0x50 guard)].
2. **CreateInPlace is vtable slot +0x48.**
3. **External-ref ARRAYS** (a `{ptr,count,cap}` header whose Serialize reads a count
   then per-element `FindOrLoad`, e.g. exe `FUN_140032090`/`FUN_1400049c0`): reuse the
   shared, proven loader — declare `extern "C" void Desc_LoadExternalArray(void* serBuf,
   int count, void* arrayBase);` and call it. Do the count-read + fresh array alloc
   (`fn_MemAlloc(singleton, count*8, 8)`, zero each) yourself, then call it. (Defined in
   `code/Game/Component/componentdescription.cpp`.)
4. **Single ExternalPtr fields**: `fn_ExternalPtr_BuildSerialize(field, serBuf)` is now
   registered (= exe 0x140002340) and resolves in standalone — use it (or
   `fn_ExternalPtr_Serialize2`). Both are stream-identical.
5. **Enum fields** (exe calls serializer-encoding vtable +0x98): use
   `SerializerEncodingVCall(serBuf, 0x98, field)` (inline in engine_funcs.h).
6. The serializer is the 24-byte `serBuf`; pass it to `g_fn_Serialize_*`,
   `g_fn_String_Serialize`, `SerializerEncodingVCall`, and `Desc_LoadExternalArray`.
   Stream-error check = `*(int*)(*(void**)serBuf + 8) == 0`.
7. If your type's Serialize calls an exe helper (FUN_xxxx) that resolves via
   `ResolveAddr(Module::Exe,..)`, it is NULL in standalone — **report that addr** so the
   orchestrator registers it (don't call a possibly-null pointer).
8. CaptureBoxDescription / FrustumOutlineDescription may already have DLL-mode reimpls
   under `engine/reimpl/` — check; you may only need to add the standalone scaffolding.

## Report back (your final message)
A concise table, one row per type:
`TypeName | vtableExeAddr | size | RegisterXxxVtable fn name | REGISTER_TYPE parent | newFilePath | NullFactory line to remove | status(done/blocked) | notes`
Plus: any new engine_funcs helpers needed, and any type you couldn't finish + why.
