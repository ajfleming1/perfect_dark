/*  gfx_wiiu.h - Fast3D Wii U window manager for Perfect Dark

    Based on libultraship Wii U backend created in 2022 by GaryOderNichts
    Adapted for Perfect Dark port
*/
#ifndef GFX_WIIU_H
#define GFX_WIIU_H
#ifdef __WIIU__

#include "gfx_window_manager_api.h"

// make the default fb always 1080p to not mess with scaling
#define WIIU_DEFAULT_FB_WIDTH 1920
#define WIIU_DEFAULT_FB_HEIGHT 1080

extern bool has_foreground;
extern uint32_t frametime;

bool gfx_wiiu_init_mem1(void);

void gfx_wiiu_destroy_mem1(void);

bool gfx_wiiu_init_foreground(void);

void gfx_wiiu_destroy_foreground(void);

void* gfx_wiiu_alloc_mem1(uint32_t size, uint32_t alignment);

void gfx_wiiu_free_mem1(void* block);

void* gfx_wiiu_alloc_foreground(uint32_t size, uint32_t alignment);

void gfx_wiiu_free_foreground(void* block);

void gfx_wiiu_set_context_state(void);

extern struct GfxWindowManagerAPI gfx_wiiu;

#endif
#endif
