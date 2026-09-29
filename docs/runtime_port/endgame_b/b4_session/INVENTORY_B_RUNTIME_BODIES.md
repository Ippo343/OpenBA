# Inventory B — runtime bodies (B.4 block 0, NEXT_SESSION §3b.1 for `runtime/`, `dx11/`, `standalone/`, `engine/`)

Table: `data/B_runtime_bodies.tsv` (1,938 `c_body` rows + 771 `decl_undefined` rows, columns as A).
Input: `data/B_runtime_exports.tsv` (Runtime-x64.dll's 4,926 mangled exports + retail address, dumped once
from Ghidra `list_exports program="Runtime-x64.dll"`). Script: `scripts/B_runtime_bodies.py` (no
arguments, from the repo root; deterministic — two runs byte-identical; prints every summary quoted here).
Measured on main `b2a5434c` + the untracked `b4_session/`.

## 1. The answer

1. **1,938 C-shaped free bodies** in scope: runtime 1,641 · dx11 124 · standalone 173. 792 are
   `Rt<Ns>_<Class>_<Member>`, 597 `extern "C"`.
2. By what they ARE (column `note`, `role=`): **649 doors** (a ≤6-line body whose work is one qualified
   member call — the member exists), 23 C bodies a member delegates to, 27 duplicate bodies (C body and
   member both have code), **515 real C bodies** (no member body), 199 `Collection::Array<T>` / AVL `Set<T>`
   instances, 283 accessor helpers (`B()`, `Field<T>`, `MemFree` wrappers, `VTableOf`), 69 dx11 seam slots,
   173 standalone tool functions.
3. **710 have no code reference anywhere** (PROD or tests): 547 of the 649 doors, 113 real C bodies, 16
   duplicates, 12 collection instances, 21 helpers, 1 standalone. They hold **474 `reinterpret_cast` and
   287 bridge markers** — deletable without touching a caller. NEXT_SESSION §0.3 says 39.
4. Of the 515 real C bodies: 248 belong to a class that has a `runtime/types` header but no member
   declaration; 30 are the ONLY body of a member the header DECLARES (ctors, dtors, `operator new` of
   Scene, Renderer, MainLoop, Engine, SaveLoad, UserInterface, Camera, AnimationManager …); 140 have no
   class at all by any evidence (file-local helpers of offset reads).
5. **771 members are declared in `runtime/types` with no definition** (671 of them exported by retail):
   121 `operator=`, 50 `operator new/delete`, 16 destructors, the rest ordinary members marked `NOT PORTED`
   (Video::Configuration's 30 getters/setters, Serializer 45, Device.h 96 in all).
6. `reinterpret_cast` in `runtime/` = **4,957** occurrences: raw offset read 1,649 · `void*` receiver →
   class 549 · address-of → other type 628 · byte view of a receiver 206 · function pointer 17 · unclassified
   1,908 (read: mostly offset reads through a helper, vtable-slot reads, stream/POD views; <10% floor).
   **Half of them (2,532) are already inside MEMBER bodies** — `B(this) + 0x…` reads: making a C body a
   member removes the receiver cast, not these. standalone 1,638 (900 offset reads), dx11 135, engine 5.
7. Bridge markers in scope: **1,143 lines** (ui 311, graphics 280, video 130, standalone 119, io 67,
   dx11 51, engine 39, object 39, audio 33, save 31, …): 236 `void*` receiver → class (the doors), 128
   IO stream/serializer view, 82 smart-pointer slot, 72 `Rt*` POD ↔ real class, 66 address-of POD, 59
   `Object::Base*` type test (standalone), 30 `void*` field / factory result, 157 marker on a comment line,
   307 other.

## 2. How the table was built

- **Parse.** Every `.cpp/.h` under PROD is comment-stripped (codegrep.py's stripper, strings kept) and
  brace-scanned: namespace / `extern "C"` / class stacks, function definitions with their parameter list.
- **Row = free function in SCOPE** (`runtime/src`, `runtime/funcs`, `runtime/types`, `standalone`,
  `engine`, `dx11`; `*/build*` excluded) that is `Rt…_…` named, or `extern "C"`, or whose first parameter is
  `void*` or a pointer named like a receiver (`self*`, `this_*`, `obj`, `me`, …). Anonymous-namespace /
  `static` helpers are included, `linkage=internal`.
- **class** (authority order): (1) a retail address or mangled name in the comment block above the
  definition (stopped at the previous `}`) whose export's class AND member agree with the body name → `high`;
  (2) the export the body NAME points at (`Rt<Ns>_<Class>_<Member>`, `<Class>_<Member>`, namespace of the
  prefix preferred) → `medium`; (3) a ≤6-line body that is one qualified call `Ns::Class::Member(` → `medium`;
  (4) the first parameter's pointee is a known class (namespace of the file's directory preferred) → `medium`;
  (5) `Array`/`SetSize`/`Destroy…Range`/`Retrace`/`Rotate`/`AllocNode`… → `Collection::Array<T>` /
  `Set/Map<T>` instance, `low`; else empty (`none`). An address in a comment that is not an export marks
  `evidence=… unexported fn`. Comments name callees and neighbours: a disagreeing address is NOT taken
  (that rule turned 37 "misleading name" hits into 1 true one, §5.8).
- **class_header**: the class's definition in `runtime/types`, `engine/types` or `dx11/types` (qualified
  match; `Rt*` POD mirrors skipped). **member_declared**: the member's NAME among the functions declared in
  that class body (`Ctor*`→class name, `Dtor`→`~Class`, `OperatorNew`…). **member_defined**: a definition
  `Ns::Class::Member(` or an in-class body anywhere in PROD (`=default/delete/0` counts as declared only).
- **static_or_member**: from the export's MSVC access code (`global/static/member/virtual`); `member?` =
  inferred from a receiver-shaped first parameter.
- **callers**: code lines naming the body in PROD, minus its declarations/definitions, minus
  `obj->Name` / `Class::Name` member uses; an internal body counts its own TU only. `test_refs=` in `note`.
- **note** keys: `role`, `deleg` (C->member | member->C | both_have_bodies | C_only), `body_lines`,
  `first_param`, `casts`/`bridges` inside the definition, `linkage`, `evidence`, `DUP_NAME_DEFS`.
- `decl_undefined` rows: `name` = `Ns::Class::Member`, `note` has the declaration and its export.
- Hand check: two samples of 42 rows each, read at the source. Sample 1 (before the fixes above) 13/42 wrong,
  all in `class`/`member_declared`; the fixes are in the script. **Sample 2 after the fixes: 5/42 wrong
  (12%)**, then 1 more fixed → **~10% residual**, all in the `class` column of helpers and collection
  instances (e.g. `SpriteW` is `UI::Sprite::Width` inlined, `RtGraphics_SerializeMeshNodes` is
  `Array<GraphicsMesh::Node>::Serialize`, `RtSystem_Filename_Less` is a door to `operator<`).
  `callers=0`: 15 of 15 sampled confirmed by a whole-repo `git grep` (only their own decl/def).

## 3. Counts

Per directory (c_body rows | door (unreferenced) | real C body (unref.) | reinterpret_cast | bridges):

| dir | rows | doors (z) | C bodies (z) | casts | bridges |
|---|---:|---:|---:|---:|---:|
| runtime/src/ui | 356 | 182 (144) | 73 (19) | 1,413 | 311 |
| runtime/src/graphics | 282 | 87 (68) | 116 (25) | 1,194 | 280 |
| runtime/src/video | 258 | 139 (136) | 75 (28) | 411 | 130 |
| runtime/src/io | 116 | 23 (10) | 49 (6) | 328 | 67 |
| runtime/src/collection | 115 | 4 (0) | 5 (0) | 540 | 4 |
| runtime/src/system | 86 | 24 (13) | 39 (15) | 112 | 4 |
| runtime/src/object | 79 | 9 (5) | 23 (1) | 174 | 39 |
| runtime/src/engine | 64 | 15 (13) | 31 (1) | 204 | 39 |
| runtime/src/audio | 59 | 32 (31) | 11 (1) | 129 | 33 |
| runtime/src/math | 49 | 38 (38) | 10 (9) | 9 | 4 |
| runtime/src/input | 42 | 30 (27) | 5 (1) | 68 | 2 |
| runtime/src/achievement | 34 | 32 (32) | 0 | 15 | 9 |
| runtime/src/application | 33 | 23 (22) | 6 (1) | 25 | 2 |
| picking, save, volume, pathfinding, collision2d, crt, utility, types/* | 68 | 11 (8) | 41 (6) | 252 | 46 |
| dx11/src | 124 | 0 | 31 (0) + 69 seam slots | 135 | 51 |
| standalone | 173 | — | 173 tools (1) | 1,638 | 119 |

Top classes by C-body rows (class · rows · of them C_only · header): `Collection::Array<T>` 155/155 (none) ·
`System::MemoryAllocator` 52/8 · `Collection::Set/Map<T>` 46/46 · `Video::Context` 40/7 · `Audio::Scene` 36/16 ·
`UI::UserInterface` 33/5 · `Video::ImageBuffer` 32/5 · `Video::Device` 29/5 · `UI::Element` 28/5 ·
`Graphics::Renderer` 26/6 (header `graphics/Camera.h`) · `Application::Application` 23/2 · `Video::ContextData`
21/4 · `Object::ExternalFactory` 19/2 · `Video::DeviceData` 17/12 · `Achievement::AchievementManager` 16/1 ·
`Graphics::Camera` 16/8 · `Input::Device` 15/1 · `Video::RenderBuffer` 15/4 · `Math::Transform` 15/5 ·
`Math::Vector3` 15/2 · `UI::SpriteSheet` 15/3 · `System::String` 14/6 · `Input::KeyMap` 14/1 ·
`UI::DialogInstance` 14/0 · `Video::ShaderProgram` 14/4 · `UI::SpriteContext` 13/2 · `Engine::MainLoop` 12/2 ·
`Graphics::Scene` 12/4 · `IO::FileSystem` 11/1; 588 rows no class (helpers, tools).

Top classes by `reinterpret_cast` INSIDE their member definitions (the W0 field-naming work, not a body
move): UserInterface 303 · AnimationState 160 · SaveLoad 157 · DialogInstance 118 · AnimationManager 72 ·
Renderer 71 · MeshInstance 69 · Font 67 · Animation 52 · MainLoop 51 · FileSystem 48 · Device (Audio+Input) 44 ·
Scene 42 · Modification 41 · MaterialInstance 35 · ImageBuffer 35 · DocumentTextElement 34 · Engine 32 ·
ElementDescription 32 · Stream 30 · ParticleInstance 28 · TabDescription 25 · Element 24.

Top files by unreferenced bodies: `math/vector.cpp` 32 · `video/buffers.cpp` 27 · `video/context.cpp` 27 ·
`ui/widgets_input.cpp` 26 · `graphics/renderer.cpp` 23 · `ui/sprite.cpp` 22 · `ui/element.cpp` 20 ·
`audio/audio.cpp` 19 · `achievement/achievement_manager.cpp` 17 · `graphics/scene.cpp` 16 · `input/input.cpp` 16 ·
`math/transform.cpp` 15 · `ui/dialog.cpp` 15 · `video/renderstate.cpp` 15 · `ui/userinterface.cpp` 13.

`decl_undefined` by header: video/Device.h 96 · video/Context.h 47 · io/Serializer.h 45 · graphics/Mesh.h 37 ·
graphics/GraphicsMesh.h 25 · ui/DialogInstance.h 24 · graphics/Material.h 22 · graphics/Animation.h 19 ·
input/Input.h 18 · pathfinding/Scene.h 17 · engine/MainLoop.h 17 · video/ImageBuffer.h 17 · ui/Dialog.h 15; one
`operator=` / `operator new` in almost every UI class.

## 4. Blind spots

- **Class for 859 `medium` rows comes from a name or a parameter type**, not from retail; only 188 rows
  have an agreeing retail address/mangled name. ~10% residual error (§2), concentrated in helpers and
  collection instances. 222 rows cite a retail address that is NOT an export (template instances, private
  helpers): Ghidra could place them by vtable / caller; I checked one (`0x180001060` = inside
  `operator_new`), not the rest.
- **Overloads**: declared/defined is by member NAME, not signature (`ClearInput` ×2, `Normalize` ×2).
- **Callers are identifiers**: an address taken (`&Rt…`, `scene_watch.cpp`) counts as a caller; a name
  shared by 26 definitions (`MemFree`), 20 (`VTableOf`) or `Field` is resolved by linkage only. Macro-built
  functions (`DEFINE_TYPE`, `DX11_REGISTER_SLOT` bodies) are invisible as definitions.
- **Short-name collisions** (`Scene` ×5 namespaces, `Device`, `Stream`, `Configuration`, `String`) are
  resolved by the Rt prefix or the file's directory; a body in the "wrong" directory can still be misfiled.
- **Cast causes are regex**: the 1,908 unclassified runtime casts were read by sample (30 lines): ~1/3
  offset reads through a helper or a named constant (`B(self, 0x70)`, `self + kDeviceBackBufferWidth`),
  ~1/6 vtable-slot reads, ~1/6 stream/serializer views, ~1/4 POD ↔ class views, <10% floor (`HANDLE`,
  allocator free list). The offset count (1,649) is therefore a floor; E and C own the exact split.
- dx11 comment addresses are VideoDX11-x64.dll's: not looked up (no export dump for that DLL).
- The TSV covers `standalone/` tools only as rows; their 1,638 casts read code/Game objects (C's scope).

## 5. What surprised me (contradictions with NEXT_SESSION §3a/§0 or with comments)

1. **§0.3 "39 Runtime C-door bodies with no reference": it is 710** (547 doors + 113 C bodies + 50 others);
   `vector.cpp` alone has 32 (§0.3: 14), `transform.cpp` 15 (§0.3: 7). The B.3 doors were kept "so no caller
   has to move" and the callers moved; nobody deleted the doors. `TabElement_UpdateContainers_Body`,
   `RtGraphics_InterpolatedStateTransform_Lerp` confirmed unreferenced.
2. **Half of runtime's casts are already in members** (2,532 of 4,957). The C-body shape is the smaller
   problem in `runtime/`; the offset reads inside real members (`UserInterface` 303, `SaveLoad` 157) are the
   larger one. `Math.h`'s banner still says the `RtMath_*` shims "die in step 2": they were never deleted.
3. **30 declared members whose only body is a C function** — the audit's four (`~Scene`, `~Renderer`,
   `SaveLoad()`, `Audio::Scene()`) are right and there are 26 more: `MainLoop()`/`~MainLoop`, `Engine()`/
   `~Engine`, `AchievementManager()`, `Application()`, `AnimationManager()`/`~`, `Animation(const&)`/`~`,
   `Camera()` ×3, `Renderer()`, `Graphics::Scene()`, `Picking::Scene()`, `UserInterface()`,
   `Input::Configuration::~`, `KeyMap` deleting dtor, `ClipRegionElement::GetContentSize`, three `operator new`.
4. **771 declared-undefined members** make "declared" worthless as a status: `Video::Configuration` has 30
   `NOT PORTED` accessors declared; 121 `operator=` and 50 `operator new/delete` are declared, exported by
   retail, and defined nowhere.
5. **67 `Rt*` POD mirrors still sit next to the real classes in `runtime/types`** (`RtGraphics::Renderer`
   in `Camera.h:191` beside `Graphics::Renderer` at :659; `RtUI::DialogInstance`, `RtIo::Stream`,
   `RtObject::Base` …), and 45 file-local shadow structs carry retail names (`struct Serializer` in 19
   `.cpp`, `struct Stream` in 18, `Node` 5). The 72 "Rt POD ↔ class" and 128 "stream view" bridges are casts
   between these and the real class: they go when the mirror goes, not when a body moves.
6. **dx11 has 84 `DX11_REGISTER_SLOT(…, "Class::Member")` installs** (69 bodies matched): C bodies placed
   into `Video::*Data` vtable slots by index with a label — the carry-over-6 seam (§4.1), not B.4 work;
   `Video::*Data` declares in-class `CompanionPureCall` stubs for the same slots (my "duplicate" count).
7. `Graphics::Renderer`'s header is `runtime/types/graphics/Camera.h` (no `Renderer.h`).
8. **An address does not name a class either**: `Rt_GuardedObjectFree` (0x180006a10, called by ~100 sites)
   sits at the address Runtime exports as `?PlatformDestruct@RenderBuffer@Video@@` — identical-code
   folding. Known_port_issues 6 (misleading body names) cannot be found by a script; only this one surfaced.
9. `RtSystem_CriticalSection_Ctor` is the exported `??0CriticalSection@System@@` but its comment says "NOT a
   real constructor"; `Video_Device_Dtor` "stays a DOOR and not a real `~Device()` — RULE B5-6". Both must be
   decided, not converted mechanically.
10. §3a's `runtime/src + runtime/types` 4,772 `reinterpret_cast` LINES = 4,957 occurrences here (several per
    line): consistent.

## 6. Proposal: blocks with disjoint file sets

A block owns `runtime/types/<ns>/` headers + the `runtime/src/<ns>/` files that define their members.
Member definitions already stay inside their namespace directory (only `MemoryAllocator`, a top-level
class in `src/system`, crosses). C bodies that sit in another namespace's directory (~30 files, e.g.
`engine/singleton_ctors.cpp` holds Application / Input / IO / Collection ctors, `graphics/animationstate.cpp`
holds IO serializers) move WITH their file's block and the class owner reviews.

| order | block | files (types + src) | casts (rc) | bridges | notes |
|---|---|---:|---:|---:|---|
| 0 | **R0 dead bodies**: delete the 710 unreferenced bodies (per §0.3, one decision per non-door body) and their declarations | 166 definition files + their decl headers, incl. `vector.h`, `Filename.h`, `Mesh.h`, `MemoryAllocator.h` decls | −474 | −287 | first and ALONE: it touches `runtime/types` headers (door decls) and shrinks every later block; no behaviour change (no caller) |
| 1 | **R-HDR shared headers first**: the runtime classes code/Game reads (C's list; from this table: `MemoryAllocator`, `System::Time`, `Random`, `Utility::PerlinNoise`, `Volume::Frustum/Box/Rectangle`, `UI::Sprite`, `Graphics::EdgeMesh` vertices, `Engine::MainLoop`, `Save::SaveLoad`, `Graphics::Scene/Renderer/Camera`) | headers only | 0 | 0 | W0 field asserts; delete the `Rt*` mirror of each class here (5.5) |
| 2 | R-UI | 73 + 103 | 1,416 | 311 | 182 doors (144 dead), `UserInterface` 303 member casts, `DialogInstance` 118; `UserInterface()` ctor (5.3) |
| 2 | R-GFX | 17 + 33 | 1,195 | 280 | `AnimationState` 160, `Renderer` 71 (header is Camera.h), ctors/dtors of Scene, Renderer, Camera, AnimationManager, Animation |
| 2 | R-VIDEO (not dx11) | 10 + 33 | 459 | 131 | `Device.h` 96 undefined decls; `Video_Device_Dtor` door by rule (5.9) |
| 2 | R-IO+SAVE+OBJECT | 16 + 84 | 709 | 138 | `SaveLoad` 157; `Serializer.h` 45 undefined; local `struct Serializer/Stream` shadows (5.5) |
| 2 | R-ENGINE+APP+ACH+INPUT+AUDIO | 13 + 53 | 443 | 86 | `MainLoop`/`Engine`/`AchievementManager`/`Application` ctors+dtors live in `engine/` files |
| 2 | R-CORE (collection, system, math, picking, pathfinding, volume, collision2d, crt, utility) | 23 + 64 | 735 | 27 | `collection` 540 casts are `Array<T>` storage + element destroy callbacks: E decides the floor; `Collection::Array<T>` instances (199 rows) become the element type's dtor/assign, not free functions |
| 3 | R-STANDALONE | 77 | 1,638 | 119 | after code/Game's headers: its 900 offset reads are of Game objects |
| — | dx11 | 14 | 135 | 51 | carry-over 6 / §4.1, after B.4 |

Behaviour hazards to name in the rule sheet: (a) the 30 ctor/dtor C bodies — construction order of
singletons, retail zeroing that a C++ ctor would reorder, `ctor` vs "NOT a real constructor"; (b) ICF-folded
addresses (5.8) — one body serving several classes must stay one body or be proven identical per class;
(c) `Collection::Array<T>` destroy callbacks take an ignored `this` (RCX dead) — a member would change nothing
but the callers pass whatever is in RCX; (d) the width/signedness of each offset read (C's table) — a field
declared wider or unsigned changes behaviour (known_port_issues 5).
