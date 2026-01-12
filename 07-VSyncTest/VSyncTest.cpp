#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilLog.h>
#include <Rush/UtilTimer.h>
#include <Rush/Window.h>

#include <Common/ExampleApp.h>

#include <float.h>
#include <stdio.h>

class VSyncTestApp : public ExampleApp
{
public:
	VSyncTestApp()
	{
		m_lastFrameStartTime = m_timer.time();
	}

	void onUpdate() override
	{
		const double currentFrameStartTime = m_timer.time();
		const double deltaTime             = currentFrameStartTime - m_lastFrameStartTime;

		auto window = m_window;
		auto ctx    = Platform_GetGfxContext();

		GfxPassDesc passDesc;
		passDesc.flags          = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
		Gfx_BeginPass(ctx, passDesc);

		Gfx_SetViewport(ctx, GfxViewport(window->getFramebufferSize()));
		Gfx_SetScissorRect(ctx, window->getFramebufferSize());

		const bool overTime = deltaTime > (1.05 / 60.0);
		if (overTime)
		{
			Log::message("Long frame time: %f ms", deltaTime * 1000.0);
		}

		m_prim->begin2D(window->getSize());

		const double t    = m_timer.time() * 1.0f;
		const float  posX = float(t - (int)t) * window->getSizeFloat().x;
		const float  size = max(1.0f, window->getSizeFloat().x * 0.01f);
		m_prim->drawRect(Box2(posX - size, 0, posX + size, window->getSizeFloat().y),
		    ColorRGBA8(128, 128, 255));

		m_font->setScale(2.0f);
		char str[1024];
		snprintf(str, sizeof(str), "Frame: %d", m_frameCounter);
		m_font->draw(m_prim, Vec2(posX - 50.0f, window->getSizeFloat().y * 0.5f), str);

		snprintf(str, sizeof(str), "CPU frame time: %.2f ms", deltaTime * 1000.0);
		m_font->draw(m_prim, Vec2(10.0f), str);

		m_prim->end2D();

		Gfx_EndPass(ctx);

		++m_frameCounter;

		m_lastFrameStartTime = currentFrameStartTime;
	}

private:
	Timer               m_timer;
	u32                 m_frameCounter       = 0;
	double              m_lastFrameStartTime = 0;
};

int main(int argc, char** argv)
{
	AppConfig cfg;

	cfg.name      = "VSyncTest (" RUSH_RENDER_API_NAME ")";
	cfg.width     = 640;
	cfg.height    = 480;
	cfg.resizable = true;
	cfg.vsync     = true;
	cfg.argc = argc;
	cfg.argv = argv;

#ifdef RUSH_DEBUG
	cfg.debug = true;
#endif

	return Example_Main<VSyncTestApp>(cfg, argc, argv);
}
