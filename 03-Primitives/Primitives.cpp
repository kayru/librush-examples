#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilLog.h>
#include <Rush/Window.h>

#include <memory>
#include <stdio.h>

using namespace Rush;

class PrimitivesExample : public Application
{
public:
	PrimitivesExample() { m_prim = new PrimitiveBatch(); }

	~PrimitivesExample() { delete m_prim; }

	void update()
	{
		auto window = Platform_GetWindow();
		auto ctx    = Platform_GetGfxContext();

		GfxPassDescr passDescr;
		passDescr.flags          = GfxPassFlags::ClearAll;
		passDescr.clearColors[0] = ColorRGBA8(11, 22, 33);
		Gfx_BeginPass(ctx, passDescr);

		Gfx_SetViewport(ctx, GfxViewport(window->getFramebufferSize()));
		Gfx_SetScissorRect(ctx, window->getFramebufferSize());

		m_prim->begin2D(window->getFramebufferSize());

		m_prim->drawLine(Line2(100, 100, 100, 200), ColorRGBA8::Red());
		m_prim->drawLine(Line2(110, 100, 110, 200), ColorRGBA8::Green());
		m_prim->drawLine(Line2(120, 100, 120, 200), ColorRGBA8::Blue());

		m_prim->drawRect(Box2(130, 100, 150, 200), ColorRGBA8(128, 128, 255));

		m_prim->drawTriangle(Vec2(160, 100), Vec2(200, 100), Vec2(200, 200), ColorRGBA8(255, 128, 128));

		m_prim->end2D();

		m_prim->begin2D(Vec2(1.0f), Vec2(0.0f));
		m_prim->drawLine(Line2(0.0f, 0.0f, 1.0f, 0.0f), ColorRGBA8::Red());
		m_prim->drawLine(Line2(0.0f, 0.0f, 0.0f, 1.0f), ColorRGBA8::Green());

		m_prim->drawTriangle(Vec2(-0.5f, -0.5f), Vec2(-0.5f, 0.0f), Vec2(0.0f, -0.5f), ColorRGBA8(255, 0, 0),
		    ColorRGBA8(0, 255, 0), ColorRGBA8(0, 0, 255));

		m_prim->end2D();

		Gfx_EndPass(ctx);
	}

private:
	PrimitiveBatch* m_prim = nullptr;
};

int main()
{
	AppConfig cfg;

	cfg.name      = "Primitives (" RUSH_RENDER_API_NAME ")";
	cfg.width     = 640;
	cfg.height    = 480;
	cfg.resizable = true;

	return Platform_Main<PrimitivesExample>(cfg);
}
