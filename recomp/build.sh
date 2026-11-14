#!/bin/sh
# Build the native game from a disc image you own.
#   ./build.sh [path/to/disc1.cue]     (default: assets/image/disc.cue)
# Needs: python3, cmake, ninja, a C/C++ compiler (clang), git and network access once (SDL3 is fetched).
set -eu
cd "$(dirname "$0")"
cue="${1:-assets/image/disc.cue}"
[ -f "$cue" ] || { echo "disc image not found: $cue (put Metal Gear Solid Integral disc 1 there, see README)"; exit 1; }
mkdir -p extracted
python3 tools/discfs.py "$cue" extract 'MGS\SLPM_862.47' extracted/SLPM_862.47
python3 tools/psxrecomp.py extracted/SLPM_862.47 --out generated --hooks games/mgs/hooks.py
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ${CC:+-DCMAKE_C_COMPILER=$CC} ${CXX:+-DCMAKE_CXX_COMPILER=$CXX}
cmake --build build
echo "built ./psxrecomp-mgs"
