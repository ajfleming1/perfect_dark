#include "gx2_util.h"
#include "gfx_wiiu.h"

#include <malloc.h>
#include <whb/gfx.h>
#include <gx2/context.h>
#include <gx2/clear.h>
#include <gx2/registers.h>
#include <gx2/texture.h>
#include <gx2/draw.h>
#include <gx2/event.h>
#include <gx2/utils.h>
#include <coreinit/debug.h>

#include "gx2_shaders/conv.inc"

namespace
{

const float vertices[][2]= {
    { -1.0f,  1.0f },
    { -1.0f, -1.0f },
    {  1.0f,  1.0f },
    {  1.0f, -1.0f }
};

GX2ContextState* contextState = nullptr;

WHBGfxShaderGroup convShader = {};

GX2Sampler sampler = {};

bool CreateColorBufferForSurface(GX2ColorBuffer& colorBuffer, const GX2Surface* surface)
{
    colorBuffer = {};
    colorBuffer.surface = *surface;
    colorBuffer.surface.use |= GX2_SURFACE_USE_COLOR_BUFFER;
    colorBuffer.viewNumSlices = 1;
    GX2CalcSurfaceSizeAndAlignment(&colorBuffer.surface);
    GX2InitColorBufferRegs(&colorBuffer);

    // Make sure this matches
    if (colorBuffer.surface.imageSize != surface->imageSize ||
        colorBuffer.surface.alignment != surface->alignment) {
        OSReport("CreateColorBufferForSurface failed\n");
        return false;
    }

    return true;
}

bool CreateTextureForSurface(GX2Texture& texture, const GX2Surface* surface)
{
    texture = {};
    texture.surface = *surface;
    texture.surface.use |= GX2_SURFACE_USE_TEXTURE;
    texture.viewNumMips = 1;
    texture.viewNumSlices = 1;
    texture.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);
    GX2CalcSurfaceSizeAndAlignment(&texture.surface);
    GX2InitTextureRegs(&texture);

    // Make sure this matches
    if (texture.surface.imageSize != surface->imageSize ||
        texture.surface.alignment != surface->alignment) {
        OSReport("CreateTextureForSurface failed\n");
        return false;
    }

    return true;
}

} // namespace

bool GX2Util::Init()
{
    // Set up a custom context state
    contextState = (GX2ContextState*) memalign(GX2_CONTEXT_STATE_ALIGNMENT, sizeof(GX2ContextState));
    if (!contextState) {
        return false;
    }

    GX2SetupContextStateEx(contextState, TRUE);
    GX2SetContextState(contextState);

    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
    GX2SetBlendControl(GX2_RENDER_TARGET_0,
        GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO,
        GX2_BLEND_COMBINE_MODE_ADD,
        TRUE,
        GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO,
        GX2_BLEND_COMBINE_MODE_ADD
    );
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);

    // Set up shaders
    if (!WHBGfxLoadGFDShaderGroup(&convShader, 0, conv_gsh)) {
        OSReport("WHBGfxLoadGFDShaderGroup failed\n");
        return false;
    }

    WHBGfxInitShaderAttribute(&convShader, "Position", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitFetchShader(&convShader);

    GX2InitSampler(&sampler, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_XY_FILTER_MODE_LINEAR);

    return true;
}

bool GX2Util::Shutdown()
{
    WHBGfxFreeShaderGroup(&convShader);

    free(contextState);

    return true;
}

bool GX2Util::ConvertSurface(const GX2Surface *src, GX2Surface *dst)
{
    // Create a texture for the source
    GX2Texture texture;
    if (!CreateTextureForSurface(texture, src)) {
        return false;
    }

    // Create a colorbuffer for the destination
    GX2ColorBuffer colorBuffer;
    if (!CreateColorBufferForSurface(colorBuffer, dst)) {
        return false;
    }

    GX2SetContextState(contextState);

    GX2SetFetchShader(&convShader.fetchShader);
    GX2SetVertexShader(convShader.vertexShader);
    GX2SetPixelShader(convShader.pixelShader);

    GX2SetPixelTexture(&texture, 0);
    GX2SetPixelSampler(&sampler, 0);

    GX2SetColorBuffer(&colorBuffer, GX2_RENDER_TARGET_0);
    GX2SetViewport(0.0f, 0.0f, dst->width, dst->height, 0.0f, 1.0f);
    GX2SetScissor(0.0f, 0.0f, dst->width, dst->height);

    GX2SetAttribBuffer(0, sizeof(vertices), 2 * sizeof(float), vertices);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, 4, 0, 1);

    GX2DrawDone();
    gfx_wiiu_set_context_state();

    return true;
}
