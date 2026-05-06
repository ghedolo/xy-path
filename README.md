# gplog

RP2040 firmware that plays arbitrary 2D vector paths on an analog oscilloscope used as an XY vector display. The RP2040 streams stereo audio via a PIO-driven I2S DAC at 96 kHz — left/right channels drive the X and Y deflection inputs. A third digital output (GP5) controls beam blanking via the oscilloscope Z input: the beam is turned off during flyback between paths. Paths are defined as sequences of XY integer coordinates (0–16384) in a plain text file specified at build time.

## Hardware wiring

| RP2040 pin | Signal | Destination |
|---|---|---|
| GP2 | I2S BCLK | DAC BCLK |
| GP3 | I2S LRCLK | DAC LRCLK |
| GP4 | I2S DIN | DAC DIN |
| GP5 | Z blanking | Oscilloscope Z input |

DAC left output → oscilloscope X input  
DAC right output → oscilloscope Y input

## Path file format

```
# One path per line, wrapped in [ ]
# Coordinates: integers 0..16384  (8192 = center of screen)
[0,0, 0,16384, 16384,16384, 16384,0, 0,0]   # rectangle
[0,0, 16384,16384]                            # diagonal
```

- Empty lines and lines starting with `#` are ignored
- Each path is drawn with beam on; flyback between paths has beam off (Z blanking)

## Build

```
mkdir -p tmp/build && cd tmp/build
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain-xpack.cmake
make -j$(nproc)
```

To use a custom paths file:
```
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain-xpack.cmake -DPATHS_FILE=/path/to/file.txt
```

## Slash commands

| Command | Effect |
|---|---|
| `/flash-gplog [file]` | Build and flash (optional custom paths file), no button press required |
| `/commit-gplog` | Update README with fresh dev stats, commit and push |

## Serial commands (USB CDC, 115200 baud)

| Key | Effect |
|---|---|
| `+` / `=` | Increase `z_offset` (beam blanking guard, 0–60) |
| `-` | Decrease `z_offset` |
| `a` | Increase scale (+500, max 32767) |
| `z` | Decrease scale (−500, min 2000) |
| `d` / `c` | Flyback steps ±1 (1–40) |
| `r` | Reset all to defaults |
| `h` | Print help |

Defaults: `z_offset=20`, `scale=32000`, `flyback=10`.

## License and Credits

**License:** GPL-3.0-or-later

**Author:** ghedo (luca.ghedini@gmail.com) — 2026

**Development Tool:** The project was constructed using Claude Code by Anthropic.
