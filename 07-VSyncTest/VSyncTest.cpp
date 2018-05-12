#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilLog.h>
#include <Rush/UtilTimer.h>
#include <Rush/Window.h>

#include <float.h>
#include <stdio.h>

#ifdef __GNUC__
#define sprintf_s sprintf // TODO: make a common cross-compiler/platform equivalent
#endif

using namespace Rush;

class VSyncTestApp : public Application
{
public:
	VSyncTestApp()
	{
		m_prim = new PrimitiveBatch();
		m_font = new BitmapFontRenderer(BitmapFontRenderer::createEmbeddedFont(true, 0, 1));

		m_lastFrameStartTime = m_timer.time();
	}

	~VSyncTestApp()
	{
		delete m_font;
		delete m_prim;
	}

	void update()
	{
		const double currentFrameStartTime = m_timer.time();
		const double deltaTime             = currentFrameStartTime - m_lastFrameStartTime;

		auto window = Platform_GetWindow();
		auto ctx    = Platform_GetGfxContext();

		GfxPassDesc passDesc;
		passDesc.flags          = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
		Gfx_BeginPass(ctx, passDesc);

		Gfx_SetViewport(ctx, GfxViewport(window->getFramebufferSize()));
		Gfx_SetScissorRect(ctx, window->getFramebufferSize());

		const bool overTime = deltaTime > (1.01 / 60.0);
		if (overTime)
		{
			Log::message("Long frame time: %f ms", deltaTime * 1000.0);
		}

		m_prim->begin2D(window->getFramebufferSize());

		const double t    = m_timer.time() * 1.0f;
		const float  posX = float(t - (int)t) * window->getSizeFloat().x;
		const float  size = max(1.0f, window->getSizeFloat().x * 0.01f);
		m_prim->drawRect(Box2(posX - size, 0, posX + size, window->getSizeFloat().y),
		    overTime ? ColorRGBA8(255, 0, 0) : ColorRGBA8(128, 128, 255));

		m_font->setScale(2.0f);
		char str[1024];
		sprintf_s(str, "Frame: %d", m_frameCounter);
		m_font->draw(m_prim, Vec2(posX - 50.0f, window->getSizeFloat().y * 0.5f), str);

		sprintf_s(str, "CPU frame time: %.2f ms", deltaTime * 1000.0);
		m_font->draw(m_prim, Vec2(10.0f), str);

		m_prim->end2D();

		Gfx_EndPass(ctx);

		++m_frameCounter;

		m_lastFrameStartTime = currentFrameStartTime;
	}

private:
	PrimitiveBatch*     m_prim = nullptr;
	BitmapFontRenderer* m_font = nullptr;
	Timer               m_timer;
	u32                 m_frameCounter       = 0;
	double              m_lastFrameStartTime = 0;
};

int main()
{
	AppConfig cfg;

	cfg.name      = "VSyncTest (" RUSH_RENDER_API_NAME ")";
	cfg.width     = 640;
	cfg.height    = 480;
	cfg.resizable = true;
	cfg.vsync     = true;

	return Platform_Main<VSyncTestApp>(cfg);
}
