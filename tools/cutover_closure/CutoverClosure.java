// Step 2 of the cutover closure.  Run in Ghidra with Runtime-x64.dll as the
// current program (Script Manager, or the MCP run_ghidra_script tool).
//
// Reads  docs/runtime_port/closure/roots.txt and ported.txt (from build_roots.py)
// Writes docs/runtime_port/closure/closure.csv
//
// The closure is every function in the DLL reachable from the roots through the
// DLL's own static call graph.  That is what has to exist in our exe for the
// DLL to be unloaded, because a faithful port of a root calls exactly what the
// original called.  It is a LOWER bound: virtual dispatch edges are only
// followed where a vtable slot target is itself a census row (minted tables
// contribute their slots as roots), and data-driven callbacks are invisible.
//
// Optional script arg: the closure directory (defaults to the repo path below).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class CutoverClosure extends GhidraScript {
    @Override
    public void run() throws Exception {
        String dir = "C:/code/GIT/untitled_rev_eng_project/docs/runtime_port/closure/";
        String[] args = getScriptArgs();
        if (args != null && args.length > 0 && !args[0].isEmpty()) dir = args[0].endsWith("/") ? args[0] : args[0] + "/";

        FunctionManager fm = currentProgram.getFunctionManager();
        AddressFactory af = currentProgram.getAddressFactory();
        Set<Long> roots = readHex(dir + "roots.txt");
        Set<Long> ported = readHex(dir + "ported.txt");

        Deque<Function> q = new ArrayDeque<>();
        Set<Long> seen = new HashSet<>();
        int rootMissing = 0;
        for (Long a : roots) {
            Address addr = af.getAddress("ram:" + Long.toHexString(a));
            Function f = fm.getFunctionAt(addr);
            if (f == null) f = fm.getFunctionContaining(addr);
            if (f == null) { rootMissing++; continue; }   // data symbol (vtable/typeinfo/singleton), not a function
            if (seen.add(f.getEntryPoint().getOffset())) q.add(f);
        }
        while (!q.isEmpty()) {
            Function f = q.poll();
            if (f.isThunk()) {
                Function t = f.getThunkedFunction(true);
                if (t != null && seen.add(t.getEntryPoint().getOffset())) q.add(t);
            }
            for (Function c : f.getCalledFunctions(monitor)) {
                if (c.isExternal()) continue;
                if (seen.add(c.getEntryPoint().getOffset())) q.add(c);
            }
        }

        Map<String, int[]> byClass = new TreeMap<>();
        long bytes = 0, unportedBytes = 0;
        int unported = 0;
        PrintWriter pw = new PrintWriter(new FileWriter(dir + "closure.csv"));
        pw.println("addr,name,namespace,size,ported,external_export");
        for (Long a : seen) {
            Function f = fm.getFunctionAt(af.getAddress("ram:" + Long.toHexString(a)));
            if (f == null) continue;
            String name = f.getName();
            Namespace ns = f.getParentNamespace();
            String nsName = (ns == null || ns.isGlobal()) ? "" : ns.getName(true);
            String top = nsName.isEmpty() ? "" : nsName.split("::")[0];
            long size = f.getBody().getNumAddresses();
            boolean isPorted = ported.contains(a);
            String cls;
            if (name.startsWith("FUN_") || name.startsWith("thunk_FUN_")) cls = "FUN_pool";
            else if (top.isEmpty()) cls = "global_named";
            else cls = top;
            int[] c = byClass.computeIfAbsent(cls, k -> new int[3]);
            c[0]++;
            bytes += size;
            if (!isPorted) { c[1]++; c[2] += (int) size; unported++; unportedBytes += size; }
            boolean isExport = false;
            for (Symbol s : currentProgram.getSymbolTable().getSymbols(f.getEntryPoint())) {
                if (s.isExternalEntryPoint()) { isExport = true; break; }
            }
            pw.println("0x" + Long.toHexString(a) + ",\"" + name.replace("\"", "'") + "\"," + cls + "," + size + ","
                       + (isPorted ? 1 : 0) + "," + (isExport ? 1 : 0));
        }
        pw.close();
        println("roots=" + roots.size() + " rootsNotFunctions=" + rootMissing);
        println("REACHABLE=" + seen.size() + " unported=" + unported + " bytes=" + bytes + " unportedBytes=" + unportedBytes);
        println("class, reachable, unported, unportedBytes");
        for (Map.Entry<String, int[]> e : byClass.entrySet())
            println(e.getKey() + ", " + e.getValue()[0] + ", " + e.getValue()[1] + ", " + e.getValue()[2]);
        println("wrote " + dir + "closure.csv  -- next: tools/cutover_closure/report.py");
    }

    private Set<Long> readHex(String path) throws IOException {
        Set<Long> out = new HashSet<>();
        try (BufferedReader br = new BufferedReader(new FileReader(path))) {
            String line;
            while ((line = br.readLine()) != null) {
                line = line.trim();
                if (!line.isEmpty()) out.add(Long.parseLong(line, 16));
            }
        }
        return out;
    }
}
