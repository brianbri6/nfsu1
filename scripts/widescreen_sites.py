"""Locate the NFSU1 WidescreenFix (ThirteenAG) front-end/HUD patch sites in Speed.exe.

Prints each pattern hit with its disassembly; config/hooks.txt and
src/runtime/widescreen.c were derived from this output for the pinned build
(Speed.exe 1.4). Patterns are the mod's own (Frontend.ixx).
Usage: python scripts/widescreen_sites.py work/Speed.analysis.exe
"""
import json, re, struct, sys
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

pe = pefile.PE(sys.argv[1], fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
img = pe.get_memory_mapped_image()
md = Cs(CS_ARCH_X86, CS_MODE_32)


def find(p):
    rx = b''.join(b'.' if t == '?' else re.escape(bytes([int(t, 16)])) for t in p.split())
    return [base + m.start() for m in re.finditer(rx, img, re.S)]


def dis(va, n):
    out = []
    for i in md.disasm(img[va - base:va - base + 96], va):
        out.append(i)
        if len(out) == n:
            break
    return out


pats = {
    'hudScaleX (D8 0D [addr])': ("D8 0D ? ? ? ? 57 0F B6 B8", 1),
    'posX 320 (C7 ? ? ? ? 00 00 00 00 A0 43..)': ("C7 ? ? ? ? 00 00 00 00 A0 43 C7 ? ? ? ? 00 00 00 00 70 43", 2),
    'posX 320 (C7 ? ? ? 00 00 A0 43 C7 ...4 disp)': ("C7 ? ? ? 00 00 A0 43 C7 ? ? ? ? 00 00 00 00 70 43", 2),
    'posX 320 (C7 ? ? ? 00 00 A0 43 C7 ? ? ? 00 00 70 43)': ("C7 ? ? ? 00 00 A0 43 C7 ? ? ? 00 00 70 43", 2),
    'posX ref (D8 1D [addr])': ("D8 1D ? ? ? ? DF E0 F6 C4 41 75 0F D9 45 FC", 1),
    'mirror': ("C7 44 24 ? 00 00 DC 43 C7 44 24 ? 00 00 70 41", 4),
    'neg 320 (81 C2 C0 FE FF FF)': ("81 C2 C0 FE FF FF", 1),
    'neg 320 (05 C0 FE FF FF)': ("05 C0 FE FF FF", 1),
    'neg 320 (8D 88 C0 FE FF FF)': ("8D 88 C0 FE FF FF", 1),
    'ClientRectHook1': ("8B 06 8B 4C 24 08", 3),
    'ClientRectHook2': ("8B 07 8B 4C 24 0C", 3),
    'HudHook': ("8B 47 1C 8B 4F 20 89 84 24 10 01 00 00 8B 47 24 89 84 24 18 01 00 00 39 5C 24 04", 3),
    'minimapPos (D8 05 x2)': ("D8 05 ? ? ? ? D9 5B 68 D8 05 ? ? ? ? D9 5B 6C D9 40 3C", 4),
    'MinimapHook1': ("8B 4D 08 D9 41 68 53 57 8B 5D 0C 8B F8", 3),
    'MinimapHook2': ("D9 40 68 8B 4D 0C D8 60 70 53 56 8D 91 83 00 00 00 D9 54 24 34", 3),
    'MinimapHook3': ("D9 40 1C 8B 4D 08 D9 40 20", 3),
    'LapsHook': ("D8 43 1C 8B 54 24 18 8B 44 24 10 D9 1A 8B 4E 68 50 51 57", 3),
    'FMV quad': ("68 00 00 80 3F 68 00 00 00 3F 68 00 00 00 3F 68 00 00 00 BF 68 00 00 00 BF", 6),
    'gameState (nGameState)': ("83 3D ? ? ? ? 03 75 ? 8B 0D", 1),
}

targets = set()
try:
    for f in json.load(open('work/catalog.json'))['functions']:
        for b in f['blocks']:
            targets.add(b[0])
except OSError:
    pass

for name, (p, n) in pats.items():
    hits = find(p)
    print(f"== {name}: {len(hits)} hit(s)")
    for h in hits:
        mark = '  [block start]' if h in targets else ''
        for i in dis(h, n):
            extra = ''
            m = re.search(r'\[(0x[0-9a-f]+)\]', i.op_str)
            if m and base <= int(m.group(1), 16) < base + len(img) - 4:
                a = int(m.group(1), 16)
                extra = f'   ; [{a:#x}] = {struct.unpack("<f", img[a - base:a - base + 4])[0]:g}'
            print(f"   {i.address:08X}: {i.mnemonic} {i.op_str}{extra}{mark}")
            mark = ''
