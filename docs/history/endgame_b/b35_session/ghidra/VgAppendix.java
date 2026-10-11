// B35_SLOTS.md source data: every Video/Graphics vtable slot, the five parent vtables,
// the _TypeName/_ParentTypeName literals, the hidden-retslot exports, and the
// non-polymorphic classes' exported members. Read from the RETAIL Runtime-x64.dll.
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
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

public class VgAppendix extends GhidraScript {

    SymbolTable st;
    Memory mem;
    FunctionManager fm;
    AddressSpace sp;
    Listing lst;

    // class -> its retail vtable VA (all 39 + the five parents), read in the previous pass.
    Map<String, Long> vtOf = new HashMap<>();
    // class -> parent class name (proved by _ParentTypeName, or by shared slot targets + sizeof)
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

        // proved by ?_ParentTypeName@
        parentOf.put("Video::RenderState", "Object::External");
        parentOf.put("Video::VideoProgram", "Object::External");
        parentOf.put("Graphics::Material", "Object::External");
        parentOf.put("Graphics::MaterialInstance", "Object::External");
        parentOf.put("Graphics::AnimationEvent", "Object::External");
        parentOf.put("Graphics::AnimationGroup", "Object::External");
        parentOf.put("Graphics::EdgeMesh", "Object::External");
        parentOf.put("Graphics::Particle", "Object::External");
        parentOf.put("Graphics::ParticleSystem", "Graphics::Particle");
        parentOf.put("Graphics::GraphicsMesh", "Object::CompiledExternal");
        parentOf.put("Video::VertexLayout", "Object::CompiledExternal");
        parentOf.put("Video::ShaderProgram", "Object::CompiledExternal");
        parentOf.put("Video::VertexProgram", "Video::ShaderProgram");
        parentOf.put("Video::PixelProgram", "Video::ShaderProgram");
        parentOf.put("Video::ImageBuffer", "Object::CompiledExternal");
        parentOf.put("Graphics::GeneratedImageBuffer", "Video::ImageBuffer");
        parentOf.put("Graphics::FrameCopyImageBuffer", "Graphics::GeneratedImageBuffer");
        parentOf.put("Graphics::ReflectionImageBuffer", "Graphics::GeneratedImageBuffer");
        // proved by shared slot targets + sizeof (no _typeInfo, no _ParentTypeName)
        parentOf.put("Graphics::MeshInstance", "Graphics::Geometry");
        parentOf.put("Graphics::EdgeMeshInstance", "Graphics::Geometry");
        parentOf.put("Graphics::ParticleInstance", "Graphics::Geometry");
        parentOf.put("Graphics::ParticleSystemInstance", "Graphics::ParticleInstance");
        parentOf.put("Graphics::Light", "Graphics::Geometry");
        parentOf.put("Graphics::AmbientLight", "Graphics::Light");
        parentOf.put("Graphics::SpotLight", "Graphics::Light");
        parentOf.put("Graphics::SunLight", "Graphics::Light");
        // roots: Graphics::Geometry, the twelve Video::*Data
    }

    // The @-separated name fragments of an MSVC mangled name, innermost first, with the
    // leading operator code skipped: "??0Viewport@Video@@..." -> [Viewport, Video].
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
                if (sb.length() == 0) break;     // the @@ terminator
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

    // Read a vtable as a list of slot target addresses (0 = terminator / end of array).
    List<Long> readVt(long va, java.util.Set<Long> vtAddrs) {
        List<Long> out = new ArrayList<>();
        Address a = sp.getAddress(va);
        for (int i = 0; i < 64; i++) {
            Address cur = a.add(i * 8L);
            if (i > 0 && vtAddrs.contains(cur.getOffset())) break;
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

        // ---- collect the 39 Video/Graphics vtables -------------------------------
        java.util.Set<Long> vtAddrs = new java.util.HashSet<>();
        Map<String, Long> vg = new TreeMap<>();
        for (Symbol s : st.getAllSymbols(true)) {
            String n = s.getName();
            if (!n.startsWith("??_7")) continue;
            if (n.contains("@Video@@6B@") || n.contains("@Graphics@@6B@")) {
                String cls = n.substring(4, n.length() - 5);       // "??_7X@Ns@@6B@" -> "X@Ns"
                int at = cls.lastIndexOf('@');
                String key = cls.substring(at + 1) + "::" + cls.substring(0, at);
                vg.put(key, s.getAddress().getOffset());
                vtOf.put(key, s.getAddress().getOffset());
                vtAddrs.add(s.getAddress().getOffset());
            }
        }
        for (Long v : vtOf.values()) vtAddrs.add(v);

        println("");
        println("@@@@@@ SECTION 1 -- " + vg.size() + " minted-vtable classes");
        emit(vg, vtAddrs);

        Map<String, Long> parents = new java.util.LinkedHashMap<>();
        parents.put("Object::Base", vtOf.get("Object::Base"));
        parents.put("Object::External", vtOf.get("Object::External"));
        parents.put("Object::CompiledExternal", vtOf.get("Object::CompiledExternal"));
        parents.put("Object::Internal", vtOf.get("Object::Internal"));
        parents.put("Object::ExternalList", vtOf.get("Object::ExternalList"));
        println("");
        println("@@@@@@ SECTION 2 -- the five parent vtables");
        emit(parents, vtAddrs);

        // ---- SECTION 3: the literals --------------------------------------------
        println("");
        println("@@@@@@ SECTION 3 -- _TypeName / _ParentTypeName literals");
        List<String> lits = new ArrayList<>();
        for (Symbol s : st.getAllSymbols(true)) {
            String n = s.getName();
            if (!n.startsWith("?_ParentTypeName@") && !n.startsWith("?_TypeName@")) continue;
            if (!(n.contains("@Video@@") || n.contains("@Graphics@@"))) continue;
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
            lits.add(String.format("0x%s  %-58s -> \"%s\"", s.getAddress(), n, lit));
        }
        Collections.sort(lits, new Comparator<String>() {
            public int compare(String a, String b) {
                return a.substring(12).compareTo(b.substring(12));
            }
        });
        println("count=" + lits.size());
        for (String l : lits) println(l);

        // ---- SECTION 4: hidden retslot exports ----------------------------------
        println("");
        println("@@@@@@ SECTION 4 -- hidden-retslot exports (?AV / ?AU); ?AW4 listed separately");
        List<String> rs = new ArrayList<>(), en = new ArrayList<>();
        for (Symbol s : st.getAllSymbols(true)) {
            String n = s.getName();
            if (!n.startsWith("?") || n.startsWith("??_") || n.startsWith("?_")) continue;
            if (!(n.contains("@Video@@") || n.contains("@Graphics@@"))) continue;
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

        // ---- SECTION 5: non-polymorphic classes' exports ------------------------
        println("");
        println("@@@@@@ SECTION 5 -- exported members of the non-polymorphic classes");
        String[] want = {
            "Video::Context", "Video::Device", "Video::Configuration", "Video::RenderBuffer",
            "Video::IndexBuffer", "Video::VertexBuffer", "Video::ConstantBuffer",
            "Video::Viewport", "Graphics::Camera", "Graphics::Scene", "Graphics::Renderer",
            "Graphics::Animation", "Graphics::AnimationManager", "Graphics::AnimationState"
        };
        Map<String, List<String>> byCls = new TreeMap<>();
        for (Symbol s : st.getAllSymbols(true)) {
            String n = s.getName();
            if (!n.startsWith("?")) continue;
            if (!(n.contains("@Video@@") || n.contains("@Graphics@@"))) continue;
            // Fragment immediately before the namespace fragment. The operator prefix
            // (??0 / ??1 / ??_7 / ??4 ...) must be skipped first, or "??0Viewport@Video@@"
            // buckets as "??0Viewport" and every top-level ctor/dtor goes missing.
            String[] f = nameFragments(n);
            if (f == null) continue;
            String ns = null, cls = null;
            for (int k = 0; k < f.length; k++) {
                if (f[k].equals("Video") || f[k].equals("Graphics")) {
                    ns = f[k];
                    if (k >= 1) cls = f[k - 1];
                    break;
                }
            }
            if (cls == null) continue;
            String key = ns + "::" + cls;
            List<String> l = byCls.get(key);
            if (l == null) { l = new ArrayList<>(); byCls.put(key, l); }
            l.add(String.format("0x%s  %s", s.getAddress(), n));
        }
        for (String w : want) {
            List<String> l = byCls.get(w);
            println("");
            println("##### " + w + "   exports=" + (l == null ? 0 : l.size()));
            if (l == null) continue;
            Collections.sort(l, new Comparator<String>() {
                public int compare(String a, String b) {
                    return a.substring(14).compareTo(b.substring(14));
                }
            });
            for (String x : l) println("    " + x);
        }
    }

    void emit(Map<String, Long> classes, java.util.Set<Long> vtAddrs) {
        for (Map.Entry<String, Long> e : classes.entrySet()) {
            String cls = e.getKey();
            long va = e.getValue();
            String par = parentOf.get(cls);
            Long parVa = (par == null) ? null : vtOf.get(par);
            List<Long> pslots = (parVa == null) ? new ArrayList<Long>() : readVt(parVa, vtAddrs);
            List<Long> slots = readVt(va, vtAddrs);
            println("");
            println(String.format("### %s  vtable=0x%x  slots=%d  sizeof=%s  parent=%s",
                    cls, va, slots.size(), sizeOf(va),
                    par == null ? "ROOT (no base vtable shares a slot target)" : par));
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
