#version 460
#extension GL_EXT_ray_tracing : enable

#include "Common.glsl"

layout(location = 0) rayPayloadInEXT DefaultPayload payload;
hitAttributeEXT vec2 hitAttributes;

void main()
{
	payload.hitT = gl_HitTEXT;
	payload.color = vec3(
		1 - hitAttributes.x - hitAttributes.y,
		hitAttributes.x,
		hitAttributes.y);
}
