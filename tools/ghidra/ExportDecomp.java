// Decompile every function and write to <arg0>/decomp.c, plus a function list.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;

public class ExportDecomp extends GhidraScript {
    @Override
    public void run() throws Exception {
        String outDir = getScriptArgs()[0];
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        try (PrintWriter c = new PrintWriter(new FileWriter(outDir + "/decomp.c"));
             PrintWriter l = new PrintWriter(new FileWriter(outDir + "/functions.txt"))) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (monitor.isCancelled()) break;
                l.printf("%s %d %s%n", f.getEntryPoint(), f.getBody().getNumAddresses(), f.getName());
                DecompileResults r = di.decompileFunction(f, 60, monitor);
                c.printf("// ==== %s @ %s%n", f.getName(), f.getEntryPoint());
                if (r != null && r.decompileCompleted()) c.println(r.getDecompiledFunction().getC());
                else c.println("// decompile failed");
            }
        }
    }
}
