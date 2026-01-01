#version 450

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "Common.glsl"

layout (location = 0) in vec3 a_pos0;
layout (location = 1) in vec3 a_nor0;
layout (location = 2) in vec2 a_tex0;

layout (location = 0) out vec3 v_nor;
layout (location = 1) out vec2 v_tex;

void main()
{
	vec3 worldPos = (vec4(a_pos0, 1.0) * g_matWorld).xyz;
	gl_Position = vec4(worldPos, 1.0) * g_matViewProj;
	v_tex = a_tex0;
	v_nor = a_nor0;
}
