#include "TestFramework.h"

#include <Common/Utils.h>
#include <Rush/GfxDevice.h>
#include <Rush/UtilLog.h>

using namespace Test;
using namespace Rush;

class RayTracingTextureArrayTest final : public GfxTestCase
{
public:
	explicit RayTracingTextureArrayTest(GfxContext* ctx)
	{
		if (!ctx)
		{
			return;
		}

		const GfxCapability& caps = Gfx_GetCapability();
		if (!caps.compute)
		{
			m_skipReason = "Compute is not supported.";
			return;
		}

#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
		const char* shaderName = RUSH_SHADER_NAME("TestRayTracing.metal");
		const bool shaderSupported = caps.shaderTypeSupported(GfxShaderSourceType_MSL_BIN);
#else
		const char* shaderName = RUSH_SHADER_NAME("TestRayTracing.hlsl");
		const bool shaderSupported = caps.shaderTypeSupported(GfxShaderSourceType_SPV);
#endif

		if (!shaderSupported)
		{
			m_skipReason = "Required shader source type is not supported.";
			return;
		}

		GfxShaderSource shaderSource = loadShaderFromFile(shaderName);
		if (shaderSource.empty())
		{
			m_skipReason = "Failed to load compute shader.";
			return;
		}

		m_computeShader = Gfx_CreateComputeShader(shaderSource);
		if (!m_computeShader.valid())
		{
			m_skipReason = "Failed to create compute shader.";
			return;
		}

		m_samplerNearest = Gfx_CreateSamplerState(GfxSamplerDesc::makePoint());
		m_samplerLinear = Gfx_CreateSamplerState(GfxSamplerDesc::makeLinear());

		const ColorRGBA8 tex0Pixels[4] = {
			ColorRGBA8(255, 0, 0, 255),
			ColorRGBA8(0, 255, 0, 255),
			ColorRGBA8(255, 0, 0, 255),
			ColorRGBA8(0, 255, 0, 255),
		};
		const ColorRGBA8 tex1Pixels[4] = {
			ColorRGBA8(0, 255, 255, 255),
			ColorRGBA8(255, 0, 255, 255),
			ColorRGBA8(0, 255, 255, 255),
			ColorRGBA8(255, 0, 255, 255),
		};

		GfxTextureDesc textureDesc = GfxTextureDesc::make2D(2, 2, GfxFormat_RGBA8_Unorm);
		m_textures[0] = Gfx_CreateTexture(textureDesc, tex0Pixels);
		m_textures[1] = Gfx_CreateTexture(textureDesc, tex1Pixels);

		GfxBufferDesc outputDesc(GfxBufferFlags::Storage, GfxFormat_Unknown, 1, sizeof(u32) * 4);
		outputDesc.hostVisible = true;
		outputDesc.debugName = "TestRayTracingOutput";
		m_outputBuffer = Gfx_CreateBuffer(outputDesc);

		GfxComputePipelineDesc pipelineDesc;
		pipelineDesc.cs = m_computeShader.get();
		pipelineDesc.bindings.descriptorSets[0].samplers = 3;
		pipelineDesc.bindings.descriptorSets[0].textures = 2;
		pipelineDesc.bindings.descriptorSets[0].rwBuffers = 1;
		pipelineDesc.bindings.descriptorSets[0].flags = GfxDescriptorSetFlags::TextureArray;
		pipelineDesc.bindings.descriptorSets[0].stageFlags = GfxStageFlags::Compute;
		pipelineDesc.workGroupSize = {1, 1, 1};

		m_pipeline = Gfx_CreateComputePipeline(pipelineDesc);
		if (!m_pipeline.valid())
		{
			m_skipReason = "Failed to create compute technique.";
			return;
		}

		m_expected[0] = packColor(ColorRGBA8(255, 0, 0, 255));
		m_expected[1] = packColor(ColorRGBA8(0, 255, 255, 255));
		m_expected[2] = packColor(ColorRGBA8(128, 128, 0, 255));
		m_expected[3] = packColor(ColorRGBA8(128, 128, 255, 255));

		m_ready = true;
	}

	void render(GfxContext* ctx) override
	{
		if (!m_ready)
		{
			logSkipOnce();
			return;
		}

		Gfx_SetComputePipeline(ctx, m_pipeline);
		Gfx_SetSampler(ctx, 0, m_samplerNearest);
		Gfx_SetSampler(ctx, 1, m_samplerLinear);
		Gfx_SetSampler(ctx, 2, m_samplerNearest);
		Gfx_SetTexture(ctx, 0, m_textures[0]);
		Gfx_SetTexture(ctx, 1, m_textures[1]);
		Gfx_SetStorageBuffer(ctx, 0, m_outputBuffer);
		Gfx_Dispatch(ctx, 1, 1, 1);
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

	GfxOwn<GfxComputeShader> m_computeShader;
	GfxOwn<GfxSampler>       m_samplerNearest;
	GfxOwn<GfxSampler>       m_samplerLinear;
	GfxOwn<GfxTexture>       m_textures[2];
	GfxOwn<GfxBuffer>        m_outputBuffer;
	GfxOwn<GfxComputePipeline> m_pipeline;

	u32 m_expected[4] = {};
};

RUSH_REGISTER_TEST(RayTracingTextureArrayTest, "raytracing",
	"Validates texture array indexing and sampling via compute readback.");
