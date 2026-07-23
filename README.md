# OpenRGB plugin - Keychron V6 Ultra 8K (custom ZMK firmware)

Adds the **Keychron V6 Ultra 8K** to OpenRGB as a directly-controllable per-key
RGB device. Requires the keyboard to be running the **custom ZMK firmware** with
OpenRGB direct-control support (ZMK issue #893) - stock Keychron firmware will
not respond and the device won't appear.

> **Personal project, shared as-is.** I built this for my own keyboard and put
> it on GitHub in case it's useful to someone else. No support, warranty, or
> maintenance is promised - use it at your own risk.

Target: **OpenRGB 1.0rc3**, plugin API v4. Prebuilt for **Windows** (x64/x86); on **Linux**, build from source (below).

> Companion firmware: [naaraxi/zmk (branch `openrgb`)](https://github.com/naaraxi/zmk/tree/openrgb) - the custom ZMK build this plugin talks to, plus the host flasher.

## How it works
The custom firmware exposes an `id_openrgb` (0x16) command on its raw-HID
channel (USB usage page `0xFF60`). This plugin enumerates `3434:0C60`, verifies
it's the custom firmware (`GET_LED_COUNT` → 108), and registers an
`RGBController` with a **Direct** mode. Selecting Direct hands the keyboard to
OpenRGB; a background keepalive re-arms the firmware's 3-second auto-hand-back
watchdog so colors persist, and control returns to your onboard/Launcher
lighting when OpenRGB exits or the keyboard goes wireless (USB-only).

Caps/Num-Lock indicators still overlay on top (handled in firmware).

## Install (Windows)
1. Download the DLL for your OpenRGB build from the [latest Release](../../releases/latest)
   - `OpenRGBKeychronV6UltraPlugin-x64.dll` (64-bit OpenRGB, the usual case) or
   `-x86.dll` (32-bit). Every push also uploads them as Actions artifacts.
2. In OpenRGB, open **Settings -> Plugins**, click **Install Plugin**, and select
   the downloaded DLL - OpenRGB copies it into place for you. (Or drop it into
   `%APPDATA%\OpenRGB\plugins\` by hand.)
3. The keyboard appears under Devices in **Direct** mode, ready to control -
   no restart needed (if it doesn't show up, restart OpenRGB).

## Build from source (Linux)
No prebuilt Linux binary is shipped - build the `.so` yourself (quick):

```bash
# 1. build deps (Debian/Ubuntu; on Arch use: qt5-base hidapi)
sudo apt-get install -y build-essential pkg-config qtbase5-dev qtbase5-dev-tools libhidapi-dev

# 2. get this plugin's source and enter it
git clone https://github.com/naaraxi/keychron_v6u_openrgb.git
cd keychron_v6u_openrgb

# 3. clone the OpenRGB source it builds against, into an OpenRGB/ subfolder
#    (OpenRGB ships no standalone SDK; plugins compile against its source - this
#     is exactly what the CI does. --depth 1 keeps it shallow.)
git clone --depth 1 --branch release_candidate_1.0rc3 https://gitlab.com/CalcProgrammer1/OpenRGB.git OpenRGB

# 4. build - stay in the plugin dir; OpenRGB/ is just a subfolder it compiles against
qmake OpenRGBKeychronV6UltraPlugin.pro CONFIG+=release
make -j"$(nproc)"
```

Then load `libOpenRGBKeychronV6UltraPlugin.so` via OpenRGB's **Settings -> Plugins
-> Install Plugin** (or copy it into `~/.config/OpenRGB/plugins/` by hand).

The keyboard appears under Devices in **Direct** mode, ready to control (restart
OpenRGB if it doesn't show up). Notes:
- Build against the **same Qt major version** your OpenRGB uses - Qt5 for 1.0rc3
  and most distro packages; a Qt6 OpenRGB needs a Qt6 build. A mismatch just
  won't load.
- If you don't run OpenRGB as root, you need a udev rule granting access to the
  keyboard's hidraw node (OpenRGB's own device rules cover it).

## Build (Windows)
CI (`.github/workflows/build-windows.yml`) builds x64 + x86 on every push: Qt
**5.15.0** (`win64_msvc2019_64` / `win32_msvc2019`), `qmake` + `nmake`, checking
out OpenRGB at `release_candidate_1.0rc3` into `./OpenRGB`. The Qt patch version
must be **<=** the host OpenRGB's (1.0rc3 ships 5.15.0), or the plugin faults
inside `Qt5Core` on load. Local build: same steps with Qt 5.15 + MSVC.

## Known issues
This plugin is still young and isn't 100% compatible with everything OpenRGB
does. Per-device control (the main use case) works fine, but some actions can
crash OpenRGB **on some systems**. The two below are what's turned up so far -
they're **almost certainly not the only rough edges**, so treat this as a
non-exhaustive list and expect the occasional surprise. Each of these has an
easy workaround:

- **"Rescan Devices" can crash OpenRGB.** You don't normally need to rescan -
  the keyboard is detected at startup and stays detected.
- **"Apply to all devices" can crash OpenRGB.** Set this keyboard's mode/color
  from its own device page instead.

These are edge paths - the core per-device control is solid and unaffected.

## Files
- `KeychronV6UltraController.*` - the raw-HID protocol (0x16 command).
- `RGBController_KeychronV6Ultra.*` - OpenRGB device wrapper (108 LEDs, Direct).
- `OpenRGBKeychronV6UltraPlugin.*` - plugin entry: enumerate + register.
