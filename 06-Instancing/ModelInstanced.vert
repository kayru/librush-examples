#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 0) uniform Global
{
	mat4 g_matViewProj;
};

layout(constant_id = 0) const int maxBatchCount = 1000;
layout(binding = 1) uniform Instance
{
	layout(row_major) mat4 g_matWorld[maxBatchCount];
};

layout (location = 0) in vec3 a_pos0;
layout (location = 1) in vec4 a_col0;

layout (location = 0) out vec4 v_color;

void main()
{
	vec3 worldPos = (vec4(a_pos0, 1) * g_matWorld[gl_InstanceIndex]).xyz;
	gl_Position = vec4(worldPos, 1) * g_matViewProj;
	v_color = a_col0;
}
