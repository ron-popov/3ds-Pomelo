# Pomelo — APT internals notes

## Build

To check that the project compiles, run `make 3dsx` from the repo root
(requires `DEVKITARM` to be set).

This file documents what has been verified (via Ghidra reverse-engineering of a
real Nintendo `ns` system module, cross-checked against a real-hardware IPC
packet capture of the stock Home Menu, and empirical on-device/Mikage
debugging) about how the 3DS APT service actually behaves. Pomelo runs as the
Home Menu (`APT:A`/host role), which exercises code paths regular guest apps
never touch, so `libctru`'s stock assumptions don't fully apply here — this is
why pomelo ships its own libctru fork
([libctru-for-homemenu](https://github.com/ron-popov/libctru-for-homemenu),
vendored as the `libctru` submodule).

For the same reason pomelo also ships a **citro3d fork**, vendored as the
`citro3d` submodule (see the 28-bit offset ceiling section below). Both forks
are wired up through `LIBDIRS` in the Makefile, and **their entries must stay
ahead of `$(CTRULIB)`** — devkitPro's libctru directory bundles its own
`libcitro3d.a` and `c3d/` headers, `LIBDIRS` is expanded into `-I`/`-L` flags in
order, and first match wins. Get the order wrong and the build silently links
stock citro3d, reintroducing the GPU hang below with no visible error.

Mikage's APT HLE is treated as behaviorally equivalent to real hardware for
this project — bugs reproduced under Mikage are assumed to reflect real APT
semantics, not emulator-specific divergence, unless proven otherwise.

## NS-side APT command IDs (verified against a real-hardware IPC capture)

Confirmed by decoding real Home Menu IPC headers (`cmd_id = header>>16`,
`normal_params = (header>>6)&0x3F`, `translate_params = header&0x3F`) and
matching them against the `online_ns_module` binary's `handle_ns_ipc` switch:

| cmd  | Name                          |
|------|-------------------------------|
| 0x02 | Initialize                    |
| 0x0E | GlanceParameter               |
| 0x15 | PrepareToStartApplication     |
| 0x1A | GetSharedFont                 |
| 0x1B | StartApplication               |
| 0x1C | WakeupApplication              |
| 0x43 | NotifyToWait                   |
| 0x4B | AppletUtility                  |

**Do not trust `0x1a == WakeupApplication`** — this was an initial
misattribution (based on imperfect memory of 3dbrew's command table) that got
corrected mid-session. `0x1a` is actually `GetSharedFont`; the real
`WakeupApplication` is `0x1c`.

## The transition-lock bitmask (`0x001211aa` in `online_ns_module`)

A 16-bit global flags word in NS is the actual mechanism behind the
`0xe0a0cc08` ("Invalid State") error from `APT_PrepareToStartApplication` /
`APT_WakeupApplication`. Each "role" (Application, various system applets)
claims a distinct bit. Five primitives operate on it:

- **`FUN_0010d4ac(bit)`** — bit-test: `flags==0 || (flags&bit)!=0`.
- **`FUN_0010e3ec(bit1, bit2)`** — **non-blocking conditional acquire**. Fails
  immediately (returns 0, and callers surface `0xe0a0cc08`) if
  `flags!=0 && (flags&bit1)==0 && (flags&bit2)==0`. This is what
  `APT_PrepareToStartApplication`, the real `APT_WakeupApplication` (both use
  bit `0x10`), and `APT_TryLockTransition` (`AppletUtility` id 6) all use.
  **There is no retry/wait built into this path** — if something else holds
  the lock at that exact instant, the call fails outright.
- **`FUN_0010a288(bit)`** — **blocking** acquire: loops, sleeping and
  retrying, until it can claim the bit. Used by `APT_LockTransition` when its
  `flag` argument is `false`. Distinct from the non-blocking primitive above.
- **`FUN_0010e468(bit)`** — unconditional exchange (steals the lock
  immediately, no waiting). Used by `APT_LockTransition` when `flag` is
  `true`, and internally by `PrepareToStartApplication`'s fast-path.
- **`FUN_0010f2c4(bit)`** — behind `APT_UnlockTransition(transition)`.
  **Only actually clears the flags word if `bit==0` (unconditional force-clear)
  or the currently-held flags already overlap `bit`.** Otherwise it's a
  **silent no-op that still returns success** — this is the crux of the whole
  double-launch bug (see below).

`AppletUtility`'s `id` dispatch (from decompiling `FUN_0010ee9c`):
`id=4` → SleepIfShellClosed (reads the flags word via `FUN_0010e4a0`, branches
on whether it's zero), `id=5` → LockTransition, `id=6` → TryLockTransition,
`id=7` → UnlockTransition. The function's switch always falls through to
`return 0;` — i.e. **every `AppletUtility` call, real result aside, replies
with the same fixed success framing at the NS/IPC-response level**; the
actual outcome must be inferred from the effect (e.g. via a follow-up
`TryLockTransition` probe), not assumed from the fact that the call
"succeeded".

Other confirmed globals: `g_abAPTState` (0x00120580, 96 bytes — per-session
state: `+0x0f` FSM state, `+0x1b` NotifyToWait flag, `+0x40` self appID,
`+0x44` NotifyToWait target appID, `+0x48` lock/arbiter handle),
`g_abAPTStateLock` (0x001205e0 — a `svcArbitrateAddress`-based mutex, *not* a
second state struct — this was an initial, disproven hypothesis),
`g_abAppletSlots` (0x001217f0, 160 bytes = 5×32 — per-category
Application/HomeMenu/etc. applet slot array).

## Real bugs found and fixed in `libctru-for-homemenu`'s `apt.c`

1. **`APT_AppletUtility` returned `cmdbuf[2]` instead of the real IPC
   `Result`.** Leftover from a refactor that merged a separate scalar `out`
   parameter (`*out = cmdbuf[2]`) and a static-buffer `buf2` parameter into a
   single `out`; the return statement was never updated to just `return ret;`
   (where `ret` — from `aptSendCommand` — already *is* the correct result).
   This meant **every** `AppletUtility`-based call
   (`SleepIfShellClosed`/`LockTransition`/`TryLockTransition`/`UnlockTransition`)
   was silently returning bogus success/failure codes, masking real failures
   from NS. Fixed: `return ret;`.
2. **`APT_TryLockTransition` had a pointer bug**: it passed `&succeeded`
   (address of the local `bool*` parameter, i.e. a `bool**`) and
   `sizeof(succeeded)` (pointer size) instead of `succeeded` (the caller's
   actual output buffer) and `sizeof(*succeeded)`. This corrupted both the
   output write and the IPC static-buffer size encoding. Fixed to pass
   `succeeded, sizeof(*succeeded)`.
3. **The actual root cause of "launch a game, close it, launch it again ⇒
   `0xe0a0cc08`"**: `aptWaitForWakeUp()` (`libctru/source/services/apt.c`)
   called `APT_UnlockTransition(0x10)` after a normal (non-JUMPTOHOME)
   wakeup. Per `FUN_0010f2c4` above, passing `0x10` only clears the lock if
   NS's flags word *already* holds exactly that bit — if something else
   (unidentified; plausibly a different role/bit briefly held during the
   exiting title's own teardown) holds a different bit at that moment, the
   unlock silently no-ops while still reporting success, and the next
   `PrepareToStartApplication`'s non-blocking acquire of bit `0x10` fails
   immediately. Fixed by passing `0` instead — `APT_UnlockTransition(0)` is
   NS's designated **unconditional** force-clear, which is the correct thing
   for the Home Menu (which has unquestioned authority over transition state
   once a title has exited) to call. Confirmed fixed on both Mikage and real
   hardware.

Diagnosing bug 3 required temporarily patching in extra `_aptDebug(...)`
probes (`APT_UnlockTransition`'s and a diagnostic `APT_TryLockTransition`'s
real results) gated behind `#ifdef LIBCTRU_APT_DEBUG` — this pattern (patch a
few `_aptDebug` calls in, rebuild, ask for a fresh `pomelo_debug.log`, remove
them once root-caused) is the effective way to debug this stack, since there's
no interactive debugger available on-device or in Mikage.

## The citro3d 28-bit vertex-buffer offset ceiling (fixed in the citro3d fork)

Symptom: after switching `MemoryType` from `Application` to `System` in
`source/template.rsf`, the bottom screen went blank and the console hung on the
**very first rendered frame**. GSP accepted the `ProcessCommandList` GX command
and programmed the PICA's `command_processor_config` registers normally, but
the GPU never finished and never raised the `P3D` interrupt, so
`gxCmdQueueWait` blocked forever. `MemoryFill` (`PSC0`) kept completing fine
throughout, which is what proves the GPU and GSP's interrupt relay were both
alive — only command-list execution was stuck.

**Root cause: citro3d does not give the GPU absolute vertex addresses.**
`GPUREG_ATTRIBBUFFERS_LOC` (0x0200) holds a base as `paddr >> 3`, and every
vertex buffer (`0x0203`+) and the index buffer (`GPUREG_INDEXBUFFER_CONFIG`,
0x0227) is expressed as a **byte offset from that base**. The hardware's offset
field is 28 bits, so the reachable window is always
`[base, base + 0x10000000)`. Upstream citro3d hardcodes
`BUFFER_BASE_PADDR 0x18000000` (VRAM's base) — which on an Old 3DS spans VRAM
through the end of FCRAM at `0x28000000`, i.e. exactly all GPU-addressable
memory. On a New 3DS, FCRAM runs to `0x30000000` and that no longer holds.

Under `MemoryType: System` pomelo's linear heap landed at physical
`0x27f32000`–`0x28b32000`, straddling the `0x28000000` ceiling with only ~824KB
below it, so essentially every citro2d allocation overflowed the field. The
truncated offset pointed the vertex fetcher back inside VRAM (at the
framebuffer), and the GPU stalled on the malformed primitive.

Fixed by rebasing to `0x20000000` (FCRAM's base) in the fork's
`citro3d/source/buffers.c`, sliding the window to `0x20000000`–`0x30000000`.
**Note this is a forced choice, not a clean fix:** `0x30000000 - 0x18000000` is
384MB, wider than a 28-bit field, so no single base can cover both VRAM and all
of New 3DS FCRAM. The cost is that vertex/index buffers in VRAM are now
rejected by the guards in `BufInfo_Add`/`C3D_DrawElements`; nothing in pomelo
puts them there, and framebuffers and textures are unaffected because they use
**absolute** address registers (`COLORBUFFER_LOC`, `TEXUNIT0_ADDR1`, …) rather
than this offset scheme.

Two things worth remembering, since both produced misleading evidence while
diagnosing this:

- **Only vertex and index data go through the base+offset scheme.** Textures,
  colour/depth buffers, and the command list itself are all addressed
  absolutely. A capture showing the real Home Menu writing physical
  `0x28005aa0` into `command_processor_config` (0x18E8) says nothing about this
  bug — different register, different addressing mode — and briefly led to the
  wrong conclusion that the GPU simply could not read N3DS extended FCRAM. It
  can.
- **The real Home Menu solves this properly and does not hit the ceiling.**
  Decompiling `FUN_0002fad0` in the Home Menu binary (`myconsole_homemenu`)
  shows it recomputes the base dynamically: `0x000303e8` does
  `biclt r3, r2, #0xf` — base = `min(vertex buffer addresses)` aligned down to
  16 bytes, tracked at `[r5,#0x6a0]` — then rebases every stored offset
  (`0x30450`) and only re-emits `ATTRIBBUFFERS_LOC` when the base actually
  changed. That Nintendo bothers to track a running minimum and rebase is
  independent confirmation that the offset field really is narrow. Porting that
  adaptive-base approach is the general fix if VRAM vertex buffers are ever
  needed alongside high FCRAM.

The most effective way to debug this class of problem is to decode the command
list before submitting it — patch a decoder into `GX_ProcessCommandList` in the
libctru fork (`libctru/source/gpu/gx.c`) that walks the buffer and logs the
address-bearing registers. Command format, per `GPUCMD_AddInternal` in
`libctru/source/gpu/gpu.c`: `[param0][header]` plus any extra params, padded to
an even word count, where the header packs the register in bits 0-9, a byte
mask in 16-19, the count of *extra* params in 20-27, and a "consecutive
registers" flag in bit 31. **That bit-31 flag matters** — citro3d emits texture
and framebuffer addresses via `GPUCMD_AddIncrementalWrites`, so they arrive in
*extra* params landing at `reg+j`, never in `param0`; a decoder that only
inspects `param0` will miss every address in the list.

## Hazard: never trigger IPC between `svcSendSyncRequest` and reading its response

`aptSendCommand()` uses `getThreadCommandBuffer()` — a **single, shared
per-thread scratch region** reused by *every* synchronous IPC call issued by
that thread, including unrelated ones (e.g. filesystem writes for logging).
Calling anything that itself does IPC (including `log_debug`, if it flushes to
the SD card) **in between** `memcpy`-ing a request into that buffer and
`svcSendSyncRequest`, or between `svcSendSyncRequest` returning and
`memcpy`-ing the response back out, clobbers the in-flight
request/response before it's consumed. This was hit directly: adding a debug
log call right after `svcSendSyncRequest` but before the response was copied
out corrupted `APT_Initialize`'s handle-bearing response and made pomelo fail
to boot, on both Mikage and real hardware, with no further log output (the
corruption cascaded into undefined behavior before anything else could log).
Any future instrumentation of `aptSendCommand` (or similar raw-IPC wrappers)
must keep all logging strictly outside that window.

## Known desync: Home-button pause vs. real exit (unresolved, deliberately not "fixed")

`aptWaitForWakeUp` returns different `APT_Command` values depending on why the
app woke up — notably `APTCMD_WAKEUP_EXIT` (10, a title actually closed) vs.
`APTCMD_WAKEUP_PAUSE` (11, the title was merely suspended via a Home-button
press). Pomelo's `STATE_BACKGROUND` handler (`source/main.c`) currently
**does not distinguish these** — any wakeup is treated as "the title exited"
and pomelo tears down and reconstructs its own menu. This is intentional per
the actual pomelo↔game contract: **the launched title is expected to
terminate itself in response to the Home-button press**, at which point the
wakeup pomelo receives corresponds to a real exit even though the *first*
notification NS delivers for a Home-button press is nominally
`WAKEUP_PAUSE`-class. Do not "fix" this by making pomelo special-case
`APTCMD_WAKEUP_PAUSE` into a no-op/stay-in-background — that was tried and
reverted, because it broke the normal case (a game that promptly closes
itself). If a title *doesn't* close itself promptly after Home is pressed,
pomelo can still observe `APT_IsRegistered(APPID_APPLICATION)` staying `true`
after reconstructing its menu ("Previous app is still running" in the log) —
this is a symptom of the title's own shutdown being slow/stuck, not a pomelo
bug per se.

There is a real, still-unresolved hang report in this vein: on real hardware,
after a Home-button press left a title in this semi-stuck "still registered"
state and the user repeatedly retried launching (mashing A) and pressed Home
again, a **third** APT notification event (very likely the physical Power
button) caused the `aptEventHandler` thread to hang inside the blocking
`APT_InquireNotification` call — before it ever reached the code that would
call `PTMSYSM_ShutdownAsync`. Since Power-button delivery on 3DS has no path
other than this same APT notification channel, once stuck there the only
recovery is a hard power-off. The likely trigger is downstream fallout from
the same title-still-registered desync above, but this has not been proven —
next time it's reproduced, instrument `aptSendCommand` (see the hazard note
above for how to do this safely) to determine whether the hang is local lock
contention (`aptLockHandle` held by another in-flight call on the main
thread) or NS itself going unresponsive to pomelo's session.
