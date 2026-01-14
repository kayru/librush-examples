#include "TestFramework.h"

#include <Common/Utils.h>
#include <Rush/GfxDevice.h>
#include <Rush/MathCommon.h>
#include <Rush/UtilLog.h>

#include <cstring>

using namespace Test;
using namespace Rush;

class RayTracingPipelineTest final : public GfxTestCase
{
public:
	explicit RayTracingPipelineTest(GfxContext* ctx)
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
		if (!caps.shaderTypeSupported(GfxShaderSourceType_SPV))
		{
			m_skipReason = "SPIR-V shaders are not supported.";
			return;
		}
#endif

		GfxShaderBindingDesc bindings = {};
		bindings.useDefaultDescriptorSet = true;
#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
		bindings.descriptorSets[0].rwBuffers = 3;
#else
		bindings.descriptorSets[0].rwBuffers = 1;
#endif
		bindings.descriptorSets[0].accelerationStructures = 1;
		bindings.descriptorSets[0].stageFlags = GfxStageFlags::RayTracing;

		// TODO: Standardize the ray tracing test shaders on a single language across backends.
		GfxRayTracingPipelineDesc pipelineDesc = {};
#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
		GfxShaderSource rayGenSource = loadShaderFromFile(RUSH_SHADER_NAME("TestRayTracingPipeline.metal"));
		if (rayGenSource.empty())
		{
			m_skipReason = "Failed to load Metal ray tracing shader.";
			return;
		}
		GfxShaderSource missSource = rayGenSource;
		missSource.entry = "missShader";
		GfxShaderSource closestHitSource = rayGenSource;
		closestHitSource.entry = "closestHitShader";
		pipelineDesc.rayGen = rayGenSource;
		pipelineDesc.miss = missSource;
		pipelineDesc.closestHit = closestHitSource;
#else
		pipelineDesc.rayGen     = loadShaderFromFile(RUSH_SHADER_NAME("TestRayTracingPipeline.rgen"));
		pipelineDesc.miss       = loadShaderFromFile(RUSH_SHADER_NAME("TestRayTracingPipeline.rmiss"));
		pipelineDesc.closestHit = loadShaderFromFile(RUSH_SHADER_NAME("TestRayTracingPipeline.rchit"));
#endif
		pipelineDesc.maxRecursionDepth = 1;
		pipelineDesc.bindings = bindings;

		if (pipelineDesc.rayGen.empty() || pipelineDesc.miss.empty() || pipelineDesc.closestHit.empty())
		{
			m_skipReason = "Failed to load ray tracing shaders.";
			return;
		}

		m_pipeline = Gfx_CreateRayTracingPipeline(pipelineDesc);
		if (!m_pipeline.valid())
		{
			m_skipReason = "Failed to create ray tracing pipeline.";
			return;
		}

		if (!createScene(ctx))
		{
			return;
		}

		if (!createHitGroupSbt())
		{
			return;
		}

		GfxBufferDesc outputDesc(GfxBufferFlags::Storage, GfxFormat_Unknown, 1, sizeof(u32) * 4);
		outputDesc.hostVisible = true;
		outputDesc.debugName = "TestRayTracingPipelineOutput";
		m_outputBuffer = Gfx_CreateBuffer(outputDesc);

#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
		m_expected[0] = 3;
#else
		m_expected[0] = 2;
#endif
		m_expected[1] = 1;
		m_expected[2] = 0;
		m_expected[3] = 0;
		m_ready = true;
	}

	void render(GfxContext* ctx) override
	{
		if (!m_ready)
		{
			if (m_loggedSkip && !m_skipReason.empty())
			{
				return;
			}
			if (!m_skipReason.empty())
			{
				RUSH_LOG("[Test] SKIP: %s", m_skipReason.c_str());
				m_loggedSkip = true;
			}
			return;
		}

		Gfx_SetStorageBuffer(ctx, 0, m_outputBuffer);
#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
		Gfx_SetStorageBuffer(ctx, 1, m_vertexBuffer);
		Gfx_SetStorageBuffer(ctx, 2, m_indexBuffer);
#endif
		Gfx_SetAccelerationStructure(ctx, 0, m_tlas);
		Gfx_TraceRays(ctx, m_pipeline, m_hitGroupSbt, 1, 1, 1);
	}

	TestResult validate(GfxContext*, const TestImage*) override
	{
		if (!m_ready)
		{
			return TestResult::pass();
		}

		Gfx_Finish();

		GfxMappedBuffer mapped = Gfx_MapBuffer(m_outputBuffer);
		if (!mapped.data || mapped.size < sizeof(m_expected))
		{
			Gfx_UnmapBuffer(mapped);
			return TestResult::fail("Failed to map output buffer for readback");
		}

		const u32* data = reinterpret_cast<const u32*>(mapped.data);
		const size_t expectedCount = sizeof(m_expected) / sizeof(m_expected[0]);
		for (size_t i = 0; i < expectedCount; ++i)
		{
			if (data[i] != m_expected[i])
			{
				Gfx_UnmapBuffer(mapped);
				return TestResult::fail("Output mismatch at %zu: got %u expected %u",
				    i, data[i], m_expected[i]);
			}
		}

		Gfx_UnmapBuffer(mapped);
		return TestResult::pass();
	}

private:
	bool createScene(GfxContext* ctx)
	{
		DynamicArray<Vec3> vertices;
		vertices.push_back(Vec3(1.0f, 0.0f, 0.0f));
		vertices.push_back(Vec3(0.0f, 1.0f, 0.0f));
		vertices.push_back(Vec3(-1.0f, -1.0f, 0.0f));

		DynamicArray<u32> indices;
		indices.push_back(0);
		indices.push_back(1);
		indices.push_back(2);

		m_vertexBuffer = Gfx_CreateBuffer(GfxBufferFlags::RayTracing | GfxBufferFlags::Storage, GfxFormat::GfxFormat_RGB32_Float,
			u32(vertices.size()), u32(sizeof(Vec3)), vertices.data());
		m_indexBuffer = Gfx_CreateBuffer(GfxBufferFlags::RayTracing | GfxBufferFlags::Storage, GfxFormat::GfxFormat_R32_Uint,
			u32(indices.size()), 4, indices.data());

		if (!m_vertexBuffer.valid() || !m_indexBuffer.valid())
		{
			m_skipReason = "Failed to create geometry buffers.";
			return false;
		}

		DynamicArray<GfxRayTracingGeometryDesc> geometries;
		GfxRayTracingGeometryDesc geometryDesc;
		geometryDesc.indexBuffer  = m_indexBuffer.get();
		geometryDesc.indexFormat  = GfxFormat::GfxFormat_R32_Uint;
		geometryDesc.indexCount   = u32(indices.size());
		geometryDesc.vertexBuffer = m_vertexBuffer.get();
		geometryDesc.vertexFormat = GfxFormat::GfxFormat_RGB32_Float;
		geometryDesc.vertexStride = u32(sizeof(Vec3));
		geometryDesc.vertexCount  = u32(vertices.size());
		geometryDesc.isOpaque     = true;
		geometries.push_back(geometryDesc);

		GfxAccelerationStructureDesc blasDesc;
		blasDesc.type         = GfxAccelerationStructureType::BottomLevel;
		blasDesc.geometyCount = u32(geometries.size());
		blasDesc.geometries   = geometries.data();
		m_blas = Gfx_CreateAccelerationStructure(blasDesc);

		GfxAccelerationStructureDesc tlasDesc;
		tlasDesc.type          = GfxAccelerationStructureType::TopLevel;
		tlasDesc.instanceCount = 1;
		m_tlas = Gfx_CreateAccelerationStructure(tlasDesc);

		if (!m_blas.valid() || !m_tlas.valid())
		{
			m_skipReason = "Failed to create acceleration structures.";
			return false;
		}

		GfxOwn<GfxBuffer> instanceBuffer = Gfx_CreateBuffer(GfxBufferFlags::Transient | GfxBufferFlags::RayTracing);
		auto instanceData = Gfx_BeginUpdateBuffer<GfxRayTracingInstanceDesc>(ctx, instanceBuffer.get(), tlasDesc.instanceCount);
		instanceData[0].init();
		instanceData[0].accelerationStructureHandle = Gfx_GetAccelerationStructureHandle(m_blas);
		Gfx_EndUpdateBuffer(ctx, instanceBuffer);

		Gfx_BuildAccelerationStructure(ctx, m_blas);
		Gfx_AddFullPipelineBarrier(ctx);

		Gfx_BuildAccelerationStructure(ctx, m_tlas, instanceBuffer);
		Gfx_AddFullPipelineBarrier(ctx);

		return true;
	}

	bool createHitGroupSbt()
	{
#if RUSH_RENDER_API == RUSH_RENDER_API_MTL
		return true;
#else
		const GfxCapability& caps = Gfx_GetCapability();
		if (!caps.rtShaderHandleSize || !caps.rtSbtAlignment)
		{
			m_skipReason = "Ray tracing shader handle size is unavailable.";
			return false;
		}

		const u32 handleSize = caps.rtShaderHandleSize;
		const u32 stride = alignCeiling(handleSize, caps.rtSbtAlignment);
		DynamicArray<u8> sbtData;
		sbtData.resize(stride, 0);

		const u8* handle = Gfx_GetRayTracingShaderHandle(m_pipeline, GfxRayTracingShaderType::HitGroup, 0);
		if (!handle)
		{
			m_skipReason = "Failed to retrieve hit group shader handle.";
			return false;
		}

		memcpy(sbtData.data(), handle, handleSize);

		m_hitGroupSbt = Gfx_CreateBuffer(GfxBufferFlags::RayTracing, 1, stride, sbtData.data());
		if (!m_hitGroupSbt.valid())
		{
			m_skipReason = "Failed to create hit group SBT.";
			return false;
		}

		return true;
#endif
	}

	bool m_ready = false;
	bool m_loggedSkip = false;
	String m_skipReason;

	GfxOwn<GfxRayTracingPipeline>    m_pipeline;
	GfxOwn<GfxBuffer>                m_vertexBuffer;
	GfxOwn<GfxBuffer>                m_indexBuffer;
	GfxOwn<GfxAccelerationStructure> m_blas;
	GfxOwn<GfxAccelerationStructure> m_tlas;
	GfxOwn<GfxBuffer>                m_hitGroupSbt;
	GfxOwn<GfxBuffer>                m_outputBuffer;

	u32 m_expected[4] = {};
};

RUSH_REGISTER_TEST(RayTracingPipelineTest, "raytracing",
	"Validates raygen/miss/closest-hit execution, payload flow, and buffer access.");
