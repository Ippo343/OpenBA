# Wave 9 — Lane H: hardening the bypass checker, and what it could not see

**Question the lane answers:** `tools/port_bypass_audit.py` is the only instrument
in this project that can see a **port bypass** — a function that is ported,
registered, counted by the census and reported clean by the diff oracle, while the
process runs the DLL's body. It reported a clean tree. **Was that "no bypasses" or
"none of the shape I can see"?**

It was the second. Two blind spots, both measured, both now closed, and the second
one was not in the brief — it fell out of the cross-check.

| | before | after |
|---|---|---|
| NAME-door sites the classifier gave a verdict to | 361 | **437** |
| …of which `OK-RESOLVE-ONLY` (asserted, no evidence) | 8 | **5** (each now naming the local it checked) |
| Bypasses found on the known-answer fixture | **1 of 3** | **3 of 3** |
| Raw-door statements the `--by-name` grep sees that the parser does not | (no such mode) | **0 of 378** |
| `rawdoor_audit.py --self-test` | **FAILS** (exit 2) | passes, with a positive control |
| New bypasses in the tree as it stands | — | **0** |

---

## 1. Blind spot one: `called` meant "on this line"

`rawdoor_audit.py` computed, per raw-door site, whether the resolve was
*immediately* applied to an argument list:

```c
RT_ORIGINAL_ONCE(ReleaseFn, "?Release@String@System@@AEAAXXZ")(&tmp);   // seen
```

`port_bypass_audit.py` consumed that bit and, when it was false, returned
**`OK-RESOLVE-ONLY` — "pointer stored, not called here"**. That verdict is correct
for a pointer that is genuinely only stored. It is wrong the moment the pointer is
called, and the calling shape is the *common* one:

```c
ReleaseFn release = RtOriginal<ReleaseFn>("?Release@String@System@@AEAAXXZ");
release(&dateStr);          // <- a separate statement
release(&timeStr);
```

`runtime/src/system/value_types.cpp` held **three** live-path `String::Release`
bypasses. The audit reported **one**. Two more of the same shape
(`widgets_input.cpp`'s `StringToInteger()` and `RealNumberEditElementSetValue()`
accessors, where the pointer escapes by `return`) were reported by no checker at
all. All five were found by a human grepping a mangled name.

**The fix.** `rawdoor_audit.scan()` gained a second pass. When a raw resolve is
assigned to something, the target is followed through the **brace-matched
enclosing function** and every later use is recorded — `call`, `escape-return`,
`escape-arg` — each with the guard state of *its own* line. Pairing is by
assignment target and enclosing function, **never by a line window**: a window is
what let a twelve-line comment delete a site from `resolve_fallback_audit.py`
(166 → 162, reading as a clean pass), and the positive fixture deliberately puts a
long comment between the resolve and the call so that cannot come back silently.

`port_bypass_audit` now treats "immediate call **or** unguarded later use" as
called. `OK-RESOLVE-ONLY` survives only where it is *checked*, and now prints its
evidence:

```
OK-RESOLVE-ONLY  runtime/src/audio/audio.cpp:840  (in RtAudio_Scene_StopEffect)
    ?IsEffectPlaying@Scene@Audio@@QEBA_NI@Z
    [pointer stored; no unguarded use of `isPlaying` in RtAudio_Scene_StopEffect]
```

That is lane V's `?IsEffectPlaying` distinction — resolve-outside-guard harmless,
call-outside-guard fatal — **verified** rather than inferred from the absence of an
argument list. Of the eight old `OK-RESOLVE-ONLY` rows, three are these genuine
ones; four were the `IO::Serializer` ctor/dtor/`GetVersion`/`GetEncoding` ports
whose body *is* a call to the DLL under their own registered name (`OK-DELEGATING`,
a cutover row, not a bypass) and one is the `!g_portOn` fallback arm
(`OK-PORT-OFF`). All five had been getting the right *exit code* for the wrong
*reason*.

### The proof, on the known answer

Two independent demonstrations, because "it now finds three" is only meaningful
against a version that found one.

**a. Same tree, same census, only the checker changes.** `git archive 955f78a~1`
into a scratch snapshot, plus the one `RegisterRuntimeExport` line that 955f78a
added for `?Release@String@System@@AEAAXXZ` — i.e. exactly the state in which the
three bypasses existed. Denominator identical in both runs (364 sites), so this is
not a population change:

```
OLD tools:  BYPASS=1   OK-RESOLVE-ONLY=10
  BYPASS          runtime/src/system/value_types.cpp:387
  OK-RESOLVE-ONLY runtime/src/system/value_types.cpp:314   <-- the miss
  OK-RESOLVE-ONLY runtime/src/system/value_types.cpp:333   <-- the miss

NEW tools:  BYPASS=3
  BYPASS runtime/src/system/value_types.cpp:314  [`release` call at :315, outside every guard]
  BYPASS runtime/src/system/value_types.cpp:333  [`release` call at :356, outside every guard]
  BYPASS runtime/src/system/value_types.cpp:387
```

**b. A committed fixture, so it stays proved.**
`tools/fixtures/bypass_value_types_before.cpp` is that file verbatim, and
`port_bypass_audit.py --self-test` asserts the hardened classifier reports BYPASS
at exactly its lines 330 / 349 / 403 **and** that the repaired tree reports none.
If either half stops holding, the self-test exits 2.

---

## 2. Blind spot two, which was not in the brief: 76 sites with **no verdict at all**

The `--by-name` cross-check (§3) was built to be a superset, and its first run
reported **79 raw-door statements the parser had not seen**. Expect your first
number to be too big: 3 of the 79 were an artefact of my own reporting — I printed
the line of the *name literal* while the parser records the line of the *door*, so
identical sites read as a disagreement. Fixed, and the remaining **76 were real**.

The macro sweep read mangled names from the resolve's **own line**. The normal
formatting in this tree puts the name on the next one:

```c
RtOriginal<Box*(*)(const Box*, Box*, const Vector3*)>(
    "?Extend@Box@Volume@@QEBA?AV12@AEBVVector3@Math@@@Z")(self, &theirs, p);
```

With no name on the door's line the site had no candidate, and the loop
`continue`d past it — **not** classified `OK-` anything, simply absent. The
population was 361 when it should have been 437. Every one of the three comment
incidents this wave presented the same way: a clean pass over a silently smaller
denominator.

Names are statement-scoped now, bounded at the **next** raw door in the same
statement so that

```c
VerifyDtorThunk(&sDtor, "??1DialogController@UI@@UEAA@XZ",
                ResolveExportRaw("??_7DialogController@UI@@6B@"),
                ResolveExportRaw("??1Internal@Object@@UEAA@XZ"));
```

does not attribute the three names to each other. All 76 recovered sites classify
`OK-ORACLE` / `OK-DATA` — **none is a bypass** — but they were being counted as
scanned while receiving no verdict, and that is the failure mode, not the verdict.

---

## 3. `--by-name`: the cross-check that cannot be defeated by parsing

For every mangled name we have **registered**, grep the tree for that literal and
report every raw-door spelling of it. Pairing is by **statement** (back to the
previous `;`/`{`/`}`, forward to the next `;`), not by a line window. Comments are
stripped first — a policy comment containing a worked example of a mangled symbol
made `export_name_audit.py` report a fabricated 663rd phantom, and this tool's own
docstring quotes several.

It has no verdicts; a grep cannot tell an oracle site from a bypass. What it
answers is the question the parser cannot answer about itself: **is the parser's
population the whole population?** It is a mode of the existing tool, not a fourth
overlapping script, and it exits non-zero when it sees a statement the parser did
not.

```
BY-NAME cross-check: 573 ported names; 378 raw-door statements mention one;
                     0 of those the parser did NOT see
```

**Run it after every port that LANDS, not after every edit.** A site is correct
when written and becomes a bypass when a *different* lane ports its name, with
nothing in that file changing — `Timer::SecondsToTicks` did exactly that.

---

## 4. The self-test now has an effectiveness control

`rawdoor_audit.py --self-test` **was failing, correctly**: it asserted the checker
still found the two defects lane V discovered, and both have since been repaired.

Inverting those two to "assert clean" and stopping there would have produced a
test that passes just as happily when the matcher is broken — the tautology the
brief warned about, and the same shape as every incident this wave has paid for.
So the self-test now has three legs, and the tree assertion is only one of them:

| leg | fixture | asserts |
|---|---|---|
| **effectiveness control** | `tools/fixtures/rawdoor_positive.cpp` | 3 shapes MUST still be reported: the immediate call, the accessor (`release` called a statement later, across a long comment), and the static accessor that escapes by `return` — the two the old classifier could not see |
| **false-positive control** | `tools/fixtures/rawdoor_negative.cpp` | 0 reported: a correctly guarded oracle call; `?IsEffectPlaying`'s resolve-outside-guard-with-all-calls-inside; and a pointer only *compared* (`probe == nullptr`), so `==` is not read as a store |
| regression | the tree | the two repaired lane V sites are still clean |

```
positive fixture: 3 sites, 1 UNGUARDED-CALL, 2 DEFERRED-CALL
negative fixture: 3 sites, 0 UNGUARDED-CALL, 0 DEFERRED-CALL
tree:           550 sites, 27 UNGUARDED-CALL, 74 DEFERRED-CALL
SELF-TEST OK
```

`port_bypass_audit.py --self-test` is the same idea against the known-answer
fixture of §1.

---

## 5. `DEFERRED-CALL` is counted everywhere and gates nowhere — deliberately

The deferred pass finds **74** rows in the tree. Lane V refused to gate on bare
resolves because ~450 of them are how all of `runtime/` reaches its **unported**
neighbours, and burying a real finding is the failure this checker exists to
prevent. That reasoning still holds for the *cutover-null* question. It does not
hold for the *port-bypass* question, where "the name is PORTED" is the
discriminator — so:

* `rawdoor_audit.py` prints the DEFERRED-CALL count in **every** summary line
  (there is no invocation that reports zero without reporting what zero was out
  of), lists them under `--deferred`, and fails on them only under
  `--strict-deferred`. Its exit-1 gate is still UNGUARDED-CALL alone, unchanged at
  **27**, so this lane cannot have moved that number.
* `port_bypass_audit.py` consumes `FileScan.sites` and gates on exactly the ported
  subset. That is where a deferred call becomes a **BYPASS**.

Spot-checking the 74: they are vtable/singleton accessors returning a slot pointer
(`goal.cpp`, `engine.cpp`), the `array_probe` / `deleting_dtor_probe` harnesses,
and `companion_data.cpp`'s twelve DX11 companions — unported neighbours to a row,
which is why `port_bypass_audit` classifies them `OK-DATA` / `OK-UNPORTED` rather
than flagging them.

## 6. Verdicts that must NOT be flagged — preserved, with their evidence

* `Audio::Configuration::Save`'s readback stays `RT_ORIGINAL_ONCE` on purpose:
  **the write is the work, the readback is the ORACLE.** Converting it would make
  the audit greener and destroy the oracle, because ours-writes/ours-reads agrees
  with itself.
* `GraphElement::RemoveGraphs` and the four `IO::Serializer` entry points are
  `OK-DELEGATING`: the port's body **is** a call to the DLL under its own
  registered name, so `ResolveExport` would recurse into itself. A cutover row,
  not a bypass. (Four of these were reaching that right answer by the wrong route
  — `OK-RESOLVE-ONLY` — until this lane.)
* The `!g_portOn` fallback arm is `OK-PORT-OFF`.

## 7. One incident of my own, recorded because it is the wave's own lesson

Writing `\b` into a **non-raw** Python string is how lane R's name-door checker
went 4 → 20 → 2. I did it again, by a different route: a heredoc collapsed `\\b`
to a literal `0x08` backspace in three regexes — including `FN_DEF`, the one that
finds the enclosing function, i.e. the exact failure that makes every site report
"no enclosing function" and read as BYPASS. It was caught before a single number
was produced, by `cat -A` on the written file rather than by reading back what I
thought I had typed. **`\b` is a valid Python escape and raises no warning; `\w`
next to it does.** Edit tools, not heredocs, for anything containing a backslash.

---

## What lane H did NOT do

`docs/runtime_port/WAVE9.md`'s pre-cutover gate row for `port_bypass_audit.py`
still carries the caveat *"Until the checker follows a local across a statement, 0
means 'none of the shape it can see'"*. That is now false, and the row should gain
`--by-name` as part of the gate command. The file was dirty from another lane
while this one ran, so the edit is left to the integrator rather than merged
blind.
