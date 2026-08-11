import sys
sys.path.insert(0, 'tools')
from sles import Sles
from mips import decode

s = Sles('game-iso/SHO/SLES_551.47')
va = 0x1f62d0

print(f"--- Disassembly from {hex(va)} ---")
for i in range(25):
    addr = va + i * 4
    off = s.va2off(addr)
    if off is None:
        break
    w = int.from_bytes(s.d[off:off+4], 'little')
    inst = decode(w, addr)
    
    # Try to resolve targets
    target = ""
    if inst[0] in ('jal', 'j'):
        target_addr = ((addr + 4) & 0xF0000000) | ((w & 0x03FFFFFF) << 2)
        target = f" -> {hex(target_addr)}"
    
    print(f"{hex(addr)}: {inst[0]} {inst[1]}{target}")
