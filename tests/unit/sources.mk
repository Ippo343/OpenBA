# Sources from the tree that the unit tests link against. One family per line; keep it
# to the classes under test and what they need (allocator, crc). Each core batch appends.
UNIT_TREE_SRCS = \
  runtime/src/system/string.cpp \
  runtime/src/system/filename.cpp
