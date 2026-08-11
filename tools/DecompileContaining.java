import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.app.decompiler.*;
import java.io.File;
import java.io.FileWriter;

public class DecompileContaining extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Address addr = currentProgram.getAddressFactory().getAddress(args[0]);
        Function func = currentProgram.getFunctionManager().getFunctionContaining(addr);
        if (func == null) {
            println("No function contains " + args[0]);
            return;
        }
        
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        DecompileResults res = decomp.decompileFunction(func, 30, monitor);
        
        File out = new File(args[1], func.getName() + ".c");
        out.getParentFile().mkdirs();
        FileWriter fw = new FileWriter(out);
        fw.write(res.getDecompiledFunction().getC());
        fw.close();
        println("Decompiled " + func.getName() + " to " + out.getAbsolutePath());
    }
}
