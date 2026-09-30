# AlienFX-linux

Ported version of [AlienFX](https://github.com/T-Troll/alienfx-tools) for Linux, which gives
complete light support for most of the device dell produce.

Check out `Example-App/` for a sample application.
All testing applications are located in `alienfx-cli/` folder.

Tested on linux:

- Alienware elc (g-series,alienware m series)
- Darfon (alienware series)

## Verified in this fork

Per-key keyboard color is verified on one laptop. The Area-51 sequence runs only when the USB id is Darfon `0d62:1bbc`. For that id, the built-in key map wins over `~/.local/share/alienfx/mappings.json`. Other keyboards keep the upstream protocol and still need a mappings file. A Darfon keyboard with neither a shipped profile nor a mappings entry is reported by `setall` and left unchanged.

| Laptop | Keyboard | What works here |
| --- | --- | --- |
| Alienware 16 Area-51 AA16250 | Darfon `0d62:1bbc`, chassis AW-ELC `187c:0551` | `setkeys` for per-key color, `keyboarddim` for keyboard brightness, `keyboardeffect` (`breathe`, `wave`, `pulse`, `spectrum`, `rainbow`), `powerbutton` for plugged-in and battery colors, `powerrainbow` for a slower rainbow on the power button. Selected by USB id. Static color and rainbow were used on this machine. |
| Alienware 18 Area-51 AA18250 | not verified | Same generation. Not claimed. |

`setall`, `setone`, `setzone`, `setaction`, `setpower`, `setdim`, `setglobal`, `status`, and `probe` behave as before. `setpower` is the existing per-device power command. `powerbutton` and `powerrainbow` target light `0x1b` on AW-ELC `187c:0551`, then `0550`. The lightbar, logo, speakers, and trackpad are still driven by AWCC, not by these keyboard commands.

On `0d62:1bbc`, generic APIv5 color (index + 1, then loop and update) does not stick, and a reset before the color leaves the hardware animation running. This fork stops that animation and writes the wire ids with no index shift. That path is not used for any other PID.

## AlienFan-SDK

Library for controlling fans and sensors of Alienware hardware.

### Supported devices:

Any Alienware/Dell G-series hardware from 2010 to 2025.

## AlienFX-SDK

Better AlienFX/LightFX SDK than Dell official's one without any limitations:

- Better performance. Update rate can be very high for modern devices.
- Tiny footprint both in code and RAM.
- Full hardware features support - per-key and per-device effect set, hardware brightness, hardware power buttons.
- Using direct hardware access, so no additional software required.

### Supported devices:

Any Alienware/Dell G-series hardware with RGB lights from 2010 to 2025. Including mouses, keyboards, monitors.
Read more details about supported devices and models [here](https://github.com/T-Troll/alienfx-tools/wiki/Supported-and-tested-devices-list)

### Supported device API versions:

- ~~ACPI-controlled lights - 3 lights, 8 bit/color (v0) - Aurora R6/R7 (using this API require AlienFan-SDK library from [AlienFX-Tools](https://github.com/T-Troll/alienfx-tools) project).~~ Not Planned
- 9 bytes 8 bit/color, reportID 2 control (v1) - Ancient notebooks - deprecated and removed.
- 9 bytes 4 bit/color, reportID 2 control (v2) - Older notebooks (like m14/17x, 13R1/R2)
- 12 bytes 8 bit/color, reportID 2 control (v3) - Old notebooks (like 15R5)
- 34 bytes 8 bit/color, reportID 0 control (v4) - Modern notebooks/desktop (all m-series, x-series, Dell g-series, Aurora R8+)
- 64 bytes 8 bit/color, featureID 0xcc control (v5) - Modern notebooks internal per-key RGB keyboard (all m- and x-series)
- 65 bytes 8 bit/color, interrupt control (v6) - Mouses
- 65 bytes 8 bit/color, featureID control (v7) - Monitors
- 65 bytes 8 bit/color, Interrupt control (v8) - External keyboards

Check compatibility list and API details [here](https://github.com/T-Troll/alienfx-tools/wiki/Supported-and-tested-devices-list).

Some notebooks can have 2 devices - APIv4 (for logo, power button, etc) and APIv5 for keyboard.

### Supported hardware features:

- Multiply devices detection and handling
- User-provided device, light or group (zone) names
- Change light color
- Change multiply lights color
- Change light hardware effect (except APIv0 and v5)
- Change multiply lights hardware effects (except APIv0 and APIv5)
- Hardware-backed global light off/on/dim (dim is software for v6 and should be done by application)
- Global (all lights) hardware light effects (APIv5, v8)
- Power/indicator button light control (AC/battery hardware switch and effects)

## Testing

```bash
cmake -S . -B build -G Ninja -DALIENFX_BUILD_CLI=ON -DALIENFX_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

`pipe` starts `alienfx_cli`, writes to its stdin, and checks stdout, stderr, and the exit code. `--help` and `-h` must list the lighting commands. `setall` and `setkeys` with no arguments must fail before a device is opened. `lint` walks the C and C++ sources for trailing whitespace, CR bytes, a missing final newline, and conflict markers. Neither test names a laptop or opens a USB device. GitHub Actions runs both on every pull request, and once more on each push to `main`.

## Dependencies

- cmake
- ninja

## Build

```bash
mkdir build/
cd build/
cmake .. -G Ninja -DALIENFX_BUILD_CLI=ON -DALIENFX_BUILD_EXAMPLE=ON #OPTIONAL -DCMAKE_BUILD_TYPE=Debug
ninja
```

It would build 3 executables:

- `AlienFX-SDK` - static library with AlienFX SDK
- `alienfx-cli` - command line tool for testing and configuring lights
- `Example-App` - sample application

### Help Menu

```bash
AlienFX CLI v1.2.0
Control all of your Alienware device from the comfort of your terminal


alienfx_cli [OPTIONS] SUBCOMMAND


OPTIONS:
  -h,     --help              Print this help message and exit
  -v                          Display program version information and exit
          --brightness UINT [255]
                              Global brightness 0-255
          --tempo UINT [5]    Tempo for actions
          --length UINT [5]   Length for actions
  -r,     --repeat INT:NONNEGATIVE [1]
                              Repeat count (0 = infinite)
  -d,     --delay INT:NONNEGATIVE [0]
                              Delay between repeats (ms)
  -s,     --save              Save to hardware (persist across reboots)

SUBCOMMANDS:
  setall                      r g b - set all lights
  setkeys                     id r g b [id r g b ...] - set individual keyboard keys
  keyboarddim                 0-255 - brightness of the Area-51 keyboard
  keyboardeffect              breathe|wave|pulse|spectrum|rainbow r g b - hardware effect on
                              the Area-51 keyboard
  powerbutton                 r g b r2 g2 b2 - plugged-in color, then battery color, on the
                              power button
  powerrainbow                Slow rainbow on the power button, separate from the chassis
                              animation
  setone                      dev light r g b - set one light
  setzone                     zone r g b - set zone lights
  setaction                   dev light action r g b [action r g b] - set light and enable
                              action
  setpower                    dev light r g b r2 g2 b2 - set power button colors
  setzoneaction               zone action r g b [action r g b] - set all zone lights and enable
                              action
  setdim                      [dev] br - set brightness
  setglobal                   dev type mode [r g b [r g b]]
  status                      Show devices, lights and zones
  probe                       Probe lights and set names (interactive)
  createlightzone             Interactively create a light zone/group
  getpowerprofile             Get current power profile
  supportedprofiles           Get supported power profiles
  setpowerprofile             Set power profile (requires root)
  reset                       Reset device (if supported)
```
### UPDATES

#### UPDATE (#1)

For now if u want to configure any device you have to run `HOME=/home/username sudo ./alienfx_cli probe` And configure + probe all ur device (this time it would save it to config files

Once done u can see ur devices by running `./alienfx_cli status` which would output

````
Device #0 - Alienware AW-ELC, VID#6268, PID#1361, APIv4, 4 lights
  Light ID#0 - Left
  Light ID#1 - Middle Left
  Light ID#2 - Middle Right
  Light ID#3 - Right
More devices in ur case
````

And then u can use any of the function of alienfx-cli
Example: To set all lights to red i would do `./alienfx_cli setall 255 0 0` and every light would be red


Currently all device in windows version are supported (except some acpi device)


#### UPDATE (#2)

Added Create Zone command `createlightzone` its intractive

Now u can create ur custom zone of any light type

Example:

 I wanna make WASD key change colour together i can create a zone and then directly change color of that zone

```
❮ ./alienfx_cli status
2026-02-28 18:25:11.404 (   0.013s) [        CB3267C0]               main.cpp:42    INFO| 1 low-level devices found.
Device #0 - Alienware AW-ELC, VID#0x187c, PID#0x551, APIv4, 4 lights
  Light ID#0 - Left
  Light ID#1 - Middle Left
  Light ID#2 - Middle Right
  Light ID#3 - Right
5 zones:
  Zone #1 (1 lights) - left
  Zone #2 (1 lights) - middle_left
  Zone #3 (1 lights) - middle_right
  Zone #4 (1 lights) - right
  Zone #5 (4 lights) - all
```


#### UPDATE (#3)

- Added `setpower` for setting power button colors and `--save` for permanently applying the light effects
- Added `reset` for supported device to reset the light chip.

# Credits

- [T-Troll](https://github.com/T-Troll) - for original sdk and resources
- [StromyCoder](https://github.com/StormyCoder) - for testing in m18 r2
- [ZackSM](https://github.com/valtasar27) - for testing m18 r1

All the other people for testing and feedback.
