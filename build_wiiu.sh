#!/bin/bash
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITPPC=${DEVKITPRO}/devkitPPC
export PATH=${DEVKITPPC}/bin:${DEVKITPRO}/tools/bin:$PATH

ROMID=${1:-ntsc-final}
echo "Building for ROMID: ${ROMID}"

mkdir -p build-wiiu
cd build-wiiu
rm -f CMakeCache.txt
powerpc-eabi-cmake -G"Unix Makefiles" -DROMID=${ROMID} ..
make -j$(nproc)
