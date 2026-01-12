#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilLog.h>
#include <Rush/Window.h>

#include <stdio.h>

#include <Common/ExampleApp.h>
#include <Common/ImGuiImpl.h>
#include <imgui.h>

class ImGuiApp : public ExampleApp
{
public:
	ImGuiApp()
	{
		ImGuiImpl_Startup(m_window);
	}

	~ImGuiApp()
	{
		ImGuiImpl_Shutdown();
	}

	void onUpdate() override
	{
		ImGuiImpl_Update(1.0f / 60.0f);

		auto window = m_window;
		auto ctx    = Platform_GetGfxContext();

		GfxPassDesc passDesc;
		passDesc.flags = GfxPassFlags::ClearAll;
		passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
		Gfx_BeginPass(ctx, passDesc);

		Gfx_SetViewport(ctx, GfxViewport(window->getFramebufferSize()));
		Gfx_SetScissorRect(ctx, window->getFramebufferSize());

		ImGui::ShowTestWindow();
		ImGuiImpl_Render(ctx, m_prim);

		Gfx_EndPass(ctx);
	}
};

int main(int argc, char** argv)
{
	AppConfig cfg;

	cfg.name      = "ImGui (" RUSH_RENDER_API_NAME ")";
	cfg.width     = 1280;
	cfg.height    = 720;
	cfg.resizable = true;
	cfg.argc = argc;
	cfg.argv = argv;

#ifdef RUSH_DEBUG
	cfg.debug = true;
#endif

	return Example_Main<ImGuiApp>(cfg, argc, argv);
}
