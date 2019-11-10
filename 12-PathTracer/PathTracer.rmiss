#version 460
#extension GL_NV_ray_tracing : enable

#include "Common.glsl"

layout(location = 0) rayPayloadInNV DefaultPayload payload;

void main()
{
	payload.hitT = -1.0f;
}
