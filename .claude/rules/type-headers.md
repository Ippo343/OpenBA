---
paths:
  - "runtime/types/**"
---

# Binary-compatible type headers

- MinGW GCC x64 uses the same Microsoft x64 ABI as MSVC (one calling convention on Win64)
- `static_assert` on `sizeof` and on the `offsetof` of every field: a layout mismatch must
  fail the build, not the run
- Virtual calls go through raw function pointers read from the vtable at offset 0
- The struct layout itself is documented in Ghidra first (see the `ghidra` skill)
