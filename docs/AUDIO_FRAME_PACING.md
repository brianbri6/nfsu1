# Audio and frame pacing follow-up (2026-10-15)

Tester reports affect all three PC ports, including Dimensity 9300/8400 and
Snapdragon 675. Those models have not been tested locally; the connected
Redmi Note 8 is a separate device. Do not treat these changes as a confirmed
fix for every reported audio or frame-rate problem.

The frame limiter previously slept while owning the serialized guest machine.
It now saves the current thread context, releases the machine until its frame
deadline, then reacquires it and restores the context. Guest audio, streaming
and network workers can progress during that idle time. The startup scheduler
regression exercises timed handovers alongside ordinary yields and checks
integer, FS, x87, XMM, TLS and last-error isolation.

AAudio requests low-latency shared output at the device-selected sample rate.
The initial buffer is two hardware bursts. After observed output underruns,
the guest thread grows the buffer one burst at a time, up to eight bursts and
never beyond the stream capacity. Devices may decline the low-latency request.
Audio health logs report actual performance mode, rate, burst, buffer size,
underrun count and peak callback duration every 120 successful game Presents.
No logging or buffer resizing was added inside the audio callback.

The mixer still uses a mutex while rendering and publishing PCM. Callback
starvation, stale guest streaming data and graphics stalls remain distinct
possible causes; the new diagnostics help distinguish them. Stream recovery
following audio-device disconnects is not implemented by this change.

Build validation is recorded in work/audio-pacing-build.log (ignored).
Physical testing of this final audio revision on the affected tester devices
is pending. No release assets are replaced by this source commit.

AAudio buffer tuning reference:
https://developer.android.com/ndk/guides/audio/aaudio/aaudio
