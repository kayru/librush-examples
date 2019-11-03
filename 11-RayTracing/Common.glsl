#ifndef INCLUDED_COMMON_GLSL
#define INCLUDED_COMMON_GLSL

// global resources

layout(set=0, binding=0) uniform Constants
{
	ivec2 outputSize;
};
layout(set=0, binding=1, rgba16f) uniform image2D outputImage;
layout(set=0, binding=2) uniform accelerationStructureNV TLAS;

// common types and functions

struct DefaultPayload
{
	vec3 color;
	float hitT;
};

struct RayDesc
{
	vec3 origin;
	float minT;
	vec3 direction;
	float maxT;
};

#endif // INCLUDED_COMMON_GLSL
