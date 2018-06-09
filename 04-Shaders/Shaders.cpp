#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilFile.h>
#include <Rush/UtilLog.h>
#include <Rush/UtilTimer.h>
#include <Rush/Window.h>

#include <Common/Utils.h>

#include <memory>
#include <stdio.h>

class ShadersApp : public Rush::Application
{
public:
	ShadersApp()
	{
		GfxVertexShader vs;
		GfxPixelShader  ps;

		const auto& caps = Gfx_GetCapability();

		vs = Gfx_CreateVertexShader(loadShaderFromFile("Vertex.hlsl.bin"));
		ps = Gfx_CreatePixelShader(loadShaderFromFile("Pixel.hlsl.bin"));

		GfxVertexFormatDesc fmtDesc;
		fmtDesc.add(0, GfxVertexFormatDesc::DataType::Float2, GfxVertexFormatDesc::Semantic::Position, 0);
		GfxVertexFormat vf = Gfx_CreateVertexFormat(fmtDesc);

		GfxShaderBindings bindings;
		bindings.addConstantBuffer("Global", 0);
		m_tech = Gfx_CreateTechnique(GfxTechniqueDesc(ps, vs, vf, &bindings));

		GfxBufferDesc cbDesc(GfxBufferFlags::TransientConstant, GfxFormat_Unknown, 1, sizeof(m_constants));
		m_cb = Gfx_CreateBuffer(cbDesc);

		float vertices[] = {
		    -3.0f,
		    -1.0f,
		    +1.0f,
		    -1.0f,
		    +1.0f,
		    +3.0f,
		};

		GfxBufferDesc vbDesc(GfxBufferFlags::Vertex, GfxFormat_Unknown, 3, fmtDesc.streamStride(0));
		m_vb = Gfx_CreateBuffer(vbDesc, vertices);

		Gfx_Release(vs);
		Gfx_Release(ps);
		Gfx_Release(vf);
	}

	~ShadersApp()
	{
		Gfx_Release(m_tech);
		Gfx_Release(m_vb);
		Gfx_Release(m_cb);
	}

	void update()
	{
		auto window = Platform_GetWindow();
		auto ctx    = Platform_GetGfxContext();

		m_constants.shaderParams.x = (float)window->getSize().x;
		m_constants.shaderParams.y = (float)window->getSize().y;
		m_constants.shaderParams.z = (float)m_timer.time();
		m_constants.shaderParams.w = 0;
		Gfx_UpdateBuffer(ctx, m_cb, &m_constants, sizeof(m_constants));

		GfxPassDesc passDesc;
		passDesc.flags          = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(30, 40, 50);
		Gfx_BeginPass(ctx, passDesc);

		Gfx_SetViewport(ctx, window->getSize());
		Gfx_SetScissorRect(ctx, window->getSize());

		Gfx_SetTechnique(ctx, m_tech);
		Gfx_SetVertexStream(ctx, 0, m_vb);
		Gfx_SetPrimitive(ctx, GfxPrimitive::TriangleList);

		Gfx_SetConstantBuffer(ctx, 0, m_cb);

		Gfx_Draw(ctx, 0, 3);

		Gfx_EndPass(ctx);

		Gfx_SetPresentInterval(1);
	}

private:
	GfxBuffer    m_vb;
	GfxTechnique m_tech;
	GfxBuffer    m_cb;

	struct Constants
	{
		Vec4 shaderParams = Vec4(0.0f);
	};

	Constants m_constants;

	Timer m_timer;
};

int main()
{
	AppConfig cfg;

	cfg.name      = "Shaders (" RUSH_RENDER_API_NAME ")";
	cfg.width     = 800;
	cfg.height    = 600;
	cfg.resizable = true;

	return Platform_Main<ShadersApp>(cfg);
}
