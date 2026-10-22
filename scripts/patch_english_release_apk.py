#!/usr/bin/env python3
"""Patch the upstream v0.2.5 Android APK in place via apktool.

Uses the original and English-translated source code to derive a safe literal
translation table, then patches smali UI text plus size-bounded native messages.
Game data, identifiers and Android package name are left unchanged.
"""
import argparse
import difflib
import json
import re
import subprocess
from pathlib import Path

BASE = "58d9d4f63d59c941031f91973ae48c2cdb1a42b1"
JAVA = "android/app/src/main/java/com/nfsu1/androidevolved/"
QUOTED = re.compile(r'"(?:\\.|[^"\\])*"')
SMALI = re.compile(r'(?m)^(?P<head>\s*const-string(?:/jumbo)?\s+[vp]\d+,\s*)"(?P<value>(?:\\.|[^"\\])*)"')
IMMUTABLE = {"Spanish", "English", "French", "German", "Italian", "Dutch", "Swedish", "Korean", "Chinese"}
NATIVE = {
 "Arranque CRT pendiente": "CRT startup pending",
 "Ejecutando entrada CRT del juego…": "Running game CRT...",
 "No se pudo cargar el driver Vulkan ": "Vulkan driver failed: ",
 ". Elige el driver del sistema en el launcher.": ". Select the system driver in launcher.",
 "Datos accesibles: memoria interna/nfsu1\nVulkan 1.1: presentación activa\n": "Game files: internal storage/nfsu1\nVulkan 1.1: active\n",
 "No se pudo abrir el driver seleccionado": "Could not open selected driver",
 "El paquete no ofrece vkGetInstanceProcAddr": "Missing vkGetInstanceProcAddr",
 "El paquete no ofrece vkCreateInstance": "Missing vkCreateInstance",
 "Driver con funciones Vulkan incompletas": "Vulkan driver missing functions",
 "No se encontró una GPU Vulkan": "No Vulkan GPU found",
 "Datos del juego invalidos: bloque de recursos fuera de memoria; revisa tu copia de NFS Underground 1": "Invalid game data: resource outside memory; check NFS Underground 1",
 "Datos del juego truncados: cabecera de recurso incompleta en ": "Truncated game data: bad resource header at ",
 "Datos del juego truncados: recurso ": "Truncated game data: resource ",
 " excede su bloque; revisa los archivos de tu copia de PC": " exceeds its block; check your PC game files",
 "Tabla de idioma vacia o invalida: revisa LANGUAGES y selecciona un idioma disponible de tu copia de NFS Underground 1": "Invalid language table: check LANGUAGES and your NFS Underground 1 files",
}
def translations():
    mapping = {}
    files = subprocess.check_output(
        ["git", "diff", "--name-only", BASE, "HEAD", "--", JAVA], text=True
    ).splitlines()
    for file in files:
        if not file.endswith(".java"):
            continue
        old = subprocess.check_output(["git", "show", BASE + ":" + file], text=True).splitlines()
        new = Path(file).read_text(encoding="utf-8").splitlines()
        for op, a, b, c, d in difflib.SequenceMatcher(
            None, old, new, autojunk=False
        ).get_opcodes():
            if op != "replace" or b - a != d - c:
                continue
            for x, y in zip(old[a:b], new[c:d]):
                before, after = QUOTED.findall(x), QUOTED.findall(y)
                if len(before) != len(after) or QUOTED.sub('""', x) != QUOTED.sub('""', y):
                    continue
                for s, t in zip(before, after):
                    s, t = s[1:-1], t[1:-1]
                    if s == t or s in IMMUTABLE:
                        continue
                    if s in mapping and mapping[s] != t:
                        raise RuntimeError("Conflicting translation " + repr(s))
                    mapping[s] = t
    mapping["Español"] = "Spanish"
    mapping["Ejecutando entrada CRT"] = "Running game CRT"
    return mapping

def patch_smali(root, mapping):
    used = set()
    changes = 0
    files = 0
    found_language = False
    for path in root.glob("smali*/**/*.smali"):
        source = path.read_text(encoding="utf-8")
        def replace(m):
            nonlocal changes
            value = m["value"]
            if value not in mapping:
                return m.group(0)
            changes += 1
            used.add(value)
            return m["head"] + '"' + mapping[value] + '"'
        result = SMALI.sub(replace, source)
        if path.name == "GameOptions.smali":
            method = re.compile(
                r"(?ms)^\.method\s+[^\n]*\slanguage\(Landroid/content/Context;\)Ljava/lang/String;\n.*?^\.end method"
            )
            def change_default(m):
                segment, count = re.subn(
                    r'(\bconst-string(?:/jumbo)?\s+[vp]\d+,\s*)"Spanish"',
                    r'\1"English"', m.group(0), count=1
                )
                if count != 1:
                    raise RuntimeError("Default game language not found")
                return segment
            result, count = method.subn(change_default, result, count=1)
            found_language = count == 1
        if result != source:
            path.write_text(result, encoding="utf-8")
            files += 1
    if not found_language:
        raise RuntimeError("GameOptions.language not found in APK")
    if changes < 65:
        raise RuntimeError("Only %s UI strings patched; rejecting incomplete translation" % changes)
    print("Translated %d literal occurrences in %d smali files; %d distinct translations" %
          (changes, files, len(used)))
    print("Source strings not present in DEX:", json.dumps(sorted(set(mapping) - used), ensure_ascii=False))

def patch_native(root):
    count = 0
    for path in root.glob("lib/**/*.so"):
        original = path.read_bytes()
        data = original
        for spanish, english in NATIVE.items():
            a, b = spanish.encode("utf-8"), english.encode("utf-8")
            if len(b) > len(a):
                raise RuntimeError("Native replacement is too long: " + spanish)
            if a in data:
                count += data.count(a)
                data = data.replace(a, b + b"\x00" * (len(a) - len(b)))
        if data != original:
            path.write_bytes(data)
            print("Translated native messages in", path)
    print("Native message replacements:", count)

def patch_version(root):
    yml = root / "apktool.yml"
    content = yml.read_text(encoding="utf-8")
    content, n1 = re.subn(r"(?m)^(\s*versionCode:\s*).+$", r"\g<1>'8'", content, count=1)
    content, n2 = re.subn(r"(?m)^(\s*versionName:\s*).+$", r"\g<1>0.2.6", content, count=1)
    if n1 != 1 or n2 != 1:
        raise RuntimeError("Cannot update APK version metadata")
    yml.write_text(content, encoding="utf-8")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("apktool_directory", type=Path)
    args = parser.parse_args()
    table = translations()
    print("Derived UI translation entries:", len(table))
    patch_smali(args.apktool_directory, table)
    patch_native(args.apktool_directory)
    patch_version(args.apktool_directory)

if __name__ == "__main__":
    main()
