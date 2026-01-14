#include <metal_stdlib>
#include <metal_raytracing>

using namespace metal;
using namespace metal::raytracing;

#define PT_FLAG_USE_ENVMAP              (1u << 0u)
#define PT_FLAG_USE_NEUTRAL_BACKGROUND  (1u << 1u)
#define PT_FLAG_USE_DEPTH_OF_FIELD      (1u << 2u)
#define PT_FLAG_USE_NORMAL_MAPPING      (1u << 3u)
#define PT_FLAG_DEBUG_SIMPLE_SHADING    (1u << 4u)
#define PT_FLAG_DEBUG_DISABLE_ACCUMULATION (1u << 5u)
#define PT_FLAG_DEBUG_HIT_MASK          (1u << 6u)

#define PT_DEBUG_VIS_NONE               0u
#define PT_DEBUG_VIS_ALBEDO             1u
#define PT_DEBUG_VIS_GEO_NORMAL         2u
#define PT_DEBUG_VIS_SHADING_NORMAL     3u
#define PT_DEBUG_VIS_NORMAL_MAPPED      4u
#define PT_DEBUG_VIS_TANGENT            5u
#define PT_DEBUG_VIS_BITANGENT          6u
#define PT_DEBUG_VIS_METALNESS          7u
#define PT_DEBUG_VIS_ROUGHNESS          8u
#define PT_DEBUG_VIS_UV                 9u

#define PT_MATERIAL_MODE_PBR_METALLIC_ROUGHNESS   0u
#define PT_MATERIAL_MODE_PBR_SPECULAR_GLOSSINESS  1u

static constant float kPi = 3.14159265359f;

static inline float pow5(float v)
{
	float v2 = v * v;
	return v2 * v2 * v;
}

static inline float max3(float3 v)
{
	return max(max(v.x, v.y), v.z);
}

static inline float3 safeNormalize(float3 v)
{
	float l = length(v);
	return l == 0.0f ? v : v / l;
}

static inline float D_GGX(float linearRoughness, float NoH)
{
	float oneMinusNoHSquared = 1.0f - NoH * NoH;
	float a = NoH * linearRoughness;
	float k = linearRoughness / (oneMinusNoHSquared + a * a);
	float d = k * k * (1.0f / kPi);
	return max(0.0f, d);
}

static inline float G1_Smith(float linearRoughness, float NoL)
{
	float a2 = linearRoughness * linearRoughness;
	return 2.0f * NoL / (NoL + sqrt(a2 + (1.0f - a2) * (NoL * NoL)));
}

static inline float3 F_Schlick(float3 f0, float f90, float VoH)
{
	float f = pow5(1.0f - VoH);
	return f + f0 * (f90 - f);
}

static inline float3 importanceSampleDGGXVNDF(float2 uv, float linearRoughness, float3 V)
{
	float3 Vh = normalize(float3(linearRoughness * V.x, linearRoughness * V.y, V.z));
	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	float3 T1 = lensq > 0.0f ? float3(-Vh.y, Vh.x, 0.0f) * rsqrt(lensq) : float3(1.0f, 0.0f, 0.0f);
	float3 T2 = cross(Vh, T1);

	float r = sqrt(uv.x);
	float phi = 2.0f * kPi * uv.y;
	float t1 = r * cos(phi);
	float t2 = r * sin(phi);
	float s = 0.5f * (1.0f + Vh.z);
	t2 = (1.0f - s) * sqrt(max(0.0f, 1.0f - t1 * t1)) + s * t2;

	float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0f, 1.0f - t1 * t1 - t2 * t2)) * Vh;
	return float3(linearRoughness * Nh.x, linearRoughness * Nh.y, max(0.0f, Nh.z));
}

static inline uint randomUint16(thread uint& seed)
{
	seed = 214013u * seed + 2531011u;
	return (seed ^ (seed >> 16));
}

static inline uint randomUint32(thread uint& seed)
{
	uint a = randomUint16(seed) << 16;
	uint b = randomUint16(seed) & 0xffffu;
	return a | b;
}

static inline float randomFloat(thread uint& seed)
{
	seed = 214013u * seed + 2531011u;
	return float(seed >> 16) * (1.0f / 65535.0f);
}

static inline float2 randomFloat2(thread uint& seed)
{
	return float2(randomFloat(seed), randomFloat(seed));
}

static inline uint hashFnv1(uint x)
{
	uint state = 0x811c9dc5u;
	for (uint i = 0; i < 4; ++i)
	{
		state *= 0x01000193u;
		state ^= (x & 0xffu);
		x >>= 8u;
	}
	return state;
}

static inline float3 mapToUniformSphere(float2 uv)
{
	float phi = uv.x * kPi * 2.0f;
	float z = 1.0f - 2.0f * uv.y;
	float r = sqrt(1.0f - z * z);
	float x = r * cos(phi);
	float y = r * sin(phi);
	return float3(x, y, z);
}

static inline float2 sampleUniformDisk(thread uint& seed)
{
	for (;;)
	{
		float2 v;
		v.x = randomFloat(seed) * 2.0f - 1.0f;
		v.y = randomFloat(seed) * 2.0f - 1.0f;
		if (dot(v, v) <= 1.0f)
		{
			return v;
		}
	}
}

static inline float Halton(int b, int i)
{
	float r = 0.0f;
	float f = 1.0f;
	while (i > 0)
	{
		f = f / float(b);
		r = r + f * float(i % b);
		i = int(floor(float(i) / float(b)));
	}
	return r;
}

static inline float2 Halton23(int i)
{
	return float2(Halton(2, i), Halton(3, i));
}

static inline float2 cartesianToLatLongTexcoord(float3 p)
{
	float u = (1.0f + atan2(p.z, -p.x) / kPi);
	float v = acos(clamp(p.y, -1.0f, 1.0f)) / kPi;
	return float2(u * 0.5f, v);
}

static inline float3 latLongTexcoordToCartesian(float2 uv)
{
	float theta = kPi * (uv.x * 2.0f - 1.0f);
	float phi = kPi * uv.y;
	float x = cos(theta) * sin(phi);
	float y = cos(phi);
	float z = -sin(theta) * sin(phi);
	return float3(x, y, z);
}

static inline float3 envMapPixelIndexToDirection(uint idx, float2 pixelJitter, int2 envmapSize)
{
	uint x = idx % uint(envmapSize.x);
	uint y = idx / uint(envmapSize.x);
	float2 pixelPos = float2(float(x), float(y));
	float2 uv = (pixelPos + pixelJitter) / float2(float(envmapSize.x), float(envmapSize.y));
	return latLongTexcoordToCartesian(uv);
}

static inline float powerHeuristic(float f, float g)
{
	float f2 = f * f;
	float g2 = g * g;
	return f2 / (f2 + g2);
}

static inline float3 getSimpleSkyColor(float3 dir)
{
	float3 colorT = float3(0.5f, 0.66f, 0.9f) * 2.0f;
	float3 colorB = float3(0.15f, 0.18f, 0.15f);
	float a = dir.y * 0.5f + 0.5f;
	return mix(colorB, colorT, a);
}

struct SceneConstants
{
	float4x4 matView;
	float4x4 matProj;
	float4x4 matViewProj;
	float4x4 matViewProjInv;
	float4x4 matEnvmapTransform;
	float4 cameraPosition;

	int2 outputSize;
	uint frameIndex;
	uint flags;

	int2 envmapSize;
	float2 cameraSensorSize;

	float focalLength;
	float focusDistance;
	float apertureSize;
	uint debugVisMode;
};

struct MaterialConstants
{
	packed_float4 albedoFactor;
	packed_float4 specularFactor;
	uint albedoTextureId;
	uint specularTextureId;
	uint normalTextureId;
	uint firstIndex;
	uint alphaMode;
	float metallicFactor;
	float roughnessFactor;
	float reflectance;
	uint materialMode;
};

struct Vertex
{
	packed_float3 position;
	packed_float3 normal;
	packed_float2 texcoord;
	packed_float4 tangent;
};

struct EnvmapCell
{
	float p;
	uint i;
};

struct PathTracerSet0
{
	constant SceneConstants* scene [[id(0)]];
	sampler defaultSampler [[id(1)]];
	texture2d<float, access::sample> envmapTexture [[id(2)]];
	texture2d<float, access::read_write> outputImage [[id(3)]];
	device uint* indexBuffer [[id(4)]];
	device Vertex* vertexBuffer [[id(5)]];
	device EnvmapCell* envmapDistribution [[id(6)]];
	device MaterialConstants* materials [[id(7)]];
	device uint* materialIndices [[id(8)]];
	instance_acceleration_structure tlas [[id(9)]];
};

#define PT_MAX_TEXTURES 1024

struct PathTracerSet1
{
	array<texture2d<float, access::sample>, PT_MAX_TEXTURES> textures [[id(0)]];
};

struct LightSample
{
	float3 w;
	float3 value;
	float pdfW;
};

static inline float3 applyEnvmapTransform(constant SceneConstants* scene, float3 dir)
{
	if (!scene)
	{
		return dir;
	}
	float3x3 envmapXform = float3x3(scene->matEnvmapTransform[0].xyz,
		scene->matEnvmapTransform[1].xyz,
		scene->matEnvmapTransform[2].xyz);
	return dir * envmapXform;
}

static inline LightSample sampleEnvmap(constant SceneConstants* scene,
	texture2d<float, access::sample> envmap,
	sampler s,
	float3 dir)
{
	float2 uv = cartesianToLatLongTexcoord(dir);
	float4 sample = envmap.sample(s, uv);
	LightSample result;
	result.w = applyEnvmapTransform(scene, dir);
	result.value = sample.xyz;
	result.pdfW = sample.w;
	return result;
}

static inline float3 importanceSampleSkyLightDir(constant PathTracerSet0& set0, thread uint& randomSeed)
{
	if (!set0.scene || !set0.envmapDistribution)
	{
		return float3(0.0f, 1.0f, 0.0f);
	}

	int2 envmapSize = set0.scene->envmapSize;
	if (envmapSize.x <= 0 || envmapSize.y <= 0)
	{
		return float3(0.0f, 1.0f, 0.0f);
	}

	uint texelCount = uint(envmapSize.x * envmapSize.y);
	uint i = randomUint32(randomSeed) % texelCount;
	EnvmapCell entry = set0.envmapDistribution[i];
	float2 jitter = randomFloat2(randomSeed);

	if (randomFloat(randomSeed) <= entry.p)
	{
		return envMapPixelIndexToDirection(i, jitter, envmapSize);
	}
	return envMapPixelIndexToDirection(entry.i, jitter, envmapSize);
}

static inline void buildOrthonormalBasis(float3 n, thread float3& t, thread float3& b)
{
	float3 up = fabs(n.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(0.0f, 1.0f, 0.0f);
	t = normalize(cross(up, n));
	b = cross(n, t);
}

struct Payload
{
	float hitT;
	float3 baseColor;
	float metalness;
	float roughness;
	float reflectance;
	float3 shadingNormal;
	float3 normal;
	float3 geoNormal;
	float3 tangent;
	float3 bitangent;
};

static inline float3 encodeSignedVector(float3 v)
{
	const float len = length(v);
	if (len < 1e-5f)
	{
		return float3(0.5f);
	}
	return v / len * 0.5f + 0.5f;
}

static inline float3 debugVisColor(uint mode, thread const Payload& payload, float2 uv)
{
	switch (mode)
	{
	case PT_DEBUG_VIS_ALBEDO:
		return payload.baseColor;
	case PT_DEBUG_VIS_GEO_NORMAL:
		return encodeSignedVector(payload.geoNormal);
	case PT_DEBUG_VIS_SHADING_NORMAL:
		return encodeSignedVector(payload.shadingNormal);
	case PT_DEBUG_VIS_NORMAL_MAPPED:
		return encodeSignedVector(payload.normal);
	case PT_DEBUG_VIS_TANGENT:
		return encodeSignedVector(payload.tangent);
	case PT_DEBUG_VIS_BITANGENT:
		return encodeSignedVector(payload.bitangent);
	case PT_DEBUG_VIS_METALNESS:
		return float3(payload.metalness);
	case PT_DEBUG_VIS_ROUGHNESS:
		return float3(payload.roughness);
	case PT_DEBUG_VIS_UV:
		return float3(fract(uv.x), fract(uv.y), 0.0f);
	default:
		return float3(0.0f);
	}
}

static inline float3 getCameraViewVector(constant SceneConstants* scene, float2 uv)
{
	float3 viewVector = float3((uv - 0.5f) * 2.0f, 1.0f);
	viewVector.x /= scene->matProj[0][0];
	viewVector.y /= scene->matProj[1][1];
	float3x3 view = float3x3(scene->matView[0].xyz, scene->matView[1].xyz, scene->matView[2].xyz);
	return normalize(viewVector * transpose(view));
}

static inline bool traceShadowRay(constant PathTracerSet0& set0, ray shadowRay)
{
	intersector<triangle_data, instancing> shadowIntersector;
	shadowIntersector.assume_geometry_type(geometry_type::triangle);
	shadowIntersector.set_triangle_cull_mode(triangle_cull_mode::none);

	intersection_result<triangle_data, instancing> hit = shadowIntersector.intersect(shadowRay, set0.tlas);
	return hit.type != intersection_type::none;
}

static inline float2 interpolate(float2 a, float2 b, float2 c, float3 bary)
{
	return a * bary.x + b * bary.y + c * bary.z;
}

static inline float3 interpolate(float3 a, float3 b, float3 c, float3 bary)
{
	return a * bary.x + b * bary.y + c * bary.z;
}

static inline float4 interpolate(float4 a, float4 b, float4 c, float3 bary)
{
	return a * bary.x + b * bary.y + c * bary.z;
}

static inline void fillPayload(constant PathTracerSet0& set0,
	constant PathTracerSet1& set1,
	intersection_result<triangle_data, instancing> hit,
	thread Payload& payload,
	thread float2& uvOut)
{
	payload.hitT = hit.distance;

	float2 baryUV = hit.triangle_barycentric_coord;
	float3 barycentrics = float3(1.0f - baryUV.x - baryUV.y, baryUV.x, baryUV.y);

	uint baseIndex = hit.primitive_id * 3u;
	uint i0 = set0.indexBuffer[baseIndex + 0u];
	uint i1 = set0.indexBuffer[baseIndex + 1u];
	uint i2 = set0.indexBuffer[baseIndex + 2u];

	Vertex v0 = set0.vertexBuffer[i0];
	Vertex v1 = set0.vertexBuffer[i1];
	Vertex v2 = set0.vertexBuffer[i2];

	float3 p0 = float3(v0.position);
	float3 p1 = float3(v1.position);
	float3 p2 = float3(v2.position);

	float3 normal = normalize(interpolate(float3(v0.normal), float3(v1.normal), float3(v2.normal), barycentrics));
	payload.geoNormal = normalize(cross(p1 - p0, p2 - p0));

	uvOut = interpolate(float2(v0.texcoord), float2(v1.texcoord), float2(v2.texcoord), barycentrics);

	uint materialIndex = 0u;
	if (set0.materialIndices)
	{
		materialIndex = set0.materialIndices[hit.primitive_id];
	}

	MaterialConstants material;
	material.albedoFactor = float4(1.0f);
	material.specularFactor = float4(1.0f);
	material.albedoTextureId = 0u;
	material.specularTextureId = 0u;
	material.normalTextureId = 0u;
	material.metallicFactor = 1.0f;
	material.roughnessFactor = 1.0f;
	material.reflectance = 0.04f;
	material.materialMode = PT_MATERIAL_MODE_PBR_METALLIC_ROUGHNESS;

	if (set0.materials)
	{
		material = set0.materials[materialIndex];
	}

	float4 albedoSample = float4(1.0f);
	float4 specularSample = float4(1.0f);
	if (material.albedoTextureId < PT_MAX_TEXTURES)
	{
		albedoSample = set1.textures[material.albedoTextureId].sample(set0.defaultSampler, uvOut);
	}
	if (material.specularTextureId < PT_MAX_TEXTURES)
	{
		specularSample = set1.textures[material.specularTextureId].sample(set0.defaultSampler, uvOut);
	}

	if (material.materialMode == PT_MATERIAL_MODE_PBR_METALLIC_ROUGHNESS)
	{
		payload.baseColor = albedoSample.xyz * material.albedoFactor.xyz;
		payload.metalness = material.metallicFactor * specularSample.z;
		payload.roughness = material.roughnessFactor * specularSample.y;
	}
	else
	{
		payload.metalness = max(specularSample.x * material.specularFactor.x,
			max(specularSample.y * material.specularFactor.y, specularSample.z * material.specularFactor.z));
		payload.roughness = 1.0f - specularSample.w * material.roughnessFactor;
		payload.baseColor = mix(albedoSample.xyz * material.albedoFactor.xyz,
			specularSample.xyz * material.specularFactor.xyz,
			payload.metalness);
	}

	payload.reflectance = material.reflectance;
	payload.shadingNormal = normal;
	payload.normal = normal;
	payload.tangent = float3(0.0f);
	payload.bitangent = float3(0.0f);

	float4 tangent = interpolate(float4(v0.tangent), float4(v1.tangent), float4(v2.tangent), barycentrics);
	const float tangentLen = length(tangent.xyz);
	bool hasTangent = false;
	bool hasBitangent = false;
	if (tangentLen > 1e-5f)
	{
		float3 tanU = tangent.xyz / tangentLen;
		float3 tanV = cross(payload.shadingNormal, tanU) * tangent.w;
		const float bitangentLen = length(tanV);
		if (bitangentLen > 1e-5f)
		{
			tanV /= bitangentLen;
			hasBitangent = true;
		}
		payload.tangent = tanU;
		payload.bitangent = tanV;
		hasTangent = true;
	}

	const bool useNormalMapping = set0.scene
		&& (set0.scene->flags & PT_FLAG_USE_NORMAL_MAPPING) != 0u
		&& material.normalTextureId != 0u
		&& material.normalTextureId < PT_MAX_TEXTURES;
	if (useNormalMapping && hasTangent && hasBitangent)
	{
		float3 normalSample = set1.textures[material.normalTextureId].sample(set0.defaultSampler, uvOut).xyz * 2.0f - 1.0f;
		normalSample.z = sqrt(max(0.0f, 1.0f - normalSample.x * normalSample.x - normalSample.y * normalSample.y));
		float3x3 basis = float3x3(payload.tangent, payload.bitangent, payload.normal);
		payload.normal = normalize(basis * normalSample);
	}

	if (!hit.triangle_front_facing)
	{
		payload.shadingNormal = -payload.shadingNormal;
		payload.normal = -payload.normal;
		payload.geoNormal = -payload.geoNormal;
		payload.tangent = -payload.tangent;
		payload.bitangent = -payload.bitangent;
	}
}

kernel void main0(constant PathTracerSet0& set0 [[buffer(0)]],
	constant PathTracerSet1& set1 [[buffer(1)]],
	uint2 gid [[thread_position_in_grid]])
{
	const bool useDebugFurnace = false;
	const bool useDebugReflections = false;
	const bool useDirectLighting = true;
	const bool useIndirectSpecular = true;
	const bool useRoughnessBias = true;
	const bool visNormal = false;

	const uint width = set0.outputImage.get_width();
	const uint height = set0.outputImage.get_height();
	if (gid.x >= width || gid.y >= height)
	{
		return;
	}

	if (!set0.scene)
	{
		set0.outputImage.write(float4(0.0f, 0.0f, 0.0f, 1.0f), gid);
		return;
	}

	const int2 outputSize = set0.scene->outputSize;
	const int2 pixelIndex = int2(gid);
	const float2 pixelUV = float2(pixelIndex) / float2(outputSize);

	uint pixelLinearIndex = uint(pixelIndex.x + pixelIndex.y * outputSize.y);
	uint pixelRandomSeed = hashFnv1(pixelLinearIndex + pixelLinearIndex * 1294974679u);
	uint randomSeed = hashFnv1(pixelRandomSeed + set0.scene->frameIndex);
	float2 pixelJitter = (randomFloat2(randomSeed) - 0.5f) / float2(outputSize);

	float3 result = float3(0.0f);
	float3 throughput = float3(1.0f);

	ray primaryRay;
	primaryRay.min_distance = 0.0f;
	primaryRay.max_distance = 1e9f;
	primaryRay.origin = set0.scene->cameraPosition.xyz;
	primaryRay.direction = getCameraViewVector(set0.scene, pixelUV + pixelJitter);

	if ((set0.scene->flags & PT_FLAG_USE_DEPTH_OF_FIELD) != 0u)
	{
		float denom = dot(set0.scene->matView[2].xyz, primaryRay.direction);
		float3 focusPoint = primaryRay.origin + primaryRay.direction * (set0.scene->focusDistance / denom);
		float2 apertureSample = sampleUniformDisk(randomSeed) * 0.5f;
		primaryRay.origin += (set0.scene->matView[0].xyz * apertureSample.x
			+ set0.scene->matView[1].xyz * apertureSample.y) * set0.scene->apertureSize;
		primaryRay.direction = normalize(focusPoint - primaryRay.origin);
	}

	const uint maxPathLength = useDebugFurnace ? 2u : 5u;
	float scatterPdfW = 1e9f;
	float roughnessBias = 0.0f;
	const bool useEnvmap = (set0.scene->flags & PT_FLAG_USE_ENVMAP) != 0u;
	const bool debugSimple = (set0.scene->flags & PT_FLAG_DEBUG_SIMPLE_SHADING) != 0u;
	const bool debugHitMask = (set0.scene->flags & PT_FLAG_DEBUG_HIT_MASK) != 0u;
	const uint debugVisMode = set0.scene ? set0.scene->debugVisMode : PT_DEBUG_VIS_NONE;
	const bool debugVisEnabled = debugVisMode != PT_DEBUG_VIS_NONE;
	uint lastBounce = 0u;
	bool sawHit = false;

	intersector<triangle_data, instancing> intersector;
	intersector.assume_geometry_type(geometry_type::triangle);
	intersector.set_triangle_cull_mode(triangle_cull_mode::none);

	if (debugSimple || debugHitMask || debugVisEnabled)
	{
		intersection_result<triangle_data, instancing> hit = intersector.intersect(primaryRay, set0.tlas);
		if (debugHitMask)
		{
			float3 maskColor = (hit.type == intersection_type::none) ? float3(0.0f) : float3(1.0f, 0.0f, 0.0f);
			set0.outputImage.write(float4(maskColor, 1.0f), gid);
			return;
		}
		if (debugSimple || debugVisEnabled)
		{
			float3 simpleColor = float3(0.0f);
			if (hit.type == intersection_type::none)
			{
				if (debugSimple)
				{
					simpleColor = useEnvmap
						? sampleEnvmap(set0.scene, set0.envmapTexture, set0.defaultSampler,
							applyEnvmapTransform(set0.scene, primaryRay.direction)).value
						: getSimpleSkyColor(primaryRay.direction);
					if ((set0.scene->flags & PT_FLAG_USE_NEUTRAL_BACKGROUND) != 0u)
					{
						simpleColor = float3(0.25f);
					}
				}
			}
			else
			{
				Payload payload;
				float2 uv;
				fillPayload(set0, set1, hit, payload, uv);
				if (debugVisEnabled)
				{
					simpleColor = debugVisColor(debugVisMode, payload, uv);
				}
				else
				{
					simpleColor = payload.baseColor;
				}
			}

			if ((set0.scene->flags & PT_FLAG_DEBUG_DISABLE_ACCUMULATION) != 0u)
			{
				set0.outputImage.write(float4(simpleColor, 1.0f), gid);
			}
			else
			{
				float3 previous = set0.outputImage.read(gid).xyz;
				float frame = float(set0.scene->frameIndex);
				float3 accumulated = (previous * frame + simpleColor) / (frame + 1.0f);
				set0.outputImage.write(float4(accumulated, 1.0f), gid);
			}
			return;
		}
	}

	for (uint i = 0u; i <= maxPathLength; ++i)
	{
		lastBounce = i;
		intersection_result<triangle_data, instancing> hit = intersector.intersect(primaryRay, set0.tlas);
		bool isHit = hit.type != intersection_type::none;

		if (useDebugFurnace && i > 0u)
		{
			isHit = false;
		}

		Payload payload;
		float2 uv = float2(0.0f);
		if (isHit)
		{
			sawHit = true;
			fillPayload(set0, set1, hit, payload, uv);
			if (useRoughnessBias)
			{
				payload.roughness = max(payload.roughness, roughnessBias);
				roughnessBias = payload.roughness;
			}
		}

		if (!isHit)
		{
			if (i == 0u && (set0.scene->flags & PT_FLAG_USE_NEUTRAL_BACKGROUND) != 0u)
			{
				result = float3(0.25f);
			}
			else if (useEnvmap)
			{
				float3 envDir = applyEnvmapTransform(set0.scene, primaryRay.direction);
				LightSample ls = sampleEnvmap(set0.scene, set0.envmapTexture, set0.defaultSampler, envDir);
				float misWeight = powerHeuristic(scatterPdfW, ls.pdfW);
				result += throughput * ls.value * misWeight;
			}
			else
			{
				result += throughput * getSimpleSkyColor(primaryRay.direction);
			}
			break;
		}

		if (visNormal)
		{
			result = payload.normal * 0.5f + 0.5f;
			break;
		}

		if (useDebugFurnace)
		{
			payload.baseColor = float3(1.0f);
		}
		if (useDebugReflections && i == 0u)
		{
			payload.baseColor = float3(1.0f);
		}

		float3 N = normalize(payload.normal);
		float3 V = -primaryRay.direction;

		float3 diffuseColor = payload.baseColor - payload.baseColor * payload.metalness;
		float3 specularColor = payload.baseColor * payload.metalness
			+ (payload.reflectance * (1.0f - payload.metalness));
		float linearRoughness = max(0.0001f, payload.roughness * payload.roughness);
		if (i == maxPathLength)
		{
			break;
		}

		if (useDirectLighting && !useDebugFurnace && (!useDebugReflections || i > 0u))
		{
			float3 L = float3(0.0f, 0.0f, 1.0f);
			float3 lightColor = float3(0.0f);
			float lightPdfW = 0.0f;

			if (useEnvmap)
			{
				float3 envDir = importanceSampleSkyLightDir(set0, randomSeed);
				LightSample ls = sampleEnvmap(set0.scene, set0.envmapTexture, set0.defaultSampler, envDir);
				L = ls.w;
				lightColor = ls.value;
				lightPdfW = ls.pdfW;
			}
			else
			{
				L = normalize(float3(3.0f, 5.0f, 3.0f));
				lightColor = float3(0.95f, 0.90f, 0.8f) * 2.5f * kPi;
				lightPdfW = 1.0f;
			}

			ray shadowRay;
			shadowRay.origin = primaryRay.origin + primaryRay.direction * payload.hitT;
			shadowRay.origin += payload.geoNormal * max3(abs(shadowRay.origin)) * 1e-4f;
			shadowRay.direction = L;
			shadowRay.min_distance = 0.0f;
			shadowRay.max_distance = 1e9f;

			float NoL = dot(N, L);
			if (NoL > 0.0f && lightPdfW > 0.0f && !traceShadowRay(set0, shadowRay))
			{
				float3 H = normalize(V + L);
				float NoH = max(0.0f, dot(N, H));
				float LoH = max(0.0f, dot(L, H));
				float VoH = max(0.0f, dot(V, H));

				float sD = D_GGX(linearRoughness, NoH);
				float sG = G1_Smith(linearRoughness, NoL);
				float3 sF = F_Schlick(specularColor, 1.0f, LoH);

				float denom = 4.0f * VoH;
				float brdfPdfW = denom > 0.0f ? sD * NoH / denom : 0.0f;
				float3 brdf = sD * sG * sF;

				float misWeight = useEnvmap ? powerHeuristic(lightPdfW, brdfPdfW) : 1.0f;
				result += throughput * lightColor * brdf * misWeight / (lightPdfW * 2.0f);

				float diffusePdfW = 1.0f / kPi;
				float3 diffuseBrdf = diffuseColor * NoL;
				misWeight = useEnvmap ? powerHeuristic(lightPdfW, diffusePdfW) : 1.0f;
				result += throughput * lightColor * diffuseBrdf * misWeight / (lightPdfW * 2.0f);
			}
		}

		primaryRay.origin = primaryRay.origin + primaryRay.direction * payload.hitT;
		primaryRay.origin += payload.geoNormal * max3(abs(primaryRay.origin)) * 1e-4f;

		float2 reflectionSampleUV = randomFloat2(randomSeed);
		if (i == 0u)
		{
			uint pixelRandomSeedState = pixelRandomSeed;
			float2 base = Halton23(int(set0.scene->frameIndex)) + randomFloat2(pixelRandomSeedState);
			reflectionSampleUV = base - floor(base);
		}

		const float specularProbability = useIndirectSpecular ? clamp(payload.metalness, 0.1f, 0.9f) : 0.0f;
		const bool isSpecular = randomFloat(randomSeed) <= specularProbability;

		if (isSpecular)
		{
			float3 t;
			float3 b;
			buildOrthonormalBasis(N, t, b);
			float3x3 basis = float3x3(t, b, N);

			float3 Vb = V * basis;
			float3 H = normalize(basis * importanceSampleDGGXVNDF(reflectionSampleUV, linearRoughness, Vb));
			float3 L = reflect(-V, H);

			float NoL = max(0.0f, dot(N, L));
			float NoH = max(0.0f, dot(N, H));
			float LoH = max(0.0f, dot(L, H));
			float VoH = max(0.0f, dot(V, H));

			float sD = D_GGX(linearRoughness, NoH);
			float sG = G1_Smith(linearRoughness, NoL);
			float3 sF = F_Schlick(specularColor, 1.0f, LoH);

			throughput *= sF * sG;
			throughput /= specularProbability;

			primaryRay.direction = L;
			float denom = 4.0f * VoH;
			scatterPdfW = denom > 0.0f ? sD * NoH / denom : 0.0f;
		}
		else
		{
			throughput *= useIndirectSpecular ? diffuseColor : payload.baseColor;
			throughput /= (1.0f - specularProbability);

			primaryRay.direction = safeNormalize(N + mapToUniformSphere(reflectionSampleUV));
			scatterPdfW = 1.0f / kPi;
		}
	}

	if ((set0.scene->flags & PT_FLAG_DEBUG_DISABLE_ACCUMULATION) != 0u)
	{
		set0.outputImage.write(float4(result, 1.0f), gid);
	}
	else
	{
		if (set0.scene->frameIndex > 0u)
		{
			float3 oldValue = set0.outputImage.read(gid).xyz;
			result = mix(oldValue, result, 1.0f / (float(set0.scene->frameIndex) + 1.0f));
		}
		set0.outputImage.write(float4(result, 1.0f), gid);
	}

}
