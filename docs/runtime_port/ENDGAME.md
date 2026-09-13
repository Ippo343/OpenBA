# Endgame: no DLL, real classes, no flags (design, 2026-09-13)

The cutover gate is met (STATUS.md: boot to the world and quit cleanly with
Runtime-x64.dll absent). DIRECTION.md §3 names what comes next; this document is the
design and the step list for it, written once so it is not authored again per batch.
Three phases, in this order, each with an acceptance test:

| phase | deliverable | acceptance |
|---|---|---|
| **A** | the exe never loads Runtime-x64.dll; DLL-present machinery deleted | every route in §A.5 passes with no DLL on disk; `grep -c RtOriginal` = 0 |
| **C** | `BANISHED_*` reduced to one documented options struct | `grep -c 'getenv(' code runtime dx11` = 0; flags listed in §C.3 only |
| **B** | `RtNs_Class_Method(self, …)` bodies folded into real C++ classes | registry empty; `make standalone` links with no `ResolveExport`; the resave gate PASSES |

The order matters: A and C delete most of what B would otherwise have to rewrite, and B
is the only phase that needs design decisions rather than deletion.

## 0. Facts the plan rests on (measured 2026-09-13)

- **The default build still loads the DLL.** DLL-absent is opt-in: `BANISHED_RT_NO_DLL=1`,
  set by `make cutover`. `ResolveInit` (standalone/resolve_standalone.cpp) returns false
  without the DLL unless that variable is set, and then implies `RT_MINT` + `RT_ALLOC`.
  (done 2026-09-13, A.1/A.2)
- Source: code/ 726 files / 49.6k lines, engine/ 154 / 21.9k, runtime/ 530 / 245k,
  standalone/ 280 / 53k, dx11/ 17 / 8.5k.
- DLL-present-only machinery: the diff oracle (`BANISHED_RT_DIFF`, 44 files directly, ~1,640
  oracle-vocabulary hits over 325 files in runtime/src), the raw door (`RtOriginal` /
  `RT_ORIGINAL_ONCE`, 484 sites), 19 hook/detour installers in standalone/, the alloc and
  mint shadows, the byte-pins and probes.
- Mangled-name dispatch: `engine/funcs/engine_funcs.h` 672 `fn_*` typedefs / 663 externs /
  372 `ResolveExport` initialisers; 901 `RT_EXPORT_ONCE` cached-pointer calls in ported
  bodies; 236 raw vtable-slot calls (`(*(void***)obj)[N]`); 161 hand-built vtable arrays
  (~1,790 slots) behind 360 `Register*Vtable` functions; registry calls 2,302 by name and
  3,859 by retail address; `standalone/rt_export_addrs.inc` (4,932 generated rows).
- Bodies: ~4,540 functions in runtime/src (405 files) and ~5,360 in code/Game (545 files).
- Types: 231 headers, 551 plain structs, ~2,100 `static_assert(offsetof)`, zero `class`,
  zero `virtual`. 56 exe types carry a secondary vptr (the `ControllerComponent` pattern).
- Flags: 936 distinct `BANISHED_*` (322 `RT_*`, 111 `LIVE_*`, 130 trace/diag named, 40 perf,
  18 harness drivers, 7 world setup, ~300 other); ~950 `getenv` sites (code 600,
  standalone 216, runtime 133); `LiveEnabled` has 169 call sites and 87 graduated flags in
  `kDefaultOnFlags`; `RtWaveEnabled` has 204.
- Nothing outside our tree depends on mangled names or retail vtable pointers once the DLL
  is gone. Saves and the object factory key on **class-name strings hashed by CRC**
  (runtime/types/object/TypeInfo.h); reference ids and persist ids are positional.
- **The resave gate exists and passes** (tools/resave_gate/README.md). It is the
  serializer regression check for all three phases.

## A. Remove the DLL

Mostly deletion. The coupling is centralised; the cost is losing the oracle.

1. **Flip the default.** `ResolveInit`: delete the `LoadLibrary`/`GetModuleHandle` branch for
   `Runtime-x64.dll`; the no-DLL branch becomes the only one; `RT_MINT` and `RT_ALLOC` are
   unconditional. `VideoDX11-x64.dll` is already never loaded; delete its module-table row.
   `make cutover` becomes `make sa-run`: the DLL move-aside in tools/cutover_run.sh goes.
   `[done 2026-09-13]`
2. **Tag first.** `git tag dll-present-last` on the commit before step 1, so the oracle can be
   checked out for a comparison run. This is the only copy of that instrument that survives.
   `[done 2026-09-13]`
3. **Delete the raw door.** `RtOriginal`, `RT_ORIGINAL_ONCE`, `ResolveExportRaw*`,
   `ResolveOrdinal`: every call returns null without a DLL, so every branch that only runs on
   a non-null result is dead. Delete the branch, not just the call; where a body reads
   `if (!g_portOn) return theirs(...)`, keep ours.
4. **Delete the oracle.** runtime/funcs/rt_diff.{cpp,h}, `RT_DIFF_BEGIN/WORK/SCALAR`,
   `RtDiffReport`, every `_probe.cpp` and `_shadow` file, `rt_alloc_swap.cpp`,
   `rt_orphan_probe.cpp`, `rt_selftest.cpp`, the 19 detour/hook installers, the byte-pins
   (`tools/rt_deleting_dtor_verify.py` and the `*_sweep` make targets: `libm-sweep`,
   `libm-tables`, `gfxmat24-*`). A body is left as its non-oracle arm only.
5. **Routes that must pass DLL-absent before A is closed** (all exist; run each once):
   `make sa-run` + pause-menu quit; `BANISHED_AUTOQUIT`; `BANISHED_AUTOLOAD=Gate` (the gate);
   the Load Game dialog (`BANISHED_AUTOMENU_BUTTON=2`); the Mods route; `make sa-stress`
   on the perf exe. Screenshot pairs against the tagged build for the menu and the world.
6. **What stays after A:** the registry seam (`RegisterRuntimeExport/Addr`, `ResolveExport`),
   the mint (`rt_data_mint.cpp`, `rt_export_addrs.inc`), the 155 `reg_*.cpp` files and the
   hand-built vtables. Ported bodies still find each other through them. They die in B.

Size: two to four sessions, agent-batchable by directory. Risk: every body that a
DLL-internal caller used to reach is now reached only by us, and the oracle is gone; the
gate, the screenshot routes and the stress CSV are the instruments from here.

## C. Environment flags

Four tiers. Tier 1 is the same dead code as A.3/A.4 and is done in the same pass.

1. **Delete outright**: all 322 `BANISHED_RT_*`. After A the three DIRECTION.md kept
   (`RT_DIFF`, `RT_NO_DLL`, `RT_STRICT`) are dead too. `RtWaveEnabled` goes with them.
2. **Graduate or kill**: the 87 `kDefaultOnFlags` entries become unconditional code (delete
   the `if (LiveEnabled(...))`, keep the body). The other 24 `LIVE_*` get one line each in
   the batch's diff: graduated (with the run that proved it) or deleted with the feature.
   Then `LiveEnabled`, `kDefaultOnFlags` and `BANISHED_LIVE_COMPONENTS` go.
3. **Keep, centralised**: the flags that the Makefile, tools/, perf/ and .vscode/launch.json
   read. Harness drivers (`AUTOSTART`, `AUTOQUIT`, `AUTOLOAD`, `AUTOTUTORIAL`,
   `AUTOMENU_*`, `PAUSEMENU*`, `MODAL_ANSWER`, `MENU_SCENE`, `SAVE_TEST`, `KILL_AT_FRAME`),
   world setup (`SEED`, `MAP_SIZE`, `CLIMATE`, `CAMERA_PIN`, `DIFFICULTY`, `START_CONDITION`),
   perf (`STRESS*`, `SAMPLE`, `REDZONE*`, `SIMPROF`, `FPS_CAP`, `PERF_*`), the log controls
   (`STRIP_LOGS`), `DIR`/`GAME_DIR`. One `standalone/options.{h,cpp}` reads them once at
   boot into a struct; the rest of the tree reads the struct. `env_cache.h` and its
   force-include are then unnecessary.
4. **Triage per file**: the ~300 "other" and ~130 trace flags. Default delete. Keep only
   what a tool or a documented route reads (tools/*.py and *.sh are the list), and move it
   to tier 3. The `BUILD_DEMO_*` / `FORCE_*` harness flags that perf/ uses stay.

Acceptance: no `getenv` outside standalone/options.cpp; the flag list in options.h is the
whole inventory and CLAUDE.md's harness section points at it.

## B. Real classes

### B.1 The eight decisions

1. **Keep the retail object layout during the migration.** Nothing external reads memory
   once the DLL is gone; only our own unconverted code does, at raw offsets. So a converted
   class keeps its `static_assert(offsetof)` set until its whole hierarchy is done, and old
   C bodies and new classes operate on the same objects. The asserts may stay forever as
   documentation. This is what makes the work per-class instead of a flag day.

2. **The compiler's vtable must match retail's slot order** so hand-built arrays and real
   classes coexist. GCC's `virtual ~T()` occupies two slots and shifts everything after
   it; the whole `Object::Base` tree would have to flip in one commit. So: **no virtual
   destructor during the migration.** Slot 0 is declared as a normal virtual with retail's
   signature and semantics, the other virtuals in retail slot order, and deletion goes
   through one helper:

   ```cpp
   namespace Object {
   class Base {
   public:
       // slot 0: MSVC's vector deleting destructor. flags&1 = free, flags&2 = array.
       virtual Base* DeletingDtor(unsigned flags);          // +0x00
       virtual void  Slot1();                               // +0x08 (retail name once known)
       virtual uint32_t GetTypeId() const;                  // +0x10
       virtual void  Serialize(IO::Serializer& s);          // +0x18
       // ... in retail order, one line per slot, address in the comment
   protected:
       ~Base();                                             // non-virtual, called by DeletingDtor
   };
   inline void Destroy(Base* p) { if (p) p->DeletingDtor(1); }
   }
   ```

   A GCC single-inheritance vtable for this is slot-for-slot the MSVC one (RTTI pointer at
   -1 and offset-to-top at -2 in both). Overloaded virtuals: MSVC groups overloads and
   reverses their declaration order; we give them distinct names, so the issue does not
   arise. Multiple inheritance (the 56 `ControllerComponent` types): both ABIs lay
   non-virtual bases out in declaration order first, so `class BuildComponent : public
   Component, public ControllerComponent` puts the second vptr at +0x28 as retail does;
   keep base order, assert the offset, and the compiler emits the this-adjusting thunks
   retail hand-wrote. **When the last hand-built array is gone**, one mechanical pass may
   replace `DeletingDtor` with `virtual ~T()` and `Destroy` with `delete`; that is the
   "normal virtual cutover" and it changes nothing observable because by then every vtable
   is compiler-generated and every delete goes through the helper.

3. **Templates instead of per-instantiation bodies.** Retail monomorphised
   `Collection::Array<T>`, `Set<K,V>`, `LinkedList<T>` and the port mirrored it as one C
   body per element type, each registered by retail address (runtime/src/collection/). Write
   each template once, verify it against three ported instantiations with different element
   types (trivial, with copy-ctor, with dtor), then delete the C bodies. Largest single
   deletion in B; removes most address-driven registry rows.

4. **Reflection: one macro, name-CRC ids kept.** The type id is the CRC of the class name;
   saves depend on it, so names stay exact. Collapse our `FactoryRegistration` list feeding
   the DLL's `BaseTypeInfo` chain into one static per class:

   ```cpp
   #define DECLARE_TYPE(Class, Parent, Ext) \
       static Object::TypeInfo _typeInfo;  /* the 0x38-byte record */ \
       static uint32_t TypeId() { return _typeInfo.typeId; } \
       uint32_t GetTypeId() const override { return _typeInfo.typeId; }
   #define DEFINE_TYPE(Class, Parent, Ext) \
       Object::TypeInfo Class::_typeInfo(#Class, #Parent, Ext, &Class::Create, &Class::CreateInPlace);
   ```

   The 639 `GetTypeIdByName("X")` lookups become `X::TypeId()`. `Factory::Startup` still
   walks the chain, CRCs names, and builds the parent links `IsDerivedFrom` needs; the AVL
   tree may become a map. `TypeInfo::name` stays content-only (a binary fact).

5. **Names are the original ones.** Namespace, class and method from the export table and
   the profile DLL; member names from the profile build's assert strings where they exist,
   Ghidra names otherwise. `RtUi_BorderDescription_Serialize` becomes
   `UI::BorderDescription::Serialize`; `_Reimpl`/`_Impl`/`FUN_` suffixes go. Retail
   addresses stay as comments on every body: they are the provenance.

6. **Value types become real RAII types.** `System::String` (refcounted block at
   data-8), `Filename`, the Math types, `ExternalPtr`/`InternalBasePtr`. Constructors,
   destructors, copy/move and operators, with the bodies the C functions already have.
   The manual AddRef/Release sequences in ported bodies were emitted by retail's compiler
   from RAII and disappear. This is where B starts: no virtuals, no registry, and the
   layout invariant of decision 1 is exercised by everything.

7. **Exceptions.** ~25 files use setjmp/longjmp or SEH to cross retail frames. With no
   retail frame on the stack they become C++ exceptions or error returns. Last.

8. **The gate.** tools/resave_gate/ (PASS against its golden) plus: `make sa-run` +
   pause-menu quit exit 0 with an empty crash log; `BANISHED_AUTOQUIT`; the Load Game
   dialog route; `make sa-stress` within the perf baseline; a screenshot of the menu and
   the world compared by eye against the previous batch. Agents converting classes in
   parallel by namespace merge only on a green gate.

### B.2 The per-class recipe

For one class `Ns::C` with retail vtable `??_7C@Ns@@6B@`:

1. Read the type header (runtime/types or engine/types), the hand-built array
   (`g_C_Vtable[]` / the mint row `kVSlotsN`) and the `Rt<Ns>_C_*` bodies.
2. Turn the struct into `class C : public Parent` with the same fields in the same order
   (explicit padding kept), the virtuals declared per decision 2 in slot order, and the
   non-virtual methods as members. Keep every `static_assert(offsetof)` and add
   `static_assert(sizeof)`.
3. Move each body into the class. `self->x` becomes `x`; `RT_EXPORT_ONCE(Fn, "?M@C@Ns@@…")(self, …)`
   becomes `M(…)`; `(*(void***)obj)[N](obj, …)` becomes `obj->M(…)`; `CallSlot0(p, 1)`
   becomes `Destroy(p)`.
4. `DEFINE_TYPE(C, Parent, ext)` if the class is reflected (it has a `BaseTypeInfo`).
5. Delete: the hand-built array and its `Register*Vtable`, the class's rows in
   `rt_export_addrs.inc` / `rt_data_mint_tables.inc` (regenerate, do not hand-edit), its
   `fn_*` typedefs and externs in engine_funcs.h, its `RegisterRuntimeExport/Addr` and
   `RegisterExeAddr` calls. The vtable symbol the mint used to mint is now `&C::vtable`
   only in the sense that callers no longer ask for it.
6. Build. The offset asserts catch layout drift; the linker catches every caller that still
   asked for the name.
7. Run the gate (B.1.8). Merge.

Callers in not-yet-converted files keep working through decision 1 and 2: they read the
same offsets and dispatch through the same slot indices.

### B.3 Order of hierarchies

1. `System::String`, `Filename`, Math (Vector/Quaternion/Transform), the smart pointers.
2. `Collection::Array/Set/LinkedList` as templates (decision 3).
3. `Object::Base`, `TypeInfo`, `Factory`, `External`, `Internal` and the two pointer
   types, with the reflection macro (decision 4). This is the one step with real design
   work and it is done by one person, not a batch.
4. `IO` (Stream, CompressedStream, Serializer, Encoding, Package), `System` rest.
5. `Video` and `Graphics` (dx11/ stays the plugin, implemented in-tree).
6. `UI` (53 Description/Element classes, the largest batch, fully parallel).
7. `Engine`, `Save`, `Achievement`, `Audio`, `Input`, `Pathfinding`, `Picking`.
8. `code/Game`: components and dialogs, the 56 multiple-inheritance types last.

Every converted class shrinks the registry; when `standalone/address_registry.cpp`,
`rt_data_mint*.inc` and `rt_export_addrs.inc` are empty they are deleted with
`standalone/resolve*.cpp`, and `engine/funcs/` and `engine/types/` have dissolved into
`code/Game` headers. That is the end state DIRECTION.md §3 describes.

Size: weeks with parallel agents by namespace, mechanical except B.3.3. The per-class
recipe is what an agent is handed; the gate is what it is measured by.

## D. What was done on 2026-09-13 to start

- The resave gate was built and run (tools/resave_gate/README.md). Its first run found and
  fixed the last DLL-absent save blocker (`CompressedStream::SetWindow`'s grow path,
  runtime/src/io/compressed_stream_read.cpp); with the two random front-end picks pinned
  under `BANISHED_MENU_SCENE` the resave is deterministic and the golden was cut.
- Two intermittent pre-existing defects were seen once each and are recorded in the gate
  README: a loader-thread race in `ExternalFactory::Add` and a hang in teardown after Quit.
