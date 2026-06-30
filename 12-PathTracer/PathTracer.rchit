#version 460
#extension GL_EXT_ray_tracing : enable

#define PT_CONFIG_SBT_HIT
#include "Common.glsl"

layout(location = 0) rayPayloadInEXT PtPayload payload;
hitAttributeEXT vec2 hitAttributes;

layout(shaderRecordEXT) buffer block
{
	MaterialConstants materialConstants;
};

void main()
{
	PathTracerContext ctx;

	PtHit hit;
	hit.valid = true;
	hit.t = gl_HitTEXT;
	hit.primId = gl_PrimitiveID;
	hit.bary = hitAttributes;
	hit.frontFacing = gl_HitKindEXT != 255u;

	uint indexBase = materialConstants.firstIndex + gl_PrimitiveID * 3u;
	fillPayload(ctx, hit, indexBase, materialConstants, payload);
}
