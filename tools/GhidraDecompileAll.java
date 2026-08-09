// GhidraDecompileAll.java — декомпілює всі функції з call-graph closure
// заданих точок входу (HandleAttributes + factories).
//
// Відрізняється від GhidraDecompile.java тим, що:
//   1. Використовує GhidraReach-логіку щоб зібрати всі досяжні функції
//   2. Декомпілює кожну у окремий .c файл
//   3. Іменує файли за адресою, або за відомою назвою якщо вона є
//
// Запуск (через існуючий проект — без -import):
//
//   analyzeHeadless /tmp/ghidra_sho_proj SHO_SLES \
//       -process SLES_551.47 \
//       -noanalysis \
//       -scriptPath ./tools \
//       -postScript GhidraDecompileAll.java \
//           docs/generated/address_list.txt \
//           decomp/sho_sles_out/ghidra_full \
//           [docs/generated/known_names.txt]
//
//@category Climax
import java.io.File;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class GhidraDecompileAll extends GhidraScript {

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            println("[decompile-all] need <addressList> <outDir> [knownNames]");
            return;
        }

        File outDir = new File(args[1]);
        if (!outDir.isDirectory() && !outDir.mkdirs()) {
            println("[decompile-all] cannot create " + outDir);
            return;
        }

        // ── Завантажити відомі імена (адреса → ім'я), якщо надано ─────────────
        Map<String, String> knownNames = new HashMap<>();
        if (args.length > 2) {
            for (String raw : Files.readAllLines(Paths.get(args[2]))) {
                String line = raw.trim();
                if (line.isEmpty() || line.startsWith("#")) continue;
                String[] parts = line.split("\\s+", 2);
                if (parts.length == 2) {
                    // Normalize address to lowercase without 0x prefix for matching
                    String addr = parts[0].toLowerCase().replace("0x", "");
                    knownNames.put(addr, parts[1]);
                }
            }
            println("[decompile-all] loaded " + knownNames.size() + " known names");
        }

        // ── Зібрати всі досяжні функції через BFS з address_list.txt ──────────
        List<String> seedLines = Files.readAllLines(Paths.get(args[0]));
        Deque<Function> queue  = new ArrayDeque<>();
        Set<Function>   seen   = new HashSet<>();

        for (String raw : seedLines) {
            String line = raw.trim();
            if (line.isEmpty() || line.startsWith("#")) continue;
            String addrText = line.split("\\s+")[0];
            try {
                Address a = currentProgram.getAddressFactory()
                        .getDefaultAddressSpace().getAddress(addrText);
                Function f = getFunctionContaining(a);
                if (f == null) f = createFunction(a, addrText);
                if (f != null && seen.add(f)) queue.add(f);
            } catch (Exception e) {
                println("[decompile-all] bad address: " + addrText);
            }
        }

        println("[decompile-all] seeds: " + seen.size());

        // BFS по call graph
        while (!queue.isEmpty()) {
            Function f = queue.poll();
            for (Function callee : f.getCalledFunctions(monitor)) {
                if (seen.add(callee)) queue.add(callee);
            }
        }

        println("[decompile-all] reachable functions: " + seen.size());

        // ── Декомпілювати кожну функцію ───────────────────────────────────────
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);

        int done = 0, failed = 0, skipped = 0;
        List<Function> sorted = new ArrayList<>(seen);
        sorted.sort(Comparator.comparing(f -> f.getEntryPoint().getOffset()));

        // Індексний файл: адреса → ім'я файлу
        StringBuilder index = new StringBuilder();
        index.append("# address  filename  known_name\n");

        for (Function fn : sorted) {
            String epHex = fn.getEntryPoint().toString();
            String epShort = epHex.toLowerCase().replaceFirst("^0+", "");

            // Визначити ім'я: із knownNames або з Ghidra або FUN_адреса
            String knownName = knownNames.get(epShort);
            String fnName = (knownName != null) ? knownName
                          : (fn.getName().startsWith("FUN_") ? null : fn.getName());

            // Безпечне ім'я файлу
            String fileBase = (fnName != null)
                    ? fnName.replaceAll("[^A-Za-z0-9_.-]", "_")
                    : "FUN_" + epShort;

            File outFile = new File(outDir, fileBase + ".c");

            // Пропустити вже існуючі (incremental mode)
            if (outFile.exists()) {
                skipped++;
                index.append(String.format("0x%s  %s.c  %s\n",
                        epShort, fileBase, fnName != null ? fnName : ""));
                continue;
            }

            DecompileResults res = decomp.decompileFunction(fn, 120, monitor);
            if (!res.decompileCompleted()) {
                failed++;
                continue;
            }

            String code = res.getDecompiledFunction().getC();

            try (PrintWriter w = new PrintWriter(outFile, StandardCharsets.UTF_8)) {
                w.println("/* " + (fnName != null ? fnName : fn.getName())
                        + "  @ 0x" + epShort);
                w.println(" * Decompiled by Ghidra 12.1"
                        + " from SLES_551.47.");
                if (knownName != null)
                    w.println(" * Name recovered by tools/sho_attrs.py / sho_class_registry.json.");
                w.println(" */");
                w.println();
                w.print(code);
            }

            index.append(String.format("0x%s  %s.c  %s\n",
                    epShort, fileBase, fnName != null ? fnName : ""));
            done++;
        }

        decomp.dispose();

        // Записати індекс
        Files.write(Paths.get(outDir.getPath(), "_index.txt"),
                index.toString().getBytes(StandardCharsets.UTF_8));

        println(String.format(
                "[decompile-all] done=%d  failed=%d  skipped=%d  total=%d",
                done, failed, skipped, seen.size()));
    }

}
