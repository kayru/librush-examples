#pragma once

#include <Rush/Platform.h>
#include <Rush/GfxCommon.h>

namespace Rush
{

GfxShaderSource loadShaderFromFile(
    const char* filename, const char* shaderDirectory = Platform_GetExecutableDirectory());

}
