#include "Utils.h"

#include <Rush/UtilFile.h>

namespace Rush
{

inline bool endsWith(const char* str, const char* suffix)
{
	size_t len1 = strlen(str);
	size_t len2 = strlen(suffix);

	if (len1 < len2)
	{
		return false;
	}

	return !strcmp(str + len1 - len2, suffix);
}

GfxShaderSource loadShaderFromFile(const char* filename, const char* shaderDirectory)
{
	const char* fullFilename = filename;
	char        fullFilenameBuffer[2048];
	if (shaderDirectory)
	{
		snprintf(fullFilenameBuffer, sizeof(fullFilenameBuffer), "%s/%s", shaderDirectory, filename);
		fullFilename = fullFilenameBuffer;
	}

	GfxShaderSource source;

	// TODO: auto-detect shader type from file header and check against gfx device caps

	bool isText = false;

	if (endsWith(fullFilename, ".metal"))
	{
		source.type = GfxShaderSourceType::GfxShaderSourceType_Metal;
		isText      = true;
	}
	else if (endsWith(fullFilename, ".hlsl"))
	{
		source.type = GfxShaderSourceType::GfxShaderSourceType_HLSL;
		isText      = true;
	}
	else
	{
#if RUSH_RENDER_API == RUSH_RENDER_API_DX11 || RUSH_RENDER_API == RUSH_RENDER_API_DX12
		source.type = GfxShaderSourceType::GfxShaderSourceType_DXBC;
#else
		source.type = GfxShaderSourceType::GfxShaderSourceType_SPV;
#endif
	}

	FileIn file(fullFilename);
	if (file.valid())
	{
		auto fileSize = file.length();
		source.resize(fileSize + (isText ? 1 : 0), 0);
		file.read(source.data(), fileSize);
	}
	else
	{
		Log::error("Failed to load shader '%s'", fullFilename);
	}

	return source;
}
}
