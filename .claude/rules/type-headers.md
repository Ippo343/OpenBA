---
paths:
  - "src/engine/**"
---

# Binary-compatible type headers

- The tree is built by GCC, which lays classes out by the Itanium C++ ABI on every target
  (MinGW-w64 today, Linux next): the asserts below prove that layout reproduces retail's MSVC
  offsets. On Win64 the calling convention is Microsoft's (one convention); elsewhere it is SysV,
  which changes no layout. `src/engine/abi.h` asserts the data model once.
- A pinned layout holds only fixed-width integers, pointers, float / double, bool, enums and
  other pinned classes: never `long`, `wchar_t`, `size_t`, a bit-field or a native lock / handle
  type (`tools/abi_audit.py` enforces this; its baseline is checked in)
- `static_assert` on `sizeof` and on the `offsetof` of every field: a layout mismatch must
  fail the build, not the run
- Virtual calls go through raw function pointers read from the vtable at offset 0
- The struct layout itself is documented in Ghidra first (see the `ghidra` skill)
