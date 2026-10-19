# tests/ — unit-test constraints

The `unit` target is a host-only test binary; it is not shipped and does not link SDL. Run it with
`tools/build.sh unit [flavour]` (or CMake target `run-unit`). It links real ported classes, not
mocks; keep tests behavioural rather than reimplementing the code under test.

- `unit/sources.cmake` is hand-maintained. A body a test calls must be listed in `UNIT_TREE_SRCS` or
  the failure appears at link time, not compile time. Add the `.cpp` there, or a stub in
  `tests/unit/link_stubs.cpp` only when the real body cannot link on the host.
- On Linux, CMake rewrites `src/platform/win/<seam>_win.cpp` entries to the matching
  `src/platform/posix/<seam>_posix.cpp`; do not add a Windows-only source to that list without a
  POSIX twin.
- Some tests read retail data from `BANISHED_DIR` (e.g. package/font tests). They need a real
  Banished install and should report/skip clearly when it is absent.
- Stdio is macro-wrapped by the force-included `one_write_stdio.h` on MinGW. Keep test logging on the
  existing harness rather than adding new `printf`/`fprintf` paths; `log_strip.h` is not applied to
  the unit target, so don't rely on stripped-build behaviour in tests.
- Keep tests deterministic, offline, and free of writes outside `build/`; the suite is run repeatedly
  by agents and gates.
