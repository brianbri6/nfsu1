#!/usr/bin/env python3
"""Turn the x86 code inside EA's AEMS sound banks (Sound/**/*.abk) into a
liftable analysis module.

  abk_module.py <game_root> <analysis.dll> <seeds.json> <blocks.c>

An ABKC bank carries a few small x86 routines between the header offsets
u32 @0x84 and u32 @0x88. Speed.exe loads the bank on its heap, patches every
call operand listed in the table at u32 @0x30 (operand = index N) to call
entry N of its function table at 0x6E9AE8, and registers routines of the bank
as callbacks, so the game ends up calling heap addresses. 106 banks hold only
23 distinct blocks.

Output (all derived from the game files: write it under work/ and
src/recomp/gen_abk/, never commit it):
  analysis.dll  a minimal PE32 at ABK_BASE holding every distinct block at its
                own 4 KB-aligned slot, call operands already resolved to the
                Speed.exe target, so lift32 emits RECOMP_ICALL(0x0064xxxx).
  seeds.json    each block start and every routine start after a ret.
  blocks.c      the original (unpatched) block bytes plus the operand offsets,
                for the runtime to recognise a loaded bank by its content
                (src/runtime/abk_code.c).
The game files are only read.
"""
import glob, hashlib, json, os, struct, sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

ABK_BASE = 0x10000000
SPEED_BASE = 0x400000
CALL_TABLE = 0x6E9AE8          # Speed.exe AEMS function table (entry N = e8 operand N)
TEXT_RVA = 0x1000


def speed_image(root):
    import pefile
    pe = pefile.PE(os.path.join(root, "Speed.exe"), fast_load=True)
    if pe.OPTIONAL_HEADER.SizeOfImage != 0x394368 or pe.OPTIONAL_HEADER.AddressOfEntryPoint != 0x270CB5:
        sys.exit("abk_module: Speed.exe is not the 1.4 build this port targets")
    return pe.get_memory_mapped_image()


def banks(root):
    out = {}
    for p in sorted(glob.glob(os.path.join(root, "Sound", "**", "*.abk"), recursive=True)):
        d = open(p, "rb").read()
        if d[:4] != b"ABKC":
            sys.exit(f"abk_module: {p} is not an ABKC bank")
        cs, ce = struct.unpack_from("<II", d, 0x84)
        rt = struct.unpack_from("<I", d, 0x30)[0]
        n = struct.unpack_from("<I", d, rt)[0]
        rel = [r - cs for r in struct.unpack_from(f"<{n}I", d, rt + 4)]
        if not (0 < cs < ce <= len(d)) or any(not 0 <= r <= ce - cs - 4 or d[cs + r - 1] != 0xE8 for r in rel):
            sys.exit(f"abk_module: {p}: code range or call table not understood")
        code = d[cs:ce]
        key = hashlib.sha1(code + struct.pack(f"<{n}H", *rel)).hexdigest()
        out.setdefault(key, {"code": code, "rel": rel, "files": []})["files"].append(
            os.path.relpath(p, root).replace("\\", "/"))
    return list(out.values())


def routine_starts(code, va):
    """Block start plus each routine after a ret (linear sweep; padding skipped)."""
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    starts, after_ret = [va], False
    off = 0
    while off < len(code):
        if code[off] in (0x00, 0x90, 0xCC) and (after_ret or off == 0):
            off += 1
            continue
        ins = next(md.disasm(code[off:], va + off, 1), None)
        if ins is None:
            break
        if after_ret:
            starts.append(va + off)
            after_ret = False
        if ins.mnemonic == "ret":
            after_ret = True
        off += ins.size
    return sorted(set(starts))


def write_pe(path, image_size, text):
    """Minimal PE32 DLL: one RX .text section at TEXT_RVA, file offset == RVA."""
    hdr = bytearray(TEXT_RVA)
    struct.pack_into("<2s58xI", hdr, 0, b"MZ", 0x40)
    struct.pack_into("<4sHHIIIHH", hdr, 0x40, b"PE\0\0", 0x14C, 1, 0, 0, 0, 0xE0, 0x2102)
    opt = 0x58
    struct.pack_into("<HBBIIIIIIIIIHHHHHHIIIIHHIIIIII", hdr, opt,
                     0x10B, 1, 0, len(text), 0, 0, 0, TEXT_RVA, 0, ABK_BASE, 0x1000, 0x1000,
                     4, 0, 0, 0, 4, 0, 0, image_size, TEXT_RVA, 0, 2, 0,
                     0x100000, 0x1000, 0x100000, 0x1000, 0, 16)
    struct.pack_into("<8sIIIIIIHHI", hdr, opt + 0xE0, b".text", len(text), TEXT_RVA,
                     len(text), TEXT_RVA, 0, 0, 0, 0, 0x60000020)
    open(path, "wb").write(bytes(hdr) + text)


def main(root, dll, seeds_path, blocks_c):
    img = speed_image(root)
    table = lambda n: struct.unpack_from("<I", img, CALL_TABLE - SPEED_BASE + 4 * n)[0]
    blocks = banks(root)
    text, seeds, rows = bytearray(), [], []
    for b in blocks:
        va = ABK_BASE + TEXT_RVA + len(text)
        code = bytearray(b["code"])
        for r in b["rel"]:
            n = struct.unpack_from("<I", code, r)[0]
            target = table(n)
            if not 0x401000 <= target < 0x697000:
                sys.exit(f"abk_module: {b['files'][0]}: call index {n} -> {target:#x} is not Speed.exe code")
            struct.pack_into("<i", code, r, target - (va + r + 4))
        for s in routine_starts(b["code"], va):
            seeds.append({"address": s, "note": "abk " + b["files"][0]})
        rows.append((va, b))
        text += code + b"\xCC" * (-len(code) % 0x1000)
    write_pe(dll, TEXT_RVA + len(text), bytes(text))
    json.dump(seeds, open(seeds_path, "w"), indent=1)

    with open(blocks_c, "w", newline="\n") as f:
        f.write("/* AUTO-GENERATED by scripts/abk_module.py from the local Sound/*.abk. DO NOT EDIT, DO NOT COMMIT. */\n")
        f.write('#include "nfs_abk.h"\n\n')
        for i, (va, b) in enumerate(rows):
            f.write(f"/* {', '.join(b['files'][:4])}{' ...' if len(b['files']) > 4 else ''} */\n")
            f.write(f"static const uint8_t abk_code_{i}[{len(b['code'])}] = {{")
            f.write(",".join(f"0x{x:02X}" for x in b["code"]) + "};\n")
            f.write(f"static const uint16_t abk_rel_{i}[{max(1, len(b['rel']))}] = {{")
            f.write(",".join(str(r) for r in b["rel"]) or "0")
            f.write("};\n")
        f.write("\nconst nfs_abk_block_t nfs_abk_blocks[] = {\n")
        for i, (va, b) in enumerate(rows):
            f.write(f"    {{ 0x{va:08X}u, abk_code_{i}, {len(b['code'])}u, abk_rel_{i}, {len(b['rel'])}u }},\n")
        f.write("};\n")
        f.write(f"const unsigned nfs_abk_block_count = {len(rows)};\n")
    print(f"[*] {len(blocks)} distinct code blocks, {sum(len(b['code']) for b in blocks)} bytes, "
          f"{len(seeds)} routine seeds -> {dll}")


if __name__ == "__main__":
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    main(*sys.argv[1:])
