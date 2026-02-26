/*  gfx_gx2.cpp - Fast3D GX2 backend for Perfect Dark

    Based on libultraship GX2 backend created in 2022 by GaryOderNichts
    Adapted for Perfect Dark port
*/
#ifdef __WIIU__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <malloc.h>

#include <map>
#include <array>
#include <set>
#include <algorithm>
#include <unordered_map>
#include <utility>

// Hash function for std::pair<float, float>
struct hash_pair_ff {
    size_t operator()(const std::pair<float, float>& p) const {
        auto h1 = std::hash<float>{}(p.first);
        auto h2 = std::hash<float>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#include "gfx_cc.h"
#include "gfx_rendering_api.h"
#include "gfx_pc.h"
#include "gfx_wiiu.h"

#include <gx2/texture.h>
#include <gx2/draw.h>
#include <gx2/clear.h>
#include <gx2/state.h>
#include <gx2/swap.h>
#include <gx2/event.h>
#include <gx2/utils.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/display.h>
#include <whb/log.h>
#include "gx2_shader_gen.h"
#include "gx2_util.h"

#include <proc_ui/procui.h>
#include <coreinit/memory.h>
#include <coreinit/cache.h>

#define ALIGN(x, align) (((x) + ((align)-1)) & ~((align)-1))

struct ShaderProgram {
    struct ShaderGroup group;
    uint8_t num_inputs;
    bool used_textures[2];
    bool used_noise;
    uint32_t window_params_offset;
    int32_t samplers_location[8];
};

struct Texture {
    GX2Texture texture;
    bool texture_uploaded;

    GX2Sampler sampler;
    bool sampler_set;
};

struct Framebuffer {
    GX2ColorBuffer color_buffer;
    bool colorBufferMem1;
    GX2DepthBuffer depth_buffer;
    bool depthBufferMem1;

    GX2Texture texture;
    GX2Sampler sampler;
};

static std::array<Framebuffer, 100> framebuffers;
static std::size_t used_framebuffers;
static std::size_t current_framebuffer;
static GX2DepthBuffer depthReadBuffer;

static std::map<std::pair<uint64_t, uint32_t>, struct ShaderProgram> shader_program_pool;
static struct ShaderProgram* current_shader_program;

static struct Texture* selected_textures[SHADER_MAX_TEXTURES];
static int selected_fbs[SHADER_MAX_TEXTURES];
static int last_selected_tile = 0;

// 96 Mb (should be more than enough to draw everything without waiting for the GPU)
#define DRAW_BUFFER_SIZE 0x6000000
static uint8_t* draw_buffer = nullptr;
static uint8_t* draw_ptr = nullptr;

// GX2 backend does not support the G_IMAGERECT_EXT command needed for framebuffer eyespy rendering
bool gfx_framebuffers_enabled = false;

static uint32_t frame_count;
static float current_noise_scale;
static FilteringMode current_filter_mode = FILTER_LINEAR;

static BOOL current_depth_test = TRUE;
static BOOL current_depth_write = TRUE;
static GX2CompareFunction current_depth_compare_function = GX2_COMPARE_FUNC_LESS;

static float current_viewport_x = 0.0f;
static float current_viewport_y = 0.0f;
static float current_viewport_width = WIIU_DEFAULT_FB_WIDTH;
static float current_viewport_height = WIIU_DEFAULT_FB_HEIGHT;

static uint32_t current_scissor_x = 0;
static uint32_t current_scissor_y = 0;
static uint32_t current_scissor_width = WIIU_DEFAULT_FB_WIDTH;
static uint32_t current_scissor_height = WIIU_DEFAULT_FB_HEIGHT;

static bool current_zmode_decal = false;
static float current_SSDB = -2.0f;
static bool current_use_alpha = false;

static inline GX2SamplerVar* GX2GetPixelSamplerVar(const GX2PixelShader* shader, const char* name) {
    for (uint32_t i = 0; i < shader->samplerVarCount; ++i) {
        if (strcmp(name, shader->samplerVars[i].name) == 0) {
            return &shader->samplerVars[i];
        }
    }

    return nullptr;
}

static inline int32_t GX2GetPixelSamplerVarLocation(const GX2PixelShader* shader, const char* name) {
    GX2SamplerVar* sampler = GX2GetPixelSamplerVar(shader, name);
    return sampler ? sampler->location : -1;
}

static inline int32_t GX2GetPixelUniformVarOffset(const GX2PixelShader* shader, const char* name) {
    GX2UniformVar* uniform = GX2GetPixelUniformVar(shader, name);
    return uniform ? uniform->offset : -1;
}

static const char* gfx_gx2_get_name() {
    return "GX2";
}

static int gfx_gx2_get_max_texture_size() {
    // TODO: This should be a define from the Wii U toolchain, but there isn't one yet
    return 8192;
}

static void gfx_gx2_init_framebuffer(struct Framebuffer* buffer, uint32_t width, uint32_t height) {
    memset(&buffer->color_buffer, 0, sizeof(GX2ColorBuffer));
    buffer->color_buffer.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
    buffer->color_buffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    buffer->color_buffer.surface.width = width;
    buffer->color_buffer.surface.height = height;
    buffer->color_buffer.surface.depth = 1;
    buffer->color_buffer.surface.mipLevels = 1;
    buffer->color_buffer.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    buffer->color_buffer.surface.aa = GX2_AA_MODE1X;
    buffer->color_buffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;  // Use default tiling for scan buffer compatibility
    buffer->color_buffer.viewNumSlices = 1;

    memset(&buffer->depth_buffer, 0, sizeof(GX2DepthBuffer));
    buffer->depth_buffer.surface.use = GX2_SURFACE_USE_DEPTH_BUFFER | GX2_SURFACE_USE_TEXTURE;
    buffer->depth_buffer.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    buffer->depth_buffer.surface.width = width;
    buffer->depth_buffer.surface.height = height;
    buffer->depth_buffer.surface.depth = 1;
    buffer->depth_buffer.surface.mipLevels = 1;
    buffer->depth_buffer.surface.format = GX2_SURFACE_FORMAT_FLOAT_R32;
    buffer->depth_buffer.surface.aa = GX2_AA_MODE1X;
    buffer->depth_buffer.surface.tileMode = GX2_TILE_MODE_DEFAULT;
    buffer->depth_buffer.viewNumSlices = 1;
    buffer->depth_buffer.depthClear = 1.0f;
}

static struct GfxClipParameters gfx_gx2_get_clip_parameters(void) {
    // GX2 (Wii U) uses DirectX-style NDC: z in [0, 1], not OpenGL's [-1, 1].
    // Setting z_is_from_0_to_1=true causes gfx_pc to apply (z+w)/2 remapping
    // so N64/OpenGL projection matrices work correctly with GX2's depth clipping.
    return { true, false };
}

static void gfx_gx2_set_uniforms(struct ShaderProgram* prg) {
    float window_params_array[4] = { current_noise_scale, (float)frame_count, 0.0f, 0.0f };

    GX2SetPixelUniformReg(prg->window_params_offset, 4, window_params_array);
}

static void gfx_gx2_unload_shader(struct ShaderProgram* old_prg) {
    current_shader_program = nullptr;
}

static void gfx_gx2_load_shader(struct ShaderProgram* new_prg) {
    current_shader_program = new_prg;

    GX2SetFetchShader(&new_prg->group.fetchShader);
    GX2SetVertexShader(&new_prg->group.vertexShader);
    GX2SetPixelShader(&new_prg->group.pixelShader);

    gfx_gx2_set_uniforms(new_prg);

    // Re-bind currently selected textures and samplers to the new shader
    for (int i = 0; i < SHADER_MAX_TEXTURES; i++) {
        int32_t sampler_location = new_prg->samplers_location[i];
        if (sampler_location == -1) {
            continue;
        }

        if (selected_fbs[i] != -1) {
            Framebuffer& buffer = framebuffers[selected_fbs[i]];
            GX2SetPixelTexture(&buffer.texture, sampler_location);
            GX2SetPixelSampler(&buffer.sampler, sampler_location);
        } else {
            struct Texture* tex = selected_textures[i];
            if (tex) {
                if (tex->texture_uploaded) {
                    GX2SetPixelTexture(&tex->texture, sampler_location);
                }
                if (tex->sampler_set) {
                    GX2SetPixelSampler(&tex->sampler, sampler_location);
                }
            }
        }
    }
}

static struct ShaderProgram* gfx_gx2_create_and_load_new_shader(uint64_t shader_id0, uint32_t shader_id1) {
    struct CCFeatures cc_features;
    gfx_cc_get_features(shader_id0, shader_id1, &cc_features);

    printf("Generating shader: %016llx-%08x\n", shader_id0, shader_id1);
    struct ShaderGroup group;
    if (gx2GenerateShaderGroup(&group, &cc_features) != 0) {
        printf("Failed to generate shader\n");
        current_shader_program = nullptr;
        return nullptr;
    }

    struct ShaderProgram* prg = &shader_program_pool[std::make_pair(shader_id0, shader_id1)];
    prg->group = group;
    prg->num_inputs = cc_features.num_inputs;
    prg->used_textures[0] = cc_features.used_textures[0];
    prg->used_textures[1] = cc_features.used_textures[1];
    prg->used_noise = cc_features.opt_alpha && cc_features.opt_noise;

    prg->window_params_offset = GX2GetPixelUniformVarOffset(&prg->group.pixelShader, "window_params");
    prg->samplers_location[0] = GX2GetPixelSamplerVarLocation(&prg->group.pixelShader, "uTex0");
    prg->samplers_location[1] = GX2GetPixelSamplerVarLocation(&prg->group.pixelShader, "uTex1");
    prg->samplers_location[2] = GX2GetPixelSamplerVarLocation(&prg->group.pixelShader, "uTexMask0");
    prg->samplers_location[3] = GX2GetPixelSamplerVarLocation(&prg->group.pixelShader, "uTexMask1");
    prg->samplers_location[4] = GX2GetPixelSamplerVarLocation(&prg->group.pixelShader, "uTexBlend0");
    prg->samplers_location[5] = GX2GetPixelSamplerVarLocation(&prg->group.pixelShader, "uTexBlend1");
    prg->samplers_location[6] = -1;
    prg->samplers_location[7] = -1;

    gfx_gx2_load_shader(prg);

    printf("Generated and loaded shader\n");

    return prg;
}

static struct ShaderProgram* gfx_gx2_lookup_shader(uint64_t shader_id0, uint32_t shader_id1) {
    auto it = shader_program_pool.find(std::make_pair(shader_id0, shader_id1));
    return it == shader_program_pool.end() ? nullptr : &it->second;
}

static void gfx_gx2_shader_get_info(struct ShaderProgram* prg, uint8_t* num_inputs, bool used_textures[2]) {
    *num_inputs = prg->num_inputs;
    used_textures[0] = prg->used_textures[0];
    used_textures[1] = prg->used_textures[1];
}

static uint32_t gfx_gx2_new_texture(void) {
    struct Texture* tex = (struct Texture*)calloc(1, sizeof(struct Texture));

    // some 32-bit trickery :P
    return (uint32_t)tex;
}

static void gfx_gx2_delete_texture(uint32_t texture_id) {
    struct Texture* tex = (struct Texture*)texture_id;

    if (tex->texture.surface.image) {
        GX2DrawDone();
        free(tex->texture.surface.image);
        tex->texture.surface.image = nullptr;
    }

    free((void*)tex);
}

static void gfx_gx2_select_texture(int tile, uint32_t texture_id, bool linear_filter) {
    if (tile < 0 || tile >= SHADER_MAX_TEXTURES) {
        return;
    }

    struct Texture* tex = (struct Texture*)texture_id;
    selected_textures[tile] = tex;
    selected_fbs[tile] = -1;
    last_selected_tile = tile;
    (void)linear_filter; // Linear filtering handled by sampler parameters

    if (current_shader_program) {
        int32_t sampler_location = current_shader_program->samplers_location[tile];
        if (sampler_location != -1) {
            if (tex->texture_uploaded) {
                GX2SetPixelTexture(&tex->texture, sampler_location);
            }

            if (tex->sampler_set) {
                GX2SetPixelSampler(&tex->sampler, sampler_location);
            }
        }
    }
}

static void gfx_gx2_upload_texture(const uint8_t* rgba32_buf, uint32_t width, uint32_t height) {
    struct Texture* tex = selected_textures[last_selected_tile];
    assert(tex);

    if ((tex->texture.surface.width != width) || (tex->texture.surface.height != height) ||
        !tex->texture.surface.image) {

        if (tex->texture.surface.image) {
            GX2DrawDone();
            free(tex->texture.surface.image);
            tex->texture.surface.image = nullptr;
        }

        memset(&tex->texture, 0, sizeof(GX2Texture));
        tex->texture.surface.use = GX2_SURFACE_USE_TEXTURE;
        tex->texture.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        tex->texture.surface.width = width;
        tex->texture.surface.height = height;
        tex->texture.surface.depth = 1;
        tex->texture.surface.mipLevels = 1;
        tex->texture.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
        tex->texture.surface.aa = GX2_AA_MODE1X;
        tex->texture.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
        tex->texture.viewFirstMip = 0;
        tex->texture.viewNumMips = 1;
        tex->texture.viewFirstSlice = 0;
        tex->texture.viewNumSlices = 1;
        tex->texture.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);

        GX2CalcSurfaceSizeAndAlignment(&tex->texture.surface);
        GX2InitTextureRegs(&tex->texture);

        tex->texture.surface.image = memalign(tex->texture.surface.alignment, tex->texture.surface.imageSize);
        if (!tex->texture.surface.image) {
            return;
        }
    }

    uint8_t* buf = (uint8_t*)tex->texture.surface.image;
    assert(buf);

    for (uint32_t y = 0; y < height; ++y) {
        memcpy(buf + (y * tex->texture.surface.pitch * 4), rgba32_buf + (y * width * 4), width * 4);
    }

    DCFlushRange(tex->texture.surface.image, tex->texture.surface.imageSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, tex->texture.surface.image, tex->texture.surface.imageSize);

    if (current_shader_program && current_shader_program->samplers_location[last_selected_tile] != -1) {
        GX2SetPixelTexture(&tex->texture, current_shader_program->samplers_location[last_selected_tile]);
    }

    tex->texture_uploaded = true;
}

static GX2TexClampMode gfx_cm_to_gx2(uint32_t val) {
    switch (val) {
        case G_TX_NOMIRROR | G_TX_CLAMP:
            return GX2_TEX_CLAMP_MODE_CLAMP;
        case G_TX_MIRROR | G_TX_WRAP:
            return GX2_TEX_CLAMP_MODE_MIRROR;
        case G_TX_MIRROR | G_TX_CLAMP:
            return GX2_TEX_CLAMP_MODE_MIRROR_ONCE;
        case G_TX_NOMIRROR | G_TX_WRAP:
            return GX2_TEX_CLAMP_MODE_WRAP;
    }

    return GX2_TEX_CLAMP_MODE_WRAP;
}

static void gfx_gx2_set_sampler_parameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt) {
    if (tile < 0 || tile >= SHADER_MAX_TEXTURES) {
        return;
    }

    struct Texture* tex = selected_textures[tile];
    if (!tex) {
        return;
    }

    last_selected_tile = tile;

    GX2InitSampler(&tex->sampler, GX2_TEX_CLAMP_MODE_CLAMP,
                   (linear_filter && current_filter_mode == FILTER_LINEAR) ? GX2_TEX_XY_FILTER_MODE_LINEAR
                                                                           : GX2_TEX_XY_FILTER_MODE_POINT);

    GX2InitSamplerClamping(&tex->sampler, gfx_cm_to_gx2(cms), gfx_cm_to_gx2(cmt), GX2_TEX_CLAMP_MODE_WRAP);

    if (current_shader_program && current_shader_program->samplers_location[tile] != -1) {
        GX2SetPixelSampler(&tex->sampler, current_shader_program->samplers_location[tile]);
    }

    tex->sampler_set = true;
}

// ZMODE constants from gbi.h
#define ZMODE_OPA   0
#define ZMODE_INTER 0x400
#define ZMODE_XLU   0x800
#define ZMODE_DEC   0xc00

static void gfx_gx2_set_depth_mode(bool depth_test, bool depth_update, bool depth_compare, bool depth_source_prim, uint16_t zmode) {
    current_depth_test = depth_test;
    current_depth_write = depth_update;

    if (depth_test) {
        if (depth_compare) {
            switch (zmode) {
                case ZMODE_INTER:
                    current_depth_compare_function = GX2_COMPARE_FUNC_LEQUAL;
                    GX2SetPolygonOffset(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
                    GX2SetPolygonControl(GX2_FRONT_FACE_CCW, FALSE, FALSE, FALSE, GX2_POLYGON_MODE_TRIANGLE,
                                         GX2_POLYGON_MODE_TRIANGLE, FALSE, FALSE, FALSE);
                    current_zmode_decal = false;
                    break;
                case ZMODE_OPA:
                case ZMODE_XLU:
                    current_depth_compare_function = GX2_COMPARE_FUNC_LESS;
                    GX2SetPolygonOffset(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
                    GX2SetPolygonControl(GX2_FRONT_FACE_CCW, FALSE, FALSE, FALSE, GX2_POLYGON_MODE_TRIANGLE,
                                         GX2_POLYGON_MODE_TRIANGLE, FALSE, FALSE, FALSE);
                    current_zmode_decal = false;
                    break;
                case ZMODE_DEC:
                    current_depth_compare_function = GX2_COMPARE_FUNC_LEQUAL;
                    // Enable polygon offset for decal mode
                    float SSDB = -2.0f;
                    current_SSDB = SSDB;
                    GX2SetPolygonOffset(SSDB, SSDB, SSDB, SSDB, 0.0f);
                    GX2SetPolygonControl(GX2_FRONT_FACE_CCW, FALSE, FALSE, TRUE, GX2_POLYGON_MODE_TRIANGLE,
                                         GX2_POLYGON_MODE_TRIANGLE, TRUE, TRUE, FALSE);
                    current_zmode_decal = true;
                    break;
            }
        } else {
            current_depth_compare_function = GX2_COMPARE_FUNC_ALWAYS;
            GX2SetPolygonOffset(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            GX2SetPolygonControl(GX2_FRONT_FACE_CCW, FALSE, FALSE, FALSE, GX2_POLYGON_MODE_TRIANGLE,
                                 GX2_POLYGON_MODE_TRIANGLE, FALSE, FALSE, FALSE);
            current_zmode_decal = false;
        }
    } else {
        current_depth_compare_function = GX2_COMPARE_FUNC_ALWAYS;
        GX2SetPolygonOffset(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        GX2SetPolygonControl(GX2_FRONT_FACE_CCW, FALSE, FALSE, FALSE, GX2_POLYGON_MODE_TRIANGLE,
                             GX2_POLYGON_MODE_TRIANGLE, FALSE, FALSE, FALSE);
        current_zmode_decal = false;
    }

    GX2SetDepthOnlyControl(current_depth_test, current_depth_write, current_depth_compare_function);
}

static void gfx_gx2_set_depth_range(float znear, float zfar) {
    // GX2 doesn't have a direct depth range function like OpenGL
    // The depth is controlled through the viewport settings
    // For now, just store the values if needed
    (void)znear;
    (void)zfar;
}

static void gfx_gx2_set_viewport(int x, int y, int width, int height) {
    Framebuffer& buffer = framebuffers[current_framebuffer];
    uint32_t buffer_height = buffer.color_buffer.surface.height;

    current_viewport_x = x;
    current_viewport_y = buffer_height - y - height;
    current_viewport_width = width;
    current_viewport_height = height;

    GX2SetViewport(current_viewport_x, current_viewport_y, current_viewport_width, current_viewport_height, 0.0f, 1.0f);
}

static void gfx_gx2_set_scissor(int x, int y, int width, int height) {
    Framebuffer& buffer = framebuffers[current_framebuffer];
    uint32_t buffer_height = buffer.color_buffer.surface.height;
    uint32_t buffer_width = buffer.color_buffer.surface.width;

    // Convert from OpenGL (Y=0 at bottom) to GX2 (Y=0 at top), same as set_viewport
    int gx2_x = x;
    int gx2_y = (int)buffer_height - y - height;
    int gx2_width = width;
    int gx2_height = height;

    // Clamp left/top edges into buffer (adjust size to compensate)
    if (gx2_x < 0) { gx2_width += gx2_x; gx2_x = 0; }
    if (gx2_y < 0) { gx2_height += gx2_y; gx2_y = 0; }

    // Ensure positive dimensions
    if (gx2_width <= 0) gx2_width = 1;
    if (gx2_height <= 0) gx2_height = 1;

    // Clamp right/bottom edges to buffer bounds
    if ((uint32_t)gx2_x + (uint32_t)gx2_width > buffer_width)
        gx2_width = (int)buffer_width - gx2_x;
    if ((uint32_t)gx2_y + (uint32_t)gx2_height > buffer_height)
        gx2_height = (int)buffer_height - gx2_y;

    // Final safety clamps after right/bottom edge adjustment
    if (gx2_width <= 0) gx2_width = 1;
    if (gx2_height <= 0) gx2_height = 1;

    current_scissor_x = (uint32_t)gx2_x;
    current_scissor_y = (uint32_t)gx2_y;
    current_scissor_width = (uint32_t)gx2_width;
    current_scissor_height = (uint32_t)gx2_height;

    // Always use full framebuffer to avoid portal black rectangle artifacts
    GX2SetScissor(0, 0, WIIU_DEFAULT_FB_WIDTH, WIIU_DEFAULT_FB_HEIGHT);
}

static void gfx_gx2_set_use_alpha(bool use_alpha, bool modulate) {
    current_use_alpha = use_alpha;
    (void)modulate; // Modulate not currently used
    GX2SetColorControl(GX2_LOGIC_OP_COPY, use_alpha ? 0xff : 0, FALSE, TRUE);
}

static void gfx_gx2_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    if (!current_shader_program) {
        return;
    }

    size_t vbo_len = sizeof(float) * buf_vbo_len;

    if (draw_ptr + vbo_len >= draw_buffer + DRAW_BUFFER_SIZE) {
        printf("Waiting on GPU!!!\n");
        GX2DrawDone();
        draw_ptr = draw_buffer;
    }

    float* new_vbo = (float*)draw_ptr;
    draw_ptr += ALIGN(vbo_len, GX2_VERTEX_BUFFER_ALIGNMENT);

    memcpy(new_vbo, buf_vbo, vbo_len);
    DCFlushRange(new_vbo, vbo_len);
    GX2Invalidate(GX2_INVALIDATE_MODE_ATTRIBUTE_BUFFER, new_vbo, vbo_len);

    GX2SetAttribBuffer(0, vbo_len, current_shader_program->group.stride, new_vbo);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, 3 * buf_vbo_num_tris, 0, 1);
}

static void gfx_gx2_init(void) {
    WHBLogPrintf("gfx_gx2_init: starting");

    // Init the default framebuffer
    used_framebuffers = 1;
    Framebuffer& main_framebuffer = framebuffers[0];

    gfx_gx2_init_framebuffer(&main_framebuffer, WIIU_DEFAULT_FB_WIDTH, WIIU_DEFAULT_FB_HEIGHT);
    WHBLogPrintf("gfx_gx2_init: framebuffer struct initialized %dx%d", WIIU_DEFAULT_FB_WIDTH, WIIU_DEFAULT_FB_HEIGHT);

    GX2CalcSurfaceSizeAndAlignment(&main_framebuffer.color_buffer.surface);
    GX2InitColorBufferRegs(&main_framebuffer.color_buffer);
    WHBLogPrintf("gfx_gx2_init: color buffer size=%u align=%u",
           main_framebuffer.color_buffer.surface.imageSize,
           main_framebuffer.color_buffer.surface.alignment);

    main_framebuffer.color_buffer.surface.image = gfx_wiiu_alloc_mem1(main_framebuffer.color_buffer.surface.imageSize,
                                                                      main_framebuffer.color_buffer.surface.alignment);
    WHBLogPrintf("gfx_gx2_init: color buffer image=%p", main_framebuffer.color_buffer.surface.image);
    assert(main_framebuffer.color_buffer.surface.image);
    main_framebuffer.colorBufferMem1 = true;

    GX2CalcSurfaceSizeAndAlignment(&main_framebuffer.depth_buffer.surface);
    GX2InitDepthBufferRegs(&main_framebuffer.depth_buffer);
    WHBLogPrintf("gfx_gx2_init: depth buffer size=%u align=%u",
           main_framebuffer.depth_buffer.surface.imageSize,
           main_framebuffer.depth_buffer.surface.alignment);

    main_framebuffer.depth_buffer.surface.image = gfx_wiiu_alloc_mem1(main_framebuffer.depth_buffer.surface.imageSize,
                                                                      main_framebuffer.depth_buffer.surface.alignment);
    WHBLogPrintf("gfx_gx2_init: depth buffer image=%p", main_framebuffer.depth_buffer.surface.image);
    assert(main_framebuffer.depth_buffer.surface.image);
    main_framebuffer.depthBufferMem1 = true;

    // Initialize the texture part of the main framebuffer for sampling
    memset(&main_framebuffer.texture, 0, sizeof(GX2Texture));
    main_framebuffer.texture.surface = main_framebuffer.color_buffer.surface;
    main_framebuffer.texture.surface.use = GX2_SURFACE_USE_TEXTURE;
    main_framebuffer.texture.viewNumMips = 1;
    main_framebuffer.texture.viewNumSlices = 1;
    main_framebuffer.texture.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);
    GX2InitTextureRegs(&main_framebuffer.texture);

    GX2InitSampler(&main_framebuffer.sampler, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    // create a linear aligned copy of the depth buffer to read pixels to
    memcpy(&depthReadBuffer, &main_framebuffer.depth_buffer, sizeof(GX2DepthBuffer));

    depthReadBuffer.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    depthReadBuffer.surface.width = 32;
    depthReadBuffer.surface.height = 1;

    GX2CalcSurfaceSizeAndAlignment(&depthReadBuffer.surface);

    depthReadBuffer.surface.image =
        gfx_wiiu_alloc_mem1(depthReadBuffer.surface.imageSize, depthReadBuffer.surface.alignment);
    assert(depthReadBuffer.surface.image);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_DEPTH_BUFFER, depthReadBuffer.surface.image,
                  depthReadBuffer.surface.imageSize);

    GX2SetColorBuffer(&main_framebuffer.color_buffer, GX2_RENDER_TARGET_0);
    GX2SetDepthBuffer(&main_framebuffer.depth_buffer);

    current_framebuffer = 0;

    // allocate draw buffer
    draw_buffer = (uint8_t*)memalign(GX2_VERTEX_BUFFER_ALIGNMENT, DRAW_BUFFER_SIZE);
    if (!draw_buffer) {
        return;
    }
    draw_ptr = draw_buffer;

    GX2SetRasterizerClipControl(TRUE, FALSE);

    GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
                       GX2_BLEND_COMBINE_MODE_ADD, FALSE, GX2_BLEND_MODE_ZERO, GX2_BLEND_MODE_ZERO,
                       GX2_BLEND_COMBINE_MODE_ADD);

    GX2Util::Init();
    gfx_wiiu_set_context_state();

    for (int i = 0; i < SHADER_MAX_TEXTURES; i++) {
        selected_textures[i] = nullptr;
        selected_fbs[i] = -1;
    }

    WHBLogPrintf("gfx_gx2_init: complete");
}

void gfx_gx2_shutdown(void) {
    if (has_foreground) {
        GX2DrawDone();

        if (depthReadBuffer.surface.image) {
            gfx_wiiu_free_mem1(depthReadBuffer.surface.image);
            depthReadBuffer.surface.image = nullptr;
        }

        for (auto& buffer : framebuffers) {
            if (buffer.texture.surface.image) {
                if (buffer.colorBufferMem1) {
                    gfx_wiiu_free_mem1(buffer.texture.surface.image);
                } else {
                    free(buffer.texture.surface.image);
                }
                buffer.texture.surface.image = nullptr;
            }

            if (buffer.depth_buffer.surface.image) {
                if (buffer.depthBufferMem1) {
                    gfx_wiiu_free_mem1(buffer.depth_buffer.surface.image);
                } else {
                    free(buffer.depth_buffer.surface.image);
                }
                buffer.depth_buffer.surface.image = nullptr;
            }
        }
    }

    if (draw_buffer) {
        free(draw_buffer);
        draw_buffer = nullptr;
        draw_ptr = nullptr;
    }

    GX2Util::Shutdown();
}

static void gfx_gx2_on_resize(void) {
}

static void gfx_gx2_start_frame(void) {
    // Restore state at frame start
    GX2SetViewport(current_viewport_x, current_viewport_y, current_viewport_width, current_viewport_height, 0.0f, 1.0f);
    GX2SetScissor(current_scissor_x, current_scissor_y, current_scissor_width, current_scissor_height);

    GX2SetColorControl(GX2_LOGIC_OP_COPY, current_use_alpha ? 0xff : 0, FALSE, TRUE);

    GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
                       GX2_BLEND_COMBINE_MODE_ADD, FALSE, GX2_BLEND_MODE_ZERO, GX2_BLEND_MODE_ZERO,
                       GX2_BLEND_COMBINE_MODE_ADD);

    GX2SetDepthOnlyControl(current_depth_test, current_depth_write, current_depth_compare_function);

    if (current_zmode_decal) {
        GX2SetPolygonOffset(current_SSDB, current_SSDB, current_SSDB, current_SSDB, 0.0f);
        GX2SetPolygonControl(GX2_FRONT_FACE_CCW, FALSE, FALSE, TRUE, GX2_POLYGON_MODE_TRIANGLE,
                             GX2_POLYGON_MODE_TRIANGLE, TRUE, TRUE, FALSE);
    } else {
        GX2SetPolygonOffset(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        GX2SetPolygonControl(GX2_FRONT_FACE_CCW, FALSE, FALSE, FALSE, GX2_POLYGON_MODE_TRIANGLE,
                             GX2_POLYGON_MODE_TRIANGLE, FALSE, FALSE, FALSE);
    }

    frame_count++;
}

static void gfx_gx2_end_frame(void) {
    draw_ptr = draw_buffer;

    Framebuffer& main_framebuffer = framebuffers[0];

    GX2CopyColorBufferToScanBuffer(&main_framebuffer.color_buffer, GX2_SCAN_TARGET_TV);
    GX2CopyColorBufferToScanBuffer(&main_framebuffer.color_buffer, GX2_SCAN_TARGET_DRC);
}

static void gfx_gx2_finish_render(void) {
}

static int gfx_gx2_create_framebuffer(void) {
    assert(used_framebuffers < framebuffers.size());

    std::size_t i = used_framebuffers;
    used_framebuffers++;

    Framebuffer& buffer = framebuffers[i];

    GX2InitSampler(&buffer.sampler, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    return i;
}

static void gfx_gx2_update_framebuffer_parameters(int fb, uint32_t width, uint32_t height, uint32_t msaa_level,
                                                  bool opengl_invert_y, bool render_target, bool has_depth_buffer,
                                                  bool can_extract_depth) {
    // we don't support updating the main buffer (fb 0)
    if (fb == 0) {
        return;
    }

    Framebuffer& buffer = framebuffers[fb];

    if (buffer.texture.surface.width == width && buffer.texture.surface.height == height) {
        return;
    }

    // make sure the GPU no longer writes to the buffer
    GX2DrawDone();

    if (buffer.texture.surface.image) {
        if (buffer.colorBufferMem1) {
            gfx_wiiu_free_mem1(buffer.texture.surface.image);
        } else {
            free(buffer.texture.surface.image);
        }
        buffer.texture.surface.image = nullptr;
    }

    if (buffer.depth_buffer.surface.image) {
        if (buffer.depthBufferMem1) {
            gfx_wiiu_free_mem1(buffer.depth_buffer.surface.image);
        } else {
            free(buffer.depth_buffer.surface.image);
        }
        buffer.depth_buffer.surface.image = nullptr;
    }

    gfx_gx2_init_framebuffer(&buffer, width, height);

    GX2CalcSurfaceSizeAndAlignment(&buffer.depth_buffer.surface);
    GX2InitDepthBufferRegs(&buffer.depth_buffer);

    buffer.depth_buffer.surface.image =
        gfx_wiiu_alloc_mem1(buffer.depth_buffer.surface.imageSize, buffer.depth_buffer.surface.alignment);
    // fall back to mem2
    if (!buffer.depth_buffer.surface.image) {
        buffer.depth_buffer.surface.image =
            memalign(buffer.depth_buffer.surface.alignment, buffer.depth_buffer.surface.imageSize);
        buffer.depthBufferMem1 = false;
    } else {
        buffer.depthBufferMem1 = true;
    }
    assert(buffer.depth_buffer.surface.image);

    GX2CalcSurfaceSizeAndAlignment(&buffer.color_buffer.surface);
    GX2InitColorBufferRegs(&buffer.color_buffer);

    memset(&buffer.texture, 0, sizeof(GX2Texture));
    buffer.texture.surface.use = GX2_SURFACE_USE_TEXTURE;
    buffer.texture.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    buffer.texture.surface.width = width;
    buffer.texture.surface.height = height;
    buffer.texture.surface.depth = 1;
    buffer.texture.surface.mipLevels = 1;
    buffer.texture.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    buffer.texture.surface.aa = GX2_AA_MODE1X;
    buffer.texture.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    buffer.texture.viewFirstMip = 0;
    buffer.texture.viewNumMips = 1;
    buffer.texture.viewFirstSlice = 0;
    buffer.texture.viewNumSlices = 1;
    buffer.texture.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);

    GX2CalcSurfaceSizeAndAlignment(&buffer.texture.surface);
    GX2InitTextureRegs(&buffer.texture);

    // the texture and color buffer share a buffer
    assert(buffer.color_buffer.surface.imageSize == buffer.texture.surface.imageSize);

    buffer.texture.surface.image =
        gfx_wiiu_alloc_mem1(buffer.texture.surface.imageSize, buffer.texture.surface.alignment);
    // fall back to mem2
    if (!buffer.texture.surface.image) {
        buffer.texture.surface.image = memalign(buffer.texture.surface.alignment, buffer.texture.surface.imageSize);
        buffer.colorBufferMem1 = false;
    } else {
        buffer.colorBufferMem1 = true;
    }
    assert(buffer.texture.surface.image);

    buffer.color_buffer.surface.image = buffer.texture.surface.image;
}

bool gfx_gx2_start_draw_to_framebuffer(int fb, float noise_scale) {
    Framebuffer& buffer = framebuffers[fb];

    if (noise_scale != 0.0f) {
        current_noise_scale = 1.0f / noise_scale;
    }

    // Re-establish context state before drawing each frame; required by GX2
    // for Cemu compatibility and proper GPU state restoration
    gfx_wiiu_set_context_state();

    GX2SetColorBuffer(&buffer.color_buffer, GX2_RENDER_TARGET_0);
    GX2SetDepthBuffer(&buffer.depth_buffer);

    current_framebuffer = fb;
    return true;
}

void gfx_gx2_clear_framebuffer(bool clear_color, bool clear_depth) {
    Framebuffer& buffer = framebuffers[current_framebuffer];

    if (clear_color) {
        GX2ClearColor(&buffer.color_buffer, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    if (clear_depth) {
        GX2ClearDepthStencilEx(&buffer.depth_buffer, buffer.depth_buffer.depthClear, buffer.depth_buffer.stencilClear,
                               GX2_CLEAR_FLAGS_BOTH);
    }

    gfx_wiiu_set_context_state();
}

void gfx_gx2_resolve_msaa_color_buffer(int fb_id_target, int fb_id_source) {
    Framebuffer& src_buffer = framebuffers[fb_id_source];
    Framebuffer& target_buffer = framebuffers[fb_id_target];

    if (src_buffer.color_buffer.surface.aa == GX2_AA_MODE1X) {
        GX2CopySurface(&src_buffer.color_buffer.surface, src_buffer.color_buffer.viewMip,
                       src_buffer.color_buffer.viewFirstSlice, &target_buffer.color_buffer.surface,
                       target_buffer.color_buffer.viewMip, target_buffer.color_buffer.viewFirstSlice);
    } else {
        GX2ResolveAAColorBuffer(&src_buffer.color_buffer, &target_buffer.color_buffer.surface,
                                target_buffer.color_buffer.viewMip, target_buffer.color_buffer.viewFirstSlice);
    }
}

void* gfx_gx2_get_framebuffer_texture_id(int fb_id) {
    Framebuffer& buffer = framebuffers[fb_id];

    return &buffer.texture;
}

void gfx_gx2_select_texture_fb(int fb) {
    if (fb < 0 || (size_t)fb >= used_framebuffers) {
        return;
    }

    Framebuffer& buffer = framebuffers[fb];
    selected_fbs[0] = fb;
    selected_textures[0] = nullptr;
    last_selected_tile = 0;

    if (current_shader_program) {
        int32_t sampler_location = current_shader_program->samplers_location[0];
        if (sampler_location != -1) {
            GX2SetPixelTexture(&buffer.texture, sampler_location);
            GX2SetPixelSampler(&buffer.sampler, sampler_location);
        }
    }
}

void gfx_gx2_copy_framebuffer(int fb_dst, int fb_src, int left, int top, bool flip_y, bool use_back) {
    if ((size_t)fb_dst >= used_framebuffers || (size_t)fb_src >= used_framebuffers) {
        return;
    }

    (void)flip_y;
    (void)use_back;

    GX2DrawDone();

    Framebuffer& dst_buffer = framebuffers[fb_dst];
    Framebuffer& src_buffer = framebuffers[fb_src];

    if (left < 0 || top < 0 || fb_src == 0) {
        // Scaled full copy or copy from tiled to linear surface
        GX2Util::ConvertSurface(&src_buffer.color_buffer.surface, &dst_buffer.color_buffer.surface);
    } else {
        int32_t fb_width = src_buffer.color_buffer.surface.width;
        int32_t fb_height = src_buffer.color_buffer.surface.height;

        // Clip source to destination bounds
        if (left + fb_width > (int32_t)dst_buffer.color_buffer.surface.width) {
            fb_width = (int32_t)dst_buffer.color_buffer.surface.width - left;
        }
        if (top + fb_height > (int32_t)dst_buffer.color_buffer.surface.height) {
            fb_height = (int32_t)dst_buffer.color_buffer.surface.height - top;
        }

        if (fb_width > 0 && fb_height > 0) {
            GX2Rect src = { 0, 0, fb_width, fb_height };
            GX2Point dst = { left, top };
            GX2CopySurfaceEx(&src_buffer.color_buffer.surface, 0, 0, &dst_buffer.color_buffer.surface, 0, 0, 1, &src, &dst);
        }
    }

    GX2DrawDone();
    gfx_wiiu_set_context_state();
}

void gfx_gx2_read_framebuffer_to_cpu(int fb_id, uint32_t width, uint32_t height, uint16_t* rgba16_buf) {
    if ((size_t)fb_id >= used_framebuffers) {
        return;
    }

    Framebuffer& buffer = framebuffers[fb_id];

    // Create a temporary linear surface in the correct format
    GX2Surface surface;
    memset(&surface, 0, sizeof(GX2Surface));
    surface.use = GX2_SURFACE_USE_TEXTURE;
    surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    surface.width = width;
    surface.height = height;
    surface.depth = 1;
    surface.mipLevels = 1;
    surface.format = GX2_SURFACE_FORMAT_UNORM_A1_B5_G5_R5; //GX2_SURFACE_FORMAT_UNORM_R5_G5_B5_A1;
    surface.aa = GX2_AA_MODE1X;
    surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    GX2CalcSurfaceSizeAndAlignment(&surface);

    surface.image = memalign(surface.alignment, surface.imageSize);
    DCFlushRange(surface.image, surface.imageSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, surface.image, surface.imageSize);

    GX2Util::ConvertSurface(&buffer.color_buffer.surface, &surface);
    GX2DrawDone();

    gfx_wiiu_set_context_state();

    for (uint32_t y = 0; y < height; y++) {
        memcpy(rgba16_buf + y * width, ((uint16_t*) surface.image) + y * surface.pitch, width * 2);
    }

    free(surface.image);
}

static std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
gfx_gx2_get_pixel_depth(int fb_id, const std::set<std::pair<float, float>>& coordinates) {
    Framebuffer& buffer = framebuffers[fb_id];

    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff> res;
    GX2Rect srcRects[25];
    GX2Point dstPoints[25];
    size_t num_coordinates = coordinates.size();
    while (num_coordinates > 0) {
        size_t numRects = 25;
        if (num_coordinates < numRects) {
            numRects = num_coordinates;
        }
        num_coordinates -= numRects;

        // initialize rects and points
        for (size_t i = 0; i < numRects; ++i) {
            const auto& c = *std::next(coordinates.begin(), num_coordinates + i);
            const int32_t x = (int32_t)std::clamp(c.first, 0.0f, (float)(buffer.depth_buffer.surface.width - 1));
            const int32_t y = (int32_t)std::clamp(c.second, 0.0f, (float)(buffer.depth_buffer.surface.height - 1));

            srcRects[i] = GX2Rect{ x, (int32_t)buffer.depth_buffer.surface.height - y, x + 1,
                                   (int32_t)(buffer.depth_buffer.surface.height - y) + 1 };

            // dst points will be spread over the x-axis of the buffer
            dstPoints[i] = GX2Point{ (int32_t)i, 0 };
        }

        // Invalidate the buffer first
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_DEPTH_BUFFER, depthReadBuffer.surface.image,
                      depthReadBuffer.surface.imageSize);

        // Perform the copy
        GX2CopySurfaceEx(&buffer.depth_buffer.surface, 0, 0, &depthReadBuffer.surface, 0, 0, numRects, srcRects,
                         dstPoints);

        // Wait for draws to be done and restore context, in case GPU was used
        GX2DrawDone();
        gfx_wiiu_set_context_state();

        // read the pixels from the depthReadBuffer
        for (size_t i = 0; i < numRects; ++i) {
            uint32_t tmp = __builtin_bswap32(*((uint32_t*)depthReadBuffer.surface.image + i));
            float val = std::bit_cast<float>(tmp);

            const auto& c = *std::next(coordinates.begin(), num_coordinates + i);
            res.emplace(c, val * 65532.0f);
        }
    }

    return res;
}

void gfx_gx2_set_texture_filter(FilteringMode mode) {
    // three-point is not implemented in the shaders yet
    if (mode == FILTER_THREE_POINT) {
        mode = FILTER_LINEAR;
    }

    current_filter_mode = mode;
    gfx_texture_cache_clear();
}

FilteringMode gfx_gx2_get_texture_filter(void) {
    return current_filter_mode;
}

// Stub for clear_shaders since we don't support it
static void gfx_gx2_clear_shaders(void) {
    // Not implemented
}

struct GfxRenderingAPI gfx_gx2_api = {
    .get_name = gfx_gx2_get_name,
    .get_max_texture_size = gfx_gx2_get_max_texture_size,
    .get_clip_parameters = gfx_gx2_get_clip_parameters,
    .unload_shader = gfx_gx2_unload_shader,
    .load_shader = gfx_gx2_load_shader,
    .create_and_load_new_shader = gfx_gx2_create_and_load_new_shader,
    .lookup_shader = gfx_gx2_lookup_shader,
    .shader_get_info = gfx_gx2_shader_get_info,
    .clear_shaders = gfx_gx2_clear_shaders,
    .new_texture = gfx_gx2_new_texture,
    .select_texture = gfx_gx2_select_texture,
    .upload_texture = gfx_gx2_upload_texture,
    .set_sampler_parameters = gfx_gx2_set_sampler_parameters,
    .set_depth_mode = gfx_gx2_set_depth_mode,
    .set_depth_range = gfx_gx2_set_depth_range,
    .set_viewport = gfx_gx2_set_viewport,
    .set_scissor = gfx_gx2_set_scissor,
    .set_use_alpha = gfx_gx2_set_use_alpha,
    .draw_triangles = gfx_gx2_draw_triangles,
    .init = gfx_gx2_init,
    .on_resize = gfx_gx2_on_resize,
    .start_frame = gfx_gx2_start_frame,
    .end_frame = gfx_gx2_end_frame,
    .finish_render = gfx_gx2_finish_render,
    .create_framebuffer = gfx_gx2_create_framebuffer,
    .update_framebuffer_parameters = gfx_gx2_update_framebuffer_parameters,
    .start_draw_to_framebuffer = gfx_gx2_start_draw_to_framebuffer,
    .copy_framebuffer = gfx_gx2_copy_framebuffer,
    .clear_framebuffer = gfx_gx2_clear_framebuffer,
    .resolve_msaa_color_buffer = gfx_gx2_resolve_msaa_color_buffer,
    .get_framebuffer_texture_id = gfx_gx2_get_framebuffer_texture_id,
    .select_texture_fb = gfx_gx2_select_texture_fb,
    .delete_texture = gfx_gx2_delete_texture,
    .set_texture_filter = gfx_gx2_set_texture_filter,
    .get_texture_filter = gfx_gx2_get_texture_filter,
};

#endif
