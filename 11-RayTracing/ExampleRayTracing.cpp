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

	m_camera.lookAt(Vec3(0, 0, 0), Vec3(0, 0, 1));
	m_constantBuffer = Gfx_CreateBuffer(GfxBufferFlags::TransientConstant);

	if (Gfx_GetCapability().rayTracingNV)
	{
		GfxRayTracingPipelineDesc pipelineDesc;
		pipelineDesc.rayGen = loadShaderFromFile(RUSH_SHADER_NAME("Primary.rgen"));
		pipelineDesc.miss = loadShaderFromFile(RUSH_SHADER_NAME("Primary.rmiss"));

		pipelineDesc.bindings.constantBuffers = 1;
		pipelineDesc.bindings.rwImages = 1;
		//pipelineDesc.bindings.accelerationStructures = 1;

		m_rtPipeline = Gfx_CreateRayTracingPipeline(pipelineDesc);

		// TODO: write a utility function to generate SBT easily and correctly
		const size_t shaderHandleSize = Gfx_GetCapability().rtShaderHandleSize;
		const size_t shaderCount = 2; // rgen + rmiss
		DynamicArray<u8> sbt(shaderHandleSize * shaderCount);
		memcpy(sbt.data() + shaderHandleSize * 0, Gfx_GetRayTracingShaderHandle(m_rtPipeline, GfxRayTracingShaderType::RayGen, 0), shaderHandleSize);
		memcpy(sbt.data() + shaderHandleSize * 1, Gfx_GetRayTracingShaderHandle(m_rtPipeline, GfxRayTracingShaderType::Miss, 0), shaderHandleSize);
		m_sbtBuffer = Gfx_CreateBuffer(GfxBufferFlags::None, shaderCount, shaderHandleSize, sbt.data());
	}
}

ExampleRayTracing::~ExampleRayTracing()
{
	Gfx_Finish(); // TODO: enqueue all resource destruction to avoid wait-for-idle
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

	const Mat4 matView = m_camera.buildViewMatrix();
	const Mat4 matProj = m_camera.buildProjMatrix(caps.projectionFlags);
	const Mat4 matViewProj = matView * matProj;

	{
		Constants* constants = Gfx_BeginUpdateBuffer<Constants>(ctx, m_constantBuffer);
		constants->matViewProj = matViewProj;
		constants->outputSize = outputImageDesc.getSize2D();
		Gfx_EndUpdateBuffer(ctx, m_constantBuffer);
	}

	GfxMarkerScope markerFrame(ctx, "Frame");

	if (Gfx_GetCapability().rayTracingNV)
	{
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

