#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/tmp/build"
UF2="$BUILD/gplog.uf2"

# --- build ---

mkdir -p "$BUILD"
cd "$BUILD"

if [ -n "$1" ]; then
    rm -f CMakeCache.txt
    cmake "$ROOT" -DCMAKE_TOOLCHAIN_FILE=../../toolchain-xpack.cmake -DPATHS_FILE="$1"
elif [ ! -f CMakeCache.txt ]; then
    cmake "$ROOT" -DCMAKE_TOOLCHAIN_FILE=../../toolchain-xpack.cmake
fi

cmake --build . --parallel

if [ ! -f "$UF2" ]; then
    echo "UF2 not found after build: $UF2"
    exit 1
fi

# --- flash ---

echo "Reboot in BOOTSEL..."
picotool reboot -f -u

echo "Waiting for RPI-RP2 (max 15s)..."
for i in $(seq 1 30); do
    if [ -d /Volumes/RPI-RP2 ]; then break; fi
    sleep 0.5
done

if [ ! -d /Volumes/RPI-RP2 ]; then
    echo "Timeout: RPI-RP2 not found."
    exit 1
fi

sleep 0.5

echo "Copying gplog.uf2 -> /Volumes/RPI-RP2/"
for attempt in 1 2 3; do
    if cp "$UF2" /Volumes/RPI-RP2/ 2>/dev/null; then
        break
    fi
    if [ $attempt -lt 3 ]; then
        sleep 1
    else
        echo "Flash failed: Permission denied"
        exit 1
    fi
done
echo "Flash done."
