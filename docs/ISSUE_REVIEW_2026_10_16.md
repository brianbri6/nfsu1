# Android issue review, 16 October 2026

Reviewed the 38 available Underground 1 issues and the 662 available
Underground 2 issues, including comments. Local copies and downloaded logs
are kept outside Git. Many reports contain no problem description or game
log; a ZIP path on a reporter's phone is not an uploaded attachment.

## Underground 1

- [#33](https://github.com/codepdbh/nfsu1-recompiled/issues/33) and
  [#35](https://github.com/codepdbh/nfsu1-recompiled/issues/35): ARM64
  Build ID `90b109fa80a9d010bfcbe97bedc4ccfe2757fd05`, PC `0x39129f4`.
  The matching library resolves this to `sub_0059FB80`, the language-table
  binary search. It subtracts one from the entry count without checking
  whether the table is empty, then reads a computed address.
- [#36](https://github.com/codepdbh/nfsu1-recompiled/issues/36): same build,
  PC `0xb7e9f4`, `sub_00447C00`, reading a resource chunk header. The
  resource walk advances by each chunk's length without validating bounds.
- Added entry checks for these two functions, compiled into direct and
  indirect calls through `RECOMP_ENTER`. Valid inputs continue through the
original lifted function. Empty/unreadable language tables, incomplete
  chunk headers, oversized payloads and unmapped buffers stop with an
  explanatory runtime message rather than a native access violation.
  These checks do not reconstruct missing data or establish why a user's
  table was empty. Re-testing the affected copies is still necessary.
- Startup regression exercises malformed buffers and empty tables, plus
  present/missing keys through the original language search and guest ABI.
- [#22](https://github.com/codepdbh/nfsu1-recompiled/issues/22) attachment
  was downloaded. Its exit records show swipe/installation termination,
  not evidence of the same native crash. It is not counted as fixed.

## Shared graphics and diagnostics

The prepared DXVK source is shared with Underground 2. The old release
library's disassembly showed an unconditional call to `ClearContainer`
for every back-buffer entry despite an existing source preparation patch.
Regenerated the shared source, and added a preparation recipe hash checked
by CMake so a stale/unverified preparation fails the build explicitly.
The new library must be checked for the actual null guard before release.

Also guard destruction of absent Vulkan swapchain/surface handles, make
surface teardown idempotent, and test the actual prepared method bodies
with injected allocation failures. Stop logging the same module lookup
on every call: issue logs contain repeated `d3d9.dll`/`d3d9d.dll` lookups
that obscure diagnostics and add unnecessary work. Reports now include
the installed APK's ABI, independently of the phone's supported ABIs.

Both applications request `sensorLandscape` for launcher, GPU probe and
game, allowing either horizontal orientation while retaining landscape.

No issues are automatically closed and no reporter is told that an
unreproduced hardware/driver problem is resolved.

## Validation

Both architectures compile. On the connected Redmi Note 8, ARM64 passed
the startup asset regressions and reached a four-car race with profile
paulo. ARMv7 was installed and verified as `/system/bin/app_process32`;
its startup asset regressions and frame presentation also passed.
Repeated module load/lookup lines appear once per name in the new log.
This is not a test on the MediaTek models reporting the crashes.

The four APKs across both games have verified version codes/names,
original signing certificate, one matching ELF architecture per APK and
all three activity orientations set to sensorLandscape. Matching native
symbols were archived locally by APK Build ID for future crash analysis.

During Underground 2 testing, an old graphics cache triggered a native
presentation wait and a white screen; rebuilding the cache restored racing
and the user confirmed success. Both apps therefore use cache epoch
dxvk-v2 for this renderer update and offer a confirmed graphics-cache
reset in the launcher. Old caches are backed up, with no changes to game
data, profiles or settings. This does not assert the same fault occurred
in Underground 1.
