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

// (ENDGAME B.3.9 UI-D: the DialogInstance row -- its Store list, factory
// and ??0DialogInstance@UI@@QEAA@XZ door -- is the class's own now,
// runtime/src/ui/dialog.cpp.)

const Spec kSpecs[] = {
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
