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

		if (!createScene(ctx))
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

		Gfx_SetSampler(ctx, 0, m_samplerNearest);
		Gfx_SetTexture(ctx, 0, m_textures[0]);
		Gfx_SetTexture(ctx, 1, m_textures[1]);
		Gfx_SetStorageBuffer(ctx, 0, m_outputBuffer);
		Gfx_SetStorageBuffer(ctx, 1, m_materialBuffer);
		Gfx_SetStorageBuffer(ctx, 2, m_materialIndexBuffer);
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
				return TestResult::fail("Output mismatch at %zu: got 0x%08X expected 0x%08X",
					i, data[i], m_expected[i]);
			}
		}

		Gfx_UnmapBuffer(mapped);
		return TestResult::pass();
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

	static u32 packColor(const ColorRGBA8& color)
	{
		return static_cast<u32>(color);
	}

	bool createScene(GfxContext* ctx)
	{
		DynamicArray<Vec3> vertices;
		vertices.push_back(Vec3(-0.5f, -0.5f, 0.0f));
		vertices.push_back(Vec3(0.5f, -0.5f, 0.0f));
		vertices.push_back(Vec3(0.0f, 0.5f, 0.0f));

		DynamicArray<u32> indices;
		indices.push_back(0);
		indices.push_back(1);
		indices.push_back(2);

		m_vertexBuffer = Gfx_CreateBuffer(GfxBufferFlags::RayTracing, GfxFormat::GfxFormat_RGB32_Float,
			u32(vertices.size()), u32(sizeof(Vec3)), vertices.data());
		m_indexBuffer = Gfx_CreateBuffer(GfxBufferFlags::RayTracing, GfxFormat::GfxFormat_R32_Uint,
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

	bool m_ready = false;
	bool m_loggedSkip = false;
	String m_skipReason;

	GfxOwn<GfxRayTracingPipeline>    m_pipeline;
	GfxOwn<GfxBuffer>                m_vertexBuffer;
	GfxOwn<GfxBuffer>                m_indexBuffer;
	GfxOwn<GfxAccelerationStructure> m_blas;
	GfxOwn<GfxAccelerationStructure> m_tlas;
	GfxOwn<GfxBuffer>                m_hitGroupSbt;
	GfxOwn<GfxBuffer>                m_materialBuffer;
	GfxOwn<GfxBuffer>                m_materialIndexBuffer;
	GfxOwn<GfxBuffer>                m_outputBuffer;
	GfxOwn<GfxSampler>               m_samplerNearest;
	GfxOwn<GfxTexture>               m_textures[2];

	u32 m_expected[4] = {};
};

RUSH_REGISTER_TEST(RayTracingMaterialBindingTest, "raytracing",
	"Validates Metal RT material data and texture array access.");
