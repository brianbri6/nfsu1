# Auditoría: qué reutiliza Underground 1 de Underground 2

Fecha: 2026-10-13. Fuente: el árbol real de `nfsu2-recompiled/` (commit `9e202a7`) y `pcrecomp/` con `patches/pcrecomp` 0001–0014.

Regla: el port de Underground 2 no se modifica para este proyecto. Lo que Underground 1 necesita de él se **copia** a `nfsu1-recompiled/` (y se marca como candidato a módulo compartido, fase 6) o se usa **sin cambios** desde `pcrecomp/`.

## Recompilador

| Componente | Estado | Para NFSU1 |
|---|---|---|
| `pcrecomp/tools` (disasm32, lift32, pe_analyze) + parches 0001–0014 | genérico | **se usa tal cual** (mismo checkout parcheado) |
| `pcrecomp/tools/cpp/vtable_scan.py` | genérico | semillas: `Speed.exe` no tiene RTTI ni reubicaciones |
| `scripts/run_lift.py` | genérico salvo que lee `config/` de su propio repo | **copiado** a `nfsu1-recompiled/scripts` (su `config/` es el de NFSU1) |
| `scripts/prepare_module.py` | genérico | copiado (DLL del juego, si las hubiera) |
| `scripts/normalize_exe.py` | específico de SPEED2 (reubicaciones ocultas en una sección sin nombre) | no aplica: `Speed.exe` no conserva reubicaciones |
| `config/overrides.txt`, `exclude.txt`, `hooks.txt` | direcciones de SPEED2 | **nuevos y vacíos** en `nfsu1-recompiled/config` |

## Host de Windows (`src/`, MSVC x86 + `native32`)

| Archivo | Dependencia de NFSU2 | Para NFSU1 |
|---|---|---|
| `src/host/main.c` | nombre `SPEED2.EXE`, carpeta, `GUEST_SPAN 0x532000`, variables `NFSU2_*`, widescreen | adaptar (copia): `Speed.exe`, span 0x395000 |
| `src/host/profiler.c` | ninguna | copia sin cambios |
| `src/shims/win32_shims.c` | ninguna (recursos, ventana) | copia, revisar las importaciones nuevas |
| `src/runtime/d3d9_trace.c` | ninguna (ganchos de vtable de D3D9 para modo ventana) | copia sin cambios |
| `src/runtime/dinput_trace.c` | dos direcciones de depuración de SPEED2 (0x874C40, 0x865980) | copia sin esas direcciones |
| `src/runtime/nfs_runtime.c` | rango de código 0x401000–0x783000 en el informe de cierre | adaptar: `.text` de Speed.exe acaba en 0x697000 |
| `src/runtime/native_range.c` | ninguna | copia sin cambios |
| `src/runtime/overrides.c` | direcciones de SPEED2 (Cyrix, memmove de la CRT) | **reimplementar**: localizar las mismas rutinas en Speed.exe por bytes |
| `src/runtime/widescreen.c` | todo específico (parches del WidescreenFix de NFSU2) | **reimplementar** con los patrones de NFSU1 |
| `src/platform/nfs_log.c` | ninguna | copia sin cambios |

## Runtime de Android (`android/app/src/main/cpp`)

| Archivo | Dependencia de NFSU2 | Para NFSU1 |
|---|---|---|
| `guest_memory.h`, `guest_heap.h` | arenas fijas (encajan: Speed.exe acaba en 0x794368, < 0x01000000) | reutilizable |
| `guest_threads.inc`, `guest_kernel.inc`, `guest_files.inc`, `guest_window.inc`, `guest_network.inc`, `guest_audio.inc`, `guest_input.inc` | HLE de Win32 por nombre de función, sin direcciones | reutilizable (+ ~20 importaciones nuevas) |
| `guest_runtime.cpp` | **sí**: comprobación `imageSize 0x532000 / entry 0x35bcc7`, etiquetas de resolución en 0x78fa1c…, `0x870d1c`, `0x8b446c`, `0x725520`, `0x800538` | separar en un "perfil de juego" (direcciones por juego) |
| `d3d9_bridge.cpp` + `.inc` | ninguna (puente COM D3D9 → DXVK) | reutilizable: NFSU1 también usa `d3d9.dll` |
| `guest_modules.inc` | `server.dll` de NFSU2 | perfil por juego |
| `main.cpp`, `vulkan_driver.cpp`, `gpu_probe.cpp`, `gamepad_input.h`, `vertex_formats.h` | ninguna | reutilizables |
| `ports/dxvk_android` (`prepare.py`) | ninguna | reutilizable tal cual |
| Java (launcher, controles, opciones, informes, actualizaciones) | paquete `com.nfsu2.androidevolved`, textos, repositorio de GitHub | copia con paquete propio (`com.nfsu1.androidevolved`) |

## Conclusión

- **Se comparte sin cambios**: pcrecomp parcheado, DXVK Android, el puente D3D9, el HLE de Win32 por nombre, Vulkan/gamepad/probe.
- **Necesita adaptador ("perfil de juego")**: nombre y tamaño del ejecutable, direcciones de ganchos (resolución, widescreen, overrides), DLL del juego, identidad del APK.
- **Se reimplementa**: widescreen y overrides de NFSU1, buscando en `Speed.exe` las mismas rutinas por patrones de bytes, nunca por direcciones de NFSU2.
