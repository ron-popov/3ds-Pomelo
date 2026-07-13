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

## Real bug found and fixed in `source/template.rsf`: `MemoryType: Application`

Launching memory-hungry titles (confirmed with `super_mario_3d_land_us`) crashed
a bit after wakeup, with the title itself calling `svcBreak(USERBREAK_PANIC)`
from inside its own resource allocator's out-of-memory handler (a
Nintendo-SDK-style bidirectional frame-heap allocator, hit while it was loading
early-boot/intro assets). Mikage treats a guest kernel panic as fatal to the
whole emulation session, which is why this looked like "Mikage crashes a bit
after the game is woken up" rather than a guest-process-local failure. The
title booted fine under the stock Home Menu with the same Mikage build, which
ruled out an emulator-side or console-model (SystemMode/SystemModeExt)
explanation and pointed at something pomelo itself does differently.

Root cause: `source/template.rsf` (which `makerom -f cxi` turns into
`pomelo.cxi`'s exheader — the `cxi`/`install_mikage`/`install_sdcard` Makefile
targets all consume it) set `MemoryType: Application`. `MemoryType` selects
which FCRAM partition a process's own memory is drawn from; `Application` put
pomelo's own resident heap/texture/GX allocations in the **same** partition the
launched title needs, instead of the isolated `System` partition. A real dump
of the stock Home Menu (`title_dumps/myconsole_0004003000008F02_homemenu`,
same UniqueId `0x008f` pomelo's RSF deliberately mirrors) confirms it uses
`Memory Type: SYSTEM`. Fixed by changing pomelo's RSF to match:
`MemoryType: System`.

`git blame` traces the wrong value back to the "Finally a template.rsf that
runs on real hardware" commit, which bundled it together with several other,
actually-necessary fixes (`Priority: 16`, the `Dependency` list,
`IoAccessControl` entries) needed to get NS to accept the title at all —
`MemoryType: Application` was very likely leftover shotgun-debugging cruft
from that session that was never revisited, not something load-bearing.

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
