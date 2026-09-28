// ============================================================================
// tests/unit/collection_link_stubs.cpp — the Collection port's cross-family
// registration entry points, stubbed for the host (ENDGAME B.3.2)
// ============================================================================
//
// `RtCollectionPortInit()` (runtime/src/collection/array.cpp) calls one
// `*Register()` per file in runtime/src/collection/. Nothing in a host unit
// test calls `RtCollectionPortInit`, but the linker still needs every symbol it
// names, and linking the Set / LinkedList / introsort files just to satisfy
// them would drag their own by-address doors into a test that does not exercise
// them.
//
// SHARED BETWEEN THE B.3.2 FAMILY BATCHES, the same way
// tests/unit/rt_port_stub.cpp is shared. **When the SET or LIST batch adds its
// real file to tests/unit/sources.mk, delete that file's line HERE in the same
// commit** — two definitions of one registrar is a link error, and the fix is
// always "keep the real one".
//
// linkedlist.cpp and introsort.cpp are LINKED (the LIST batch added them to
// sources.mk), so their two registrars are NOT stubbed here — the real ones
// win. The rest of the Set family is still stub-only.
//
// The registrars return the number of names registered; zero is the honest
// answer for a build in which nothing was registered at all (the registry
// itself is inert here — see rt_port_stub.cpp).
// ============================================================================

