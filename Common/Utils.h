#pragma once

#include <Rush/Platform.h>
#include <Rush/GfxCommon.h>
#include <Rush/UtilDataStream.h>
#include <vector>
#include <string>

namespace Rush
{

GfxShaderSource loadShaderFromFile(
    const char* filename, const char* shaderDirectory = Platform_GetExecutableDirectory());

template <typename T, size_t SIZE> struct MovingAverage
{
	MovingAverage() { reset(); }

	inline void reset()
	{
		idx = 0;
		sum = 0;
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
	}

	inline T get() const { return sum / SIZE; }

	size_t idx;
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

bool endsWith(const char* str, const char* suffix);

}
