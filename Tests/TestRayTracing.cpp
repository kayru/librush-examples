#include "TestFramework.h"

#include <Common/Utils.h>
#include <Rush/GfxDevice.h>
#include <Rush/MathCommon.h>
#include <Rush/UtilLog.h>

#include <cstring>

using namespace Test;
using namespace Rush;

// Validates ray query texture array access: traces rays against a scene
// with two triangle instances at different positions, each assigned a
// different instanceID. The shader uses the instanceID to index into a
// texture array and writes the sampled color to the output buffer.
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
		if (!caps.rayTracingInline)
		{
			m_skipReason = "Inline ray tracing (ray queries) is not supported.";
			return;
		}

		if (!caps.compute)
		{
			m_skipReason = "Compute is not supported.";
			return;
		}

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

		const char* shaderName = RUSH_SHADER_NAME("TestRayTracing.hlsl");
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

		if (!createScene(ctx))
		{
			return;
		}

		m_samplerNearest = Gfx_CreateSamplerState(GfxSamplerDesc::makePoint());

		// Texture 0: red (assigned to instance 0 at x=-1)
		const ColorRGBA8 tex0Pixels[4] = {
			ColorRGBA8(255, 0, 0, 255),
			ColorRGBA8(255, 0, 0, 255),
			ColorRGBA8(255, 0, 0, 255),
			ColorRGBA8(255, 0, 0, 255),
		};
		// Texture 1: blue (assigned to instance 1 at x=+1)
		const ColorRGBA8 tex1Pixels[4] = {
			ColorRGBA8(0, 0, 255, 255),
			ColorRGBA8(0, 0, 255, 255),
			ColorRGBA8(0, 0, 255, 255),
			ColorRGBA8(0, 0, 255, 255),
		};

		GfxTextureDesc textureDesc = GfxTextureDesc::make2D(2, 2, GfxFormat_RGBA8_Unorm);
		m_textures[0] = Gfx_CreateTexture(textureDesc, tex0Pixels);
		m_textures[1] = Gfx_CreateTexture(textureDesc, tex1Pixels);

		GfxBufferDesc outputDesc(GfxBufferFlags::Storage, GfxFormat_Unknown, 1, sizeof(u32) * 2);
		outputDesc.hostVisible = true;
		outputDesc.debugName = "TestRayTracingOutput";
		m_outputBuffer = Gfx_CreateBuffer(outputDesc);

		GfxComputePipelineDesc pipelineDesc;
		pipelineDesc.cs = m_computeShader.get();
		pipelineDesc.bindings.descriptorSets[0].samplers = 1;
		pipelineDesc.bindings.descriptorSets[0].textures = 2;
		pipelineDesc.bindings.descriptorSets[0].rwBuffers = 1;
		pipelineDesc.bindings.descriptorSets[0].accelerationStructures = 1;
		pipelineDesc.bindings.descriptorSets[0].stageFlags = GfxStageFlags::Compute;
		pipelineDesc.workGroupSize = {1, 1, 1};

		m_pipeline = Gfx_CreateComputePipeline(pipelineDesc);
		if (!m_pipeline.valid())
		{
			m_skipReason = "Failed to create compute pipeline.";
			return;
		}

		// Ray hitting instance 0 (instanceID=0) samples texture 0 -> red
		// Ray hitting instance 1 (instanceID=1) samples texture 1 -> blue
		m_expected[0] = packColor(ColorRGBA8(255, 0, 0, 255));
		m_expected[1] = packColor(ColorRGBA8(0, 0, 255, 255));

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
		Gfx_SetTexture(ctx, 0, m_textures[0]);
		Gfx_SetTexture(ctx, 1, m_textures[1]);
		Gfx_SetStorageBuffer(ctx, 0, m_outputBuffer);
		Gfx_SetAccelerationStructure(ctx, 0, m_tlas);
		Gfx_Dispatch(ctx, 1, 1, 1);
	}

	TestResult validate(GfxContext*, const TestImage*) override
	{
		if (!m_ready)
		{
			return TestResult::pass();
		}

		return validateBufferU32(m_outputBuffer, m_expected, 2);
	}

private:
	bool createScene(GfxContext* ctx)
	{
		// Single triangle BLAS, instanced twice at different X positions.
		const Vec3 vertices[] = {
			Vec3(-0.5f, -0.5f, 0.0f),
			Vec3( 0.5f, -0.5f, 0.0f),
			Vec3( 0.0f,  0.5f, 0.0f),
		};
		const u32 indices[] = { 0, 1, 2 };

		m_vertexBuffer = Gfx_CreateBuffer(GfxBufferFlags::RayTracing,
			GfxFormat::GfxFormat_RGB32_Float, 3, u32(sizeof(Vec3)), vertices);
		m_indexBuffer = Gfx_CreateBuffer(GfxBufferFlags::RayTracing,
			GfxFormat::GfxFormat_R32_Uint, 3, 4, indices);

		if (!m_vertexBuffer.valid() || !m_indexBuffer.valid())
		{
			m_skipReason = "Failed to create geometry buffers.";
			return false;
		}

		GfxRayTracingGeometryDesc geometryDesc;
		geometryDesc.indexBuffer  = m_indexBuffer.get();
		geometryDesc.indexFormat  = GfxFormat::GfxFormat_R32_Uint;
		geometryDesc.indexCount   = 3;
		geometryDesc.vertexBuffer = m_vertexBuffer.get();
		geometryDesc.vertexFormat = GfxFormat::GfxFormat_RGB32_Float;
		geometryDesc.vertexStride = u32(sizeof(Vec3));
		geometryDesc.vertexCount  = 3;
		geometryDesc.isOpaque     = true;

		GfxAccelerationStructureDesc blasDesc;
		blasDesc.type          = GfxAccelerationStructureType::BottomLevel;
		blasDesc.geometryCount = 1;
		blasDesc.geometries    = &geometryDesc;
		m_blas = Gfx_CreateAccelerationStructure(blasDesc);

		GfxAccelerationStructureDesc tlasDesc;
		tlasDesc.type          = GfxAccelerationStructureType::TopLevel;
		tlasDesc.instanceCount = 2;
		m_tlas = Gfx_CreateAccelerationStructure(tlasDesc);

		if (!m_blas.valid() || !m_tlas.valid())
		{
			m_skipReason = "Failed to create acceleration structures.";
			return false;
		}

		GfxOwn<GfxBuffer> instanceBuffer = Gfx_CreateBuffer(GfxBufferFlags::Transient | GfxBufferFlags::RayTracing);
		auto instanceData = Gfx_BeginUpdateBuffer<GfxRayTracingInstanceDesc>(ctx, instanceBuffer.get(), 2);

		const u64 blasHandle = Gfx_GetAccelerationStructureHandle(m_blas);

		// Instance 0: translated to x=-1, instanceID=0
		instanceData[0].init();
		instanceData[0].transform[3] = -1.0f; // translate X
		instanceData[0].instanceID = 0;
		instanceData[0].accelerationStructureHandle = blasHandle;

		// Instance 1: translated to x=+1, instanceID=1
		instanceData[1].init();
		instanceData[1].transform[3] = 1.0f; // translate X
		instanceData[1].instanceID = 1;
		instanceData[1].accelerationStructureHandle = blasHandle;

		Gfx_EndUpdateBuffer(ctx, instanceBuffer);

		Gfx_BuildAccelerationStructure(ctx, m_blas);
		Gfx_AddFullPipelineBarrier(ctx);

		Gfx_BuildAccelerationStructure(ctx, m_tlas, instanceBuffer);
		Gfx_AddFullPipelineBarrier(ctx);

		return true;
	}

	GfxOwn<GfxComputeShader>         m_computeShader;
	GfxOwn<GfxSampler>               m_samplerNearest;
	GfxOwn<GfxTexture>               m_textures[2];
	GfxOwn<GfxBuffer>                m_outputBuffer;
	GfxOwn<GfxComputePipeline>       m_pipeline;
	GfxOwn<GfxBuffer>                m_vertexBuffer;
	GfxOwn<GfxBuffer>                m_indexBuffer;
	GfxOwn<GfxAccelerationStructure> m_blas;
	GfxOwn<GfxAccelerationStructure> m_tlas;

	u32 m_expected[2] = {};
};

RUSH_REGISTER_TEST(RayTracingTextureArrayTest, "raytracing",
	"Validates ray query with per-instance texture array access.");
