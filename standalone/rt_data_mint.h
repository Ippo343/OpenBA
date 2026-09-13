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
// PER-ROW REGISTRATION
// ============================================================================

// Register one minted row through both doors. False if `symbol` is not a
// minted row at all. Valid only after RtDataMintInit() has laid the storage
// out. standalone/cutover_manifest.cpp publishes its REQUIRED rows this way.
bool RtMintRegisterRow(const char* symbol);

// The minted storage / the DLL VA for one row, or nullptr / 0 when the symbol
// is not minted.
void*              RtMintRowStorage(const char* symbol);
unsigned long long RtMintRowDllVa(const char* symbol);

// The CHECKING half of a promotion: proves `symbol` is a minted row and that
// `expectVa` is the VA the generated table carries, records it as promoted, and
// returns the minted storage. Aborts on either failure. The two Register* calls
// are made by reg_mint_promoted.cpp itself, with the symbol spelled as a
// LITERAL, so tools/runtime_census.py can see the port.
void* RtMintPromotionStorage(const char* symbol, unsigned long long expectVa);

// True once RtMintPromotionStorage has accepted this symbol.
bool RtMintRowIsPromoted(const char* symbol);

// WAVE 25 lane VTGFX. How many slots of the minted VTABLE row `symbol` the
// mint had to fill with a named abort trap because no port was registered by
// the time RtDataMintInit() ran. Valid only AFTER RtDataMintInit().
//
//   >= 0   a minted vtable row; 0 means every non-purecall slot is ours
//   -1     `symbol` is not a minted VTABLE row at all -- which is NOT the
//          same answer as "zero traps", and a caller that conflates the two
//          would read a typo as a green light.
//
int RtMintVtableTrapCount(const char* symbol);

// True once RtMintRegisterRow has registered this symbol.
bool RtMintRowIsArmed(const char* symbol);

// Defined in standalone/reg_mint_promoted.cpp. Returns the number of rows
// promoted. NOT CALLED since ENDGAME A.1: with no DLL the mint publishes every
// row unconditionally, so a promotion would be a no-op re-registration. The
// file is kept because its per-row RT_UNPROMOTABLE tags are the source the
// build gate (tools/cutover_manifest.py --gate) parses.
int RtMintRegisterPromotedRows();

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
