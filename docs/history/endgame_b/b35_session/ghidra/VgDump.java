// Dump Video/Graphics vtables from the retail Runtime-x64.dll
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class VgDump extends GhidraScript {

    SymbolTable st;
    Memory mem;
    FunctionManager fm;
    AddressSpace sp;

    String symName(Address a) {
        String prim = null, mangled = null;
        for (Symbol s : st.getSymbols(a)) {
            if (s.isPrimary()) prim = s.getName();
            if (s.getName().startsWith("?")) mangled = s.getName();
        }
        Function f = fm.getFunctionAt(a);
        if (prim == null) prim = (f != null) ? f.getName() : "NOSYM";
        return prim + " | " + (mangled != null ? mangled : "-");
    }

    @Override
    public void run() throws Exception {
        st = currentProgram.getSymbolTable();
        mem = currentProgram.getMemory();
        fm = currentProgram.getFunctionManager();
        sp = currentProgram.getAddressFactory().getDefaultAddressSpace();

        String filter = "";
        String[] args = getScriptArgs();
        if (args != null && args.length > 0) filter = args[0];

        List<Symbol> vts = new ArrayList<>();
        Set<Long> vtOff = new HashSet<>();
        for (Symbol s : st.getAllSymbols(true)) {
            String n = s.getName();
            if (n.startsWith("??_7") && (n.contains("@Video@@6B@") || n.contains("@Graphics@@6B@"))) {
                if (filter.isEmpty() || n.contains(filter)) {
                    vts.add(s);
                }
                vtOff.add(s.getAddress().getOffset());
            }
        }
        Collections.sort(vts, new Comparator<Symbol>() {
            public int compare(Symbol a, Symbol b) {
                return Long.compare(a.getAddress().getOffset(), b.getAddress().getOffset());
            }
        });

        println("### VTABLE COUNT = " + vts.size());
        for (Symbol s : vts) {
            Address a = s.getAddress();
            println("");
            println("=== " + s.getName() + " @ " + a);
            for (int i = 0; i < 64; i++) {
                Address cur = a.add(i * 8L);
                if (i > 0 && vtOff.contains(cur.getOffset())) { println("  -- next vtable here, stop"); break; }
                long p;
                try { p = mem.getLong(cur); } catch (Exception e) { println("  -- unreadable, stop"); break; }
                if (p == 0) { println(String.format("  [%2d] +0x%02x NULL", i, i * 8)); break; }
                Address tgt;
                try { tgt = sp.getAddress(p); } catch (Exception e) { println("  -- bad ptr 0x" + Long.toHexString(p)); break; }
                if (!mem.contains(tgt)) { println("  -- 0x" + Long.toHexString(p) + " outside image, stop"); break; }
                if (fm.getFunctionAt(tgt) == null && fm.getFunctionContaining(tgt) == null) {
                    println("  -- 0x" + Long.toHexString(p) + " not code, stop"); break;
                }
                println(String.format("  [%2d] +0x%02x 0x%x  %s", i, i * 8, p, symName(tgt)));
            }
        }
    }
}
