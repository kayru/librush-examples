#ifndef INCLUDED_PT_SAMPLING
#define INCLUDED_PT_SAMPLING

// Sample generators shared by all backends. A SamplerState is seeded per pixel/frame, then
// next1D/next2D draw successive scalar dimensions of the path. Both generators are unbiased.
//
// LCG is the per-pixel white-noise baseline. Sobol uses a genuine multi-dimensional low-discrepancy
// sequence: dimension d has its own direction numbers (uploaded in PT_SOBOL), randomised per pixel
// by a digital (XOR) shift. Different dimensions therefore use distinct sequences, which is what
// keeps the high-dimensional estimator unbiased -- re-scrambling a single base sequence per
// dimension would leave dimensions correlated. Only the leading PT_QMC_DIMS dimensions use Sobol;
// deeper bounces fall back to the LCG stream (unbiased, and far cheaper where QMC buys the least).
//
// Requires ShaderShared.glsl (randomFloat, hashFnv1) and the PT_SOBOL accessor.

SHADER_INLINE uint pcgHash(uint v)
{
	uint state = v * 747796405u + 2891336453u;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	return (word >> 22u) ^ word;
}

SHADER_INLINE uint hashCombine(uint a, uint b)
{
	return pcgHash(a ^ (b + 0x9e3779b9u + (a << 6u) + (a >> 2u)));
}

SHADER_INLINE float uintToFloat01(uint x)
{
	return float(x >> 8u) * (1.0f / 16777216.0f); // top 24 bits -> [0,1)
}

// Sobol sample for dimension d (d < PT_QMC_DIMS): XOR the direction numbers for the set bits of i.
SHADER_INLINE uint sobolSampleRaw(PathTracerContext ctx, uint d, uint i)
{
	uint x = 0u;
	uint base = d * 32u;
	uint k = 0u;
	while (i != 0u)
	{
		if ((i & 1u) != 0u)
		{
			x ^= PT_SOBOL(ctx, base + k);
		}
		i >>= 1u;
		k += 1u;
	}
	return x;
}

// One Sobol scalar for dimension d, randomised by a per-(pixel,dim) digital shift. The shift keeps
// the (t,m,s)-net property and decorrelates pixels/dimensions while staying unbiased.
SHADER_INLINE float sobolScalar(PathTracerContext ctx, uint d, uint index, uint pixelSeed)
{
	uint shift = pixelSeed ^ (d * 0x9e3779b9u);
	return uintToFloat01(sobolSampleRaw(ctx, d, index) ^ shift);
}

struct SamplerState
{
	uint mode;
	uint pixelSeed;   // per-pixel scramble seed
	uint sampleIndex; // frame / sample number
	uint dim;         // scalar dimension counter, advanced per draw
	uint lcgState;    // LCG substream (baseline + deep-dimension fallback)
};

SHADER_INLINE SamplerState samplerInit(PathTracerContext ctx, uvec2 pixel, uint frameIndex, uint mode)
{
	SamplerState s;
	s.mode = mode;
	s.sampleIndex = frameIndex;
	s.dim = 0u;
	s.pixelSeed = hashCombine(pixel.x, pixel.y);
	s.lcgState = hashFnv1(s.pixelSeed + frameIndex * 0x9e3779b9u);
	return s;
}

SHADER_INLINE vec2 samplerNext2D(PathTracerContext ctx, INOUT(SamplerState) s)
{
	uint d = s.dim;
	s.dim = d + 2u;
	// Guard on the second lane too: a pair straddling the QMC boundary would read one dimension past
	// the Sobol buffer, so fall the whole pair back to LCG there (keeps the 2D sample coherent).
	if (s.mode == PT_SAMPLER_LCG || d + 1u >= PT_QMC_DIMS)
	{
		return randomFloat2(s.lcgState);
	}
	return vec2(sobolScalar(ctx, d, s.sampleIndex, s.pixelSeed),
		sobolScalar(ctx, d + 1u, s.sampleIndex, s.pixelSeed));
}

SHADER_INLINE float samplerNext1D(PathTracerContext ctx, INOUT(SamplerState) s)
{
	uint d = s.dim;
	s.dim = d + 1u;
	if (s.mode == PT_SAMPLER_LCG || d >= PT_QMC_DIMS)
	{
		return randomFloat(s.lcgState);
	}
	return sobolScalar(ctx, d, s.sampleIndex, s.pixelSeed);
}

// Full-range integer draw for discrete choices (e.g. importance-sampled envmap texel).
SHADER_INLINE uint samplerNextUint(INOUT(SamplerState) s)
{
	uint d = s.dim;
	s.dim = d + 1u;
	return hashCombine(hashCombine(s.pixelSeed, d), s.sampleIndex + 0x243f6a88u);
}

// Concentric mapping of a unit-square sample to the unit disk (Shirley-Chiu); QMC-friendly
// replacement for rejection sampling.
SHADER_INLINE vec2 sampleConcentricDisk(vec2 u)
{
	vec2 o = 2.0f * u - 1.0f;
	if (o.x == 0.0f && o.y == 0.0f)
	{
		return vec2(0.0f);
	}
	float r, theta;
	if (abs(o.x) > abs(o.y))
	{
		r = o.x;
		theta = (M_PI / 4.0f) * (o.y / o.x);
	}
	else
	{
		r = o.y;
		theta = (M_PI / 2.0f) - (M_PI / 4.0f) * (o.x / o.y);
	}
	return r * vec2(cos(theta), sin(theta));
}

#endif // INCLUDED_PT_SAMPLING
