#version 450

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "Common.glsl"

layout (location = 0) in vec3 v_nor;
layout (location = 1) in vec2 v_tex;

layout (location = 0) out vec4 fragColor;

void main()
{
	vec4 texColor = texture(sampler2D(texture0, sampler0), v_tex);
	fragColor = g_baseColor * texColor;
}
