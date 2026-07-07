#ifndef INCLUDED_PT_SAMPLING
#define INCLUDED_PT_SAMPLING

// Sample generators shared by all backends. A SamplerState is seeded per pixel/frame, then
// next1D/next2D draw successive scalar dimensions of the path. Both generators are unbiased.
//
// LCG is the per-pixel white-noise baseline. Sobol uses a genuine multi-dimensional low-discrepancy
// sequence: dimension d has its own direction numbers (uploaded in PT_SOBOL). The sample index is
// Owen-scrambled per pixel and each output is Owen-scrambled per (pixel, dim) -- a nonlinear digit
// permutation (Burley 2020) that decorrelates pixels/dimensions and, unlike a digital XOR shift,
// destroys the net's structured error and bad projections rather than translating them (a shift
// leaves low-frequency splotches at low spp). Only the leading PT_QMC_DIMS dimensions use Sobol;
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

SHADER_INLINE uint reverseBits32(uint x)
{
	x = ((x & 0xaaaaaaaau) >> 1u) | ((x & 0x55555555u) << 1u);
	x = ((x & 0xccccccccu) >> 2u) | ((x & 0x33333333u) << 2u);
	x = ((x & 0xf0f0f0f0u) >> 4u) | ((x & 0x0f0f0f0fu) << 4u);
	x = ((x & 0xff00ff00u) >> 8u) | ((x & 0x00ff00ffu) << 8u);
	return (x >> 16u) | (x << 16u);
}

// Hash-based Owen (nested uniform) scramble, Burley "Practical Hash-Based Owen Scrambling" (JCGT 2020).
// Unlike a digital XOR shift, this permutes the binary digits nonlinearly, so it breaks up the Sobol
// net's structured error and poor high-dimensional projections instead of merely translating them --
// which is what removes the low-frequency splotches a plain shift leaves at low sample counts.
SHADER_INLINE uint owenScramble(uint x, uint seed)
{
	x = reverseBits32(x);
	x += seed;
	x ^= x * 0x6c50b47cu;
	x ^= x * 0xb82f1e52u;
	x ^= x * 0xc7afe638u;
	x ^= x * 0x8d22f6e6u;
	return reverseBits32(x);
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

// One Owen-scrambled Sobol scalar for dimension d. sobolIndex is the per-pixel Owen-scrambled sample
// index (shared across dimensions so the point stays a coherent Sobol point); the output is then
// Owen-scrambled per (pixel, dim). Both scrambles stay unbiased.
SHADER_INLINE float sobolScalar(PathTracerContext ctx, uint d, uint sobolIndex, uint pixelSeed)
{
	uint x = sobolSampleRaw(ctx, d, sobolIndex);
	x = owenScramble(x, hashCombine(pixelSeed, d + 0x9e3779b9u));
	return uintToFloat01(x);
}

struct SamplerState
{
	uint mode;
	uint pixelSeed;   // per-pixel scramble seed
	uint sampleIndex; // frame / sample number (used by discrete uint draws)
	uint sobolIndex;  // per-pixel Owen-scrambled sample index, constant across dimensions
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
	// Owen-scramble the sample index per pixel so pixels walk decorrelated orderings of the sequence.
	s.sobolIndex = owenScramble(frameIndex, hashCombine(s.pixelSeed, 0x51633e2du));
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
	return vec2(sobolScalar(ctx, d, s.sobolIndex, s.pixelSeed),
		sobolScalar(ctx, d + 1u, s.sobolIndex, s.pixelSeed));
}

SHADER_INLINE float samplerNext1D(PathTracerContext ctx, INOUT(SamplerState) s)
{
	uint d = s.dim;
	s.dim = d + 1u;
	if (s.mode == PT_SAMPLER_LCG || d >= PT_QMC_DIMS)
	{
		return randomFloat(s.lcgState);
	}
	return sobolScalar(ctx, d, s.sobolIndex, s.pixelSeed);
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
