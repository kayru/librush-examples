#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilLog.h>
#include <Rush/Window.h>

#include <stdio.h>

#include <Common/ImGuiImpl.h>
#include <imgui.h>

class ImGuiApp : public Application
{
public:
	ImGuiApp()
	{
		m_prim = new PrimitiveBatch;
		ImGuiImpl_Startup(Platform_GetWindow());
	}

	~ImGuiApp()
	{
		ImGuiImpl_Shutdown();
		delete m_prim;
	}

	void update()
	{
		ImGuiImpl_Update(1.0f / 60.0f);

		auto window = Platform_GetWindow();
		auto ctx    = Platform_GetGfxContext();

		GfxPassDesc passDesc;
		passDesc.flags = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
		Gfx_BeginPass(ctx, passDesc);

		Gfx_SetViewport(ctx, GfxViewport(window->getSize()));
		Gfx_SetScissorRect(ctx, window->getSize());

		ImGui::ShowTestWindow();
		ImGuiImpl_Render(ctx, m_prim);

		Gfx_EndPass(ctx);
	}

private:

	PrimitiveBatch* m_prim = nullptr;
};

int main()
{
	AppConfig cfg;

	cfg.name      = "ImGui (" RUSH_RENDER_API_NAME ")";
	cfg.width     = 1280;
	cfg.height    = 720;
	cfg.resizable = true;

#ifdef RUSH_DEBUG
	cfg.debug = true;
#endif

	return Platform_Main<ImGuiApp>(cfg);
}
