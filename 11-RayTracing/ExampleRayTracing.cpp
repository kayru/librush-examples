#include "ExampleRayTracing.h"

#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilLog.h>
#include <Rush/Window.h>

#include <Rush/MathTypes.h>
#include <Rush/UtilFile.h>
#include <Rush/UtilHash.h>
#include <Rush/UtilLog.h>

#ifdef __GNUC__
#define sprintf_s sprintf // TODO: make a common cross-compiler/platform equivalent
#endif

static AppConfig g_appCfg;

int main(int argc, char** argv)
{
	g_appCfg.name = "RayTracing (" RUSH_RENDER_API_NAME ")";

	g_appCfg.width     = 1280;
	g_appCfg.height    = 720;
	g_appCfg.argc      = argc;
	g_appCfg.argv      = argv;
	g_appCfg.resizable = true;

#ifdef RUSH_DEBUG
	g_appCfg.debug = true;
	Log::breakOnError = true;
#endif

	return Platform_Main<ExampleRayTracing>(g_appCfg);
}


ExampleRayTracing::ExampleRayTracing() : ExampleApp()
{
	Gfx_SetPresentInterval(1);

	m_constantBuffer = Gfx_CreateBuffer(GfxBufferFlags::TransientConstant);

	if (Gfx_GetCapability().rayTracingNV)
	{
		GfxRayTracingPipelineDesc pipelineDesc;
		pipelineDesc.rayGen     = loadShaderFromFile(RUSH_SHADER_NAME("Primary.rgen"));
		pipelineDesc.miss       = loadShaderFromFile(RUSH_SHADER_NAME("Primary.rmiss"));
		pipelineDesc.closestHit = loadShaderFromFile(RUSH_SHADER_NAME("Primary.rchit"));

		pipelineDesc.bindings.constantBuffers        = 1;
		pipelineDesc.bindings.rwImages               = 1;
		pipelineDesc.bindings.accelerationStructures = 1;

		m_rtPipeline = Gfx_CreateRayTracingPipeline(pipelineDesc);

		// TODO: write a utility function to generate SBT easily and correctly
		const size_t     shaderHandleSize = Gfx_GetCapability().rtShaderHandleSize;
		const size_t     shaderCount      = 1; // one hit group
		DynamicArray<u8> sbt(shaderHandleSize * shaderCount);
		memcpy(sbt.data() + shaderHandleSize * 0,
		    Gfx_GetRayTracingShaderHandle(m_rtPipeline, GfxRayTracingShaderType::HitGroup, 0), shaderHandleSize);
		m_sbtBuffer = Gfx_CreateBuffer(GfxBufferFlags::RayTracing, shaderCount, u32(shaderHandleSize), sbt.data());
	}
}

ExampleRayTracing::~ExampleRayTracing()
{
}

void ExampleRayTracing::update()
{
	const GfxCapability& caps = Gfx_GetCapability();
	GfxContext* ctx = Platform_GetGfxContext();

	GfxTextureDesc outputImageDesc = Gfx_GetTextureDesc(m_outputImage);
	if (!m_outputImage.valid() || outputImageDesc.getSize2D() != m_window->getSize())
	{
		outputImageDesc = GfxTextureDesc::make2D(
		    m_window->getSize(), GfxFormat_RGBA16_Float, GfxUsageFlags::StorageImage_ShaderResource);

		m_outputImage = Gfx_CreateTexture(outputImageDesc);
	}

	{
		struct Constants
		{
			Tuple2i outputSize;
		};

		Constants* constants = Gfx_BeginUpdateBuffer<Constants>(ctx, m_constantBuffer);
		constants->outputSize = outputImageDesc.getSize2D();
		Gfx_EndUpdateBuffer(ctx, m_constantBuffer);
	}

	GfxMarkerScope markerFrame(ctx, "Frame");

	if (Gfx_GetCapability().rayTracingNV)
	{
		if (!m_tlas.valid())
		{
			createScene(ctx);
		}

		GfxMarkerScope markerRT(ctx, "RT");
		Gfx_SetConstantBuffer(ctx, 0, m_constantBuffer);
		Gfx_SetStorageImage(ctx, 0, m_outputImage);

		Gfx_TraceRays(ctx, m_rtPipeline, m_tlas, m_sbtBuffer, outputImageDesc.width, outputImageDesc.height);

		Gfx_AddImageBarrier(ctx, m_outputImage, GfxResourceState_ShaderRead);
	}

	{
		GfxPassDesc passDesc;
		passDesc.flags = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
		Gfx_BeginPass(ctx, passDesc);

		GfxMarkerScope markerUI(ctx, "UI");

		Gfx_SetBlendState(ctx, m_blendStates.lerp);
		Gfx_SetDepthStencilState(ctx, m_depthStencilStates.disable);

		m_prim->begin2D(m_window->getSize());

		if (Gfx_GetCapability().rayTracingNV)
		{
			m_prim->setTexture(m_outputImage);
			Box2 rect(Vec2(0.0f), m_window->getSizeFloat());
			m_prim->drawTexturedQuad(rect);
		}
		else
		{
			Vec2 pos = Vec2(10, 10);
			m_font->setScale(4.0f);
			pos = m_font->draw(m_prim, pos, "Ray tracing is not supported.\n", ColorRGBA8::Red());
		}

		m_prim->end2D();

		Gfx_EndPass(ctx);
	}

}

void ExampleRayTracing::createScene(GfxContext* ctx)
{
	DynamicArray<Vec3> vertices;
	vertices.push_back(Vec3(-1, -1, 1) * 0.5f);
	vertices.push_back(Vec3(0, 1, 1) * 0.5f);
	vertices.push_back(Vec3(1, -1, 1) * 0.5f);

	DynamicArray<u32> indices;
	indices.push_back(0);
	indices.push_back(1);
	indices.push_back(2);

	GfxOwn<GfxBuffer> vb = Gfx_CreateBuffer(GfxBufferFlags::None, GfxFormat::GfxFormat_RGB32_Float, u32(vertices.size()), u32(sizeof(Vec3)), vertices.data());
	GfxOwn<GfxBuffer> ib = Gfx_CreateBuffer(GfxBufferFlags::None, GfxFormat::GfxFormat_R32_Uint, u32(indices.size()), 4, indices.data());

	DynamicArray<GfxRayTracingGeometryDesc> geometries;
	{
		GfxRayTracingGeometryDesc geometryDesc;
		geometryDesc.indexBuffer  = ib.get();
		geometryDesc.indexFormat  = GfxFormat::GfxFormat_R32_Uint;
		geometryDesc.indexCount   = u32(indices.size());
		geometryDesc.vertexBuffer = vb.get();
		geometryDesc.vertexFormat = GfxFormat::GfxFormat_RGB32_Float;
		geometryDesc.vertexStride = u32(sizeof(Vec3));
		geometryDesc.vertexCount  = u32(vertices.size());
		geometries.push_back(geometryDesc);
	}

	GfxAccelerationStructureDesc blasDesc;
	blasDesc.type         = GfxAccelerationStructureType::BottomLevel;
	blasDesc.geometyCount = u32(geometries.size());
	blasDesc.geometries   = geometries.data();
	m_blas                = Gfx_CreateAccelerationStructure(blasDesc);

	GfxAccelerationStructureDesc tlasDesc;
	tlasDesc.type          = GfxAccelerationStructureType::TopLevel;
	tlasDesc.instanceCount = 1;
	m_tlas                 = Gfx_CreateAccelerationStructure(tlasDesc);

	GfxOwn<GfxBuffer> instanceBuffer = Gfx_CreateBuffer(GfxBufferFlags::Transient);
	{
		auto instanceData = Gfx_BeginUpdateBuffer<GfxRayTracingInstanceDesc>(ctx, instanceBuffer.get(), tlasDesc.instanceCount);
		instanceData[0].init();
		instanceData[0].accelerationStructureHandle = Gfx_GetAccelerationStructureHandle(m_blas);
		Gfx_EndUpdateBuffer(ctx, instanceBuffer);
	}

	Gfx_BuildAccelerationStructure(ctx, m_blas);
	Gfx_BuildAccelerationStructure(ctx, m_tlas, instanceBuffer);
}

