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

const Store kCharacterSet[] = {
    { 0x000, 8, SK_VTABLE, 0, "??_7Base@Object@@6B@", 0x1800375a3 },
    { 0x000, 8, SK_VTABLE, 0, "??_7External@Object@@6B@", 0x1800375ad },
    { 0x008, 8, SK_IMM, 0x0ull, nullptr, 0x1800375bb },
    { 0x010, 8, SK_IMM, 0x0ull, nullptr, 0x1800375be },
    { 0x018, 8, SK_IMM, 0x0ull, nullptr, 0x1800375c2 },
    { 0x000, 8, SK_VTABLE, 0, "??_7CharacterSet@UI@@6B@", 0x1800375cd },
    { 0x020, 8, SK_IMM, 0x0ull, nullptr, 0x1800375d0 },
};
const Store kComboDialog[] = {
    { 0x000, 8, SK_VTABLE, 0, "??_7ComboDialog@UI@@6B@", 0x1800748be },
    { 0x0b8, 8, SK_IMM, 0x0ull, nullptr, 0x1800748c3 },
    { 0x0c0, 8, SK_IMM, 0x0ull, nullptr, 0x1800748ca },
    { 0x0c8, 8, SK_IMM, 0x0ull, nullptr, 0x1800748d1 },
};
const Store kCursor[] = {
    { 0x000, 8, SK_VTABLE, 0, "??_7Base@Object@@6B@", 0x18003b0d3 },
    { 0x000, 8, SK_VTABLE, 0, "??_7External@Object@@6B@", 0x18003b0dd },
    { 0x008, 8, SK_IMM, 0x0ull, nullptr, 0x18003b0eb },
    { 0x010, 8, SK_IMM, 0x0ull, nullptr, 0x18003b0ee },
    { 0x018, 8, SK_IMM, 0x0ull, nullptr, 0x18003b0f2 },
    { 0x000, 8, SK_VTABLE, 0, "??_7Cursor@UI@@6B@", 0x18003b0fd },
    { 0x028, 8, SK_IMM, 0x0ull, nullptr, 0x18003b100 },
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
const Store kFont[] = {
    { 0x000, 8, SK_VTABLE, 0, "??_7Base@Object@@6B@", 0x180031b15 },
    { 0x000, 8, SK_VTABLE, 0, "??_7External@Object@@6B@", 0x180031b1f },
    { 0x008, 8, SK_IMM, 0x0ull, nullptr, 0x180031b2d },
    { 0x010, 8, SK_IMM, 0x0ull, nullptr, 0x180031b30 },
    { 0x018, 8, SK_IMM, 0x0ull, nullptr, 0x180031b34 },
    { 0x000, 8, SK_VTABLE, 0, "??_7Font@UI@@6B@", 0x180031b3f },
    { 0x020, 8, SK_IMM, 0x0ull, nullptr, 0x180031b42 },
    { 0x028, 8, SK_IMM, 0x0ull, nullptr, 0x180031b46 },
    { 0x030, 8, SK_IMM, 0x0ull, nullptr, 0x180031b4a },
    { 0x038, 8, SK_IMM, 0x0ull, nullptr, 0x180031b4e },
    { 0x040, 8, SK_IMM, 0x0ull, nullptr, 0x180031b52 },
    { 0x048, 8, SK_IMM, 0x0ull, nullptr, 0x180031b56 },
    { 0x050, 8, SK_IMM, 0x0ull, nullptr, 0x180031b5a },
    { 0x058, 4, SK_IMM, 0x0ull, nullptr, 0x180031b5e },
    { 0x060, 8, SK_IMM, 0x0ull, nullptr, 0x180031b61 },
    { 0x068, 8, SK_IMM, 0x0ull, nullptr, 0x180031b65 },
    { 0x070, 8, SK_IMM, 0x0ull, nullptr, 0x180031b69 },
    { 0x078, 8, SK_IMM, 0x0ull, nullptr, 0x180031b6d },
    { 0x080, 8, SK_IMM, 0x0ull, nullptr, 0x180031b71 },
    { 0x088, 8, SK_IMM, 0x0ull, nullptr, 0x180031b78 },
};

const Spec kSpecs[] = {
    { "CharacterSet", 0x28ull, 8,
      nullptr, kCharacterSet,
      (uint16_t)(sizeof(kCharacterSet)/sizeof(Store)),
      "?CreateNewObject@CharacterSet@UI@@SAPEAVBase@Object@@XZ", 0x180037540ull,
      "?CreateInPlaceObject@CharacterSet@UI@@SAPEAVBase@Object@@PEAX@Z", 0x1800375f0ull,
      nullptr, 0ull },
    { "ComboDialog", 0xd0ull, 8,
      "??0DialogInstance@UI@@QEAA@XZ", kComboDialog,
      (uint16_t)(sizeof(kComboDialog)/sizeof(Store)),
      "?CreateNewObject@ComboDialog@UI@@SAPEAVBase@Object@@XZ", 0x180074850ull,
      "?CreateInPlaceObject@ComboDialog@UI@@SAPEAVBase@Object@@PEAX@Z", 0x1800748f0ull,
      nullptr, 0ull },
    { "Cursor", 0x30ull, 8,
      nullptr, kCursor,
      (uint16_t)(sizeof(kCursor)/sizeof(Store)),
      "?CreateNewObject@Cursor@UI@@SAPEAVBase@Object@@XZ", 0x18003b070ull,
      "?CreateInPlaceObject@Cursor@UI@@SAPEAVBase@Object@@PEAX@Z", 0x18003b120ull,
      nullptr, 0ull },
    { "DialogController", 0x0ull, 0,
      nullptr, nullptr,
      0,
      "?CreateNewObject@DialogController@UI@@SAPEAVBase@Object@@XZ", 0x18003b6e0ull,
      "?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z", 0x18003b6f0ull,
      nullptr, 0ull },
    { "DialogControllerConfig", 0x0ull, 0,
      nullptr, nullptr,
      0,
      "?CreateNewObject@DialogControllerConfig@UI@@SAPEAVBase@Object@@XZ", 0x18003b480ull,
      "?CreateInPlaceObject@DialogControllerConfig@UI@@SAPEAVBase@Object@@PEAX@Z", 0x18003b490ull,
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
    { "Font", 0x90ull, 8,
      nullptr, kFont,
      (uint16_t)(sizeof(kFont)/sizeof(Store)),
      "?CreateNewObject@Font@UI@@SAPEAVBase@Object@@XZ", 0x180031ab0ull,
      "?CreateInPlaceObject@Font@UI@@SAPEAVBase@Object@@PEAX@Z", 0x180031ba0ull,
      nullptr, 0ull },
};
// END GENERATED SECTION
