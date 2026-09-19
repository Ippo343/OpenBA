// Parent vtables, who stamps each Video/Graphics vtable, and sizeof from the deleting dtor.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

public class VgDump2 extends GhidraScript {

    SymbolTable st;
    Memory mem;
    FunctionManager fm;
    AddressSpace sp;
    Listing lst;
    ReferenceManager rm;

    String symName(Address a) {
        String prim = null, mangled = null;
        for (Symbol s : st.getSymbols(a)) {
            if (s.isPrimary()) prim = s.getName();
            if (s.getName().startsWith("?")) mangled = s.getName();
        }
        Function f = fm.getFunctionAt(a);
        if (prim == null) prim = (f != null) ? f.getName() : "NOSYM";
        return prim + (mangled != null ? " | " + mangled : "");
    }

    void dumpVt(String label, long addr, int max) {
        Address a = sp.getAddress(addr);
        println("=== " + label + " @ " + a);
        for (int i = 0; i < max; i++) {
            Address cur = a.add(i * 8L);
            long p;
            try { p = mem.getLong(cur); } catch (Exception e) { println("  -- unreadable"); return; }
            if (p == 0) { println(String.format("  [%2d] NULL", i)); return; }
            Address tgt;
            try { tgt = sp.getAddress(p); } catch (Exception e) { return; }
            if (!mem.contains(tgt)) { println("  -- end (0x" + Long.toHexString(p) + ")"); return; }
            if (fm.getFunctionAt(tgt) == null && fm.getFunctionContaining(tgt) == null) {
                println("  -- end, not code (0x" + Long.toHexString(p) + ")"); return;
            }
            println(String.format("  [%2d] 0x%x  %s", i, p, symName(tgt)));
        }
    }

    @Override
    public void run() throws Exception {
        st = currentProgram.getSymbolTable();
        mem = currentProgram.getMemory();
        fm = currentProgram.getFunctionManager();
        sp = currentProgram.getAddressFactory().getDefaultAddressSpace();
        lst = currentProgram.getListing();
        rm = currentProgram.getReferenceManager();

        println("##################### PART 1: PARENT VTABLES");
        String[] parents = {
            "??_7Base@Object@@6B@", "??_7External@Object@@6B@",
            "??_7CompiledExternal@Object@@6B@", "??_7Internal@Object@@6B@",
            "??_7ExternalList@Object@@6B@"
        };
        for (String pn : parents) {
            List<Symbol> ss = new ArrayList<>();
            for (Symbol s : st.getGlobalSymbols(pn)) ss.add(s);
            if (ss.isEmpty()) { println("=== " + pn + " NOT FOUND"); continue; }
            dumpVt(pn, ss.get(0).getAddress().getOffset(), 16);
            println("");
        }

        println("##################### PART 2: VTABLE XREFS (who stamps) + SIZEOF");
        List<Symbol> vts = new ArrayList<>();
        for (Symbol s : st.getAllSymbols(true)) {
            String n = s.getName();
            if (n.startsWith("??_7") && (n.contains("@Video@@6B@") || n.contains("@Graphics@@6B@"))) vts.add(s);
        }
        Collections.sort(vts, new Comparator<Symbol>() {
            public int compare(Symbol a, Symbol b) { return a.getName().compareTo(b.getName()); }
        });

        for (Symbol s : vts) {
            Address a = s.getAddress();
            println("");
            println("### " + s.getName() + " @ " + a);
            // who references it
            ReferenceIterator it = rm.getReferencesTo(a);
            List<String> refs = new ArrayList<>();
            while (it.hasNext()) {
                Reference r = it.next();
                Address from = r.getFromAddress();
                Function f = fm.getFunctionContaining(from);
                refs.add("    ref " + from + " in " + (f != null ? symName(f.getEntryPoint()) : "<no fn>"));
            }
            println("  STAMPED/REFERENCED BY " + refs.size() + " site(s):");
            for (String r : refs) println(r);
            // slot 0 = deleting dtor -> scan for MOV EDX/ECX,imm (the size) and the calls
            long p0;
            try { p0 = mem.getLong(a); } catch (Exception e) { continue; }
            Address d0 = sp.getAddress(p0);
            Function dtor = fm.getFunctionContaining(d0);
            if (dtor == null) { println("  slot0 not a function"); continue; }
            println("  slot0 dtor " + d0 + " " + symName(dtor.getEntryPoint())
                    + " body " + dtor.getBody().getNumAddresses() + " bytes");
            InstructionIterator ii = lst.getInstructions(dtor.getBody(), true);
            StringBuilder sb = new StringBuilder();
            while (ii.hasNext()) {
                Instruction ins = ii.next();
                String mn = ins.getMnemonicString();
                if (mn.equals("MOV") || mn.equals("LEA") || mn.equals("CALL") || mn.equals("JMP")) {
                    sb.append("      ").append(ins.getAddress()).append("  ").append(ins).append("\n");
                }
            }
            println("  slot0 body (MOV/LEA/CALL/JMP only):");
            print(sb.toString());
        }
    }
}
