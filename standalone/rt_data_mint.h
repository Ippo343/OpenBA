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

// ============================================================================
// PER-ROW SELECTIVITY — Wave 10 lane MINTSEL
// ============================================================================
//
// BANISHED_RT_MINT=1 is one switch over all 197 rows and it does NOT boot with
// the DLL present: it publishes our zeroed
// ?_singleton@MemoryAllocator@System@@ over the DLL's live one and dies in
// EnterCriticalSection on a null self. That blocker lives in the 38 STORAGE
// rows. It says nothing about any vtable or any BaseTypeInfo, and before this
// API there was no way to say so with a measurement, because a single row
// could not be registered on its own.
//
// Three doors now, in increasing order of commitment:
//
//   RtMintRegisterRow(sym)        one row, both doors (name + address).
//   BANISHED_RT_MINT_ROWS=<csv>   the same, from the environment, for
//                                 MEASURING a candidate in a real boot.
//                                 `Foo*` is a prefix; a token matching no row
//                                 aborts rather than silently registering
//                                 nothing (which would read as "safe").
//   reg_mint_promoted.cpp         rows registered on EVERY run because they
//                                 were measured safe. Those are ports.
// ============================================================================

// Register one minted row through both doors. False if `symbol` is not a
// minted row at all. Valid only after RtDataMintInit() has laid the storage
// out — which is why the promoted rows are registered from inside it.
bool RtMintRegisterRow(const char* symbol);

// The minted storage / the DLL VA for one row, or nullptr / 0 when the symbol
// is not minted. reg_mint_promoted.cpp uses both to fail fast on a symbol that
// the generator has since dropped or moved.
void*              RtMintRowStorage(const char* symbol);
unsigned long long RtMintRowDllVa(const char* symbol);

// The CHECKING half of a promotion: proves `symbol` is a minted row and that
// `expectVa` is the VA the generated table carries, records it as promoted,
// and returns the minted storage. Aborts on either failure. The two
// Register* calls are made by reg_mint_promoted.cpp itself, with the symbol
// spelled as a LITERAL, so tools/runtime_census.py can see the port.
void* RtMintPromotionStorage(const char* symbol, unsigned long long expectVa);

// True once RtMintPromotionStorage has accepted this symbol.
bool RtMintRowIsPromoted(const char* symbol);

// True once RtMintRegisterRow has registered this symbol — i.e. it was ARMED
// for measurement (BANISHED_RT_MINT_ROWS or a direct call) rather than promoted.
// The readiness report prints armed rows for the same reason it prints promoted
// ones: a candidate that goes READY would otherwise print no line at all.
bool RtMintRowIsArmed(const char* symbol);

// Defined in standalone/reg_mint_promoted.cpp. Returns the number of rows
// promoted. Called by RtDataMintInit().
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
// The 104 `?_typeInfo@` rows, by index, for standalone/tinfo_door_screen.cpp.
// `symbol` is the mangled name; `storage` is the minted node (the same pointer
// RtMintPromotionStorage hands a promotion). Valid at any time -- the table is
// link-time constant; the CONTENT of `storage` is only meaningful after
// RtDataMintInit().
int  RtMintTypeInfoRowCount();
bool RtMintTypeInfoRowAt(int i, const char** symbol, void** storage);

// ============================================================================
// WAVE 25 lane MANIFEST -- the cutover publish manifest
// ============================================================================
// standalone/cutover_manifest.cpp. `RtCutoverDllAbsent()` is the cutover
// detector (the same `GetModuleInfo(Module::Runtime)->handle` test the mint
// shadow report uses, deliberately, so there is ONE answer to that question).
//
// `RtCutoverPublishManifest(force)` walks the generated
// standalone/cutover_manifest.inc and registers every row the manifest marks
// REQUIRED, naming every row it holds and every by-name door it knowingly
// leaves resolving NULL. With the DLL present and `force` false it registers
// nothing -- those rows' HARMFUL verdicts still apply on that route.
// `force` is armed by BANISHED_RT_CUTOVER_MANIFEST=1 and is a measurement
// knob: with the DLL loaded it deliberately creates the split brain the tags
// warn about. Returns the number of rows registered.
bool RtCutoverDllAbsent();
int  RtCutoverPublishManifest(bool force);
