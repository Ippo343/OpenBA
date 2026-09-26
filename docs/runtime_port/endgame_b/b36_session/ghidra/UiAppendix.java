// B36_SLOTS.md source data: every UI vtable slot, the parent vtables, the
// _TypeName/_ParentTypeName literals, the hidden-retslot exports, who stamps each
// vtable, and the per-class export lists. Read from the RETAIL Runtime-x64.dll.
// Adapted from b35_session/ghidra/VgAppendix.java; the parent map is DERIVED from
// the _ParentTypeName literals read in this run, not seeded by hand.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

public class UiAppendix extends GhidraScript {

    SymbolTable st;
    Memory mem;
    FunctionManager fm;
    AddressSpace sp;
    Listing lst;

    static final String NS = "UI";

    Map<String, Long> vtOf = new HashMap<>();
    Map<String, String> parentOf = new HashMap<>();

    void seed() {
        vtOf.put("Object::Base", 0x180154788L);
        vtOf.put("Object::External", 0x1801547a0L);
        vtOf.put("Object::CompiledExternal", 0x180154a30L);
        vtOf.put("Object::Internal", 0x180154720L);
        vtOf.put("Object::ExternalList", 0x180154800L);
        parentOf.put("Object::External", "Object::Base");
        parentOf.put("Object::CompiledExternal", "Object::External");
        parentOf.put("Object::ExternalList", "Object::External");
        parentOf.put("Object::Internal", "Object::Base");
    }

    String[] nameFragments(String m) {
        if (!m.startsWith("?")) return null;
        int i = 1;
        if (m.length() > 1 && m.charAt(1) == '?') {
            i = (m.length() > 2 && m.charAt(2) == '_') ? 4 : 3;
        }
        List<String> parts = new ArrayList<>();
        StringBuilder sb = new StringBuilder();
        while (i < m.length()) {
            char c = m.charAt(i);
            if (c == '@') {
                if (sb.length() == 0) break;
                parts.add(sb.toString());
                sb.setLength(0);
            } else {
                sb.append(c);
            }
            i++;
        }
        if (parts.size() < 2) return null;
        return parts.toArray(new String[0]);
    }

    String mangledAt(Address a) {
        String mangled = null, prim = null;
        for (Symbol s : st.getSymbols(a)) {
            if (s.getName().startsWith("?")) mangled = s.getName();
            if (s.isPrimary()) prim = s.getName();
        }
        if (mangled != null) return mangled;
        Function f = fm.getFunctionAt(a);
        if (prim == null) prim = (f != null) ? f.getName() : "NOSYM";
        if (prim.equals("_purecall")) return "_purecall";
        return "(unexported) " + prim;
    }

    List<Long> readVt(long va, Set<Long> vtAddrs) {
        List<Long> out = new ArrayList<>();
        Address a = sp.getAddress(va);
        for (int i = 0; i < 64; i++) {
            Address cur = a.add(i * 8L);
            if (i > 0 && vtAddrs.contains(cur.getOffset())) break;
            if (i > 0 && hasVtSymbol(cur)) break;
            long p;
            try { p = mem.getLong(cur); } catch (Exception e) { break; }
            if (p == 0) break;
            Address tgt;
            try { tgt = sp.getAddress(p); } catch (Exception e) { break; }
            if (!mem.contains(tgt)) break;
            if (fm.getFunctionAt(tgt) == null && fm.getFunctionContaining(tgt) == null) break;
            out.add(p);
        }
        return out;
    }

    // Any other class's `??_7` symbol (or any data symbol) at this address ends the array:
    // the next vtable in .rdata is not always one of ours.
    boolean hasVtSymbol(Address a) {
        for (Symbol s : st.getSymbols(a)) {
            String n = s.getName();
            if (n.startsWith("??_7")) return true;
            if (n.startsWith("??_R")) return true;
        }
        return false;
    }

    String sizeOf(long vtVa) {
        try {
            long p0 = mem.getLong(sp.getAddress(vtVa));
            Function d = fm.getFunctionContaining(sp.getAddress(p0));
            if (d == null) return "?";
            InstructionIterator ii = lst.getInstructions(d.getBody(), true);
            while (ii.hasNext()) {
                Instruction ins = ii.next();
                if (!ins.getMnemonicString().equals("MOV")) continue;
                String s = ins.toString();
                if (s.startsWith("MOV EDX,0x")) return s.substring(8);
            }
        } catch (Exception e) { /* fall through */ }
        return "?";
    }

    String retTypeOf(String mangled) {
        int i = mangled.indexOf("?A");
        if (i < 0) return "?";
        char k = mangled.charAt(i + 2);
        int j = i + 3;
        if (k == 'W') { while (j < mangled.length() && Character.isDigit(mangled.charAt(j))) j++; }
        List<String> frag = new ArrayList<>();
        StringBuilder sb = new StringBuilder();
        while (j < mangled.length()) {
            char c = mangled.charAt(j);
            if (c == '@') { if (sb.length() == 0) break; frag.add(sb.toString()); sb.setLength(0); }
            else sb.append(c);
            j++;
        }
        Collections.reverse(frag);
        String joined = String.join("::", frag);
        String kind = (k == 'V') ? "class" : (k == 'U') ? "struct" : "enum";
        return joined + "  [" + kind + " by value]";
    }

    String readCStr(Address a) {
        StringBuilder sb = new StringBuilder();
        try {
            for (int i = 0; i < 160; i++) {
                byte b = mem.getByte(a.add(i));
                if (b == 0) break;
                sb.append((char) (b & 0xff));
            }
        } catch (Exception e) { return "<unreadable>"; }
        return sb.toString();
    }

    String literalOf(Symbol s) {
        Function f = fm.getFunctionAt(s.getAddress());
        String lit = "<no fn>";
        if (f != null) {
            InstructionIterator ii = lst.getInstructions(f.getBody(), true);
            while (ii.hasNext()) {
                Instruction ins = ii.next();
                if (!ins.getMnemonicString().equals("LEA")) continue;
                for (Reference r : ins.getReferencesFrom()) {
                    if (mem.contains(r.getToAddress())) lit = readCStr(r.getToAddress());
                }
            }
        }
        return lit;
    }

    @Override
    public void run() throws Exception {
        st = currentProgram.getSymbolTable();
        mem = currentProgram.getMemory();
        fm = currentProgram.getFunctionManager();
        sp = currentProgram.getAddressFactory().getDefaultAddressSpace();
        lst = currentProgram.getListing();
        seed();

        println("PROGRAM " + currentProgram.getName() + " functions=" + fm.getFunctionCount()
                + " imagebase=" + currentProgram.getImageBase());

        String nsTag = "@" + NS + "@@";

        // ---- collect the UI vtables ------------------------------------------------
        Set<Long> vtAddrs = new HashSet<>();
        Map<String, Long> ui = new TreeMap<>();
        for (Symbol s : st.getAllSymbols(true)) {
            String n = s.getName();
            if (!n.startsWith("??_7")) continue;
            if (n.endsWith(nsTag + "6B@")) {
                String cls = n.substring(4, n.length() - 5);
                int at = cls.lastIndexOf('@');
                String key = cls.substring(at + 1) + "::" + cls.substring(0, at);
                ui.put(key, s.getAddress().getOffset());
                vtOf.put(key, s.getAddress().getOffset());
                vtAddrs.add(s.getAddress().getOffset());
            }
        }
        for (Long v : vtOf.values()) vtAddrs.add(v);

        // ---- SECTION 3 first (parents derive from it) -------------------------------
        List<String> lits = new ArrayList<>();
        Map<String, String> typeNameOf = new TreeMap<>();
        for (Symbol s : st.getAllSymbols(true)) {
            String n = s.getName();
            boolean isPar = n.startsWith("?_ParentTypeName@");
            boolean isTn = n.startsWith("?_TypeName@");
            if (!isPar && !isTn) continue;
            if (!n.contains(nsTag)) continue;
            String lit = literalOf(s);
            String[] f = nameFragments(n);
            String cls = (f != null && f.length >= 3) ? f[2] + "::" + f[1] : "?";
            lits.add(String.format("0x%s  %-58s -> \"%s\"", s.getAddress(), n, lit));
            if (isPar) {
                String par = lit.contains("::") ? lit : (NS + "::" + lit);
                parentOf.put(cls, par);
            } else {
                typeNameOf.put(cls, lit);
            }
        }
        Collections.sort(lits, new Comparator<String>() {
            public int compare(String a, String b) {
                return a.substring(12).compareTo(b.substring(12));
            }
        });

        println("");
        println("@@@@@@ SECTION 1 -- " + ui.size() + " minted-vtable classes (parent = _ParentTypeName literal, namespace-qualified by the rule 'bare means same namespace')");
        emit(ui, vtAddrs);

        Map<String, Long> parents = new java.util.LinkedHashMap<>();
        parents.put("Object::Base", vtOf.get("Object::Base"));
        parents.put("Object::External", vtOf.get("Object::External"));
        parents.put("Object::CompiledExternal", vtOf.get("Object::CompiledExternal"));
        parents.put("Object::Internal", vtOf.get("Object::Internal"));
        parents.put("Object::ExternalList", vtOf.get("Object::ExternalList"));
        println("");
        println("@@@@@@ SECTION 2 -- the five Object parent vtables");
        emit(parents, vtAddrs);

        println("");
        println("@@@@@@ SECTION 3 -- _TypeName / _ParentTypeName literals");
        println("count=" + lits.size());
        for (String l : lits) println(l);

        // ---- SECTION 4: hidden retslot exports -------------------------------------
        println("");
        println("@@@@@@ SECTION 4 -- hidden-retslot exports (?AV / ?AU); ?AW4 listed separately");
        List<String> rs = new ArrayList<>(), en = new ArrayList<>();
        for (Symbol s : st.getAllSymbols(true)) {
            String n = s.getName();
            if (!n.startsWith("?") || n.startsWith("??_") || n.startsWith("?_")) continue;
            if (!n.contains(nsTag)) continue;
            int i = n.indexOf("?A");
            if (i < 0 || i + 2 >= n.length()) continue;
            char k = n.charAt(i + 2);
            String row = String.format("0x%s  %-110s %s", s.getAddress(), n, retTypeOf(n));
            if (k == 'V' || k == 'U') rs.add(row);
            else if (k == 'W') en.add(row);
        }
        Collections.sort(rs); Collections.sort(en);
        println("retslot count=" + rs.size());
        for (String l : rs) println(l);
        println("-- NOT retslot: enum by value, returned in EAX. count=" + en.size());
        for (String l : en) println(l);

        // ---- SECTION 5: who stamps / references each vtable -------------------------
        println("");
        println("@@@@@@ SECTION 5 -- functions referencing each UI vtable address (stampers)");
        for (Map.Entry<String, Long> e : ui.entrySet()) {
            Address va = sp.getAddress(e.getValue());
            List<String> refs = new ArrayList<>();
            ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(va);
            while (ri.hasNext()) {
                Reference r = ri.next();
                Function f = fm.getFunctionContaining(r.getFromAddress());
                String who = (f == null) ? ("<no fn> " + r.getFromAddress()) : mangledAt(f.getEntryPoint()) + " @0x" + f.getEntryPoint();
                if (!refs.contains(who)) refs.add(who);
            }
            Collections.sort(refs);
            println(String.format("### %s  vtable=0x%x  refs=%d", e.getKey(), e.getValue(), refs.size()));
            for (String r : refs) println("    " + r);
        }

        // ---- SECTION 6: every export, bucketed by class ---------------------------
        println("");
        println("@@@@@@ SECTION 6 -- every " + NS + " export, by class (nested classes bucket as Outer::Inner)");
        Map<String, List<String>> byCls = new TreeMap<>();
        for (Symbol s : st.getAllSymbols(true)) {
            String n = s.getName();
            if (!n.startsWith("?")) continue;
            if (!n.contains(nsTag)) continue;
            String[] f = nameFragments(n);
            if (f == null) continue;
            int nsIdx = -1;
            for (int k = 0; k < f.length; k++) if (f[k].equals(NS)) { nsIdx = k; break; }
            if (nsIdx < 1) continue;
            // fragments innermost-first: [member, Inner, Outer, NS] -> class Outer::Inner
            StringBuilder key = new StringBuilder(NS);
            for (int k = nsIdx - 1; k >= 1; k--) key.append("::").append(f[k]);
            if (nsIdx == 1) key.append("::").append(f[0]);   // ??0Class@UI@@ style: class is f[0]
            boolean opShape = n.startsWith("??");
            if (opShape && nsIdx >= 1) {
                // operator names: fragments are [Class, ..., NS]; f[0] IS the class
                key = new StringBuilder(NS);
                for (int k = nsIdx - 1; k >= 0; k--) key.append("::").append(f[k]);
            }
            String kk = key.toString();
            List<String> l = byCls.get(kk);
            if (l == null) { l = new ArrayList<>(); byCls.put(kk, l); }
            l.add(String.format("0x%s  %s", s.getAddress(), n));
        }
        ctorStamps(ui);
        println("");
        println("classes=" + byCls.size());
        for (Map.Entry<String, List<String>> e : byCls.entrySet()) {
            List<String> l = e.getValue();
            Collections.sort(l, new Comparator<String>() {
                public int compare(String a, String b) {
                    return a.substring(14).compareTo(b.substring(14));
                }
            });
            String vt = vtOf.containsKey(e.getKey()) ? String.format(" vtable=0x%x", vtOf.get(e.getKey())) : " (no vtable export)";
            println("");
            println("##### " + e.getKey() + "   exports=" + l.size() + vt);
            for (String x : l) println("    " + x);
        }
    }

    // Section 7 helper: every `??_7` referenced from a function, in address order of the
    // referencing instruction, with the instruction text (shows the stamp chain and any
    // stamp at a non-zero `this` offset = secondary vptr).
    void ctorStamps(Map<String, Long> ui) {
        println("");
        println("@@@@@@ SECTION 7 -- vtable stamps inside each class's ctor(s) and dtor(s) (secondary-vptr check)");
        for (String cls : ui.keySet()) {
            String bare = cls.substring(cls.indexOf("::") + 2);
            for (Symbol s : st.getAllSymbols(true)) {
                String n = s.getName();
                if (!(n.startsWith("??0" + bare + "@" + NS + "@@") || n.startsWith("??1" + bare + "@" + NS + "@@"))) continue;
                Function f = fm.getFunctionAt(s.getAddress());
                if (f == null) continue;
                println(String.format("### %s  %s @0x%s", cls, n, s.getAddress()));
                InstructionIterator ii = lst.getInstructions(f.getBody(), true);
                while (ii.hasNext()) {
                    Instruction ins = ii.next();
                    for (Reference r : ins.getReferencesFrom()) {
                        Address t = r.getToAddress();
                        for (Symbol ts : st.getSymbols(t)) {
                            if (ts.getName().startsWith("??_7")) {
                                Instruction nx = ins.getNext();
                                println(String.format("    0x%s  %s  -> %s   | next: %s", ins.getAddress(), ins, ts.getName(), nx == null ? "" : nx.toString()));
                            }
                        }
                    }
                    if (ins.getMnemonicString().equals("CALL")) {
                        for (Reference r : ins.getReferencesFrom()) {
                            Address t = r.getToAddress();
                            String m = mangledAt(t);
                            if (m.startsWith("??0") || m.startsWith("??1")) println(String.format("    0x%s  CALL %s", ins.getAddress(), m));
                        }
                    }
                }
            }
        }
    }

    void emit(Map<String, Long> classes, Set<Long> vtAddrs) {
        for (Map.Entry<String, Long> e : classes.entrySet()) {
            String cls = e.getKey();
            long va = e.getValue();
            String par = parentOf.get(cls);
            Long parVa = (par == null) ? null : vtOf.get(par);
            List<Long> pslots = (parVa == null) ? new ArrayList<Long>() : readVt(parVa, vtAddrs);
            List<Long> slots = readVt(va, vtAddrs);
            println("");
            println(String.format("### %s  vtable=0x%x  slots=%d  sizeof=%s  parent=%s%s",
                    cls, va, slots.size(), sizeOf(va),
                    par == null ? "ROOT/UNKNOWN (no _ParentTypeName)" : par,
                    (par != null && parVa == null) ? "  (parent vtable NOT in table)" : ""));
            for (int i = 0; i < slots.size(); i++) {
                long t = slots.get(i);
                String inh = "";
                if (i < pslots.size() && pslots.get(i).longValue() == t) {
                    inh = "   inherited-from " + par;
                }
                println(String.format("  [%2d] +0x%02x 0x%x  %s%s",
                        i, i * 8, t, mangledAt(sp.getAddress(t)), inh));
            }
        }
    }
}
