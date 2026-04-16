#include "TestFramework.h"

#include <Common/Utils.h>
#include <Rush/GfxDevice.h>
#include <Rush/MathCommon.h>
#include <Rush/UtilLog.h>

using namespace Test;
using namespace Rush;

// Validates ray query texture array access: traces rays against a scene
// with four triangle instances at different positions, each assigned a
// different instanceID. The shader uses the instanceID to index into a
// texture array (bound in a separate descriptor set) and writes the
// sampled color to the output buffer.
class RayTracingTextureArrayTest final : public GfxTestCase
{
public:
	static constexpr u32 InstanceCount = 4;

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

		if (!createTextures())
		{
			return;
		}

		GfxBufferDesc outputDesc(GfxBufferFlags::Storage, GfxFormat_Unknown, 1, sizeof(u32) * InstanceCount);
		outputDesc.hostVisible = true;
		outputDesc.debugName = "TestRayTracingOutput";
		m_outputBuffer = Gfx_CreateBuffer(outputDesc);

		// Descriptor set 0: sampler, output buffer, TLAS
		// Descriptor set 1: texture array (separate set for bindless-style access)
		GfxComputePipelineDesc pipelineDesc;
		pipelineDesc.cs = m_computeShader.get();
		pipelineDesc.bindings.descriptorSets[0].samplers = 1;
		pipelineDesc.bindings.descriptorSets[0].rwBuffers = 1;
		pipelineDesc.bindings.descriptorSets[0].accelerationStructures = 1;
		pipelineDesc.bindings.descriptorSets[0].stageFlags = GfxStageFlags::Compute;
		pipelineDesc.bindings.descriptorSets[1].textures = InstanceCount;
		pipelineDesc.bindings.descriptorSets[1].flags = GfxDescriptorSetFlags::TextureArray;
		pipelineDesc.bindings.descriptorSets[1].stageFlags = GfxStageFlags::Compute;
		pipelineDesc.workGroupSize = {InstanceCount, 1, 1};

		m_pipeline = Gfx_CreateComputePipeline(pipelineDesc);
		if (!m_pipeline.valid())
		{
			m_skipReason = "Failed to create compute pipeline.";
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
		Gfx_SetSampler(ctx, 0, m_samplerNearest);
		Gfx_SetStorageBuffer(ctx, 0, m_outputBuffer);
		Gfx_SetAccelerationStructure(ctx, 0, m_tlas);
		Gfx_SetDescriptors(ctx, 1, m_textureDescriptorSet);
		Gfx_Dispatch(ctx, 1, 1, 1);
	}

	TestResult validate(GfxContext*, const TestImage*) override
	{
		if (!m_ready)
		{
			return TestResult::pass();
		}

		return validateBufferU32(m_outputBuffer, m_expected, InstanceCount);
	}

private:
	bool createTextures()
	{
		m_samplerNearest = Gfx_CreateSamplerState(GfxSamplerDesc::makePoint());

		const ColorRGBA8 colors[InstanceCount] = {
			ColorRGBA8(255, 0,   0,   255), // red
			ColorRGBA8(0,   255, 0,   255), // green
			ColorRGBA8(0,   0,   255, 255), // blue
			ColorRGBA8(255, 255, 0,   255), // yellow
		};

		GfxTextureDesc textureDesc = GfxTextureDesc::make2D(2, 2, GfxFormat_RGBA8_Unorm);
		GfxTexture textureHandles[InstanceCount];

		for (u32 i = 0; i < InstanceCount; ++i)
		{
			const ColorRGBA8 pixels[4] = { colors[i], colors[i], colors[i], colors[i] };
			m_textures[i] = Gfx_CreateTexture(textureDesc, pixels);
			textureHandles[i] = m_textures[i].get();
			m_expected[i] = packColor(colors[i]);
		}

		GfxDescriptorSetDesc textureSetDesc;
		textureSetDesc.textures = InstanceCount;
		textureSetDesc.flags = GfxDescriptorSetFlags::TextureArray;
		textureSetDesc.stageFlags = GfxStageFlags::Compute;

		m_textureDescriptorSet = Gfx_CreateDescriptorSet(textureSetDesc);
		if (!m_textureDescriptorSet.valid())
		{
			m_skipReason = "Failed to create texture descriptor set.";
			return false;
		}

		Gfx_UpdateDescriptorSet(m_textureDescriptorSet,
		    nullptr,          // constantBuffers
		    nullptr,          // samplers
		    textureHandles,   // textures
		    nullptr,          // storageImages
		    nullptr);         // storageBuffers

		return true;
	}

	bool createScene(GfxContext* ctx)
	{
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
		tlasDesc.instanceCount = InstanceCount;
		m_tlas = Gfx_CreateAccelerationStructure(tlasDesc);

		if (!m_blas.valid() || !m_tlas.valid())
		{
			m_skipReason = "Failed to create acceleration structures.";
			return false;
		}

		GfxOwn<GfxBuffer> instanceBuffer = Gfx_CreateBuffer(GfxBufferFlags::Transient | GfxBufferFlags::RayTracing);
		auto instanceData = Gfx_BeginUpdateBuffer<GfxRayTracingInstanceDesc>(ctx, instanceBuffer.get(), InstanceCount);

		const u64 blasHandle = Gfx_GetAccelerationStructureHandle(m_blas);

		// Place instances along X axis: -3, -1, +1, +3
		for (u32 i = 0; i < InstanceCount; ++i)
		{
			instanceData[i].init();
			instanceData[i].transform[3] = -3.0f + 2.0f * static_cast<float>(i);
			instanceData[i].instanceID = i;
			instanceData[i].accelerationStructureHandle = blasHandle;
		}

		Gfx_EndUpdateBuffer(ctx, instanceBuffer);

		Gfx_BuildAccelerationStructure(ctx, m_blas);
		Gfx_AddFullPipelineBarrier(ctx);

		Gfx_BuildAccelerationStructure(ctx, m_tlas, instanceBuffer);
		Gfx_AddFullPipelineBarrier(ctx);

		return true;
	}

	GfxOwn<GfxComputeShader>         m_computeShader;
	GfxOwn<GfxSampler>               m_samplerNearest;
	GfxOwn<GfxTexture>               m_textures[InstanceCount];
	GfxOwn<GfxDescriptorSet>         m_textureDescriptorSet;
	GfxOwn<GfxBuffer>                m_outputBuffer;
	GfxOwn<GfxComputePipeline>       m_pipeline;
	GfxOwn<GfxBuffer>                m_vertexBuffer;
	GfxOwn<GfxBuffer>                m_indexBuffer;
	GfxOwn<GfxAccelerationStructure> m_blas;
	GfxOwn<GfxAccelerationStructure> m_tlas;

	u32 m_expected[InstanceCount] = {};
};

RUSH_REGISTER_TEST(RayTracingTextureArrayTest, "raytracing",
	"Validates ray query with per-instance texture array access via separate descriptor set.");
