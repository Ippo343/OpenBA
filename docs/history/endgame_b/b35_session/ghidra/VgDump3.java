// Per-class export inventory for Video and Graphics, from the retail Runtime-x64.dll.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

public class VgDump3 extends GhidraScript {

    // For "?Name@Class@Ns@@..." or "??0Class@Ns@@..." pull out Class and Ns.
    String[] classOf(String m) {
        if (!m.startsWith("?")) return null;
        int i = 1;
        if (m.charAt(1) == '?') {
            // ??0 / ??1 / ??_7 / ??4 ... skip the operator code
            i = 2;
            if (m.charAt(2) == '_') i = 4; else i = 3;
        }
        // now read @-separated name fragments
        List<String> parts = new ArrayList<>();
        StringBuilder sb = new StringBuilder();
        while (i < m.length()) {
            char c = m.charAt(i);
            if (c == '@') {
                if (sb.length() == 0) break; // @@ terminator
                parts.add(sb.toString());
                sb.setLength(0);
            } else {
                sb.append(c);
            }
            i++;
        }
        if (parts.size() < 2) return null;
        // parts are innermost-first: [member, Class, Ns] for ?Name@Class@Ns@@
        // or [Class, Ns] for ??0Class@Ns@@
        return parts.toArray(new String[0]);
    }

    @Override
    public void run() throws Exception {
        SymbolTable st = currentProgram.getSymbolTable();
        Map<String, List<String>> byClass = new TreeMap<>();
        for (Symbol s : st.getAllSymbols(true)) {
            String n = s.getName();
            if (!n.startsWith("?")) continue;
            if (!(n.contains("@Video@@") || n.contains("@Graphics@@")
                  || n.contains("@Video@") || n.contains("@Graphics@"))) continue;
            String[] p = classOf(n);
            if (p == null) continue;
            // find the fragment right before "Video" / "Graphics"
            String cls = null, ns = null;
            for (int k = 0; k < p.length; k++) {
                if (p[k].equals("Video") || p[k].equals("Graphics")) {
                    ns = p[k];
                    if (k >= 1) cls = p[k - 1];
                    break;
                }
            }
            if (cls == null) continue;
            String key = ns + "::" + cls;
            List<String> l = byClass.get(key);
            if (l == null) { l = new ArrayList<>(); byClass.put(key, l); }
            l.add(s.getAddress() + " " + n);
        }
        for (Map.Entry<String, List<String>> e : byClass.entrySet()) {
            List<String> l = e.getValue();
            Collections.sort(l);
            boolean hasVt = false, hasTi = false;
            List<String> ctors = new ArrayList<>();
            for (String x : l) {
                String nm = x.substring(x.indexOf(' ') + 1);
                if (nm.startsWith("??_7")) hasVt = true;
                if (nm.startsWith("?_typeInfo@")) hasTi = true;
                if (nm.startsWith("??0") || nm.startsWith("??1")) ctors.add(x);
            }
            println("");
            println("##### " + e.getKey() + "   exports=" + l.size()
                    + "  vtable=" + (hasVt ? "YES" : "no")
                    + "  _typeInfo=" + (hasTi ? "YES" : "no"));
            for (String c : ctors) println("    " + c);
        }
    }
}
