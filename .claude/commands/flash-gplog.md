Build and flash the gplog firmware to the RP2040.

Usage: /flash-gplog [path]

If a path argument is provided ($ARGUMENTS), use it as the paths source.
The path can be absolute or relative to the project root — it is NOT assumed to be inside vect/.
If no argument is given, all vect/*.txt files are used.

Follow these steps exactly, in order:

## 1. Determine paths file

If $ARGUMENTS is non-empty, use it verbatim as PATHS_FILE (absolute or relative path as-is).
Otherwise use the project default (no -DPATHS_FILE needed, all vect/*.txt are used).

## 2. Build

Change to the build directory and configure + build:

```
cd /Users/ghedo/script/AllClaude/rp2040/gplog/tmp/build
```

If $ARGUMENTS is non-empty:
```
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain-xpack.cmake -DPATHS_FILE=<$ARGUMENTS>
```
Otherwise, if CMakeCache.txt already exists just run:
```
cmake --build .
```
If CMakeCache.txt does not exist, configure first:
```
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchain-xpack.cmake
cmake --build .
```

Stop if the build fails — do not proceed to flash.

## 3. Reboot into BOOTSEL

```
picotool reboot -f -u
```

Requires dangerouslyDisableSandbox: true (accesses /Volumes/).

## 4. Wait for the volume

```
sleep 2 && ls /Volumes/RPI-RP2/
```

If not mounted yet, wait and retry. Do not proceed until RPI-RP2 is visible.

## 5. Flash

```
cp /Users/ghedo/script/AllClaude/rp2040/gplog/tmp/build/gplog.uf2 /Volumes/RPI-RP2/
```

Requires dangerouslyDisableSandbox: true.

The Pico reboots automatically after the copy.
