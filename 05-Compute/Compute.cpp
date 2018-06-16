#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilFile.h>
#include <Rush/UtilLog.h>
#include <Rush/UtilTimer.h>
#include <Rush/Window.h>

#include <Common/Utils.h>

#include <memory>
#include <stdio.h>

class ComputeApp : public Application
{
public:
	ComputeApp()
	{
		m_prim = new PrimitiveBatch();

		m_computeShader = Gfx_CreateComputeShader(loadShaderFromFile(RUSH_SHADER_NAME("ComputeShader.hlsl")));

		GfxBufferDesc cbDesc(GfxBufferFlags::TransientConstant, GfxFormat_Unknown, 1, sizeof(m_constants));
		m_constantBuffer = Gfx_CreateBuffer(cbDesc);

		GfxShaderBindings bindings;
		bindings.addConstantBuffer("Global", 0);
		bindings.addStorageImage("outputImage", 1);
		m_technique = Gfx_CreateTechnique(GfxTechniqueDesc(m_computeShader, &bindings));

		GfxTextureDesc textureDesc = GfxTextureDesc::make2D(m_imageSize.x, m_imageSize.y, GfxFormat_RGBA8_Unorm,
		    GfxUsageFlags::ShaderResource | GfxUsageFlags::StorageImage);

		m_texture = Gfx_CreateTexture(textureDesc);
	}

	~ComputeApp()
	{
		delete m_prim;

		Gfx_Release(m_constantBuffer);
		Gfx_Release(m_texture);
		Gfx_Release(m_technique);
		Gfx_Release(m_computeShader);
	}

	void update()
	{
		auto window = Platform_GetWindow();
		auto ctx    = Platform_GetGfxContext();

		m_constants.x = (float)m_timer.time();
		m_constants.y = 0.0f;
		m_constants.z = 0.0f;
		m_constants.w = 0.0f;

		Gfx_UpdateBuffer(ctx, m_constantBuffer, &m_constants, sizeof(m_constants));

		Gfx_SetTechnique(ctx, m_technique);
		Gfx_SetStorageImage(ctx, 0, m_texture);
		Gfx_SetConstantBuffer(ctx, 0, m_constantBuffer);
		Gfx_Dispatch(ctx, m_imageSize.x / 8, m_imageSize.y / 8, 1);

		Gfx_AddImageBarrier(ctx, m_texture, GfxResourceState_ShaderRead);

		GfxPassDesc backBufferPassDesc;
		backBufferPassDesc.flags          = GfxPassFlags::ClearAll;
		backBufferPassDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
		Gfx_BeginPass(ctx, backBufferPassDesc);
		Gfx_SetViewport(ctx, window->getSize());
		Gfx_SetScissorRect(ctx, window->getSize());

		m_prim->begin2D(window->getSize());
		m_prim->setTexture(m_texture);
		Box2 rect(Vec2(0.0f), window->getSizeFloat());
		m_prim->drawTexturedQuad(rect);
		m_prim->end2D();

		Gfx_EndPass(ctx);
	}

private:
	GfxBuffer        m_constantBuffer;
	GfxComputeShader m_computeShader;
	GfxTechnique     m_technique;
	GfxTexture       m_texture;
	PrimitiveBatch*  m_prim      = nullptr;
	Tuple2u          m_imageSize = {640, 480};
	Timer            m_timer;
	Vec4             m_constants = Vec4(0.0);
};

int main()
{
	AppConfig cfg;

	cfg.name = "Compute (" RUSH_RENDER_API_NAME ")";

	cfg.width     = 640;
	cfg.height    = 480;
	cfg.resizable = true;

	return Platform_Main<ComputeApp>(cfg);
}
