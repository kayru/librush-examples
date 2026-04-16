#include "TestFramework.h"

#include <Common/Utils.h>
#include <Rush/GfxDevice.h>
#include <Rush/UtilLog.h>

using namespace Test;
using namespace Rush;

class RayTracingMaterialBindingTest final : public GfxTestCase
{
public:
	explicit RayTracingMaterialBindingTest(GfxContext* ctx)
	{
		if (!ctx)
		{
			return;
		}

		const GfxCapability& caps = Gfx_GetCapability();
		if (!caps.rayTracingPipeline)
		{
			m_skipReason = "Ray tracing pipeline is not supported.";
			return;
		}

#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
		if (!caps.shaderTypeSupported(GfxShaderSourceType_MSL_BIN))
		{
			m_skipReason = "MSL shaders are not supported.";
			return;
		}
#else
		m_skipReason = "Metal-only test.";
		return;
#endif

		GfxShaderBindingDesc bindings = {};
		bindings.useDefaultDescriptorSet = true;
		bindings.descriptorSets[0].samplers = 1;
		bindings.descriptorSets[0].textures = 2;
		bindings.descriptorSets[0].rwBuffers = 3;
		bindings.descriptorSets[0].accelerationStructures = 1;
		bindings.descriptorSets[0].flags = GfxDescriptorSetFlags::TextureArray;
		bindings.descriptorSets[0].stageFlags = GfxStageFlags::RayTracing;

		GfxRayTracingPipelineDesc pipelineDesc = {};
		GfxShaderSource rayGenSource = loadShaderFromFile(RUSH_SHADER_NAME("TestRayTracingMaterialBinding.metal"));
		if (rayGenSource.empty())
		{
			m_skipReason = "Failed to load Metal ray tracing shader.";
			return;
		}

		pipelineDesc.rayGen = rayGenSource;
		pipelineDesc.maxRecursionDepth = 1;
		pipelineDesc.bindings = bindings;

		m_pipeline = Gfx_CreateRayTracingPipeline(pipelineDesc);
		if (!m_pipeline.valid())
		{
			m_skipReason = "Failed to create ray tracing pipeline.";
			return;
		}

		if (!createSimpleTriangleScene(ctx, m_scene, GfxBufferFlags::None, m_skipReason))
		{
			return;
		}

		m_samplerNearest = Gfx_CreateSamplerState(GfxSamplerDesc::makePoint());

		const ColorRGBA8 tex0Pixels[4] = {
			ColorRGBA8(255, 0, 0, 255),
			ColorRGBA8(0, 255, 0, 255),
			ColorRGBA8(255, 0, 0, 255),
			ColorRGBA8(0, 255, 0, 255),
		};
		const ColorRGBA8 tex1Pixels[4] = {
			ColorRGBA8(0, 0, 255, 255),
			ColorRGBA8(255, 255, 0, 255),
			ColorRGBA8(0, 0, 255, 255),
			ColorRGBA8(255, 255, 0, 255),
		};

		GfxTextureDesc textureDesc = GfxTextureDesc::make2D(2, 2, GfxFormat_RGBA8_Unorm);
		m_textures[0] = Gfx_CreateTexture(textureDesc, tex0Pixels);
		m_textures[1] = Gfx_CreateTexture(textureDesc, tex1Pixels);

		GfxBufferDesc outputDesc(GfxBufferFlags::Storage, GfxFormat_Unknown, 1, sizeof(u32) * 4);
		outputDesc.hostVisible = true;
		outputDesc.debugName = "TestRayTracingMaterialBindingOutput";
		m_outputBuffer = Gfx_CreateBuffer(outputDesc);

		MaterialConstants material = {};
		material.albedoFactor = Vec4(1.0f);
		material.specularFactor = Vec4(0.0f);
		material.albedoTextureId = 1;
		material.specularTextureId = 0;
		material.normalTextureId = 0;
		material.firstIndex = 0;
		material.alphaMode = 0;
		material.metallicFactor = 0.0f;
		material.roughnessFactor = 1.0f;
		material.reflectance = 0.0f;
		material.materialMode = 0;

		m_materialBuffer = Gfx_CreateBuffer(GfxBufferFlags::Storage, GfxFormat_Unknown, 1, sizeof(MaterialConstants), &material);

		const u32 materialIndices[1] = { 0 };
		m_materialIndexBuffer = Gfx_CreateBuffer(GfxBufferFlags::Storage, GfxFormat_Unknown, 1,
			u32(sizeof(materialIndices[0])), materialIndices);

		m_expected[0] = packColor(ColorRGBA8(255, 255, 0, 255));
		m_expected[1] = 0;
		m_expected[2] = 0;
		m_expected[3] = 0;
		m_ready = true;
	}

	void render(GfxContext* ctx, GfxTexture) override
	{
		if (!m_ready)
		{
			logSkipOnce();
			return;
		}

		Gfx_SetSampler(ctx, 0, m_samplerNearest);
		Gfx_SetTexture(ctx, 0, m_textures[0]);
		Gfx_SetTexture(ctx, 1, m_textures[1]);
		Gfx_SetStorageBuffer(ctx, 0, m_outputBuffer);
		Gfx_SetStorageBuffer(ctx, 1, m_materialBuffer);
		Gfx_SetStorageBuffer(ctx, 2, m_materialIndexBuffer);
		Gfx_SetAccelerationStructure(ctx, 0, m_scene.tlas);
		Gfx_TraceRays(ctx, m_pipeline, m_hitGroupSbt, 1, 1, 1);
	}

	TestResult validate(GfxContext*, const TestImage*) override
	{
		if (!m_ready)
		{
			return TestResult::pass();
		}

		return validateBufferU32(m_outputBuffer, m_expected, 4);
	}

private:
	struct MaterialConstants
	{
		Vec4 albedoFactor;
		Vec4 specularFactor;
		u32 albedoTextureId;
		u32 specularTextureId;
		u32 normalTextureId;
		u32 firstIndex;
		u32 alphaMode;
		float metallicFactor;
		float roughnessFactor;
		float reflectance;
		u32 materialMode;
	};

	GfxOwn<GfxRayTracingPipeline> m_pipeline;
	SimpleRTScene                 m_scene;
	GfxOwn<GfxBuffer>             m_hitGroupSbt;
	GfxOwn<GfxBuffer>             m_materialBuffer;
	GfxOwn<GfxBuffer>             m_materialIndexBuffer;
	GfxOwn<GfxBuffer>             m_outputBuffer;
	GfxOwn<GfxSampler>            m_samplerNearest;
	GfxOwn<GfxTexture>            m_textures[2];

	u32 m_expected[4] = {};
};

RUSH_REGISTER_TEST(RayTracingMaterialBindingTest, "raytracing",
	"Validates Metal RT material data and texture array access.");
