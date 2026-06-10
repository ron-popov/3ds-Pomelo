# Pomelo - 3DS Custom Home Menu Architecture & Documentation

## 1. Project Overview
**Pomelo** is a custom Home Menu replacement for the Nintendo 3DS (Horizon OS).
* **Target Title ID:** `0004003000008F02` (Overriding the official Nintendo Home Menu).
* **Environment:** Bare-metal Horizon OS (`.cia` package installed to CTRNAND).
* **Privilege Level:** `APT:A` (Applet Privilege) — Pomelo acts as the **Host**, managing Library Applets and standard Guest Applications (`APT:U`), rather than functioning as a standard game.

---

## 2. APT (Applet Notification) Architecture
Because Pomelo is a system shell, it cannot rely on the default `libctru` `apt.c` implementation blindly. Default `libctru` assumes the app is a **Guest** and will deadlock the GPU or crash the console if it receives applet transition signals meant for the Host.

### The Applet Lifecycle Hook (`aptHook`)
Pomelo intercepts APT signals *before* they hit `aptMainLoop`'s default handlers.
* **Suspend/Restore:** Handled dynamically. If a Library Applet (like Settings or Keyboard) is launched, Pomelo yields the GPU and audio hardware. When the applet closes, Pomelo reacquires them and redraws its UI.
* **Applet Independence:** To prevent `libctru` from killing the app gracefully, `aptMainLoop()` is kept inside an infinite `while(true)` loop. The kernel is never allowed to cleanly exit the `main()` function.

---

## 3. Hardware Event Handling (Bare Metal vs. Emulation)

### The Power Button Shutdown Sequence
Handling the physical Power Button requires distinct logic because real hardware behaves differently than the Mikage emulator.
* **Mikage Behavior:** Emulators fake shutdowns by broadcasting `APTSIGNAL_SHUTDOWN` (`7`) directly to the app.
* **Bare Metal Behavior:** The physical MCU negotiates with the `ns` (Nintendo Shell) sysmodule. `ns` sends an **IPC Message (Parameter)** to the Home Menu (Message ID `0x0C`, Sender ID `0x0100` / `APPID_NS`).
* **Pomelo's Solution:** Pomelo logs and monitors `APT_ReceiveParameter`. When it detects the `0x0C` IPC message from `APPID_NS` containing the power button payload, it intercepts it, draws any necessary UI, and manually triggers `PTMSYSM_ShutdownAsync(0)`.
* **The Dead Loop:** After calling `PTMSYSM_ShutdownAsync(0)`, the thread **must** enter a dead loop (`while(1) { gspWaitForVBlank(); }`). It takes the MCU a few hundred milliseconds to physically sever the voltage rails via I2C; returning early will cause a fatal kernel panic before power loss.

### Sleep Mode (The Lid)
Triggered by `APTSIGNAL_SLEEP_QUERY`. Pomelo must respond via `APT_ReplySleepQuery()`. On `APTSIGNAL_SLEEP_ENTER`, Pomelo halts UI/Audio threads using `LightEvent_Clear()` so the MCU can lower CPU voltage.

---

## 4. Threading & Synchronization
Pomelo uses two distinct synchronization primitives depending on the boundary:

* **Intra-Process (Thread-to-Thread):** `LightEvent`
  * Exists in user-mode RAM. Extremely fast. Used to pause Pomelo's internal audio and rendering threads when the OS sleeps.
* **Inter-Process (Process-to-Sysmodule):** `Kernel Events` & `IPC`
  * Exists in protected Kernel memory (`0xFFF...`). Managed via `Handle`. Required to communicate with background sysmodules (e.g., launching applets, talking to PTM).

---

## 5. Memory Management & Stack Safety
**CRITICAL: Stack Overflow Prevention**
The default `libctru` APT background thread (`aptEventHandlerThread`) only has a **4KB stack** (`0x1000`). 
* Using `<stdio.h>` formatters like `vsnprintf` inside this thread (e.g., for logging APT signals) will cause a massive stack allocation (2.5KB+), instantly overflowing the thread boundary and causing a hardware data abort.
* **Solution:** 1. Large buffer allocations inside functions have been moved to the **Heap** (`malloc`).
  2. Heavy string formatting for logs is offloaded to the main thread (which has a 256KB / `0x40000` stack), or utilizes lightweight embedded formatters (`stb_sprintf`).
  3. Stack usage can be monitored live by capturing the ARM Stack Pointer (`__builtin_frame_address(0)` or via a `constructor` function reading the `sp` register).

---

## 6. Build Configuration (`template.rsf`)
To build Pomelo as a root system shell, the RSF configuration mandates high-level kernel access.

* **Category:** `SystemApplication`
* **Target:** Internal NAND (`UseOnSD: false`)
* **HandleTableSize:** `0x200` (512 simultaneous handles allowed).
* **ServiceAccessControl:** Grants access to `apt:a` (Applet mode), `ptm:sysm` (Hardware power control), `gsp::Gpu`, `hid:USER`, etc.

### Sysmodule Dependencies
Pomelo explicitly declares 29 Sysmodule dependencies in its RSF. This guarantees the Horizon Kernel loads core services (like `mcu`, `am`, `ptm`, `ns`, `gsp`) into memory *before* booting Pomelo. 
*(Refer to local template.rsf for the full 0x00040130... mapping).*

---

## 7. Assets & UI
* **Icon / Title Metadata:** Pomelo uses the `.smdh` format for application metadata (Short Title, Long Title, Publisher).
* **Generation:** Handled automatically in the Makefile using `bannertool makesmdh` or `smdhtool`.
* **Resolution:** Requires a 48x48 pixel PNG icon, which the tool automatically packs alongside a 24x24 downscaled version.
