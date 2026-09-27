// ============================================================================
// runtime/src/ui/factory_ui1_tables.inc — WAVE 28 LANE FACTUI1
//
// MECHANICALLY TRANSCRIBED FROM C:\GOG Games\Banished\Runtime-x64.dll with
// objdump/pefile. Included by factory_ui1.cpp ONLY; read that file's banner
// before touching anything here.
//
// Each Store row is  { offset, width, kind, immediate, vtable-name, srcVa }
// where srcVa is the retail instruction the row was transcribed from, so any
// line can be checked with:
//     objdump -d --start-address=<srcVa> --stop-address=<srcVa+0x10>
//             "C:/GOG Games/Banished/Runtime-x64.dll"
//
// The repeated +0x000 vtable stores are retail's DEAD STORES and are KEPT.
// Widths are the instruction's operand size, not a field's type: several rows
// are UNALIGNED or OVERLAP their neighbours and that is what retail does.
// ============================================================================

const Store kComboDialog[] = {
    { 0x000, 8, SK_VTABLE, 0, "??_7ComboDialog@UI@@6B@", 0x1800748be },
    { 0x0b8, 8, SK_IMM, 0x0ull, nullptr, 0x1800748c3 },
    { 0x0c0, 8, SK_IMM, 0x0ull, nullptr, 0x1800748ca },
    { 0x0c8, 8, SK_IMM, 0x0ull, nullptr, 0x1800748d1 },
};
const Store kDialogInstance[] = {
    { 0x000, 8, SK_VTABLE, 0, "??_7DialogInstance@UI@@6B@", 0x180114cc3 },
    { 0x010, 8, SK_IMM, 0x0ull, nullptr, 0x180114cc8 },
    { 0x018, 8, SK_IMM, 0x0ull, nullptr, 0x180114ccc },
    { 0x020, 8, SK_IMM, 0x0ull, nullptr, 0x180114cd0 },
    { 0x028, 8, SK_IMM, 0x0ull, nullptr, 0x180114cd4 },
    { 0x038, 8, SK_IMM, 0x0ull, nullptr, 0x180114cd8 },
    { 0x040, 8, SK_IMM, 0x0ull, nullptr, 0x180114cdc },
    { 0x048, 4, SK_IMM, 0x0ull, nullptr, 0x180114ce0 },
    { 0x050, 8, SK_IMM, 0x0ull, nullptr, 0x180114ce3 },
    { 0x058, 8, SK_IMM, 0x0ull, nullptr, 0x180114ce7 },
    { 0x060, 4, SK_IMM, 0xffffffffull, nullptr, 0x180114ceb },
    { 0x068, 8, SK_IMM, 0x0ull, nullptr, 0x180114cf2 },
    { 0x070, 8, SK_IMM, 0x0ull, nullptr, 0x180114cf6 },
    { 0x078, 8, SK_IMM, 0x0ull, nullptr, 0x180114cfa },
    { 0x080, 8, SK_IMM, 0x0ull, nullptr, 0x180114cfe },
    { 0x088, 8, SK_IMM, 0x0ull, nullptr, 0x180114d05 },
    { 0x090, 8, SK_IMM, 0xffffffffffffffffull, nullptr, 0x180114d0c },
    { 0x098, 1, SK_IMM, 0x0ull, nullptr, 0x180114d17 },
    { 0x0a0, 8, SK_IMM, 0x0ull, nullptr, 0x180114d1d },
    { 0x0a8, 8, SK_IMM, 0x0ull, nullptr, 0x180114d24 },
};

const Spec kSpecs[] = {
    { "ComboDialog", 0xd0ull, 8,
      "??0DialogInstance@UI@@QEAA@XZ", kComboDialog,
      (uint16_t)(sizeof(kComboDialog)/sizeof(Store)),
      "?CreateNewObject@ComboDialog@UI@@SAPEAVBase@Object@@XZ", 0x180074850ull,
      "?CreateInPlaceObject@ComboDialog@UI@@SAPEAVBase@Object@@PEAX@Z", 0x1800748f0ull,
      nullptr, 0ull },
    { "DialogInstance", 0xb8ull, 8,
      "??0Internal@Object@@QEAA@XZ", kDialogInstance,
      (uint16_t)(sizeof(kDialogInstance)/sizeof(Store)),
      "?CreateNewObject@DialogInstance@UI@@SAPEAVBase@Object@@XZ", 0x1800454f0ull,
      nullptr, 0ull,
      "??0DialogInstance@UI@@QEAA@XZ", 0x180114ca0ull },
    { "Element", 0x0ull, 0,
      nullptr, nullptr,
      0,
      "?CreateNewObject@Element@UI@@SAPEAVBase@Object@@XZ", 0x180035450ull,
      nullptr, 0ull,
      nullptr, 0ull },
    { "ElementDescription", 0x0ull, 0,
      nullptr, nullptr,
      0,
      "?CreateNewObject@ElementDescription@UI@@SAPEAVBase@Object@@XZ", 0x180035100ull,
      "?CreateInPlaceObject@ElementDescription@UI@@SAPEAVBase@Object@@PEAX@Z", 0x180035110ull,
      nullptr, 0ull },
};
// END GENERATED SECTION
