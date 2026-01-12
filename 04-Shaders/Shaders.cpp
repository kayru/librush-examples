#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilFile.h>
#include <Rush/UtilLog.h>
#include <Rush/UtilTimer.h>
#include <Rush/Window.h>

#include <Common/ExampleApp.h>
#include <Common/Utils.h>

#include <memory>
#include <stdio.h>

class ShadersApp : public Rush::ExampleApp
{
public:
	ShadersApp()
	{
		auto vs = Gfx_CreateVertexShader(loadShaderFromFile(RUSH_SHADER_NAME("Vertex.hlsl")));
		auto ps = Gfx_CreatePixelShader(loadShaderFromFile(RUSH_SHADER_NAME("Pixel.hlsl")));

		GfxVertexFormatDesc fmtDesc;
		fmtDesc.add(0, GfxVertexFormatDesc::DataType::Float2, GfxVertexFormatDesc::Semantic::Position, 0);
		auto vf = Gfx_CreateVertexFormat(fmtDesc);

		GfxShaderBindingDesc bindings;
		bindings.descriptorSets[0].constantBuffers = 1;
		m_tech = Gfx_CreateTechnique(GfxTechniqueDesc(ps, vs, vf, bindings));

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
	}

	~ShadersApp()
	{
	}

	void onUpdate() override
	{
		auto window = m_window;
		auto ctx    = Platform_GetGfxContext();

		const Tuple2i framebufferSize = window->getFramebufferSize();
		m_constants.shaderParams.x = (float)framebufferSize.x;
		m_constants.shaderParams.y = (float)framebufferSize.y;
		m_constants.shaderParams.z = (float)m_timer.time();
		m_constants.shaderParams.w = 0;
		Gfx_UpdateBuffer(ctx, m_cb, &m_constants, sizeof(m_constants));

		GfxPassDesc passDesc;
		passDesc.flags          = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(30, 40, 50);
		Gfx_BeginPass(ctx, passDesc);

		Gfx_SetViewport(ctx, window->getFramebufferSize());
		Gfx_SetScissorRect(ctx, window->getFramebufferSize());

		Gfx_SetTechnique(ctx, m_tech);
		Gfx_SetVertexStream(ctx, 0, m_vb);
		Gfx_SetPrimitive(ctx, GfxPrimitive::TriangleList);

		Gfx_SetConstantBuffer(ctx, 0, m_cb);

		Gfx_Draw(ctx, 0, 3);

		Gfx_EndPass(ctx);

		Gfx_SetPresentInterval(1);
	}

private:
	GfxOwn<GfxBuffer>    m_vb;
	GfxOwn<GfxTechnique> m_tech;
	GfxOwn<GfxBuffer>    m_cb;

	struct Constants
	{
		Vec4 shaderParams = Vec4(0.0f);
	};

	Constants m_constants;

	Timer m_timer;
};

int main(int argc, char** argv)
{
	AppConfig cfg;

	cfg.name      = "Shaders (" RUSH_RENDER_API_NAME ")";
	cfg.width     = 800;
	cfg.height    = 600;
	cfg.resizable = true;
	cfg.argc = argc;
	cfg.argv = argv;

#ifdef RUSH_DEBUG
	cfg.debug = true;
#endif

	return Example_Main<ShadersApp>(cfg, argc, argv);
}
