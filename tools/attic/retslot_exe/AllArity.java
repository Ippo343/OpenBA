// Ghidra script (run via GhidraMCP run_script_inline or from ghidra_scripts/).
// Screen 2's measuring instrument: for each target exe function, look at what
// RETAIL'S OWN COMPILER emitted at every call site -- the highest MS-x64
// argument register written on the run-up to the CALL. This does NOT ask Ghidra
// what the signature is, which matters because 231 of the 234 signatures in the
// audited corpus are SignatureSource.ANALYSIS and were shown to be wrong in
// BOTH directions (over-counting four rows, under-counting seven dialog ctors
// that Ghidra called 1-parameter and whose call sites set four registers plus
// three stack arguments).
//
// args: <file with one 0x14....... per line> <output tsv>
import ghidra.app.script.GhidraScript;
import ghidra.app.services.ProgramManager;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Program;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public class AllArity extends GhidraScript {
    Program findProgram(String name) {
        ProgramManager pm = state.getTool().getService(ProgramManager.class);
        for (Program p : pm.getAllOpenPrograms()) if (p.getName().equals(name)) return p;
        return null;
    }

    static int idx(String r) {
        if (r.equals("RCX") || r.equals("ECX") || r.equals("CX") || r.equals("CL")) return 1;
        if (r.equals("RDX") || r.equals("EDX") || r.equals("DX") || r.equals("DL")) return 2;
        if (r.equals("R8") || r.equals("R8D") || r.equals("R8W") || r.equals("R8B")) return 3;
        if (r.equals("R9") || r.equals("R9D") || r.equals("R9W") || r.equals("R9B")) return 4;
        if (r.startsWith("XMM0")) return 1;
        if (r.startsWith("XMM1")) return 2;
        if (r.startsWith("XMM2")) return 3;
        if (r.startsWith("XMM3")) return 4;
        return 0;
    }

    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        Program prog = findProgram("Application-x64.exe");
        List<String> targets = new ArrayList<String>();
        BufferedReader br = new BufferedReader(new FileReader(a[0]));
        String line;
        while ((line = br.readLine()) != null) { line = line.trim(); if (line.length() > 0) targets.add(line); }
        br.close();
        PrintWriter out = new PrintWriter(a[1], "UTF-8");
        out.println("addr\tname\tsites\tmaxreg\tstackarg\tregs");

        for (String t : targets) {
            Address tad = prog.getAddressFactory().getAddress(t);
            Function tf = prog.getFunctionManager().getFunctionAt(tad);
            ReferenceIterator it = prog.getReferenceManager().getReferencesTo(tad);
            int nsites = 0, maxreg = 0;
            boolean stackArg = false;
            Set<String> union = new LinkedHashSet<String>();
            while (it.hasNext()) {
                Reference r = it.next();
                if (!r.getReferenceType().isCall()) continue;
                Instruction call = prog.getListing().getInstructionAt(r.getFromAddress());
                if (call == null) continue;
                nsites++;
                // Walk back to the previous CALL or branch. This window is a
                // SUPERSET of the argument setup: it also catches scratch loads
                // (0x140055430's caller loads R8 for an IMUL), which is why a
                // row this flags still has to be read by hand.
                Instruction cur = call.getPrevious();
                for (int i = 0; i < 40 && cur != null; i++) {
                    if (cur.getFlowType().isCall()) break;
                    for (Object o : cur.getResultObjects()) {
                        if (o instanceof Register) {
                            String nm = ((Register) o).getName();
                            int k = idx(nm);
                            if (k > 0) { union.add(nm); if (k > maxreg) maxreg = k; }
                        }
                    }
                    String s = cur.toString();
                    if (s.startsWith("MOV") && (s.contains("[RSP + 0x20]") || s.contains("[RSP + 0x28]")
                            || s.contains("[RSP + 0x30]") || s.contains("[RSP + 0x38]"))) stackArg = true;
                    if (cur.getFlowType().isJump()) break;
                    cur = cur.getPrevious();
                }
            }
            out.println(t + "\t" + (tf == null ? "?" : tf.getName()) + "\t" + nsites + "\t"
                    + maxreg + "\t" + stackArg + "\t" + union);
        }
        out.close();
        println("done " + targets.size());
    }
}
