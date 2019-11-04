#ifndef INCLUDED_COMMON_GLSL
#define INCLUDED_COMMON_GLSL

// global resources

layout(set=0, binding=0)
uniform SceneConstants
{
	mat4 matView;
	mat4 matProj;
	mat4 matViewProj;
	mat4 matViewProjInv;
	vec4 cameraPosition;
	ivec2 outputSize;
	uint frameIndex;
	uint padding;
};

layout(set=0, binding=1)
uniform sampler defaultSampler;

layout(set=0, binding=2, rgba32f)
uniform image2D outputImage;

layout(set=0, binding=3, std430)
buffer IndexBuffer
{
	uint indexBuffer[];
};

struct Vertex
{
	float position[3];
	float normal[3];
	float texcoord[2];
};

layout(set=0, binding=4, std430)
buffer VertexBuffer
{
	Vertex vertexBuffer[];
};

vec3 getPosition(Vertex v) { return vec3(v.position[0], v.position[1], v.position[2]); }
vec3 getNormal(Vertex v) { return vec3(v.normal[0], v.normal[1], v.normal[2]); }
vec2 getTexcoord(Vertex v) { return vec2(v.texcoord[0], v.texcoord[1]); }

layout(set=0, binding=5)
uniform accelerationStructureNV TLAS;

#define MaxTextures 255
layout(set=1, binding = 0)
uniform texture2D textureDescriptors[MaxTextures];

// common types and functions

struct MaterialConstants
{
	vec4 baseColor;
	uint albedoTextureId;
	uint firstIndex;
	uint padding0;
	uint padding1;
};

struct DefaultPayload
{
	float hitT;
	vec3 albedo;
	vec3 normal;
};

struct RayDesc
{
	vec3 origin;
	float minT;
	vec3 direction;
	float maxT;
};

#endif // INCLUDED_COMMON_GLSL
