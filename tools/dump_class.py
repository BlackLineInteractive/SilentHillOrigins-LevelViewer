"""Dump every instance of one class out of SH.ARC, with all its properties.

`property_observations.py` summarises a class across the archive -- how many
instances, how much each property varies. That is the right tool for deciding
which property is a real knob. It is the wrong tool when the instances *are*
the data: the 164 `CInventoryItemDef` objects are the game's item table, and a
histogram of them is useless where the rows themselves are what you want.

    python3 tools/dump_class.py game-iso/SHO/SH.ARC CInventoryItemDef \\
        --json docs/generated/sho_inventory.json

Properties come out per component, keyed by index, exactly as they sit in the
`0x0704` record -- no naming, no interpretation. What an index means is settled
by comparing rows (see SHO-port/docs/COMBAT_AND_WEAPONS.md) or by reading the
class in the executable, not by this script.
"""
import argparse
import json
import struct
import sys
import zlib
from collections import defaultdict

from property_observations import classify

RW_VER = 0x1C020065


def read_value(pay, kind):
    if kind == 'name':
        return pay.split(b'\0')[0].decode('latin1', 'replace')
    if len(pay) == 4:
        if kind == 'float':
            return round(struct.unpack_from('<f', pay)[0], 4)
        return struct.unpack_from('<i', pay)[0]
    return f'<{len(pay)} bytes>'


def objects(path):
    """Yield (container, class, {component: {index: value}}) for the archive."""
    d = open(path, 'rb').read()
    _, n, _, nt_off, nt_size = struct.unpack_from('<4sIIII', d, 0)
    nt = d[nt_off:nt_off + nt_size]
    tag = struct.pack('<I', 0x0704)
    ver = struct.pack('<I', RW_VER)

    for i in range(n):
        no, off, cs, us = struct.unpack_from('<IIII', d, 0x14 + i * 16)
        name = nt[no:nt.index(b'\0', no)].decode('latin1')
        if name.endswith('.txd'):
            continue
        try:
            c = zlib.decompress(d[off:off + cs]) if us else d[off:off + cs]
        except Exception:
            continue

        pos = 0
        while True:
            o = c.find(tag, pos)
            if o < 0:
                break
            pos = o + 1
            if o + 12 > len(c) or c[o + 8:o + 12] != ver:
                continue
            size = struct.unpack_from('<I', c, o + 4)[0]
            if size == 0 or o + 12 + size > len(c):
                continue

            p, end = o + 16, o + 12 + size
            cls = comp = None
            comps = defaultdict(dict)
            while p + 8 <= end:
                rs, rid = struct.unpack_from('<II', c, p)
                if rs < 8 or p + rs > end:
                    break
                kind, idx = rid >> 24, rid & 0xFFFFFF
                pay = c[p + 8:p + rs]
                # 0x20 names the class, 0x80 opens a component, 0x00 is a
                # property of the component currently open. A 0x0704 record is
                # several components back to back, each restarting its index at
                # zero -- flattening them is the bug TODO.md section 4 records.
                if kind == 0x20 and cls is None:
                    cls = pay.split(b'\0')[0].decode('latin1', 'replace')
                elif kind == 0x80:
                    comp = pay.split(b'\0')[0].decode('latin1', 'replace')
                elif kind == 0x00 and cls and comp:
                    comps[comp][idx] = read_value(pay, classify(pay))
                p += rs
            pos = o + 12 + size
            yield name, cls, dict(comps)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('archive')
    ap.add_argument('classname')
    ap.add_argument('--json')
    args = ap.parse_args()

    out = []
    for container, cls, comps in objects(args.archive):
        if cls == args.classname:
            out.append({'container': container, 'components': comps})

    print(f'{len(out)} instances of {args.classname}')
    if not out:
        return 1
    counts = defaultdict(int)
    for o in out:
        for comp in o['components']:
            counts[comp] += 1
    for comp, n in sorted(counts.items(), key=lambda x: -x[1]):
        print(f'  {comp}: {n}')

    if args.json:
        json.dump(out, open(args.json, 'w'), indent=1)
        print(f'wrote {args.json}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
