#!/bin/bash
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITPPC=${DEVKITPRO}/devkitPPC
export PATH=${DEVKITPPC}/bin:${DEVKITPRO}/tools/bin:$PATH

ROMID=${1:-ntsc-final}
echo "Building for ROMID: ${ROMID} using wut toolchain"

mkdir -p build-wiiu-wut
cd build-wiiu-wut
rm -f CMakeCache.txt

# Check if wut toolchain exists
# WUT_TOOLCHAIN="${DEVKITPRO}/wut/share/wut.toolchain.cmake"
# if [ ! -f "$WUT_TOOLCHAIN" ]; then
#     echo "Error: wut toolchain not found at $WUT_TOOLCHAIN"
#     echo "Please ensure wut is installed in your devkitPro environment."
#     exit 1
# fi

# Use the recommended wrapper for Wii U builds
CMAKE_WIIU="${DEVKITPRO}/portlibs/wiiu/bin/powerpc-eabi-cmake"

if [ ! -f "$CMAKE_WIIU" ]; then
    echo "Error: Wii U cmake wrapper not found at $CMAKE_WIIU"
    echo "Falling back to standard powerpc-eabi-cmake (check PATH)"
    CMAKE_WIIU="powerpc-eabi-cmake"
fi

"$CMAKE_WIIU" -G"Unix Makefiles" -DROMID=${ROMID} ..
make -j$(nproc)
