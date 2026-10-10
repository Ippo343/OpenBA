#pragma once
// ============================================================================
// src/engine/io/serializer_internal.h — the ported IO::Serializer surface (Wave 4, A)
// ============================================================================
//
// Byte-faithful reimplementations of the 22 exported IO::Serializer members our
// tree reaches. See serializer.cpp for the retail addresses, the pinned
// ReaderWriter vtable offsets and the oracle story; see
// src/engine/io/Serializer.h for the layouts.
//
// ----------------------------------------------------------------------------
// TWO ENTRY PATHS, ONE IMPLEMENTATION
// ----------------------------------------------------------------------------
// These functions are reached two ways, and that is deliberate — they are the
// SAME function pointer both times, not two ports of one class:
//
//   1. (B3R RT-IO: the RegisterRuntimeExport rows and RtIoSerializerRegister()
//      are gone -- no reader was left; callers use IO::Serializer's members.)
//
//   2. RegisterDispatchSlot(0x1401524xx, &RtSerializer_X) in
//      standalone/address_registry.cpp. The exe's retail-shaped code reads
//      `*(fn**)slot` directly and never consults ResolveExport, so those 14
//      slots must be bound to something; they are bound to these functions.
//      They are NOT a second redirection mechanism — removing them would leave
//      the slots null and EXEC-of-0 at the first save.
//
// ----------------------------------------------------------------------------
// BANISHED_REIMPL_SERIALIZER IS RETIRED
// ----------------------------------------------------------------------------
// The 2025-era version of this file gated every wrapper on
// BANISHED_REIMPL_SERIALIZER, defaulting OFF, so the shipped build tail-called
// the DLL. That gate is gone and replaced by the wave switches
// the old DLL-era switch / the old DLL-era switch (src/engine/io/rt_io_port.cpp),
// which default ON. RtSerializer_ReimplEnabled() no longer exists.
//
// The kill switch has to be tested INSIDE each body rather than expressed by
// "do not register", because path 2 above binds the dispatch slots
// unconditionally from AddressRegistryInit(). With the old DLL-era switch
// nothing is registered, the internal flag stays false, and every one of these
// functions tail-calls the shipped DLL — i.e. both paths are killed together.
// ============================================================================

#include <cstdint>

#include "io/Serializer.h"

// B.4 Z LAST IO: the 19 `extern "C"` RtSerializer_* forwarders that were
// declared here (the exe import-slot family over the `rt::io::Serializer`
// POD) had no caller left anywhere in the tree and are deleted with their
// bodies; every caller calls `IO::Serializer`'s members.
