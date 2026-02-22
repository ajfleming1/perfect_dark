/*  gfx_wiiu.cpp - Fast3D Wii U window manager for Perfect Dark

    Based on libultraship Wii U backend created in 2022 by GaryOderNichts
    Adapted for Perfect Dark port
*/
#ifdef __WIIU__

#include <stdio.h>
#include <time.h>
#include <malloc.h>

#include <coreinit/time.h>
#include <coreinit/thread.h>
#include <coreinit/foreground.h>
#include <coreinit/memory.h>
#include <coreinit/memheap.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/memexpheap.h>
#include <coreinit/memfrmheap.h>

#include <gx2/state.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <gx2/swap.h>
#include <gx2/mem.h>
#include <gx2r/mem.h>

#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_udp.h>
#include <whb/log_cafe.h>
#include <proc_ui/procui.h>
#include <proc_ui/memory.h>

#include <vpad/input.h>
#include <padscore/kpad.h>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif

#include "gfx_window_manager_api.h"
#include "gfx_pc.h"
#include "gfx_gx2.h"
#include "gfx_wiiu.h"

static MEMHeapHandle heap_MEM1 = nullptr;
static MEMHeapHandle heap_foreground = nullptr;

bool has_foreground = false;
static void* mem1_storage = nullptr;
static void* command_buffer_pool = nullptr;
GX2ContextState* context_state = nullptr;

static GX2TVRenderMode tv_render_mode;
static void* tv_scan_buffer = nullptr;
static uint32_t tv_scan_buffer_size = 0;
static uint32_t tv_width;
static uint32_t tv_height;

static GX2DrcRenderMode drc_render_mode;
static void* drc_scan_buffer = nullptr;
static uint32_t drc_scan_buffer_size = 0;

static int frame_divisor = 1;
uint32_t frametime = 1;

bool gfx_wiiu_init_mem1(void) {
    MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);
    uint32_t size;
    void* base;

    size = MEMGetAllocatableSizeForFrmHeapEx(heap, 4);
    if (!size) {
        printf("%s: MEMGetAllocatableSizeForFrmHeapEx == 0", __FUNCTION__);
        return false;
    }

    base = MEMAllocFromFrmHeapEx(heap, size, 4);
    if (!base) {
        printf("%s: MEMAllocFromFrmHeapEx failed", __FUNCTION__);
        return false;
    }

    heap_MEM1 = MEMCreateExpHeapEx(base, size, 0);
    if (!heap_MEM1) {
        printf("%s: MEMCreateExpHeapEx failed", __FUNCTION__);
        return false;
    }

    return true;
}

void gfx_wiiu_destroy_mem1(void) {
    MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);

    if (heap_MEM1) {
        MEMDestroyExpHeap(heap_MEM1);
        heap_MEM1 = nullptr;
    }

    MEMFreeToFrmHeap(heap, MEM_FRM_HEAP_FREE_ALL);
}

bool gfx_wiiu_init_foreground(void) {
    MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_FG);
    uint32_t size;
    void* base;

    size = MEMGetAllocatableSizeForFrmHeapEx(heap, 4);
    if (!size) {
        printf("%s: MEMGetAllocatableSizeForFrmHeapEx failed", __FUNCTION__);
        return false;
    }

    base = MEMAllocFromFrmHeapEx(heap, size, 4);
    if (!base) {
        printf("%s: MEMAllocFromFrmHeapEx failed", __FUNCTION__);
        return false;
    }

    heap_foreground = MEMCreateExpHeapEx(base, size, 0);
    if (!heap_foreground) {
        printf("%s: MEMCreateExpHeapEx failed", __FUNCTION__);
        return false;
    }

    return true;
}

void gfx_wiiu_destroy_foreground(void) {
    MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_FG);

    if (heap_foreground) {
        MEMDestroyExpHeap(heap_foreground);
        heap_foreground = nullptr;
    }

    MEMFreeToFrmHeap(heap, MEM_FRM_HEAP_FREE_ALL);
}

void* gfx_wiiu_alloc_mem1(uint32_t size, uint32_t alignment) {
    if (!heap_MEM1) {
        return nullptr;
    }
    return MEMAllocFromExpHeapEx(heap_MEM1, size, alignment);
}

void gfx_wiiu_free_mem1(void* block) {
    if (!heap_MEM1) {
        return;
    }
    MEMFreeToExpHeap(heap_MEM1, block);
}

void* gfx_wiiu_alloc_foreground(uint32_t size, uint32_t alignment) {
    if (!heap_foreground) {
        return nullptr;
    }
    return MEMAllocFromExpHeapEx(heap_foreground, size, alignment);
}

void gfx_wiiu_free_foreground(void* block) {
    if (!heap_foreground) {
        return;
    }
    MEMFreeToExpHeap(heap_foreground, block);
}

static uint32_t gfx_wiiu_proc_callback_acquired(void* context) {
    has_foreground = true;
    gfx_wiiu_init_foreground();

    tv_scan_buffer = gfx_wiiu_alloc_foreground(tv_scan_buffer_size, GX2_SCAN_BUFFER_ALIGNMENT);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, tv_scan_buffer, tv_scan_buffer_size);
    GX2SetTVBuffer(tv_scan_buffer, tv_scan_buffer_size, tv_render_mode, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
                   GX2_BUFFERING_MODE_DOUBLE);

    drc_scan_buffer = gfx_wiiu_alloc_foreground(drc_scan_buffer_size, GX2_SCAN_BUFFER_ALIGNMENT);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU, drc_scan_buffer, drc_scan_buffer_size);
    GX2SetDRCBuffer(drc_scan_buffer, drc_scan_buffer_size, drc_render_mode, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
                    GX2_BUFFERING_MODE_DOUBLE);

    return 0;
}

static uint32_t gfx_wiiu_proc_callback_released(void* context) {
    gfx_gx2_shutdown();
    gfx_wiiu_destroy_foreground();
    has_foreground = false;

    return 0;
}

static void gfx_wiiu_init(const struct GfxWindowInitSettings *settings) {
    WHBLogPrintf("gfx_wiiu_init: starting");

    WHBProcInit();
    WHBLogPrintf("gfx_wiiu_init: WHBProcInit done");

    // Init ProcUI
    uint32_t mem1_addr, mem1_size;
    OSGetMemBound(OS_MEM1, &mem1_addr, &mem1_size);
    WHBLogPrintf("gfx_wiiu_init: MEM1 addr=0x%08x size=%u", mem1_addr, mem1_size);

    mem1_storage = memalign(0x40, mem1_size);
    WHBLogPrintf("gfx_wiiu_init: mem1_storage=%p", mem1_storage);

    ProcUISetMEM1Storage(mem1_storage, mem1_size);
    WHBLogPrintf("gfx_wiiu_init: ProcUISetMEM1Storage done");

    ProcUIRegisterCallback(PROCUI_CALLBACK_ACQUIRE, gfx_wiiu_proc_callback_acquired, nullptr, 100);
    ProcUIRegisterCallback(PROCUI_CALLBACK_RELEASE, gfx_wiiu_proc_callback_released, nullptr, 100);
    WHBLogPrintf("gfx_wiiu_init: ProcUI callbacks registered");

    // Init GX2
    command_buffer_pool = memalign(GX2_COMMAND_BUFFER_ALIGNMENT, 0x400000);
    WHBLogPrintf("gfx_wiiu_init: command_buffer_pool=%p", command_buffer_pool);

    GX2Init((uint32_t[]){ GX2_INIT_CMD_BUF_BASE, (uintptr_t)command_buffer_pool, GX2_INIT_CMD_BUF_POOL_SIZE, 0x400000,
                          GX2_INIT_END });
    WHBLogPrintf("gfx_wiiu_init: GX2Init done");

    // Create context state
    context_state = (GX2ContextState*)memalign(GX2_CONTEXT_STATE_ALIGNMENT, sizeof(GX2ContextState));
    WHBLogPrintf("gfx_wiiu_init: context_state=%p", context_state);

    GX2SetupContextStateEx(context_state, TRUE);
    GX2SetContextState(context_state);
    WHBLogPrintf("gfx_wiiu_init: context state setup done");

    // Set TV render mode
    GX2TVScanMode scanMode = GX2GetSystemTVScanMode();
    WHBLogPrintf("gfx_wiiu_init: TV scan mode=%d", (int)scanMode);

    switch (scanMode) {
        case GX2_TV_SCAN_MODE_480I:
        case GX2_TV_SCAN_MODE_480P:
            tv_render_mode = GX2_TV_RENDER_MODE_WIDE_480P;
            tv_width = 854;
            tv_height = 480;
            break;
        case GX2_TV_SCAN_MODE_1080I:
        case GX2_TV_SCAN_MODE_1080P:
            tv_render_mode = GX2_TV_RENDER_MODE_WIDE_1080P;
            tv_width = 1920;
            tv_height = 1080;
            break;
        case GX2_TV_SCAN_MODE_720P:
        default:
            tv_render_mode = GX2_TV_RENDER_MODE_WIDE_720P;
            tv_width = 1280;
            tv_height = 720;
            break;
    }
    WHBLogPrintf("gfx_wiiu_init: tv_render_mode=%d tv=%dx%d", (int)tv_render_mode, tv_width, tv_height);

    drc_render_mode = GX2_DRC_RENDER_MODE_SINGLE;

    uint32_t tv_unk, drc_unk;
    GX2CalcTVSize(tv_render_mode, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_DOUBLE, &tv_scan_buffer_size, &tv_unk);
    GX2CalcDRCSize(drc_render_mode, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_DOUBLE, &drc_scan_buffer_size, &drc_unk);
    WHBLogPrintf("gfx_wiiu_init: tv_scan_buffer_size=%u drc_scan_buffer_size=%u", tv_scan_buffer_size, drc_scan_buffer_size);

    gfx_wiiu_init_mem1();
    WHBLogPrintf("gfx_wiiu_init: init_mem1 done");

    // Should call Acquire callback immediately
    WHBLogPrintf("gfx_wiiu_init: entering ProcUI loop");
    ProcUIStatus status;
    while ((status = ProcUIProcessMessages(TRUE)) != PROCUI_STATUS_IN_FOREGROUND) {
        WHBLogPrintf("gfx_wiiu_init: ProcUI status=%d", (int)status);
        if (status == PROCUI_STATUS_RELEASE_FOREGROUND) {
            ProcUIDrawDoneRelease();
        }
        if (status == PROCUI_STATUS_EXITING) {
            WHBLogPrintf("gfx_wiiu_init: exiting due to PROCUI_STATUS_EXITING");
            return;
        }
    }
    WHBLogPrintf("gfx_wiiu_init: ProcUI loop done, has_foreground=%d", (int)has_foreground);

    // In Cemu (and some real hardware configs), the app starts in foreground but the
    // ACQUIRE callback is never triggered by ProcUIProcessMessages. If that happened,
    // manually invoke the acquire callback now so scan buffers and has_foreground are set up.
    if (!has_foreground) {
        WHBLogPrintf("gfx_wiiu_init: acquire callback not fired, calling manually");
        gfx_wiiu_proc_callback_acquired(nullptr);
        WHBLogPrintf("gfx_wiiu_init: manual acquire done, has_foreground=%d", (int)has_foreground);
    }

    // These may not be supported in Cemu, but are needed on real hardware
    GX2SetTVScale(WIIU_DEFAULT_FB_WIDTH, WIIU_DEFAULT_FB_HEIGHT);
    GX2SetDRCScale(WIIU_DEFAULT_FB_WIDTH, WIIU_DEFAULT_FB_HEIGHT);
    WHBLogPrintf("gfx_wiiu_init: TV/DRC scale set");

    GX2SetSwapInterval(frame_divisor);
    WHBLogPrintf("gfx_wiiu_init: swap interval set");

    gfx_current_dimensions.width = gfx_current_game_window_viewport.width = WIIU_DEFAULT_FB_WIDTH;
    gfx_current_dimensions.height = gfx_current_game_window_viewport.height = WIIU_DEFAULT_FB_HEIGHT;
    WHBLogPrintf("gfx_wiiu_init: complete");
}

static void gfx_wiiu_close(void) {
    if (has_foreground) {
        gfx_wiiu_proc_callback_released(nullptr);
        gfx_wiiu_destroy_mem1();
    }

    GX2Shutdown();

    if (context_state) {
        free(context_state);
        context_state = nullptr;
    }

    if (command_buffer_pool) {
        free(command_buffer_pool);
        command_buffer_pool = nullptr;
    }

    ProcUISetMEM1Storage(nullptr, 0);
    free(mem1_storage);
}

void gfx_wiiu_set_context_state(void) {
    GX2SetContextState(context_state);
}

static int gfx_wiiu_get_display_mode(int modenum, int *out_w, int *out_h) {
    if (modenum == 0) {
        *out_w = WIIU_DEFAULT_FB_WIDTH;
        *out_h = WIIU_DEFAULT_FB_HEIGHT;
        return 1;
    }
    return 0;
}

static int gfx_wiiu_get_current_display_mode(int *out_w, int *out_h) {
    *out_w = WIIU_DEFAULT_FB_WIDTH;
    *out_h = WIIU_DEFAULT_FB_HEIGHT;
    return 0;
}

static int gfx_wiiu_get_num_display_modes(void) {
    return 1;
}

static int32_t gfx_wiiu_get_fullscreen_state(void) {
    return 1; // Always fullscreen on Wii U
}

static void gfx_wiiu_set_fullscreen_changed_callback(void (*on_fullscreen_changed)(bool is_now_fullscreen)) {
}

static void gfx_wiiu_set_fullscreen(bool enable) {
}

static void gfx_wiiu_set_fullscreen_exclusive(bool exc) {
}

static void gfx_wiiu_set_fullscreen_flag(int32_t mode) {
}

static int32_t gfx_wiiu_get_fullscreen_flag_mode(void) {
    return 1;
}

static int32_t gfx_wiiu_get_maximized_state(void) {
    return 1;
}

static void gfx_wiiu_set_maximize(bool enable) {
}

static void gfx_wiiu_get_active_window_refresh_rate(uint32_t* refresh_rate) {
    *refresh_rate = 60;
}

static void gfx_wiiu_set_cursor_visibility(bool visible) {
}

static void gfx_wiiu_set_closest_resolution(int32_t width, int32_t height, bool should_center) {
}

static void gfx_wiiu_set_dimensions(uint32_t width, uint32_t height, int32_t posX, int32_t posY) {
}

static void gfx_wiiu_get_dimensions(uint32_t* width, uint32_t* height, int32_t* posX, int32_t* posY) {
    *width = WIIU_DEFAULT_FB_WIDTH;
    *height = WIIU_DEFAULT_FB_HEIGHT;
    *posX = 0;
    *posY = 0;
}

static void gfx_wiiu_get_centered_positions(int32_t width, int32_t height, int32_t *posX, int32_t *posY) {
    *posX = 0;
    *posY = 0;
}

static void gfx_wiiu_handle_events(void) {
    ProcUIStatus status = ProcUIProcessMessages(TRUE);
    if (status == PROCUI_STATUS_RELEASE_FOREGROUND) {
        ProcUIDrawDoneRelease();
    }
}

static bool gfx_wiiu_start_frame(void) {
    uint32_t swap_count, flip_count;
    OSTime last_flip, last_vsync;
    uint32_t wait_count = 0;

    while (true) {
        GX2GetSwapStatus(&swap_count, &flip_count, &last_flip, &last_vsync);

        if (flip_count >= swap_count) {
            break;
        }

        if (wait_count >= 10) {
            GX2WaitForFlip();
            break;
        }

        wait_count++;
        OSSleepTicks(OSMicrosecondsToTicks(1000));
    }

    return has_foreground;
}

static void gfx_wiiu_swap_buffers_begin(void) {
    GX2SwapScanBuffers();
    GX2Flush();

    GX2SetTVEnable(TRUE);
    GX2SetDRCEnable(TRUE);
}

static void gfx_wiiu_swap_buffers_end(void) {
    static uint64_t lastTime = 0;

    GX2DrawDone();

    uint64_t now = OSGetTime();
    frametime = OSTicksToMicroseconds(now - lastTime);
    lastTime = now;
}

static double gfx_wiiu_get_time(void) {
    return OSTicksToMicroseconds(OSGetTime()) / 1000000.0;
}

static int32_t gfx_wiiu_get_target_fps(void) {
    return 60 / frame_divisor;
}

static void gfx_wiiu_set_target_fps(int fps) {
    if (fps >= 60) {
        frame_divisor = 1;
    } else if (fps >= 30) {
        frame_divisor = 2;
    } else if (fps >= 20) {
        frame_divisor = 3;
    } else {
        frame_divisor = 4;
    }

    GX2SetSwapInterval(frame_divisor);
}

static bool gfx_wiiu_can_disable_vsync(void) {
    return false;
}

static void* gfx_wiiu_get_window_handle(void) {
    return nullptr;
}

static void gfx_wiiu_set_window_title(const char* title) {
}

static int gfx_wiiu_get_swap_interval(void) {
    return frame_divisor;
}

static bool gfx_wiiu_set_swap_interval(int interval) {
    frame_divisor = interval;
    GX2SetSwapInterval(frame_divisor);
    return true;
}

struct GfxWindowManagerAPI gfx_wiiu = {
    gfx_wiiu_init,
    gfx_wiiu_close,
    gfx_wiiu_get_display_mode,
    gfx_wiiu_get_current_display_mode,
    gfx_wiiu_get_num_display_modes,
    gfx_wiiu_get_fullscreen_state,
    gfx_wiiu_set_fullscreen_changed_callback,
    gfx_wiiu_set_fullscreen,
    gfx_wiiu_set_fullscreen_exclusive,
    gfx_wiiu_set_fullscreen_flag,
    gfx_wiiu_get_fullscreen_flag_mode,
    gfx_wiiu_get_maximized_state,
    gfx_wiiu_set_maximize,
    gfx_wiiu_get_active_window_refresh_rate,
    gfx_wiiu_set_cursor_visibility,
    gfx_wiiu_set_closest_resolution,
    gfx_wiiu_set_dimensions,
    gfx_wiiu_get_dimensions,
    gfx_wiiu_get_centered_positions,
    gfx_wiiu_handle_events,
    gfx_wiiu_start_frame,
    gfx_wiiu_swap_buffers_begin,
    gfx_wiiu_swap_buffers_end,
    gfx_wiiu_get_time,
    gfx_wiiu_get_target_fps,
    gfx_wiiu_set_target_fps,
    gfx_wiiu_can_disable_vsync,
    gfx_wiiu_get_window_handle,
    gfx_wiiu_set_window_title,
    gfx_wiiu_get_swap_interval,
    gfx_wiiu_set_swap_interval,
};

#endif
