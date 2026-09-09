# Wave 11 kickoff — the object creation path

**Written 2026-09-09 at the close of Wave 10E, for the next session.**
Not dispatched. Read this before scoping anything.

## Why this is the next wave

The UI namespace is finished at its proven ceiling (186/188 = 98.9%). The
question that ended it pointed straight here: `??_7Dialog@UI@@6B@` is
unpromotable **because the object creation path is unported**, and that turned
out not to be one row's problem.

```
Runtime exports matching ?CreateNewObject@ / ?CreateInPlaceObject@ : 202
...referenced anywhere in our source outside the generated mint table :   0
```

**202 factory exports, none ported.** The same fact from the other side, already
recorded in `standalone/reg_mint_promoted.cpp`: all 104 minted `BaseTypeInfo`
rows carry `factoryFn` / `inPlaceFactoryFn` as **named abort traps** — no factory
in the mint is ported.

`Object` is also the lowest-completion namespace: **32 / 56 = 57.1%**, and it is
the most foundational one — `Base`, `Internal`, `External`, `Factory`, the
reflection chain everything else reaches through.

And it is squarely on the **cutover critical path**. When the DLL is gone,
construction has to be ours; today every object the framework builds is built by
retail.

## The two mechanisms to understand before scoping

These are established, and a lane that does not know them will re-derive them the
expensive way:

1. **In-DLL stamps are unredirectable.** The DLL's derived constructors reach
   `??0Element` by `rel32`, and the factory stamps its vtable by RIP-relative
   `LEA`. Neither goes through an import slot, the port registry, or a detour.
   **We can only intercept at doors, and these are not doors.** This is why
   `??_7Dialog` cannot be reached by registering anything — only by owning the
   factory that does the stamping.
2. **The exe never consumed vtable rows, in any build.** `Application-x64.exe`
   imports 899 symbols, 833 of them from Runtime, and **zero `??_7`**. It gets
   vtable pointers by constructing objects through Runtime factories. So there is
   no exe-side consumer to preserve — the whole question lives inside the DLL.

## Scope this with a DECOMPOSITION lane FIRST

**Do not dispatch "port the 202 factories".** Wave 10 opened with "UI is 29
short" and the decomposition showed the 29 were six different blocker classes,
only some of which were UI work at all — that decomposition is what stopped the
wave being scoped wrong (`WAVE10_UI_CENSUS.md` §1 is the model).

The first lane's deliverable is a table of the 202 by **blocker class**, not a
to-do list. Questions it should answer:

* How many are the trivial two-line `new T()` / placement-new shape, and how many
  do real work?
* How many are already reachable through something we own, and how many are only
  reachable through `Object::Factory`'s chain — which has a known split brain:
  our `Factory::Startup` resolves `?_firstTypeInfo@` **by name** and so walks the
  DLL's chain, not the minted one.
* What does porting a factory actually buy per row — does it move the census, or
  only unblock a `??_7` promotion, or neither?
* Which of the 104 `factoryFn` / `inPlaceFactoryFn` abort traps become reachable
  the moment a factory is ported? **That is a live risk, not a footnote**: those
  traps are currently unreachable *because* no factory is ported, and the mint's
  own design rule is that they abort loudly rather than fail silently.
* Is the allocator involved? `BANISHED_RT_ALLOC` arms with the DLL present
  (Wave 10 lane ALLOC) but is opt-in and has stayed unset in every lane since.
  Object construction allocates; say whether that gate is on this path.

## The immediate, cheap win to check first

`??_7Dialog@UI@@6B@`'s expiry is **mechanical and already written down**: port and
register either `?CreateNewObject@Dialog@UI@@` (`0x180038530`) or
`?CreateInPlaceObject@Dialog@UI@@` (`0x180038610`), then confirm with
`[DlgTrip] foreignDispatch > 0` using the tripwire lane FINAL left in the tree
(`runtime/src/ui/dialog_vptr_trip.{h,cpp}`, off by default). Two functions, an
instrument that already exists, and it takes UI to 187/188.

Do that as a *probe* of the decomposition's conclusions, not as the wave's goal.

## What NOT to do

* Do not promote `??_7DialogController@UI@@6B@` if a factory port makes it look
  reachable. Its verdict is **permanent and independent of creation**: its single
  consumer stamps the pointer and then calls `??1Internal@Object@@`, whose first
  statement overwrites the same qword **unread**. Creating the object differently
  does not change that.
* Do not read `runtime_census.py`'s `ported` column as the goal. Porting a
  factory faithfully will pull in callees and may push `called` up faster than
  `ported` — that is the honest price of resolving by name instead of opening
  address doors, and it has happened in four consecutive waves.
