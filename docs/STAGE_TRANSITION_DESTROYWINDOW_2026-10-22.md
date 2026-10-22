# NFSU1 Android: race/stage transition freeze investigation (2026-10-22)

## Reproduction evidence

The upstream v0.2.5 release still lists between-race audio carry-over and Underground
story progression failures as open issues. In upstream
[issue #102](https://github.com/codepdbh/nfsu1-recompiled/issues/102),
a post-race/audio-loop report contains:

```
Guest integration boundary: HLE pendiente: user32.dll!DestroyWindow desde 0x00411830
```

This is a concrete missing Win32 host call in the 0.2.5 runtime. The original
`windowFunctions` mapping did not include `DestroyWindow`; `resolveFunction`
therefore dispatched it to `unsupported()`, throwing `GuestStop` and halting
the guest thread. The native AAudio output can continue playing its loop
after the guest stops updating DirectSound buffers.

There is not yet a device log proving that the user's reported second-stage
freeze has exactly this failure signature. It may also be related to upstream
[#53](https://github.com/codepdbh/nfsu1-recompiled/issues/53) (audio between
races) or [#42](https://github.com/codepdbh/nfsu1-recompiled/issues/42)
(Underground story transition/cutscenes).

## Source-side fix

- `guest_window_destroy.h`: recursive window teardown with re-entry guard;
  WM_DESTROY, child destruction, WM_NCDESTROY, cleanup.
- `guest_window.inc`: native implementation of `DestroyWindow(HWND)`,
  including invalid-window handling, focus/capture cleanup, paint contexts
  and queued window-message cleanup.
- `guest_kernel.inc`: bind `user32.dll!DestroyWindow`.
- `tests/guest_window_destroy.cpp`: standalone test for recursion, message
  order, invalid handles, and re-entrant calls.
- `.github/workflows/guest-window-tests.yml`: compiles and exercises the
  regression test on Ubuntu.

### Unit test

```bash
c++ -std=c++17 -Wall -Wextra -Werror -O2 tests/guest_window_destroy.cpp -o /tmp/guest_window_destroy
/tmp/guest_window_destroy
```

## Packaging status and important limitations

**The v0.2.7 English APK does not include this runtime fix.** Patching Java/smali
text cannot add a C++ Win32 import implementation to an existing compiled
ELF. The native game library must be rebuilt with the generated lifted game
source and its dependencies. The public repo does not contain `src/recomp/gen`
or `src/recomp/gen_abk`; they are intentionally generated locally from
the user's original PC game copy.

For a real phone validation, build `libnfsu1_android.so` from the updated
source, package the APK, sign it with a *persistent* local key, and test going
from the end of a race through loading the next stage. Inspect the diagnostics
for `DestroyWindow`, `HLE pendiente`, AAudio underruns and last D3D9 Present.

The change is a targeted hypothesis supported by an upstream error log, not a
verified fix to all post-race audio or loading hangs.
