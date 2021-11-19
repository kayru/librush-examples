#version 460
#extension GL_EXT_ray_tracing : enable

#include "Common.glsl"

layout(location = 0) rayPayloadInEXT DefaultPayload payload;
hitAttributeEXT vec2 hitAttributes;

layout(shaderRecordEXT) buffer block
{
	MaterialConstants materialConstants;
};

#define INTERPOLATE(v, f, b) (f(v[0]) * b.x + f(v[1]) * b.y + f(v[2]) * b.z)

void main()
{
	payload.hitT = gl_HitTEXT;

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

	vec4 albedoSample   = texture(sampler2D(textureDescriptors[materialConstants.albedoTextureId],   defaultSampler), uv);
	vec4 specularSample = texture(sampler2D(textureDescriptors[materialConstants.specularTextureId], defaultSampler), uv);

	if (materialConstants.materialMode == PT_MATERIAL_MODE_PBR_METALLIC_ROUGHNESS)
	{
		payload.baseColor.rgb = albedoSample.rgb * materialConstants.albedoFactor.rgb;
		payload.metalness = materialConstants.metallicFactor * specularSample.b;
		payload.roughness = materialConstants.roughnessFactor * specularSample.g;
	}
	else
	{
		payload.metalness = max3(specularSample.rgb * materialConstants.specularFactor.rgb);
		payload.roughness = 1.0 - specularSample.a * materialConstants.roughnessFactor;
		payload.baseColor.rgb = mix(albedoSample.rgb * materialConstants.albedoFactor.rgb, specularSample.rgb * materialConstants.specularFactor.rgb, payload.metalness);
	}

	payload.reflectance = materialConstants.reflectance;

	payload.normal = normalize(INTERPOLATE(triVertices, getNormal, barycentrics));

	payload.geoNormal = cross(getPosition(triVertices[1]) - getPosition(triVertices[0]), getPosition(triVertices[2]) - getPosition(triVertices[0]));
	payload.geoNormal = normalize(payload.geoNormal);

	if (gl_HitKindEXT == 255)
	{
		payload.normal = -payload.normal;
		payload.geoNormal = -payload.geoNormal;
	}
}
