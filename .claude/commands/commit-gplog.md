Commit the current state of the gplog project to GitHub.

Follow these steps exactly, in order:

## 1. Compute development stats

Run: `source .venv/bin/activate && python3 tmp/gen_dev_stats.py`

If .venv does not exist, create it first: `python3 -m venv .venv && source .venv/bin/activate && pip install -r tmp/requirements.txt`

Capture the full stdout — paste it verbatim into the README.

## 2. Read the source files

Read `main.c` and `renderer.c` to understand the current state (defaults, serial commands, any recent changes).

## 3. Write README.md (in English, overwrite every time)

Write a fresh `README.md` with the following sections, **all in English**:

### gplog

One-paragraph description: RP2040 firmware that plays arbitrary 2D vector paths on an analog oscilloscope used as an XY vector display. The RP2040 streams stereo audio via a PIO-driven I2S DAC at 96 kHz — left/right channels drive the X and Y deflection inputs. A third digital output (GP5) controls beam blanking via the oscilloscope Z input: the beam is turned off during flyback between paths. Paths are defined as sequences of XY integer coordinates (0–16384) in a plain text file specified at build time.

### Hardware wiring

| RP2040 pin | Signal | Destination |
|---|---|---|
| GP2 | I2S BCLK | DAC BCLK |
| GP3 | I2S LRCLK | DAC LRCLK |
| GP4 | I2S DIN | DAC DIN |
| GP5 | Z blanking | Oscilloscope Z input |

DAC left output → oscilloscope X input  
DAC right output → oscilloscope Y input

### Path file format

```
# One path per line, wrapped in [ ]
# Coordinates: integers 0..16384  (8192 = center of screen)
[0,0, 0,16384, 16384,16384, 16384,0, 0,0]   # rectangle
[0,0, 16384,16384]                            # diagonal
```

### Build

```
mkdir -p tmp/build && cd tmp/build
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain-xpack.cmake
make -j$(nproc)
```

To use a custom paths file:
```
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain-xpack.cmake -DPATHS_FILE=/path/to/file.txt
```

### Slash commands

| Command | Effect |
|---|---|
| `/flash-gplog [file]` | Build and flash (optional custom paths file), no button press required |
| `/commit-gplog` | Update README with fresh dev stats, commit and push |

### Serial commands (USB CDC, 115200 baud)

Read the actual defaults from `main.c` (DEF_* constants) and write them accurately.

| Key | Effect |
|---|---|
| `+` / `=` | Increase `z_offset` (beam blanking guard, 0–60) |
| `-` | Decrease `z_offset` |
| `a` | Increase scale (+500, max 32767) |
| `z` | Decrease scale (−500, min 2000) |
| `d` / `c` | Flyback steps ±1 (1–40) |
| `r` | Reset all to defaults |
| `h` | Print help |

### License and Credits

**License:** GPL-3.0-or-later

**Author:** ghedo (luca.ghedini@gmail.com) — 2026

**Development Tool:** The project was constructed using Claude Code by Anthropic.

---

Then append the full output of `gen_dev_stats.py` (the Development effort section).

## 4. Stage files

```
git add main.c renderer.c renderer.h CMakeLists.txt audio_i2s.pio \
        pico_sdk_import.cmake toolchain-xpack.cmake flash.sh \
        paths.txt README.md .gitignore \
        tmp/gen_paths_h.py \
        .claude/commands/flash-gplog.md .claude/commands/commit-gplog.md
```

## 5. Commit

Read `git diff --cached` to understand what changed, then write a concise English commit message (imperative mood, max 72 chars). Create the commit.

## 6. Push

`git push`

If the upstream is not set: `git push -u origin main`
