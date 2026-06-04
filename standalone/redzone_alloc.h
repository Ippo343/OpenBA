// standalone/redzone_alloc.h
//
// Debug-only heap-overflow detector for the engine's custom block allocator
// (System::MemoryAllocator, Runtime 0x1800c6e60). That allocator uses inline
// boundary-tag metadata (an 8-byte MemoryTag immediately before each user
// pointer, and the next block's tag immediately after the user region), so an
// out-of-bounds WRITE past a buffer corrupts the neighbouring block's tag and
// crashes LATER during free-list coalescing — far from the actual bad write.
//
// This shim interposes the fn_MemAlloc / fn_MemFreeStatic global pointers
// (which ALL of code/Game/ routes through) with wrappers that over-allocate by
// a tail redzone and stamp a canary right after the requested size. The bad
// write trips the canary; RedzoneAlloc_ValidateAll(tag) — called at checkpoints
// in MenuScene::Slot5 / Geometry::Serialize — reports the overflowing buffer by
// size + allocation backtrace BEFORE the displaced crash.
//
// Tail-only: the returned pointer is the allocator's real pointer (no head
// offset), so a block freed by any path (our wrapper OR Runtime-internal Free)
// stays correct. Zero overhead unless BANISHED_REDZONE is set (any value but
// 0/false), so release builds and normal test runs are unaffected.

#pragma once
#ifdef STANDALONE_EXE

// Swap fn_MemAlloc / fn_MemFreeStatic for the redzone wrappers if the
// BANISHED_REDZONE env var is set. Call once, right after EngineFuncsInit().
// No-op (and leaves the real pointers in place) when the env var is unset.
void RedzoneAlloc_Install();

// True if the shim is active (env var was set and install succeeded).
bool RedzoneAlloc_Enabled();

// Scan every live (wrapper-allocated) block's tail canary. For each corrupted
// one, log the requested size, allocation sequence number, and captured alloc
// backtrace. `tag` identifies the checkpoint (e.g. "Slot5 step9"). Cheap no-op
// when the shim is disabled. Returns the number of corrupted blocks found.
int RedzoneAlloc_ValidateAll(const char* tag);

// Walk the engine allocator's own boundary-tag chain and validate every block
// (live OR free, ours OR Runtime's) — catches free-list poisoning the per-block
// canaries miss (corruption of a freed block, or a wild-index write that skips
// the canary). When a bad block is found, the physically-preceding block is
// looked up in our live table to surface the overflower's alloc backtrace.
// Aborts cleanly (exit 0x123) on detection unless BANISHED_REDZONE_ABORT=0.
// Cheap no-op when the shim is disabled. Returns corrupted-block count.
int RedzoneAlloc_HeapCheck(const char* tag);

// Shim-INDEPENDENT one-shot heap dump — safe to call from the crash handler on
// a fully-native run (redzone shim NOT required). Walks the allocator's
// boundary tags, finds the first corrupt block, and dumps the physically-
// preceding block's vtable resolved to module+offset (names the overflower).
// Does not abort, does not take the allocator lock.
extern "C" void RedzoneAlloc_HeapDump(const char* tag);

// Free-list LINK validation at a code checkpoint. Aborts (exit 0x124) naming
// the tag at the first corrupt forward link. Only active under BANISHED_REDZONE.
extern "C" int RedzoneAlloc_CheckLinks(const char* tag);

// Optional write-through sink so the crash-context dump survives a re-entrant
// fault. The crash handler sets this to its FILE_FLAG_WRITE_THROUGH logger.
extern "C" void RedzoneAlloc_SetDumpSink(void (*sink)(const char*));

#endif // STANDALONE_EXE
