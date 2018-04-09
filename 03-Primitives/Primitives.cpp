#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilLog.h>
#include <Rush/Window.h>

#include <stdio.h>

using namespace Rush;

class PrimitivesApp : public Application
{
public:
	PrimitivesApp()
	{
		m_prim = new PrimitiveBatch();
		m_font = new BitmapFontRenderer(BitmapFontRenderer::createEmbeddedFont(true, 0, 1));
	}

	~PrimitivesApp()
	{
		delete m_font;
		delete m_prim;
	}

	void update()
	{
		auto window = Platform_GetWindow();
		auto ctx    = Platform_GetGfxContext();

		GfxPassDesc passDesc;
		passDesc.flags          = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
		Gfx_BeginPass(ctx, passDesc);

		Gfx_SetViewport(ctx, GfxViewport(window->getFramebufferSize()));
		Gfx_SetScissorRect(ctx, window->getFramebufferSize());

		// Basic shape rendering

		{
			m_prim->begin2D(window->getFramebufferSize());

			m_prim->drawLine(Line2(100, 100, 100, 200), ColorRGBA8::Red());
			m_prim->drawLine(Line2(110, 100, 110, 200), ColorRGBA8::Green());
			m_prim->drawLine(Line2(120, 100, 120, 200), ColorRGBA8::Blue());

			m_prim->drawRect(Box2(130, 100, 150, 200), ColorRGBA8(128, 128, 255));

			m_prim->drawTriangle(Vec2(160, 100), Vec2(200, 100), Vec2(200, 200), ColorRGBA8(255, 128, 128));

			m_prim->end2D();
		}

		{
			m_prim->begin2D(Vec2(1.0f), Vec2(0.0f));

			m_prim->drawLine(Line2(0.0f, 0.0f, 1.0f, 0.0f), ColorRGBA8::Red());
			m_prim->drawLine(Line2(0.0f, 0.0f, 0.0f, 1.0f), ColorRGBA8::Green());

			m_prim->drawTriangle(Vec2(-0.5f, -0.5f), Vec2(-0.5f, 0.0f), Vec2(0.0f, -0.5f), ColorRGBA8(255, 0, 0),
			    ColorRGBA8(0, 255, 0), ColorRGBA8(0, 0, 255));

			m_prim->end2D();
		}

		// Text rendering
		{
			m_prim->begin2D(window->getFramebufferSize());

			Vec2 pos = Vec2(10, 10);

			pos = m_font->draw(m_prim, pos, "Hello world!\n", ColorRGBA8::Red());
			pos = m_font->draw(m_prim, pos, "This is line 2.\n", ColorRGBA8::Green());
			pos = m_font->draw(m_prim, pos, "the quick brown fox jumps over the lazy dog\n", ColorRGBA8::White());
			pos = m_font->draw(m_prim, pos, "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG\n", ColorRGBA8::White());
			pos = m_font->draw(m_prim, pos, "0 1 2 3 4 5 6 7 8 9\n", ColorRGBA8::White());

			m_prim->end2D();
		}

		Gfx_EndPass(ctx);
	}

private:
	PrimitiveBatch*     m_prim = nullptr;
	BitmapFontRenderer* m_font = nullptr;
};

int main()
{
	AppConfig cfg;

	cfg.name      = "Primitives (" RUSH_RENDER_API_NAME ")";
	cfg.width     = 640;
	cfg.height    = 480;
	cfg.resizable = true;

	return Platform_Main<PrimitivesApp>(cfg);
}
