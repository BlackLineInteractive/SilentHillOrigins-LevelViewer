// GhidraDecompileEverything.java — decompiles every function in the program.
//
// The existing GhidraDecompileAll.java walks the call graph outward from the
// HandleAttributes entry points, which is right when the question is "what does
// this class do with its properties" and wrong when the question is "how does
// the game work". The boot sequence, the menu state machine, the audio
// scheduler and the animation blending are not reachable from a property
// handler, so that closure misses them: it yields 464 functions out of the
// several thousand the executable contains.
//
// This one takes the whole function manager. No entry-point list, no closure,
// no filter -- if Ghidra found a function, it gets decompiled.
//
//   analyzeHeadless <projDir> <projName> \
//       -process SLES_551.47 -noanalysis \
//       -scriptPath ./tools \
//       -postScript GhidraDecompileEverything.java <outDir> [indexFile]
//
// Files are named <Name>_<address>.c so that two functions Ghidra gave the same
// name (thunks, or several FUN_ at different addresses) cannot overwrite each
// other -- a plain name collision silently loses code, which is the sort of
// quiet data loss that is hard to notice later.
//
//@category Climax
import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class GhidraDecompileEverything extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("[all] need <outDir> [indexFile]");
            return;
        }

        File outDir = new File(args[0]);
        if (!outDir.isDirectory() && !outDir.mkdirs()) {
            println("[all] cannot create " + outDir);
            return;
        }

        DecompInterface decomp = new DecompInterface();
        DecompileOptions opts = new DecompileOptions();
        decomp.setOptions(opts);
        // Names and types from the analysis are worth more than raw speed here;
        // this runs once and the output is read by people.
        decomp.toggleCCode(true);
        decomp.toggleSyntaxTree(true);
        decomp.setSimplificationStyle("decompile");
        if (!decomp.openProgram(currentProgram)) {
            println("[all] decompiler would not open the program: "
                    + decomp.getLastMessage());
            return;
        }

        List<String> index = new ArrayList<>();
        int ok = 0, failed = 0, total = 0;

        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext()) {
            if (monitor.isCancelled()) break;
            Function f = it.next();
            total++;

            String addr = f.getEntryPoint().toString();
            String name = f.getName().replaceAll("[^A-Za-z0-9_]", "_");
            File out = new File(outDir, name + "_" + addr + ".c");

            DecompileResults res = decomp.decompileFunction(f, 60, monitor);
            if (res == null || !res.decompileCompleted()
                    || res.getDecompiledFunction() == null) {
                failed++;
                index.add(addr + "\t" + f.getName() + "\tFAILED");
                continue;
            }

            try (PrintWriter w = new PrintWriter(out, "UTF-8")) {
                w.println("// " + f.getName() + " @ 0x" + addr);
                w.println("// size " + f.getBody().getNumAddresses() + " bytes");
                w.println();
                w.print(res.getDecompiledFunction().getC());
            }
            ok++;
            index.add(addr + "\t" + f.getName() + "\t" + out.getName());

            if (ok % 250 == 0)
                println("[all] " + ok + " decompiled...");
        }

        decomp.dispose();

        if (args.length >= 2) {
            try (PrintWriter w = new PrintWriter(new File(args[1]), "UTF-8")) {
                w.println("# address\tname\tfile");
                for (String line : index)
                    w.println(line);
            }
        }

        println("[all] " + total + " functions, " + ok + " decompiled, "
                + failed + " failed -> " + outDir);
    }
}
