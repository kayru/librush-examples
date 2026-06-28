#ifndef INCLUDED_SHADER_SHARED
#define INCLUDED_SHADER_SHARED

// Helpers shared between the GLSL (Vulkan) and Metal (MSL) path tracers.
// A thin compatibility layer papers over the type-name, qualifier and inline
// differences so the function bodies below compile unchanged on both backends.

#ifdef __METAL_VERSION__
	#define SHADER_INLINE static inline
	#define INOUT(T) thread T&
	#define vec2 float2
	#define vec3 float3
	#define vec4 float4
	#define mat3 float3x3
	#define mat4 float4x4
	#define inversesqrt rsqrt
#else
	#define SHADER_INLINE
	#define INOUT(T) inout T
	#define atan2(y, x) atan((y), (x))
	#ifndef saturate
		#define saturate(x) clamp((x), 0.0, 1.0)
	#endif
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

SHADER_INLINE float pow2(float x)
{
	return x * x;
}

SHADER_INLINE float pow5(float x)
{
	float x2 = x * x;
	return x2 * x2 * x;
}

SHADER_INLINE float max3(vec3 v)
{
	return max(max(v.x, v.y), v.z);
}

SHADER_INLINE vec3 safeNormalize(vec3 v)
{
	float l = length(v);
	return l == 0.0f ? v : v / l;
}

SHADER_INLINE uint randomUint16(INOUT(uint) seed)
{
	seed = 214013u * seed + 2531011u;
	return (seed ^ (seed >> 16));
}

SHADER_INLINE uint randomUint32(INOUT(uint) seed)
{
	uint a = randomUint16(seed) << 16;
	uint b = randomUint16(seed) & 0xffffu;
	return a | b;
}

SHADER_INLINE float randomFloat(INOUT(uint) seed)
{
	seed = 214013u * seed + 2531011u;
	return float(seed >> 16) * (1.0f / 65535.0f);
}

SHADER_INLINE vec2 randomFloat2(INOUT(uint) seed)
{
	float x = randomFloat(seed);
	float y = randomFloat(seed);
	return vec2(x, y);
}

SHADER_INLINE uint hashFnv1(uint x)
{
	uint state = 0x811c9dc5u;
	for (uint i = 0u; i < 4u; ++i)
	{
		state *= 0x01000193u;
		state ^= (x & 0xffu);
		x = x >> 8u;
	}
	return state;
}

SHADER_INLINE vec3 mapToUniformSphere(vec2 uv)
{
	float phi = uv.x * M_PI * 2.0f;
	float z = 1.0f - 2.0f * uv.y;
	float r = sqrt(1.0f - z * z);
	float x = r * cos(phi);
	float y = r * sin(phi);
	return vec3(x, y, z);
}

SHADER_INLINE vec2 sampleUniformDisk(INOUT(uint) seed)
{
	for (;;)
	{
		vec2 v;
		v.x = randomFloat(seed) * 2.0f - 1.0f;
		v.y = randomFloat(seed) * 2.0f - 1.0f;
		if (dot(v, v) <= 1.0f)
		{
			return v;
		}
	}
}

SHADER_INLINE float Halton(int b, int i)
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

SHADER_INLINE vec2 Halton23(int i)
{
	return vec2(Halton(2, i), Halton(3, i));
}

SHADER_INLINE float powerHeuristic(float f, float g)
{
	float f2 = f * f;
	float g2 = g * g;
	return f2 / (f2 + g2);
}

// Metallic-roughness material resolved to the values the BRDF consumes.
struct Surface
{
	vec3 diffuseColor;
	vec3 specularColor;
	float linearRoughness;
};

SHADER_INLINE Surface unpackSurface(vec3 baseColor, float metalness, float roughness, float reflectance)
{
	Surface s;
	s.diffuseColor = baseColor - baseColor * metalness;
	s.specularColor = baseColor * metalness + reflectance * (1.0f - metalness);
	s.linearRoughness = max(0.0001f, roughness * roughness);
	return s;
}

// GGX BRDF primitives (adapted from Filament).

SHADER_INLINE float D_GGX(float linearRoughness, float NoH)
{
	float oneMinusNoHSquared = 1.0f - NoH * NoH;
	float a = NoH * linearRoughness;
	float k = linearRoughness / (oneMinusNoHSquared + a * a);
	float d = k * k * (1.0f / M_PI);
	return max(0.0f, d);
}

SHADER_INLINE float G1_Smith(float linearRoughness, float NoL)
{
	float a2 = linearRoughness * linearRoughness;
	return 2.0f * NoL / (NoL + sqrt(a2 + (1.0f - a2) * pow2(NoL)));
}

SHADER_INLINE vec3 F_Schlick(vec3 f0, float f90, float VoH)
{
	float f = pow5(1.0f - VoH);
	return f + f0 * (f90 - f);
}

// Sampling the GGX VNDF: http://jcgt.org/published/0007/04/01/paper.pdf
SHADER_INLINE vec3 importanceSampleDGGXVNDF(vec2 uv, float linearRoughness, vec3 V)
{
	vec3 Vh = normalize(vec3(linearRoughness * V.x, linearRoughness * V.y, V.z));

	float lensq = pow2(Vh.x) + pow2(Vh.y);
	vec3 T1 = lensq > 0.0f ? vec3(-Vh.y, Vh.x, 0.0f) * inversesqrt(lensq) : vec3(1.0f, 0.0f, 0.0f);
	vec3 T2 = cross(Vh, T1);

	float r = sqrt(uv.x);
	float phi = 2.0f * M_PI * uv.y;
	float t1 = r * cos(phi);
	float t2 = r * sin(phi);
	float s = 0.5f * (1.0f + Vh.z);
	t2 = (1.0f - s) * sqrt(max(0.0f, 1.0f - pow2(t1))) + s * t2;

	vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0f, 1.0f - pow2(t1) - pow2(t2))) * Vh;
	return vec3(linearRoughness * Nh.x, linearRoughness * Nh.y, max(0.0f, Nh.z));
}

// Lat-long environment mapping (http://gl.ict.usc.edu/Data/HighResProbes).
// latLongTexcoordToCartesian is the exact inverse of cartesianToLatLongTexcoord.

SHADER_INLINE vec2 cartesianToLatLongTexcoord(vec3 p)
{
	float u = 1.0f + atan2(p.z, -p.x) / M_PI;
	float v = acos(clamp(p.y, -1.0f, 1.0f)) / M_PI;
	return vec2(u * 0.5f, v);
}

SHADER_INLINE vec3 latLongTexcoordToCartesian(vec2 uv)
{
	float theta = M_PI * (uv.x * 2.0f - 1.0f);
	float phi = M_PI * uv.y;
	return vec3(-sin(phi) * cos(theta), cos(phi), sin(phi) * sin(theta));
}

// Hard-coded key light and analytic sky gradient.
SHADER_INLINE vec3 getSunDirection()
{
	return normalize(vec3(3.0f, 5.0f, 3.0f));
}

SHADER_INLINE vec3 getSunColor()
{
	return vec3(0.95f, 0.90f, 0.8f) * 2.5f * M_PI;
}

SHADER_INLINE vec3 getSkyColor(vec3 dir)
{
	vec3 colorT = vec3(0.5f, 0.66f, 0.9f) * 2.0f;
	vec3 colorB = vec3(0.15f, 0.18f, 0.15f);
	float a = dir.y * 0.5f + 0.5f;
	return mix(colorB, colorT, a);
}

// World-space ray direction through pixel uv ([0,1], top-left origin).
SHADER_INLINE vec3 getCameraViewVector(vec2 uv, mat4 matView, mat4 matProj)
{
	vec3 viewVector = vec3((uv - 0.5f) * 2.0f, 1.0f);
	viewVector.x /= matProj[0][0];
	viewVector.y /= matProj[1][1];
	mat3 view = mat3(matView[0].xyz, matView[1].xyz, matView[2].xyz);
	return normalize(viewVector * transpose(view));
}

// Focus-assist overlay opacity: 1 in focus, fading as the circle of confusion grows.
// hitDepth is the perpendicular distance from camera to the primary hit.
SHADER_INLINE float focalPlaneOverlay(float hitDepth, float focusDistance, float apertureSize,
	float focalLength, float sensorWidth, float outputWidthPx, float falloffPx)
{
	if (focusDistance <= 0.0f || hitDepth <= 0.0f)
	{
		return 0.0f;
	}
	float worldCoCRadius = (apertureSize * 0.5f) * abs(hitDepth - focusDistance) / focusDistance;
	float pixelCoCRadius = worldCoCRadius * (focalLength / sensorWidth) * outputWidthPx / hitDepth;
	float range = max(falloffPx, 1e-3f);
	return 1.0f - smoothstep(0.0f, range, pixelCoCRadius);
}

#endif // INCLUDED_SHADER_SHARED
