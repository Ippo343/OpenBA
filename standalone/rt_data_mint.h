#pragma once
// ============================================================================
// standalone/rt_data_mint.h — Wave 9 lane C2: the minted exported DATA
// ============================================================================
//
// Waves 1-8 ported FUNCTIONS. The 70 exported DATA symbols in Runtime-x64.dll
// that our tree consumes cannot be ported: a `??_7` vtable, a `?_singleton@`
// pointer, a `?_typeInfo@` BaseTypeInfo and the ten other exported `.data`
// statics have no body to reimplement. They have to be MINTED — storage we
// own, laid out so that every ported and every not-yet-ported consumer agrees.
//
// Ground truth for the mint is lane C's harvest
// (docs/runtime_port/wave9_data_harvest.csv), taken while the DLL is still on
// disk, because after cutover there is nowhere left to check it against. The
// tables themselves are generated: `tools/rt_mint_gen.py` ->
// `standalone/rt_data_mint_tables.inc`.
//
// TWO THINGS THIS IS NOT
//
//  * It is NOT live by default. Minting means allocating the storage and
//    filling the vtables; REGISTERING it (so ResolveExport hands our table to
//    our callers instead of the DLL's) happens only under BANISHED_RT_MINT=1.
//    While the DLL is loaded, its own code keeps using its own singletons, so
//    handing our callers a second, empty set would split the process in half.
//    Cutover is when that flips, and it is the last commit of the wave.
//
//  * A minted vtable slot NEVER points into the DLL. It is either our ported
//    function or a named trap. That is deliberate: a slot quietly holding a
//    DLL address would work perfectly until the DLL is gone and then fault
//    with nothing to say. A trap says which vtable and which slot.
//
// ============================================================================

// Mint the storage and fill the vtables from the port registry. Call from
// RuntimePortInit(), AFTER every wave's *_PortInit() — a slot is filled with
// our port only if that port is registered by the time this runs.
void RtDataMintInit();

// The shadow check (BANISHED_RT_MINT_SHADOW=1). Compares the mint against the
// still-present DLL and prints a report. This is the lane's whole gate and it
// stops being possible at cutover, which is why it exists now.
// `phase` is a label for the report ("init", "exit", ...).
void RtDataMintShadowReport(const char* phase);
