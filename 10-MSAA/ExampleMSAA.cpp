#include <Rush/GfxBitmapFont.h>
#include <Rush/GfxPrimitiveBatch.h>
#include <Rush/Platform.h>
#include <Rush/UtilTimer.h>
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

		{
			GfxTextureDesc desc = GfxTextureDesc::make2D(64, 64);
			desc.usage = GfxUsageFlags::RenderTarget | GfxUsageFlags::ShaderResource | GfxUsageFlags::TransferDst;
			m_resolveTarget = Gfx_CreateTexture(desc);
		}

		m_blendPremult = Gfx_CreateBlendState(GfxBlendStateDesc::makePremultiplied());
		m_blendOpaque = Gfx_CreateBlendState(GfxBlendStateDesc::makeOpaque());
	}

	~ImGuiApp()
	{
		ImGuiImpl_Shutdown();
		delete m_prim;
	}

	void update()
	{
		int desiredSampleCount = 1 << m_msaaQuality;
		if (m_currentSampleCount != desiredSampleCount)
		{
			GfxTextureDesc desc = Gfx_GetTextureDesc(m_resolveTarget);
			desc.samples = desiredSampleCount;
			desc.usage = GfxUsageFlags::RenderTarget | GfxUsageFlags::TransferSrc;
			m_msaaTarget = Gfx_CreateTexture(desc);
			m_currentSampleCount = desiredSampleCount;
		}

		const float dt = float(m_deltaTime.time());
		ImGuiImpl_Update(dt);

		if (m_enableAnimation)
		{
			if (m_animationTime > 1.0)
			{
				m_animationTime = m_animationTime - floorf(m_animationTime);
			}

			m_animationTime += dt * 0.1f;
		}

		auto window = Platform_GetWindow();
		auto ctx    = Platform_GetGfxContext();

		// off-screen rendering

		{
			GfxPassDesc passDesc;
			passDesc.flags = GfxPassFlags::ClearAll;
			passDesc.clearColors[0] = ColorRGBA8(11, 22, 33);
			passDesc.color[0] = m_enableMSAA ? m_msaaTarget.get() : m_resolveTarget.get();
			Gfx_BeginPass(ctx, passDesc);
			Gfx_SetBlendState(ctx, m_blendPremult);
			m_prim->begin2D(Vec2(1.0f), Vec2(0.0f));

			auto positionFromAngle = [](float a)
			{
				float sa = sinf(a);
				float ca = cosf(a);
				return Vec2(ca, sa);
			};

			float t = (m_animationTime * TwoPi) / 3.0f;

			ColorRGBA8 colors[] = {
				ColorRGBA8::Red(30),
				ColorRGBA8::Green(30),
				ColorRGBA8::Blue(30),
				ColorRGBA8::White(30),
			};

			for (ColorRGBA8 c : colors)
			{
				m_prim->drawTriangle(
					positionFromAngle(t + (0 * TwoPi / 3.0f)),
					positionFromAngle(t + (1 * TwoPi / 3.0f)),
					positionFromAngle(t + (2 * TwoPi / 3.0f)),
					c);
				t += (TwoPi / RUSH_COUNTOF(colors));
			}

			m_prim->end2D();

			Gfx_EndPass(ctx);

			if (m_enableMSAA)
			{
				Gfx_ResolveImage(ctx, m_msaaTarget, m_resolveTarget);
			}

			Gfx_AddImageBarrier(ctx, m_resolveTarget, GfxResourceState_ShaderRead);
		}

		// render to back buffer
		{
			GfxPassDesc passDesc;
			passDesc.flags = GfxPassFlags::ClearAll;
			passDesc.clearColors[0] = ColorRGBA8(0, 0, 0);
			Gfx_BeginPass(ctx, passDesc);
			Gfx_SetBlendState(ctx, m_blendOpaque);

			Gfx_SetViewport(ctx, GfxViewport(window->getSize()));
			Gfx_SetScissorRect(ctx, window->getSize());

			m_prim->begin2D(1.0f, 1.0f);

			TexturedQuad2D q;

			q.pos[0] = Vec2(0.0f, 0.0f);
			q.pos[1] = Vec2(1.0f, 0.0f);
			q.pos[2] = Vec2(1.0f, 1.0f);
			q.pos[3] = Vec2(0.0f, 1.0f);

			q.tex[0] = Vec2(0.0f, 0.0f);
			q.tex[1] = Vec2(1.0f, 0.0);
			q.tex[2] = Vec2(1.0f, 1.0f);
			q.tex[3] = Vec2(0.0f, 1.0f);

			m_prim->setTexture(m_resolveTarget);
			m_prim->drawTexturedQuad(&q);

			m_prim->end2D();

			if (ImGui::Begin("Settings"))
			{
				ImGui::Checkbox("Enable Animation", &m_enableAnimation);
				ImGui::SliderFloat("Animation time", &m_animationTime, 0.0f, 1.0f);
				ImGui::Checkbox("Enable MSAA", &m_enableMSAA);
				char msaaText[3] = {};
				snprintf(msaaText, sizeof(msaaText), "%d", 1 << m_msaaQuality);
				ImGui::SliderInt("Sample count", &m_msaaQuality, 1, 3, msaaText);
				
			}
			ImGui::End();

			ImGuiImpl_Render(ctx, m_prim);

			Gfx_EndPass(ctx);
		}

		m_deltaTime.reset();
	}

private:

	PrimitiveBatch* m_prim = nullptr;
	bool m_enableAnimation = false;
	bool m_enableMSAA = false;
	int m_msaaQuality = 1;
	float m_animationTime = 0.0f;

	GfxOwn<GfxBlendState> m_blendPremult;
	GfxOwn<GfxBlendState> m_blendOpaque;
	GfxOwn<GfxTexture> m_resolveTarget;
	GfxOwn<GfxTexture> m_msaaTarget;
	int m_currentSampleCount = 0;

	Timer m_deltaTime;
};

int main()
{
	AppConfig cfg;

	cfg.name      = "MSAA (" RUSH_RENDER_API_NAME ")";
	cfg.width     = 1024;
	cfg.height    = 1024;
	cfg.resizable = true;

#ifdef RUSH_DEBUG
	cfg.debug = true;
#endif

	return Platform_Main<ImGuiApp>(cfg);
}
