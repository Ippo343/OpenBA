# Host unit tests for the real classes (ENDGAME B)

`mingw32-make unit` compiles every `tests/unit/*.cpp` plus the sources listed in
`tests/unit/sources.mk` into `tests/unit/build/unit.exe` and runs it. No game directory, no
window, no registry: the classes under test must stand on their own
(docs/runtime_port/endgame_b/B31_RULES.md rule 6). A test file is a list of `UNIT_TEST`
functions using the CHECK macros in `unit.h`; `main.cpp` runs them all and exits with the
failure count. `-DBANISHED_UNIT_TEST` is defined for these builds.
