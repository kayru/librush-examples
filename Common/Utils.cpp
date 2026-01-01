#include "Utils.h"

#include <Rush/UtilFile.h>
#include <Rush/UtilCamera.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace Rush
{

namespace
{
	u32 parseU32(const char* value, u32 defaultValue)
	{
		if (!value)
		{
			return defaultValue;
		}

		char* end = nullptr;
		long parsed = std::strtol(value, &end, 10);
		if (end == value || parsed <= 0)
		{
			return defaultValue;
		}

		return static_cast<u32>(parsed);
	}

	std::string trimQuotes(std::string_view text)
	{
		if (text.size() >= 2 && ((text.front() == '"' && text.back() == '"') || (text.front() == '\'' && text.back() == '\'')))
		{
			return std::string(text.substr(1, text.size() - 2));
		}
		return std::string(text);
	}

	bool matchArgValue(const char* arg, const char* longKey, const char* shortKey, const char*& value)
	{
		if (!arg || (!longKey && !shortKey))
		{
			return false;
		}

		if (std::strncmp(arg, "--", 2) != 0)
		{
			return false;
		}

		const char* name = arg + 2;
		const char* eq = std::strchr(name, '=');
		if (!eq)
		{
			return false;
		}

		const size_t nameLen = size_t(eq - name);
		if (longKey && std::strlen(longKey) == nameLen && std::strncmp(name, longKey, nameLen) == 0)
		{
			value = eq + 1;
			return true;
		}

		if (shortKey && std::strlen(shortKey) == nameLen && std::strncmp(name, shortKey, nameLen) == 0)
		{
			value = eq + 1;
			return true;
		}

		return false;
	}

	bool findArgValue(int argc, char** argv, const char* longKey, const char* shortKey, const char*& value)
	{
		const char* lastValue = nullptr;

		for (int i = 1; i < argc; ++i)
		{
			const char* arg = argv[i];
			if (!arg)
			{
				continue;
			}

			const char* foundValue = nullptr;
			if (matchArgValue(arg, longKey, shortKey, foundValue))
			{
				if (foundValue)
				{
					lastValue = foundValue;
				}
				continue;
			}
		}

		value = lastValue;
		return value != nullptr;
	}

}

bool endsWith(const char* str, const char* suffix)
{
	size_t len1 = strlen(str);
	size_t len2 = strlen(suffix);

	if (len1 < len2)
	{
		return false;
	}

	return !strcmp(str + len1 - len2, suffix);
}

void fixDirectorySeparatorsInplace(std::string& path)
{
	for(char& c : path)
	{
		if (c == '\\')
		{
			c = '/';
		}
	}
}

std::string sanitizeFilename(std::string_view name, const char* fallback)
{
	if (name.empty())
	{
		return fallback ? fallback : "Unnamed";
	}

	std::string normalized(name);
	for (char& c : normalized)
	{
		const bool ascii = static_cast<unsigned char>(c) < 0x80;
		const bool ok = ascii && (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.');
		if (!ok)
		{
			c = '_';
		}
	}

	return normalized;
}

std::string toLower(std::string_view text)
{
	std::string out(text);
	for (char& c : out)
	{
		if (c >= 'A' && c <= 'Z')
		{
			c = char(c - 'A' + 'a');
		}
	}
	return out;
}

bool getArgString(int argc, char** argv, const char* longKey, const char* shortKey, std::string& out)
{
	const char* value = nullptr;
	if (!findArgValue(argc, argv, longKey, shortKey, value) || !value)
	{
		return false;
	}

	out = trimQuotes(value);
	return true;
}

bool getArgU32(int argc, char** argv, const char* longKey, const char* shortKey, u32& out)
{
	const char* value = nullptr;
	if (!findArgValue(argc, argv, longKey, shortKey, value))
	{
		return false;
	}

	out = parseU32(value, out);
	return true;
}

bool getPositionalArg(int argc, char** argv, int position, const char*& value)
{
	if (position < 0)
	{
		return false;
	}

	int current = 0;
	bool treatAsPositional = false;
	for (int i = 1; i < argc; ++i)
	{
		const char* arg = argv[i];
		if (!arg)
		{
			continue;
		}

		if (!treatAsPositional && std::strcmp(arg, "--") == 0)
		{
			treatAsPositional = true;
			continue;
		}

		if (!treatAsPositional && arg[0] == '-')
		{
			continue;
		}

		if (current == position)
		{
			value = arg;
			return true;
		}

		++current;
	}

	return false;
}

HumanFriendlyValue getHumanFriendlyValue(double v)
{
	if (v >= 1e9)
	{
		return HumanFriendlyValue{ v / 1e9, "Billion" };
	}
	else if (v >= 1e6)
	{
		return HumanFriendlyValue{ v / 1e6, "Million" };
	}
	else if (v >= 1e3)
	{
		return HumanFriendlyValue{ v / 1e3, "Thousand" };
	}
	else
	{
		return HumanFriendlyValue{ v, "" };
	}
}

HumanFriendlyValue getHumanFriendlyValueShort(double v)
{
	if (v >= 1e9)
	{
		return HumanFriendlyValue{ v / 1e9, "B" };
	}
	else if (v >= 1e6)
	{
		return HumanFriendlyValue{ v / 1e6, "M" };
	}
	else if (v >= 1e3)
	{
		return HumanFriendlyValue{ v / 1e3, "K" };
	}
	else
	{
		return HumanFriendlyValue{ v, "" };
	}
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

	if (endsWith(fullFilename, ".metallib"))
	{
		source.type = GfxShaderSourceType::GfxShaderSourceType_MSL_BIN;
		source.entry = "main0";
	}
	else if (endsWith(fullFilename, ".metal"))
	{
		source.type = GfxShaderSourceType::GfxShaderSourceType_MSL;
		source.entry = "main0";
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
		RUSH_LOG_ERROR("Failed to load shader '%s'", fullFilename);
	}

	return source;
}

void interpolateCamera(
    Camera& camera, const Camera& target, float deltaTime, float positionSmoothing, float rotationSmoothing)
{
	float t1 = 1.0f - float(pow(pow(positionSmoothing, 60.0f), deltaTime));
	float t2 = 1.0f - float(pow(pow(rotationSmoothing, 60.0f), deltaTime));
	camera.blendTo(target, t1, t2);
}

TexturedQuad2D makeFullScreenQuad()
{
	TexturedQuad2D q;

	q.pos[0] = Vec2(-1.0f, 1.0f);
	q.pos[1] = Vec2(1.0f, 1.0f);
	q.pos[2] = Vec2(1.0f, -1.0f);
	q.pos[3] = Vec2(-1.0f, -1.0f);

	q.tex[0] = Vec2(0.0f, 0.0f);
	q.tex[1] = Vec2(1.0f, 0.0);
	q.tex[2] = Vec2(1.0f, 1.0f);
	q.tex[3] = Vec2(0.0f, 1.0f);

	return q;
}

}
