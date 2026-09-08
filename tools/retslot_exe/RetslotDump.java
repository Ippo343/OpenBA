import ghidra.app.script.GhidraScript;
import ghidra.framework.plugintool.PluginTool;
import ghidra.app.services.ProgramManager;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.Structure;
import ghidra.program.model.data.Union;
import ghidra.program.model.data.Array;
import ghidra.program.model.data.TypeDef;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.listing.Program;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

public class RetslotDump extends GhidraScript {
    Program findProgram(String name) {
        PluginTool tool = state.getTool();
        ProgramManager pm = tool.getService(ProgramManager.class);
        for (Program p : pm.getAllOpenPrograms()) {
            if (p.getName().equals(name)) return p;
        }
        return null;
    }

    static DataType strip(DataType t) {
        while (t instanceof TypeDef) t = ((TypeDef) t).getBaseDataType();
        return t;
    }

    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        String inPath = a[0];
        String outPath = a[1];
        Program prog = findProgram("Application-x64.exe");
        if (prog == null) { println("Application-x64.exe NOT OPEN"); return; }

        List<String> addrs = new ArrayList<String>();
        BufferedReader br = new BufferedReader(new FileReader(inPath));
        String line;
        while ((line = br.readLine()) != null) {
            line = line.trim();
            if (line.length() > 0) addrs.add(line);
        }
        br.close();

        PrintWriter out = new PrintWriter(outPath, "UTF-8");
        out.println("addr\tname\tsigsrc\tconv\tnparams\tretname\tretlen\tretkind\tvarargs\tepilogue\tproto");
        int n = 0;
        for (String s : addrs) {
            Address ad = prog.getAddressFactory().getAddress(s);
            Function f = prog.getFunctionManager().getFunctionAt(ad);
            if (f == null) {
                Function c = prog.getFunctionManager().getFunctionContaining(ad);
                out.println(s + "\tNO_FUNCTION_AT_ADDR" + (c == null ? "" : " (inside " + c.getName() + " @" + c.getEntryPoint() + ")")
                        + "\t-\t-\t-\t-\t-\t-\t-\t-\t-");
                n++;
                continue;
            }
            DataType rt = f.getReturnType();
            DataType rs = strip(rt);
            String kind = "scalar";
            if (rs instanceof Structure) kind = "STRUCT";
            else if (rs instanceof Union) kind = "UNION";
            else if (rs instanceof Array) kind = "ARRAY";
            else if (rs.getName().equals("void")) kind = "void";

            // last 6 instructions -- the MSVC by-value-return epilogue restores the
            // hidden slot pointer into RAX, so the tail is load-bearing evidence
            // that Ghidra's declared return type alone cannot give.
            StringBuilder tail = new StringBuilder();
            List<String> insns = new ArrayList<String>();
            Instruction ins = prog.getListing().getInstructionAt(f.getEntryPoint());
            while (ins != null && f.getBody().contains(ins.getAddress())) {
                insns.add(ins.toString());
                ins = ins.getNext();
            }
            int from = Math.max(0, insns.size() - 6);
            for (int i = from; i < insns.size(); i++) {
                if (tail.length() > 0) tail.append(" | ");
                tail.append(insns.get(i));
            }

            out.println(s + "\t" + f.getName()
                    + "\t" + f.getSignatureSource()
                    + "\t" + f.getCallingConventionName()
                    + "\t" + f.getParameterCount()
                    + "\t" + rt.getName()
                    + "\t" + rt.getLength()
                    + "\t" + kind
                    + "\t" + f.hasVarArgs()
                    + "\t" + tail.toString()
                    + "\t" + f.getSignature().getPrototypeString());
            n++;
        }
        out.close();
        println("dumped " + n + " rows to " + outPath);
    }
}
