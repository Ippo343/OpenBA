# Sources from the tree that the unit tests link against. One family per line; keep it
# to the classes under test and what they need (allocator, crc). Each core batch appends
# with `UNIT_TREE_SRCS +=` so parallel batches merge cleanly.
UNIT_TREE_SRCS =

# ENDGAME B.3.1 String/Filename core (tests/unit/string_test.cpp, filename_test.cpp).
UNIT_TREE_SRCS += \
  runtime/src/system/string.cpp \
  runtime/src/system/filename.cpp

# ENDGAME B.3.1 smart-pointer core (tests/unit/object_ptr_test.cpp): header-only.
# Object::ExternalPtr<T> / InternalBasePtr / InternalPtr<T> are non-owning views
# whose whole host-testable surface is inline in runtime/types/object/*.h; their
# only out-of-line body (Serialize) needs the live serializer + factory and is
# covered by the real run, not by make unit. No tree sources to link.
UNIT_TREE_SRCS +=
