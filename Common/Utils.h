#pragma once

#include <Rush/Platform.h>
#include <Rush/GfxCommon.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/UtilDataStream.h>
#include <Rush/UtilTimer.h>
#include <vector>
#include <string>
#include <filesystem>
#include <string_view>

namespace Rush
{

class Camera;

#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
#define RUSH_SHADER_NAME(x) x ".metallib"
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

struct TimingScope
{
	TimingScope(MovingAverage<double, 60>& output) : m_output(output) {}
	~TimingScope() { m_output.add(m_timer.time()); }

	MovingAverage<double, 60>& m_output;
	Timer                      m_timer;
};

template <typename T> static void writeContainer(DataStream& stream, const std::vector<T>& data)
{
	const u32 count = (u32)data.size();
	stream.writeT(count);
	stream.write(data.data(), u64(count) * sizeof(T));
}

// Returns false (leaving the container empty) when the stream does not hold `count` elements,
// so a corrupt count can neither over-allocate nor silently yield zero-filled elements.
template <typename T> static bool readContainer(DataStream& stream, std::vector<T>& data)
{
	u32 count = 0;
	stream.readT(count);
	data.clear();
	const u64 byteSize = u64(count) * sizeof(T);
	if (stream.tell() + byteSize > stream.length())
	{
		return false;
	}
	data.resize(count);
	return stream.read(data.data(), byteSize) == byteSize;
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

std::string sanitizeFilename(std::string_view name, const char* fallback = "Unnamed");
std::string toLower(std::string_view text);
bool getArgString(int argc, char** argv, const char* longKey, const char* shortKey, std::string& out);
bool getArgU32(int argc, char** argv, const char* longKey, const char* shortKey, u32& out);
bool getPositionalArg(int argc, char** argv, int position, const char*& value);

struct HumanFriendlyValue
{
	double value;
	const char* unit;
};

HumanFriendlyValue getHumanFriendlyValue(double v);
HumanFriendlyValue getHumanFriendlyValueShort(double v);

void interpolateCamera(Camera& camera, const Camera& target, float deltaTime, float positionSmoothing = 0.9f,
    float rotationSmoothing = 0.85f);

Camera makeFramedCamera(const Box3& bounds, float aspect, float fov = 1.0f, float nearClip = 0.25f);

// Loads an image into a full RGBA8 mip chain. Threadsafe; returns false if the file won't load.
bool loadImageWithMips(const char* filename, GfxFormat format, GfxTextureDesc& outDesc, std::vector<u8> (&outMips)[16]);

// Per-scene config path next to the executable: "<tag>_config_<hash>.bin", hashed
// from the model path (resolved against cwd). modelFilename may be null/empty.
std::string sceneConfigPath(const char* tag, const char* modelFilename);

TexturedQuad2D makeFullScreenQuad();

struct ProceduralSceneVertex
{
	Vec3 position;
	Vec3 normal;
	Vec2 texcoord;
	Vec3 tangent;
	Vec3 bitangent;
};

struct ProceduralSceneMaterial
{
	Vec4        baseColor = Vec4(1.0f);
	Vec3        emissive = Vec3(0.0f); // emitted radiance; nonzero on area-light surfaces
	std::string diffuseTextureName; // empty when untextured
	std::string roughnessTextureName; // grayscale roughness (native models)
	std::string normalTextureName;
};

struct ProceduralSceneSegment
{
	u32 material = 0;
	u32 indexOffset = 0;
	u32 indexCount = 0;
};

struct ProceduralSceneData
{
	std::vector<ProceduralSceneVertex> vertices;
	std::vector<u32> indices;
	std::vector<ProceduralSceneSegment> segments;
	std::vector<ProceduralSceneMaterial> materials;
	Box3 bounds = Box3(Vec3(0.0f), Vec3(0.0f));

	bool hasAreaLight = false;
	Vec3 lightOrigin = Vec3(0.0f);
	Vec3 lightEdgeU = Vec3(0.0f);
	Vec3 lightEdgeV = Vec3(0.0f);
	Vec3 lightEmission = Vec3(0.0f);
};

enum class ProceduralScene : u32
{
	CornellBox, // authentic self-lit Cornell Box (default)
	BoxOnPlane, // simple diffuse cube on a ground plane
};

void buildProceduralScene(ProceduralSceneData& out, ProceduralScene kind = ProceduralScene::CornellBox);

// Loads a Wavefront OBJ into the neutral scene representation (X-mirrored to engine convention,
// one segment per material run, raw material ids). Returns false on load failure.
bool loadObjScene(const char* filename, ProceduralSceneData& out);

// Loads a native .model into the neutral scene representation (texture paths resolved + normalized).
bool loadModelScene(const char* filename, ProceduralSceneData& out);

// Dispatches to the loader matching the file extension (.obj or .model; glTF is app-specific).
bool loadSceneFromFile(const char* filename, ProceduralSceneData& out);

} // namespace Rush
