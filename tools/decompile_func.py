import subprocess
import sys

addr = sys.argv[1]
open('/tmp/seed.txt', 'w').write(f'{addr}\n')

subprocess.run([
    "/usr/local/Cellar/ghidra/12.1.2/libexec/support/analyzeHeadless",
    "/tmp/ghidra_sho_proj", "SHO_SLES",
    "-process", "SLES_551.47",
    "-noanalysis",
    "-scriptPath", "./tools",
    "-postScript", "GhidraDecompileAll.java",
    "/tmp/seed.txt",
    "/tmp/ghidra_out"
], check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

import glob
for f in glob.glob('/tmp/ghidra_out/*.c'):
    print(f"--- {f} ---")
    print(open(f).read())
