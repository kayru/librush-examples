#ifndef COMMON_GLSL
#define COMMON_GLSL

layout (set = 0, binding = 0) uniform SceneConstants
{
	mat4 g_matViewProj;
	mat4 g_matWorld;
};

layout (set = 1, binding = 0) uniform MaterialConstants
{
	vec4 g_baseColor;
};

// Keep sampler and texture in the same descriptor set for Metal argument buffers.
layout (set = 1, binding = 1) uniform sampler sampler0;

layout (set = 1, binding = 2) uniform texture2D texture0;

#endif // COMMON_GLSL
