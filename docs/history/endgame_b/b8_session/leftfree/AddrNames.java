// Ghidra inline script (run through the MCP's run_script_inline): for every address in
// addrs.txt, the function name Ghidra has at it, its namespace, prototype and calling
// convention -> ghidra_names.tsv. The exe is stripped: the names are FUN_ / ad-hoc, never a
// retail symbol; the CLASS of a body is read from the body (its RCX object), not from here.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.nio.file.*;
import java.util.*;
public class AddrNames extends GhidraScript {
    @Override
    public void run() throws Exception {
        Program p = currentProgram;
        if (!p.getName().startsWith("Application")) {
            for (Program q : state.getTool().getService(ghidra.app.services.ProgramManager.class).getAllOpenPrograms()) {
                if (q.getName().startsWith("Application")) { p = q; }
            }
        }
        String dir = "C:/code/GIT/untitled_rev_eng_project/docs/runtime_port/endgame_b/b8_session/leftfree/";
        List<String> lines = Files.readAllLines(Paths.get(dir + "addrs.txt"));
        StringBuilder sb = new StringBuilder();
        FunctionManager fm = p.getFunctionManager();
        for (String l : lines) {
            l = l.trim(); if (l.isEmpty()) continue;
            if (!l.startsWith("0x14")) { sb.append(l).append("\tDLL\t\t\n"); continue; }
            Address a = p.getAddressFactory().getAddress(l.substring(2));
            Function f = fm.getFunctionAt(a);
            if (f == null) { Function g = fm.getFunctionContaining(a); sb.append(l).append("\tNOFUNC\t").append(g == null ? "" : g.getName(true)).append("\t\n"); continue; }
            String ns = f.getParentNamespace() == null ? "" : f.getParentNamespace().getName(true);
            String cc = "";
            try { cc = f.getCallingConventionName(); } catch (Exception e) {}
            sb.append(l).append("\t").append(f.getName(true)).append("\t").append(ns).append("\t").append(f.getSignature().getPrototypeString()).append("\t").append(cc).append("\n");
        }
        Files.write(Paths.get(dir + "ghidra_names.tsv"), sb.toString().getBytes("UTF-8"));
        println("wrote " + lines.size());
    }
}
