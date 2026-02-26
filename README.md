# AlienFX-linux

Ported version of [AlienFX](https://github.com/T-Troll/alienfx-tools) for Linux, which gives
complete light support for most of the device dell produce.

Check out `Example-App/` for a sample application.
All testing applications are located in `alienfx-cli/` folder.

Tested on linux:

- Alienware elc (g-series,alienware m series)
- Darfon (alienware series)

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

~~- ACPI-controlled lights - 3 lights, 8 bit/color (v0) - Aurora R6/R7 (using this API require AlienFan-SDK library from [AlienFX-Tools](https://github.com/T-Troll/alienfx-tools) project).~~ Not Planned

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

## Dependencies

- cmake
- ninja

## Build

```bash
mkdir build/
cd build/
cmake .. -G Ninja -DALIENFX_BUILD_CLI=ON -DALIENFX_BUILD_EXAMPLE=ON
ninja
```

It would build 3 executables:

- `AlienFX-SDK` - static library with AlienFX SDK
- `Example-App` - sample application
- `alienfx-cli` - command line tool for testing and configuring lights

# Credits

- [T-Troll](https://github.com/T-Troll) - for original sdk and resources
- [StromyCoder](https://github.com/StormyCoder) - for testing in m18 r2
- # [ZackSM](https://github.com/valtasar27) - for testing m18 r1
    <small> bro wanted his name big </small>
