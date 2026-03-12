#!/bin/bash
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITPPC=${DEVKITPRO}/devkitPPC
export PATH=${DEVKITPPC}/bin:${DEVKITPRO}/tools/bin:$PATH

ROMID=${1:-ntsc-final}
echo "Building for ROMID: ${ROMID} using wut toolchain"

# Auto-clean: remove build directory
echo "Cleaning build directory..."
rm -rf build-wiiu-wut

mkdir -p build-wiiu-wut
cd build-wiiu-wut

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

# Auto-copy to SD card
echo "Copying build to SD card..."
# MSYS2 path: use /d/ instead of D:/
SD_DEST="/e/wiiu/apps/perfectdark"

# Create destination directory if it doesn't exist
mkdir -p "$SD_DEST"

# Copy all three executables to root of perfectdark folder
if [ -f "pd.ppc.elf" ]; then
    cp -v "pd.ppc.elf" "$SD_DEST/pd.ppc.elf"
    echo "✓ Copied pd.ppc.elf"
fi

if [ -f "pd.ppc.rpx" ]; then
    cp -v "pd.ppc.rpx" "$SD_DEST/pd.ppc.rpx"
    echo "✓ Copied pd.ppc.rpx"
fi

if [ -f "pd.ppc.wuhb" ]; then
    cp -v "pd.ppc.wuhb" "$SD_DEST/pd.ppc.wuhb"
    echo "✓ Copied pd.ppc.wuhb"
fi

# Copy meta.xml if it exists
if [ -f "../port/meta.xml" ]; then
    cp -v "../port/meta.xml" "$SD_DEST/meta.xml"
    echo "✓ Copied meta.xml"
fi

echo "Build complete! App is at: $SD_DEST"
