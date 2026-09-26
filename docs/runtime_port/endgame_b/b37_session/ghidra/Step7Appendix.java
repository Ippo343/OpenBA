// B37_SLOTS.md source data for ENDGAME B.3 step 7: the namespaces Engine, Save, Achievement,
// Audio, Input, Pathfinding, Picking and Application (plus System::Thread, the base of
// Engine::Loader and Audio::Stream). Read from the RETAIL Runtime-x64.dll only.
// Adapted from b36_session/ghidra/UiAppendix.java (vtables terminated on the next ??_7/??_R
// symbol, sizeof from the slot-0 deleting dtor's MOV EDX,imm, parents from _ParentTypeName
// literals). Additions for step 7:
//   - a namespace LIST, with MSVC name back-references (`@Application@1@`) resolved;
//   - every export's demangled signature (Ghidra's MicrosoftDemangler);
//   - the first instructions of EVERY ctor/dtor export of every class in the set (not only the
//     polymorphic ones) with every LEA into non-code memory and its symbol, so a class with no
//     exported vtable can be settled from its first store;
//   - the prologue of every export that takes a class BY VALUE as an argument (B6-11);
//   - the slot-0 body of each vtable disassembled up to its first RET (size + which dtor it calls).
// Read-only: no rename, no annotation.
import ghidra.app.script.GhidraScript;
import ghidra.app.util.demangler.DemangledObject;
import ghidra.app.util.demangler.microsoft.MicrosoftDemangler;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

public class Step7Appendix extends GhidraScript {

    SymbolTable st;
    Memory mem;
    FunctionManager fm;
    AddressSpace sp;
    Listing lst;
    MicrosoftDemangler dm = new MicrosoftDemangler();

    static final Set<String> NSS = new HashSet<>(Arrays.asList(
            "Engine", "Save", "Achievement", "Audio", "Input", "Pathfinding", "Picking", "Application"));

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

    // Qualified-name fragments, innermost first, with MSVC digit back-references resolved
    // (`?_singleton@Application@1@` -> [_singleton, Application, Application]).
    // For ??_7 / ??0 / ??1 / ??_G the special-name prefix is skipped and fragment 0 is the class.
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
                i++;
                continue;
            }
            if (sb.length() == 0 && Character.isDigit(c)) {
                int d = c - '0';
                String ref = d < parts.size() ? parts.get(d) : (parts.isEmpty() ? "?" : parts.get(0));
                parts.add(ref);
                i++;
                continue;
            }
            if (sb.length() == 0 && c == '?' ) {
                // template name ?$X@...@ inside a qualifier: keep it opaque up to its '@@'
                int end = m.indexOf("@@", i);
                if (end < 0) break;
                parts.add(m.substring(i, end + 1));
                i = end + 2;
                continue;
            }
            sb.append(c);
            i++;
        }
        if (parts.size() < 2) return null;
        return parts.toArray(new String[0]);
    }

    // "NS::Class" (or "NS::Outer::Inner", or "NS::<free>") for a symbol in the set, else null.
    String classKey(String n) {
        String[] f = nameFragments(n);
        if (f == null) return null;
        String ns = f[f.length - 1];
        if (!NSS.contains(ns)) return null;
        boolean special = n.startsWith("??");
        StringBuilder key = new StringBuilder(ns);
        int lo = special ? 0 : 1;
        if (!special && f.length == 2) return ns + "::<free>";
        for (int k = f.length - 2; k >= lo; k--) key.append("::").append(f[k]);
        return key.toString();
    }

    String demangle(String n, Address a) {
        try {
            DemangledObject o = dm.demangle(dm.createMangledContext(n, null, currentProgram, a));
            if (o == null) return "<no demangle>";
            return o.getSignature(false);
        } catch (Exception e) {
            return "<demangle failed: " + e.getMessage() + ">";
        }
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

    boolean hasVtSymbol(Address a) {
        for (Symbol s : st.getSymbols(a)) {
            String n = s.getName();
            if (n.startsWith("??_7")) return true;
            if (n.startsWith("??_R")) return true;
        }
        return false;
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
        return "(no MOV EDX,imm in slot 0)";
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

    String blockName(Address a) {
        MemoryBlock b = mem.getBlock(a);
        return b == null ? "?" : b.getName();
    }

    // Symbol names at a data address, or "(no symbol, <block>)".
    String dataName(Address t) {
        StringBuilder sb = new StringBuilder();
        for (Symbol ts : st.getSymbols(t)) {
            if (sb.length() > 0) sb.append(" | ");
            sb.append(ts.getName());
        }
        if (sb.length() == 0) return "(no symbol, " + blockName(t) + ")";
        return sb.toString() + " (" + blockName(t) + ")";
    }

    // First `max` instructions of a function (stopping at the first RET), each with the
    // name of any referenced non-code address and any called function.
    void dumpHead(Function f, int max, String indent) {
        InstructionIterator ii = lst.getInstructions(f.getBody(), true);
        int n = 0;
        while (ii.hasNext() && n < max) {
            Instruction ins = ii.next();
            StringBuilder extra = new StringBuilder();
            for (Reference r : ins.getReferencesFrom()) {
                Address t = r.getToAddress();
                if (!mem.contains(t)) continue;
                Function tf = fm.getFunctionAt(t);
                if (tf != null) extra.append("  => ").append(mangledAt(t));
                else extra.append("  => ").append(dataName(t));
            }
            println(String.format("%s0x%s  %-48s%s", indent, ins.getAddress(), ins.toString(), extra));
            n++;
            if (ins.getMnemonicString().equals("RET")) break;
        }
    }

    // Split a demangled parameter list at top-level commas.
    List<String> params(String sig) {
        List<String> out = new ArrayList<>();
        int open = sig.indexOf('(');
        int close = sig.lastIndexOf(')');
        if (open < 0 || close < open) return out;
        String body = sig.substring(open + 1, close);
        int depth = 0;
        StringBuilder sb = new StringBuilder();
        for (char c : body.toCharArray()) {
            if (c == '<' || c == '(') depth++;
            if (c == '>' || c == ')') depth--;
            if (c == ',' && depth == 0) { out.add(sb.toString().trim()); sb.setLength(0); continue; }
            sb.append(c);
        }
        if (sb.toString().trim().length() > 0) out.add(sb.toString().trim());
        return out;
    }

    boolean byValueClass(String p) {
        String q = p.trim();
        if (!(q.startsWith("class ") || q.startsWith("struct ") || q.startsWith("union ")
                || q.startsWith("const class ") || q.startsWith("const struct "))) return false;
        if (q.endsWith("*") || q.endsWith("&") || q.contains("* ") || q.contains("& ")
                || q.endsWith("*const") || q.endsWith("&const")) return false;
        // strip template args to test for a trailing pointer/reference
        int depth = 0;
        StringBuilder outer = new StringBuilder();
        for (char c : q.toCharArray()) {
            if (c == '<') depth++;
            else if (c == '>') depth--;
            else if (depth == 0) outer.append(c);
        }
        String o = outer.toString();
        return !(o.contains("*") || o.contains("&"));
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
        if (currentProgram.getName().equals("Application-x64.exe") && fm.getFunctionCount() == 11473) {
            runExe();
            return;
        }
        if (!currentProgram.getName().equals("Runtime-x64.dll") || fm.getFunctionCount() != 15290) {
            println("ABORT: not the retail Runtime-x64.dll");
            return;
        }

        // ---- collect every symbol once --------------------------------------------------
        List<Symbol> all = new ArrayList<>();
        for (Symbol s : st.getAllSymbols(true)) all.add(s);

        // ---- the vtables in the set (+ System::Thread) ----------------------------------
        Set<Long> vtAddrs = new HashSet<>();
        Map<String, Long> set = new TreeMap<>();
        for (Symbol s : all) {
            String n = s.getName();
            if (!n.startsWith("??_7")) continue;
            vtAddrs.add(s.getAddress().getOffset());
            String key = classKey(n);
            if (n.equals("??_7Thread@System@@6B@")) key = "System::Thread";
            if (key == null) continue;
            set.put(key, s.getAddress().getOffset());
            vtOf.put(key, s.getAddress().getOffset());
        }
        for (Long v : vtOf.values()) vtAddrs.add(v);

        // ---- literals ----------------------------------------------------------------
        List<String> lits = new ArrayList<>();
        for (Symbol s : all) {
            String n = s.getName();
            boolean isPar = n.startsWith("?_ParentTypeName@");
            boolean isTn = n.startsWith("?_TypeName@");
            if (!isPar && !isTn) continue;
            String key = classKey(n);
            if (key == null) continue;
            String lit = literalOf(s);
            lits.add(String.format("0x%s  %-64s -> \"%s\"", s.getAddress(), n, lit));
            if (isPar) {
                String ns = key.substring(0, key.indexOf("::"));
                String par = lit.contains("::") ? lit : (ns + "::" + lit);
                parentOf.put(key, par);
            }
        }
        parentOf.put("System::Thread", null);
        Collections.sort(lits);

        println("");
        println("@@@@@@ SECTION 1 -- " + set.size() + " exported vtables in the step-7 set (+System::Thread)");
        emit(set, vtAddrs);

        Map<String, Long> parents = new java.util.LinkedHashMap<>();
        for (String p : new String[]{"Object::Base", "Object::External", "Object::CompiledExternal", "Object::Internal", "Object::ExternalList"})
            parents.put(p, vtOf.get(p));
        println("");
        println("@@@@@@ SECTION 2 -- the five Object parent vtables");
        emit(parents, vtAddrs);

        println("");
        println("@@@@@@ SECTION 3 -- _TypeName / _ParentTypeName literals in the set");
        println("count=" + lits.size());
        for (String l : lits) println(l);

        // ---- SECTION 4: retslot exports -------------------------------------------------
        println("");
        println("@@@@@@ SECTION 4 -- hidden-retslot exports (?AV / ?AU) in the set; ?AW4 listed separately");
        List<String> rs = new ArrayList<>(), en = new ArrayList<>();
        for (Symbol s : all) {
            String n = s.getName();
            if (!n.startsWith("?") || n.startsWith("??_") || n.startsWith("?_")) continue;
            if (classKey(n) == null) continue;
            // the return type follows the access/cv code; find the first ?A after the '@@'+code
            int q = n.indexOf("@@");
            if (q < 0) continue;
            int i = n.indexOf("?A", q);
            if (i < 0 || i + 2 >= n.length()) continue;
            // only a return type: it must sit right after the calling convention letters
            String tail = n.substring(q + 2, i);
            if (tail.length() > 5) continue;
            char k = n.charAt(i + 2);
            String virt = (tail.startsWith("U") || tail.startsWith("E") || tail.startsWith("M")) ? "VIRTUAL" : (tail.startsWith("S") || tail.startsWith("K") || tail.startsWith("C") || tail.startsWith("Y")) ? "static/free" : "non-virtual";
            String row = String.format("0x%s  %-7s %-110s %s", s.getAddress(), virt, n, retTypeOf(n));
            if (k == 'V' || k == 'U') rs.add(row);
            else if (k == 'W') en.add(row);
        }
        Collections.sort(rs); Collections.sort(en);
        println("retslot count=" + rs.size());
        for (String l : rs) println(l);
        println("-- NOT retslot: enum by value, returned in EAX. count=" + en.size());
        for (String l : en) println(l);

        // ---- SECTION 5: stampers ------------------------------------------------------
        println("");
        println("@@@@@@ SECTION 5 -- functions referencing each vtable address (stampers)");
        for (Map.Entry<String, Long> e : set.entrySet()) {
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

        // ---- SECTION 6: every export by class, demangled --------------------------------
        println("");
        println("@@@@@@ SECTION 6 -- every export in the set, by class, with Ghidra's demangling");
        Map<String, List<String>> byCls = new TreeMap<>();
        Map<String, List<Symbol>> symsByCls = new TreeMap<>();
        for (Symbol s : all) {
            String n = s.getName();
            if (!n.startsWith("?")) continue;
            String key = classKey(n);
            if (key == null) continue;
            if (!byCls.containsKey(key)) { byCls.put(key, new ArrayList<String>()); symsByCls.put(key, new ArrayList<Symbol>()); }
            byCls.get(key).add(String.format("0x%s  %s\n                    %s", s.getAddress(), n, demangle(n, s.getAddress())));
            symsByCls.get(key).add(s);
        }
        println("classes=" + byCls.size());
        for (Map.Entry<String, List<String>> e : byCls.entrySet()) {
            List<String> l = e.getValue();
            Collections.sort(l);
            String vt = vtOf.containsKey(e.getKey()) ? String.format(" vtable=0x%x", vtOf.get(e.getKey())) : " (no vtable export)";
            println("");
            println("##### " + e.getKey() + "   exports=" + l.size() + vt);
            for (String x : l) println("    " + x);
        }

        // ---- SECTION 7: every ctor/dtor export's head, every class (first-store test) ----
        println("");
        println("@@@@@@ SECTION 7 -- head of every ??0 / ??1 export in the set (first stores, vtable LEAs, base calls)");
        for (Map.Entry<String, List<Symbol>> e : symsByCls.entrySet()) {
            for (Symbol s : e.getValue()) {
                String n = s.getName();
                if (!(n.startsWith("??0") || n.startsWith("??1"))) continue;
                Function f = fm.getFunctionAt(s.getAddress());
                if (f == null) continue;
                println("");
                println(String.format("### %s  %s @0x%s", e.getKey(), n, s.getAddress()));
                dumpHead(f, 40, "    ");
            }
        }

        // ---- SECTION 8: by-value class arguments, with prologues --------------------------
        println("");
        println("@@@@@@ SECTION 8 -- exports taking a class BY VALUE as an argument (prologue, B6-11)");
        int cnt = 0;
        for (Map.Entry<String, List<Symbol>> e : symsByCls.entrySet()) {
            for (Symbol s : e.getValue()) {
                String n = s.getName();
                if (!n.startsWith("?") || n.startsWith("??_7") || n.startsWith("?_")) continue;
                String sig = demangle(n, s.getAddress());
                List<String> bv = new ArrayList<>();
                List<String> ps = params(sig);
                for (int k = 0; k < ps.size(); k++) if (byValueClass(ps.get(k))) bv.add("arg" + (k + 1) + ": " + ps.get(k));
                if (bv.isEmpty()) continue;
                cnt++;
                println("");
                println(String.format("### %s  0x%s  %s", e.getKey(), s.getAddress(), n));
                println("    SIG " + sig);
                for (String b : bv) println("    BYVAL " + b);
                Function f = fm.getFunctionAt(s.getAddress());
                if (f == null) { println("    (not a function)"); continue; }
                dumpHead(f, 18, "    ");
            }
        }
        println("byvalue-arg exports=" + cnt);

        // ---- SECTION 9: slot-0 bodies (size + dtor called) ------------------------------
        println("");
        println("@@@@@@ SECTION 9 -- slot-0 bodies of each vtable in the set");
        for (Map.Entry<String, Long> e : set.entrySet()) {
            long p0 = mem.getLong(sp.getAddress(e.getValue()));
            Function f = fm.getFunctionContaining(sp.getAddress(p0));
            println("");
            println(String.format("### %s  slot0=0x%x", e.getKey(), p0));
            if (f == null) { println("    (no function)"); continue; }
            dumpHead(f, 40, "    ");
        }
    }

    // =====================================================================================
    // EXE MODE (Application-x64.exe, 11473 functions). Read-only.
    //   E1  the five exe loop-subclass vtables named in the brief, slot by slot: each target
    //       is either an exe body or an import THUNK into Runtime-x64.dll (named);
    //   E2  every exe function that calls a Runtime ctor/dtor import of a step-7 class, with
    //       the instructions around the call (allocation size, vtable stamps after it);
    //   E3  the vtables E2 finds stamped right after a base-ctor call, dumped like E1
    //       (the exe Goal / GoalInstance family and any other subclass);
    //   E4  the import census: every Runtime import in the eight namespaces, with the exe
    //       functions that reach it (directly through the IAT or through a thunk).
    // =====================================================================================
    Map<Long, String> thunkOf = new HashMap<>();   // thunk entry -> mangled import
    Map<Long, String> iatOf = new HashMap<>();     // IAT slot -> mangled import
    Map<String, List<Long>> importSites = new TreeMap<>(); // mangled -> code addresses referencing it

    String importName(Function ext) {
        try {
            ghidra.program.model.symbol.ExternalLocation el = ext.getExternalLocation();
            if (el != null) {
                String o = el.getOriginalImportedName();
                if (o != null) return el.getLibraryName() + "!" + o;
                return el.getLibraryName() + "!" + el.getLabel();
            }
        } catch (Exception e) { /* fall through */ }
        return "?!" + ext.getName();
    }

    void collectImports() {
        ghidra.program.model.symbol.ReferenceManager rm = currentProgram.getReferenceManager();
        ghidra.program.model.listing.FunctionIterator it = fm.getExternalFunctions();
        while (it.hasNext()) {
            Function ext = it.next();
            String nm = importName(ext);
            ReferenceIterator ri = rm.getReferencesTo(ext.getEntryPoint());
            while (ri.hasNext()) {
                Reference r = ri.next();
                Address from = r.getFromAddress();
                iatOf.put(from.getOffset(), nm);
            }
        }
        ghidra.program.model.listing.FunctionIterator fi = fm.getFunctions(true);
        while (fi.hasNext()) {
            Function f = fi.next();
            if (!f.isThunk()) continue;
            Function th = f.getThunkedFunction(true);
            if (th != null && th.isExternal()) thunkOf.put(f.getEntryPoint().getOffset(), importName(th));
        }
        // code sites. Ghidra attaches the external reference either to the IAT slot (data) or
        // directly to the instruction that reads it; an undefined `FF 25` JMP stub reads the
        // slot too, and its callers are the real sites.
        for (Map.Entry<Long, String> e : new ArrayList<Map.Entry<Long, String>>(iatOf.entrySet())) {
            Address x = sp.getAddress(e.getKey());
            Function fx = fm.getFunctionContaining(x);
            if (fx != null && !fx.isThunk()) { addSite(e.getValue(), e.getKey()); continue; }
            ReferenceIterator ri = rm.getReferencesTo(x);
            while (ri.hasNext()) {
                Reference r = ri.next();
                Address from = r.getFromAddress();
                Function f = fm.getFunctionContaining(from);
                if (f != null && f.isThunk()) continue;
                if (f == null) {
                    // an undefined stub: `FF 25` at from-2
                    Address stub = from;
                    try {
                        if ((mem.getByte(from) & 0xff) != 0xff) stub = from.subtract(2);
                    } catch (Exception ex) { /* keep */ }
                    ReferenceIterator si = rm.getReferencesTo(stub);
                    boolean any = false;
                    while (si.hasNext()) { any = true; addSite(e.getValue(), si.next().getFromAddress().getOffset()); }
                    if (!any) addSite(e.getValue(), from.getOffset());
                    continue;
                }
                addSite(e.getValue(), from.getOffset());
            }
        }
        for (Map.Entry<Long, String> e : thunkOf.entrySet()) {
            ReferenceIterator ri = rm.getReferencesTo(sp.getAddress(e.getKey()));
            while (ri.hasNext()) {
                Reference r = ri.next();
                addSite(e.getValue(), r.getFromAddress().getOffset());
            }
        }
    }

    void addSite(String nm, long at) {
        List<Long> l = importSites.get(nm);
        if (l == null) { l = new ArrayList<>(); importSites.put(nm, l); }
        if (!l.contains(at)) l.add(at);
    }

    boolean inSet(String imp) {
        int bang = imp.indexOf('!');
        String m = bang >= 0 ? imp.substring(bang + 1) : imp;
        if (!m.toLowerCase().startsWith("?") ) return false;
        if (!imp.toLowerCase().startsWith("runtime")) return false;
        return classKey(m) != null;
    }

    // Bytes at a slot target Ghidra has no function for: `FF 25 disp32` is an import JMP
    // stub (the IAT slot is read and named), `C2 00 00` / `C3` is an empty body.
    String decodeStub(Address a) {
        try {
            int b0 = mem.getByte(a) & 0xff, b1 = mem.getByte(a.add(1)) & 0xff;
            if (b0 == 0xff && b1 == 0x25) {
                int disp = mem.getInt(a.add(2));
                long iat = a.getOffset() + 6 + disp;
                String nm = iatOf.get(iat);
                if (nm == null) {
                    // the IAT slot's own reference to the external function
                    for (Reference r : currentProgram.getReferenceManager().getReferencesFrom(sp.getAddress(iat))) {
                        Function ext = fm.getFunctionAt(r.getToAddress());
                        if (ext != null && ext.isExternal()) nm = importName(ext);
                    }
                }
                return String.format("STUB JMP [0x%x] -> %s", iat, nm == null ? "?" : nm);
            }
            if (b0 == 0xc2 && b1 == 0x00) return "EXE RET 0 (empty body, no Ghidra function)";
            if (b0 == 0xc3) return "EXE RET (empty body, no Ghidra function)";
        } catch (Exception e) { /* fall through */ }
        return "EXE code, no Ghidra function";
    }

    String exeTarget(long t) {
        Address a = sp.getAddress(t);
        if (thunkOf.containsKey(t)) return "THUNK -> " + thunkOf.get(t);
        Function f = fm.getFunctionAt(a);
        if (f == null) return decodeStub(a);
        if (f.isThunk()) {
            Function th = f.getThunkedFunction(true);
            if (th != null && th.isExternal()) return "THUNK -> " + importName(th);
        }
        return "EXE " + f.getName() + (f.getEntryPoint().equals(a) ? "" : " (+" + (t - f.getEntryPoint().getOffset()) + ")");
    }

    boolean referencedFromCode(Address a) {
        ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(a);
        while (ri.hasNext()) {
            Reference r = ri.next();
            if (fm.getFunctionContaining(r.getFromAddress()) != null) return true;
        }
        return false;
    }

    void dumpExeVt(String label, long va) {
        Address a = sp.getAddress(va);
        List<String> rows = new ArrayList<>();
        String stop = "64-slot cap";
        int thunks = 0, bodies = 0;
        for (int i = 0; i < 64; i++) {
            Address cur = a.add(i * 8L);
            if (i > 0 && referencedFromCode(cur)) { stop = "next address is referenced from code (another table)"; break; }
            long p;
            try { p = mem.getLong(cur); } catch (Exception e) { stop = "unreadable"; break; }
            if (p == 0) { stop = "zero qword"; break; }
            Address tgt;
            try { tgt = sp.getAddress(p); } catch (Exception e) { stop = "not an address"; break; }
            if (!mem.contains(tgt)) { stop = "target outside the image"; break; }
            MemoryBlock tb = mem.getBlock(tgt);
            if (tb == null || !tb.isExecute()) { stop = "target is not in an executable block"; break; }
            String t = exeTarget(p);
            if (t.startsWith("THUNK") || t.startsWith("STUB")) thunks++; else bodies++;
            rows.add(String.format("  [%2d] +0x%02x 0x%x  %s", i, i * 8, p, t));
        }
        println("");
        println(String.format("### %s  vtable=0x%x  slots=%d  (exe bodies=%d, Runtime thunks=%d)  stop: %s",
                label, va, rows.size(), bodies, thunks, stop));
        for (String r : rows) println(r);
        // who stamps / references it
        List<String> refs = new ArrayList<>();
        ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(a);
        while (ri.hasNext()) {
            Reference r = ri.next();
            Function f = fm.getFunctionContaining(r.getFromAddress());
            String who = (f == null) ? ("<no fn> " + r.getFromAddress()) : (f.getName() + " @0x" + f.getEntryPoint() + " ref@0x" + r.getFromAddress());
            refs.add(who);
        }
        println("  referenced by (" + refs.size() + "):");
        for (String r : refs) println("    " + r);
    }

    void runExe() throws Exception {
        collectImports();
        println("imports from any DLL: iat=" + iatOf.size() + " thunks=" + thunkOf.size());

        println("");
        println("@@@@@@ SECTION E1 -- the five exe loop-subclass vtables named in the brief");
        long[] loops = {0x14015e2f0L, 0x14015ed00L, 0x140158800L, 0x140158870L, 0x1401588d0L};
        String[] names = {"GameUpdateLoop (brief: 22 slots?)", "MenuDialogLoopT (MenuDLT)", "splash DialogLoaderLoopT subclass",
                          "LoaderLoop subclass 'inner'", "LoaderLoop subclass 'outer'"};
        for (int i = 0; i < loops.length; i++) dumpExeVt(names[i], loops[i]);

        println("");
        println("@@@@@@ SECTION E2 -- exe call sites of Runtime ctor/dtor imports of step-7 classes (8 instructions before, 8 after)");
        Set<Long> stamped = new java.util.TreeSet<>();
        Map<Long, String> stampedBy = new HashMap<>();
        for (Map.Entry<String, List<Long>> e : importSites.entrySet()) {
            String imp = e.getKey();
            if (!inSet(imp)) continue;
            String m = imp.substring(imp.indexOf('!') + 1);
            if (!(m.startsWith("??0") || m.startsWith("??1"))) continue;
            for (Long at : e.getValue()) {
                Address ca = sp.getAddress(at);
                Instruction ins = lst.getInstructionAt(ca);
                if (ins == null) continue;
                Function f = fm.getFunctionContaining(ca);
                println("");
                println(String.format("### %s  from %s @0x%x", m, f == null ? "<no fn>" : f.getName() + "(0x" + f.getEntryPoint() + ")", at));
                Instruction p = ins;
                List<Instruction> before = new ArrayList<>();
                for (int k = 0; k < 8 && p != null; k++) { p = p.getPrevious(); if (p != null) before.add(0, p); }
                for (Instruction b : before) println("    " + b.getAddress() + "  " + b);
                println("  > " + ins.getAddress() + "  " + ins + "   ; " + m);
                Instruction n = ins;
                for (int k = 0; k < 8 && n != null; k++) {
                    n = n.getNext();
                    if (n == null) break;
                    StringBuilder x = new StringBuilder();
                    for (Reference r : n.getReferencesFrom()) {
                        Address t = r.getToAddress();
                        if (mem.contains(t) && fm.getFunctionAt(t) == null) {
                            MemoryBlock b = mem.getBlock(t);
                            x.append("  => data ").append(t).append(b == null ? "" : " (" + b.getName() + ")");
                            if (m.startsWith("??0") && n.getMnemonicString().equals("LEA") && b != null && b.getName().equals(".rdata")) {
                                stamped.add(t.getOffset());
                                if (!stampedBy.containsKey(t.getOffset())) stampedBy.put(t.getOffset(), m + " in " + (f == null ? "?" : f.getName()));
                            }
                        }
                    }
                    println("    " + n.getAddress() + "  " + n + x);
                }
            }
        }

        println("");
        println("@@@@@@ SECTION E3 -- .rdata tables LEA'd within 8 instructions after a step-7 base ctor call (candidate subclass vtables)");
        for (Long v : stamped) dumpExeVt("after " + stampedBy.get(v), v);

        println("");
        println("@@@@@@ SECTION E4 -- import census: Runtime imports in the eight namespaces and the exe code reaching each");
        Map<String, Integer> perNs = new TreeMap<>();
        Map<String, Integer> perNsReached = new TreeMap<>();
        Set<String> uniq = new java.util.TreeSet<>(iatOf.values());
        for (String imp : uniq) {
            if (!inSet(imp)) continue;
            String m = imp.substring(imp.indexOf('!') + 1);
            String key = classKey(m);
            String ns = key.substring(0, key.indexOf("::"));
            perNs.put(ns, (perNs.containsKey(ns) ? perNs.get(ns) : 0) + 1);
            List<Long> sites = importSites.containsKey(imp) ? importSites.get(imp) : new ArrayList<Long>();
            if (!sites.isEmpty()) perNsReached.put(ns, (perNsReached.containsKey(ns) ? perNsReached.get(ns) : 0) + 1);
            Set<String> fns = new java.util.TreeSet<>();
            int dataRefs = 0;
            for (Long s : sites) {
                Function f = fm.getFunctionContaining(sp.getAddress(s));
                if (f == null) { dataRefs++; fns.add("<data 0x" + Long.toHexString(s) + ">"); }
                else fns.add(f.getName());
            }
            println(String.format("%-100s sites=%d fns=%d  %s", m, sites.size(), fns.size(),
                    fns.size() > 10 ? new ArrayList<String>(fns).subList(0, 10) + " ..." : fns.toString()));
        }
        println("per-namespace distinct imports: " + perNs);
        println("per-namespace imports with >=1 site: " + perNsReached);
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
                if (i < pslots.size() && pslots.get(i).longValue() == t) inh = "   inherited-from " + par;
                // also mark a slot shared with ANY of the five Object parents at the same index
                if (inh.isEmpty()) {
                    for (String op : new String[]{"Object::Base", "Object::External", "Object::CompiledExternal", "Object::Internal", "Object::ExternalList", "System::Thread"}) {
                        Long ov = vtOf.get(op);
                        if (ov == null || op.equals(cls)) continue;
                        List<Long> os = readVt(ov, vtAddrs);
                        if (i < os.size() && os.get(i).longValue() == t) { inh = "   same-as " + op; break; }
                    }
                }
                println(String.format("  [%2d] +0x%02x 0x%x  %s%s", i, i * 8, t, mangledAt(sp.getAddress(t)), inh));
            }
        }
    }
}
