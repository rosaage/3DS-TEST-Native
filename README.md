# 3DS TEST Native

A native alternative to the original Lua tester, built with libctru, citro2d and citro3d. It starts directly on the test screen and needs no Lua engine, game directory, or legacy sf2d libraries.

- Live X/Y values and position plots for the Circle Pad and either the built-in C-stick or a Circle Pad Pro.
- Held-button indicators for A/B/X/Y, D-pad, L/R, ZL/ZR, START and SELECT.
- Touch coordinates and a short tone when an input is pressed.
- Hold START + SELECT together for about one second to exit. HOME is handled by the system.

On New 3DS/2DS hardware, the C-stick and ZL/ZR use libctru's built-in HID/IRRST support. On original 3DS hardware, the app searches for a Circle Pad Pro over infrared and reads its stick, ZL/ZR and extra R button. Press a button on the CPP to wake it. The second panel shows connection status and any IR initialization error instead of claiming these controls require a New 3DS. It reconnects automatically and clears stale button/stick state when reports stop.

The built-in sticks display signed HID readings with no added dead zone or normalization (plot scale: +/-156). **CPP values are the original uncalibrated 12-bit samples, from 0 to 4095**, with nominal center 2048. Its plot uses that nominal center and the entire 12-bit range; it does not apply factory or user calibration. Increasing X is right and increasing Y is up. Only the plot marker is clamped. CPP and HID values therefore use different units. Audio is optional: if DSP cannot initialize, input testing still works and the screen says sound is unavailable.

CPP communication requires access to the `ir:USER` service. The CIA configuration includes this permission. When using `.3dsx`, use a Homebrew Launcher environment that grants this service (such as a current Rosalina setup); if access is denied, try the CIA build. CPP support has been compiled and its packet decoder tested, but connection and infrared behavior still need validation on physical hardware. New 3DS hardware uses its internal controls rather than trying to connect to an external CPP.

## Build

In a devkitPro MSYS2 terminal, install the standard toolchain if needed:

```sh
pacman -S --needed 3ds-dev
make
```

Output: `3DS-TEST-Native.3dsx` and `3DS-TEST-Native.smdh`. Copy both into `SD:/3ds/3DS-TEST-Native/` and launch from the Homebrew Launcher. No additional data files are required. The build reuses the original repository's icon; source and assets retain the repository's license.

## Homebrew distribution ZIP

Run `make dist` to build the app and create `3DS-TEST-Native.zip`. Packaging requires `bsdtar` (included in devkitPro MSYS2); you can override its path with `BSDTAR=...`.

The ZIP is ready to extract to the SD card root and contains:

```text
3ds/
  3DS-TEST-Native/
    licenses/
    LICENSE
    3DS-TEST-Native.3dsx
    3DS-TEST-Native.smdh
```

## CIA build

Install [makerom](https://github.com/3DSGuy/Project_CTR/releases) and [bannertool](https://github.com/Steveice10/bannertool/releases), placing their executables on your PATH. These tools are only required for CIA packaging.

```sh
make cia
```

Output: `3DS-TEST-Native.cia`, for installation on a console running custom firmware. This embeds the same native application, the original banner image, and a silent banner sound. It also builds the usual `.3dsx` output.

You can specify tool paths explicitly:

```sh
make cia BANNERTOOL=C:/tools/bannertool.exe
```

The native CIA uses title ID `0004000000C64B00`, separate from the original tool's `0004000000C64A00`. Override its unique ID with `make cia APP_UNIQUE_ID=0xC64C` if needed. Choose an ID that does not conflict with your installed titles.

## CPP implementation and checks

The CPP client is implemented locally; it does not require rebuilding libctru or installing another library. Its wire format follows the [Circle Pad Pro protocol](https://3dbrew.org/wiki/Circle_Pad_Pro) and [IRUSER shared-memory layout](https://3dbrew.org/wiki/IRUSER_Shared_Memory). It uses bounded packet parsing, CRC-8 validation, nonblocking frame-based polling, and connection retries. It requests input reports only and does not write calibration data. Infrared communication is released while sleeping or suspended and restarted after resuming.

The pure packet decoder can be checked on a computer with GCC:

```sh
gcc -std=c11 -Wall -Wextra -Werror tests/test_cpp_packet.c -o build/test_cpp_packet
./build/test_cpp_packet
```

Building verifies compilation and packaging, not hardware behavior. Check CPP wake/connection, both stick axes, simultaneous console/CPP buttons, loss of infrared connection, reconnect, sleep/resume, audio, the exit gesture, and HOME-menu launching on a real console.

## Original readme
[Original readme file](README_legacy.md)
