#!/usr/bin/env python3
"""Seeds for switch arms the catalog's jump-table recovery missed.

  switch_seeds.py <analysis.exe> <catalog.json> <out.json> [--merge seeds.json ...]

sub_005C3E7D dispatches through `jmp [eax*4 + 0x5C4C6E]`, a 288-entry table
inside .text; the catalog knew only part of it and the boot stopped at
"ITAIL to unlifted guest address 0x005C4A7F". For every such indirect jump in
every catalogued function, read the table while its entries stay inside that
function, and seed each target that is not already a block start or jump
target. A seed inside a function lifts as an alias entry: the lifted switch
tail-calls it with the same registers, as the original jump would run it.
"""
import json, struct, sys
import capstone


def main(argv):
    exe, catalog, out = argv[0], argv[1], argv[2]
    merge = argv[argv.index("--merge") + 1:] if "--merge" in argv else []
    data = open(exe, "rb").read()
    cat = json.load(open(catalog))
    base = cat["image_base"]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    found = set()
    tables = 0
    for f in cat["functions"]:
        lo, hi = f["address"], f["end"]
        if hi <= lo or hi - lo > 0x40000:
            continue
        known = set(f["jump_targets"]) | {b[0] for b in f["blocks"]}
        for b in f["blocks"]:
            start, end = b[0], b[1]
            for ins in md.disasm(data[start - base:end - base], start):
                if ins.mnemonic != "jmp" or not ins.operands:
                    continue
                op = ins.operands[0]
                if op.type != capstone.x86.X86_OP_MEM or op.mem.scale != 4 or not op.mem.disp or op.mem.base:
                    continue
                table = op.mem.disp & 0xFFFFFFFF
                off = table - base
                if not (0 <= off < len(data) - 4):
                    continue
                tables += 1
                for k in range(1024):
                    if off + 4 * k + 4 > len(data):
                        break
                    v = struct.unpack_from("<I", data, off + 4 * k)[0]
                    if not (lo <= v < hi):
                        break
                    if v not in known:
                        found.add(v)
    seeds = [{"address": v, "note": "switch arm"} for v in sorted(found)]
    have = set(found)
    for path in merge:
        for e in json.load(open(path)):
            a = e.get("address") if isinstance(e, dict) else e
            a = int(a, 0) if isinstance(a, str) else a
            if a not in have:
                have.add(a)
                seeds.append(e if isinstance(e, dict) else {"address": a})
    json.dump(seeds, open(out, "w"), indent=1)
    print(f"[*] {tables} switch tables read, {len(found)} unknown arms; {len(seeds)} seeds -> {out}")


if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    main(sys.argv[1:])
