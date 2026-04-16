#include "TestFramework.h"

#include <Common/Utils.h>
#include <Rush/GfxDevice.h>
#include <Rush/MathCommon.h>
#include <Rush/UtilLog.h>

using namespace Test;
using namespace Rush;

class RayTracingMaterialIndexTest final : public GfxTestCase
{
public:
	explicit RayTracingMaterialIndexTest(GfxContext* ctx)
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
		bindings.descriptorSets[0].rwBuffers = 2;
		bindings.descriptorSets[0].accelerationStructures = 1;
		bindings.descriptorSets[0].stageFlags = GfxStageFlags::RayTracing;

		GfxRayTracingPipelineDesc pipelineDesc = {};
		GfxShaderSource rayGenSource = loadShaderFromFile(RUSH_SHADER_NAME("TestRayTracingMaterialIndex.metal"));
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

		GfxBufferDesc outputDesc(GfxBufferFlags::Storage, GfxFormat_Unknown, 1, sizeof(u32) * 4);
		outputDesc.hostVisible = true;
		outputDesc.debugName = "TestRayTracingMaterialIndexOutput";
		m_outputBuffer = Gfx_CreateBuffer(outputDesc);

		const u32 materialIndices[1] = { 7 };
		m_materialIndexBuffer = Gfx_CreateBuffer(GfxBufferFlags::Storage, GfxFormat_Unknown, 1,
			u32(sizeof(materialIndices[0])), materialIndices);
		if (!m_outputBuffer.valid() || !m_materialIndexBuffer.valid())
		{
			m_skipReason = "Failed to create material index buffers.";
			return;
		}

		m_expected[0] = 7;
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

		Gfx_SetStorageBuffer(ctx, 0, m_outputBuffer);
		Gfx_SetStorageBuffer(ctx, 1, m_materialIndexBuffer);
		Gfx_SetAccelerationStructure(ctx, 0, m_scene.tlas);
		Gfx_TraceRays(ctx, m_pipeline, m_hitGroupSbt, 1, 1, 1);
	}

	TestResult validate(GfxContext*, const TestImage*) override
	{
		if (!m_ready)
		{
			return TestResult::pass();
		}

		return validateBufferU32(m_outputBuffer, m_expected, 4, "%u");
	}

private:
	GfxOwn<GfxRayTracingPipeline> m_pipeline;
	SimpleRTScene                 m_scene;
	GfxOwn<GfxBuffer>             m_hitGroupSbt;
	GfxOwn<GfxBuffer>             m_materialIndexBuffer;
	GfxOwn<GfxBuffer>             m_outputBuffer;

	u32 m_expected[4] = {};
};

RUSH_REGISTER_TEST(RayTracingMaterialIndexTest, "raytracing",
	"Validates Metal RT material index reads via hit primitive ID.");
