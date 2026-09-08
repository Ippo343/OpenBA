# Wave 9 — Lane V: a written verdict for every `[RtRawResidue]` row

**Question the lane answers:** of the 104 requests lane 0 found going through the three
newly-lit *Raw* doors, which ones **must be satisfied before `Runtime-x64.dll` can be
removed**, and which die harmlessly with the oracle?

Every one of the 104 now carries a verdict:
`docs/runtime_port/wave9_rawresidue_verdicts.csv`.

| Verdict | Rows |
|---|---|
| **REAL DEPENDENCY** | **70** |
| **DATA READ** | **29** (all real too — see §4) |
| ORACLE-ONLY | 3 |
| DEAD | 2 |
| untriaged | **0** |

**The headline: 99 of 104 are real cutover work, not oracle scaffolding.** Lane 0's rough
split guessed ~54 census-blind / ~34 ported-body / ~16 port-residue and said "many are
`VerifyBody` at registration, diagnostic, dies with the DLL harmlessly". That last clause
is the part this lane had to overturn, and §2 is why.

---

## 1. The REAL DEPENDENCY list, in full

This is the remaining cutover work on the raw door. It is disjoint from
`wave9_residue_union.txt` only in the sense that the doors differ — several names appear in
both, and porting satisfies both at once.

### 1a. The IO::Serializer cluster — 22 rows, and it is the most dangerous item in the wave

`?Serialize@Serializer@IO@@…` ×17, `?SerializeBytes@…`, `?GetVersion@…`, `?GetEncoding@…`,
`??0Serializer@IO@@QEAA@AEAVStream@1@H@Z`, `??1Serializer@IO@@QEAA@XZ`.

These are **not** the harmless registration diagnostics lane 0's tag suggested. Read the
code, not the tag:

```
runtime/src/io/serializer.cpp:544   for each of 19 forwarders:
                                        fn = ResolveExportRaw(name);
                                        if (!DecodeForwarderSlot(fn, &slot)) VerifyFailed(...)
runtime/src/io/serializer.cpp:563/584/596   VerifyBody(name, expectedBytes, n)
runtime/src/io/serializer.cpp:601-605
        if (g_verifyFailures != 0) {
            printf("... IO::Serializer stays on the DLL ...");
            g_portOn = false;
            return 0;                       // <-- registers NOTHING
        }
```

With the DLL absent, `ResolveExportRaw` returns null (`resolve_standalone.cpp:549`), all 22
checks fail, and **the port switches itself off**. The bodies then take their fallback:

```
runtime/src/io/serializer.cpp:221-224 (RT_SER_SCALAR)
        if (!g_portOn) {
            RT_ORIGINAL_ONCE(Orig, MANGLED)(self, v, name);   // null call
            return;
        }
```

So the failure mode at cutover is not "the checks print a warning". It is: **the save/load
path calls a null pointer**, from a port that was verified, landed, and is counted as done.
The check is *fail-closed against the wrong hazard* — it was written to catch a wrong
ReaderWriter vtable slot, and a missing DLL is indistinguishable from one.

**What satisfies it:** make the verification skip-with-a-loud-line when
`ResolveExportRaw` returns null for *all* of them (that is the cutover signature, not a
slot mismatch), and make the `!g_portOn` fallback a hard, named failure instead of an
indirect call through null. Neither change touches the ported bodies.

### 1b. Live calls on a normal path — 46 rows (22 + 46 + 2 = the 70)

Each is `RT_ORIGINAL_ONCE(...)` or `RtOriginal<...>(...)` invoked on a path a normal run
executes. At cutover each is a call through null.

| Area | Symbols | Call site |
|---|---|---|
| IO streams | `??0/??1CompressedStream`, `??0/??1MemoryStream`, `?Open@CompressedStream`, `?Open@MemoryStream` | `io/compressedstream.cpp:189,191,314,315,376`; `save/saveload.cpp:802` |
| Graphics | `?SetBounds@Geometry`, `?ResetAnimationState@AnimationState`, `?PlayAnimation@AnimationState` | `graphics/mesh.cpp:228,231,235` |
| Video buffers | `?PlatformConstruct@{Vertex,Index,Image}Buffer`, `?PlatformDestruct@ImageBuffer` | `video/buffers.cpp:54,57,60,66`; `graphics/mesh.cpp:247,250` |
| Allocator | `?MemAlloc@MemoryAllocator`, `?MemFree@MemoryAllocator` | 7 files — **lane E** |
| Audio | `?FindFree3DEffect@Scene`, `?Stop@Device`, `?Set@Persistence@IO` | `audio/audio.cpp:440,743,491`; `video/device.cpp:650` |
| UI | `?RemoveChildren@Element`, `?CacheText@TextElement`, `?ToInteger@String`, `?SetValue@NumberEditElement`, `?GetIndexFromName@StringTable` | `ui/widgets_layout.cpp:84,94`; `ui/widgets_input.cpp:134,149`; `ui/sprite.cpp:581,629` |
| System | `?SecondsToTicks@Timer`, `?TicksToSeconds@Timer`, `?DateAsString@DateTime`, `?TimeAsString@DateTime`, `?Format@String`, `?Release@String`, `?FormatParamCount@String`, `?FormatParamList@String` | `system/value_types.cpp:246,256,303,305,308,370,312,331,385`; `system/string.cpp:709,735`; `engine/loops.cpp:362,365` |
| Save | `?GetDataDirectory@FileSystem`, `?UpdateList/UpdateLoad/UpdateLoadFromFile@SaveLoad` | `save/saveload.cpp:567,657,660,663` |
| Object | `?PersistObject@InternalFactory` | `object/internal.cpp:625` |
| CRT | `addr:0x18007fbfc` — the DLL's **own** statically-linked `_beginthreadex` | `system/thread_win.cpp:152`, from the `Thread` ctor at `:129` |

The table lists 37; the other nine are in §1c because they have a cheaper answer.

`0x18007fbfc` deserves its own line: it is not an export, it is not MinGW/UCRT's, and
without it `System::Thread` cannot start a thread. Lane D found it; it is still open.

### 1c. Nine of those 46 are **port-bypasses** — we already ported the callee

The function is registered and our own body walks past it into the DLL, exactly the class
lane R has been repairing. Seven of these nine are **new** (lane R's sweep found five, of
which `?Rand@Random` and `?IsDerivedFrom` overlap this list):

```
??0Geometry@Graphics@@QEAA@XZ            graphics/mesh.cpp:218   -> ported graphics/scene.cpp:1720
??1Geometry@Graphics@@UEAA@XZ            graphics/mesh.cpp:224   -> ported graphics/scene.cpp:1723
??1External@Object@@UEAA@XZ              video/buffers.cpp:76    -> ported object/external.cpp:680
??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z  graphics/mesh.cpp:238   -> ported math/transform.cpp:1000
??4String@System@@QEAAAEAV01@AEBV01@@Z   ui/widgets_layout.cpp:107 -> ported system/string.cpp:914
??HSystem@@YA?AVString@0@AEBV10@0@Z      save/saveload.cpp:570   -> ported system/string.cpp:931
??YString@System@@QEAA?AV01@PEBD@Z       system/value_types.cpp:384 -> ported system/string.cpp:919
?Set@String@System@@QEAAXPEBD@Z          system/value_types.cpp:329 -> ported system/string.cpp:920
?AdjustDelimiters@Filename@System@@AEAAXXZ save/saveload.cpp:581 -> ported system/filename.cpp:446
```

Each is one token — `RtOriginal`/`RT_ORIGINAL_ONCE` → `ResolveExport` — and each removes a
cutover dependency **and** a live `GetProcAddress` from a running path
(`reference_rtoriginal_is_getprocaddress.md`). `??DQuaternion` is the sharpest: Wave 1
proved that port bit-exact, and `MeshInstance::UpdateHierarchy` has been running the DLL's
copy ever since.

Two of the nine deserve a caveat rather than a blind edit: `??0Geometry` /
`??1Geometry` have a written justification at `graphics/mesh.cpp:206` — *"going to the DLL
is the byte-faithful choice as well as the one that removes a cross-lane registration-order
dependency."* That argument was sound while the DLL existed. It is not an argument at
cutover, and whoever changes it must satisfy the ordering concern it names rather than
delete the comment.

### 1d. Two oracle calls that escaped their guard — a defect, and a live cost today

```
runtime/src/audio/audio.cpp:918
    bool theirs = RT_ORIGINAL_ONCE(IsPausedFn, "?IsPaused@Scene@Audio@@QEBA_NH@Z")(self, soundType);
    RT_DIFF_SHADOW(theirs == paused, ...);

runtime/src/ui/widgets_layout.cpp:290
    RT_ORIGINAL_ONCE(Fn, "?GetSprite@ImageElement@UI@@QEBA?AVSprite@2@XZ")(self, scratch);
    RT_DIFF_BYTES(r, scratch, 16);
```

In both, the *result is used only by the diff macro on the next line*, but the **call** is
outside every guard. So on every ordinary run today, `Audio::Scene::Pause` and
`UI::ImageElement::GetSprite` each make a cross-DLL call whose value is discarded — and at
cutover both are calls through null. The fix is to move the call inside
`if (rt_diff_on__)`; nothing else changes.

The contrast worth recording: `?IsEffectPlaying` at `audio.cpp:834` and `:1031` looks
identical at a glance — an `RT_ORIGINAL_ONCE` at statement level, outside the guard — but
there it only **stores the pointer** into a local and every call through it is inside
`RT_DIFF_SHADOW`. Resolve-outside-guard is harmless (`ResolveExportRaw` returns null safely
with no module handle); call-outside-guard is fatal. **The residue cannot tell the two
apart — only reading the site can**, which is the whole reason this lane exists.

---

## 2. The answer to "how many of the 34 are a crash vs. a `VerifyBody`"

Of lane 0's 34 `PORTED-BODY-CALLS-SHIPPED` rows:

| | Count |
|---|---|
| Serializer registration gate — **fatal, see §1a** | **19** |
| live `RT_ORIGINAL_ONCE`/`RtOriginal` on a normal path — **fatal** | **12** |
| oracle call that escaped its guard — **fatal** | **1** (`?GetSprite@ImageElement`) |
| genuinely oracle-only | 1 (`?IsEffectPlaying`) |
| already repaired since the measurement | 1 (`?IsDerivedFrom`) |

**32 of 34 are cutover-fatal. Zero are the harmless `VerifyBody` the tag implied.** The
19 Serializer rows *are* `VerifyBody` calls — they are fatal because of what the failing
check then does, not because of the call itself. A tag naming the mechanism is not a
verdict about the consequence.

---

## 3. ORACLE-ONLY — 3 rows, and what "verified" means for each

All three are **verified by reading the gate**, not inferred:

| Row | Gate, quoted |
|---|---|
| `??1Internal@Object@@UEAA@XZ` | `dialog.cpp:897` is an **argument** to `VerifyDtorThunk`, whose first line is `if (!RtDiffEnabled()) return;` (`dialog.cpp:401`). The resolve runs every boot — hence 9/9 cells — the value is used only under diff. Second site `internal.cpp:699` is inside the shadow wrapper, registered only under `BANISHED_RT_INTERNAL_SHADOW` (`internal.cpp:772`, `:817`). |
| `??_7DialogController@UI@@6B@` | same call, same early-out (`dialog.cpp:896`). |
| `?IsEffectPlaying@Scene@Audio` | the unguarded thing is the **resolve** (`audio.cpp:834`, `:1031`); all five call sites are inside `RT_DIFF_SCALAR` / `if (rt_diff_on__)` / an `RT_DIFF_SHADOW` argument (`:350`, `:409`, `:805`, `:834`, `:1031`). |

**3 of 3 verified by quoting the gate; 0 inferred.** But note the trap in the first two:
ORACLE-ONLY is a verdict about **this door**. `??1Internal` and `??_7DialogController` are
*also* read live through `ResolveExport` at `dialog.cpp:730-731`, which is the
port-registry door and is already in `wave9_residue_union.txt`. A row can be oracle-only
here and a hard dependency there. Anyone reading only this file will undercount.

---

## 4. DATA READ — 29 rows, and six are not in lane C2's mint

All 29 are read live and **used**: a vtable stamped into an object's slot 0, or a singleton
slot dereferenced. Not one is oracle-only. Cross-referenced against
`wave9_data_harvest.csv`:

**Covered by lane C2 (23):** `??_7Base@Object`, `??_7External@Object`,
`??_7CompiledExternal@Object`, `??_7CompressedMemoryStream@IO`, `??_7DialogController@UI`,
`??_7Goal@Achievement`, `??_7GoalInstance@Achievement`, `??_7ImageBuffer@Video`,
`??_7ImageBufferData@Video`, `??_7RenderStateData@Video`, `??_7Thread@System`,
`?_typeInfo@GoalInstance@Achievement`, and the 11 singletons
(`AnimationManager`, `Configuration@Video`, `Device@Audio`, `Device@Input`, `Device@Video`,
`Factory@Object`, `FileSystem@IO`, `InternalFactory@Object`, `MainLoop@Engine`,
`Modification@IO`, `Persistence@IO`, `SaveLoad@Save`).

**NOT covered — lane C2 must add these six:**

```
??_7MeshInstance@Graphics@@6B@              graphics/mesh.cpp:265
??_7EdgeMeshInstance@Graphics@@6B@          graphics/mesh.cpp:269
??_7EdgeMesh@Graphics@@6B@                  graphics/mesh.cpp:273
?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA   object/type_system.cpp:65
?_secondsPerTick@Timer@System@@0NA          engine/engine.cpp:217, save/saveload.cpp:163,
                                            application/application_win.cpp:45
?_frameIndex@ParticleInstance@Graphics@@1HA graphics/animation.cpp:189
```

The three Graphics vtables are the same kind of object as the ones already harvested. The
last three are **not vtables** — a `BaseTypeInfo*` head pointer, a QPC-derived `double`, and
a `static int` — so a mint that only knows how to reproduce vtables does not cover them by
extension. `?_secondsPerTick` is the awkward one: it is written at runtime by
`System::Timer`'s init from `QueryPerformanceFrequency`, so minting a *value* for it is
wrong; the port has to own the initialisation.

Two DATA rows are dereferenced with **no null guard**, so they fault rather than degrade:
`engine/engine.cpp:355` (`*VideoDeviceSingletonSlot()` in the `MainLoop` ctor) and
`video/device.cpp:640` (`*PersistenceSingletonSlot()`).

---

## 5. DEAD — 2 rows, and the proof is a tree-wide literal sweep

| Row | Proof |
|---|---|
| `?ThreadFunc@Thread@System@@CAIPEAX@Z` | `thread_win.cpp:143` now reads `ResolveExport`, changed by Wave 9 lane P **after** this residue was measured (`git diff 357221a..HEAD`). |
| `?IsDerivedFrom@Base@Object@@QEBA_NI@Z` | `widgets_input.cpp:112` likewise, Wave 9 lane R. |

The proof is not "it did not appear in a run" — the lane brief forbids that, and rightly.
It is that a sweep of **every** literal occurrence of the name across `code/`, `runtime/`,
`engine/`, `standalone/` and `dx11/`, paired to its enclosing raw-door call by looking at
the text *preceding* the literal in a brace-joined window, finds **no remaining raw-door
site**. Both are now port-aware and both ports are registered
(`thread_win.cpp:346`, `object/type_system.cpp:497`).

---

## 6. Method, and where it could still be wrong

* **Paired by assignment target and enclosing expression, never by proximity.** For each of
  the 103 name rows the literal was located in every source file, then a site was accepted
  only if a raw-door token (`ResolveExportRaw`, `RtOriginal`, `RT_ORIGINAL_ONCE`,
  `RtVtable`, `VerifyBody`, `RtDataExport`, `ResolveRuntimeRaw`) appears in the text
  *before* the literal within the brace-joined window. **103 of 103 rows found a literal;
  85 paired to a raw site automatically and the remaining 18 were resolved by hand** — 16
  because the Serializer names live in a table walked by a loop and in a macro parameter,
  2 because their raw site had already been repaired (§5). The pass **failed open**: an
  unpaired row was a question, never a silent zero.
* **Every verdict was read at the source line, and the file was opened rather than
  grepped**, because the distinguishing fact — is the call inside `if (rt_diff_on__)`? —
  is invisible to a line-based match. That is how §1d was found and how `?IsEffectPlaying`
  avoided being mislabelled with it.
* **I did not sweep the ~490 `RT_ORIGINAL_ONCE` / `RtOriginal<>` sites as a checker**
  (297 and 195 respectively, measured at HEAD, excluding the macro's own definition)
  to produce this table; the table is the 104 measured rows resolved to their sites. The
  checker in §7 is a separate artifact and its output is a *triage list*, not a verdict.

**What this lane does not prove.** The verdict is about the tree at HEAD, and the residue
was measured at `357221a`; three lanes have been writing since. Two rows had already been
repaired in that interval (§5), which is evidence the drift is real and in the right
direction — but a row marked REAL DEPENDENCY here could be repaired by the time it is read.
Re-run §7's checker rather than trusting the line numbers.

Second: **residue is not reachability**, and lane 0 said so. A `DEAD` verdict here means
"the call site cannot execute", proved from the code. A `REAL DEPENDENCY` verdict means
"the site executes on a path the harness reached" — it does not rank them, and porting is
not the only answer for all 70. Several (`?UpdateSave`, `?UpdateDelete`, the screenshot and
autosave paths) are stub-first candidates.

---

## 7. `tools/rawdoor_audit.py` — the standing checker

The manual pass above cannot be repeated by hand every time a lane lands. The checker
mechanises exactly the §1d question: **a raw resolve whose result is called in the same
expression, outside every diff guard.**

```
tools/rawdoor_audit.py              # 570 raw sites scanned, 49 UNGUARDED-CALL, exit 1
tools/rawdoor_audit.py --self-test  # asserts it still finds the two §1d defects
tools/rawdoor_audit.py --all        # every raw site, guarded or not
```

* **It exits 2 when it cannot check** — an unreadable file, or a brace balance that does
  not return to zero (which makes its guard tracking meaningless for that file). It does
  not degrade to a partial answer. That is not decoration: the first run exited 2 on
  **11 files**, and both causes were real bugs in the checker (a `{` inside a string
  literal, and a `//` comment containing `/*` that opened a block comment swallowing the
  rest of the file). A checker that had "degraded gracefully" would have printed a
  confident number computed from a corrupted brace count.
* **`--self-test` fails if the two known defects stop being reported.** A checker that
  reports nothing is a broken checker until proved otherwise
  (`reference_probe_needs_an_effectiveness_control.md`).
* **The allowlist carries evidence, not names.** Thirteen sites are guarded by something
  lexical scanning cannot see — a `ReplayGuard::copy` flag, a helper every caller wraps in
  `if (rt_diff_on__)`, a macro *definition* rather than a call. Each entry names the guard
  and the line that establishes it.

**It finds 12 live raw calls the 9-cell matrix never reached**, which is the point of
having a static checker alongside a runtime one: `?Commit@CompressedStream` (×2),
`?UpdateSave`/`?UpdateDelete@SaveLoad`, `?UpdateContainers@TabElement`,
`?GetSprite@LabelElement`, `?GetLink@DocumentTextElement`, `?RemoveGraphs@GraphElement`,
`?GetDefaultResolution@Device@Video`, and three `filesystem_win.cpp` string helpers. Each
is a cutover crash site on a path the matrix simply did not drive — the screenshot,
autosave and error-dialog paths lane 0 named as known holes.

**Its limits, stated plainly.** It reports only the resolve-and-call-in-one-expression
shape. The far more common pattern — resolve into a cached accessor, call it from
elsewhere — is invisible to it and is why §1b is a hand list: `graphics/mesh.cpp`,
`video/buffers.cpp` and `ui/widgets_input.cpp` all hide live dependencies behind an
accessor and the checker reports none of them. Do not read a clean run as "no raw-door
dependencies remain".
