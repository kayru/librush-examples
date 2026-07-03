#include "Utils.h"

#include <Rush/UtilFile.h>
#include <Rush/UtilCamera.h>
#include <Rush/UtilHash.h>
#include <Rush/UtilLog.h>

#include <stb_image.h>
#include <stb_image_resize.h>
#include <tiny_obj_loader.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
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
		if (end == value || parsed < 0)
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

	bool matchArgKey(const char* arg, const char* key)
	{
		if (!arg || !key)
		{
			return false;
		}

		if (std::strncmp(arg, "--", 2) != 0)
		{
			return false;
		}

		const char* name = arg + 2;
		return std::strcmp(name, key) == 0;
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

			if (matchArgKey(arg, longKey) || matchArgKey(arg, shortKey))
			{
				const char* next = (i + 1 < argc) ? argv[i + 1] : nullptr;
				if (next && next[0] != '-')
				{
					lastValue = next;
				}
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

Camera makeFramedCamera(const Box3& bounds, float aspect, float fov, float nearClip)
{
	Camera camera(aspect, fov, nearClip);
	camera.lookAt(Vec3(bounds.m_max) + Vec3(2.0f), bounds.center());
	return camera;
}

bool loadImageWithMips(const char* filename, GfxFormat format, GfxTextureDesc& outDesc, std::vector<u8> (&outMips)[16])
{
	int w = 0, h = 0, comp = 0;
	u8* pixels = stbi_load(filename, &w, &h, &comp, 4);
	if (!pixels)
	{
		return false;
	}

	u32 mipIndex = 0;

	{
		const u32 levelSize = w * h * 4;
		outMips[mipIndex].resize(levelSize);
		memcpy(outMips[mipIndex].data(), pixels, levelSize);
		mipIndex++;
	}

	u32 mipWidth  = w;
	u32 mipHeight = h;
	while (mipWidth != 1 && mipHeight != 1)
	{
		const u32 nextMipWidth  = max<u32>(1, mipWidth / 2);
		const u32 nextMipHeight = max<u32>(1, mipHeight / 2);

		outMips[mipIndex].resize(nextMipWidth * nextMipHeight * 4);

		const int resizeResult = stbir_resize_uint8(outMips[mipIndex - 1].data(), mipWidth, mipHeight, mipWidth * 4,
		    outMips[mipIndex].data(), nextMipWidth, nextMipHeight, nextMipWidth * 4, 4);
		RUSH_ASSERT(resizeResult);

		mipIndex++;
		mipWidth  = nextMipWidth;
		mipHeight = nextMipHeight;
	}

	outDesc      = GfxTextureDesc::make2D(w, h, format);
	outDesc.mips = mipIndex;

	free(pixels);
	return true;
}

std::string sceneConfigPath(const char* tag, const char* modelFilename)
{
	std::string key;
	if (!modelFilename || !*modelFilename)
	{
		key = "procedural";
	}
	else
	{
		std::error_code ec;
		const std::filesystem::path canonical = std::filesystem::weakly_canonical(modelFilename, ec);
		key = ec ? std::string(modelFilename) : canonical.generic_string();
#if defined(_WIN32)
		key = toLower(key);
#endif
	}

	const u64 hash = hashStrFnv1a64(key.c_str());

	char name[64];
	snprintf(name, sizeof(name), "%s_config_%016llx.bin", tag, static_cast<unsigned long long>(hash));

	return std::string(Platform_GetExecutableDirectory()) + "/" + name;
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

void buildProceduralScene(ProceduralSceneData& out)
{
	out.vertices.clear();
	out.indices.clear();
	out.segments.clear();
	out.materials.clear();

	ProceduralSceneMaterial planeMaterial;
	planeMaterial.baseColor = Vec4(0.7f, 0.7f, 0.7f, 1.0f);
	out.materials.push_back(planeMaterial);

	ProceduralSceneMaterial cubeMaterial = planeMaterial;
	cubeMaterial.baseColor = Vec4(0.2f, 0.6f, 1.0f, 1.0f);
	out.materials.push_back(cubeMaterial);

	auto addFace = [&out](const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& v3,
		const Vec3& normal, const Vec3& tangent, u32 material)
	{
		const u32 baseIndex = u32(out.vertices.size());
		const Vec3 bitangent = cross(normal, tangent);

		ProceduralSceneVertex verts[4];
		verts[0] = { v0, normal, Vec2(0.0f, 0.0f), tangent, bitangent };
		verts[1] = { v1, normal, Vec2(1.0f, 0.0f), tangent, bitangent };
		verts[2] = { v2, normal, Vec2(1.0f, 1.0f), tangent, bitangent };
		verts[3] = { v3, normal, Vec2(0.0f, 1.0f), tangent, bitangent };
		out.vertices.insert(out.vertices.end(), std::begin(verts), std::end(verts));

		out.indices.push_back(baseIndex + 0);
		out.indices.push_back(baseIndex + 1);
		out.indices.push_back(baseIndex + 2);
		out.indices.push_back(baseIndex + 0);
		out.indices.push_back(baseIndex + 2);
		out.indices.push_back(baseIndex + 3);

		if (out.segments.empty() || out.segments.back().material != material)
		{
			ProceduralSceneSegment segment;
			segment.material = material;
			segment.indexOffset = u32(out.indices.size()) - 6;
			segment.indexCount = 6;
			out.segments.push_back(segment);
		}
		else
		{
			out.segments.back().indexCount += 6;
		}
	};

	const float planeSize = 4.0f;
	addFace(Vec3(-planeSize, 0.0f, -planeSize),
	        Vec3(-planeSize, 0.0f,  planeSize),
	        Vec3( planeSize, 0.0f,  planeSize),
	        Vec3( planeSize, 0.0f, -planeSize),
	        Vec3(0.0f, 1.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), 0);

	const float cubeSize = 1.0f;
	const Vec3 c(0.0f, 0.75f, 0.0f);
	const float hs = cubeSize * 0.5f;

	addFace(Vec3(c.x - hs, c.y - hs, c.z + hs),
	        Vec3(c.x + hs, c.y - hs, c.z + hs),
	        Vec3(c.x + hs, c.y + hs, c.z + hs),
	        Vec3(c.x - hs, c.y + hs, c.z + hs),
	        Vec3(0.0f, 0.0f, 1.0f), Vec3(1.0f, 0.0f, 0.0f), 1);

	addFace(Vec3(c.x + hs, c.y - hs, c.z - hs),
	        Vec3(c.x - hs, c.y - hs, c.z - hs),
	        Vec3(c.x - hs, c.y + hs, c.z - hs),
	        Vec3(c.x + hs, c.y + hs, c.z - hs),
	        Vec3(0.0f, 0.0f, -1.0f), Vec3(-1.0f, 0.0f, 0.0f), 1);

	addFace(Vec3(c.x - hs, c.y - hs, c.z - hs),
	        Vec3(c.x - hs, c.y - hs, c.z + hs),
	        Vec3(c.x - hs, c.y + hs, c.z + hs),
	        Vec3(c.x - hs, c.y + hs, c.z - hs),
	        Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), 1);

	addFace(Vec3(c.x + hs, c.y - hs, c.z + hs),
	        Vec3(c.x + hs, c.y - hs, c.z - hs),
	        Vec3(c.x + hs, c.y + hs, c.z - hs),
	        Vec3(c.x + hs, c.y + hs, c.z + hs),
	        Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), 1);

	addFace(Vec3(c.x - hs, c.y + hs, c.z + hs),
	        Vec3(c.x + hs, c.y + hs, c.z + hs),
	        Vec3(c.x + hs, c.y + hs, c.z - hs),
	        Vec3(c.x - hs, c.y + hs, c.z - hs),
	        Vec3(0.0f, 1.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), 1);

	addFace(Vec3(c.x - hs, c.y - hs, c.z - hs),
	        Vec3(c.x + hs, c.y - hs, c.z - hs),
	        Vec3(c.x + hs, c.y - hs, c.z + hs),
	        Vec3(c.x - hs, c.y - hs, c.z + hs),
	        Vec3(0.0f, -1.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), 1);

	if (!out.vertices.empty())
	{
		out.bounds = Box3(out.vertices[0].position, out.vertices[0].position);
		for (const auto& v : out.vertices)
		{
			out.bounds.expand(v.position);
		}
	}
}

bool loadObjScene(const char* filename, ProceduralSceneData& out)
{
	std::vector<tinyobj::shape_t>    shapes;
	std::vector<tinyobj::material_t> materials;
	std::string                      errors;

	const std::string directory = directoryFromFilename(filename);

	if (!tinyobj::LoadObj(shapes, materials, errors, filename, directory.c_str()))
	{
		RUSH_LOG_ERROR("OBJ loader error: %s", errors.c_str());
		return false;
	}

	out.materials.reserve(materials.size());
	for (const auto& objMaterial : materials)
	{
		ProceduralSceneMaterial mat;
		mat.baseColor = Vec4(objMaterial.diffuse[0], objMaterial.diffuse[1], objMaterial.diffuse[2], 1.0f);
		if (!objMaterial.diffuse_texname.empty())
		{
			mat.diffuseTextureName = directory + objMaterial.diffuse_texname;
			fixDirectorySeparatorsInplace(mat.diffuseTextureName);
		}
		out.materials.push_back(std::move(mat));
	}

	out.bounds.expandInit();

	for (const auto& shape : shapes)
	{
		const u32   firstVertex = (u32)out.vertices.size();
		const auto& mesh        = shape.mesh;

		const u32  vertexCount   = (u32)mesh.positions.size() / 3;
		const bool haveTexcoords = !mesh.texcoords.empty();
		const bool haveNormals   = mesh.positions.size() == mesh.normals.size();

		for (u32 i = 0; i < vertexCount; ++i)
		{
			ProceduralSceneVertex v = {};

			v.position.x = mesh.positions[i * 3 + 0];
			v.position.y = mesh.positions[i * 3 + 1];
			v.position.z = mesh.positions[i * 3 + 2];

			out.bounds.expand(v.position);

			if (haveTexcoords)
			{
				v.texcoord.x = mesh.texcoords[i * 2 + 0];
				v.texcoord.y = 1.0f - mesh.texcoords[i * 2 + 1];
			}

			if (haveNormals)
			{
				v.normal.x = mesh.normals[i * 3 + 0];
				v.normal.y = mesh.normals[i * 3 + 1];
				v.normal.z = mesh.normals[i * 3 + 2];
			}

			// OBJ is right-handed; mirror X to match the engine's convention.
			v.position.x = -v.position.x;
			v.normal.x   = -v.normal.x;

			out.vertices.push_back(v);
		}

		if (!haveNormals)
		{
			const u32 triangleCount = (u32)mesh.indices.size() / 3;
			for (u32 i = 0; i < triangleCount; ++i)
			{
				const u32 idxA = firstVertex + mesh.indices[i * 3 + 0];
				const u32 idxB = firstVertex + mesh.indices[i * 3 + 2];
				const u32 idxC = firstVertex + mesh.indices[i * 3 + 1];

				const Vec3 faceNormal = normalize(cross(
				    out.vertices[idxB].position - out.vertices[idxA].position,
				    out.vertices[idxC].position - out.vertices[idxB].position));

				out.vertices[idxA].normal += faceNormal;
				out.vertices[idxB].normal += faceNormal;
				out.vertices[idxC].normal += faceNormal;
			}

			for (u32 i = firstVertex; i < (u32)out.vertices.size(); ++i)
			{
				out.vertices[i].normal = normalize(out.vertices[i].normal);
			}
		}

		int       currentMaterialId = -1;
		const u32 triangleCount     = (u32)mesh.indices.size() / 3;
		for (u32 triangleIt = 0; triangleIt < triangleCount; ++triangleIt)
		{
			if (mesh.material_ids[triangleIt] != currentMaterialId || out.segments.empty())
			{
				currentMaterialId = mesh.material_ids[triangleIt];
				ProceduralSceneSegment seg;
				seg.material    = u32(currentMaterialId); // raw id (-1 -> 0xFFFFFFFF); caller clamps as needed
				seg.indexOffset = (u32)out.indices.size();
				seg.indexCount  = 0;
				out.segments.push_back(seg);
			}

			// Swap winding (v0, v2, v1) to compensate for the X mirror above.
			out.indices.push_back(mesh.indices[triangleIt * 3 + 0] + firstVertex);
			out.indices.push_back(mesh.indices[triangleIt * 3 + 2] + firstVertex);
			out.indices.push_back(mesh.indices[triangleIt * 3 + 1] + firstVertex);

			out.segments.back().indexCount += 3;
		}
	}

	return true;
}

}
