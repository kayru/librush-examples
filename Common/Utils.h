#pragma once

#include <Rush/Platform.h>
#include <Rush/GfxCommon.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/UtilDataStream.h>
#include <vector>
#include <string>

namespace Rush
{

class Camera;

#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
#define RUSH_SHADER_NAME(x) x ".metal"
#else
#define RUSH_SHADER_NAME(x) x ".bin"
#endif

GfxShaderSource loadShaderFromFile(
    const char* filename, const char* shaderDirectory = Platform_GetExecutableDirectory());

template <typename T, size_t SIZE> struct MovingAverage
{
	MovingAverage() { reset(); }

	inline void reset()
	{
		idx = 0;
		sum = 0;
		size = 0;
		for (size_t i = 0; i < SIZE; ++i)
		{
			buf[i] = 0;
		}
	}

	inline void add(T v)
	{
		sum += v;
		sum -= buf[idx];
		buf[idx] = v;
		idx      = (idx + 1) % SIZE;
		if (size < SIZE)
		{
			size++;
		}
	}

	inline T get() const { return sum / max(size_t(1), size); }

	size_t idx;
	size_t size;
	T      sum;
	T      buf[SIZE];
};

template <typename T> static void writeContainer(DataStream& stream, const std::vector<T>& data)
{
	u32 count = (u32)data.size();
	stream.writeT(count);
	stream.write(data.data(), count * (u32)sizeof(T));
}

template <typename T> static void readContainer(DataStream& stream, std::vector<T>& data)
{
	u32 count = 0;
	stream.readT(count);
	data.clear();
	data.resize(count);
	stream.read(data.data(), count * (u32)sizeof(T));
}

inline std::string directoryFromFilename(const std::string& filename)
{
	size_t pos = filename.find_last_of("/\\");
	if (pos != std::string::npos)
	{
		return filename.substr(0, pos + 1);
	}
	else
	{
		return std::string();
	}
}

void fixDirectorySeparatorsInplace(std::string& path);

bool endsWith(const char* str, const char* suffix);

struct HumanFriendlyValue
{
	double value;
	const char* unit;
};

HumanFriendlyValue getHumanFriendlyValue(double v);
HumanFriendlyValue getHumanFriendlyValueShort(double v);

void interpolateCamera(Camera& camera, const Camera& target, float deltaTime, float positionSmoothing = 0.9f,
    float rotationSmoothing = 0.85f);

TexturedQuad2D makeFullScreenQuad();

} // namespace Rush
