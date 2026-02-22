#pragma once

#include <gx2/surface.h>

namespace GX2Util
{

bool Init();

bool Shutdown();

bool ConvertSurface(const GX2Surface *src, GX2Surface *dst);

}; // namespace GX2Util
