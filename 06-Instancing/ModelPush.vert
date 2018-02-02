#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 0) uniform Global
{
	mat4 g_matViewProj;
};

layout(push_constant) uniform Instance
{
	mat4 g_matWorld;
} instanceConstants;

layout (location = 0) in vec3 a_pos0;
layout (location = 1) in vec4 a_col0;

layout (location = 0) out vec4 v_color;

void main()
{
	vec3 worldPos = (vec4(a_pos0, 1) * instanceConstants.g_matWorld).xyz;
	gl_Position = vec4(worldPos, 1) * g_matViewProj;
	v_color = a_col0;
}
