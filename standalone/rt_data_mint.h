#pragma once
// ============================================================================
// standalone/rt_data_mint.h — Wave 9 lane C2: the minted exported DATA
// ============================================================================
//
// Waves 1-8 ported FUNCTIONS. The exported DATA symbols in Runtime-x64.dll that
// our tree consumes cannot be ported (the count moves as other lanes land
// ports -- see WAVE9_LANE_C2.md section 1): a `??_7` vtable, a `?_singleton@`
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
// ONE THING THIS IS NOT
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

// ============================================================================
// B.3.9 (D7) -- THE STORAGE REDIRECT
// ============================================================================
// Make `cell` (a class static / file static the caller owns) the storage of
// the minted storage row `symbol` -- its mangled name, or "0x<VA>" for a row
// the mint carries by address only. Call it from a `*PortInit()` (they all run
// before RtDataMintInit()); from then on the name door, the address door, the
// manifest publish and the class answer with the same bytes.
//
// Seeding rule (full text: standalone/rt_mint_redirect.h): the mint never
// memsets or re-seeds a redirected cell. Zero comes from the language (static
// storage); a row's kInit seed is copied in HERE, once, if the cell is still
// all-zero (or verified if the class's own initialiser already holds it); what
// the owner writes after this call is never overwritten. Aborts at boot on an
// unknown row, a null cell, a second different cell, a row the mint constructs
// (`_loader`, `_rng`), a seed conflict, or a call after the mint ran.
//
// Declared byte-identically in runtime/funcs/rt_port.h for the runtime tree.
void RtMintRedirectStorage(const char* symbol, void* cell);

// ============================================================================
// PER-ROW REGISTRATION
// ============================================================================

// Register one minted row through both doors. False if `symbol` is not a
// minted row at all. Valid only after RtDataMintInit() has laid the storage
// out. standalone/cutover_manifest.cpp publishes its REQUIRED rows this way.
bool RtMintRegisterRow(const char* symbol);

// ============================================================================
// WAVE 11 FOLLOW-UP 5 (lane TINFO) — the minted-node predicate
// ============================================================================
// True when `p` points into the minted BaseTypeInfo storage array. Used by
// runtime/src/object/factory_dispatch_probe.cpp to answer, by measurement,
// whether promoting a `?_typeInfo@` row can ever put one of our nodes (and so
// one of the 104 factory abort traps) in front of a live `factoryFn` dispatch.
// Exact address-range test; valid before and after any promotion.
bool RtMintIsMintedTypeInfo(const void* p);

// The predicate's positive control: `&kTypeInfoStorage[index]`, a pointer that
// MUST test true. nullptr when `index` is out of range.
const void* RtMintTypeInfoStorageProbe(int index);

// ============================================================================
// WAVE 24 lane TINFO -- enumerate the minted BaseTypeInfo rows
// ============================================================================
// The 104 `?_typeInfo@` rows, by index. `symbol` is the mangled name;
// `storage` is the minted node. Valid at any time -- the table is link-time
// constant; the CONTENT of `storage` is only meaningful after RtDataMintInit().
int  RtMintTypeInfoRowCount();
bool RtMintTypeInfoRowAt(int i, const char** symbol, void** storage);

// ============================================================================
// WAVE 25 lane MANIFEST -- the cutover publish manifest
// ============================================================================
// standalone/cutover_manifest.cpp. `RtCutoverPublishManifest()` walks the
// generated standalone/cutover_manifest.inc and registers every row the
// manifest marks REQUIRED, naming every row it holds and every by-name door it
// knowingly leaves resolving NULL. Returns the number of rows registered.
int  RtCutoverPublishManifest();
