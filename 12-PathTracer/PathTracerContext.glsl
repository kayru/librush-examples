#ifndef INCLUDED_PT_CONTEXT
#define INCLUDED_PT_CONTEXT

// Resource access for the shared fillPayload/loop is via the PT_* macros below, so one
// body compiles for every backend. Metal is keyed on __METAL_VERSION__; each Vulkan entry
// #defines its config token before including this:
//   PT_CONFIG_SBT_RAYGEN  owns the SBT payload + render loop
//   PT_CONFIG_SBT_HIT     closest-hit, only needs fillPayload (rmiss needs no token)

struct PtRay
{
	vec3  origin;
	float minT;
	vec3  direction;
	float maxT;
};

struct PtHit
{
	bool  valid;
	float t;
	uint  primId;
	vec2  bary;        // (b1, b2); b0 = 1 - b1 - b2
	bool  frontFacing;
};

struct LightSample
{
	vec3  w;
	vec3  value;
	float pdfW;
};

struct PtPayload
{
	float hitT;
	vec3  baseColor;
	float metalness;
	float roughness;
	float reflectance;
	vec3  shadingNormal; // pre normal-map
	vec3  normal;        // post normal-map
	vec3  geoNormal;
	vec3  tangent;
	vec3  bitangent;
	vec2  texcoord;
};

#ifdef __METAL_VERSION__

// Resources arrive through argument-buffer structs declared in PathTracer.metal.
struct PathTracerContext
{
	constant PathTracerSet0* s0;
	constant PathTracerSet1* s1;
};

#define PT_SCENE(ctx, field)        ((ctx).s0->scene->field)
#define PT_INDEX(ctx, i)            ((ctx).s0->indexBuffer[(i)])
#define PT_VERTEX(ctx, i)           ((ctx).s0->vertexBuffer[(i)])
#define PT_TEXTURE(ctx, id, uv)     ((ctx).s1->textures[(id)].sample((ctx).s0->defaultSampler, (uv)))
#define PT_ENVMAP(ctx, uv)          ((ctx).s0->envmapTexture.sample((ctx).s0->defaultSampler, (uv)))
#define PT_ENVDIST(ctx, i)          ((ctx).s0->envmapDistribution[(i)])
#define PT_ENVDIST_VALID(ctx)       ((ctx).s0->envmapDistribution != nullptr)
#define PT_OUTPUT_READ(ctx, px)     ((ctx).s0->outputImage.read(uint2(px)).xyz)
#define PT_OUTPUT_WRITE(ctx, px, v) ((ctx).s0->outputImage.write(float4((v), 1.0f), uint2(px)))
#define PT_FOCUS_WRITE(ctx, val)    ((ctx).s0->focusFeedback[0] = (val))

// Vertex members are packed; bridge to aligned vecs.
#define PT_VTX_POS(v) float3((v).position)
#define PT_VTX_NRM(v) float3((v).normal)
#define PT_VTX_UV(v)  float2((v).texcoord)
#define PT_VTX_TAN(v) float4((v).tangent)

// Metal runs the whole path tracer inline in one kernel.
#define PT_HAS_RENDER_LOOP

#else // GLSL / Vulkan: resources are module globals (Common.glsl), context carries nothing.

struct PathTracerContext
{
	int unused;
};

#define PT_SCENE(ctx, field)        (field)
#define PT_INDEX(ctx, i)            (indexBuffer[(i)])
#define PT_VERTEX(ctx, i)           (vertexBuffer[(i)])
#define PT_TEXTURE(ctx, id, uv)     (texture(sampler2D(textureDescriptors[(id)], defaultSampler), (uv)))
#define PT_ENVMAP(ctx, uv)          (texture(sampler2D(envmapTexture, defaultSampler), (uv)))
#define PT_ENVDIST(ctx, i)          (envmapDistributionBuffer[(i)])
#define PT_ENVDIST_VALID(ctx)       (true)
#define PT_OUTPUT_READ(ctx, px)     (imageLoad(outputImage, ivec2(px)).rgb)
#define PT_OUTPUT_WRITE(ctx, px, v) imageStore(outputImage, ivec2(px), vec4((v), 1.0))
#define PT_FOCUS_WRITE(ctx, val)    focusFeedback[0] = (val)

// Vertex members are float[N] with accessors in Common.glsl.
#define PT_VTX_POS(v) getPosition(v)
#define PT_VTX_NRM(v) getNormal(v)
#define PT_VTX_UV(v)  getTexcoord(v)
#define PT_VTX_TAN(v) getTangent(v)

// The Vulkan ray-generation shader owns the SBT payload and drives the render loop.
#ifdef PT_CONFIG_SBT_RAYGEN
#define PT_HAS_RENDER_LOOP
layout(location = 0) rayPayloadEXT PtPayload sbtPayload;
#endif

#endif

#endif // INCLUDED_PT_CONTEXT
