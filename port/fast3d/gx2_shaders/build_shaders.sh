#!/bin/bash
# to build shaders you need to place a copy of latte-assembler into the current directory
# latte-assembler is part of decaf-emu <https://github.com/decaf-emu/decaf-emu>

cd "${0%/*}"

# conv
echo "Building conv ..."
./latte-assembler assemble --vsh=conv.vsh --psh=conv.psh conv.gsh
xxd -i conv.gsh > conv.inc
echo "Done!"
