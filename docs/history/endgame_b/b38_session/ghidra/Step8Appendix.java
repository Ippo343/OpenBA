// B38_SLOTS.md source data for ENDGAME B.3 step 8: every exe-phase class of the RETAIL
// Application-x64.exe (11473 functions). EXE mode only; read-only (no rename, no annotation).
// Extends b37_session/ghidra/Step7Appendix.java (its import/IAT/stub decoding is kept verbatim).
//
// The exe has NO MSVC RTTI for game classes (the only `.?AV` type descriptors are std::exception,
// std::bad_exception and type_info), so tables are found structurally:
//   - a TABLE CANDIDATE is every .rdata address that a code LEA materialises and whose
//     first qword points into .text (an exe body or an `FF 25` import stub);
//   - a table's slots run until the next candidate start, a zero qword or a qword that is not a
//     pointer into an executable block (the class-name string literals sit right after tables);
//   - a STAMP is `LEA R,[table]` followed (before R is redefined) by `MOV qword ptr [B+d],R`;
//     B is classified as `this` (RCX at entry and its copies) or other; d is the vptr offset;
//   - a function's STAMP CHAIN is every stamp / call in address order, with calls annotated by
//     what RCX holds (`this`, `this+0xNN`);
//   - reflected classes are the callers of the Runtime import
//     `??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z`
//     (RCX = the type-info global, RDX = name, R8 = parent name, R9 = third literal,
//     [RSP+0x20] = create, [RSP+0x28] = create-in-place). The create function's MemAlloc
//     size is sizeof and its first direct call after the allocation is the ctor.
//   - sizeof for every `new`: each MemAlloc call site with a constant size, and the first
//     direct call after it (the ctor) with the table that ctor stamps last at this+0.
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
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

public class Step8Appendix extends GhidraScript {

    SymbolTable st;
    Memory mem;
    FunctionManager fm;
    AddressSpace sp;
    Listing lst;

    static final String BTI = "??0BaseTypeInfo@Object@@QEAA@PEBD00P6APEAVBase@1@XZP6APEAV21@PEAX@Z@Z";

    Map<Long, String> thunkOf = new HashMap<>();   // thunk entry -> mangled import
    Map<Long, String> iatOf = new HashMap<>();     // IAT slot -> mangled import

    // ---- import naming (from Step7Appendix) -------------------------------------------------
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
            while (ri.hasNext()) iatOf.put(ri.next().getFromAddress().getOffset(), nm);
        }
        ghidra.program.model.listing.FunctionIterator fi = fm.getFunctions(true);
        while (fi.hasNext()) {
            Function f = fi.next();
            if (!f.isThunk()) continue;
            Function th = f.getThunkedFunction(true);
            if (th != null && th.isExternal()) thunkOf.put(f.getEntryPoint().getOffset(), importName(th));
        }
    }

    String shortImp(String n) {
        if (n == null) return "?";
        int b = n.indexOf('!');
        return b >= 0 ? n.substring(b + 1) : n;
    }

    // ---- memory helpers ----------------------------------------------------------------------
    int u8(long va) { try { return mem.getByte(sp.getAddress(va)) & 0xff; } catch (Exception e) { return -1; } }
    int s32(long va) { try { return mem.getInt(sp.getAddress(va)); } catch (Exception e) { return 0; } }
    long q(long va) { try { return mem.getLong(sp.getAddress(va)); } catch (Exception e) { return 0; } }

    boolean isExec(long va) {
        try {
            Address a = sp.getAddress(va);
            if (!mem.contains(a)) return false;
            MemoryBlock b = mem.getBlock(a);
            return b != null && b.isExecute() && b.getName().equals(".text");
        } catch (Exception e) { return false; }
    }

    String blockOf(long va) {
        try {
            MemoryBlock b = mem.getBlock(sp.getAddress(va));
            return b == null ? "" : b.getName();
        } catch (Exception e) { return ""; }
    }

    String cstr(long va) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 200; i++) {
            int c = u8(va + i);
            if (c <= 0) break;
            if (c < 0x20 || c > 0x7e) return null;
            sb.append((char) c);
        }
        return sb.length() == 0 ? null : sb.toString();
    }

    String lit(long va) {
        if (va == 0) return "null";
        String s = cstr(va);
        return s == null ? String.format("0x%x(not a string)", va) : "\"" + s + "\"";
    }

    String iatAt(long slot) {
        String nm = iatOf.get(slot);
        if (nm == null) {
            for (Reference r : currentProgram.getReferenceManager().getReferencesFrom(sp.getAddress(slot))) {
                Function ext = fm.getFunctionAt(r.getToAddress());
                if (ext != null && ext.isExternal()) nm = importName(ext);
            }
        }
        return nm;
    }

    // Import that a `FF 25` stub at `va` jumps through, else null.
    String stubImport(long va) {
        if (u8(va) == 0xff && u8(va + 1) == 0x25) return iatAt(va + 6 + s32(va + 2));
        if (thunkOf.containsKey(va)) return thunkOf.get(va);
        Function f = fm.getFunctionAt(sp.getAddress(va));
        if (f != null && f.isThunk()) {
            Function th = f.getThunkedFunction(true);
            if (th != null && th.isExternal()) return importName(th);
        }
        return null;
    }

    Set<Long> purecalls = new HashSet<>();

    // A slot target, classified. Raw-byte decode first (Ghidra often has no function at tiny bodies).
    String classify(long t) {
        String imp = stubImport(t);
        if (imp != null) return "IMPORT " + shortImp(imp);
        if (purecalls.contains(t)) return "_purecall";
        int b0 = u8(t), b1 = u8(t + 1), b2 = u8(t + 2), b3 = u8(t + 3);
        if (b0 == 0xc3) return "TINY RET";
        if (b0 == 0xc2 && b1 == 0 && b2 == 0) return "TINY RET 0 (c2 00 00)";
        if (b0 == 0x33 && b1 == 0xc0 && b2 == 0xc3) return "TINY return 0 (xor eax,eax)";
        if (b0 == 0x32 && b1 == 0xc0 && b2 == 0xc3) return "TINY return false (xor al,al)";
        if (b0 == 0xb0 && b2 == 0xc3) return "TINY return " + b1 + " (mov al)";
        if (b0 == 0xb8 && u8(t + 5) == 0xc3) return "TINY return " + s32(t + 1) + " (mov eax)";
        if (b0 == 0x0f && b1 == 0x57 && b2 == 0xc0 && b3 == 0xc3) return "TINY return 0.0f (xorps)";
        // lea rax,[rip+d]; ret
        if (b0 == 0x48 && b1 == 0x8d && b2 == 0x05 && u8(t + 7) == 0xc3) {
            long x = t + 7 + s32(t + 3);
            String s = cstr(x);
            return s != null ? "TINY returns literal \"" + s + "\" @0x" + Long.toHexString(x)
                             : String.format("TINY returns &0x%x (%s)", x, blockOf(x));
        }
        // lea rax,[rcx+imm]; ret  -> returns the address of a member (ControllerComponent slot 17)
        if (b0 == 0x48 && b1 == 0x8d && b2 == 0x41 && u8(t + 4) == 0xc3) return String.format("TINY returns this+0x%x", u8(t + 3));
        if (b0 == 0x48 && b1 == 0x8d && b2 == 0x81 && u8(t + 7) == 0xc3) return String.format("TINY returns this+0x%x", s32(t + 3));
        // mov eax,[rip+d]; ret  -> the type id of a BaseTypeInfo global
        if (b0 == 0x8b && b1 == 0x05 && u8(t + 6) == 0xc3) {
            long x = t + 6 + s32(t + 2);
            String n = tiName.get(x);
            return String.format("TINY returns dword [0x%x]%s", x, n == null ? "" : " = TypeId(" + n + ")");
        }
        // mov rax,[rip+d]; ret
        if (b0 == 0x48 && b1 == 0x8b && b2 == 0x05 && u8(t + 7) == 0xc3) {
            long x = t + 7 + s32(t + 3);
            return String.format("TINY returns qword [0x%x]", x);
        }
        // this-adjust thunks: sub/add rcx,imm ; jmp
        if (b0 == 0x48 && b1 == 0x83 && (b2 == 0xe9 || b2 == 0xc1)) {
            int imm = (byte) b3;
            if (b2 == 0xe9) imm = -imm;
            long j = t + 4;
            if (u8(j) == 0xe9) {
                long tgt = j + 5 + s32(j + 1);
                return String.format("THUNK this%+d -> 0x%x %s", imm, tgt, fnName(tgt));
            }
            if (u8(j) == 0xeb) {
                long tgt = j + 2 + (byte) u8(j + 1);
                return String.format("THUNK this%+d -> 0x%x %s", imm, tgt, fnName(tgt));
            }
        }
        if (b0 == 0x48 && b1 == 0x81 && (b2 == 0xe9 || b2 == 0xc1)) {
            int imm = s32(t + 3);
            if (b2 == 0xe9) imm = -imm;
            long j = t + 7;
            if (u8(j) == 0xe9) {
                long tgt = j + 5 + s32(j + 1);
                return String.format("THUNK this%+d -> 0x%x %s", imm, tgt, fnName(tgt));
            }
        }
        if (b0 == 0xe9) {
            long tgt = t + 5 + s32(t + 1);
            String ti = stubImport(tgt);
            return String.format("JMP 0x%x %s", tgt, ti != null ? "IMPORT " + shortImp(ti) : fnName(tgt));
        }
        Function f = fm.getFunctionAt(sp.getAddress(t));
        if (f == null) {
            Function c = fm.getFunctionContaining(sp.getAddress(t));
            if (c != null) return "OWN (inside " + c.getName() + ")";
            return "OWN (no Ghidra function)";
        }
        long sz = f.getBody().getNumAddresses();
        return "OWN " + f.getName() + " (" + sz + " B)";
    }

    String fnName(long va) {
        Function f = fm.getFunctionAt(sp.getAddress(va));
        if (f != null) return f.getName();
        Function c = fm.getFunctionContaining(sp.getAddress(va));
        return c == null ? "<no fn>" : c.getName() + "+" + (va - c.getEntryPoint().getOffset());
    }

    // ---- register helpers --------------------------------------------------------------------
    String base(Register r) {
        if (r == null) return null;
        Register b = r.getBaseRegister();
        return b == null ? r.getName() : b.getName();
    }

    static final Set<String> VOLATILE = new HashSet<>(java.util.Arrays.asList(
            "RAX", "RCX", "RDX", "R8", "R9", "R10", "R11"));

    Set<String> writtenRegs(Instruction ins) {
        Set<String> w = new HashSet<>();
        for (Object o : ins.getResultObjects()) if (o instanceof Register) w.add(base((Register) o));
        return w;
    }

    // [B + d] memory operand of operand `op`: returns {baseReg, disp} or null.
    Object[] memOp(Instruction ins, int op) {
        Object[] objs = ins.getOpObjects(op);
        String b = null; long d = 0; int regs = 0;
        for (Object o : objs) {
            if (o instanceof Register) { b = base((Register) o); regs++; }
            else if (o instanceof Scalar) d = ((Scalar) o).getSignedValue();
        }
        if (regs != 1) return null;
        return new Object[]{b, d};
    }

    // ---- table discovery ---------------------------------------------------------------------
    TreeSet<Long> cand = new TreeSet<>();
    Map<Long, List<Long>> tableRefs = new TreeMap<>();    // table -> code sites referencing it
    Map<Long, List<Long>> slotsOf = new TreeMap<>();
    Map<Long, String> stopOf = new HashMap<>();

    boolean isTableHead(long va) {
        long p = q(va);
        return isExec(p);
    }

    void findCandidates() {
        InstructionIterator it = lst.getInstructions(true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            if (!blockOf(ins.getAddress().getOffset()).equals(".text")) continue;
            // only an address MATERIALISED by LEA starts a table: Ghidra also attaches constant-
            // propagated references to `CALL qword ptr [RAX+0x48]` (a slot inside a table),
            // which would split the table there (first run: 0x14015c908, 0x14015e248, ...)
            if (!ins.getMnemonicString().equals("LEA")) continue;
            for (Reference r : ins.getReferencesFrom()) {
                long t = r.getToAddress().getOffset();
                if (!blockOf(t).equals(".rdata")) continue;
                if (iatOf.containsKey(t)) continue;
                if (!isTableHead(t)) continue;
                cand.add(t);
                List<Long> l = tableRefs.get(t);
                if (l == null) { l = new ArrayList<>(); tableRefs.put(t, l); }
                l.add(ins.getAddress().getOffset());
            }
        }
        for (Long t : cand) {
            List<Long> s = new ArrayList<>();
            String stop = "128-slot cap";
            for (int i = 0; i < 128; i++) {
                long a = t + 8L * i;
                if (i > 0 && cand.contains(a)) { stop = "next table"; break; }
                if (iatOf.containsKey(a)) { stop = "IAT"; break; }
                long p = q(a);
                if (p == 0) { stop = "zero qword"; break; }
                if (!isExec(p)) { stop = String.format("non-code qword 0x%x%s", p, cstr(a) != null ? " (string \"" + cstr(a) + "\")" : ""); break; }
                // a data label with references from data (another table's pointer) also ends it
                s.add(p);
            }
            slotsOf.put(t, s);
            stopOf.put(t, stop);
        }
    }

    // ---- per-function stamp chains -----------------------------------------------------------
    static class Ev {
        long at; String kind; long table; String baseReg; long disp; boolean isThis; String text;
    }
    Map<Long, List<Ev>> chainOf = new TreeMap<>();                // fn entry -> events
    Map<Long, List<long[]>> stampsOfTable = new TreeMap<>();       // table -> {fn, at, disp, isThis}
    Map<Long, List<Long>> otherRefsOfTable = new TreeMap<>();

    void scanFunctions() {
        Set<Long> stampSites = new HashSet<>();
        ghidra.program.model.listing.FunctionIterator fi = fm.getFunctions(true);
        while (fi.hasNext()) {
            Function f = fi.next();
            if (f.isThunk() || f.isExternal()) continue;
            List<Ev> evs = new ArrayList<>();
            Map<String, Long> pending = new HashMap<>();        // reg -> table
            Map<String, Long> thisOff = new HashMap<>();        // reg -> this offset
            thisOff.put("RCX", 0L);
            InstructionIterator ii = lst.getInstructions(f.getBody(), true);
            boolean hasStamp = false;
            while (ii.hasNext()) {
                Instruction ins = ii.next();
                String mn = ins.getMnemonicString();
                long at = ins.getAddress().getOffset();
                if (mn.equals("LEA")) {
                    String dst = base(ins.getRegister(0));
                    Long tbl = null;
                    for (Reference r : ins.getReferencesFrom()) {
                        long t = r.getToAddress().getOffset();
                        if (cand.contains(t)) tbl = t;
                    }
                    // LEA dst,[this_alias + imm]
                    Object[] mo = memOp(ins, 1);
                    pending.remove(dst);
                    Long newOff = null;
                    if (mo != null && thisOff.containsKey((String) mo[0])) newOff = thisOff.get((String) mo[0]) + (Long) mo[1];
                    thisOff.remove(dst);
                    if (newOff != null) thisOff.put(dst, newOff);
                    if (tbl != null) pending.put(dst, tbl);
                    continue;
                }
                if (mn.equals("MOV")) {
                    // store of a pending table
                    Register src = ins.getRegister(1);
                    if (src != null && ins.getRegister(0) == null) {
                        String s = base(src);
                        if (pending.containsKey(s)) {
                            Object[] mo = memOp(ins, 0);
                            Ev e = new Ev();
                            e.at = at; e.kind = "STAMP"; e.table = pending.get(s);
                            if (mo == null) {
                                // RIP-relative global: a static object
                                long g = 0;
                                for (Reference r : ins.getReferencesFrom()) g = r.getToAddress().getOffset();
                                e.baseReg = String.format("global 0x%x", g); e.disp = 0; e.isThis = false;
                            } else {
                                String b = (String) mo[0];
                                long d = (Long) mo[1];
                                if (thisOff.containsKey(b)) { e.isThis = true; e.disp = thisOff.get(b) + d; e.baseReg = b; }
                                else { e.isThis = false; e.disp = d; e.baseReg = b; }
                            }
                            evs.add(e); hasStamp = true;
                            stampSites.add(at);
                            List<long[]> l = stampsOfTable.get(e.table);
                            if (l == null) { l = new ArrayList<>(); stampsOfTable.put(e.table, l); }
                            l.add(new long[]{f.getEntryPoint().getOffset(), at, e.disp, e.isThis ? 1 : 0});
                            continue;
                        }
                    }
                    // register copy of a this alias
                    Register d0 = ins.getRegister(0);
                    if (d0 != null && src != null) {
                        String dn = base(d0), sn = base(src);
                        Long off = thisOff.get(sn);
                        Long pt = pending.get(sn);
                        thisOff.remove(dn); pending.remove(dn);
                        if (off != null) thisOff.put(dn, off);
                        if (pt != null) pending.put(dn, pt);
                        continue;
                    }
                }
                if (mn.equals("CALL")) {
                    Ev e = new Ev();
                    e.at = at; e.kind = "CALL";
                    Long rcx = thisOff.get("RCX");
                    String tgt = "?";
                    for (Reference r : ins.getReferencesFrom()) {
                        long t = r.getToAddress().getOffset();
                        if (iatOf.containsKey(t)) tgt = "IMPORT " + shortImp(iatOf.get(t));
                        else if (r.getToAddress().isExternalAddress()) {
                            Function ext = fm.getFunctionAt(r.getToAddress());
                            tgt = ext == null ? "EXTERNAL" : "IMPORT " + shortImp(importName(ext));
                        } else {
                            String si = stubImport(t);
                            tgt = si != null ? "IMPORT " + shortImp(si) : String.format("0x%x %s", t, fnName(t));
                        }
                    }
                    if (tgt.equals("?")) tgt = "indirect " + ins.toString();
                    e.text = tgt;
                    e.table = 0;
                    e.disp = rcx == null ? Long.MIN_VALUE : rcx;
                    evs.add(e);
                    // clobber volatile registers
                    for (String v : VOLATILE) { pending.remove(v); thisOff.remove(v); }
                    continue;
                }
                for (String w : writtenRegs(ins)) { pending.remove(w); thisOff.remove(w); }
            }
            if (hasStamp) chainOf.put(f.getEntryPoint().getOffset(), evs);
        }
        // every reference to a table that is not a stamp
        for (Map.Entry<Long, List<Long>> e : tableRefs.entrySet()) {
            for (Long site : e.getValue()) {
                Instruction ins = lst.getInstructionAt(sp.getAddress(site));
                if (ins == null) continue;
                // the LEA feeding a stamp is the stamp; others are "other refs"
                boolean fed = false;
                Instruction n = ins;
                for (int k = 0; k < 16 && n != null; k++) { n = n.getNext(); if (n != null && stampSites.contains(n.getAddress().getOffset())) { fed = true; break; } }
                if (!fed) {
                    List<Long> l = otherRefsOfTable.get(e.getKey());
                    if (l == null) { l = new ArrayList<>(); otherRefsOfTable.put(e.getKey(), l); }
                    l.add(site);
                }
            }
        }
    }

    // the table a function stamps LAST at this+0 (its "own" table if it is a ctor)
    Long lastThis0(long fn) {
        List<Ev> evs = chainOf.get(fn);
        if (evs == null) return null;
        Long r = null;
        for (Ev e : evs) if (e.kind.equals("STAMP") && e.isThis && e.disp == 0) r = e.table;
        return r;
    }
    Long firstThis0(long fn) {
        List<Ev> evs = chainOf.get(fn);
        if (evs == null) return null;
        for (Ev e : evs) if (e.kind.equals("STAMP") && e.isThis && e.disp == 0) return e.table;
        return null;
    }

    long resolveJmp(long t) {
        // follow a single `E9` tail jump (incremental-link style thunk)
        if (u8(t) == 0xe9) return t + 5 + s32(t + 1);
        return t;
    }

    // ---- registrations -----------------------------------------------------------------------
    static class Reg {
        long site, fn, ti, name, parent, third, create; String createInPlace; long cip;
        long size = -1; long ctor = 0; Long table = null;
    }
    List<Reg> regs = new ArrayList<>();
    Map<Long, String> tiName = new HashMap<>();
    Map<Long, String> nameOfTable = new HashMap<>();   // from reflection (create -> ctor)

    void scanRegistrations() {
        long btiSlot = 0;
        for (Map.Entry<Long, String> e : iatOf.entrySet()) if (shortImp(e.getValue()).equals(BTI)) {
            Function fx = fm.getFunctionContaining(sp.getAddress(e.getKey()));
            if (fx == null && blockOf(e.getKey()).equals(".rdata")) btiSlot = e.getKey();
        }
        println(String.format("BaseTypeInfo IAT slot 0x%x", btiSlot));
        ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(sp.getAddress(btiSlot));
        while (ri.hasNext()) {
            Reference r = ri.next();
            Instruction call = lst.getInstructionAt(r.getFromAddress());
            if (call == null || !call.getMnemonicString().equals("CALL")) continue;
            Function f = fm.getFunctionContaining(call.getAddress());
            // forward-simulate from up to 40 instructions back
            Instruction s = call;
            for (int k = 0; k < 40; k++) {
                Instruction p = s.getPrevious();
                if (p == null) break;
                if (f != null && !f.getBody().contains(p.getAddress())) break;
                if (p.getMnemonicString().equals("CALL") || p.getMnemonicString().equals("RET")) break;
                s = p;
            }
            Map<String, String> val = new HashMap<>();
            Map<Long, String> stk = new HashMap<>();
            for (Instruction i = s; i != null && !i.getAddress().equals(call.getAddress()); i = i.getNext()) {
                String mn = i.getMnemonicString();
                Register d0 = i.getRegister(0);
                if (mn.equals("LEA") && d0 != null) {
                    String v = null;
                    for (Reference rr : i.getReferencesFrom()) v = "A" + rr.getToAddress().getOffset();
                    val.put(base(d0), v);
                } else if (mn.equals("XOR") && d0 != null && i.getRegister(1) != null && base(d0).equals(base(i.getRegister(1)))) {
                    val.put(base(d0), "A0");
                } else if (mn.equals("MOV") && d0 != null) {
                    String v = null;
                    Register s1 = i.getRegister(1);
                    if (s1 != null) v = val.get(base(s1));
                    else for (Reference rr : i.getReferencesFrom()) {
                        long t = rr.getToAddress().getOffset();
                        v = iatOf.containsKey(t) ? "I" + shortImp(iatOf.get(t)) : "M" + t;
                    }
                    val.put(base(d0), v);
                } else if (mn.equals("MOV") && d0 == null) {
                    Object[] mo = memOp(i, 0);
                    Register s1 = i.getRegister(1);
                    if (mo != null && "RSP".equals(mo[0])) {
                        stk.put((Long) mo[1], s1 != null ? val.get(base(s1)) : "imm");
                    }
                } else {
                    for (String w : writtenRegs(i)) val.remove(w);
                }
            }
            Reg g = new Reg();
            g.site = call.getAddress().getOffset();
            g.fn = f == null ? 0 : f.getEntryPoint().getOffset();
            g.ti = av(val.get("RCX"));
            g.name = av(val.get("RDX"));
            g.parent = av(val.get("R8"));
            g.third = av(val.get("R9"));
            g.create = av(stk.get(0x20L));
            String c = stk.get(0x28L);
            g.cip = av(c);
            g.createInPlace = c == null ? "?" : c.startsWith("I") ? "IMPORT " + c.substring(1) : c.startsWith("A") ? String.format("0x%x %s", g.cip, fnName(g.cip)) : c;
            regs.add(g);
            String nm = cstr(g.name);
            if (nm != null) tiName.put(g.ti, nm);
        }
    }

    long av(String v) {
        if (v == null || !v.startsWith("A")) return -1;
        return Long.parseLong(v.substring(1));
    }

    // MemAlloc-shaped call: size in RCX (constant tracked), then first direct call = ctor.
    long memAllocSlot = 0;
    List<long[]> allocs = new ArrayList<>();   // {site, size, ctor, fn}

    void scanAllocs() {
        for (Map.Entry<Long, String> e : iatOf.entrySet()) {
            String n = shortImp(e.getValue());
            if (n.startsWith("?MemAlloc@") && blockOf(e.getKey()).equals(".rdata")) { memAllocSlot = e.getKey(); println("MemAlloc import: " + n + String.format(" @0x%x", e.getKey())); }
        }
        ReferenceIterator ri = currentProgram.getReferenceManager().getReferencesTo(sp.getAddress(memAllocSlot));
        while (ri.hasNext()) {
            Reference r = ri.next();
            Instruction call = lst.getInstructionAt(r.getFromAddress());
            if (call == null || !call.getMnemonicString().equals("CALL")) continue;
            Function f = fm.getFunctionContaining(call.getAddress());
            Instruction s = call;
            for (int k = 0; k < 12; k++) {
                Instruction p = s.getPrevious();
                if (p == null) break;
                if (f != null && !f.getBody().contains(p.getAddress())) break;
                if (p.getMnemonicString().equals("CALL") || p.getMnemonicString().equals("RET")) break;
                s = p;
            }
            Map<String, Long> k = new HashMap<>();
            for (Instruction i = s; i != null && !i.getAddress().equals(call.getAddress()); i = i.getNext()) {
                String mn = i.getMnemonicString();
                Register d0 = i.getRegister(0);
                if (d0 == null) continue;
                String dn = base(d0);
                Object[] o1 = i.getOpObjects(1);
                if (mn.equals("MOV") && o1.length == 1 && o1[0] instanceof Scalar) k.put(dn, ((Scalar) o1[0]).getUnsignedValue());
                else if (mn.equals("LEA")) {
                    Object[] mo = memOp(i, 1);
                    if (mo != null && k.containsKey((String) mo[0])) k.put(dn, k.get((String) mo[0]) + (Long) mo[1]);
                    else k.remove(dn);
                } else if (mn.equals("XOR") && i.getRegister(1) != null && dn.equals(base(i.getRegister(1)))) k.put(dn, 0L);
                else for (String w : writtenRegs(i)) k.remove(w);
            }
            long size = k.containsKey("RCX") ? k.get("RCX") : -1;
            long ctor = 0;
            Instruction n = call;
            for (int j = 0; j < 14 && n != null; j++) {
                n = n.getNext();
                if (n == null || (f != null && !f.getBody().contains(n.getAddress()))) break;
                if (n.getMnemonicString().equals("CALL")) {
                    for (Reference rr : n.getReferencesFrom()) {
                        long t = rr.getToAddress().getOffset();
                        if (blockOf(t).equals(".text")) ctor = t;
                    }
                    break;
                }
            }
            allocs.add(new long[]{call.getAddress().getOffset(), size, ctor, f == null ? 0 : f.getEntryPoint().getOffset()});
        }
    }

    // own table of a ctor (following one E9 tail jump), or of the allocating function if the
    // ctor is inlined
    Long ctorTable(long ctor) {
        long c = resolveJmp(ctor);
        return lastThis0(c);
    }

    // ---- main --------------------------------------------------------------------------------
    @Override
    public void run() throws Exception {
        st = currentProgram.getSymbolTable();
        mem = currentProgram.getMemory();
        fm = currentProgram.getFunctionManager();
        sp = currentProgram.getAddressFactory().getDefaultAddressSpace();
        lst = currentProgram.getListing();

        println("PROGRAM " + currentProgram.getName() + " functions=" + fm.getFunctionCount()
                + " imagebase=" + currentProgram.getImageBase());
        if (!currentProgram.getName().equals("Application-x64.exe") || fm.getFunctionCount() != 11473) {
            println("ABORT: not the retail Application-x64.exe");
            return;
        }
        collectImports();
        println("imports: iat=" + iatOf.size() + " thunks=" + thunkOf.size());
        // _purecall: any function / import whose name is _purecall
        ghidra.program.model.listing.FunctionIterator fi = fm.getFunctions(true);
        while (fi.hasNext()) { Function f = fi.next(); if (f.getName().contains("purecall")) purecalls.add(f.getEntryPoint().getOffset()); }
        for (Map.Entry<Long, String> e : thunkOf.entrySet()) if (e.getValue().contains("purecall")) purecalls.add(e.getKey());
        for (Long p : purecalls) println(String.format("_purecall candidate 0x%x %s", p, fnName(p)));

        scanRegistrations();
        findCandidates();
        scanFunctions();
        scanAllocs();

        // reflected name per table: create -> MemAlloc size + ctor -> table
        for (Reg g : regs) {
            if (g.create > 0) {
                for (long[] a : allocs) if (a[3] == g.create) { g.size = a[1]; g.ctor = a[2]; break; }
                Long t = null;
                if (g.ctor != 0) t = ctorTable(g.ctor);
                if (t == null) t = lastThis0(g.create);
                if (t == null) {
                    // inlined ctor on the allocation result (not `this`): any disp-0 stamp in create
                    List<Ev> evs = chainOf.get(g.create);
                    if (evs != null) for (Ev e : evs) if (e.kind.equals("STAMP") && e.disp == 0) t = e.table;
                }
                g.table = t;
                String nm = cstr(g.name);
                if (t != null && nm != null) {
                    String prev = nameOfTable.get(t);
                    nameOfTable.put(t, prev == null ? nm : prev + "|" + nm);
                }
            }
        }

        // ================= SECTION R: registrations =================
        println("");
        println("@@@@@@ SECTION R -- reflected exe classes: every call of " + BTI + " (" + regs.size() + ")");
        println("# fn | typeinfo | name | parent | third | create | createInPlace | sizeof(MemAlloc in create) | ctor | table");
        regs.sort((a, b) -> Long.compare(a.site, b.site));
        for (Reg g : regs) {
            println(String.format("REG fn=0x%x %s ti=0x%x name=%s parent=%s third=%s create=0x%x cip=%s size=%s ctor=%s table=%s",
                    g.fn, fnName(g.fn), g.ti, lit(g.name), lit(g.parent), lit(g.third), g.create, g.createInPlace,
                    g.size < 0 ? "?" : "0x" + Long.toHexString(g.size),
                    g.ctor == 0 ? "?" : String.format("0x%x %s", g.ctor, fnName(g.ctor)),
                    g.table == null ? "?" : "0x" + Long.toHexString(g.table)));
        }

        // ================= SECTION T: tables =================
        int stamped = 0;
        for (Long t : cand) if (stampsOfTable.containsKey(t)) stamped++;
        println("");
        println("@@@@@@ SECTION T -- every .rdata table candidate (" + cand.size() + "; stamped by a ctor/dtor: " + stamped + ")");
        Map<Long, Integer> shareCount = new HashMap<>();
        for (List<Long> s : slotsOf.values()) for (Long p : new HashSet<>(s)) shareCount.put(p, (shareCount.containsKey(p) ? shareCount.get(p) : 0) + 1);
        for (Long t : cand) {
            List<Long> s = slotsOf.get(t);
            String label = "";
            Symbol ps = st.getPrimarySymbol(sp.getAddress(t));
            if (ps != null && !ps.getName().startsWith("PTR_") && !ps.getName().startsWith("DAT_")) label = ps.getName();
            // name by Type()/TypeName slots
            String tyName = null, tnLit = null;
            for (int i = 0; i < Math.min(4, s.size()); i++) {
                String c = classify(s.get(i));
                int k = c.indexOf("= TypeId(");
                if (k >= 0 && tyName == null) tyName = c.substring(k + 9, c.length() - 1) + " (slot " + i + ")";
                k = c.indexOf("returns literal ");
                if (k >= 0 && tnLit == null) tnLit = c.substring(k + 16, c.indexOf(" @0x", k)) + " (slot " + i + ")";
            }
            Set<String> disps = new TreeSet<>();
            List<long[]> sl = stampsOfTable.get(t);
            if (sl != null) for (long[] x : sl) disps.add((x[3] == 1 ? "this+" : "other+") + String.format("0x%x", x[2]));
            println("");
            println(String.format("### TABLE 0x%x slots=%d stop=[%s] label=%s reflected=%s TypeId=%s literal=%s stampedAt=%s",
                    t, s.size(), stopOf.get(t), label.isEmpty() ? "-" : label,
                    nameOfTable.containsKey(t) ? nameOfTable.get(t) : "-", tyName == null ? "-" : tyName,
                    tnLit == null ? "-" : tnLit, disps.isEmpty() ? "NEVER" : disps.toString()));
            for (int i = 0; i < s.size(); i++) {
                long p = s.get(i);
                int sh = shareCount.containsKey(p) ? shareCount.get(p) : 1;
                println(String.format("  [%2d] +0x%03x 0x%x  %s%s", i, i * 8, p, classify(p), sh > 1 ? "  [in " + sh + " tables]" : ""));
            }
            if (sl != null) for (long[] x : sl)
                println(String.format("  stamp %s+0x%x at 0x%x in 0x%x %s", x[3] == 1 ? "this" : "other", x[2], x[1], x[0], fnName(x[0])));
            List<Long> o = otherRefsOfTable.get(t);
            if (o != null) for (Long x : o) {
                Instruction ins = lst.getInstructionAt(sp.getAddress(x));
                println(String.format("  ref   0x%x %s : %s", x, fnName(x), ins == null ? "?" : ins.toString()));
            }
        }

        // ================= SECTION C: stamp chains =================
        println("");
        println("@@@@@@ SECTION C -- stamp chains: every function with a stamp, stamps and calls in address order (" + chainOf.size() + ")");
        for (Map.Entry<Long, List<Ev>> e : chainOf.entrySet()) {
            println("");
            println(String.format("### FN 0x%x %s", e.getKey(), fnName(e.getKey())));
            for (Ev v : e.getValue()) {
                if (v.kind.equals("STAMP"))
                    println(String.format("  0x%x STAMP 0x%x at %s+0x%x (%s)", v.at, v.table, v.isThis ? "this" : v.baseReg, v.disp, v.baseReg));
                else {
                    // only calls with this in RCX, or calls to stamping functions / imports of ctors/dtors
                    long tgt = 0;
                    String t = v.text;
                    boolean keep = v.disp != Long.MIN_VALUE;
                    if (keep) println(String.format("  0x%x CALL(rcx=this%s) %s", v.at, v.disp == 0 ? "" : String.format("+0x%x", v.disp), t));
                }
            }
        }

        // ================= SECTION S: secondary vptrs =================
        println("");
        println("@@@@@@ SECTION S -- stores of a table at a NON-ZERO this offset (secondary vptrs / embedded objects)");
        Map<String, Set<String>> sec = new TreeMap<>();
        for (Map.Entry<Long, List<Ev>> e : chainOf.entrySet()) {
            Long own = lastThis0(e.getKey());
            for (Ev v : e.getValue()) {
                if (!v.kind.equals("STAMP") || !v.isThis || v.disp == 0) continue;
                String key = String.format("fn 0x%x %s own(this+0)=%s", e.getKey(), fnName(e.getKey()), own == null ? "-" : "0x" + Long.toHexString(own));
                Set<String> x = sec.get(key);
                if (x == null) { x = new TreeSet<>(); sec.put(key, x); }
                x.add(String.format("+0x%x=0x%x", v.disp, v.table));
            }
        }
        for (Map.Entry<String, Set<String>> e : sec.entrySet()) println("SEC " + e.getKey() + "  " + e.getValue());
        // calls with this+N in RCX to a function that stamps at this+0 (non-inlined base at offset)
        println("");
        println("@@@@@@ SECTION S2 -- calls with RCX=this+N (N!=0) into a function that stamps this+0 (a base or member constructed at N)");
        for (Map.Entry<Long, List<Ev>> e : chainOf.entrySet()) {
            for (Ev v : e.getValue()) {
                if (!v.kind.equals("CALL") || v.disp == Long.MIN_VALUE || v.disp == 0) continue;
                long t = 0;
                String tx = v.text;
                if (tx.startsWith("0x")) t = Long.parseLong(tx.substring(2, tx.indexOf(' ')), 16);
                if (t == 0) continue;
                Long tt = lastThis0(resolveJmp(t));
                if (tt == null) continue;
                println(String.format("S2 fn 0x%x %s own=%s calls 0x%x %s at this+0x%x -> stamps 0x%x",
                        e.getKey(), fnName(e.getKey()), lastThis0(e.getKey()) == null ? "-" : "0x" + Long.toHexString(lastThis0(e.getKey())),
                        t, fnName(t), v.disp, tt));
            }
        }

        // ================= SECTION A: allocations =================
        println("");
        println("@@@@@@ SECTION A -- MemAlloc sites: size, the next direct call (ctor) and the table it stamps last at this+0 (" + allocs.size() + ")");
        allocs.sort((a, b) -> Long.compare(a[0], b[0]));
        for (long[] a : allocs) {
            Long t = a[2] == 0 ? null : ctorTable(a[2]);
            println(String.format("ALLOC site=0x%x in %s size=%s ctor=%s table=%s",
                    a[0], fnName(a[0]), a[1] < 0 ? "?" : "0x" + Long.toHexString(a[1]),
                    a[2] == 0 ? "-" : String.format("0x%x %s", a[2], fnName(a[2])), t == null ? "-" : "0x" + Long.toHexString(t)));
        }

        // ================= SECTION P: parents =================
        println("");
        println("@@@@@@ SECTION P -- parent per stamped table (ctor evidence: the this+0 stamp before it in the same fn, or the");
        println("@@@@@@ last call(rcx=this) before it into a fn whose last this+0 stamp is P, or a Runtime ??0 import; dtor evidence:");
        println("@@@@@@ the this+0 stamp after it, or the first call(rcx=this) after it into a stamping fn or a Runtime ??1 import)");
        Map<Long, TreeSet<String>> ctorPar = new TreeMap<>(), dtorPar = new TreeMap<>();
        for (Map.Entry<Long, List<Ev>> e : chainOf.entrySet()) {
            List<Ev> evs = e.getValue();
            for (int i = 0; i < evs.size(); i++) {
                Ev v = evs.get(i);
                if (!v.kind.equals("STAMP") || !v.isThis) continue;
                // backwards
                for (int j = i - 1; j >= 0; j--) {
                    Ev w = evs.get(j);
                    if (w.kind.equals("STAMP") && w.isThis && w.disp == v.disp) { add(ctorPar, v.table, String.format("0x%x (inline, fn 0x%x)", w.table, e.getKey())); break; }
                    if (w.kind.equals("CALL") && w.disp == v.disp) {
                        String p = callParent(w.text);
                        if (p != null) { add(ctorPar, v.table, p + String.format(" (call, fn 0x%x)", e.getKey())); break; }
                    }
                }
                for (int j = i + 1; j < evs.size(); j++) {
                    Ev w = evs.get(j);
                    if (w.kind.equals("STAMP") && w.isThis && w.disp == v.disp) { add(dtorPar, v.table, String.format("0x%x (inline, fn 0x%x)", w.table, e.getKey())); break; }
                    if (w.kind.equals("CALL") && w.disp == v.disp) {
                        String p = callParent(w.text);
                        if (p != null) { add(dtorPar, v.table, p + String.format(" (call, fn 0x%x)", e.getKey())); break; }
                    }
                }
            }
        }
        for (Long t : cand) {
            if (!stampsOfTable.containsKey(t)) continue;
            println(String.format("PAR 0x%x ctor=%s dtor=%s", t,
                    ctorPar.containsKey(t) ? ctorPar.get(t) : "[]", dtorPar.containsKey(t) ? dtorPar.get(t) : "[]"));
        }
        println("");
        println("END");
    }

    void add(Map<Long, TreeSet<String>> m, long k, String v) {
        TreeSet<String> s = m.get(k);
        if (s == null) { s = new TreeSet<>(); m.put(k, s); }
        s.add(v);
    }

    String callParent(String text) {
        if (text.startsWith("IMPORT ")) {
            String n = text.substring(7);
            if (n.startsWith("??0") || n.startsWith("??1")) return n;
            return null;
        }
        if (!text.startsWith("0x")) return null;
        long t = Long.parseLong(text.substring(2, text.indexOf(' ')), 16);
        long r = resolveJmp(t);
        String si = stubImport(r);
        if (si != null) { String n = shortImp(si); return (n.startsWith("??0") || n.startsWith("??1")) ? n : null; }
        Long a = lastThis0(r);
        if (a == null) return null;
        Long f0 = firstThis0(r);
        // a ctor ends on its own table; a dtor starts on it
        return String.format("0x%x", a) + (f0 != null && !f0.equals(a) ? String.format("/first 0x%x", f0) : "");
    }
}
