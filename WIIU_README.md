# Perfect Dark - Wii U Port

A port of [Perfect Dark](https://github.com/fgsfdsfgs/perfect_dark) to the Nintendo Wii U, based on the x86 PC decompilation/port by fgsfdsfgs.

**This is a beta release.** The game is playable but has known issues.

## Requirements

- Wii U with Homebrew Channel
- SD card
- Perfect Dark ROM: `ntsc-final` / `US V1.1` (md5 `e03b088b6ac9e0080440efed07c1e40f`)

## Installation

1. Copy the `perfectdark` folder to `sd:/wiiu/apps/perfectdark/` on your SD card
2. Place your Perfect Dark ROM file in the same folder, named `pd.ntsc-final.z64`
3. Launch from Homebrew Launcher

### SD Card Layout

```
sd:/
  wiiu/
    apps/
      perfectdark/
        pd.ppc.rpx
        meta.xml
        pd.ntsc-final.z64
```

## What Works

- Main menu and all menus
- Single player campaign (tested through multiple missions)
- HUD elements (health, ammo, timer)
- Controller input via Wii U GamePad
- Audio
- Saving/loading
- Exit to Wii U menu

## Known Issues

- **No menu blur effect** - Pause menu background is solid black instead of a blurred screenshot of gameplay. The framebuffer blit operations used for blur are not supported on the GX2 backend.
- **Curly particle trails** - Particle effects (explosions, bullet trails) have incorrect geometry/transform, causing curly or distorted trails.
- **Vision modes disabled** - Night vision, thermal vision, and X-ray vision overlays are disabled. The GX2 backend does not support the blended overlay render modes these effects require.
- **No motion blur** - Motion blur effect is disabled.
- **No fisheye effect** - The eye spy camera fisheye distortion is disabled.
- **Screen fade/tint disabled** - Screen color tint effects (used by vision modes) are disabled due to unsupported render modes on GX2.

## Building from Source

### Requirements

- [devkitPro](https://devkitpro.org/) with devkitPPC and Wii U portlibs
- WUT (Wii U Toolchain)
- SDL2 for Wii U
- zlib for Wii U

### Build

```bash
./build_wiiu_wut.sh
```

The build script will compile and output `pd.ppc.rpx` in the `build-wiiu-wut/` directory.

## Credits

- [n64decomp/perfect_dark](https://github.com/n64decomp/perfect_dark) - Original N64 decompilation
- [fgsfdsfgs/perfect_dark](https://github.com/fgsfdsfgs/perfect_dark) - PC port this Wii U port is based on
- GaryOderNichts - LibUltraship Wii U/GX2 backend reference
