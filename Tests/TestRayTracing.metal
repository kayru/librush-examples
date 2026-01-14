#include <metal_stdlib>

using namespace metal;

struct TestArgs
{
	sampler samplerNearest [[id(0)]];
	sampler samplerLinear [[id(1)]];
	sampler samplerUnused [[id(2)]];
	array<texture2d<float, access::sample>, 2> textures [[id(3)]];
	device uint4* results [[id(5)]];
};

static inline uint pack_color(float4 color)
{
	float4 clamped = clamp(color, 0.0f, 1.0f) * 255.0f + 0.5f;
	uint4 rgba = uint4(clamped);
	return rgba.x | (rgba.y << 8) | (rgba.z << 16) | (rgba.w << 24);
}

kernel void main0(const constant TestArgs& args [[buffer(0)]])
{
	float2 uvNearest = float2(0.25f, 0.25f);
	float2 uvLinear = float2(0.5f, 0.5f);

	float4 nearest0 = args.textures[0].sample(args.samplerNearest, uvNearest);
	float4 nearest1 = args.textures[1].sample(args.samplerNearest, uvNearest);
	float4 linear0 = args.textures[0].sample(args.samplerLinear, uvLinear);
	float4 linear1 = args.textures[1].sample(args.samplerLinear, uvLinear);

	args.results[0] = uint4(
		pack_color(nearest0),
		pack_color(nearest1),
		pack_color(linear0),
		pack_color(linear1));
}
