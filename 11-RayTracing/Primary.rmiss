#version 460
#extension GL_EXT_ray_tracing : enable

#include "Common.glsl"

layout(location = 0) rayPayloadInEXT DefaultPayload payload;

void main()
{
	payload.hitT = -1.0f;
}
