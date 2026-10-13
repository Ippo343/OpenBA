# DIRECTTICK -- qualified direct calls for the two dominant slot-8 ticks

IDEAS_20261012 §2.5 (rank 5). Faithful exact (same function, same arguments, same order), perf
only. Kill switch: `static constexpr bool kDirectTick` in `src/game/Entity/entitylist.cpp`, beside
`kWalkPrefetch`. Base: main at efa4a8dbf. Nothing was run (no game, no stress town, no resave
gate): the coordinator's A/B decides (IDEAS §2.5: keep only on neutral-or-better in both towns,
drop on any regression).

## 1. What the walk did, and what it does now

The snapshot walk (`WalkUpdateTreeSnapshot<kWalkBatch>`, inlined into `EntityList::Update` and
`EntityList::UpdateWalkBatch`) ticks each listed component through the `tick` lambda built by
`MakeWalkTick`. Before: `TickOneComponent(c, time)` = `c->Tick(time)`, a virtual call (compiler
slot 9 = retail slot 8, `call *0x48(%rax)`). Now (kDirectTick):

```cpp
template <bool kCharArm>
[[gnu::always_inline]] static inline void TickOneComponentDirect(Component* c, const System::Time* time,
                                                                 const WalkVptrs& vt) {
    if (g_compProfOn >= 2) { TickOneComponent(c, time); return; }   // the profiled path, as before
    const void* vp = *reinterpret_cast<const void* const*>(c);  // CAST-OK: vslot
    if constexpr (kCharArm) {
        if (vp == vt.character) [[likely]] {
            static_cast<CharacterComponent*>(c)->CharacterComponent::Tick(time);
            return;
        }
    }
    if (vp == vt.model) [[likely]] static_cast<ModelComponent*>(c)->ModelComponent::Tick(time);
    else                           c->Tick(time);
}
```

`vt` is `{CharacterComponent::Vtable(), ModelComponent::Vtable()}`, read once per walk in
`MakeWalkTick` (the same hoist the walk prefetch and walk_batch use); GCC keeps the two values in
stack slots `0x40(%rsp)` / `0x48(%rsp)` (L1-resident, not a reload of the object). The tree walk
(`ComponentEntity::TickComponents`) and the bucket Update phase are untouched.

Perf objects (`build/cmake-perf/.../entitylist.cpp.obj`), `objdump -dr --no-show-raw-insn`, the
cached visit's tick in `EntityList::Update` (TickList::Cached<false>, the hot path), just before
the post-tick deleted-bit / key checks.

**Before** (efa4a8dbf, same command line):

```
    2ac5:	mov    0x0(%rip),%rax        # .refptr.g_compProfOn
    2acc:	cmpl   $0x1,(%rax)
    2acf:	jg     2f5c                  # COMPPROF=2 path
    2ad5:	mov    0x0(%r13),%rax        # the vptr
    2ad9:	mov    0x108(%rsp),%rdx      # time
    2ae1:	mov    %r13,%rcx
    2ae4:	call   *0x48(%rax)           # slot 8 (compiler slot 9)
    2ae7:	mov    0x40(%rbx),%edx       # entity flags (deleted bit)
    2aea:	test   $0x4,%dh
    2aed:	jne    300f
    2af3:	cmpb   $0x1,0xa(%r12)        # en.state == kCached
```

**After** (kDirectTick = true):

```
    2b1f:	mov    0x0(%rip),%rax        # .refptr.g_compProfOn
    2b26:	cmpl   $0x1,(%rax)
    2b29:	jg     2fd2                  # COMPPROF=2 path (virtual, as before)
    2b2f:	mov    (%rcx),%rax           # the vptr: the ONE load
    2b32:	mov    0x118(%rsp),%rdx      # time
    2b3a:	cmp    %rax,0x40(%rsp)       # == CharacterComponent::Vtable() ?
    2b3f:	jne    3162
    2b45:	call   CharacterComponent::Tick      # direct (fall-through)
    2b4a:	mov    0x40(%rdi),%eax       # entity flags (deleted bit)
    2b4d:	test   $0x4,%ah
    2b50:	jne    3186
    2b56:	cmpb   $0x1,0xa(%r12)        # en.state == kCached
    ...
    3162:	cmp    %rax,0x48(%rsp)       # == ModelComponent::Vtable() ?
    3167:	jne    317e
    3169:	call   ModelComponent::Tick          # direct
    316e:	jmp    2b4a
    ...
    317e:	call   *0x48(%rax)           # any other class: the virtual call, same %rax
```

The vptr is loaded once (`mov (%rcx),%rax`) and the fallback `call *0x48(%rax)` reuses the
register: no second load. The extra work per tick is one or two compares against stack slots.

The ext.walk_batch instantiation (`UpdateWalkBatch`), cached visit, before / after:

```
before:                                          after:
  4626: mov    0x0(%rbp),%rax   # vptr             4897: mov    (%rcx),%rax       # vptr (one load)
  462a: cmp    %rax,0x38(%rsp)  # char?            489a: cmp    %rax,0x40(%rsp)   # char?
  462f: je     473e  -> WalkBatch::TickCharacter   489f: je     49f8  -> WalkBatch::TickCharacter
  4635: mov    g_compProfOn,%rdx                   48a5: mov    g_compProfOn,%rdx
  463c: cmpl   $0x1,(%rdx)                         48ac: cmpl   $0x1,(%rdx)
  463f: jg     47e7                                48af: jg     495c
  4645: mov    0x108(%rsp),%rdx                    48b5: mov    0x118(%rsp),%rdx
  464d: mov    %rbp,%rcx                           48bd: cmp    %rax,0x48(%rsp)   # model?
  4650: call   *0x48(%rax)                         48c2: jne    4d5d              # -> call *0x48(%rax)
                                                   48c8: call   ModelComponent::Tick
```

**`[[likely]]`, and why.** Without it (first build) GCC laid both direct arms out of line (taken
`je`, `call`, `jmp` back) with the indirect call as the fall-through, so the common case paid two
taken branches. With `[[likely]]` the character call is the fall-through and the model arm is one
taken branch away. A model tick in the faithful walk is therefore: two compares, one taken `jne`,
a direct call, a `jmp` back; versus one predicted indirect call before. Whether that wins is what
the A/B measures (§5).

**Prefetch count** (PREFETCH §4's deletion trap): `objdump -d entitylist.cpp.obj | grep -c
prefetch` = **68 before, 68 after**.

**Off switch.** `kDirectTick = false` compiled with the same command line and diffed against the
efa4a8dbf object (`objdump -d`): the walk and dispatch code are identical; two cosmetic
differences remain elsewhere in the TU (one `cmp %rax,%r8` / `cmp %r8,%rax` operand swap in a walk
prefetch loop's `jne` bound test, and the COMDAT `Collection::Set<...>::Clear` inlined in a
different order). Neither changes behaviour.

## 2. Proof: the direct target is the virtual target

1. **The vptr names the dynamic type.** `X::Vtable()` (`DEFINE_EXE_VTABLE`, exe_type.h) is
   `&_ZTV<X>[2]`, the address point of X's compiler vtable, which is exactly the vptr a constructed
   X carries (tests/unit/exe_class_abi_test.cpp PART C, `exe_vtable_handle_is_the_objects_vptr`).
   Only complete objects whose most-derived class is X carry X's address point, and both classes are
   `final` (`static_assert(std::is_final<...>)` in entitylist.cpp), so `vp == X::Vtable()` means
   the dynamic type is exactly X.
2. **Slot 8 of X is X::Tick.** Both classes declare `void Tick(const System::Time*) override`
   themselves (CharacterComponent.h:52, ModelComponent.h:61). The static_asserts check that
   `decltype(&X::Tick)` is `void (X::*)(const System::Time*)`. If a header ever dropped the
   override, `&X::Tick` would name `Component::Tick` and its type would be
   `void (Component::*)(...)`, so the assert fails at compile time. The slot is checked by the
   existing unit tests `CHECK_SLOT(CharacterComponent, Tick, 8)` (cit_abi_test.cpp:160) and
   `CHECK_SLOT(ModelComponent, Tick, 8)` (vis_abi_test.cpp:232). These decode the PMF of the
   class's own member to its compiler vtable index, which is retail slot 8 with the
   virtual-destructor shift. A final class's slot-8 entry is its final overrider, here `X::Tick`.
3. **Same arguments.** The qualified call passes the same `this` (a static_cast down a single,
   non-virtual inheritance chain, adjustment 0: `CHECK_SLOT` fails on a this-adjusting PMF) and the
   same `time`.

Hence `static_cast<X*>(c)->X::Tick(time)` calls the function `c->Tick(time)` would call, with the
same arguments, whenever the arm is taken. No new unit test was added: the slot facts are already
asserted by the two CHECK_SLOT lines and PART C. The two facts that are specific to this change
(the class is final, and the override is declared in the class itself) are compile-time asserts
beside the call.

## 3. Composition with ext.walk_batch

`MakeWalkTick<true>` (UpdateWalkBatch) keeps its own order: the walk_batch character compare comes
first, and a character goes to `WalkBatch::TickCharacter` exactly as before. Only the else branch
changed: what was `TickOneComponent` is now `TickOneComponentDirect<false>`, so a model's tick is a
direct `ModelComponent::Tick`, the same body. `kCharArm = false` drops the character arm from that
instantiation. A character never reaches it there, and GCC cannot prove that the two separate
`CharacterComponent::Vtable()` calls return the same value, so without this it emitted a dead
compare plus an unreachable `CharacterComponent::Tick` call (seen in the second build; now gone:
UpdateWalkBatch references only `WalkBatch::TickCharacter` and `ModelComponent::Tick`). Both
instantiations still get their vtables once per walk, outside the loop, and there is no toggle
inside the loop. With ext.walk_batch on, behaviour is unchanged: characters take the same path, and
models run the same function.

COMPPROF=2 (`g_compProfOn >= 2`) still goes through `TickOneComponent`, the virtual call with its
per-type QPC bracket. Profiled runs therefore measure the virtual dispatch, not the direct one, so
use the sampler or the plain treeWalk numbers for the A/B.

## 4. Files

| file | change |
|---|---|
| `src/game/Entity/entitylist.cpp` | `kDirectTick` (beside `kWalkPrefetch`); `TickOneComponentDirect<kCharArm>` with the three static_asserts; `MakeWalkTick` builds the vptr pair once and routes the non-batched tick through it; `#include <type_traits>` |

## 5. Doubts

- **The gain may be zero or negative.** IDEAS §2.5 put it at ≤1% realistic. The indirect call was
  well predicted wherever consecutive ticks share a type. In the walk order a character's components
  alternate (character, model, ...), so the indirect predictor is history-indexed and probably
  predicts well already. The direct path adds a compare plus a taken branch for models. Keep it only
  on neutral-or-better in both towns.
- **Layout is GCC's call.** `[[likely]]` on two consecutive arms makes only the first one the
  fall-through. If models dominate the measured ticks, swapping the arm order is the one variant
  worth trying. I did not build it.
- **`CAST-OK: vslot` on the new vptr read.** It is the same marker and form as the existing walk
  reads. castmetric counts it under cast_ok, not in the total, which stays 2.
- Scope: the stale-entry `RetailFrom` passes inside `TickList::Cached` use the same `tick`
  lambda, so they go direct too. The unsorted visit (`!cached` -> `ComponentEntity::TickComponents`)
  and the AVL tree walk keep the virtual call. That is intended: they are the faithful fallbacks and
  are not hot at steady state.

## 6. Build results (this block)

- `tools/build.sh build perf --jobs 2`: OK. Compiled with its own command line, the TU shows the
  same four pre-existing `-Wunused-parameter` warnings (ReportStagingCorrupt / ReportTreeCorrupt)
  before and after, so no new warnings.
- `tools/build.sh unit perf --jobs 2`: **1126 tests, 0 failures**.
- `python tools/abi_audit.py`: OK.
- `tools/acceptance/acceptance.sh`: **0 lines off target** (R14-R16 skipped as usual, no new-game
  log).
- `python tools/castmetric.py src`: **total 2** (unchanged).
- Prefetch instructions in the entitylist object: **68 / 68**.

## 7. Results (coordinator A/B)

_Placeholder: heavy and compact towns, interleaved pairs, kDirectTick true vs false, alongside
rank 1 per IDEAS §4. Keep on neutral-or-better in both, drop on any regression._
