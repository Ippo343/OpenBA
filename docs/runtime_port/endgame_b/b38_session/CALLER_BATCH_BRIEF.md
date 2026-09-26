# B.3.8 caller + fold batch brief (coordinator, 2026-09-26)

Step 8's class blocks made ~330 exe classes real C++ classes with compiler vtables (headers
`code/Game/<Dir>/<Class>.h`; rules `B38_RULES.md`; the per-block reports are summarised in the merge
commits `git log --merges --oneline main`). Two kinds of work were left, and your batch does BOTH for
its directories, measured in `b38_session/CALLER_SURFACE.md` (read it first; its appendices list every
site with file:line for your batch):

**A. Fold (spelling 6).** Most blocks kept the old free function as the body and made the member a
one-line delegate (`void X::Update(T* t) { X_Update_Impl(this, t); }`). ENDGAME B.2 step 3 says the body
MOVES into the member: `self->x` becomes `x`, `reinterpret_cast<uint8_t*>(self) + 0x48` reads become the
named field where the class declares one, calls to sibling free functions of the same class become
member calls. Then the free function is deleted — UNLESS it is reached by ADDRESS (a `RegisterExeAddr`
row with a live reader, a `ResolveAddr`/`R<>`/dispatch-slot user, a table) or by NAME from another file
you cannot convert: then it stays as a one-line door delegating to the member (qualified call:
`reinterpret_cast<X*>(self)->X::M(args); // B.3.8 bridge`). A body you fold that has a function-local
lazy cache (`static T cached; if (!cached) cached = …`) turns it into B6-5's shape (a class static /
namespace-scope atomic initialised without a first-call race, or `X::TypeId()` for type ids) — the
loader thread builds these objects.

**B. Callers (spellings 1-5).** (1) a raw vtable dispatch on a PROVED receiver (CALLER_SURFACE §2: SELF/
DECL/TYPE-ARRAY are proved; candidates need your proof from the code that produced the pointer, never
from its name) becomes a virtual call on the proved static type — slot-2 `Type()` scans become
`reinterpret_cast<Object::Base*>(p)->Type()` (or the existing typed helper); (2) calls to an old entry
point become member calls; (3) `ResolveAddr(Module::Exe, 0x14…)`/`R<>`/`engine_funcs` door calls onto a
converted member become member calls (`Component::Persist`'s 49 sites are the big one); (4)
`ResolveAddr(Exe, kVtable)` readers that stamp a converted class's table become the real ctor (B8-5:
same bytes) or `X::Vtable()`; (5) `GetTypeIdByName("X")` lookups / their lazy caches for a converted
concrete class become `X::TypeId()` (abstract bases keep the by-name lookup). An unproved receiver, a
call inside a printf/TRACE argument (B5-11), a by-value return that is not in the "construct in the
caller's storage exactly once" shape: LEAVE and list.

## Rules (all of BLOCK_BRIEF.md's hard and process rules hold; the additions)
- Your DIRECTORIES are in your prompt; edit nothing else (outside: minimum, listed). Files in block
  LOOP's set (`code/Game/Game/**`, `entitylist*`, the `standalone/*_stubs.cpp` loop files) are HELD:
  do not touch them.
- Retail-faithful: a fold moves code, it does not change it. If you find a port bug on the way, fix it
  in its OWN commit titled `… (behaviour: …)` with the retail evidence, or list it. CALLER_SURFACE §9
  lists five defects already noticed — the one in your directories is yours
  (`foresterwork.cpp:416` Type() without `this`; `cleararea.cpp:266` slot 6 through a 2-arg typedef;
  `pickup.cpp:87` resolves an unregistered 0x1400f2f00; `citizenui.cpp:662` reads the unregistered
  FollowDialog table).
- Bridge casts you add at a converted call whose receiver is still `void*`/POD carry `// B.3.8 bridge`.
- Delete only: a free function whose last caller you converted and that has no address/name binding
  left; a file-local cache/typedef/guard whose last user you converted; a `RegisterExeAddr` row in YOUR
  files that has no reader left (CALLER_SURFACE §5/§7 lists them) together with its register call if
  that call is in your files. `standalone/address_registry.cpp` and `engine/funcs/engine_funcs.{h,cpp}`
  are the coordinator's (the fragment/closing pass): list what you would delete there.
- Commit per class family (~5-15 classes), title `ENDGAME B.3.8 callers (<batch>): <what>`, via
  `git commit -F <file>`, ending `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- STATIC gate before every commit (BLOCK_BRIEF: debug, perf, unit, dispatch_slot_map, cutover_manifest,
  rt_mint_gen, no new warnings in touched files). NO GAME RUN (the coordinator gates the merge).
- If your context gets deep: commit what is verified and report exactly what is left (class list). A
  shorter honest batch beats a risky one; the coordinator relaunches the rest.

## Report
Branch + commits; members folded (count per class) and free functions deleted vs kept-as-door (with the
binding that keeps each); call sites converted per spelling (1)-(5); bridges added; rows/caches deleted;
every site LEFT with its reason; behaviour commits (for bisecting); gate verbatim.
