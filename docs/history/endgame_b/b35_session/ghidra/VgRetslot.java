// Every Video/Graphics export whose mangled return type is a class BY VALUE (?A...).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class VgRetslot extends GhidraScript {
    @Override
    public void run() throws Exception {
        SymbolTable st = currentProgram.getSymbolTable();
        List<String> out = new ArrayList<>();
        for (Symbol s : st.getAllSymbols(true)) {
            String n = s.getName();
            if (!n.startsWith("?") || n.startsWith("??_") || n.startsWith("?_")) continue;
            if (!(n.contains("@Video@@") || n.contains("@Graphics@@"))) continue;
            // MSVC: after the access/calling-convention code the return type follows.
            // A class/struct returned BY VALUE is encoded "?AV" / "?AU" / "?AW".
            int i = n.indexOf("?A");
            if (i < 0) continue;
            if (i + 2 >= n.length()) continue;
            char k = n.charAt(i + 2);
            if (k != 'V' && k != 'U' && k != 'W') continue;
            out.add(s.getAddress() + "  " + n);
        }
        Collections.sort(out);
        println("### BY-VALUE (hidden retslot) Video/Graphics exports = " + out.size());
        for (String l : out) println(l);
    }
}
