#version 460
#extension GL_NV_ray_tracing : enable

#include "Common.glsl"

layout(location = 0) rayPayloadInNV DefaultPayload payload;
hitAttributeNV vec2 hitAttributes;

layout(shaderRecordNV) buffer block
{
	MaterialConstants materialConstants;
};

#define INTERPOLATE(v, f, b) (f(v[0]) * b.x + f(v[1]) * b.y + f(v[2]) * b.z)

void main()
{
	payload.hitT = gl_HitTNV;

	vec3 barycentrics = vec3(
		1 - hitAttributes.x - hitAttributes.y,
		hitAttributes.x,
		hitAttributes.y);

	uint triIndices[3];
	triIndices[0] = indexBuffer[materialConstants.firstIndex + gl_PrimitiveID*3 + 0];
	triIndices[1] = indexBuffer[materialConstants.firstIndex + gl_PrimitiveID*3 + 1];
	triIndices[2] = indexBuffer[materialConstants.firstIndex + gl_PrimitiveID*3 + 2];

	Vertex triVertices[3];
	triVertices[0] = vertexBuffer[triIndices[0]];
	triVertices[1] = vertexBuffer[triIndices[1]];
	triVertices[2] = vertexBuffer[triIndices[2]];

	vec2 uv = INTERPOLATE(triVertices, getTexcoord, barycentrics);
	payload.albedo.rgb = texture(sampler2D(textureDescriptors[materialConstants.albedoTextureId], defaultSampler), uv).rgb;
	payload.albedo *= materialConstants.baseColor.rgb;

	payload.normal = normalize(INTERPOLATE(triVertices, getNormal, barycentrics));
}
