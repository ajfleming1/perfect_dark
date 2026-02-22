#!/bin/bash
export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=/opt/devkitpro/devkitPPC
export PATH=/opt/devkitpro/portlibs/wiiu/bin:/opt/devkitpro/devkitPPC/bin:/opt/devkitpro/tools/bin:$PATH

cd /c/Users/andre/Desktop/two-ship/perfect_dark
rm -rf build-wiiu
mkdir -p build-wiiu
powerpc-eabi-cmake -G"Unix Makefiles" -Bbuild-wiiu .
cd build-wiiu
make -j4
