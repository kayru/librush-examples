#version 460
#extension GL_NV_ray_tracing : enable

#include "Common.glsl"

layout(location = 0) rayPayloadInNV DefaultPayload payload;
hitAttributeNV vec2 hitAttributes;

void main()
{
	payload.hitT = gl_HitTNV;
	payload.color = vec3(
		1 - hitAttributes.x - hitAttributes.y,
		hitAttributes.x,
		hitAttributes.y);
}
