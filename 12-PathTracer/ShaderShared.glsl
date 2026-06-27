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
#else
	#define SHADER_INLINE
	#define INOUT(T) inout T
	#ifndef saturate
		#define saturate(x) clamp((x), 0.0, 1.0)
	#endif
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

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
