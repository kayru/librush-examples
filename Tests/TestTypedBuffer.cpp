#include "TestFramework.h"

#include <Common/Utils.h>
#include <Rush/GfxDevice.h>
#include <Rush/UtilLog.h>

#include <cstring>

using namespace Test;
using namespace Rush;

// Validates that rwBuffers and rwTypedBuffers bind to distinct buffers
// when both are present in the same descriptor set.
class TypedBufferBindingTest final : public GfxTestCase
{
public:
	explicit TypedBufferBindingTest(GfxContext* ctx)
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

		const char* shaderName = RUSH_SHADER_NAME("TestTypedBuffer.hlsl");
#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
		const bool shaderSupported = caps.shaderTypeSupported(GfxShaderSourceType_MSL_BIN);
#else
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

		// Buffer A: untyped storage (rwBuffer) for output
		{
			GfxBufferDesc desc(GfxBufferFlags::Storage, GfxFormat_Unknown, 1, sizeof(u32));
			desc.hostVisible = true;
			desc.debugName = "TestTypedBufferA";
			m_bufferA = Gfx_CreateBuffer(desc);
		}

		// Buffer B: typed storage (rwTypedBuffer) for output
		{
			GfxBufferDesc desc(GfxBufferFlags::Storage, GfxFormat_R32_Uint, 1, sizeof(u32));
			desc.hostVisible = true;
			desc.debugName = "TestTypedBufferB";
			m_bufferB = Gfx_CreateBuffer(desc);
		}

		// Zero-initialize both buffers
		{
			const u32 zero = 0;
			GfxMappedBuffer mapped = Gfx_MapBuffer(m_bufferA);
			if (mapped.data)
			{
				std::memset(mapped.data, 0, mapped.size);
			}
			Gfx_UnmapBuffer(mapped);
		}
		{
			GfxMappedBuffer mapped = Gfx_MapBuffer(m_bufferB);
			if (mapped.data)
			{
				std::memset(mapped.data, 0, mapped.size);
			}
			Gfx_UnmapBuffer(mapped);
		}

		GfxComputePipelineDesc pipelineDesc;
		pipelineDesc.cs = m_computeShader.get();
		pipelineDesc.bindings.descriptorSets[0].rwBuffers = 1;
		pipelineDesc.bindings.descriptorSets[0].rwTypedBuffers = 1;
		pipelineDesc.bindings.descriptorSets[0].stageFlags = GfxStageFlags::Compute;
		pipelineDesc.workGroupSize = {1, 1, 1};

		m_pipeline = Gfx_CreateComputePipeline(pipelineDesc);
		if (!m_pipeline.valid())
		{
			m_skipReason = "Failed to create compute technique.";
			return;
		}

		m_ready = true;
	}

	void render(GfxContext* ctx, GfxTexture) override
	{
		if (!m_ready)
		{
			logSkipOnce();
			return;
		}

		Gfx_SetComputePipeline(ctx, m_pipeline);
		Gfx_SetStorageBuffer(ctx, 0, m_bufferA);
		Gfx_SetStorageBuffer(ctx, 1, m_bufferB);
		Gfx_Dispatch(ctx, 1, 1, 1);
	}

	TestResult validate(GfxContext*, const TestImage*) override
	{
		if (!m_ready)
		{
			return TestResult::pass();
		}

		Gfx_Finish();

		// Validate buffer A (rwBuffer) contains 0xAAAAAAAA
		{
			GfxMappedBuffer mapped = Gfx_MapBuffer(m_bufferA);
			if (!mapped.data || mapped.size < sizeof(u32))
			{
				Gfx_UnmapBuffer(mapped);
				return TestResult::fail("Failed to map buffer A for readback");
			}
			const u32 value = *reinterpret_cast<const u32*>(mapped.data);
			Gfx_UnmapBuffer(mapped);
			if (value != 0xAAAAAAAAu)
			{
				return TestResult::fail("Buffer A: got 0x%08X expected 0xAAAAAAAA", value);
			}
		}

		// Validate buffer B (rwTypedBuffer) contains 0xBBBBBBBB
		{
			GfxMappedBuffer mapped = Gfx_MapBuffer(m_bufferB);
			if (!mapped.data || mapped.size < sizeof(u32))
			{
				Gfx_UnmapBuffer(mapped);
				return TestResult::fail("Failed to map buffer B for readback");
			}
			const u32 value = *reinterpret_cast<const u32*>(mapped.data);
			Gfx_UnmapBuffer(mapped);
			if (value != 0xBBBBBBBBu)
			{
				return TestResult::fail("Buffer B (rwTypedBuffer): got 0x%08X expected 0xBBBBBBBB", value);
			}
		}

		return TestResult::pass();
	}

private:
	GfxOwn<GfxComputeShader> m_computeShader;
	GfxOwn<GfxBuffer> m_bufferA;
	GfxOwn<GfxBuffer> m_bufferB;
	GfxOwn<GfxComputePipeline> m_pipeline;
};

RUSH_REGISTER_TEST(TypedBufferBindingTest, "gfx",
	"Validates that rwBuffers and rwTypedBuffers bind to distinct buffers.");
