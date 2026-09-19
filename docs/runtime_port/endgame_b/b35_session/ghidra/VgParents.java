// Read the literal each ?_ParentTypeName@X@{Video,Graphics}@@ returns.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class VgParents extends GhidraScript {
    String readCStr(Address a) {
        StringBuilder sb = new StringBuilder();
        try {
            for (int i = 0; i < 128; i++) {
                byte b = currentProgram.getMemory().getByte(a.add(i));
                if (b == 0) break;
                sb.append((char) (b & 0xff));
            }
        } catch (Exception e) { return "<unreadable>"; }
        return sb.toString();
    }

    @Override
    public void run() throws Exception {
        List<String> out = new ArrayList<>();
        for (Symbol s : currentProgram.getSymbolTable().getAllSymbols(true)) {
            String n = s.getName();
            if (!n.startsWith("?_ParentTypeName@") && !n.startsWith("?_TypeName@")) continue;
            if (!(n.contains("@Video@@") || n.contains("@Graphics@@"))) continue;
            Function f = currentProgram.getFunctionManager().getFunctionAt(s.getAddress());
            if (f == null) { out.add(n + " -> <no fn>"); continue; }
            String lit = "<none>";
            InstructionIterator ii = currentProgram.getListing().getInstructions(f.getBody(), true);
            while (ii.hasNext()) {
                Instruction ins = ii.next();
                if (!ins.getMnemonicString().equals("LEA")) continue;
                for (Reference r : ins.getReferencesFrom()) {
                    Address t = r.getToAddress();
                    if (currentProgram.getMemory().contains(t)) lit = readCStr(t);
                }
            }
            out.add(n + "  ->  \"" + lit + "\"");
        }
        Collections.sort(out);
        println("### count = " + out.size());
        for (String l : out) println(l);
    }
}
