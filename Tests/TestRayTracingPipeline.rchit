#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT uint payload;
hitAttributeEXT vec2 hitAttributes;

void main()
{
	payload = 2u;
}
